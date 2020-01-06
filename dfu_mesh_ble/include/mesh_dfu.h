/***************************************************************************
 *
 * Copyright(c) 2015,2016 Intel Corporation.
 * Copyright(c) 2017 PHYTEC Messtechnik GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of Intel Corporation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***************************************************************************/

/**
 * @file
 * @brief USB Device Firmware Upgrade (DFU) public header
 *
 * Header follows the Device Class Specification for
 * Device Firmware Upgrade Version 1.1
 */

#ifndef ZEPHYR_INCLUDE_CLASS_MESH_DFU_H_
#define ZEPHYR_INCLUDE_CLASS_MESH_DFU_H_

#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <errno.h>
#include <flash.h>
#include <flash_map.h>
#include <init.h>
#include <kernel.h>
#include <logging/log.h>
#include <misc/reboot.h>
#include <stdio.h>

#include "image.h"
#include "mesh_requests.h"

/* size of stack area used by each thread */
#define STACKSIZE 2048

/* scheduling priority used by each thread */
#define PRIORITY 7

enum dfu_status {
    OK_STATUS,
    ERRO_READ_STATUS,
    ERRO_WRITE_STATUS,
    ERRO_ERASE_STATUS,
    ERRO_CHECK_ERASED_STATUS,
    ERRO_VERIFY_STATUS,
};

enum dfu_state {
    IDLE_STATE,
    INIT_STATE,
    DOWNLOAD_STATE,
    BOOT_AND_VERIFY_STATE,
    ERROR_STATE,
};

enum dfu_event { DOWNLOAD_EVENT, RESPONSE_EVENT, TIMEOUT_EVENT, ABORT_EVENT };

/* Device data structure */

struct info_image {
    struct image_version version;
    u32_t image_size;
    u8_t flash_area_id;
    const struct flash_area *fap;
    // add bootable ?
};

struct dfu_data_t {
    struct info_image slots[NUM_OF_SLOTS];

    // Infos to update
    struct flash_img_context ctx;
    struct image_version version_update;
    u32_t image_size_update;
    u32_t offset_update;
    u16_t address_update;

    // Buffer
    u8_t buffer[SZ_DATA]; /* DFU data buffer */
    u16_t buffer_size;
    u32_t buffer_offset;

    // State machine
    enum dfu_state state;   /* State of the DFU device */
    enum dfu_status status; /* Status of the DFU device */
    u8_t timeout_events;
};

typedef struct {
    enum dfu_event event;
    u8_t buff[MAX_SIZE_DATA];
} msgq_dfu_event;

int match_version(struct image_version version);

u8_t get_state();

u8_t get_timeout_events();

void dfu_request(u8_t property);

void dfu_upload(u32_t offset);

void to_buffer(u8_t *data, u16_t data_size, u32_t offset);

u32_t bytes_written();

u16_t address_update();

u32_t image_size_update();

struct image_version get_update_image_version();

void reset_timeout();

extern struct k_msgq msgq_dfu;
extern struct k_mutex mutex_dfu;
// extern struct k_mutex dfu_upload_mutex;

#endif /* ZEPHYR_INCLUDE_USB_CLASS_USB_DFU_H_ */
