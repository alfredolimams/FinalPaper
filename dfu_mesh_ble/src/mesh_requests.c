#include "mesh_requests.h"

LOG_MODULE_REGISTER(mesh_requests);

void send_dfu_get_data(u32_t offset, struct image_version version, u16_t address)
{
    LOG_DBG("DFU: get data\n");
    struct net_buf_simple *msg = dfu_cli_model->pub->msg;
    bt_mesh_model_msg_init(msg, BT_MESH_MODEL_DFU_GET_DATA);

    // [8] Version + [4] Offset + [2] Address
    net_buf_simple_add_u8(msg, version.iv_major);
    net_buf_simple_add_u8(msg, version.iv_minor);
    net_buf_simple_add_le16(msg, version.iv_revision);
    net_buf_simple_add_le32(msg, version.iv_build_num);
    net_buf_simple_add_le32(msg, offset);
    net_buf_simple_add_le32(msg, address);

    int err = bt_mesh_model_publish(dfu_cli_model);
    if (err) {
        printk("[ERR] BT MESH MODEL PUB ERR %d MESSAGE TO 0x%04x\n", err, dfu_cli_model->pub->addr);
    }
}

void send_dfu_get_property(u8_t property, u16_t address)
{
    LOG_DBG("DFU: get property\n");
    struct net_buf_simple *msg = dfu_cli_model->pub->msg;
    bt_mesh_model_msg_init(msg, BT_MESH_MODEL_DFU_GET_PROPERTY);

    // [1] Property + [2] Address
    net_buf_simple_add_u8(msg, property);
    net_buf_simple_add_le16(msg, address);

    int err = bt_mesh_model_publish(dfu_cli_model);
    if (err) {
        printk("[ERR] BT MESH MODEL PUB ERR %d MESSAGE TO 0x%04x\n", err, dfu_cli_model->pub->addr);
    }
}

void send_dfu_status_property(u8_t property, u8_t *data)
{
    printk("[INF] DFU: status property\n");
    struct net_buf_simple *msg = dfu_srv_model->pub->msg;
    bt_mesh_model_msg_init(msg, BT_MESH_MODEL_DFU_STATUS_PROPERTY);
    size_t bytes = 0;
    struct image_version version;
    u32_t image_size = 0;

    // [1] Property
    net_buf_simple_add_u8(msg, property);

    switch (property) {
    case VERSION_PROPERTY:  // [8] Version
        bytes = 8;
        printk("[INF] Send version\n");
        memcpy(&version, data, sizeof version);
        net_buf_simple_add_u8(msg, version.iv_major);
        net_buf_simple_add_u8(msg, version.iv_minor);
        net_buf_simple_add_le16(msg, version.iv_revision);
        net_buf_simple_add_le32(msg, version.iv_build_num);
        break;
    case IMAGE_SIZE_PROPERTY:  // [4] Data size
        bytes = 4;
        printk("[INF] Send image size\n");
        memcpy(&image_size, data, sizeof image_size);
        printk("[INF] Image size is %d\n", image_size);
        net_buf_simple_add_le32(msg, image_size);
        break;
    case STATUS_PROPERTY:  // [1] Status
        bytes = 1;
        printk("[INF] Send status\n");
        net_buf_simple_add_u8(msg, data[0]);
        break;
    default:
        break;
    }

    for (size_t i = bytes; i < 12; i++) {
        net_buf_simple_add_u8(msg, 0);
    }

    int err = bt_mesh_model_publish(dfu_srv_model);
    if (err) {
        printk("[ERR] BT MESH MODEL PUB ERR %d MESSAGE TO 0x%04x\n", err, dfu_srv_model->pub->addr);
    }
}

int send_dfu_status_data(u32_t offset, struct image_version version, u16_t data_size, u8_t *data)
{
    printk("[DBG] DFU STATUS DATA: With %d offset and %d data size\n", offset, data_size);
    struct net_buf_simple *msg = dfu_srv_model->pub->msg;
    bt_mesh_model_msg_init(msg, BT_MESH_MODEL_DFU_STATUS_DATA);

    // // [8] Version + [4] Offset + [2] Data size +
    net_buf_simple_add_u8(msg, version.iv_major);
    net_buf_simple_add_u8(msg, version.iv_minor);
    net_buf_simple_add_le16(msg, version.iv_revision);
    net_buf_simple_add_le32(msg, version.iv_build_num);
    net_buf_simple_add_le32(msg, offset);
    net_buf_simple_add_le16(msg, data_size);

    // // + [256] Data
    for (size_t i = 0; i < data_size; i++) {
        net_buf_simple_add_u8(msg, data[i]);
    }
    for (size_t i = data_size; i <= SZ_DATA; i++) {
        net_buf_simple_add_u8(msg, data[i]);
    }

    u8_t attempt = 0;

    while (bt_mesh_model_publish(dfu_srv_model))
    {
        k_sleep(K_MSEC(100));
        attempt++;
        if( attempt >= 5 ){
            printk("[ERR] Publish data status\n");
            return -1;
        }
    }
    return 0;
}