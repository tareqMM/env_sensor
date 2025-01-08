/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_ENVIRONMENTAL_SENSORS_H_
#define APP_ENVIRONMENTAL_SENSORS_H_

#include <zephyr/drivers/sensor.h>

/* Measurements ranges for SHT40 sensor */
#define SENSOR_TEMP_CELSIUS_MIN           (-40)
#define SENSOR_TEMP_CELSIUS_MAX           (125)
#define SENSOR_TEMP_CELSIUS_TOLERANCE     (0.2)
#define SENSOR_HUMIDITY_PERCENT_MIN       (0)
#define SENSOR_HUMIDITY_PERCENT_MAX       (100)
#define SENSOR_HUMIDITY_PERCENT_TOLERANCE (1.8)

/**
 * @brief Triggers a new measurement for humidity and temperature.
 *
 * @return 0 on success, or a negative error code if the measurement fails.
 */
int humidity_temperature_svc_trigger_measurement(void);

/**
 * @brief Updates the Zigbee attributes for temperature and humidity.
 *
 * This function reads the temperature and humidity values from the sensor,
 * converts them to the Zigbee ZCL attribute format, and updates the
 * corresponding ZCL attributes for the environmental sensor.
 *
 * @return 0 on success, or a negative error code if an error occurs.
 */
int humidity_temperature_svc_update_zb_attributes(void);

/**
 * @brief Initialize the humidity and temperature sensor.
 *
 * @return 0 on success, or -ENODEV on failure
 */
int humidity_temperature_svc_init(void);

#endif
