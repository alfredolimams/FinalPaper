#ifndef _BT_CALLBACKS_H
#define _BT_CALLBACKS_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <logging/log.h>
#include <misc/printk.h>
#include <settings/settings.h>

#include "image.h"
#include "mesh_dfu.h"
#include "mesh_requests.h"

void dfu_get_property(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                      struct net_buf_simple *buf);

void dfu_get_data(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                  struct net_buf_simple *buf);

void dfu_status_property(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                         struct net_buf_simple *buf);

void dfu_status_data(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
                     struct net_buf_simple *buf);

#endif  // _BT_CALLBACKS_H