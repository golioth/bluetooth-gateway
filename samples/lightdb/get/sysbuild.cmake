if(BOARD MATCHES "bsim")
  ExternalZephyrProject_Add(
    APPLICATION bsim_2G4_phy
    SOURCE_DIR ${ZEPHYR_BLUETOOTH_GATEWAY_MODULE_DIR}/bsim_bin
    BOARD bsim_2G4_phy
  )
  sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bsim_2G4_phy)

  if(SB_CONFIG_BSIM_HANDBRAKE)
    ExternalZephyrProject_Add(
      APPLICATION bsim_handbrake
      SOURCE_DIR ${ZEPHYR_BLUETOOTH_GATEWAY_MODULE_DIR}/bsim_bin
      BOARD bsim_device/native/handbrake
    )
    sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} bsim_handbrake)
  endif()
endif()
