/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

void pouch_gateway_uplink_start(struct bt_conn *conn);
void pouch_gateway_uplink_cleanup(struct bt_conn *conn);
