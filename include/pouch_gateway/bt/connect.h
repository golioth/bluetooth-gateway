/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <zephyr/bluetooth/conn.h>

#include <pouch_gateway/types.h>

struct pouch_gateway_node_info *pouch_gateway_get_node_info(const struct bt_conn *conn);

void pouch_gateway_bt_finished(struct bt_conn *conn);
