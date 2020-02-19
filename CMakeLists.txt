# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# Must define custom board before includeing boilerplate
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD xiaomi_bt_sensor_dev)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)


project(onoff-app)

target_sources(app PRIVATE src/main.c)
