#include <zephyr/kernel.h>

#include <zephyr/device.h>

#include <zephyr/drivers/uart.h>

#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>

#include <zigbee/zigbee_app_utils.h>
#include <zcl/zb_zcl_common.h>
#include <zigbee/zigbee_error_handler.h>

#include <zb_nrf_platform.h>

#include <dk_buttons_and_leds.h>

#include "humidity_temperature_svc.h"

#include "zb_environmental_sensor.h"

#include "zigbee_svc.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zigbee_svc, LOG_LEVEL_DBG);

#define LONG_POLL_INTERVAL_MS 64000

/* Weather check period */
#define MEASURING_PERIOD_MSEC (1000 * CONFIG_MEASURING_PERIOD_SECONDS)

/* Delay for first weather check */
#define MEASURING_INITIAL_DELAY_MSEC (1000 * CONFIG_FIRST_MEASURING_DELAY_SECONDS)

/* Button used to enter the Identify mode */
#define IDENTIFY_MODE_BUTTON DK_BTN1_MSK

/* LED used for device identification. */
#define IDENTIFY_LED DK_LED4

/* Button to start Factory Reset */
#define FACTORY_RESET_BUTTON IDENTIFY_MODE_BUTTON

/* Version of the application software (1 byte). */
#define SENSOR_INIT_BASIC_APP_VERSION 01

/* Version of the hardware of the device (1 byte). */
#define SENSOR_INIT_BASIC_HW_VERSION 01

/* Manufacturer name (32 bytes). */
#define SENSOR_INIT_BASIC_MANUF_NAME "SHAM_TBZ"

/* Model number assigned by manufacturer (32-bytes long string). */
#define SENSOR_INIT_BASIC_MODEL_ID "Temperature & Humidity Sensor 0.1"

/* First 8 bytes specify the date of manufacturer of the device
 * in ISO 8601 format (YYYYMMDD). The rest (8 bytes) are manufacturer specific.
 */
#define SENSOR_INIT_BASIC_DATE_CODE "20250108"

/* Describes the type of physical environment.
 * For possible values see section 3.2.2.2.10 of ZCL specification.
 */
#define BULB_INIT_BASIC_PH_ENV ZB_ZCL_BASIC_ENV_UNSPECIFIED

/* Stores all cluster-related attributes */
static struct zb_device_ctx dev_ctx;

/* Declare attribute list for Basic cluster */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &dev_ctx.basic_attr.zcl_version,
				     &dev_ctx.basic_attr.app_version,
				     &dev_ctx.basic_attr.stack_version,
				     &dev_ctx.basic_attr.hw_version, dev_ctx.basic_attr.mf_name,
				     dev_ctx.basic_attr.model_id, dev_ctx.basic_attr.date_code,
				     &dev_ctx.basic_attr.power_source,
				     dev_ctx.basic_attr.location_id, &dev_ctx.basic_attr.ph_env,
				     dev_ctx.basic_attr.sw_ver);

/* Declare attribute list for Identify cluster (client). */
ZB_ZCL_DECLARE_IDENTIFY_CLIENT_ATTRIB_LIST(identify_client_attr_list);

/* Declare attribute list for Identify cluster (server). */
ZB_ZCL_DECLARE_IDENTIFY_SERVER_ATTRIB_LIST(identify_server_attr_list,
					   &dev_ctx.identify_attr.identify_time);

/* Declare attribute list for temperature cluster */
ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(temperature_measurement_attr_list,
					    &dev_ctx.temp_attrs.measure_value,
					    &dev_ctx.temp_attrs.min_measure_value,
					    &dev_ctx.temp_attrs.max_measure_value,
					    &dev_ctx.temp_attrs.tolerance);

/* Declare attribute list for humidity cluster */
ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(humidity_measurement_attr_list,
						    &dev_ctx.humidity_attrs.measure_value,
						    &dev_ctx.humidity_attrs.min_measure_value,
						    &dev_ctx.humidity_attrs.max_measure_value);

/* Clusters setup */
ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_CLUSTER_LIST(environmental_sensor_cluster_list, basic_attr_list,
						identify_client_attr_list,
						identify_server_attr_list,
						temperature_measurement_attr_list,
						humidity_measurement_attr_list);

/* Endpoint setup (single) */
ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_EP(environmental_sensor_ep, ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
				      environmental_sensor_cluster_list);

/* Device context */
ZBOSS_DECLARE_DEVICE_CTX_1_EP(environmental_sensor_ctx, environmental_sensor_ep);

void zigbee_svc_clusters_init(void)
{
	/* Basic cluster attributes */
	dev_ctx.basic_attr.zcl_version = ZB_ZCL_VERSION;
	dev_ctx.basic_attr.app_version = SENSOR_INIT_BASIC_APP_VERSION;
	dev_ctx.basic_attr.hw_version = SENSOR_INIT_BASIC_HW_VERSION;
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;
	dev_ctx.basic_attr.ph_env = BULB_INIT_BASIC_PH_ENV;

	/* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte
	 * should contain string length without trailing zero.
	 *
	 * For example "test" string will be encoded as:
	 *   [(0x4), 't', 'e', 's', 't']
	 */
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name, SENSOR_INIT_BASIC_MANUF_NAME,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id, SENSOR_INIT_BASIC_MODEL_ID,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_MODEL_ID));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.date_code, SENSOR_INIT_BASIC_DATE_CODE,
			      ZB_ZCL_STRING_CONST_SIZE(SENSOR_INIT_BASIC_DATE_CODE));

	/* Identify cluster attributes */
	dev_ctx.identify_attr.identify_time = ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

	/* Temperature */
	dev_ctx.temp_attrs.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MIN;
	dev_ctx.temp_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MAX;
	dev_ctx.temp_attrs.tolerance = ENVIRONMENTAL_SENSOR_ATTR_TEMP_TOLERANCE;

	/* Humidity */
	dev_ctx.humidity_attrs.measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MIN;
	dev_ctx.humidity_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MAX;
	/* Humidity measurements tolerance is not supported at the moment */
}

static void start_identifying(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (ZB_JOINED()) {
		/*
		 * Check if endpoint is in identifying mode,
		 * if not put desired endpoint in identifying mode.
		 */
		if (dev_ctx.identify_attr.identify_time ==
		    ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE) {

			zb_ret_t zb_err_code =
				zb_bdb_finding_binding_target(ENVIRONMENTAL_SENSOR_ENDPOINT_NB);

			if (zb_err_code == RET_OK) {
				LOG_DBG("Manually enter identify mode");
			} else if (zb_err_code == RET_INVALID_STATE) {
				LOG_WRN("RET_INVALID_STATE - Cannot enter identify mode");
			} else {
				ZB_ERROR_CHECK(zb_err_code);
			}
		} else {
			LOG_DBG("Manually cancel identify mode");
			zb_bdb_finding_binding_target_cancel();
		}
	} else {
		LOG_WRN("Device not in a network - cannot identify itself");
	}
}

void zigbee_svc_configure_long_poll_interval(void)
{
	/* Ensure the device has joined the network before setting the poll interval */
	if (ZB_JOINED()) {
		zb_zdo_pim_set_long_poll_interval(LONG_POLL_INTERVAL_MS);
		LOG_DBG("Long Poll interval set to %d ms", LONG_POLL_INTERVAL_MS);
	} else {
		LOG_WRN("Device is not joined to the network. Cannot set Long Poll interval");
	}
}

/**@brief Function to toggle the identify LED
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void toggle_identify_led(zb_bufid_t bufid)
{
	static int blink_status;

	dk_set_led(IDENTIFY_LED, (++blink_status) % 2);
	ZB_SCHEDULE_APP_ALARM(toggle_identify_led, bufid, ZB_MILLISECONDS_TO_BEACON_INTERVAL(100));
}

/**@brief Function to handle identify notification events on the first endpoint.
 *
 * @param  bufid  Unused parameter, required by ZBOSS scheduler API.
 */
static void identify_callback(zb_bufid_t bufid)
{
	zb_ret_t zb_err_code;

	if (bufid) {
		/* Schedule a self-scheduling function that will toggle the LED */
		ZB_SCHEDULE_APP_CALLBACK(toggle_identify_led, bufid);
	} else {
		/* Cancel the toggling function alarm and turn off LED */
		zb_err_code = ZB_SCHEDULE_APP_ALARM_CANCEL(toggle_identify_led, ZB_ALARM_ANY_PARAM);
		ZVUNUSED(zb_err_code);

		dk_set_led(IDENTIFY_LED, 0);
	}
}

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (IDENTIFY_MODE_BUTTON & has_changed) {
		if (IDENTIFY_MODE_BUTTON & button_state) {
			/* Button changed its state to pressed */
		} else {
			/* Button changed its state to released */
			if (was_factory_reset_done()) {
				/* The long press was for Factory Reset */
				LOG_DBG("After Factory Reset - ignore button release");
			} else {
				/* Button released before Factory Reset */

				/* Start identification mode */
				zb_ret_t ret = ZB_SCHEDULE_APP_CALLBACK(start_identifying, 0);

				if (ret) {
					LOG_ERR("Failed to schedule app callback: %d", ret);
				}

				/* Inform default signal handler about user input at the device */
				user_input_indicate();
			}
		}
	}

	check_factory_reset_button(button_state, has_changed);
}

static void gpio_init(void)
{
	int ret = dk_buttons_init(button_changed);

	if (ret) {
		LOG_ERR("Cannot init buttons (ret: %d)", ret);
	}

	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("Cannot init LEDs (ret: %d)", ret);
	}
}

static void check_weather(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	int ret = humidity_temperature_svc_trigger_measurement();

	if (ret) {
		LOG_ERR("Failed to check weather: %d", ret);
	} else {
		ret = humidity_temperature_svc_update_zb_attributes();
		if (ret) {
			LOG_ERR("Failed to update humidity and temperature attributes: %d", ret);
		}
	}

	zb_ret_t zb_err = ZB_SCHEDULE_APP_ALARM(
		check_weather, 0, ZB_MILLISECONDS_TO_BEACON_INTERVAL(MEASURING_PERIOD_MSEC));
	if (zb_err) {
		LOG_ERR("Failed to schedule app alarm: %d", zb_err);
	}
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	zb_zdo_app_signal_hdr_t *signal_header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);

	/* Detect ZBOSS startup */
	switch (signal) {
	case ZB_ZDO_SIGNAL_SKIP_STARTUP:
		/* ZBOSS framework has started - schedule first weather check */
		status = ZB_SCHEDULE_APP_ALARM(
			check_weather, 0,
			ZB_MILLISECONDS_TO_BEACON_INTERVAL(MEASURING_INITIAL_DELAY_MSEC));
		if (status) {
			LOG_ERR("Failed to schedule app alarm: %d", status);
		}
		break;

	case ZB_COMMON_SIGNAL_CAN_SLEEP: {
		zb_uint32_t sleep_duration = *(zb_uint32_t *)signal_header;
		if (sleep_duration >= 100U) {
			LOG_WRN("Entering sleep mode for %u ms", sleep_duration);
			zb_sleep_now();
		}
		break;
	}
	default:
		LOG_WRN("Unhandled Zigbee signal: %d", signal);
		break;
	}

	/* Let default signal handler process the signal*/
	ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

	/*
	 * All callbacks should either reuse or free passed buffers.
	 * If bufid == 0, the buffer is invalid (not passed).
	 */
	if (bufid) {
		zb_buf_free(bufid);
	}
}

void zigbee_svc_start(void)
{
	/* Enable Sleepy End Device behavior */
	zb_set_rx_on_when_idle(ZB_FALSE);
	if (IS_ENABLED(CONFIG_RAM_POWER_DOWN_LIBRARY)) {
		power_down_unused_ram();
	}

	/* Start Zigbee stack */
	zigbee_enable();

	LOG_INF("Zigbee environmental sensor started");
}

void zigbee_svc_init(void)
{
	register_factory_reset_button(FACTORY_RESET_BUTTON);

	gpio_init();

	/* Register device context (endpoint) */
	ZB_AF_REGISTER_DEVICE_CTX(&environmental_sensor_ctx);

	/* Init Basic and Identify and measurements-related attributes */
	zigbee_svc_clusters_init();

	/* Register callback to identify notifications */
	ZB_AF_SET_IDENTIFY_NOTIFICATION_HANDLER(ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
						identify_callback);
}
