/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

struct downlink_context *gateway_downlink_start(struct bt_conn *conn);
void gateway_downlink_cleanup(struct bt_conn *conn);
