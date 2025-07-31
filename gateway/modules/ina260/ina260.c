/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_ina260

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ina260, LOG_LEVEL_DBG);

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/drivers/gpio.h>

#include <zephyr/drivers/sensor/ina260.h>

static int ina260_reg_read(const struct device *dev,
		uint8_t reg_addr,
		uint16_t *reg_data)
{
	const struct ina260_device_config *cfg = dev->config;
	uint8_t rx_buf[2];
	int err;

	err = i2c_write_read_dt(&cfg->bus,
				&reg_addr, sizeof(reg_addr),
				rx_buf, sizeof(rx_buf));

	*reg_data = sys_get_be16(rx_buf);

	return err;
}

static int ina260_sample_fetch(const struct device *dev,
			       enum sensor_channel chan)
{
	int err;
	uint16_t reading;

	struct ina260_data *data = dev->data;

	if (chan != SENSOR_CHAN_ALL &&
		chan != SENSOR_CHAN_VOLTAGE &&
		chan != SENSOR_CHAN_POWER &&
		chan != SENSOR_CHAN_CURRENT) {
		return -ENOTSUP;
	}

	if (chan == SENSOR_CHAN_ALL ||
		chan == SENSOR_CHAN_VOLTAGE) {

		err = ina260_reg_read(dev, INA260_REG_VOLTAGE, &reading);
		if (err) {
			LOG_ERR("Error reading bus voltage.");
			return err;
		}
		data->vol = (int16_t)reading;
	}

	if (chan == SENSOR_CHAN_ALL ||
		chan == SENSOR_CHAN_POWER)	{

		err = ina260_reg_read(dev, INA260_REG_POWER, &reading);
		if (err) {
			LOG_ERR("Error reading power register.");
			return err;
		}
		data->pow = (uint16_t)reading;
	}

	if (chan == SENSOR_CHAN_ALL ||
		chan == SENSOR_CHAN_CURRENT) {

		err = ina260_reg_read(dev, INA260_REG_CURRENT, &reading);
		if (err) {
			LOG_ERR("Error reading current register.");
			return err;
		}
		data->cur = (int16_t)reading;
	}
	return err;
}

static int ina260_channel_get(const struct device *dev,
			      enum sensor_channel chan,
			      struct sensor_value *val)
{
	int err;
	double calculated;

	struct ina260_data *data = dev->data;

	switch ((int16_t)chan) {
	case SENSOR_CHAN_VOLTAGE:
		calculated = ((double)data->vol * 125) / 100000;
		break;
	case SENSOR_CHAN_POWER:
		calculated = (double)data->pow / 100;
		break;
	case SENSOR_CHAN_CURRENT:
		calculated = ((double)data->cur * 125) / 100000;
		break;
	case SENSOR_CHAN_INA260_VOLTAGE_RAW:
		val->val1 = data->vol;
		return 0;
	case SENSOR_CHAN_INA260_CURRENT_RAW:
		val->val1 = data->cur;
		return 0;
	case SENSOR_CHAN_INA260_POWER_RAW:
		val->val1 = data->pow;
		return 0;
	default:
		return -ENOTSUP;
	}

	err = sensor_value_from_double(val, calculated);
	return err;
}

static int ina260_init(const struct device *dev)
{
	const struct ina260_device_config *config = dev->config;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	return 0;
}

static const struct sensor_driver_api ina260_api = {
	.sample_fetch = ina260_sample_fetch,
	.channel_get = ina260_channel_get
};

#define INA260_INIT(n)									\
	static struct ina260_data ina260_data_##n;					\
											\
	static const struct ina260_device_config ina260_device_config_##n = {		\
		.bus = I2C_DT_SPEC_INST_GET(n)						\
	};										\
											\
	SENSOR_DEVICE_DT_INST_DEFINE(n, ina260_init, NULL,				\
			      &ina260_data_##n, &ina260_device_config_##n, POST_KERNEL,	\
			      CONFIG_SENSOR_INIT_PRIORITY, &ina260_api);

DT_INST_FOREACH_STATUS_OKAY(INA260_INIT)
