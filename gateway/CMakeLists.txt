# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gateway)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/block.c
  src/bt/connect.c
  src/bt/cert.c
  src/bt/downlink.c
  src/bt/scan.c
  src/bt/uplink.c
  src/cert.c
  src/downlink.c
  src/uplink.c
  src/main.c
)

generate_inc_file_for_target(app
  src/server.der
  ${ZEPHYR_BINARY_DIR}/include/generated/server.der.inc)
