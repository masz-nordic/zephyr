# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

ExternalZephyrProject_Add(
  APPLICATION remote
  SOURCE_DIR ${APP_DIR}/remote
  BOARD nrf54h20dk/nrf54h20/cpuflpr
)

add_dependencies(${DEFAULT_IMAGE} remote)
sysbuild_add_dependencies(FLASH ${DEFAULT_IMAGE} remote)
