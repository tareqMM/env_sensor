/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_ZIGBEE_SVC_H
#define APP_ZIGBEE_SVC_H

#include "zb_environmental_sensor.h"

enum zigbee_function {
	ZIGBEE_START_JOINING,
	ZIGBEE_WIPE_DATA,
	ZIGBEE_UPDATE_TEMPERATURE_ATTRIBUTE,
	ZIGBEE_UPDATE_HUMIDITY_ATTRIBUTE,
};

/**
 * @brief Schedules a Zigbee function for execution.
 *
 * @param[in] fn_id The Zigbee function to execute:
 *                  - ZIGBEE_START_JOINING: Start Zigbee network joining.
 *                  - ZIGBEE_WIPE_DATA: Factory reset and wipe Zigbee data.
 *                  - ZIGBEE_UPDATE_TEMPERATURE_ATTRIBUTE: Update temperature attribute.
 *                  - ZIGBEE_UPDATE_HUMIDITY_ATTRIBUTE: Update humidity attribute.
 * @param[in] user_param Data associated with the function (scaled sensor values for updates).
 *
 * @return 0 on success, negative error code on failure.
 *
 * @note The function is executed asynchronously via the Zigbee stack.
 */
int zigbee_svc_schedule_fn(enum zigbee_function fn_id, uint16_t user_param);

/**
 * @brief Starts the Zigbee service.
 *
 * @details Function configures the Zigbee device as a Sleepy End Device (SED),
 *          powers down unused RAM if configured, and enables the Zigbee stack.
 */
void zigbee_svc_start(void);

/**
 * @brief Initializes the Zigbee service.
 */
void zigbee_svc_init(void);

#endif /* APP_ZIGBEE_SVC_H */
