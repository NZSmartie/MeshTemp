# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/BU9795
  )

list(APPEND SYSCALL_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/BU9795/zephyr
  )

# Because our out of tree driver has it's own bindings, we need to include it in the DTS_ROOT
list(APPEND DTS_ROOT
  ${CMAKE_CURRENT_SOURCE_DIR}/drivers/BU9795/zephyr)

# Must define custom board before includeing boilerplate
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(BOARD xiaomi_bt_sensor_dev)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)


project(onoff-app)

target_sources(app PRIVATE src/main.c)
