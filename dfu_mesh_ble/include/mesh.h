#ifndef _MESH_H
#define _MESH_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <logging/log.h>
#include <misc/printk.h>
#include <settings/settings.h>

#include "mesh_callbacks.h"

int output_number(bt_mesh_output_action_t action, u32_t number);

void prov_complete(u16_t net_idx, u16_t addr);

void prov_reset(void);

void bt_ready(int err);

extern struct bt_mesh_model* dfu_srv_model;
extern struct bt_mesh_model* dfu_cli_model;

#endif  // _MESH_H