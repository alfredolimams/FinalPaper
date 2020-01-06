#include "dfu_ble.h"

#include <bluetooth/uuid.h>
#include <errno.h>
#include <logging/log.h>
#include <misc/byteorder.h>
#include <misc/reboot.h>
#include <misc/util.h>
#include <shell/shell.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr.h>
#include <zephyr/types.h>
#include <bluetooth/gatt.h>

LOG_MODULE_REGISTER(dfu_ble, 3);

/* EFU - OTA service.
 * {FFF5E361-853B-482D-B102-46412D9A7413}
 */
static struct bt_uuid_128 efu_bt_svc_uuid = BT_UUID_INIT_128(
    0x13, 0x74, 0x9a, 0x2d, 0x41, 0x46, 0x02, 0xb1, 0x2d, 0x48, 0x3b, 0x85, 0x61, 0xe3, 0xf5, 0xff);

/* EFU - OTA hardware type characteristic;
 * {FFF5E362-853B-482D-B102-46412D9A7413}
 */
static struct bt_uuid_128 efu_bt_chr_hw_tp_uuid = BT_UUID_INIT_128(
    0x13, 0x74, 0x9a, 0x2d, 0x41, 0x46, 0x02, 0xb1, 0x2d, 0x48, 0x3b, 0x85, 0x62, 0xe3, 0xf5, 0xff);

/* EFU - OTA version characteristic;
 * {FFF5E363-853B-482D-B102-46412D9A7413}
 */
static struct bt_uuid_128 efu_bt_chr_vrs_uuid = BT_UUID_INIT_128(
    0x13, 0x74, 0x9a, 0x2d, 0x41, 0x46, 0x02, 0xb1, 0x2d, 0x48, 0x3b, 0x85, 0x63, 0xe3, 0xf5, 0xff);

/* EFU - OTA data exchange characteristic; used for both requests and responses.
 * {FFF5E364-853B-482D-B102-46412D9A7413}
 */
static struct bt_uuid_128 efu_bt_chr_exg_uuid = BT_UUID_INIT_128(
    0x13, 0x74, 0x9a, 0x2d, 0x41, 0x46, 0x02, 0xb1, 0x2d, 0x48, 0x3b, 0x85, 0x64, 0xe3, 0xf5, 0xff);

/*
 * DFU DATA
 */

static struct dfu_data_t dfu_data = {
    .slots[0].flash_area_id = 0,
    .slots[1].flash_area_id = 0,
};

/*
 * Messages queues
 */

K_MSGQ_DEFINE(msgq_send, sizeof(msgq_dfu_event), 10, 4);

K_MSGQ_DEFINE(msgq_receive, sizeof(msgq_dfu_event), 10, 4);

/*
 * Threads stack
 */

#define MY_STACK_SIZE 4096
#define MY_PRIORITY 5

int send_channel(struct device *dev);
int receive_channel(struct device *dev);

K_THREAD_DEFINE(receive_channel_thread, MY_STACK_SIZE, receive_channel, NULL, NULL, NULL, MY_PRIORITY, 0, K_FOREVER);
K_THREAD_DEFINE(send_channel_thread, MY_STACK_SIZE, send_channel, NULL, NULL, NULL, MY_PRIORITY, 0, K_FOREVER);

static int find_connection(struct bt_conn *conn){
    for (int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
        if( !memcmp(conn, dfu_data.connections[i].conn, sizeof conn) ) {
            return i;
        }
    }
    return -ENXIO;
}

static int add_connection(struct bt_conn *conn)
{
    if( !conn ){
        return -EFAULT;
    } else if( dfu_data.number_of_connections >= MAX_CONNECTION ){
        return -EBUSY;
    }

    int id = find_connection(conn);
    if( id == -ENXIO ){
        id = find_connection(NULL);
        LOG_WRN("New connection is with ID: %d", id);
        if( id >= 0 ){
            dfu_data.connections[id].conn = conn;
            dfu_data.connections[id].mtu = 23;
            dfu_data.number_of_connections++;
        } else {
            return -1;
        }
    }
    return 0;
}

static int purge_connection(struct bt_conn *conn)
{
    if( !conn ){
        return -EFAULT;
    }

    int id = find_connection(conn);
    if( id == -ENXIO ){
        return id;
    }

    memset( &dfu_data.connections[id], 0, sizeof dfu_data.connections[id] );
    return 0;
}

/*
 * Read func
 */

static u8_t read_func(struct bt_conn *conn, u8_t err,
			 struct bt_gatt_read_params *params,
			 const void *data, u16_t length)
{
    if( err ){
        LOG_WRN("Read complete: err 0x%02x length %u", err, length);
        goto err_read_func;
    }

	if (!data) {
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

    int id = find_connection(conn);
    u8_t * p_data = (u8_t *)data;
    if( id < 0 ){
        goto err_read_func;
    }    

    if( params->single.handle == dfu_data.connections[id].hw_type_handle ){
        memcpy(&dfu_data.connections[id].hardware_type, p_data, sizeof dfu_data.connections[id].hardware_type);
        if( dfu_data.connections[id].hardware_type == dfu_data.hardware_type ){
            LOG_DBG("Device with sample hardware type");        
            msgq_dfu_event q_event = {
                .event = READ_EVENT,
                .id_connection = id,
                .handle = dfu_data.connections[id].version_handle,
            };
            k_msgq_put(&msgq_send, &q_event, K_NO_WAIT);
        }
    } else if( params->single.handle == dfu_data.connections[id].version_handle ) {
        memcpy(&dfu_data.connections[id].version, p_data, sizeof dfu_data.connections[id].version);        
        if( cmp_version(&dfu_data.connections[id].version, &dfu_data.slots[0].version) > 0 && dfu_data.state == DFU_IDLE_STATE ){

            LOG_DBG("Device of connection %d has newer version", id);        
            msgq_dfu_event q_event = {
                .event = WRITE_EVENT,
                .id_connection = id,
                .handle = dfu_data.connections[id].exchange_handle,
                .offset = 0,
                .data_size = 0,
            };
            
            // Request to the device is going to updating state
            q_event.data[ q_event.data_size ] = SET_MSG;
            q_event.data_size++;
            q_event.data[ q_event.data_size ] = STATE_CMD;
            q_event.data_size++;
            q_event.data[ q_event.data_size ] = DFU_UPDATING_STATE;            
            q_event.data_size++;                        

            k_msgq_put(&msgq_send, &q_event, K_NO_WAIT);
        }
    } else {
        LOG_WRN("Handle unknown");        
    }

    err_read_func:
	return BT_GATT_ITER_CONTINUE;
}

/*
 * Exchange func
 */

static void exchange_func(struct bt_conn *conn, u8_t err, struct bt_gatt_exchange_params *params)
{
    int id = find_connection(conn);
    if( id < 0 ){
        LOG_ERR("Not found ID");
        return;
    }

    LOG_INF("Connection %d exchanges %s", id, err == 0U ? "successful" : "failed");
    if (!err) {
        u16_t mtu    = bt_gatt_get_mtu(conn);
        dfu_data.connections[id].mtu = mtu - 3;
        LOG_WRN("New MTU is %d", mtu);
    }
}

/*
 * Discover function
 */

static u8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          struct bt_gatt_discover_params *params)
{
    static struct bt_gatt_chrc *gatt_chrc;
    int id = find_connection(conn);

    if( id < 0 ){
        LOG_ERR("Not found ID");
        return BT_GATT_ITER_STOP;
    }

    if (!attr) {
        dfu_data.connections[id].enable_dfu_read = dfu_data.connections[id].version_handle && dfu_data.connections[id].hw_type_handle;
        dfu_data.connections[id].enable_dfu_write = dfu_data.connections[id].exchange_handle ? 1 : 0;

        if( dfu_data.connections[id].enable_dfu_read && dfu_data.connections[id].enable_dfu_write ){
            LOG_WRN("Discover complete and found OTA service!");
        } else if ( dfu_data.connections[id].enable_dfu_read ){
            LOG_WRN("Discover complete and found OTA service, but only DFU read!");
        } else if ( dfu_data.connections[id].enable_dfu_write ){
            LOG_WRN("Discover complete and found OTA service, but only DFU write!");
        } else {
            LOG_WRN("Discover complete and not found OTA service!");
        }

        if( dfu_data.connections[id].enable_dfu_read ){
            LOG_DBG("Request hardware type");
            msgq_dfu_event q_event = {
                .event = READ_EVENT,
                .id_connection = id,
                .handle = dfu_data.connections[id].hw_type_handle,
            };

            k_msgq_put(&msgq_send, &q_event, K_NO_WAIT);
        }
        (void) memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }
    
    switch (params->type) {
    case BT_GATT_DISCOVER_SECONDARY:
    case BT_GATT_DISCOVER_PRIMARY:
        break;
    case BT_GATT_DISCOVER_CHARACTERISTIC:
        gatt_chrc = attr->user_data;
        if (!bt_uuid_cmp(gatt_chrc->uuid, &efu_bt_chr_vrs_uuid.uuid)) {
            LOG_DBG("Find version characteristic with handle: 0x%x", attr->handle + 1);
            dfu_data.connections[id].version_handle = attr->handle + 1;
        } else if ( !bt_uuid_cmp(gatt_chrc->uuid, &efu_bt_chr_exg_uuid.uuid) ){
            LOG_DBG("Find exchange characteristic with handle: 0x%x", attr->handle + 1);
            dfu_data.connections[id].exchange_handle = attr->handle + 1;
        } else if ( !bt_uuid_cmp(gatt_chrc->uuid, &efu_bt_chr_hw_tp_uuid.uuid) ){
            LOG_DBG("Find hardware type characteristic with handle: 0x%x", attr->handle + 1);
            dfu_data.connections[id].hw_type_handle = attr->handle + 1;
        }
        break;
    case BT_GATT_DISCOVER_INCLUDE:
        break;
    default:
        break;
    }

    return BT_GATT_ITER_CONTINUE;
}

/*
 * Write handler for the OTA characteristic; processes an incoming OTA request.
 */
static ssize_t dfu_bt_chr_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, u16_t len, u16_t offset, u8_t flags)
{
    LOG_DBG("Arrived %d bytes", len);

    int id = find_connection(conn);

    if( id < 0 ){
        goto error_write_chr;
    }

    if( !dfu_data.connections[id].enable_dfu_write ){
        goto error_write_chr;
    }

    u8_t *p = cast(u8_t *, buf);

    msgq_dfu_event q_event = {
        .event = WRITE_EVENT,
        .id_connection = id,
        .handle = dfu_data.connections[id].exchange_handle,
    };

    memcpy(q_event.data, p, len);

    k_msgq_put(&msgq_receive, &q_event, K_NO_WAIT);

    error_write_chr:
    return len;
}

/*
 * Read handler for the OTA characteristic; processes an incoming OTA request.
 */
static ssize_t read_efu_chr_vrs(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       u16_t len, u16_t offset)
{
    struct image_version ver;
    memcpy(&ver, &dfu_data.slots[0].version, sizeof dfu_data.slots[0].version);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ver, sizeof(ver));
}

static ssize_t read_efu_chr_hw_tp(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       u16_t len, u16_t offset)
{
    u8_t hardware_type;
    memcpy(&hardware_type, &dfu_data.hardware_type, sizeof dfu_data.hardware_type);
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &hardware_type, sizeof(hardware_type));
}

/*
 * GATT attributs 
 */

static struct bt_gatt_attr efu_bt_attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&efu_bt_svc_uuid),
    BT_GATT_CHARACTERISTIC( &efu_bt_chr_hw_tp_uuid.uuid, 
                            BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ, 
                            read_efu_chr_hw_tp, 
                            NULL, 
                            &dfu_data.hardware_type),
    BT_GATT_CHARACTERISTIC( &efu_bt_chr_vrs_uuid.uuid, 
                            BT_GATT_CHRC_READ,
                            BT_GATT_PERM_READ, 
                            read_efu_chr_vrs, 
                            NULL, 
                            &dfu_data.slots[0].version),
    BT_GATT_CHARACTERISTIC( &efu_bt_chr_exg_uuid.uuid, 
                            BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                            BT_GATT_PERM_WRITE, // | BT_GATT_PERM_WRITE_AUTHEN | BT_GATT_PERM_WRITE_ENCRYPT, 
                            NULL, 
                            dfu_bt_chr_write, 
                            NULL),
};

/*
 * GATT service 
 */

static struct bt_gatt_service efu_bt_svc = BT_GATT_SERVICE(efu_bt_attrs);

int dfu_init(u8_t hardware_type){

    LOG_INF("DFU init:");

    if( dfu_data.slots[0].flash_area_id || dfu_data.slots[0].flash_area_id ){
        return -EBUSY;
    }

    dfu_data.slots[0].flash_area_id = DT_FLASH_AREA_IMAGE_0_ID;
    dfu_data.slots[1].flash_area_id = DT_FLASH_AREA_IMAGE_1_ID;

    dfu_data.hardware_type = hardware_type;
    dfu_data.can_reboot = 1;

    LOG_INF(" + Images:");

    for (size_t slot = 0; slot < NUM_OF_SLOTS; ++slot) {
        read_image_info(dfu_data.slots[slot].flash_area_id, &dfu_data.slots[slot].image_size,
                        &dfu_data.slots[slot].version);
        if (flash_area_open(dfu_data.slots[slot].flash_area_id, &dfu_data.slots[slot].fap)) {
            LOG_ERR(" + + Error opening slot %d area", slot);
            return -ENOMEM;
        }
        if (dfu_data.slots[slot].version.iv_major == (u8_t)(-1)
            && dfu_data.slots[slot].version.iv_minor == (u8_t)(-1)
            && dfu_data.slots[slot].version.iv_revision == (u16_t)(-1)
            && dfu_data.slots[slot].version.iv_build_num == (u32_t)(-1)) {
            LOG_WRN(" + + Slot %d doesn't have image", slot);
        } else {
            LOG_INF(" + + Slot %d is running version: %d.%d.%d+%d with %d bytes", slot,
                    dfu_data.slots[slot].version.iv_major, dfu_data.slots[slot].version.iv_minor,
                    dfu_data.slots[slot].version.iv_revision,
                    dfu_data.slots[slot].version.iv_build_num, dfu_data.slots[slot].image_size);
        }
    }

    LOG_INF(" + Start threads");
    k_thread_start(receive_channel_thread);
    k_thread_start(send_channel_thread);

    return 0;
}

int dfu_confirm_image()
{
    return boot_write_img_confirmed();
}

int dfu_mode(u8_t enable)
{
    if (enable != 0 && enable != 1 && enable != 2 ) {
        LOG_ERR(" + Start threads");
        return -ENOTSUP;
    } else if (dfu_data.state == cast(enum dfu_state, enable)) {
        return -EINVAL;
    }
    dfu_data.state     = enable ? DFU_IDLE_STATE : APP_STATE;
    dfu_data.immediate = enable & 1;
    if (dfu_data.state == DFU_IDLE_STATE) {
        return dfu_bt_register();
    }
    return dfu_bt_unregister();
}

int dfu_is_connected()
{
    return dfu_data.number_of_connections;
}

int init_write()
{
    if (dfu_data.state != DFU_IDLE_STATE) {
        return -EINVAL;
    }

    if (flash_img_init(&dfu_data.ctx)) {
        LOG_ERR("Init flash image");
        dfu_data.state  = DFU_ERROR_STATE;
        dfu_data.status = ERROR_INIT_WRITE_STATUS;
        return -EACCES;
    }

    if (flash_area_erase(dfu_data.ctx.flash_area, 0, dfu_data.ctx.flash_area->fa_size)) {
        LOG_ERR("Erase flash");
        dfu_data.state  = DFU_ERROR_STATE;
        dfu_data.status = ERROR_ERASE_STATUS;
        return -EACCES;
    }

    if (flash_img_bytes_written(&dfu_data.ctx)) {
        LOG_ERR("Buff in memory");
        dfu_data.state = DFU_ERROR_STATE;
        return -EEXIST;
    }
    return 0;
};

int dfu_bt_register(void)
{
    LOG_INF("DFU Service register!");
    return bt_gatt_service_register(&efu_bt_svc);
}

int dfu_bt_unregister(void)
{
    LOG_INF("DFU Service unregister!");
    return bt_gatt_service_unregister(&efu_bt_svc);
}

int dfu_connect(struct bt_conn *conn)
{
    LOG_INF("Connect DFU");
    int err;
    int id;

    if ( err = add_connection(conn), err) {
        LOG_ERR("Aditional connection (err %d)", err);
        goto end_dfu_connect;
    }
    id = find_connection(conn);

    if( id < 0 ){
        err = id;
        LOG_ERR("Not found connection");
        goto end_dfu_connect;
    }
    struct bt_conn * conn_id = dfu_data.connections[id].conn;
    dfu_data.exchange_params.func = exchange_func;

    if (err = bt_gatt_exchange_mtu(conn_id, &dfu_data.exchange_params), err) {
        LOG_ERR("Exchange failed (err %d)", err);
        goto end_dfu_connect;
    } else {
        LOG_DBG("Exchange pending...");
    }

    dfu_data.discover_params.func         = discover_func;
    dfu_data.discover_params.start_handle = 0x0001;
    dfu_data.discover_params.end_handle   = 0xffff;
    dfu_data.discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC;

    if (err = bt_gatt_discover(conn_id, &dfu_data.discover_params), err) {
        LOG_ERR("Discover failed (err %d)", err);
        goto end_dfu_connect;
    } else {
        LOG_DBG("Discover pending...");
    }

    end_dfu_connect:
    return err;
}

int dfu_disconnect(struct bt_conn *conn)
{
    LOG_INF("Disconnect DFU");
    int err;

    if (conn == NULL) {
        return -EFAULT;
    }

    if( err = purge_connection(conn), err ){
        return err;
    }
    dfu_data.number_of_connections--;

    if( !dfu_data.number_of_connections ){
        dfu_data.can_reboot = 1;
    }

    return 0;
}

static void write_func(struct bt_conn *conn, u8_t err, struct bt_gatt_write_params *params)
{
    if( err ){
        LOG_DBG("Write complete: err %u", err);
    } else {
        LOG_DBG("Write complete");
    }
    (void) memset(&dfu_data.write_params, 0, sizeof(dfu_data.write_params));
}

int send_request_write(msgq_dfu_event event)
{
    int err;
    u8_t attempts = 0;

    if (!dfu_data.connections[event.id_connection].conn) {
        LOG_ERR("Not connected");
        return -ENOEXEC;
    }

    if(!event.handle){
        LOG_ERR("Not get value handle");
        return -ENOEXEC;
    }

    if( event.data_size > dfu_data.connections[event.id_connection].mtu || event.data_size > MAX_MSG_BT_SIZE ){
        LOG_ERR("Message is bigger than buffer size");
        return -ENOEXEC;
    }

    while (dfu_data.write_params.func) {
        attempts++;
        if (attempts >= 5) {
            LOG_ERR("Write going");
            return -ENOEXEC;
        }
        k_sleep(K_MSEC(200));
    }

    memcpy(dfu_data.gatt_write_buf, event.data, event.data_size);

    dfu_data.write_params.data   = dfu_data.gatt_write_buf;
    dfu_data.write_params.length = event.data_size;
    dfu_data.write_params.handle = event.handle;
    dfu_data.write_params.offset = event.offset;
    dfu_data.write_params.func   = write_func;

    LOG_DBG("Sending %d bytes", dfu_data.write_params.length);

    err = bt_gatt_write(dfu_data.connections[event.id_connection].conn, &dfu_data.write_params);
    if (err) {
        LOG_ERR("Write failed (err %d)", err);
    } else {
        LOG_DBG("Write pending");
    }

    return err;
}

int send_request_read(msgq_dfu_event event){
    int err;
    u8_t attempts = 0;

    if (!dfu_data.connections[event.id_connection].conn) {
        LOG_ERR("Not connected");
        return -ENOEXEC;
    }

    if(!event.handle){
        LOG_ERR("Not get value handle");
        return -ENOEXEC;
    }

    while (dfu_data.read_params.func) {
        attempts++;
        if (attempts >= 5) {
            LOG_ERR("Read going");
            return -ENOEXEC;
        }
        k_sleep(K_MSEC(200));
    }
    
    dfu_data.read_params.func = read_func;
    dfu_data.read_params.handle_count = 1;
    dfu_data.read_params.single.handle = event.handle;
    dfu_data.read_params.single.offset = event.offset;

    if (err = bt_gatt_read(dfu_data.connections[event.id_connection].conn, &dfu_data.read_params), err) {
        LOG_ERR("Handle 0x%x GATT read request failed (err %d)", event.handle, err);
    } else {
        LOG_DBG("GATT read pending...");
    }

    return err;
}

int send_channel(struct device *dev){

    LOG_INF("Thread send channel");

    static msgq_dfu_event event;

    while(1) {
        k_msgq_get(&msgq_send, &event, K_FOREVER);
        LOG_DBG("New event in send channel");
        switch (event.event)
        {
        case READ_EVENT:
            LOG_DBG("Read event");
            send_request_read(event);
            break;
        case WRITE_EVENT:
            LOG_DBG("Write event");
            send_request_write(event);            
            break;
        default:
            LOG_DBG("Unknown event");
            break;
        }
        k_sleep(K_MSEC(10));
    }

    return 0;
}

int process_get_requests(msgq_dfu_event event){
    u16_t offset = 1;
    enum dfu_cmd command = cast(enum dfu_cmd, event.data[offset++]);
    int err = 0;

    msgq_dfu_event reply_event = {
        .offset = 0,
        .id_connection = event.id_connection,
        .handle = event.handle,
        .event = WRITE_EVENT,
        .data_size = 0,
    };

    if( command >= number_of_commands ){
        err = -EINVAL;
        LOG_ERR("Commands with error (%d)", err);
        goto err_process_get_requests;
    }

    reply_event.data[reply_event.data_size++] = STS_MSG;
    reply_event.data[reply_event.data_size++] = command;

    if( command == IMAGE_CHUNK_CMD && dfu_data.state == DFU_UPDATING_STATE ){
        LOG_DBG("Image chunk");
        u32_t chunk_offset;
        u16_t chunk_size;
        memcpy( &chunk_offset, event.data + offset, sizeof chunk_offset );
        
        // Data
        chunk_size = MIN(dfu_data.slots[0].image_size - chunk_offset, MAX_CHUNK_SIZE_IMAGE);
        memcpy(reply_event.data + reply_event.data_size, &chunk_offset, sizeof chunk_offset);
        reply_event.data_size += sizeof chunk_offset;
        memcpy(reply_event.data + reply_event.data_size, &chunk_size, sizeof chunk_size);
        reply_event.data_size += sizeof chunk_size;
        if (err = flash_area_read(dfu_data.slots[0].fap, chunk_offset, 
                    reply_event.data + reply_event.data_size, chunk_size), err) {
            LOG_ERR("Error read (%d)", err);
            dfu_data.state  = DFU_ERROR_STATE;
            dfu_data.status = ERROR_READ_STATUS;
            goto err_process_get_requests;
        }
        reply_event.data_size += chunk_size;
        k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
    } else if( command == STATE_CMD ){
        LOG_DBG("State");
        reply_event.data[reply_event.data_size++] = dfu_data.state;
        k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
    } else if( command == CONFIRM_IMAGE_CMD ){
        reply_event.data[reply_event.data_size++] = boot_is_img_confirmed();
        k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
    } else {
        LOG_WRN("Request invalid!");
        goto err_process_get_requests;
    }
    err_process_get_requests:
    return err;
}

int dfu_swap_slots(int permanent){
    
    if (!dfu_data.can_reboot) {
        LOG_ERR("Cannot reboot");
        return -EACCES;
    }

    int err = 0;
    if (err = boot_request_upgrade(permanent), err) {
        LOG_ERR("Boot request upgrade failed with err (%d)", err);
        return err;
    }

    sys_reboot(SYS_REBOOT_COLD);
    return err;
}

int process_set_requests(msgq_dfu_event event){

    u16_t offset = 1;
    enum dfu_cmd command = cast(enum dfu_cmd, event.data[offset++]);
    int err = 0;

    msgq_dfu_event reply_event = {
        .offset = 0,
        .id_connection = event.id_connection,
        .handle = event.handle,
        .event = WRITE_EVENT,
        .data_size = 0,
    };

    if( command >= number_of_commands ){
        err = -EINVAL;
        LOG_ERR("Commands with error (%d)", err);
        goto err_process_set_requests;
    }

    reply_event.data[reply_event.data_size++] = STS_MSG;
    reply_event.data[reply_event.data_size++] = command;
    
    if( command == STATE_CMD ){
        enum dfu_state new_state = cast(enum dfu_state, event.data[offset++]);
        LOG_DBG("New state: %d", new_state);
        if( (dfu_data.state == DFU_IDLE_STATE && 
            (new_state == DFU_DOWNLOADING_STATE || new_state == DFU_UPDATING_STATE)) || (dfu_data.state == DFU_UPDATING_STATE && new_state == DFU_UPDATING_STATE ) ){
            dfu_data.state = new_state;
            dfu_data.can_reboot = 0;
            LOG_INF("[State machine] Changed to %s", new_state == DFU_DOWNLOADING_STATE
                                        ? "DFU_DOWNLOADING_STATE" : "DFU_UPDATING_STATE");
            reply_event.data[reply_event.data_size++] = new_state;
            k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
            LOG_DBG("Put in message queue");
        } else {
            LOG_WRN("Unknown command type");
        }
    } else if( command == REBOOT_CMD && (dfu_data.state == DFU_DOWNLOADED_STATE || dfu_data.state == DFU_IDLE_STATE || dfu_data.state == DFU_ERROR_STATE) ){
        u8_t reboot_type = event.data[offset++];
        if( reboot_type == SYS_REBOOT_COLD || reboot_type == SYS_REBOOT_WARM ){
            sys_reboot(reboot_type);
        } else {
            LOG_WRN("Unknown reboot type");
        }
    } else if( command == TEST_IMAGE_CMD && (dfu_data.state == DFU_IDLE_STATE || dfu_data.state == DFU_DOWNLOADED_STATE) ) {
        u8_t permanent = event.data[offset++];
        if (dfu_data.immediate) {
            k_sleep(K_SECONDS(5));
            if( err = dfu_swap_slots(permanent), err){
                LOG_ERR("Test image error (%d)", err);
                dfu_data.state  = DFU_ERROR_STATE;
                dfu_data.status = ERROR_REBOOT_STATUS;
                goto err_process_set_requests;
            }
        } else {
            LOG_DBG(" + + Reboot programmed");
            // TODO:
        }
    } else if( command == CONFIRM_IMAGE_CMD && dfu_data.state == DFU_IDLE_STATE ) {
        if( err = dfu_confirm_image(), err ){
            LOG_ERR("Confirm image error (%d)", err);
            goto err_process_set_requests;
        }
    } else {
        LOG_WRN("Unknown command");
    }
    err_process_set_requests:
    return err;
}

int process_replies(msgq_dfu_event event){

    u16_t offset = 1;
    enum dfu_cmd command = cast(enum dfu_cmd, event.data[offset++]);
    int err = 0;

    msgq_dfu_event reply_event = {
        .offset = 0,
        .id_connection = event.id_connection,
        .handle = event.handle,
        .event = WRITE_EVENT,
        .data_size = 0,
    };

    if( command >= number_of_commands ){
        err = -EINVAL;
        LOG_ERR("Commands with error (%d)", err);
        goto err_process_replies;
    }

    LOG_DBG("Check command:");
    if( command == IMAGE_CHUNK_CMD && dfu_data.state == DFU_DOWNLOADING_STATE ){
        LOG_DBG(" + Image chunk:");
        if( event.id_connection != dfu_data.id_connection_update ) {
            goto err_process_replies;
        }

        u32_t chunk_offset;
        u16_t chunk_size;
        memcpy( &chunk_offset, event.data + offset, sizeof chunk_offset );
        offset += sizeof chunk_offset;
        memcpy( &chunk_size, event.data + offset, sizeof chunk_size );
        offset += sizeof chunk_size;

        if( chunk_offset != dfu_data.offset_update ){
            goto err_process_replies;
        }

        // Finish update
        if( !chunk_size ){
            LOG_DBG(" + + Write last image chunk");
            if( err = flash_img_buffered_write(&dfu_data.ctx, event.data + offset, chunk_size, true), err){
                dfu_data.state = DFU_ERROR_STATE;
                dfu_data.status = ERROR_WRITE_STATUS;                
                LOG_ERR("Reboot error (%d)", err);
                goto err_process_replies;
            }
            LOG_INF("[State machine] Changed to DFU_DOWNLOADED_STATE");
            dfu_data.can_reboot = 1;
            dfu_data.state = DFU_DOWNLOADED_STATE;

            reply_event.data[ reply_event.data_size++ ] = STS_MSG;
            reply_event.data[ reply_event.data_size++ ] = STATE_CMD;
            reply_event.data[ reply_event.data_size++ ] = dfu_data.state;
            k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);

        } else {
            LOG_DBG(" + + Write image chunk");
            if (err = flash_img_buffered_write(&dfu_data.ctx, event.data + offset, chunk_size, false), err) {
                LOG_ERR("Write error (%d)", err);
                dfu_data.state  = DFU_ERROR_STATE;
                dfu_data.status = ERROR_WRITE_STATUS;
                goto err_process_replies;
            }
            dfu_data.offset_update += chunk_size;

            reply_event.data[ reply_event.data_size++ ] = GET_MSG;
            reply_event.data[ reply_event.data_size++ ] = IMAGE_CHUNK_CMD;
            memcpy( reply_event.data + reply_event.data_size, &dfu_data.offset_update, sizeof dfu_data.offset_update );
            reply_event.data_size += sizeof dfu_data.offset_update;

            LOG_DBG(" + + Request image chunk with offset %d", dfu_data.offset_update);
            k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
        }
    } else if ( command == STATE_CMD ){
        LOG_DBG(" + State:");
        enum dfu_state state = cast(enum dfu_state, event.data[offset++]);
        if( dfu_data.state == DFU_IDLE_STATE && state == DFU_UPDATING_STATE ){
            LOG_DBG(" + + Will receive data to update of connection %d!", event.id_connection);
            
            dfu_data.id_connection_update = event.id_connection;
            dfu_data.can_reboot = 0;

            if( err = init_write(), err ){
                LOG_ERR("Init write error (%d)", err);
                dfu_data.state  = DFU_ERROR_STATE;
                dfu_data.status = ERROR_INIT_WRITE_STATUS;
                goto err_process_replies;
            }
            LOG_INF("[State machine] Changed to DFU_DOWNLOADING_STATE");
            dfu_data.state = DFU_DOWNLOADING_STATE;
            dfu_data.offset_update = 0;
            reply_event.data[ reply_event.data_size++ ] = GET_MSG;
            reply_event.data[ reply_event.data_size++ ] = IMAGE_CHUNK_CMD;
            memcpy( reply_event.data + reply_event.data_size, &dfu_data.offset_update, sizeof dfu_data.offset_update );
            reply_event.data_size += sizeof dfu_data.offset_update;
            
            LOG_DBG(" + + Request image chunk with offset %d", dfu_data.offset_update);
            k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
        } else if( dfu_data.state == DFU_UPDATING_STATE && state == DFU_DOWNLOADED_STATE ){
            reply_event.data[ reply_event.data_size++ ] = SET_MSG;
            reply_event.data[ reply_event.data_size++ ] = TEST_IMAGE_CMD;
            reply_event.data[ reply_event.data_size++ ] = SYS_REBOOT_COLD;

            LOG_DBG(" + + Request to device test image");
            k_msgq_put(&msgq_send, &reply_event, K_NO_WAIT);
        } else {
            LOG_WRN("Not is valid command");
        }
    } else {
        LOG_WRN("Not is valid command type!");
    }
    err_process_replies:
    return err;
}

int receive_channel(struct device *dev){

    LOG_INF("Thread receive channel");

    static msgq_dfu_event event;
    enum dfu_msg_type msg_type;

    while(1) {
        k_msgq_get(&msgq_receive, &event, K_FOREVER);
        LOG_DBG("New event in receive channel");
        switch (event.event)
        {
        case WRITE_EVENT:
            LOG_DBG("Write event");
            msg_type = cast(enum dfu_msg_type, event.data[0]);
            switch (msg_type)
            {
            case SET_MSG:
                process_set_requests(event);
                break;
            case GET_MSG:
                process_get_requests(event);
                break;
            case STS_MSG:
                process_replies(event);
                break;
            default:
                LOG_WRN("Unknown event");
                break;
            }
            break;
        case TIMEOUT_EVENT:
            LOG_DBG("Time out event");
            break;
        default:
            LOG_WRN("Unknown event");
            break;
        }
        k_sleep(K_MSEC(10));
    }
    return 0;
}

static int cmd_mode(const struct shell *shell, size_t argc, char *argv[])
{
    u8_t in = strtoul(argv[1], NULL, 10);

    if (dfu_mode(in)) {
        shell_error(shell, "Error");
    }

    return 0;
}

static int cmd_show_connections(const struct shell *shell, size_t argc, char *argv[])
{
    shell_print(shell, "There are %d connections", dfu_data.number_of_connections);   
    for (int i = 0 ; i < MAX_CONNECTION ; ++i ){
        if( !dfu_data.connections[i].conn ){
            shell_print(shell, "Channel %d: doesn't have connection", i);   
        } else {
            if ( !dfu_data.connections[i].enable_dfu_read ){
                shell_print(shell, "Channel %d: connection doesn't have DFU read", i);
            } else {
                shell_print(shell, "Channel %d: hardware type is %d and version is %d.%d.%d+%d", i, dfu_data.connections[i].hardware_type, 
                dfu_data.connections[i].version.iv_major, dfu_data.connections[i].version.iv_minor, 
                dfu_data.connections[i].version.iv_revision, dfu_data.connections[i].version.iv_build_num);
            }
        }
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    dfu_cmds, SHELL_CMD_ARG(mode, NULL, "Unregister DFU service", cmd_mode, 2, 0),
    SHELL_CMD_ARG(show_connections, NULL, "Show all connections", cmd_show_connections, 1, 0), SHELL_SUBCMD_SET_END);


static int cmd_dfu(const struct shell *shell, size_t argc, char **argv)
{
    if (argc == 1) {
        shell_help(shell);
        /* shell returns 1 when help is printed */
        return 1;
    }

    shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

    return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(dfu, &dfu_cmds, "Device firmware update shell commands", cmd_dfu, 1, 1);
