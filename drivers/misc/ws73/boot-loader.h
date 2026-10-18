/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BOOT_LOADER_H
#define _WS73_BOOT_LOADER_H

#include "boot.h"

enum ws73_boot_session_stage {
	WS73_SESSION_NEW,
	WS73_SESSION_IDENTITY,
	WS73_SESSION_INPUTS,
	WS73_SESSION_TRANSFER,
	WS73_SESSION_WAIT_RUNTIME,
};

struct ws73_boot_session {
	enum ws73_boot_session_stage stage;
	int error;
	struct ws73_chip_observation observation;
	struct ws73_boot_progress progress;
};

/*
 * One serialized, sleepable attempt per zero-initialized session. Caller owns
 * the device/IO/session until return and must interrupt blocked IO on removal.
 * The owner must establish boot-register access and select explicitly sourced
 * board inputs for the intended hardware or bounded development trial. This
 * API supplies no board defaults and cannot turn a successful load into cold
 * boot, RF/trim, or cross-board qualification.
 * expected binds the explicit board inputs to a supported U/USB or E/USB
 * identity. A fresh READM must agree before any firmware request or write;
 * mismatch is terminal, never an invitation to switch profiles. This check
 * does not prove cold-ROM identity validity or board input provenance.
 * It then selects exactly one USB U/E profile, acquires and
 * validates all three original files, prepares RAM/XO, then sends FILES/QUIT.
 * Firmware references stay live until all synchronous transfers have joined.
 * Filesystem acquisition is synchronous; cancellation/deadline is checked
 * again before any write. No userspace firmware fallback or retry is used.
 * First error is retained, successful reuse returns -EALREADY. WAIT_RUNTIME
 * is not Ready and conveys no state/cache to a re-enumerated runtime instance.
 */
int ws73_boot_start(struct device *dev, const struct ws73_boot_io *io,
		    const struct ws73_boot_board *board,
		    const struct btws73_chip_identity *expected, u64 deadline_ms,
		    struct ws73_boot_session *session);

#endif
