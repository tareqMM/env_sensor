/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "humidity_temperature_svc.h"
#include "zigbee_svc.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	zigbee_svc_init();

	humidity_temperature_svc_init();

	zigbee_svc_start();

	return 0;
}
