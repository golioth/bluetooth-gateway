# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/modules/ina260)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gateway)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/app_sensors.c
  src/block.c
  src/bt/connect.c
  src/bt/downlink.c
  src/bt/scan.c
  src/bt/uplink.c
  src/downlink.c
  src/uplink.c
  src/main.c
)


