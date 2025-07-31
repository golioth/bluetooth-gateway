/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INA260_H__
#define __INA260_H__

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

/* Register addresses */
#define INA260_REG_CURRENT	0x01
#define INA260_REG_VOLTAGE	0x02
#define INA260_REG_POWER	0x03

/* Calc values */
#define INA260_PER_BIT_MULT 125
#define INA260_CALC_DIVISOR 100000

/* Custom channels */
enum sensor_channel_ina260 {
	/** RAW Voltage Reading **/
	SENSOR_CHAN_INA260_VOLTAGE_RAW = SENSOR_CHAN_PRIV_START,
	/** RAW Current Reading **/
	SENSOR_CHAN_INA260_CURRENT_RAW,
	/** RAW Power Reading **/
	SENSOR_CHAN_INA260_POWER_RAW
};

/* Structs */
struct ina260_device_config {
	struct i2c_dt_spec bus;
};

struct ina260_data {
	int16_t vol;
	int16_t cur;
	uint16_t pow;
};

#endif /* INA260_H__ */
