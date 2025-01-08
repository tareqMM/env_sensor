/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APP_ENVIRONMENTAL_SENSOR_H
#define APP_ENVIRONMENTAL_SENSOR_H

#include <zcl/zb_zcl_temp_measurement_addons.h>
#include <zcl/zb_zcl_basic_addons.h>

/* Zigbee Cluster Library 4.4.2.2.1.1: MeasuredValue = 100x temperature in degrees Celsius */
#define ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER 100
/* Zigbee Cluster Library 4.7.2.1.1: MeasuredValue = 100x water content in % */
#define ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER    100

/* Measurements ranges scaled for attribute values */
#define ENVIRONMENTAL_SENSOR_ATTR_TEMP_MIN                                                         \
	(SENSOR_TEMP_CELSIUS_MIN * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER)
#define ENVIRONMENTAL_SENSOR_ATTR_TEMP_MAX                                                         \
	(SENSOR_TEMP_CELSIUS_MAX * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER)
#define ENVIRONMENTAL_SENSOR_ATTR_TEMP_TOLERANCE                                                   \
	(SENSOR_TEMP_CELSIUS_TOLERANCE * ZCL_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_MULTIPLIER)

#define ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MIN                                                     \
	(SENSOR_HUMIDITY_PERCENT_MIN * ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER)
#define ENVIRONMENTAL_SENSOR_ATTR_HUMIDITY_MAX                                                     \
	(SENSOR_HUMIDITY_PERCENT_MAX * ZCL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_MULTIPLIER)

/* Number chosen for the single endpoint provided by weather station */
#define ENVIRONMENTAL_SENSOR_ENDPOINT_NB 42

/* Temperature sensor device version */
#define ZB_HA_DEVICE_VER_TEMPERATURE_SENSOR       0
/* Basic, temperature, humidity */
#define ZB_HA_ENVIRONMENTAL_SENSOR_IN_CLUSTER_NUM 3

#define ZB_HA_ENVIRONMENTAL_SENSOR_OUT_CLUSTER_NUM 0

/* Temperature, humidity */
#define ZB_HA_ENVIRONMENTAL_SENSOR_REPORT_ATTR_COUNT 2

/** @brief Declare cluster list for environmental sensor device
    @param cluster_list_name - cluster list variable name
    @param basic_attr_list - attribute list for Basic cluster
    @param temperature_measurement_attr_list - attribute list for temperature measurement cluster
    @param humidity_measurement_attr_list - attribute list for humidity measurement cluster
 */
#define ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_CLUSTER_LIST(cluster_list_name, basic_attr_list,        \
							temperature_measurement_attr_list,         \
							humidity_measurement_attr_list)            \
	zb_zcl_cluster_desc_t cluster_list_name[] = {                                              \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC,                                       \
				    ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),             \
				    (basic_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,                 \
				    ZB_ZCL_MANUF_CODE_INVALID),                                    \
		ZB_ZCL_CLUSTER_DESC(                                                               \
			ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                                        \
			ZB_ZCL_ARRAY_SIZE(temperature_measurement_attr_list, zb_zcl_attr_t),       \
			(temperature_measurement_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,           \
			ZB_ZCL_MANUF_CODE_INVALID),                                                \
		ZB_ZCL_CLUSTER_DESC(                                                               \
			ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,                                \
			ZB_ZCL_ARRAY_SIZE(humidity_measurement_attr_list, zb_zcl_attr_t),          \
			(humidity_measurement_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,              \
			ZB_ZCL_MANUF_CODE_INVALID),                                                \
	}

#define ZB_ZCL_DECLARE_ENVIRONMENTAL_SENSOR_DESC(ep_name, ep_id, in_clust_num, out_clust_num)      \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);                                       \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num)                                        \
	simple_desc_##ep_name = {ep_id,                                                            \
				 ZB_AF_HA_PROFILE_ID,                                              \
				 ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,                               \
				 ZB_HA_DEVICE_VER_TEMPERATURE_SENSOR,                              \
				 0,                                                                \
				 in_clust_num,                                                     \
				 out_clust_num,                                                    \
				 {                                                                 \
					 ZB_ZCL_CLUSTER_ID_BASIC,                                  \
					 ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,                       \
					 ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT,               \
				 }}

#define ZB_HA_DECLARE_ENVIRONMENTAL_SENSOR_EP(ep_name, ep_id, cluster_list)                        \
	ZB_ZCL_DECLARE_ENVIRONMENTAL_SENSOR_DESC(ep_name, ep_id,                                   \
						 ZB_HA_ENVIRONMENTAL_SENSOR_IN_CLUSTER_NUM,        \
						 ZB_HA_ENVIRONMENTAL_SENSOR_OUT_CLUSTER_NUM);      \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info##ep_name,                                \
					   ZB_HA_ENVIRONMENTAL_SENSOR_REPORT_ATTR_COUNT);          \
	ZB_AF_DECLARE_ENDPOINT_DESC(                                                               \
		ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL,                                      \
		ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list,              \
		(zb_af_simple_desc_1_1_t *)&simple_desc_##ep_name,                                 \
		ZB_HA_ENVIRONMENTAL_SENSOR_REPORT_ATTR_COUNT, reporting_info##ep_name, 0, NULL)

/**@brief Temperature Measurement cluster attributes according to ZCL Spec 4.7.2.2.1. */
struct zb_zcl_humidity_measurement_attrs_t {
	zb_uint16_t measure_value;
	zb_uint16_t min_measure_value;
	zb_uint16_t max_measure_value;
};

struct zb_device_ctx {
	zb_zcl_basic_attrs_ext_t basic_attr;
	zb_zcl_temp_measurement_attrs_t temp_attrs;
	struct zb_zcl_humidity_measurement_attrs_t humidity_attrs;
};

#endif /* APP_ENVIRONMENTAL_SENSOR_H */
