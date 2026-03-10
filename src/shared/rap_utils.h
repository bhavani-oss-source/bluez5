// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * BlueZ - Bluetooth protocol stack for Linux
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "src/shared/io.h"

typedef void (*bt_rap_debug_func_t)(const char *str, void *user_data);
typedef void (*bt_rap_destroy_func_t)(void *user_data);

struct rap_utils {
	bt_rap_debug_func_t debug_func;
	bt_rap_destroy_func_t debug_destroy;
	void *debug_data;
};

bool rap_utils_set_debug(struct rap_utils *utils, bt_rap_debug_func_t func,
			void *user_data, bt_rap_destroy_func_t destroy);
void rap_utils_debug(struct rap_utils *utils, const char *format, ...);

#define DBG(_utils, fmt, ...) \
	rap_utils_debug(_utils, "%s:%s() " fmt, __FILE__, __func__, ##__VA_ARGS__)