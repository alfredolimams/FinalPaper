#include "mesh.h"

LOG_MODULE_REGISTER(BT_MESH);

static u8_t dev_uuid[16] = {0xdd, 0xda};

const struct bt_mesh_prov prov = {
    .uuid           = dev_uuid,
    .output_size    = 1,
    .output_actions = BT_MESH_DISPLAY_NUMBER,
    .output_number  = output_number,
    .complete       = prov_complete,
    .reset          = prov_reset,
};

/*
 * Server Configuration Declaration
 */

static struct bt_mesh_cfg_srv cfg_srv = {
    .relay  = BT_MESH_RELAY_DISABLED,
    .beacon = BT_MESH_BEACON_ENABLED,
#if defined(CONFIG_BT_MESH_FRIEND)
    .frnd = BT_MESH_FRIEND_ENABLED,
#else
    .frnd       = BT_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BT_MESH_GATT_PROXY)
    .gatt_proxy = BT_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = BT_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 0,

    /* 3 transmissions with 20ms interval */
    .net_transmit     = BT_MESH_TRANSMIT(0, 20),
    .relay_retransmit = BT_MESH_TRANSMIT(0, 20),
};

static struct bt_mesh_health_srv health_srv = {};
static struct bt_mesh_cfg_cli cfg_cli       = {};

const struct bt_mesh_model_op dfu_cli_ops[] = {
    {BT_MESH_MODEL_DFU_STATUS_PROPERTY, SZ_STATUS_PROPERTY, dfu_status_property},
    {BT_MESH_MODEL_DFU_STATUS_DATA, SZ_STATUS_DATA, dfu_status_data},
    BT_MESH_MODEL_OP_END,
};

const struct bt_mesh_model_op dfu_srv_ops[] = {
    {BT_MESH_MODEL_DFU_GET_PROPERTY, SZ_GET_PROPERTY, dfu_get_property},
    {BT_MESH_MODEL_DFU_GET_DATA, SZ_GET_DATA, dfu_get_data},
    BT_MESH_MODEL_OP_END,
};

BT_MESH_MODEL_PUB_DEFINE(dfu_cli_pub, NULL, 3 + SZ_GET_DATA);
BT_MESH_MODEL_PUB_DEFINE(dfu_srv_pub, NULL, 3 + SZ_STATUS_DATA);
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

struct bt_mesh_model dfu_models[] = {
    BT_MESH_MODEL_VND(BT_COMP_ID_LF, BT_MESH_MODEL_ID_DFU_CLI, dfu_cli_ops, &dfu_cli_pub, NULL),
    BT_MESH_MODEL_VND(BT_COMP_ID_LF, BT_MESH_MODEL_ID_DFU_SRV, dfu_srv_ops, &dfu_srv_pub, NULL),
};

struct bt_mesh_model root_models[] = {
    BT_MESH_MODEL_CFG_SRV(&cfg_srv),
    BT_MESH_MODEL_CFG_CLI(&cfg_cli),
    BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
};

struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(0, root_models, dfu_models),
};

const struct bt_mesh_comp comp = {
    .cid        = BT_COMP_ID_LF,
    .elem       = elements,
    .elem_count = ARRAY_SIZE(elements),
};

int output_number(bt_mesh_output_action_t action, u32_t number)
{
    LOG_WRN("OOB Number: %u", number);
    printk("OOB Number: %u\n", number);
    return 0;
}

void prov_complete(u16_t net_idx, u16_t addr)
{
    LOG_INF("PROVISIONING WAS COMPLETED");
}

void prov_reset(void)
{
    LOG_INF("PROVISION RESET");
    bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

void bt_ready(int err)
{
    LOG_INF("BT MESH INIT");
    struct bt_le_oob oob;

    if (err) {
        LOG_ERR("BT MESH INIT FAILD ERR: %d", err);
        return;
    }

    err = bt_mesh_init(&prov, &comp);
    if (err) {
        LOG_ERR("BT MESH INIT FAILED WITH ERR: %d", err);
        return;
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    /* Use identity address as device UUID */
    if (bt_le_oob_get_local(BT_ID_DEFAULT, &oob)) {
        LOG_ERR("Identity Address unavailable");
    } else {
        memcpy(dev_uuid, oob.addr.a.val, 6);
    }

    printk("UUID: ");
    for (size_t i = 0; i < 16; i++) {
        printk("%02x", dev_uuid[i]);
    }
    printk("\n");


    bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
    LOG_INF("FINISH BT MESH INIT");
}

void counter_handler(struct k_timer* timer_id)
{
    LOG_ERR("COUNTER HANDLER");
}

struct bt_mesh_model* dfu_srv_model = &dfu_models[1];
struct bt_mesh_model* dfu_cli_model = &dfu_models[0];