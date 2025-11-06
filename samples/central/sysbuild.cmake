if(BOARD MATCHES "bsim")
  ExternalZephyrProject_Add(
    APPLICATION bsim_2G4_phy
    SOURCE_DIR ${ZEPHYR_POUCH_GATEWAY_MODULE_DIR}/bsim_bin
    BOARD bsim_2G4_phy
  )
  sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bsim_2G4_phy)

  if(SB_CONFIG_BSIM_HANDBRAKE)
    ExternalZephyrProject_Add(
      APPLICATION bsim_handbrake
      SOURCE_DIR ${ZEPHYR_POUCH_GATEWAY_MODULE_DIR}/bsim_bin
      BOARD bsim_device/native/handbrake
    )
    sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bsim_handbrake)
  endif()

  ExternalZephyrProject_Add(
    APPLICATION peripheral
    SOURCE_DIR ${ZEPHYR_BASE}/samples/bluetooth/peripheral
  )
  sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} peripheral)

  # Workaround for nrf52_bsim with Zephyr 4.0.0
  # TODO; remove after Zephyr bump
  set_config_bool(peripheral CONFIG_PICOLIBC y)
endif()
