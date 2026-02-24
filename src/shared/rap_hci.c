// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2012-2014  Intel Corporation. All rights reserved.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>


#include "src/shared/mainloop.h"
#include "src/shared/io.h"
#include "src/log.h"
#include "src/btd.h"
#include "src/shared/util.h"
#include "src/shared/queue.h"
#include "src/shared/hci.h"
#include "src/shared/rap_hci.h"
void handle_hci_le_meta_evts(uint8_t status, uint16_t length, const void *param, void *user_data);

// CS State Definitions
enum cs_state_t {
	CS_INIT,
	CS_STOPPED,
	CS_STARTED,
	CS_WAIT_SEC_CMPLT,
	CS_WAIT_CONFIG_CMPLT,
	CS_WAIT_PROC_CMPLT,
	CS_HOLD,
	CS_UNSPECIFIED
};

// State Machine Context
struct cs_state_machine_t {
	enum cs_state_t current_state;
	struct bt_hci *hci;
	bool initiator;
	bool procedure_active;
};

// State Machine Functions
void cs_state_machine_init(struct cs_state_machine_t *sm, struct bt_hci *hci)
{
	if (!sm) return;

	memset(sm, 0, sizeof(struct cs_state_machine_t));
	sm->current_state = CS_UNSPECIFIED;
	sm->hci = hci;
	sm->initiator = false;
	sm->procedure_active = false;
	DBG("cs_state_machine_init");
}

// State Transition Logic
void cs_set_state(struct cs_state_machine_t *sm, enum cs_state_t new_state)
{
	if (!sm) {
		error("Error: State machine not initialized\n");
		return;
	}

	// Don't trigger callback if state hasn't changed
	if (sm->current_state == new_state) {
		return;
	}

	const char *state_names[] = {
		"CS_INIT", "CS_STOPPED", "CS_STARTED", "CS_WAIT_SEC_CMPLT",
		"CS_WAIT_CONFIG_CMPLT", "CS_WAIT_PROC_CMPLT", "CS_HOLD", "CS_UNSPECIFIED"
	};

	DBG("[STATE] Transition: %s → %s\n", state_names[sm->current_state],
			state_names[new_state]);
	// Update state and trigger callback
	enum cs_state_t old_state = sm->current_state;
	sm->current_state = new_state;

	// Special handling for transition from old state
	if (old_state == CS_STARTED && new_state != CS_STARTED) {
		// Unregister subevent result handlers when leaving CS_STARTED
		DBG("[HCI] Unregistering subevent result handlers\n");
	}
}

enum cs_state_t cs_get_current_state(const struct cs_state_machine_t *sm)
{
	return sm ? sm->current_state : CS_UNSPECIFIED;
}

bool cs_is_procedure_active(const struct cs_state_machine_t *sm)
{
	return sm ? sm->procedure_active : false;
}

// HCI Event Callbacks
static void rap_def_settings_done_cb (const void *data, uint8_t size,
							void *user_data)
{
	cs_state_machine_t *sm = (cs_state_machine_t*)user_data;
	DBG("[EVENT] CS default Setting Complete (size=0x%02X)\n", size);
	
	 if (status == 0) {
		// Success - proceed to configuration
		cs_set_state(sm, CS_WAIT_SEC_CMPLT);
		
		//Reflector role
		DBG("Waiting for Security complete event...\n");
		//Initiator role - Send CS Sec cmd
	} else {
		// Error - transition to stopped
		error("[ERROR] CS Set default setting failed with status 0x%02X\n", status);
		cs_set_state(sm, CS_STOPPED);
	}
}
void rap_send_hci_def_settings_command(struct bt_hci *hci,
						struct hci_evt_le_cs_read_rmt_supp_cap_complete *ev)
{
	struct hci_cp_le_cs_set_default_settings cp;
	struct btd_opts btd_opts;
	unsigned int status;
	DBG("rap_send_hci_def_settings_command");
	memset(&cp, 0, sizeof(cp));

	if(ev->handle)
		cp.handle = ev->handle;
	cp.role_enable = btd_opts.bcs.role;
	cp.cs_sync_ant_sel = btd_opts.bcs.cs_sync_ant_sel;
	cp.max_tx_power = btd_opts.bcs.max_tx_power;

	DBG("sending set default settings case");
	status =  bt_hci_send(hci, HCI_OP_LE_CS_SET_DEFAULT_SETTINGS, &cp, sizeof(cp),
					rap_def_settings_done_cb, NULL, NULL);
	DBG("sending set default settings case, status : %d", status);
	if (!status)
		error("Failed to send default settings cmd");

	return ;
}

static void rap_rd_rmt_supp_cap_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_read_rmt_supp_cap_complete *ev =
			(struct hci_evt_le_cs_read_rmt_supp_cap_complete*)data;

	DBG("[EVENT] Remote Capabilities Complete (status=0x%02X)\n", ev->status);

	if (ev->status == 0) {
		rap_send_hci_def_settings_command(sm->hci, ev);
		cs_set_state(sm, CS_INIT);
	} else {
		// Error - transition to stopped
		error("[ERROR] Remote capabilities failed with status 0x%02X\n", ev->status);
		cs_set_state(sm, CS_STOPPED);
	}
	//Send to rap
}

static void rap_cs_sec_enable_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	
	DBG("[EVENT] Security Enable Complete (size=0x%02X)\n", size);
	
	if (status == 0) {
		// Success - proceed to configuration
		cs_set_state(sm, CS_WAIT_CONFIG_CMPLT);
		
		//Reflector role
		DBG("Waiting for Config complete event...\n");
		//Initiator role - Send CS Config cmd
	} else {
		// Error - transition to stopped
		error("[ERROR] Security enable failed with status 0x%02X\n", status);
		cs_set_state(sm, CS_STOPPED);
	}
	//Send to rap
}

static void rap_cs_config_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	
	DBG("[EVENT] Configuration Complete (size=0x%02X)\n", size);
	
	if (status == 0) {
		// Success - proceed to procedure enable
		cs_set_state(sm, CS_WAIT_PROC_CMPLT);
		
		// Reflector role
		DBG("Waiting for procedure enable event...\n");
		//Initiator role - Send CS Procedure Parameters and CS proc enable cmds
	} else {
		// Error - transition to stopped
		error("[ERROR] Configuration failed with status 0x%02X\n", status);
		cs_set_state(sm, CS_STOPPED);
	}
	//Send to rap
}

static void rap_cs_proc_enable_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	
	DBG("[EVENT] Procedure Enable Complete (size=0x%02X)\n", size);
	
	if (status == 0) {
		// Success - procedure started
		cs_set_state(sm, CS_STARTED);
	} else {
		// Error - transition to stopped
		error("[ERROR] Procedure enable failed with status 0x%02X\n", status);
		cs_set_state(sm, CS_STOPPED);
	}
	//Send to rap
}

static void rap_cs_subevt_result_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	
	DBG("[EVENT] Subevent Result (length=%u)\n", size);
	
	if (status != 0) {
		error("[WARNING] Subevent result with error status 0x%02X\n", status);
		//check what to do
		if (status == 0x0F) { 
			cs_set_state(sm, CS_STOPPED);
		}
		return;
	}

	//send to rap
}

static void rap_cs_subevt_result_cont_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	
	DBG("[EVENT] Subevent Result Continue (length=%u)\n", size);
	
   if (status != 0) {
		error("[WARNING] Subevent result continue with error status 0x%02X\n", status);
		return;
	}

	//send to rap
}


// HCI Event Registration
static void rap_handle_hci_events(const void *data, uint8_t size, void *user_data)
{
	const uint8_t *p = data; 
	const char *opcode_name = "UNKNOWN";

	DBG("-- rap_handle_hci_events --");

	/* At least the LE subevent byte must be present */
	if (size < 1) {
		error("[HCI] LE Meta: malformed event (size=%u)\n", size);
		return;
	}

	const uint8_t subevent = p[0];
	const uint8_t *payload = p + 1;
	const uint8_t payload_len = (uint8_t)(size - 1);

	switch(subevent) {
		case HCI_EVT_LE_CS_READ_RMT_SUPP_CAP_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_READ_RMT_SUPP_CAP_COMPLETE";
			rap_rd_rmt_supp_cap_cmplt_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_SECURITY_ENABLE_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_SECURITY_ENABLE_COMPLETE";
			rap_cs_sec_enable_cmplt_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_CONFIG_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_CONFIG_COMPLETE";
			rap_cs_config_cmplt_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_PROCEDURE_ENABLE_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_PROCEDURE_ENABLE_COMPLETE";
			rap_cs_proc_enable_cmplt_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_SUBEVENT_RESULT:
			opcode_name = "HCI_EVT_LE_CS_SUBEVENT_RESULT";
			rap_cs_subevt_result_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_SUBEVENT_RESULT_CONTINUE:
			opcode_name = "HCI_EVT_LE_CS_SUBEVENT_RESULT_CONTINUE";
			rap_cs_subevt_result_cont_evt(payload, payload_len, user_data);
			break;
		default :
			DBG("-- rap_handle_hci_events, default case --");
			break;
	}

	DBG("[HCI] Received event: %s (0x%04X)\n", opcode_name, subevent);
}

void bt_rap_register_hci_events(struct bt_hci *hci)
{
	DBG("ras_register_hci_events");
	struct cs_state_machine_t* sm = (struct cs_state_machine_t*) malloc(sizeof(struct cs_state_machine_t));
	if (!sm || !hci)
	{ 
		error("Error: Invalid Parameters or failed to allocate state machine\n");
		return -1;
	} 

	cs_state_machine_init(sm, hci);
	DBG("--ras_register_hci_events--");
	uint8_t event_id = bt_hci_register(hci, BT_HCI_EVT_LE_META_EVENT, rap_handle_hci_events, sm, NULL);
	DBG("bt_hci_register done, event_id : %d", event_id);

	if (!event_id) {
		error("Error: Failed to register hci le meta events (event_id=0x%02X)\n", event_id);
		return;
	}
	DBG("ras_register_hci_events done");
}