/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include "src/adapter.h"
#include "monitor/bt.h"

#define HCI_OP_CMD_NONE					0x0000

#define MAX_NO_STEPS 160
#define MAX_STEP_DATA_LEN 255

#define MAKE_LE_EVT(subevt)   ( ((uint16_t)BT_HCI_EVT_LE_META_EVENT << 8) | (uint16_t)(subevt) )

/* void rap_wait_for_hci_init_event(struct btd_adapter *adapter); */
void bt_rap_register_hci_events(struct bt_hci *hci);