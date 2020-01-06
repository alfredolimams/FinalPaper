#ifndef _BT_REQUESTS_SMP_H
#define _BT_REQUESTS_SMP_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <flash_map.h>
#include <logging/log.h>
#include <misc/printk.h>
#include <settings/settings.h>

#include "image.h"

#define NUM_OF_SLOTS 2
#define MAX_SIZE_DATA 300

/* Models from the Mesh Model to OTA in mesh */
#define BT_MESH_MODEL_ID_DFU_SRV 0x1234
#define BT_MESH_MODEL_ID_DFU_CLI 0x4321

#define CID_ZEPHYR 0x0002

#define SZ_VERSION 8
#define SZ_OFFSET 4
#define SZ_ADDRESS 2
#define SZ_DATA_SIZE 2
#define SZ_DATA 32
#define SZ_PROPERTY_SIZE 1
#define SZ_PROPERTY 12
#define TIME_TO_TIMEOUT K_SECONDS(5)
#define TIME_OF_TIMES 5

#define SZ_STATUS_PROPERTY SZ_PROPERTY_SIZE + SZ_PROPERTY
#define SZ_STATUS_DATA SZ_VERSION + SZ_OFFSET + SZ_DATA_SIZE + SZ_DATA
#define SZ_GET_PROPERTY SZ_PROPERTY_SIZE + SZ_DATA_SIZE
#define SZ_GET_DATA SZ_VERSION + SZ_OFFSET + SZ_ADDRESS


#define BT_MESH_MODEL_DFU_STATUS_PROPERTY BT_MESH_MODEL_OP_3(0x01, CID_ZEPHYR)
#define BT_MESH_MODEL_DFU_STATUS_DATA BT_MESH_MODEL_OP_3(0x02, CID_ZEPHYR)

#define BT_MESH_MODEL_DFU_GET_PROPERTY BT_MESH_MODEL_OP_3(0x03, CID_ZEPHYR)
#define BT_MESH_MODEL_DFU_GET_DATA BT_MESH_MODEL_OP_3(0x04, CID_ZEPHYR)

void send_dfu_get_data(u32_t offset, struct image_version version, u16_t address);
void send_dfu_get_property(u8_t property, u16_t address);

int send_dfu_status_data(u32_t offset, struct image_version version, u16_t data_size, u8_t *data);
void send_dfu_status_property(u8_t property, u8_t *data);
enum dfu_property { VERSION_PROPERTY, IMAGE_SIZE_PROPERTY, STATUS_PROPERTY };

extern struct bt_mesh_model *dfu_srv_model;
extern struct bt_mesh_model *dfu_cli_model;

#endif  // _BT_REQUESTS_SMP__H