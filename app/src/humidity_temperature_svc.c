/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "zb_environmental_sensor.h"

#include "humidity_temperature_svc.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(humidity_temperature_svc, LOG_LEVEL_DBG);

static const struct device *const rh_temp_dev = DEVICE_DT_GET_ONE(sensirion_sht3xd);

struct humidity_temperature_data {
	struct sensor_value humidity;
	struct sensor_value temperature;
};

static struct humidity_temperature_data data;

/*
 * Sensor value is represented as having an integer and a fractional part,
 * and can be obtained using the formula val1 + val2 * 10^(-6).
 */
#define SENSOR_VAL2_DIVISOR 1000000

/*
 * Sensor value is represented as having an integer and a fractional part,
 * and can be obtained using the formula val1 + val2 * 10^(-6). Negative
 * values also adhere to the above formula, but may need special attention.
 * Here are some examples of the value representation:
 *
 *      0.5: val1 =  0, val2 =  500000
 *     -0.5: val1 =  0, val2 = -500000
 *     -1.0: val1 = -1, val2 =  0
 *     -1.5: val1 = -1, val2 = -500000
 */
static float convert_sensor_value(struct sensor_value value)
{
	float result = 0.0f;

	/* Determine sign */
	result = (value.val1 < 0 || value.val2 < 0) ? -1.0f : 1.0f;

	/* Use absolute values */
	value.val1 = value.val1 < 0 ? -value.val1 : value.val1;
	value.val2 = value.val2 < 0 ? -value.val2 : value.val2;

	/* Calculate value */
	result *= (value.val1 + value.val2 / (float)SENSOR_VAL2_DIVISOR);

	return result;
}

int humidity_temperature_svc_trigger_measurement(void)
{
	int ret;

	ret = sensor_sample_fetch(rh_temp_dev);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int humidity_temperature_svc_update_zb_attributes(void)
{
	int err = 0;
	float measured_temperature = 0.0f;
	float measured_humidity = 0.0f;
	int16_t temperature_attribute = 0;
	int16_t humidity_attribute = 0;

	err = sensor_channel_get(rh_temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &data.temperature);
	if (err) {
		LOG_ERR("Failed to get sensor channel: %d", err);
	} else {
		LOG_INF("Temperature: %3d.%06d [*C]", data.temperature.val1, data.temperature.val2);
		measured_temperature = convert_sensor_value(data.temperature);
		/* Convert measured value to attribute value, as specified in ZCL */
		temperature_attribute =
			(int16_t)(measured_temperature *
				  ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);

		/* Set ZCL attribute */
		zb_zcl_status_t status = zb_zcl_set_attr_val(
			ENVIRONMENTAL_SENSOR_ENDPOINT_NB, ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
			ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&temperature_attribute, ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			err = status;
		}
	}

	err = sensor_channel_get(rh_temp_dev, SENSOR_CHAN_HUMIDITY, &data.humidity);
	if (err) {
		LOG_ERR("Failed to get sensor channel: %d", err);
	} else {
		LOG_INF("Sensor    H:%3d.%06d [%%]", data.humidity.val1, data.humidity.val2);
		measured_humidity = convert_sensor_value(data.humidity);
		/* Convert measured value to attribute value, as specified in ZCL */
		humidity_attribute = (int16_t)(measured_humidity *
					       ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		/* Set ZCL attribute */
		zb_zcl_status_t status = zb_zcl_set_attr_val(
			ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ZB_ZCL_CLUSTER_SERVER_ROLE,
			ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
			(zb_uint8_t *)&humidity_attribute, ZB_FALSE);
		if (status) {
			LOG_ERR("Failed to set ZCL attribute: %d", status);
			err = status;
		}
	}

	return err;
}

int humidity_temperature_svc_init(void)
{
	if (!device_is_ready(rh_temp_dev)) {
		LOG_ERR("Failed to initialize humidity and temperature sensor!");
		return -ENODEV;
	}

	data.humidity.val1 = 0;
	data.humidity.val2 = 0;
	data.temperature.val1 = 0;
	data.temperature.val2 = 0;

	LOG_DBG("Humidity and temperature sensor initialized successfully");
	return 0;
}
