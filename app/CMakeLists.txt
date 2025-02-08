#
# Copyright (c) 2024 Tareq Mhisen
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES C)

target_sources(app PRIVATE
    src/events_svc.c
    src/main.c
    src/humidity_temperature_svc.c
    src/user_interface.c
    src/zigbee_svc.c
)

# Stop searching of if no zigbee network was found after 15 sec (restarting searching/joining procedure can be triggered again by button press) 
zephyr_compile_definitions(ZB_DEV_REJOIN_TIMEOUT_MS=15000)