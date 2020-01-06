#include "cmds.h"

// ######################              CMD DFU             ######################

static int cmd_mesh_send_dfu_property_status(const struct shell *shell, size_t argc, char **argv)
{
    u8_t id_property = atoi(argv[1]);
    dfu_request(id_property);
    return 0;
}

static int cmd_show_dfu_state(const struct shell *shell, size_t argc, char **argv)
{
    shell_print(shell, "My state is %d", get_state() );
    return 0;
}

static int cmd_show_dfu_timeout_events(const struct shell *shell, size_t argc, char **argv)
{
    shell_print(shell, "It happened timeout events %d", get_timeout_events() );
    return 0;
}


SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_dfu, SHELL_CMD(send_property_status, NULL, "Send property of device.", cmd_mesh_send_dfu_property_status),
    SHELL_CMD(show_dfu_state, NULL, "Show dfu state.", cmd_show_dfu_state),
    SHELL_CMD(show_timeout_events, NULL, "Show timeout events.", cmd_show_dfu_timeout_events),
    SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(dfu, &sub_dfu, "Bluetooth commands", NULL);