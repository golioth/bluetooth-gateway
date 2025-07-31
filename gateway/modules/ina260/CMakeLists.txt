# SPDX-License-Identifier: Apache-2.0

if (CONFIG_INA260)
zephyr_library()
zephyr_include_directories(include)
zephyr_library_sources(ina260.c)
endif (CONFIG_INA260)
