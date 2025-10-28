/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

void gateway_uplink_start(struct bt_conn *conn);
void gateway_uplink_cleanup(struct bt_conn *conn);
