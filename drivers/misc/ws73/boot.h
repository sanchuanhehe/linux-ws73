/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BOOT_H
#define _WS73_BOOT_H

#include "firmware.h"

#define WS73_BOOT_CHUNK_SIZE (32 * 1024)

enum ws73_boot_stage {
	WS73_BOOT_COMMAND,
	WS73_BOOT_READY,
	WS73_BOOT_PAYLOAD,
	WS73_BOOT_FILES_OK,
	WS73_BOOT_QUIT,
	WS73_BOOT_WAIT_RUNTIME,
};

struct ws73_boot_progress {
	enum ws73_boot_stage stage;
	unsigned int image;
	size_t acknowledged;
};

/*
 * Synchronous, serialized, sleepable transport. Buffers are borrowed only for
 * the call. send/receive return the actual byte count or a negative errno.
 * now_ms is monotonic. cancelled must remain true once teardown starts.
 * Transport implementations must also interrupt a blocked call on teardown.
 */
struct ws73_boot_io {
	void *context;
	u64 (*now_ms)(void *context);
	bool (*cancelled)(void *context);
	int (*send)(void *context, const u8 *data, size_t len, unsigned int timeout_ms);
	int (*receive)(void *context, u8 *data, size_t capacity, unsigned int timeout_ms);
	void (*wait_ready)(void *context); /* Sleep at least 5 ms, per SDK. */
};

/*
 * Caller must have verified identity, validated ALL immutable images and
 * completed board/calibration preparation. This only executes FILES/QUIT;
 * success means WAIT_RUNTIME, never verified runtime or an enabled function.
 * On any failure discard the session; do not replay ambiguous writes.
 */
int ws73_boot_download(const struct ws73_boot_io *io,
		       const struct ws73_fw_view image[WS73_FW_COUNT],
		       u64 deadline_ms, struct ws73_boot_progress *progress);

#endif
