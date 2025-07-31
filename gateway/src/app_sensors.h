/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SENSORS_H__
#define __APP_SENSORS_H__

#include <stdint.h>
#include <zephyr/drivers/spi.h>
#include <golioth/client.h>


typedef struct {
	const struct  device *const dev;
	uint8_t ch_num;
	int64_t laston;
	uint64_t runtime;
	uint64_t total_unreported;
	uint64_t total_cloud;
	bool loaded_from_cloud;
	bool device_ready;
} adc_node_t;

typedef struct {
	int16_t current;
	int16_t voltage;
	uint16_t power;
} vcp_raw_t;

void app_sensors_set_client(struct golioth_client *sensors_client);
void app_sensors_read_and_stream(void);

#endif /* __APP_SENSORS_H__ */
