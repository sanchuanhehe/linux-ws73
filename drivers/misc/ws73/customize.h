/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_CUSTOMIZE_H
#define _WS73_CUSTOMIZE_H

#include <linux/types.h>

#define WS73_CUSTOMIZE_SIZE 800
#define WS73_CUSTOMIZE_BODY_SIZE 140

struct ws73_frequency_reference {
	u16 frequency;
	u16 reference;
};

/* Host values, not a packed wire structure or a set of board defaults. */
struct ws73_customization {
	u32 coexistence;
	u32 disable_ll_privacy;
	u32 suspend_mode;
	u32 scan_interval;
	u32 scan_window;
	u32 front_switch;
	s32 max_power;
	u32 calibration_count;
	struct ws73_frequency_reference calibration[8];
	u16 channel_boundary[7];
	u32 srrc_switch;
	struct ws73_frequency_reference srrc[8];
};

/*
 * Encode SDK110 USB customization with use_flash = 0; flash-derived trims,
 * addresses and padding are zero. No flash override or default profile is
 * supplied. Caller must obtain board-qualified inputs before device I/O.
 * Only the structural count bound is checked, not RF/board compatibility.
 * Input/output must not overlap. Output survives errors unchanged.
 */
int ws73_customize_encode(u8 *out, size_t capacity,
			  const struct ws73_customization *parameters);
/* Exact supported TX shape only; not an RX notification or board validator. */
int ws73_customize_validate(const u8 *record, size_t length);

#endif
