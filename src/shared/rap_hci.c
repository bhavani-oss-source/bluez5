// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <endian.h>

#include "src/shared/rap_hci.h"
// ======================
// CS State Definitions
// ======================
enum cs_state_t {
	CS_INIT,
	CS_STOPPED,
	CS_STARTED,
	CS_WAIT_CONFIG_CMPLT,
	CS_WAIT_SEC_CMPLT,
	CS_WAIT_PROC_CMPLT,
	CS_HOLD,
	CS_UNSPECIFIED
};
const char *state_names[] = {
	"CS_INIT",
	"CS_STOPPED",
	"CS_STARTED",
	"CS_WAIT_CONFIG_CMPLT",
	"CS_WAIT_SEC_CMPLT",
	"CS_WAIT_PROC_CMPLT",
	"CS_HOLD",
	"CS_UNSPECIFIED"
};
// ======================
// Callback Function Type
// ======================
typedef void (*cs_callback_t)(uint16_t index, uint16_t length, const void *param, void *user_data);

// ======================
// State Machine Context
// ======================
struct cs_state_machine_t {
	enum cs_state_t current_state;
	enum cs_state_t old_state;
	struct bt_hci *hci;
	struct bt_rap *rap;
	struct rap_utils *utils;
	bool initiator;
	bool procedure_active;
};

struct cs_callback_map_t {
	int state;
	cs_callback_t callback;
};

struct cs_callback_map_t cs_callback_map[] = {
	{ CS_WAIT_CONFIG_CMPLT,	cs_config_complete_callback		},
	{ CS_WAIT_SEC_CMPLT,	  cs_sec_enable_complete_callback	},
	{ CS_WAIT_PROC_CMPLT,	 cs_procedure_enable_complete_callback },
	{ CS_STARTED,			 cs_subevent_result_callback		}
};
#define CS_CALLBACK_MAP_SIZE (sizeof(cs_callback_map) / sizeof(cs_callback_map[0]))

// ======================
// State Machine Functions
// ======================
void cs_state_machine_init(struct cs_state_machine_t *sm, struct bt_rap *rap, struct bt_hci *hci, struct rap_utils *utils)
{
	if (!sm) return;

	memset(sm, 0, sizeof(struct cs_state_machine_t));
	sm->current_state = CS_UNSPECIFIED;
	sm->rap = rap;
	sm->hci = hci;
	sm->initiator = false;
	sm->procedure_active = false;
	sm->utils = utils;
}

// ======================
// State Transition Logic
// ======================
void cs_set_state(struct cs_state_machine_t *sm, enum cs_state_t new_state)
{
	if (!sm) {
		RAP_DBG(sm->utils,"Error: State machine not initialized\n");
		return;
	}

	if (sm->current_state == new_state) {
		return;
	}

	RAP_DBG(sm->utils, "[STATE] Transition: %s → %s\n", state_names[sm->current_state], 
			state_names[new_state]);

	sm->old_state = sm->current_state;
	sm->current_state = new_state;
}

enum cs_state_t cs_get_current_state(struct cs_state_machine_t *sm)
{
	return sm ? sm->current_state : CS_UNSPECIFIED;
}

bool cs_is_procedure_active(const struct cs_state_machine_t *sm)
{
	return sm ? sm->procedure_active : false;
}
// ======================
// HCI Event Callbacks
// ======================
static void rap_def_settings_done_cb (const void *data, uint8_t size,
							void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	RAP_DBG(sm->utils,"[EVENT] CS default Setting Complete (size=0x%02X)\n", size);

	struct hci_rp_le_cs_set_default_settings *rp =
			(struct hci_rp_le_cs_set_default_settings *)data;

	if(CS_INIT != cs_get_current_state(sm)) {
		RAP_DBG(sm->utils,"Event received in Wrong State!! Expected : CS_INIT");
		return;
	}

	 if (rp->status == 0) {
		// Success - proceed to configuration
		cs_set_state(sm, CS_WAIT_CONFIG_CMPLT);

		//Reflector role
		RAP_DBG(sm->utils,"Waiting for CS Config Completed event...\n");
		//ToDO : Initiator role - Send CS Config complete cmd
	} else {
		// Error - transition to stopped
		RAP_DBG(sm->utils,"[ERROR] CS Set default setting failed with status 0x%02X\n", rp->status);
		cs_set_state(sm, CS_STOPPED);
	}
}
void rap_send_hci_def_settings_command(struct cs_state_machine_t *sm,
						struct hci_evt_le_cs_read_rmt_supp_cap_complete *ev)
{
	struct hci_cp_le_cs_set_default_settings cp;
	unsigned int status;
	memset(&cp, 0, sizeof(cp));

	if(ev->handle)
		cp.handle = ev->handle;
	cp.role_enable = cs_opt.role;
	cp.cs_sync_ant_sel = cs_opt.cs_sync_ant_sel;
	cp.max_tx_power = cs_opt.max_tx_power;

	if(!sm || !sm->hci) {
		RAP_DBG(sm->utils,"[ERR] Set Def Settings: sm or hci is null");
		return;
	}
	status =  bt_hci_send(sm->hci, HCI_OP_LE_CS_SET_DEFAULT_SETTINGS, &cp, sizeof(cp),
					rap_def_settings_done_cb, sm, NULL);
	RAP_DBG(sm->utils,"sending set default settings case, status : %d", status);
	if (!status)
		RAP_DBG(sm->utils,"Failed to send default settings cmd");

	return ;
}

static void rap_rd_rmt_supp_cap_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_read_rmt_supp_cap_complete *ev =
			(struct hci_evt_le_cs_read_rmt_supp_cap_complete*)data;

	RAP_DBG(sm->utils,"[EVENT] Remote Capabilities Complete (status=0x%02X)\n", ev->status);

	if (ev->status == 0) {
		rap_send_hci_def_settings_command(sm, ev);
		cs_set_state(sm, CS_INIT);
	} else {
		// Error - transition to stopped
		RAP_DBG(sm->utils,"[ERROR] Remote capabilities failed with status 0x%02X\n", ev->status);
		cs_set_state(sm, CS_STOPPED);
	}
}

static void rap_cs_config_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_config_complete *event =
		(struct hci_evt_le_cs_config_complete *)data;
	struct mgmt_ev_cs_config_cmplt ev;

	RAP_DBG(sm->utils,"[EVENT] Configuration Complete (size=0x%02X)\n", size);

	/* State Check */
	if(CS_WAIT_CONFIG_CMPLT != cs_get_current_state(sm)) {
		RAP_DBG(sm->utils,"Event received in Wrong State!! Expected : CS_WAIT_CONFIG_CMPLT");
		return;
	}

	if (event->status == 0) {
		// Success - proceed to Security enable complete
		cs_set_state(sm, CS_WAIT_SEC_CMPLT);

		// Reflector role
		RAP_DBG(sm->utils,"Waiting for security enable event...\n");
		//ToDO : Initiator role - Send CS Security enable cmd
	} else {
		// Error - transition to stopped
		RAP_DBG(sm->utils,"[ERROR] Configuration failed with status 0x%02X\n", event->status);
		cs_set_state(sm, CS_STOPPED);
	}

	/* Copy from HCI to Mgmt structure */
	ev.status = event->status;
	ev.conn_hdl = event->handle;
	ev.config_id = event->config_id;
	ev.action = event->action;
	ev.main_mode_type = event->main_mode_type;
	ev.sub_mode_type = event->sub_mode_type;
	ev.min_main_mode_steps = event->min_main_mode_steps;
	ev.max_main_mode_steps = event->max_main_mode_steps;
	ev.main_mode_rep = event->main_mode_rep;
	ev.mode_0_steps = event->mode_0_steps;
	ev.role = event->role;
	ev.rtt_type = event->rtt_type;
	cs_opt.rtt_type = event->rtt_type;
	ev.cs_sync_phy = event->cs_sync_phy;
	memcpy(ev.channel_map, event->channel_map, 10);
	ev.channel_map_rep = event->channel_map_rep;
	ev.channel_sel_type = event->channel_sel_type;
	ev.ch3c_shape = event->ch3c_shape;
	ev.ch3c_jump = event->ch3c_jump;
	ev.reserved = event->reserved;
	ev.t_ip1_time = event->t_ip1_time;
	ev.t_ip2_time = event->t_ip2_time;
	ev.t_fcs_time = event->t_fcs_time;
	ev.t_pm_time = event->t_pm_time;

	/* Send Callback to RAP Profile */
	for (size_t i = 0; i < CS_CALLBACK_MAP_SIZE; i++) {
		if (cs_callback_map[i].state == sm->old_state) {
			cs_callback_map[i].callback(0, size, &ev, sm->rap);
			return;
		}
	}
	RAP_DBG(sm->utils,"***rap_cs_config_cmplt_evt done***");
}

static void rap_cs_sec_enable_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_security_enable_complete *event =
		(struct hci_evt_le_cs_security_enable_complete *)data;
	struct mgmt_ev_cs_sec_enable_cmplt ev;

	RAP_DBG(sm->utils,"[EVENT] Security Enable Complete (size=0x%02X)\n", size);

	/* State Check */
	if(CS_WAIT_SEC_CMPLT != cs_get_current_state(sm)) {
		RAP_DBG(sm->utils,"Event received in Wrong State!! Expected : CS_WAIT_SEC_CMPLT");
		return;
	}

	if (event->status == 0) {
		// Success - proceed to configuration
		cs_set_state(sm, CS_WAIT_PROC_CMPLT);

		//Reflector role
		RAP_DBG(sm->utils,"Waiting for CS Proc complete event...\n");
		//ToDO : Initiator role - Send CS Proc Set Parameter and CS Proc enable cmd
	} else {
		// Error - transition to stopped
		RAP_DBG(sm->utils,"[ERROR] Security enable failed with status 0x%02X\n", event->status);
		cs_set_state(sm, CS_STOPPED);
	}

	/* Copy from HCI to Mgmt structure */
	ev.status = event->status;
	ev.conn_hdl = cpu_to_le16(event->handle);

	/* Send Callback to RAP Profile */
	for (size_t i = 0; i < CS_CALLBACK_MAP_SIZE; i++) {
		if (cs_callback_map[i].state == sm->old_state) {
			cs_callback_map[i].callback(0, size, &ev, sm->rap);
			return;
		}
	}
}

static void rap_cs_proc_enable_cmplt_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_procedure_enable_complete *event =
		(struct hci_evt_le_cs_procedure_enable_complete *) data;
	struct mgmt_ev_cs_proc_enable_cmplt ev;

	RAP_DBG(sm->utils,"[EVENT] Procedure Enable Complete (size=0x%02X)\n", size);

	/* State Check */
	if(CS_WAIT_PROC_CMPLT != cs_get_current_state(sm)) {
		RAP_DBG(sm->utils,"Event received in Wrong State!! Expected : CS_WAIT_PROC_CMPLT");
		return;
	}

	if (event->status == 0) {
		// Success - procedure started
		cs_set_state(sm, CS_STARTED);
		sm->procedure_active = true;
	} else {
		// Error - transition to stopped
		RAP_DBG(sm->utils,"[ERROR] Procedure enable failed with status 0x%02X\n", event->status);
		cs_set_state(sm, CS_STOPPED);
		sm->procedure_active = false;
	}

	/* Copy from HCI to Mgmt structure */
	ev.status = event->status;
	ev.conn_hdl = event->handle;
	ev.config_id = event->config_id;
	ev.state = event->state;
	ev.tone_ant_config_sel = event->tone_ant_config_sel;
	ev.sel_tx_pwr = event->sel_tx_pwr;
	memcpy(ev.sub_evt_len, event->sub_evt_len, 3);
	ev.sub_evts_per_evt = event->sub_evts_per_evt;
	ev.sub_evt_intrvl = event->sub_evt_intrvl;
	ev.evt_intrvl = event->evt_intrvl;
	ev.proc_intrvl = event->proc_intrvl;
	ev.proc_counter = event->proc_counter;
	ev.max_proc_len = event->max_proc_len;

	/* Send Callback to RAP Profile */
	for (size_t i = 0; i < CS_CALLBACK_MAP_SIZE; i++) {
		if (cs_callback_map[i].state == sm->old_state) {
			cs_callback_map[i].callback(0, size, &ev, sm->rap);
			return;
		}
	}
}

static void parse_i_q_sample(const uint8_t **buf, int16_t *i_sample, int16_t *q_sample)
{
	const uint8_t *p = *buf;
	uint32_t buffer = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
	uint32_t i12 =  buffer        & 0x0FFFU;   // bits 0..11
	uint32_t q12 = (buffer >> 12) & 0x0FFFU;   // bits 12..23
	*i_sample = (int16_t)((int32_t)(i12 << 20) >> 20);
	*q_sample = (int16_t)((int32_t)(q12 << 20) >> 20);
	*buf += 3;
}

static __always_inline bool consume(const uint8_t **pp, size_t *rem, size_t need)
{
	if (*rem < need)
		return false;
	*pp  += need;
	*rem -= need;
	return true;
}

static inline size_t min_size(size_t a, size_t b)
{
	return a < b ? a : b;
}
static void rap_cs_subevt_result_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct hci_evt_le_cs_subevent_result *event =
		(struct hci_evt_le_cs_subevent_result *) data;


	/* Check if Procedure is active or not */
	if(sm->procedure_active == false) {
		RAP_DBG(sm->utils,"Received CS Subevent Result event when CS Procedure is inactive!!\n Discarding the event");
		return;
	}

	const uint8_t cs_role = cs_opt.role;
	const uint8_t cs_rtt_type = cs_opt.rtt_type;
	const uint8_t max_paths = min_size((event->num_ant_paths + 1), CS_MAX_ANT_PATHS);
	uint8_t steps = min_size(event->num_steps_reported, CS_MAX_STEPS);
	struct mgmt_ev_cs_subevent_result *ev;
	size_t send_len = 0;
	size_t needed = offsetof(struct mgmt_ev_cs_subevent_result, step_data) +
					steps * sizeof(struct cs_step_data);
	ev = (struct mgmt_ev_cs_subevent_result*) malloc(needed);
	RAP_DBG(sm->utils,"[EVENT] Subevent Result (length=%u)\n", size);
	if (!ev)
		return;
	ev->conn_hdl                     = le16_to_cpu(event->handle);
	ev->config_id                    = event->config_id;
	ev->start_acl_conn_evt_counter   = event->start_acl_conn_evt_counter;
	ev->proc_counter                 = event->proc_counter;
	ev->freq_comp                    = event->freq_comp;
	ev->ref_pwr_lvl                  = event->ref_pwr_lvl;
	ev->proc_done_status             = event->proc_done_status;
	ev->subevt_done_status           = event->subevt_done_status;
	ev->abort_reason                 = event->abort_reason;
	ev->num_ant_paths                = event->num_ant_paths;
	ev->num_steps_reported           = steps;
	if (event->num_steps_reported > CS_MAX_STEPS) {
		RAP_DBG(sm->utils, "Too many steps reported: %u (max %u)",
			event->num_steps_reported, CS_MAX_STEPS);
		goto send_event;
	}
	/* Early exit for error conditions */
	if ((ev->subevt_done_status == 0xF) || (ev->proc_done_status == 0xF)) {
		RAP_DBG(sm->utils, "CS Procedure/Subevent aborted: sub evt status = %d, \
			proc status = %d, reason = %d",
			ev->subevt_done_status, ev->proc_done_status, ev->abort_reason);
		goto send_event;
	}
	const uint8_t *p   = event->step_data;
	size_t rem    = event->step_data_len;
	RAP_DBG(sm->utils, "event->step_data_len : %d", event->step_data_len);
	for (uint8_t i = 0; i < steps; i++) {
		struct cs_step_data *step = &ev->step_data[i];
		if (!consume(&p, &rem, 3)) {
			RAP_DBG(sm->utils, "Truncated header for step %u", i);
			break;
		}

		const uint8_t mode   = p[-3];
		const uint8_t chnl   = p[-2];
		const uint8_t length = p[-1];
		step->step_mode        = mode;
		step->step_chnl        = chnl;
		step->step_data_length = min_size(length, CS_MAX_STEP_DATA_LEN);
		RAP_DBG(sm->utils, "Step %u: mode=%u chnl=%u data_len=%u", i, mode,
				chnl, length);
		send_len += (step->step_data_length + 3);
		RAP_DBG(sm->utils,"Step %u: mode=%u chnl=%u rem_len=%zu",
				i, step->step_mode, step->step_chnl, rem);
		/* Validate per-step payload size */
		if (length > CS_MAX_STEP_DATA_LEN) {
			RAP_DBG(sm->utils, "Step %u payload %u exceeds max %u",
				i, length, CS_MAX_STEP_DATA_LEN);
			/* Continue parsing but clamp to max to avoid overruns */
		}
		if (rem < length) {
			RAP_DBG(sm->utils, "Truncated payload for step %u (need %u, have %zu)",
				i, length, rem);
			break;
	}

		switch (mode) {
		case CS_MODE_ZERO:
			if (!consume(&p, &rem, 3)) {
				RAP_DBG(sm->utils, "Mode 0: too short (<3)");
				break;
			}
			step->step_mode_data.mode_zero_data.packet_quality   = p[-3];
			step->step_mode_data.mode_zero_data.packet_rssi_dbm  = p[-2];
			step->step_mode_data.mode_zero_data.packet_ant       = p[-1];
			RAP_DBG(sm->utils, "CS Step mode 0");
			if (cs_role == CS_INITIATOR && rem >= 4) {
				step->step_mode_data.mode_zero_data.init_measured_freq_offset =
					bt_get_le32(p);
				consume(&p, &rem, 4);
			}
			break;
		case CS_MODE_ONE:
			if (!consume(&p, &rem, 5)) {
				RAP_DBG(sm->utils, "Mode 1: too short (<5)");
				break;
			}
			RAP_DBG(sm->utils, "CS Step mode 1");
			step->step_mode_data.mode_one_data.packet_quality  = p[-5];
			step->step_mode_data.mode_one_data.packet_rssi_dbm = p[-4];
			step->step_mode_data.mode_one_data.packet_ant      = p[-3];
			step->step_mode_data.mode_one_data.packet_nadm     = p[-2];
			RAP_DBG(sm->utils, "CS Step mode 1", max_paths);
			if (rem >= 2) {
				if (cs_role == CS_REFLECTOR)
					step->step_mode_data.mode_one_data.tod_toa_refl =
						bt_get_le16(p);
				else
					step->step_mode_data.mode_one_data.toa_tod_init =
						bt_get_le16(p);
				consume(&p, &rem, 2);
			}
			if ((cs_rtt_type == 0x01 || cs_rtt_type == 0x02) && rem >= 8) {
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_one_data.packet_pct1.i_sample,
					&step->step_mode_data.mode_one_data.packet_pct1.q_sample);
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_one_data.packet_pct2.i_sample,
					&step->step_mode_data.mode_one_data.packet_pct2.q_sample);
				if (rem >= 1)
					consume(&p, &rem, 1);
			}
			break;
		case CS_MODE_TWO: {
			if (!consume(&p, &rem, 1)) {
				RAP_DBG(sm->utils, "Mode 2: too short (<1)");
				break;
			}
			step->step_mode_data.mode_two_data.ant_perm_index = p[-1];
			RAP_DBG(sm->utils, "CS Step mode 2, max paths : %d", max_paths);
			for (uint8_t k = 0; k < max_paths; k++) {
				if (rem < 4) {
					RAP_DBG(sm->utils, "Mode 2: insufficient PCT for path %u (rem=%zu)",
						k, rem);
					break;
				}
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_two_data.tone_pct[k].i_sample,
					&step->step_mode_data.mode_two_data.tone_pct[k].q_sample);
				if (!consume(&p, &rem, 1)) {
					RAP_DBG(sm->utils, "Mode 2: missing quality indicator for path %u",
						k);
					break;
				}
				step->step_mode_data.mode_two_data.tone_quality_indicator[k] = p[-1];
				RAP_DBG(sm->utils, "tone_quality_indicator : %d",
					step->step_mode_data.mode_two_data.tone_quality_indicator[k]);
				RAP_DBG(sm->utils, "[i, q] : %d, %d",
					step->step_mode_data.mode_two_data.tone_pct[k].i_sample,
					step->step_mode_data.mode_two_data.tone_pct[k].q_sample);
			}
			break;
		}

		case CS_MODE_THREE: {
			struct cs_mode_one_data *mode_one =
				&step->step_mode_data.mode_three_data.mode_one_data;
			struct cs_mode_two_data *mode_two =
				&step->step_mode_data.mode_three_data.mode_two_data;
			if (!consume(&p, &rem, 4)) {
				RAP_DBG(sm->utils, "Mode 3: mode1 too short (<4)");
				break;
			}
			RAP_DBG(sm->utils, "CS Step mode 3");
			mode_one->packet_quality  = p[-4];
			mode_one->packet_rssi_dbm = p[-3];
			mode_one->packet_ant      = p[-2];
			mode_one->packet_nadm     = p[-1];
			if (rem >= 2) {
				if (cs_role == CS_REFLECTOR)
					mode_one->tod_toa_refl = bt_get_le16(p);
				else
					mode_one->toa_tod_init = bt_get_le16(p);
				consume(&p, &rem, 2);
			}
			if ((cs_rtt_type == 0x01 || cs_rtt_type == 0x02) && rem >= 8) {
				parse_i_q_sample(&p,
					&mode_one->packet_pct1.i_sample,
					&mode_one->packet_pct1.q_sample);
				parse_i_q_sample(&p,
					&mode_one->packet_pct2.i_sample,
					&mode_one->packet_pct2.q_sample);
			}
			if (rem >= 1) {
				mode_two->ant_perm_index = *p;
				consume(&p, &rem, 1);
				for (uint8_t k = 0; k < max_paths; k++) {
					if (rem < 5)
						break;
					parse_i_q_sample(&p,
						&mode_two->tone_pct[k].i_sample,
						&mode_two->tone_pct[k].q_sample);
					consume(&p, &rem, 1);
					mode_two->tone_quality_indicator[k] = p[-1];
				}
			}
			break;
		}
		default:
			RAP_DBG(sm->utils, "Unknown step mode %d for step %d",
				ev->step_data[i].step_mode, i);
			/* Skip the entire step data */
			size_t leftover = length - step->step_data_length;
			if (!consume(&p, &rem, leftover)) {
				RAP_DBG(sm->utils, "Step %u leftover truncate (%zu)", i, leftover);
				break;
			}
			break;
		}
	}
	send_len += offsetof(struct mgmt_ev_cs_subevent_result, step_data);
send_event:
	RAP_DBG(sm->utils, "MGMT CS subevent result processed: %zu bytes, needed: %zu bytes",
	send_len, needed);
	cs_subevent_result_callback(0, send_len, ev, sm->rap);
	free(ev);
}

static void rap_cs_subevt_result_cont_evt(const uint8_t *data, uint8_t size, void *user_data)
{
	struct cs_state_machine_t *sm = (struct cs_state_machine_t*)user_data;
	struct mgmt_ev_cs_subevent_result_cont *ev;
	struct hci_evt_le_cs_subevent_result_continue *event =
		(struct hci_evt_le_cs_subevent_result_continue *) data;

	RAP_DBG(sm->utils,"[EVENT] Subevent Result Continue (length=%u)\n", size);
	const uint8_t cs_role = cs_opt.role;
	const uint8_t cs_rtt_type = cs_opt.rtt_type;
	const uint8_t max_paths = min_size ((event->num_ant_paths + 1), CS_MAX_ANT_PATHS);
	uint8_t steps = min_size(event->num_steps_reported, CS_MAX_STEPS);
	size_t send_len = 0;

	size_t needed = offsetof(struct mgmt_ev_cs_subevent_result_cont, step_data) +
					steps * sizeof(struct cs_step_data);
	ev = (struct mgmt_ev_cs_subevent_result_cont*) malloc(needed);
	 if (!ev)
		return;
	ev->conn_hdl = le16_to_cpu(event->handle);
	ev->config_id = event->config_id;
	ev->proc_done_status = event->proc_done_status;
	ev->subevt_done_status = event->subevt_done_status;
	ev->abort_reason = event->abort_reason;
	ev->num_ant_paths = event->num_ant_paths;
	ev->num_steps_reported = steps;
	if (event->num_steps_reported > CS_MAX_STEPS) {
		RAP_DBG(sm->utils, "Too many steps reported: %u (max %u)",
			event->num_steps_reported, CS_MAX_STEPS);
		goto send_event;
	}

	/* Early exit for error conditions */
	if ((ev->subevt_done_status == 0xF) || (ev->proc_done_status == 0xF)) {
		RAP_DBG(sm->utils, "CS Procedure/Subevent aborted: sub evt status = %d, \
			proc status = %d, reason = %d",
			ev->subevt_done_status, ev->proc_done_status, ev->abort_reason);
		goto send_event;
	}
	const uint8_t *p   = event->step_data;
	size_t rem    = event->step_data_len;
	RAP_DBG(sm->utils, "event->step_data_len : %d", event->step_data_len);
	for (uint8_t i = 0; i < steps; i++) {
		struct cs_step_data *step = &ev->step_data[i];
		if (!consume(&p, &rem, 3)) {
			RAP_DBG(sm->utils, "Truncated header for step %u", i);
			break;
		}
		const uint8_t mode   = p[-3];
		const uint8_t chnl   = p[-2];
		const uint8_t length = p[-1];
		step->step_mode        = mode;
		step->step_chnl        = chnl;
		step->step_data_length = min_size(length, CS_MAX_STEP_DATA_LEN);
		RAP_DBG(sm->utils, "Step %u: mode=%u chnl=%u data_len=%u", i, mode,
				chnl, length);
		send_len += (step->step_data_length + 3);
		RAP_DBG(sm->utils, "Step %u: mode=%u chnl=%u rem_len=%zu",
				i, step->step_mode, step->step_chnl, rem);
		/* Validate per-step payload size */
		if (length > CS_MAX_STEP_DATA_LEN) {
			RAP_DBG(sm->utils, "Step %u payload %u exceeds max %u",
				i, length, CS_MAX_STEP_DATA_LEN);
			/* Continue parsing but clamp to max to avoid overruns */
		}
		if (rem < length) {
			RAP_DBG(sm->utils, "Truncated payload for step %u (need %u, have %zu)",
				i, length, rem);
			break;
		}
		 switch (mode) {
		case CS_MODE_ZERO:
			if (!consume(&p, &rem, 3)) {
				RAP_DBG(sm->utils, "Mode 0: too short (<3)");
				break;
			}
			step->step_mode_data.mode_zero_data.packet_quality   = p[-3];
			step->step_mode_data.mode_zero_data.packet_rssi_dbm  = p[-2];
			step->step_mode_data.mode_zero_data.packet_ant       = p[-1];
			RAP_DBG(sm->utils, "CS Step mode 0");
			if (cs_role == CS_INITIATOR && rem >= 4) {
				step->step_mode_data.mode_zero_data.init_measured_freq_offset =
					bt_get_le32(p);
				consume(&p, &rem, 4);
			}
			break;
		case CS_MODE_ONE:
			if (!consume(&p, &rem, 5)) {
				RAP_DBG(sm->utils, "Mode 1: too short (<5)");
				break;
			}
			RAP_DBG(sm->utils, "CS Step mode 1");
			step->step_mode_data.mode_one_data.packet_quality  = p[-5];
			step->step_mode_data.mode_one_data.packet_rssi_dbm = p[-4];
			step->step_mode_data.mode_one_data.packet_ant      = p[-3];
			step->step_mode_data.mode_one_data.packet_nadm     = p[-2];
			if (rem >= 2) {
				if (cs_role == CS_REFLECTOR)
					step->step_mode_data.mode_one_data.tod_toa_refl =
						bt_get_le16(p);
				else
					step->step_mode_data.mode_one_data.toa_tod_init =
						bt_get_le16(p);
				consume(&p, &rem, 2);
			}
			if ((cs_rtt_type == 0x01 || cs_rtt_type == 0x02) && rem >= 8) {
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_one_data.packet_pct1.i_sample,
					&step->step_mode_data.mode_one_data.packet_pct1.q_sample);
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_one_data.packet_pct2.i_sample,
					&step->step_mode_data.mode_one_data.packet_pct2.q_sample);
				if (rem >= 1)
					consume(&p, &rem, 1);
			}
			break;
		case CS_MODE_TWO: {
			if (!consume(&p, &rem, 1)) {
				RAP_DBG(sm->utils, "Mode 2: too short (<1)");
				break;
			}
			step->step_mode_data.mode_two_data.ant_perm_index = p[-1];
			RAP_DBG(sm->utils, "CS Step mode 2, max paths : %d", max_paths);
			for (uint8_t k = 0; k < max_paths; k++) {
				if (rem < 4) {
					RAP_DBG(sm->utils, "Mode 2: insufficient PCT for path %u (rem=%zu)",
						k, rem);
					break;
				}
				parse_i_q_sample(&p,
					&step->step_mode_data.mode_two_data.tone_pct[k].i_sample,
					&step->step_mode_data.mode_two_data.tone_pct[k].q_sample);
				if (!consume(&p, &rem, 1)) {
					RAP_DBG(sm->utils, "Mode 2: missing quality indicator for path %u",
						k);
					break;
				}
				step->step_mode_data.mode_two_data.tone_quality_indicator[k] = p[-1];
				RAP_DBG(sm->utils, "tone_quality_indicator : %d",
					step->step_mode_data.mode_two_data.tone_quality_indicator[k]);
				RAP_DBG(sm->utils, "[i, q] : %d, %d",
					step->step_mode_data.mode_two_data.tone_pct[k].i_sample,
					step->step_mode_data.mode_two_data.tone_pct[k].q_sample);
			}
			break;
		}

		case CS_MODE_THREE: {
			struct cs_mode_one_data *mode_one =
				&step->step_mode_data.mode_three_data.mode_one_data;
			struct cs_mode_two_data *mode_two =
				&step->step_mode_data.mode_three_data.mode_two_data;
			if (!consume(&p, &rem, 4)) {
				RAP_DBG(sm->utils, "Mode 3: mode1 too short (<4)");
				break;
			}
			RAP_DBG(sm->utils, "CS Step mode 3");
			mode_one->packet_quality  = p[-4];
			mode_one->packet_rssi_dbm = p[-3];
			mode_one->packet_ant      = p[-2];
			mode_one->packet_nadm     = p[-1];
			if (rem >= 2) {
				if (cs_role == CS_REFLECTOR)
					mode_one->tod_toa_refl = bt_get_le16(p);
				else
					mode_one->toa_tod_init = bt_get_le16(p);
				consume(&p, &rem, 2);
			}
			if ((cs_rtt_type == 0x01 || cs_rtt_type == 0x02) && rem >= 8) {
				parse_i_q_sample(&p,
					&mode_one->packet_pct1.i_sample,
					&mode_one->packet_pct1.q_sample);
				parse_i_q_sample(&p,
					&mode_one->packet_pct2.i_sample,
					&mode_one->packet_pct2.q_sample);
			}
			if (rem >= 1) {
				mode_two->ant_perm_index = *p;
				consume(&p, &rem, 1);
				for (uint8_t k = 0; k < max_paths; k++) {
					if (rem < 5)
						break;
					parse_i_q_sample(&p,
						&mode_two->tone_pct[k].i_sample,
						&mode_two->tone_pct[k].q_sample);
					consume(&p, &rem, 1);
					mode_two->tone_quality_indicator[k] = p[-1];
				}
			}
			break;
		}
		default:
			RAP_DBG(sm->utils, "Unknown step mode %d for step %d",
					ev->step_data[i].step_mode, i);
			/* Skip the entire step data */
			size_t leftover = length - step->step_data_length;
			if (!consume(&p, &rem, leftover)) {
				RAP_DBG(sm->utils, "Step %u leftover truncate (%zu)", i, leftover);
				break;
			}
			break;
		}
	}
	send_len += offsetof(struct mgmt_ev_cs_subevent_result_cont, step_data);
send_event:
	RAP_DBG(sm->utils, "MGMT CS subevent result cont processed: %zu bytes, needed: %zu bytes",
	send_len, needed);
	cs_subevent_result_cont_callback(0, send_len, ev, sm->rap);
	free(ev);
}

// ======================
// HCI Event Registration
// ======================

static void rap_handle_hci_events(const void *data, uint8_t size, void *user_data)
{
	const uint8_t *p = data; 
	const char *opcode_name = "UNKNOWN";


	/* At least the LE subevent byte must be present */
	if (size < 1) {
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
		case HCI_EVT_LE_CS_CONFIG_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_CONFIG_COMPLETE";
			rap_cs_config_cmplt_evt(payload, payload_len, user_data);
			break;
		case HCI_EVT_LE_CS_SECURITY_ENABLE_COMPLETE:
			opcode_name = "HCI_EVT_LE_CS_SECURITY_ENABLE_COMPLETE";
			rap_cs_sec_enable_cmplt_evt(payload, payload_len, user_data);
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
			break;
	}
}

void bt_rap_register_hci_events(struct bt_rap *rap, struct bt_hci *hci, struct rap_utils *utils)
{
	if(!rap || !hci) {
		return ;
	}
	struct cs_state_machine_t* sm = (struct cs_state_machine_t*) malloc(sizeof(struct cs_state_machine_t));
	if (!sm) { 
		return ;
	} 

	cs_state_machine_init(sm, rap, hci, utils);
	uint8_t event_id = bt_hci_register(hci, HCI_EVT_LE_META_EVENT,
			rap_handle_hci_events, sm, NULL);
	RAP_DBG(sm->utils,"bt_hci_register done, event_id : %d", event_id);

	if (!event_id) {
		RAP_DBG(sm->utils,"Error: Failed to register hci le meta events (event_id=0x%02X)\n", event_id);
		return;
	}
}