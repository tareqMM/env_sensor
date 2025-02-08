/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "events_svc.h"
#include "humidity_temperature_svc.h"
#include "user_interface.h"
#include "zigbee_svc.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define MEASUREMENT_PERIOD_MSEC      (1000 * CONFIG_MEASURING_PERIOD_SECONDS)
#define FIRST_MEASUREMENT_DELAY_MSEC (1000 * CONFIG_FIRST_MEASUREMENT_DELAY_SECONDS)

static void measuring_work_handler(struct k_work *_work)
{
	int ret;
	struct k_work_delayable *work = k_work_delayable_from_work(_work);

	ret = humidity_temperature_svc_trigger_measurement();
	if (ret != 0) {
		LOG_ERR("Failed to trigger humidity and temperature measurement: %d", ret);
	} else {
		ret = zigbee_svc_schedule_fn(
			ZIGBEE_UPDATE_TEMPERATURE_ATTRIBUTE,
			humidity_temperature_svc_get_temperature() *
				ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		if (ret != 0) {
			LOG_ERR("Failed to update ZCL temperature attribute!");
		}

		ret = zigbee_svc_schedule_fn(
			ZIGBEE_UPDATE_HUMIDITY_ATTRIBUTE,
			humidity_temperature_svc_get_humidity() *
				ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER);
		if (ret != 0) {
			LOG_ERR("Failed to update ZCL humidity attribute!");
		}
	}

	k_work_reschedule(work, K_MSEC(MEASUREMENT_PERIOD_MSEC));
}
K_WORK_DELAYABLE_DEFINE(measuring_work, measuring_work_handler);

static void btn_callback(enum button_evt evt)
{
	int ret;
	switch (evt) {
	case BUTTON_EVT_PRESSED_1_SEC:
		ret = zigbee_svc_schedule_fn(ZIGBEE_START_JOINING, 0);
		if (ret != 0) {
			LOG_ERR("Failed to start joining procedure!");
		}

		break;

	case BUTTON_EVT_PRESSED_3_SEC:
		break;

	case BUTTON_EVT_PRESSED_10_SEC:
		ret = zigbee_svc_schedule_fn(ZIGBEE_WIPE_DATA, 0);
		if (ret != 0) {
			LOG_ERR("Failed to wipe zigbee data!");
		}
		break;

	default:
		break;
	}
}

int main(void)
{
	int ret;

	LOG_INF("Starting up .. .. ..");

	ret = humidity_temperature_svc_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize humidity and temperature service!");
	}

	if (ui_gpio_init() != 0) {
		LOG_ERR("Failed to initialize GPIOs!");
	}

	ui_register_button_callback(btn_callback);

	zigbee_svc_init();

	zigbee_svc_start();

	while (true) {
		struct event evt;

		/* Wait for the next event */
		ret = events_svc_get_event(&evt);
		if (ret != 0) {
			LOG_WRN("Unable to get event. Err: %d", ret);
			continue;
		}

		LOG_INF("Event: %s", events_svc_type_to_text(evt.type));

		switch (evt.type) {
		case EVENT_NETWORK_CONNECTED:
			k_work_reschedule(&measuring_work,
					  K_MSEC(CONFIG_FIRST_MEASUREMENT_DELAY_SECONDS));
			break;

		case EVENT_NETWORK_NOT_CONNECTED:
			struct k_work_sync sync;
			k_work_cancel_delayable_sync(&measuring_work, &sync);
			break;

		case EVENT_ZIGBEE_DATA_WIPED:
			/* Trigger software reboot after performing factory reset */
			LOG_WRN("Rebooting device after performing factory reset . . .");
			k_msleep(1000);
			(void)ui_set_status_led_off();
			sys_reboot(SYS_REBOOT_COLD);
			break;

		default:
			break;
		}
	}
	return 0;
}
