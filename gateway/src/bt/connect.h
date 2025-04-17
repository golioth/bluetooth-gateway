/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/conn.h>

#include "types.h"

struct golioth_node_info *get_node_info(const struct bt_conn *conn);
