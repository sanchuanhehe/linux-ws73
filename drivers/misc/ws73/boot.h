/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _WS73_BOOT_H
#define _WS73_BOOT_H

#include "firmware.h"

#define WS73_BOOT_CHUNK_SIZE (32 * 1024)

enum ws73_boot_stage {
	WS73_BOOT_PREPARE,
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
	unsigned int preparation_writes;
};

struct ws73_boot_board {
	u8 xo_coarse;
	u8 xo_fine;
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

struct ws73_boot_version {
	u8 data[64];
	u8 length;
};

struct ws73_chip_observation {
	u32 raw;
	struct btws73_chip_identity chip;
};

/* Firmware's strap mapping, not proof that a boot register read was valid. */
int ws73_chip_word_decode(u32 value, struct btws73_chip_identity *chip);

/*
 * Fixed READM only; no address or write interface. Caller must establish a
 * boot transport and serialize it. Exact transfer status/length and an
 * absolute deadline are required. Success publishes an observation, not
 * authorization to select firmware: cold-start read validity remains a gate.
 * Zero is a legitimate U/USB encoding. On failure *observation is unchanged
 * and the session must be discarded, not retried against a late response.
 */
int ws73_boot_read_chip(const struct ws73_boot_io *io, u64 deadline_ms,
			struct ws73_chip_observation *observation);

/* Fixed VERSION query; opaque 1..64 actual bytes, not firmware compatibility.
 * Serialize with other boot commands; discard the session after any failure.
 * Output is unchanged on failure. No NUL termination is assumed.
 */
int ws73_boot_read_version(const struct ws73_boot_io *io, u64 deadline_ms,
			   struct ws73_boot_version *version);

/*
 * Caller must have verified identity, validated ALL immutable images and
 * completed board/calibration preparation. This only executes FILES/QUIT;
 * success means WAIT_RUNTIME, never verified runtime or an enabled function.
 * On any failure discard the session; do not replay ambiguous writes.
 */
int ws73_boot_download(const struct ws73_boot_io *io,
		       const struct ws73_fw_view image[WS73_FW_COUNT],
		       u64 deadline_ms, struct ws73_boot_progress *progress);

/*
 * Fixed SDK110 USB RAM2/3/4 preparation and explicit XO trim, then the three
 * original images and QUIT. No generic register address/value interface.
 * All image ranges and trim bounds are checked before any write. Caller must
 * first validate all file hashes, live identity and board compatibility.
 * Retain immutable inputs throughout; discard the session after any failure.
 * No automatic retry or rollback of partially acknowledged register writes.
 */
int ws73_boot_load(const struct ws73_boot_io *io,
		   const struct ws73_fw_view image[WS73_FW_COUNT],
		   const struct ws73_boot_board *board, u64 deadline_ms,
		   struct ws73_boot_progress *progress);

#endif
