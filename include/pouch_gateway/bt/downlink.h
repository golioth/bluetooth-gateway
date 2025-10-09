/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

struct pouch_gateway_downlink_context *pouch_gateway_downlink_start(struct bt_conn *conn);
void pouch_gateway_downlink_cleanup(struct bt_conn *conn);
