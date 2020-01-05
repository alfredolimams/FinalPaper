#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/hci.h>
#include <bluetooth/uuid.h>
#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <errno.h>
#include <flash.h>
#include <flash_map.h>
#include <misc/byteorder.h>
#include <misc/printk.h>
#include <settings/settings.h>
#include <stddef.h>
#include <string.h>
#include <zephyr.h>
#include <zephyr/types.h>

#include "image.h"

#define EVENT_BYTES 1
#define COMMAND_BYTES 1
#define OFFSET_BYTES 4
#define CHUNK_LENGTH_BYTES 2

#define NUM_OF_SLOTS 2
#define MAX_MSG_BT_SIZE 100
#define MAX_CHUNK_SIZE_IMAGE \
    (MAX_MSG_BT_SIZE - EVENT_BYTES - COMMAND_BYTES - OFFSET_BYTES - CHUNK_LENGTH_BYTES)
#define MAX_MSGQ_SIZE MAX_MSG_BT_SIZE

// #define STACKSIZE 4096
// #define PRIORITY 7
#define cast(type, value) ((type) value)

#define MAX_CONNECTION  5

enum dfu_cmd {
    IMAGE_CHUNK_CMD      = 0,  // Image chunk command type
    STATE_CMD            = 1,  // State command type
    REBOOT_CMD           = 2,  // Reboot command type
    ERASE_IMAGE_CMD      = 3,  // Erase image command type
    TEST_IMAGE_CMD       = 4,  // Test image command type
    CONFIRM_IMAGE_CMD    = 5,  // Confirm image command type
    number_of_commands   = 6,
};

enum dfu_msg_type {
    SET_MSG         = 0,  // Set
    GET_MSG         = 1,  // Get
    STS_MSG         = 2,  // Status
    number_of_messages_type  = 3,
};

enum dfu_status {
    OK_STATUS               = 0,  // OK
    ERROR_READ_STATUS       = 1,  // Error in read image
    ERROR_WRITE_STATUS      = 2,  // Error in write image
    ERROR_INIT_WRITE_STATUS = 3,  // Error in write image
    ERROR_ERASE_STATUS      = 4,  // Error in erase image
    ERROR_VERIFY_STATUS     = 5,  // Error in verify image
    ERROR_REBOOT_STATUS     = 6,  // Error in verify image
    number_of_status        = 7,
};

enum dfu_state {
    APP_STATE               = 0,  // Application is running with DFU disable
    DFU_IDLE_STATE          = 1,  // DFU enable, but not connected with no any device with DFU enable
    DFU_UPDATING_STATE      = 2,  // Updating firmare of others devices
    DFU_DOWNLOADING_STATE   = 3,  // Downloading new firmare
    DFU_DOWNLOADED_STATE    = 4,  // Device updated
    DFU_ERROR_STATE         = 5,  // State to any error in DFU
    number_of_states        = 6,
};

enum dfu_event {
    READ_EVENT       = 0,  // Set event
    WRITE_EVENT      = 1,  // Get event
    TIMEOUT_EVENT    = 3,  // Timeout event
    ABORT_EVENT      = 4,  // Abort event
    number_of_events = 5,
};

struct info_image {
    struct image_version version;
    u32_t image_size;
    u8_t flash_area_id;
    const struct flash_area *fap;
};

typedef struct
{
    // Connection
    struct bt_conn *conn;
    u16_t hw_type_handle;
    u16_t version_handle;
    u16_t exchange_handle;
    u8_t enable_dfu_read;
    u8_t enable_dfu_write;
    u16_t mtu;

    // Device information
    struct image_version version;
    u8_t hardware_type;
    
} gatt_connection;

struct dfu_data_t {

    // Device information 
    // + Hardware type
    u8_t hardware_type;
    // + Images
    struct info_image slots[NUM_OF_SLOTS];
    struct flash_img_context ctx;
    // + Update information
    int id_connection_update;
    u32_t offset_update;
    // + State machine
    enum dfu_state state;   /* State of the DFU device */
    enum dfu_status status; /* Status of the DFU device */
    
    u8_t can_reboot;
    u8_t immediate;
    u8_t timeout_events;

    // + Connections
    u8_t number_of_connections;
    gatt_connection connections[10];

    // Gatt information
    struct bt_gatt_write_params write_params;
    struct bt_gatt_discover_params discover_params;
    struct bt_gatt_exchange_params exchange_params;
    struct bt_gatt_read_params read_params;
    u8_t gatt_write_buf[MAX_MSG_BT_SIZE];
};

typedef struct {
    enum dfu_event event;
    u8_t id_connection;
    u16_t handle;
    u16_t offset;
    u8_t data[MAX_MSGQ_SIZE];
    u16_t data_size;
} msgq_dfu_event;

typedef void (*state_func)(msgq_dfu_event event);

typedef struct {
    state_func exec;
} state_function;

/**
 * @brief 
 * 
 * @param hardware_type 
 * @return int 
 */

int dfu_init(u8_t hardware_type);

/**
 * @brief 
 * 
 * @param enable 
 *          0 - Desable
 *          1 - Enable with auto-reboot
 *          2 - Enable withless auto-reboot
 * @return int 
 */

int dfu_mode(u8_t enable);

/**
 * @brief 
 * 
 * @return int 
 */
int dfu_confirm_image();

/**
 * @brief 
 * 
 * @return int 
 */
int dfu_bt_register(void);

/**
 * @brief 
 * 
 * @return int 
 */
int dfu_bt_unregister(void);

/**
 * @brief 
 * 
 * @param conn 
 * @return int 
 */
int dfu_connect(struct bt_conn *conn);

/**
 * @brief 
 * 
 * @param conn 
 * @return int 
 */
int dfu_disconnect(struct bt_conn *conn);