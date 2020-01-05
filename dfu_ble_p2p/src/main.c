/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <misc/byteorder.h>
#include <misc/printk.h>
#include <settings/settings.h>
#include <stddef.h>
#include <string.h>
#include <zephyr.h>
#include <zephyr/types.h>
#include <logging/log.h>


#include "bt.h"
#include "dfu_ble.h"

LOG_MODULE_REGISTER(MAIN);

void main(void)
{
    int err;
    
    if( err = bt_init(), err ){
        LOG_ERR("Bluetooth init error (%d)", err);
        goto run;
    }

    if( err = dfu_init(1), err ){
        LOG_ERR("Enable DFU error (%d)", err);
        goto run;
    }

    if( err = dfu_mode(1), err ){
        LOG_ERR("Enable DFU error (%d)", err);
        goto run;
    }

    if( err = dfu_confirm_image(), err ){
        LOG_ERR("Confirm image error (%d)", err);
        goto run;
    }

    run:
    while (1) {
        k_sleep(MSEC_PER_SEC);
    }
}
