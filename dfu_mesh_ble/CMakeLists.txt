# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(SMP_MESH)

list(APPEND HEADERS
    "${PROJECT_SOURCE_DIR}/include/"
)

list(APPEND SOURCES
    "${PROJECT_SOURCE_DIR}/src/main.c"
    "${PROJECT_SOURCE_DIR}/src/mesh.c"
    "${PROJECT_SOURCE_DIR}/src/mesh_requests.c"
    "${PROJECT_SOURCE_DIR}/src/mesh_callbacks.c"
    "${PROJECT_SOURCE_DIR}/src/cmds.c"
    "${PROJECT_SOURCE_DIR}/src/mesh_dfu.c"
    "${PROJECT_SOURCE_DIR}/src/image.c"
)

zephyr_link_libraries_ifdef(CONFIG_FILE_SYSTEM_NFFS NFFS)
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/subsys/fs/nvs)

include_directories(${HEADERS})
target_sources(app PRIVATE ${SOURCES})