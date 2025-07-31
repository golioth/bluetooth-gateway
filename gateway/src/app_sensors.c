/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_sensors, LOG_LEVEL_DBG);

#include <stdlib.h>
#include <golioth/client.h>
#include <golioth/stream.h>
#include <zcbor_encode.h>
#include <zephyr/kernel.h>
#include <zcbor_decode.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/drivers/sensor/ina260.h>

#include "app_sensors.h"



/* Convert DC reading to actual value */
int64_t calculate_reading(uint8_t upper, uint8_t lower)
{
	int16_t raw = (upper<<8) | lower;
	uint64_t big = raw * 125;
	return big;
}

#define SPI_OP	SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8) | SPI_LINES_SINGLE

struct k_sem adc_data_sem;

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\"cur\":{\"ch0\":%d,\"ch1\":%d},\"vol\":{\"ch0\":%d,\"ch1\":%d},\"pow\":{\"ch0\":%d,\"ch1\":%d}}"
#define JSON_FMT_SINGLE "{\"cur\":{\"%s\":%d},\"vol\":{\"%s\":%d},\"pow\":{\"%s\":%d}}"
#define CH0_PATH "ch0"
#define ADC_STREAM_ENDP	"sensor"
#define ADC_CUMULATIVE_ENDP	"state/cumulative"

#define ADC_CH0 0

adc_node_t adc_ch0 = {
	.dev = DEVICE_DT_GET(DT_NODELABEL(ina260_ch0)),
	.ch_num = ADC_CH0,
	.laston = -1,
	.runtime = 0,
	.total_unreported = 0,
	.total_cloud = 0,
	.loaded_from_cloud = false,
	.device_ready = false
};

static struct golioth_client *client;
static struct k_work_delayable _sensor_report_work;

#define LOOP_DELAY_S 60

static void async_error_handler(struct golioth_client *client,
                                enum golioth_status status,
                                const struct golioth_coap_rsp_code *coap_rsp_code,
                                const char *path,
                                void *arg)
{
    if (status != GOLIOTH_OK)
    {
        LOG_ERR("Async task failed: %d", status);
        return;
    }
}

static int get_adc_reading(adc_node_t *adc)
{
	int err;

	err = sensor_sample_fetch(adc->dev);
	if (err) {
		LOG_ERR("Error fetching sensor values from %s: %d", adc->dev->name, err);
		adc->device_ready = false;
		return err;
	}

	adc->device_ready = true;
	return 0;
}

static int log_sensor_values(adc_node_t *sensor, bool get_new_reading)
{
	int err;

	if (get_new_reading) {
		err = get_adc_reading(sensor);
		if (err) {
			return err;
		}
	}

	if (sensor->device_ready) {
		struct sensor_value cur, pow, vol;

		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_VOLTAGE,
				   &vol);
		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_CURRENT,
					   &cur);
		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_POWER,
				   &pow);

		LOG_INF("Device: %s, %f V, %f A, %f W",
			sensor->dev->name,
			sensor_value_to_double(&vol),
			sensor_value_to_double(&cur),
			sensor_value_to_double(&pow)
			);

		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			char ostentus_buf[32];
			uint8_t slide_num;

			snprintk(ostentus_buf, sizeof(ostentus_buf), "%.02f V",
				 sensor_value_to_double(&vol));
			slide_num = (sensor->ch_num == 0) ? CH0_VOLTAGE : CH1_VOLTAGE;
			ostentus_slide_set(o_dev, slide_num, ostentus_buf, strlen(ostentus_buf));

			snprintk(ostentus_buf, sizeof(ostentus_buf), "%.02f mA",
				 sensor_value_to_double(&cur) * 1000);
			slide_num = (sensor->ch_num == 0) ? CH0_CURRENT : CH1_CURRENT;
			ostentus_slide_set(o_dev, slide_num, ostentus_buf, strlen(ostentus_buf));

			snprintk(ostentus_buf, sizeof(ostentus_buf), "%.02f W",
				 sensor_value_to_double(&pow));
			slide_num = (sensor->ch_num == 0) ? CH0_POWER : CH1_POWER;
			ostentus_slide_set(o_dev, slide_num, ostentus_buf, strlen(ostentus_buf));
		));
	} else {
		return -ENODATA;
	}

	return 0;
}

static int get_raw_sensor_values(adc_node_t *sensor, vcp_raw_t *values, bool get_new_reading)
{
	int err;

	if (get_new_reading) {
		err = get_adc_reading(sensor);
		if (err) {
			return err;
		}
	}

	if (sensor->device_ready) {
		struct sensor_value raw;

		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_INA260_VOLTAGE_RAW,
				   &raw);
		values->voltage = raw.val1;

		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_INA260_CURRENT_RAW,
				   &raw);
		values->current = raw.val1;

		sensor_channel_get(sensor->dev,
				   (enum sensor_channel)SENSOR_CHAN_INA260_POWER_RAW,
				   &raw);
		values->power = raw.val1;

	} else {
		return -ENODATA;
	}

	return 0;
}

static int push_single_adc_to_golioth(vcp_raw_t *single_ch_data, char *single_ch_path)
{
	int err;
	char json_buf[128];

	snprintk(json_buf,
		 sizeof(json_buf),
		 JSON_FMT_SINGLE,
		 single_ch_path,
		 single_ch_data->current,
		 single_ch_path,
		 single_ch_data->voltage,
		 single_ch_path,
		 single_ch_data->power
		 );

	err = golioth_stream_set_async(client,
				       ADC_STREAM_ENDP,
				       GOLIOTH_CONTENT_TYPE_JSON,
				       json_buf,
				       strlen(json_buf),
				       async_error_handler,
				       NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		return err;
	}

	return 0;
}

static void sensor_report_work_handler(struct k_work *work)
{
    app_sensors_read_and_stream();
    k_work_schedule(&_sensor_report_work, K_SECONDS(LOOP_DELAY_S));
}

void app_sensors_read_and_stream(void)
{
    int err;
    static uint16_t counter;
    vcp_raw_t ch0_raw;
    int ch0_invalid;


    if (golioth_client_is_connected(client))
    {
        uint8_t cbor_buf[13];
        ZCBOR_STATE_E(zse, 1, cbor_buf, sizeof(cbor_buf), 1);

        bool ok = zcbor_map_start_encode(zse, 1) &&
                  zcbor_tstr_put_lit(zse, "counter") &&
                  zcbor_uint32_put(zse, counter) &&
                  zcbor_map_end_encode(zse, 1);

        if (!ok)
        {
            LOG_ERR("Failed to encode CBOR.");
            return;
        }

        size_t cbor_size = zse->payload - cbor_buf;

        LOG_DBG("Streaming counter: %d", counter);

        err = golioth_stream_set_async(client,
                                       "sensor",
                                       GOLIOTH_CONTENT_TYPE_CBOR,
                                       cbor_buf,
                                       cbor_size,
                                       async_error_handler,
                                       NULL);
        if (err)
        {
            LOG_ERR("Failed to send sensor data to Golioth: %d", err);
        }
    }
    else
    {
        LOG_DBG("No connection available, skipping streaming counter: %d", counter);
    }


    get_adc_reading(&adc_ch0);
    ch0_invalid = get_raw_sensor_values(&adc_ch0, &ch0_raw, false);
    // log_sensor_values(&adc_ch0, false);
    if (ch0_invalid) {
		LOG_WRN("Solar power data not available");
		return;
	}
    else{
        push_single_adc_to_golioth(&ch0_raw, CH0_PATH);
    }

    ++counter;
}

void app_sensors_set_client(struct golioth_client *sensors_client)
{
    client = sensors_client;
    k_work_init_delayable(&_sensor_report_work, sensor_report_work_handler);
    k_work_schedule(&_sensor_report_work, K_SECONDS(LOOP_DELAY_S));
}
