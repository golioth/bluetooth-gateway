zephyr_library()

zephyr_library_sources(bt/cert.c)
zephyr_library_sources(bt/connect.c)
zephyr_library_sources(bt/downlink.c)
zephyr_library_sources(bt/scan.c)
zephyr_library_sources(bt/uplink.c)
zephyr_library_sources(block.c)
zephyr_library_sources(cert.c)
zephyr_library_sources(downlink.c)
zephyr_library_sources(uplink.c)

zephyr_library_get_current_dir_lib_name(${ZEPHYR_BASE} lib_name)

generate_inc_file_for_target(${lib_name}
  server.der
  ${ZEPHYR_BINARY_DIR}/include/generated/pouch_gateway_server.der.inc)
