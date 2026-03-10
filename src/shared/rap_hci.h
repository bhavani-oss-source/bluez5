/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */


#include <stdbool.h>
#include <stdint.h>

#include "src/shared/rap.h"

struct cs_options {
	uint8_t role;
	uint8_t cs_sync_ant_sel;
	int8_t max_tx_power;
	int rtt_type;
};

extern struct cs_options cs_opt;

/* Channel Sounding Commands */
#define HCI_OP_LE_CS_RD_LOCAL_SUPP_CAP	0x2089
struct hci_rp_le_cs_rd_local_supp_cap {
	uint8_t	status;
	uint8_t	num_config_supported;
	uint16_t	max_consecutive_procedures_supported;
	uint8_t	num_antennas_supported;
	uint8_t	max_antenna_paths_supported;
	uint8_t	roles_supported;
	uint8_t	modes_supported;
	uint8_t	rtt_capability;
	uint8_t	rtt_aa_only_n;
	uint8_t	rtt_sounding_n;
	uint8_t	rtt_random_payload_n;
	uint16_t	nadm_sounding_capability;
	uint16_t	nadm_random_capability;
	uint8_t	cs_sync_phys_supported;
	uint16_t	subfeatures_supported;
	uint16_t	t_ip1_times_supported;
	uint16_t	t_ip2_times_supported;
	uint16_t	t_fcs_times_supported;
	uint16_t	t_pm_times_supported;
	uint8_t	t_sw_time_supported;
	uint8_t	tx_snr_capability;
} __attribute__((packed));

#define HCI_OP_LE_CS_RD_RMT_SUPP_CAP		0x208A
struct hci_cp_le_cs_rd_local_supp_cap {
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_WR_CACHED_RMT_SUPP_CAP	0x208B
struct hci_cp_le_cs_wr_cached_rmt_supp_cap {
	uint16_t	handle;
	uint8_t	num_config_supported;
	uint16_t	max_consecutive_procedures_supported;
	uint8_t	num_antennas_supported;
	uint8_t	max_antenna_paths_supported;
	uint8_t	roles_supported;
	uint8_t	modes_supported;
	uint8_t	rtt_capability;
	uint8_t	rtt_aa_only_n;
	uint8_t	rtt_sounding_n;
	uint8_t	rtt_random_payload_n;
	uint16_t	nadm_sounding_capability;
	uint16_t	nadm_random_capability;
	uint8_t	cs_sync_phys_supported;
	uint16_t	subfeatures_supported;
	uint16_t	t_ip1_times_supported;
	uint16_t	t_ip2_times_supported;
	uint16_t	t_fcs_times_supported;
	uint16_t	t_pm_times_supported;
	uint8_t	t_sw_time_supported;
	uint8_t	tx_snr_capability;
} __attribute__((packed));

struct hci_rp_le_cs_wr_cached_rmt_supp_cap {
	uint8_t	status;
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_SEC_ENABLE			0x208C
struct hci_cp_le_cs_sec_enable {
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_SET_DEFAULT_SETTINGS			0x208d
struct hci_cp_le_cs_set_default_settings {
	uint16_t  handle;
	uint8_t    role_enable;
	uint8_t    cs_sync_ant_sel;
	int8_t    max_tx_power;
} __attribute__((packed));

struct hci_rp_le_cs_set_default_settings {
	uint8_t    status;
	uint16_t  handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_RD_RMT_FAE_TABLE		0x208E
struct hci_cp_le_cs_rd_rmt_fae_table {
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_WR_CACHED_RMT_FAE_TABLE	0x208F
struct hci_cp_le_cs_wr_rmt_cached_fae_table {
	uint16_t	handle;
	uint8_t	remote_fae_table[72];
} __attribute__((packed));

struct hci_rp_le_cs_wr_rmt_cached_fae_table {
	uint8_t	status;
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_CREATE_CONFIG		0x2090
struct hci_cp_le_cs_create_config {
	uint16_t	handle;
	uint8_t	config_id;
	uint8_t	create_context;
	uint8_t	main_mode_type;
	uint8_t	sub_mode_type;
	uint8_t	min_main_mode_steps;
	uint8_t	max_main_mode_steps;
	uint8_t	main_mode_repetition;
	uint8_t	mode_0_steps;
	uint8_t	role;
	uint8_t	rtt_type;
	uint8_t	cs_sync_phy;
	uint8_t	channel_map[10];
	uint8_t	channel_map_repetition;
	uint8_t	channel_selection_type;
	uint8_t	ch3c_shape;
	uint8_t	ch3c_jump;
	uint8_t	reserved;
} __attribute__((packed));

#define HCI_OP_LE_CS_REMOVE_CONFIG		0x2091
struct hci_cp_le_cs_remove_config {
	uint16_t	handle;
	uint8_t	config_id;
} __attribute__((packed));

#define HCI_OP_LE_CS_SET_CH_CLASSIFICATION	0x2092
struct hci_cp_le_cs_set_ch_classification {
	uint8_t	ch_classification[10];
} __attribute__((packed));

struct hci_rp_le_cs_set_ch_classification {
	uint8_t    status;
} __attribute__((packed));

#define HCI_OP_LE_CS_SET_PROC_PARAM		0x2093
struct hci_cp_le_cs_set_proc_param {
	uint16_t	handle;
	uint8_t	config_id;
	uint16_t	max_procedure_len;
	uint16_t	min_procedure_interval;
	uint16_t	max_procedure_interval;
	uint16_t	max_procedure_count;
	uint8_t	min_subevent_len[3];
	uint8_t	max_subevent_len[3];
	uint8_t	tone_antenna_config_selection;
	uint8_t	phy;
	uint8_t	tx_power_delta;
	uint8_t	preferred_peer_antenna;
	uint8_t	snr_control_initiator;
	uint8_t	snr_control_reflector;
} __attribute__((packed));

struct hci_rp_le_cs_set_proc_param {
	uint8_t	status;
	uint16_t	handle;
} __attribute__((packed));

#define HCI_OP_LE_CS_SET_PROC_ENABLE		0x2094
struct hci_cp_le_cs_set_proc_enable {
	uint16_t	handle;
	uint8_t	config_id;
	uint8_t	enable;
} __attribute__((packed));

#define HCI_OP_LE_CS_TEST			0x2095
struct hci_cp_le_cs_test {
	uint8_t	main_mode_type;
	uint8_t	sub_mode_type;
	uint8_t	main_mode_repetition;
	uint8_t	mode_0_steps;
	uint8_t	role;
	uint8_t	rtt_type;
	uint8_t	cs_sync_phy;
	uint8_t	cs_sync_antenna_selection;
	uint8_t	subevent_len[3];
	uint16_t	subevent_interval;
	uint8_t	max_num_subevents;
	uint8_t	transmit_power_level;
	uint8_t	t_ip1_time;
	uint8_t	t_ip2_time;
	uint8_t	t_fcs_time;
	uint8_t	t_pm_time;
	uint8_t	t_sw_time;
	uint8_t	tone_antenna_config_selection;
	uint8_t	reserved;
	uint8_t	snr_control_initiator;
	uint8_t	snr_control_reflector;
	uint16_t	drbg_nonce;
	uint8_t	channel_map_repetition;
	uint16_t	override_config;
	uint8_t	override_parameters_length;
	uint8_t	override_parameters_data[];
} __attribute__((packed));

struct hci_rp_le_cs_test {
	uint8_t    status;
} __attribute__((packed));

#define HCI_OP_LE_CS_TEST_END			0x2096

#define HCI_EVT_LE_META_EVENT		0x3e

/* Channel Sounding Events */
#define HCI_EVT_LE_CS_READ_RMT_SUPP_CAP_COMPLETE	0x2C
struct hci_evt_le_cs_read_rmt_supp_cap_complete {
	uint8_t    status;
	uint16_t  handle;
	uint8_t    num_configs_supp;
	uint16_t   max_consec_proc_supp;
    uint8_t    num_ant_supp;
    uint8_t    max_ant_path_supp;
    uint8_t    roles_supp;
    uint8_t    modes_supp;
    uint8_t    rtt_cap;
    uint8_t    rtt_aa_only_n;
    uint8_t    rtt_sounding_n;
    uint8_t    rtt_rand_payload_n;
    uint16_t   nadm_sounding_cap;
    uint16_t   nadm_rand_cap;
    uint8_t    cs_sync_phys_supp;
    uint16_t   sub_feat_supp;
    uint16_t   t_ip1_times_supp;
    uint16_t   t_ip2_times_supp;
    uint16_t   t_fcs_times_supp;
    uint16_t   t_pm_times_supp;
    uint8_t    t_sw_times_supp;
    uint8_t    tx_snr_cap;
} __attribute__((packed));

#define HCI_EVT_LE_CS_READ_RMT_FAE_TABLE_COMPLETE	0x2D
struct hci_evt_le_cs_read_rmt_fae_table_complete {
	uint8_t	status;
	uint16_t	handle;
	uint8_t	remote_fae_table[72];
} __attribute__((packed));

#define HCI_EVT_LE_CS_SECURITY_ENABLE_COMPLETE	0x2E
struct hci_evt_le_cs_security_enable_complete {
	uint8_t	status;
	uint16_t	handle;
} __attribute__((packed));

#define HCI_EVT_LE_CS_CONFIG_COMPLETE	0x2F
struct hci_evt_le_cs_config_complete {
    uint8_t    status;
    uint16_t   handle;
    uint8_t    config_id;
    uint8_t    action;
    uint8_t    main_mode_type;
    uint8_t    sub_mode_type;
    uint8_t    min_main_mode_steps;
    uint8_t    max_main_mode_steps;
    uint8_t    main_mode_rep;
    uint8_t    mode_0_steps;
    uint8_t    role;
    uint8_t    rtt_type;
    uint8_t    cs_sync_phy;
    uint8_t    channel_map[10];
    uint8_t    channel_map_rep;
    uint8_t    channel_sel_type;
    uint8_t    ch3c_shape;
    uint8_t    ch3c_jump;
    uint8_t    reserved;
    uint8_t    t_ip1_time;
    uint8_t    t_ip2_time;
    uint8_t    t_fcs_time;
    uint8_t    t_pm_time;
} __attribute__((packed));

#define HCI_EVT_LE_CS_PROCEDURE_ENABLE_COMPLETE   0x30
struct hci_evt_le_cs_procedure_enable_complete {
	uint8_t	status;
	uint16_t	handle;
	uint8_t	config_id;
	uint8_t	state;
	uint8_t	tone_ant_config_sel;
	int8_t	sel_tx_pwr;
	uint8_t	sub_evt_len[3];
	uint8_t	sub_evts_per_evt;
	uint16_t	sub_evt_intrvl;
	uint16_t	evt_intrvl;
	uint16_t	proc_intrvl;
	uint16_t	proc_counter;
	uint16_t	max_proc_len;
} __attribute__((packed));

#define HCI_EVT_LE_CS_SUBEVENT_RESULT   0x31
#define MAX_NO_STEPS 160
#define MAX_STEP_DATA_LEN 255

struct hci_evt_le_cs_subevent_result {
	uint16_t		handle;
	uint8_t		config_id;
	uint16_t		start_acl_conn_evt_counter;
	uint16_t		proc_counter;
	uint16_t		freq_comp;
	uint8_t		ref_pwr_lvl;
	uint8_t		proc_done_status;
	uint8_t		subevt_done_status;
	uint8_t		abort_reason;
	uint8_t		num_ant_paths;
	uint8_t		num_steps_reported;
	uint16_t		step_data_len;
	uint8_t		step_data[];
} __attribute__((packed));

#define HCI_EVT_LE_CS_SUBEVENT_RESULT_CONTINUE   0x32
struct hci_evt_le_cs_subevent_result_continue {
	uint16_t		handle;
	uint8_t		config_id;
	uint8_t		proc_done_status;
	uint8_t		subevt_done_status;
	uint8_t		abort_reason;
	uint8_t		num_ant_paths;
	uint8_t		num_steps_reported;
	uint16_t		step_data_len;
	uint8_t		step_data[];
} __attribute__((packed));

#define HCI_EVT_LE_CS_TEST_END_COMPLETE			0x33
struct hci_evt_le_cs_test_end_complete {
	uint8_t	status;
} __attribute__((packed));