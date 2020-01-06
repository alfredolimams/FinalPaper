# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/../)
include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(central)

list(APPEND HEADERS
    "${PROJECT_SOURCE_DIR}/inc/"
)

list(APPEND SOURCES
    "${PROJECT_SOURCE_DIR}/src/main.c"
    "${PROJECT_SOURCE_DIR}/src/bt.c"
    "${PROJECT_SOURCE_DIR}/src/dfu_ble.c"
    "${PROJECT_SOURCE_DIR}/src/image.c"
)

include_directories(${HEADERS})
target_sources(app PRIVATE ${SOURCES})