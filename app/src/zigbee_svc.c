/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <ram_pwrdn.h>

#include <zboss_api.h>
#include <zboss_api_addons.h>
#include <zb_nrf_platform.h>
#include <zigbee/zigbee_app_utils.h>
#include <zigbee/zigbee_error_handler.h>

#include "events_svc.h"
#include "humidity_temperature_svc.h"
#include "user_interface.h"
#include "zigbee_svc.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zigbee_svc, LOG_LEVEL_DBG);

#define KEEP_ALIVE_PERIOD_MSEC (1000 * CONFIG_KEEP_ALIVE_PERIOD_SECONDS)
#define LONG_POLL_PERIOD_MSEC  (1000 * CONFIG_LONG_POLL_PERIOD_SECONDS)
#define IEEE_ADDR_BUF_SIZE     17

/* Stores all cluster-related attributes */
static struct zb_device_ctx dev_ctx;
static bool zigbee_data_wiped;

/* Declare attribute list for Basic cluster */
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list, &dev_ctx.basic_attr.zcl_version, NULL, NULL,
				     NULL, dev_ctx.basic_attr.mf_name, dev_ctx.basic_attr.model_id,
				     dev_ctx.basic_attr.date_code, &dev_ctx.basic_attr.power_source,
				     NULL, NULL, NULL);

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
	dev_ctx.basic_attr.power_source = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY;

	/* Use ZB_ZCL_SET_STRING_VAL to set strings, because the first byte
	 * should contain string length without trailing zero.
	 *
	 * For example "test" string will be encoded as:
	 *   [(0x4), 't', 'e', 's', 't']
	 */
	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.mf_name, CONFIG_SENSOR_INIT_BASIC_MANUF_NAME,
			      ZB_ZCL_STRING_CONST_SIZE(CONFIG_SENSOR_INIT_BASIC_MANUF_NAME));

	ZB_ZCL_SET_STRING_VAL(dev_ctx.basic_attr.model_id, CONFIG_SENSOR_INIT_BASIC_MODEL_ID,
			      ZB_ZCL_STRING_CONST_SIZE(CONFIG_SENSOR_INIT_BASIC_MODEL_ID));

	/* Temperature */
	dev_ctx.temp_attrs.measure_value = ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.temp_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MIN;
	dev_ctx.temp_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_TEMP_MAX;
	dev_ctx.temp_attrs.tolerance = ENVIRONMENTAL_SENSOR_ATTR_TEMP_TOLERANCE;

	/* Humidity */
	dev_ctx.humidity_attrs.measure_value = ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_UNKNOWN;
	dev_ctx.humidity_attrs.min_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MIN;
	dev_ctx.humidity_attrs.max_measure_value = ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MAX;
}

static void zigbee_svc_update_temperature_attribute(zb_bufid_t bufid, zb_uint16_t temperature)
{
	ZVUNUSED(bufid);
	/* Convert measured value to attribute value, as specified in ZCL */
	zb_int16_t temperature_attribute = (zb_int16_t)temperature;
	zb_zcl_status_t status;

	/* Set ZCL attribute */
	status = zb_zcl_set_attr_val(ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
				     ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ZB_ZCL_CLUSTER_SERVER_ROLE,
				     ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
				     (zb_uint8_t *)&temperature_attribute, ZB_FALSE);
	if (status != RET_OK) {
		LOG_ERR("Failed to update ZCL temperature attribute: %d", status);
	}
}

static void zigbee_svc_update_humidity_attribute(zb_bufid_t bufid, zb_uint16_t humidity)
{
	ZVUNUSED(bufid);
	zb_uint16_t humidity_attribute = humidity;
	zb_zcl_status_t status;

	/* Set ZCL attribute */
	status = zb_zcl_set_attr_val(
		ENVIRONMENTAL_SENSOR_ENDPOINT_NB, ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
		ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID,
		(zb_uint8_t *)&humidity_attribute, ZB_FALSE);
	if (status != RET_OK) {
		LOG_ERR("Failed to update ZCL humidity attribute: %d", status);
	}
}

static void log_reporting_info(zb_uint8_t endpoint, zb_uint16_t cluster_id, zb_uint16_t attr_id)
{
	zb_zcl_reporting_info_t *rep_info;

	rep_info = zb_zcl_find_reporting_info(endpoint, cluster_id, ZB_ZCL_CLUSTER_SERVER_ROLE,
					      attr_id);
	if (rep_info) {
		LOG_INF("Reporting info for Endpoint: %d, Cluster: 0x%04X, Attribute: 0x%04X",
			endpoint, cluster_id, attr_id);
		LOG_INF("DEF Min Interval: %d, DEF Max Interval: %d",
			rep_info->u.send_info.def_min_interval,
			rep_info->u.send_info.def_max_interval);
		LOG_INF("Min Interval: %d, Max Interval: %d", rep_info->u.send_info.min_interval,
			rep_info->u.send_info.max_interval);
		LOG_INF("Reportable Change: %d", rep_info->u.send_info.delta.s16);
	} else {
		LOG_WRN("No reporting info found for Endpoint: %d, Cluster: 0x%04X, Attribute: "
			"0x%04X",
			endpoint, cluster_id, attr_id);
	}
}

static void start_joining(zb_bufid_t bufid)
{
	ZVUNUSED(bufid);

	if (!ZB_JOINED()) {
		LOG_WRN("Device not in a network -> Restart joining procedure");
		bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
	} else {
		LOG_INF("Device is already in a network");
		ui_flash_status_led(250);
	}
}

int zigbee_svc_schedule_fn(enum zigbee_function fn_id, uint16_t user_param)
{
	zb_ret_t ret = 0;

	switch (fn_id) {

	case ZIGBEE_START_JOINING:
		ARG_UNUSED(user_param);
		ret = ZB_SCHEDULE_APP_CALLBACK(start_joining, 0);
		if (ret) {
			LOG_ERR("Failed to schedule start_joining function!: %d", ret);
		}
		break;

	case ZIGBEE_WIPE_DATA:
		ARG_UNUSED(user_param);
		LOG_WRN("Performing factory reset . . .");
		ret = ZB_SCHEDULE_APP_CALLBACK(zb_bdb_reset_via_local_action, 0);
		if (ret) {
			LOG_ERR("Failed to schedule zb_bdb_reset_via_local_action function!: %d",
				ret);
		}
		zigbee_erase_persistent_storage(true);
		zigbee_data_wiped = true;
		break;

	case ZIGBEE_UPDATE_TEMPERATURE_ATTRIBUTE:
		ret = ZB_SCHEDULE_APP_CALLBACK2(zigbee_svc_update_temperature_attribute, 0,
						user_param);
		if (ret) {
			LOG_ERR("Failed to schedule zigbee_svc_update_zb_attributes function!: %d",
				ret);
		}
		break;

	case ZIGBEE_UPDATE_HUMIDITY_ATTRIBUTE:
		ret = ZB_SCHEDULE_APP_CALLBACK2(zigbee_svc_update_humidity_attribute, 0,
						user_param);
		if (ret) {
			LOG_ERR("Failed to schedule zigbee_svc_update_zb_attributes function!: %d",
				ret);
		}
		break;

	default:
		break;
	}

	return ret;
}

void zboss_signal_handler(zb_bufid_t bufid)
{
	int ret;
	zb_zdo_app_signal_hdr_t *signal_header = NULL;
	zb_zdo_app_signal_type_t signal = zb_get_app_signal(bufid, &signal_header);
	zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);
	/* Flag indicating if a joining signal has been received since restart or leave with rejoin.
	 */
	static bool joining_signal_received = false;

	switch (signal) {
	case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
		/* fall-through */
	case ZB_BDB_SIGNAL_DEVICE_REBOOT:
		/* fall-through */
	case ZB_BDB_SIGNAL_STEERING:
		/* Call default signal handler. */
		ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));

		if (ZB_JOINED()) {
			joining_signal_received = true;

			struct event evt = {.type = EVENT_NETWORK_CONNECTED};
			ret = events_svc_send_event(&evt);
			if (ret != 0) {
				LOG_ERR("Unable to send network connected event. ret %d", ret);
			}

			log_reporting_info(ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
					   ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
					   ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID);
			log_reporting_info(ENVIRONMENTAL_SENSOR_ENDPOINT_NB,
					   ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,
					   ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID);
		} else {
			LOG_WRN("Device is not connected to any network!");

			struct event evt = {.type = EVENT_NETWORK_NOT_CONNECTED};
			ret = events_svc_send_event(&evt);
			if (ret != 0) {
				LOG_ERR("Unable to send network disconnected event. ret %d", ret);
			}
		}
		break;

	case ZB_ZDO_SIGNAL_LEAVE:
		if (status == RET_OK) {
			zb_zdo_signal_leave_params_t *leave_params = ZB_ZDO_SIGNAL_GET_PARAMS(
				signal_header, zb_zdo_signal_leave_params_t);
			LOG_INF("Network left (leave type: %d)", leave_params->leave_type);

			/* Set joining_signal_received to false to detect a broken rejoin procedure.
			 */
			if (leave_params->leave_type == ZB_NWK_LEAVE_TYPE_REJOIN) {
				joining_signal_received = false;
			}

			struct event evt;
			evt.type = EVENT_NETWORK_NOT_CONNECTED;

			if (zigbee_data_wiped) {
				evt.type = EVENT_ZIGBEE_DATA_WIPED;
			}

			ret = events_svc_send_event(&evt);
			if (ret != 0) {
				LOG_ERR("Unable to send network disconnected event. ret %d", ret);
			}
		}
		break;

	case ZB_NLME_STATUS_INDICATION: {
		zb_zdo_signal_nlme_status_indication_params_t *nlme_status_ind =
			ZB_ZDO_SIGNAL_GET_PARAMS(signal_header,
						 zb_zdo_signal_nlme_status_indication_params_t);
		if (nlme_status_ind->nlme_status.status ==
		    ZB_NWK_COMMAND_STATUS_PARENT_LINK_FAILURE) {
			LOG_WRN("Parent link failure detected.");

			/* Check for a broken rejoin procedure and restart the device to recover. */
			if (!joining_signal_received) {
				LOG_ERR("Rejoin procedure failed. Resetting device.");
				ret = zigbee_svc_schedule_fn(ZIGBEE_WIPE_DATA, 0);
				if (ret != 0) {
					LOG_ERR("Failed to wipe zigbee data!");
				}
			} else {
				ret = ZB_SCHEDULE_APP_CALLBACK(start_joining, 0);
				if (ret) {
					LOG_ERR("Failed to schedule start_joining function!: %d",
						ret);
				}
			}
		}
	} break;

	default:
		/* Let default signal handler process the signal */
		ZB_ERROR_CHECK(zigbee_default_signal_handler(bufid));
		break;
	}

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

	zb_set_ed_timeout(CONFIG_NWK_ED_DEVICE_TIMEOUT_INDEX);
	zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(KEEP_ALIVE_PERIOD_MSEC));

	/* Start Zigbee stack */
	zigbee_enable();

	LOG_INF("Zigbee environmental sensor started");
}

void zigbee_svc_init(void)
{
	/* Register device context (endpoint) */
	ZB_AF_REGISTER_DEVICE_CTX(&environmental_sensor_ctx);

	/* Init Basic and Identify and measurements-related attributes */
	zigbee_svc_clusters_init();
	zigbee_svc_update_humidity_attribute(0, 0);
	zigbee_svc_update_temperature_attribute(0, 0);
}
