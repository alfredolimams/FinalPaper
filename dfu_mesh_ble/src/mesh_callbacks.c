#include "mesh_callbacks.h"

LOG_MODULE_REGISTER(mesh_callbacks);

#define BROADCAST 0
#define BUTTON_DEBOUNCE_DELAY_MS 10000

static void get_version(struct image_version *version, struct net_buf_simple *buf)
{
    version->iv_major     = net_buf_simple_pull_u8(buf);
    version->iv_minor     = net_buf_simple_pull_u8(buf);
    version->iv_revision  = net_buf_simple_pull_le16(buf);
    version->iv_build_num = net_buf_simple_pull_le32(buf);
}

void dfu_get_property(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                      struct net_buf_simple *buf)
{
    if (bt_mesh_model_elem(model)->addr == ctx->addr) {
        return;
    }

    u8_t property = net_buf_simple_pull_u8(buf);
    u16_t address = net_buf_simple_pull_le16(buf);

    if (address != BROADCAST && bt_mesh_model_elem(model)->addr != address) {
        printk("[WRN] Invalid address\n");
        return;
    }

    dfu_request(property);
}

void dfu_get_data(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                  struct net_buf_simple *buf)
{
    if (bt_mesh_model_elem(model)->addr == ctx->addr) {
        return;
    }

    static u32_t last_time   = 0;
    static u32_t last_offset = -1;

    struct image_version version;
    u32_t offset;
    u16_t address;

    get_version(&version, buf);
    offset  = net_buf_simple_pull_le32(buf);
    address = net_buf_simple_pull_le16(buf);

    if (address != BROADCAST && bt_mesh_model_elem(model)->addr != address) {
        printk("[WRN] Wrong address\n");
        return;
    }
    printk("[INF] GET DATA - %d\n", offset);

    u32_t now = k_uptime_get_32();

    if ((offset == last_offset) && (last_time + K_SECONDS(10) >= now)) {
        printk("[WRN] Request same offset in little time\n");
        return;
    }

    if (!match_version(version)) {
        printk("[WRN] Wrong version\n");
        return;
    }

    last_offset = offset;

    printk("[INF] Request offset %d\n", offset);

    last_time = now;

    dfu_upload(offset);
}

void dfu_status_property(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                         struct net_buf_simple *buf)
{
    if (bt_mesh_model_elem(model)->addr == ctx->addr) {
        return;
    }

    enum dfu_property property;
    int offset = 0;

    property = net_buf_simple_pull_u8(buf);

    // [1] Property +
    msgq_dfu_event q_event = {
        .event   = RESPONSE_EVENT,
        .buff[0] = property,
    };
    offset += 1;

    // + [2] Address
    memcpy(q_event.buff + offset, &ctx->addr, sizeof ctx->addr);
    offset += sizeof ctx->addr;

    switch (property) {
    case VERSION_PROPERTY:
        // + [8] Version
        printk("[INF] Version property\n");
        struct image_version version;
        get_version(&version, buf);
        memcpy(q_event.buff + offset, &version, sizeof version);
        break;
    case IMAGE_SIZE_PROPERTY:
        // + [4] Image size
        printk("[INF] Image size property\n");
        u32_t image_size = net_buf_simple_pull_le32(buf);
        memcpy(q_event.buff + offset, &image_size, sizeof image_size);
        break;
    case STATUS_PROPERTY:
        // + [1] Status
        printk("[INF] Status property\n");
        u8_t status = net_buf_simple_pull_u8(buf);
        memcpy(q_event.buff + offset, &status, sizeof status);
        break;
    default:
        printk("[ERR] Invalid property\n");
        return;
        break;
    }
    k_msgq_put(&msgq_dfu, &q_event, K_NO_WAIT);
}

void dfu_status_data(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                     struct net_buf_simple *buf)
{
    printk("[DBG] STATUS DATA\n");
    if (bt_mesh_model_elem(model)->addr == ctx->addr) {
        return;
    }

    struct image_version version, update;
    static u8_t qt_offset_diff = 0;
    u32_t offset;
    u16_t data_size;
    u8_t data[SZ_DATA];

    get_version(&version, buf);
    update = get_update_image_version();

    u8_t match_major     = update.iv_major == version.iv_major;
    u8_t match_minor     = update.iv_minor == version.iv_minor;
    u8_t match_revision  = update.iv_revision == version.iv_revision;
    u8_t match_build_num = update.iv_build_num == version.iv_build_num;

    if (!(match_major & match_minor & match_revision & match_build_num)) {
        printk("[WRN] Wrong version\n");
        return;
    }

    offset    = net_buf_simple_pull_le32(buf);
    data_size = net_buf_simple_pull_le16(buf);

    if (bytes_written() != offset) {
        printk("[WRN] Offset diff: (%d)\n", offset);
        qt_offset_diff++;
        if( qt_offset_diff > 4 ){
            printk("[WRN] Request %d *\n", bytes_written());
            send_dfu_get_data(bytes_written(), update, BROADCAST);
        }
        return;
    }
    qt_offset_diff = 0;

    for (size_t i = 0; i < data_size; i++) {
        data[i] = net_buf_simple_pull_u8(buf);
    }

    if (k_mutex_lock(&mutex_dfu, K_NO_WAIT) == 0) {
        printk("[INF] Create download event\n");
        to_buffer(data, data_size, offset);
    }
}