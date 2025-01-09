/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_ZIGBEE_SVC_H
#define APP_ZIGBEE_SVC_H

/**
 * @brief Starts the Zigbee service.
 *
 * @details function configures the Zigbee device as a Sleepy End Device (SED),
 *          powers down unused RAM if configured, and enables the Zigbee stack.
 */
void zigbee_svc_start(void);

/**
 * @brief Initializes the Zigbee service.
 */
void zigbee_svc_init(void);

#endif /* APP_ZIGBEE_SVC_H */
