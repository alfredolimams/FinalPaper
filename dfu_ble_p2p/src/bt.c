#include "bt.h"

#include <shell/shell.h>
#include <shell/shell_uart.h>

#include "dfu_ble.h"
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define HELP_NONE "[none]"
#define HELP_ADDR_LE "<address: XX:XX:XX:XX:XX:XX> <type: (public|random)>"

#define NAME_LEN 30
#define CHAR_SIZE_MAX 512

static u8_t selected_id = BT_ID_DEFAULT;

struct bt_conn *default_conn;

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, 0x84, 0xaa, 0x60, 0x74, 0x52, 0x8a, 0x8b, 0x86, 0xd3, 0x4c,
                  0xb7, 0x1d, 0x1d, 0xdc, 0x53, 0x8d),
};

static bool data_cb(struct bt_data *data, void *user_data)
{
    char *name = user_data;

    switch (data->type) {
    case BT_DATA_NAME_SHORTENED:
    case BT_DATA_NAME_COMPLETE:
        memcpy(name, data->data, MIN(data->data_len, NAME_LEN - 1));
        return false;
    default:
        return true;
    }
}

static void device_found(const bt_addr_le_t *addr, s8_t rssi, u8_t evtype,
                         struct net_buf_simple *buf)
{
    char le_addr[BT_ADDR_LE_STR_LEN];
    char name[NAME_LEN];

    (void) memset(name, 0, sizeof(name));

    bt_data_parse(buf, data_cb, name);

    bt_addr_le_to_str(addr, le_addr, sizeof(le_addr));
    printk("[DEVICE]: %s, AD evt type %u, RSSI %i %s\n", le_addr, evtype, rssi, name);
}

static void advertise(void)
{
    int rc;

    bt_le_adv_stop();

    rc = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (rc) {
        printk("Advertising failed to start (rc %d)\n", rc);
        return;
    }

    printk("Advertising successfully started\n");
}

static void connected(struct bt_conn *conn, u8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    conn_addr_str(conn, addr, sizeof(addr));

    if (err) {
        printk("Failed to connect to %s (%u)\n", addr, err);
        // goto done;
    }

    printk("Connected: %s\n", addr);

    if (!default_conn) {
        default_conn = bt_conn_ref(conn);
    };

    if( err = dfu_connect(conn), err ){
        printk("Error (%d)\n", err);
    }

    // done:
    /* clear connection reference for sec mode 3 pairing */
    // if (pairing_conn) {
    // 	bt_conn_unref(pairing_conn);
    // 	pairing_conn = NULL;
    // }
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    conn_addr_str(conn, addr, sizeof(addr));
    printk("Disconnected: %s (reason %u)\n", addr, reason);

    dfu_disconnect(conn);

    if (default_conn == conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

static int char2hex(const char *c, u8_t *x)
{
    if (*c >= '0' && *c <= '9') {
        *x = *c - '0';
    } else if (*c >= 'a' && *c <= 'f') {
        *x = *c - 'a' + 10;
    } else if (*c >= 'A' && *c <= 'F') {
        *x = *c - 'A' + 10;
    } else {
        return -EINVAL;
    }

    return 0;
}

static int hexstr2array(const char *str, u8_t *array, u8_t size)
{
    int i, j;
    u8_t tmp;

    if (strlen(str) != ((size * 2U) + (size - 1))) {
        return -EINVAL;
    }

    for (i = size - 1, j = 1; *str != '\0'; str++, j++) {
        if (!(j % 3) && (*str != ':')) {
            return -EINVAL;
        } else if (*str == ':') {
            i--;
            continue;
        }

        array[i] = array[i] << 4;

        if (char2hex(str, &tmp) < 0) {
            return -EINVAL;
        }

        array[i] |= tmp;
    }

    return 0;
}

int str2bt_addr(const char *str, bt_addr_t *addr)
{
    return hexstr2array(str, addr->val, 6);
}

void conn_addr_str(struct bt_conn *conn, char *addr, size_t len)
{
    struct bt_conn_info info;

    if (bt_conn_get_info(conn, &info) < 0) {
        addr[0] = '\0';
        return;
    }

    switch (info.type) {
#if defined(CONFIG_BT_BREDR)
    case BT_CONN_TYPE_BR:
        bt_addr_to_str(info.br.dst, addr, len);
        break;
#endif
    case BT_CONN_TYPE_LE:
        bt_addr_le_to_str(bt_conn_get_dst(conn), addr, len);
        break;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected    = connected,
    .disconnected = disconnected,
    // 	.le_param_req = le_param_req,
    // 	.le_param_updated = le_param_updated,
    // #if defined(CONFIG_BT_SMP)
    // 	.identity_resolved = identity_resolved,
    // #endif
    // #if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
    // 	.security_changed = security_changed,
    // #endif
};

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }


    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    default_conn = NULL;

    bt_conn_cb_register(&conn_callbacks);
    advertise();
}

int bt_init()
{
    int err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
    }
    return err;
}

static int cmd_active_scan_on(const struct shell *shell, int dups)
{
    int err;
    struct bt_le_scan_param param = {.type       = BT_HCI_LE_SCAN_ACTIVE,
                                     .filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_ENABLE,
                                     .interval   = BT_GAP_SCAN_FAST_INTERVAL,
                                     .window     = BT_GAP_SCAN_FAST_WINDOW};

    if (dups >= 0) {
        param.filter_dup = dups;
    }

    err = bt_le_scan_start(&param, device_found);
    if (err) {
        shell_error(shell,
                    "Bluetooth set active scan failed "
                    "(err %d)",
                    err);
        return err;
    } else {
        shell_print(shell, "Bluetooth active scan enabled");
    }

    return 0;
}

static int cmd_passive_scan_on(const struct shell *shell, int dups)
{
    struct bt_le_scan_param param = {.type       = BT_HCI_LE_SCAN_PASSIVE,
                                     .filter_dup = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE,
                                     .interval   = 0x10,
                                     .window     = 0x10};
    int err;

    if (dups >= 0) {
        param.filter_dup = dups;
    }

    err = bt_le_scan_start(&param, device_found);
    if (err) {
        shell_error(shell,
                    "Bluetooth set passive scan failed "
                    "(err %d)",
                    err);
        return err;
    } else {
        shell_print(shell, "Bluetooth passive scan enabled");
    }

    return 0;
}

static int cmd_scan_off(const struct shell *shell)
{
    int err;

    err = bt_le_scan_stop();
    if (err) {
        shell_error(shell, "Stopping scanning failed (err %d)", err);
        return err;
    } else {
        shell_print(shell, "Scan successfully stopped");
    }

    return 0;
}

static int cmd_id_show(const struct shell *shell, size_t argc, char *argv[])
{
    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t i, count = CONFIG_BT_ID_MAX;

    bt_id_get(addrs, &count);

    for (i = 0; i < count; i++) {
        char addr_str[BT_ADDR_LE_STR_LEN];

        bt_addr_le_to_str(&addrs[i], addr_str, sizeof(addr_str));
        shell_print(shell, "%s%zu: %s", i == selected_id ? "*" : " ", i, addr_str);
    }

    return 0;
}

static int str2bt_addr_le(const char *str, const char *type, bt_addr_le_t *addr)
{
    int err;

    err = str2bt_addr(str, &addr->a);
    if (err < 0) {
        return err;
    }

    if (!strcmp(type, "public") || !strcmp(type, "(public)")) {
        addr->type = BT_ADDR_LE_PUBLIC;
    } else if (!strcmp(type, "random") || !strcmp(type, "(random)")) {
        addr->type = BT_ADDR_LE_RANDOM;
    } else {
        return -EINVAL;
    }

    return 0;
}

static int cmd_connect_le(const struct shell *shell, size_t argc, char *argv[])
{
    int err;
    bt_addr_le_t addr;
    struct bt_conn *conn;

    err = str2bt_addr_le(argv[1], argv[2], &addr);
    if (err) {
        shell_error(shell, "Invalid peer address (err %d)", err);
        return err;
    }

    conn = bt_conn_create_le(&addr, BT_LE_CONN_PARAM_DEFAULT);

    if (!conn) {
        shell_error(shell, "Connection failed");
        return -ENOEXEC;
    } else {
        shell_print(shell, "Connection pending");

        /* unref connection obj in advance as app user */
        bt_conn_unref(conn);
    }

    return 0;
}

static int cmd_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
    struct bt_conn *conn;
    int err;

    if (default_conn && argc < 3) {
        conn = bt_conn_ref(default_conn);
    } else {
        bt_addr_le_t addr;

        if (argc < 3) {
            shell_help(shell);
            return SHELL_CMD_HELP_PRINTED;
        }

        err = str2bt_addr_le(argv[1], argv[2], &addr);
        if (err) {
            shell_error(shell, "Invalid peer address (err %d)", err);
            return err;
        }

        conn = bt_conn_lookup_addr_le(selected_id, &addr);
    }

    if (!conn) {
        shell_error(shell, "Not connected");
        return -ENOEXEC;
    }

    err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (err) {
        shell_error(shell, "Disconnection failed (err %d)", err);
        return err;
    }

    bt_conn_unref(conn);

    return 0;
}

static int cmd_scan(const struct shell *shell, size_t argc, char *argv[])
{
    const char *action;
    int dups = -1;

    /* Parse duplicate filtering data */
    if (argc >= 3) {
        const char *dup_filter = argv[2];

        if (!strcmp(dup_filter, "dups")) {
            dups = BT_HCI_LE_SCAN_FILTER_DUP_DISABLE;
        } else if (!strcmp(dup_filter, "nodups")) {
            dups = BT_HCI_LE_SCAN_FILTER_DUP_ENABLE;
        } else {
            shell_help(shell);
            return SHELL_CMD_HELP_PRINTED;
        }
    }

    action = argv[1];
    if (!strcmp(action, "on")) {
        return cmd_active_scan_on(shell, dups);
    } else if (!strcmp(action, "off")) {
        return cmd_scan_off(shell);
    } else if (!strcmp(action, "passive")) {
        return cmd_passive_scan_on(shell, dups);
    } else {
        shell_help(shell);
        return SHELL_CMD_HELP_PRINTED;
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(bt_cmds,
                               SHELL_CMD_ARG(scan, NULL,
                                             "<value: on, passive, off> <dup filter: dups, nodups>",
                                             cmd_scan, 2, 1),
                               SHELL_CMD(id-show, NULL, "Show ids.", cmd_id_show),
                               SHELL_CMD_ARG(connect, NULL, HELP_ADDR_LE, cmd_connect_le, 3, 0),
                               SHELL_CMD_ARG(disconnect, NULL, HELP_NONE, cmd_disconnect, 1, 0),
                               SHELL_SUBCMD_SET_END);

static int cmd_bt(const struct shell *shell, size_t argc, char **argv)
{
    if (argc == 1) {
        shell_help(shell);
        return SHELL_CMD_HELP_PRINTED;
    }

    shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

    return -EINVAL;
}

SHELL_CMD_REGISTER(bt, &bt_cmds, "Bluetooth shell commands", cmd_bt);
