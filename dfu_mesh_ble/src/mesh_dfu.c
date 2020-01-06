#include "mesh_dfu.h"

LOG_MODULE_REGISTER(mesh_dfu);

static int read_image_info(int area_id, uint32_t *size, struct image_version *version)
{
    const struct flash_area *fap;
    struct image_tlv_info info;
    struct image_header hdr;
    int rc = 0;

    rc = flash_area_open(area_id, &fap);
    if (rc != 0) {
        rc = -1;
        goto done;
    }

    rc = flash_area_read(fap, 0, &hdr, sizeof(hdr));
    if (rc != 0) {
        rc = -1;
        goto done;
    }
    memcpy(version, &hdr.ih_ver, sizeof hdr.ih_ver);

    rc = flash_area_read(fap, hdr.ih_hdr_size + hdr.ih_img_size, &info, sizeof(info));
    if (rc != 0) {
        rc = -1;
        goto done;
    }

    if (info.it_magic != IMAGE_TLV_INFO_MAGIC) {
        rc = -1;
        goto done;
    }
    *size = hdr.ih_hdr_size + hdr.ih_img_size + info.it_tlv_tot;

done:
    flash_area_close(fap);
    return rc;
}

static struct dfu_data_t dfu_data = {
    .state                  = IDLE_STATE,
    .status                 = OK_STATUS,
    .timeout_events         = 0,
    .slots[0].flash_area_id = DT_FLASH_AREA_IMAGE_0_ID,
    .slots[1].flash_area_id = DT_FLASH_AREA_IMAGE_1_ID,
};

void timeout_function(struct k_timer *timer_id)
{
    dfu_data.timeout_events++;
    msgq_dfu_event q_event;

    if (dfu_data.timeout_events == TIME_OF_TIMES) {
        q_event.event  = ABORT_EVENT;
        dfu_data.state = ERROR_STATE;
        printk("[ERR] ERROR STATE\n");
    } else {
        q_event.event = TIMEOUT_EVENT;
    }
    printk("[WRN] CREATE TIMEOUT EVENT\n");
    k_msgq_put(&msgq_dfu, &q_event, K_NO_WAIT);
}

K_TIMER_DEFINE(timeout_timer, timeout_function, NULL);

typedef void (*state_func)(msgq_event);

typedef struct {
    state_func exec;
} state_function;

int match_version(struct image_version version)
{
    u8_t major     = version.iv_major == dfu_data.slots[0].version.iv_major;
    u8_t minor     = version.iv_minor == dfu_data.slots[0].version.iv_minor;
    u8_t revision  = version.iv_revision == dfu_data.slots[0].version.iv_revision;
    u8_t build_num = version.iv_build_num == dfu_data.slots[0].version.iv_build_num;
    return major & minor & revision & build_num;
}

struct image_version get_update_image_version()
{
    return dfu_data.version_update;
}

int up_version(struct image_version version)
{
    int major     = version.iv_major - dfu_data.slots[0].version.iv_major;
    int minor     = version.iv_minor - dfu_data.slots[0].version.iv_minor;
    int revision  = version.iv_revision - dfu_data.slots[0].version.iv_revision;
    int build_num = version.iv_build_num - dfu_data.slots[0].version.iv_build_num;

    if (major > 0) {
        return 1;
    } else if (!major && minor > 0) {
        return 1;
    } else if (!major && !minor && revision > 0) {
        return 1;
    } else if (!major && !minor && !revision && build_num > 0) {
        return 1;
    }
    return 0;
}

K_MSGQ_DEFINE(msgq_dfu, sizeof(msgq_dfu_event), 10, 4);

K_MUTEX_DEFINE(mutex_dfu);

K_MUTEX_DEFINE(dfu_upload_mutex);

#define min(x, y) (x < y ? x : y)

void to_buffer(u8_t *data, u16_t data_size, u32_t offset)
{
    if (dfu_data.state != DOWNLOAD_STATE) {
        printk("[ERR] It isn't download state\n");
        k_mutex_unlock(&mutex_dfu);
        return;
    }

    if (offset != dfu_data.offset_update) {
        printk("[WRN] Diff offset in process\n");
        k_mutex_unlock(&mutex_dfu);
        return;
    }

    printk("[INF] Write on flash %d bytes\n", data_size);
    if (flash_img_buffered_write(&dfu_data.ctx, data, data_size, false)) {
        printk("[ERR] DFU download: Write\n");
        dfu_data.state = ERROR_STATE;
    }
    k_timer_stop(&timeout_timer);

    dfu_data.offset_update += data_size;
    if (dfu_data.offset_update == dfu_data.image_size_update) {
        flash_img_buffered_write(&dfu_data.ctx, data, 0, true);
        dfu_data.state = BOOT_AND_VERIFY_STATE;
        k_mutex_unlock(&mutex_dfu);
        return;
    }

    printk("[INF] Request image segment from %d\n", dfu_data.offset_update);

    k_timer_start(&timeout_timer, TIME_TO_TIMEOUT, 0);
    dfu_data.timeout_events = 0;
    k_mutex_unlock(&mutex_dfu);
}

void reset_timeout()
{
    k_timer_stop(&timeout_timer);
    k_timer_start(&timeout_timer, TIME_TO_TIMEOUT, 0);
}

void dfu_request(u8_t property)
{
    u8_t data[12];

    switch (property) {
    case VERSION_PROPERTY:
        printk("[INF] REQUEST - VERSION\n");
        memcpy(data, &dfu_data.slots[0].version, sizeof dfu_data.slots[0].version);
        break;
    case IMAGE_SIZE_PROPERTY:
        printk("[INF] REQUEST - DATA_SIZE\n");
        memcpy(data, &dfu_data.slots[0].image_size, sizeof dfu_data.slots[0].image_size);
        break;
    case STATUS_PROPERTY:
        printk("[INF] REQUEST - STATUS\n");
        memcpy(data, &dfu_data.status, sizeof dfu_data.status);
        break;
    default:
        printk("[WRN] INVALID PROPERTY\n");
        return;
        break;
    }
    send_dfu_status_property(property, data);
}

void dfu_upload(u32_t offset)
{
    static u32_t offset_in = -1;
    static u8_t step = 0;

    printk("[INF] UPDATED OFFSET OF OFFSET\n");
    offset_in = offset;
    step = 0;


    if (k_mutex_lock(&dfu_upload_mutex, K_NO_WAIT)) {
        return;
    }

    u8_t buffer[SZ_DATA];
    u16_t data_size = 0;
    
    while (offset_in < dfu_data.slots[0].image_size) {
        data_size = min(dfu_data.slots[0].image_size - offset_in, SZ_DATA);
        if (flash_area_read(dfu_data.slots[0].fap, offset_in, buffer, data_size)) {
            printk("[ERR] DFU idle error in read\n");
            return;
        }

        if (send_dfu_status_data(offset_in, dfu_data.slots[0].version, data_size, buffer)) {
            break;
        }
        offset_in += data_size;
        k_sleep(K_SECONDS(1) + step*K_MSEC(100));
        step++;
        if( step > 10 )
        {
            step = 10;
        }
    }
    k_mutex_unlock(&dfu_upload_mutex);
}

void idle_process(msgq_dfu_event event)
{
    printk("[INF] DFU idle\n");

    if (event.event != RESPONSE_EVENT) {
        printk("[WRN] DFU idle: event isn't response type\n");
        return;
    }

    if (event.buff[0] != VERSION_PROPERTY) {
        printk("[WRN] DFU idle: property isn't version type\n");
        return;
    }

    struct image_version version;
    memcpy(&version, event.buff + 3, sizeof version);

    if (up_version(version)) {
        // Change state
        dfu_data.state = INIT_STATE;
        memcpy(&dfu_data.address_update, event.buff + 1, sizeof dfu_data.address_update);
        memcpy(&dfu_data.version_update, &version, sizeof dfu_data.version_update);

        // Request image size
        send_dfu_get_property(IMAGE_SIZE_PROPERTY, dfu_data.address_update);
        k_timer_start(&timeout_timer, TIME_TO_TIMEOUT, 0);
        dfu_data.timeout_events = 0;
    }
}

void init_process(msgq_dfu_event event)
{
    if (event.event != RESPONSE_EVENT) {
        printk("[WRN] DFU idle: event isn't response type\n");
        return;
    }

    if (event.buff[0] != IMAGE_SIZE_PROPERTY) {
        printk("[WRN] DFU idle: property isn't image size type\n");
        return;
    }

    u16_t address;
    memcpy(&address, event.buff + 1, sizeof address);

    if (address != dfu_data.address_update) {
        printk("[WRN] DFU idle: property isn't image size type\n");
        return;
    }

    memcpy(&dfu_data.image_size_update, event.buff + 3, sizeof dfu_data.image_size_update);
    dfu_data.state = DOWNLOAD_STATE;

    if (flash_img_init(&dfu_data.ctx)) {
        printk("[ERR] Init flash image\n");
        dfu_data.state = ERROR_STATE;
        return;
    }
    dfu_data.offset_update = 0;
    if (flash_area_erase(dfu_data.ctx.flash_area, 0, dfu_data.ctx.flash_area->fa_size)) {
        printk("[ERR] Erase flash\n");
        dfu_data.state = ERROR_STATE;
    }

    if (flash_img_bytes_written(&dfu_data.ctx)) {
        printk("[ERR] Buff in memory\n");
        dfu_data.state = ERROR_STATE;
        return;
    }

    k_timer_start(&timeout_timer, TIME_TO_TIMEOUT, 0);
    dfu_data.timeout_events = 0;

    printk("[INF] REQUEST FIRST IMAGE SEGMENTATION\n");
    send_dfu_get_data(dfu_data.offset_update, dfu_data.version_update, dfu_data.address_update);
}

void download_process(msgq_dfu_event event)
{
    printk("[INF] DFU DOWNLOAD\n");
    if (event.event != DOWNLOAD_EVENT) {
        printk("[ERR] Invalid event to this state\n");
        return;
    }
}

void verify_and_boot_process(msgq_dfu_event event)
{
    if (boot_request_upgrade(0)) {
        msgq_dfu_event q_event;
        q_event.event  = ABORT_EVENT;
        dfu_data.state = ERROR_STATE;
        k_msgq_put(&msgq_dfu, &q_event, K_FOREVER);
        return;
    }
    sys_reboot(SYS_REBOOT_COLD);
}

void error_process(msgq_dfu_event event)
{
    dfu_data.state = IDLE_STATE;
}

static int mesh_dfu_init(struct device *dev)
{
    printk("[INF] Mesh DFU init\n");

    printk("[INF] ###    INFORMATION     ###\n");
    for (size_t slot = 0; slot < 2; ++slot) {
        printk("[INF] *************************\n");
        printk("[INF] ***    Slot %d     ***\n", slot);
        // Setup
        read_image_info(dfu_data.slots[slot].flash_area_id, &dfu_data.slots[slot].image_size,
                        &dfu_data.slots[slot].version);
        if (flash_area_open(dfu_data.slots[slot].flash_area_id, &dfu_data.slots[slot].fap)) {
            printk("[ERR] Error opening\n");
            continue;
        }
        // Log
        printk("[INF] Image size: %d\n", dfu_data.slots[slot].image_size);
        printk("[INF] Version: %d.%d.%d+%d\n", dfu_data.slots[slot].version.iv_major,
               dfu_data.slots[slot].version.iv_minor, dfu_data.slots[slot].version.iv_revision,
               dfu_data.slots[slot].version.iv_build_num);
    }
    printk("[INF] ###########################\n");

    if (cmp_version(dfu_data.slots[0].version, dfu_data.slots[1].version) > 0) {
        //
        printk("[INF] Imagem confirmada\n");
        boot_is_img_confirmed();
    }

    static state_function funcs[] = {{idle_process},
                                     {init_process},
                                     {download_process},
                                     {verify_and_boot_process},
                                     {error_process}};

    static msgq_dfu_event event;
    while (1) {
        k_msgq_get(&msgq_dfu, &event, K_FOREVER);
        if (event.event == TIMEOUT_EVENT) {
            printk("[WRN] TIMEOUT EVENT\n");
            switch (dfu_data.state) {
            case INIT_STATE:
                // Requesitar o tamanho
                send_dfu_get_property(IMAGE_SIZE_PROPERTY, dfu_data.address_update);
                break;
            case DOWNLOAD_STATE:
                // Requesitar o segmento da imagem
                send_dfu_get_data(dfu_data.offset_update, dfu_data.version_update,
                                  dfu_data.address_update);
                break;
            default:
                break;
            }
            k_timer_start(&timeout_timer, (dfu_data.timeout_events + 1) * TIME_TO_TIMEOUT, 0);
        } else {
            funcs[dfu_data.state].exec(event);
        }
        k_sleep(K_MSEC(1));
    }

    return 0;
}

u8_t get_state()
{
    return dfu_data.state;
}

u8_t get_timeout_events()
{
    return dfu_data.timeout_events;
}

u32_t bytes_written()
{
    return dfu_data.offset_update;
}

u16_t address_update()
{
    return dfu_data.address_update;
}

u32_t image_size_update()
{
    return dfu_data.image_size_update;
}

K_THREAD_DEFINE(mesh_dfu, STACKSIZE, mesh_dfu_init, NULL, NULL, NULL, PRIORITY, 0, K_NO_WAIT);