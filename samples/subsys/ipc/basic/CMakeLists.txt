#
# Copyright (c) 2024 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

if(NOT CONFIG_BOARD_NRF54L15PDK_NRF54L15_CPUAPP)
  message(FATAL_ERROR "${BOARD} is not supported for this sample")
endif()

project(ipc_basic_host)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

target_sources(app PRIVATE src/main.c)

include(ExternalProject)
