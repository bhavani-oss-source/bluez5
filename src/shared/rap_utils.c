// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * BlueZ - Bluetooth protocol stack for Linux
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define _GNU_SOURCE
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

#include "src/shared/rap_utils.h"

#define EXTRA_LOGGING_DUMPING_RAW_BYTES
#ifdef EXTRA_LOGGING_DUMPING_RAW_BYTES
void rap_utils_dbg_hexdump_line(struct rap_utils *rap, const char *prefix, const uint8_t *buf, size_t len, size_t max);
#endif

bool rap_utils_set_debug(struct rap_utils *utils, bt_rap_debug_func_t func,
			void *user_data, bt_rap_destroy_func_t destroy)
{
	if (!utils)
		return false;

	if (utils->debug_destroy)
		utils->debug_destroy(utils->debug_data);

	utils->debug_func = func;
	utils->debug_destroy = destroy;
	utils->debug_data = user_data;

	return true;
}

void rap_utils_debug(struct rap_utils *utils, const char *format, ...)
{
	va_list ap;

	if (!utils || !format || !utils->debug_func)
		return;

	va_start(ap, format);
	util_debug_va(utils->debug_func, utils->debug_data, format, ap);
	va_end(ap);
}

/* Compact hexdump helper: prints up to max bytes on one DBG line */
void rap_utils_dbg_hexdump_line(struct rap_utils *utils, const char *prefix, const uint8_t *buf, size_t len, size_t max)
{
	if (!utils) {
		return;
	}
	if (!buf || len == 0) {
		DBG(utils,"%s: <empty>", prefix ? prefix : "hexdump");
		return;
	}
	size_t n = len < max ? len : max;
	/* 3 chars per byte ("xx ") + room for prefix */
	char line[3 * 1024]; /* supports up to max=128 safely */
	size_t wr = 0;
	for (size_t i = 0; i < n && (wr + 3) < sizeof(line); i++) {
		wr += snprintf(line + wr, sizeof(line) - wr, "%02x ", buf[i]);
	}
	if (wr > 0) line[wr - 1] = '\0'; /* trim trailing space */
	DBG(utils,"%s (%zu/%zu): %s%s",
		prefix ? prefix : "hexdump",
		n, len, line, (len > n ? " ..." : ""));
}