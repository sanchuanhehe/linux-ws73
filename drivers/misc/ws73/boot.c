// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/unaligned.h>

#include "boot.h"

#define WS73_BOOT_IO_TIMEOUT_MS 2000

static int ws73_boot_remaining(const struct ws73_boot_io *io, u64 deadline)
{
	u64 now;

	if (io->cancelled(io->context))
		return -ENODEV;
	now = io->now_ms(io->context);
	if (now >= deadline)
		return -ETIMEDOUT;
	return min_t(u64, deadline - now, WS73_BOOT_IO_TIMEOUT_MS);
}

static int ws73_boot_send(const struct ws73_boot_io *io, const u8 *data,
			  size_t len, u64 deadline)
{
	int timeout = ws73_boot_remaining(io, deadline);
	int ret;

	if (timeout < 0)
		return timeout;
	ret = io->send(io->context, data, len, timeout);
	if (ret < 0)
		return ret;
	if (ret != len)
		return -EREMOTEIO;
	ret = ws73_boot_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

static int ws73_boot_expect(const struct ws73_boot_io *io, const char *expected,
			    const char *tag, const char *echo, u64 deadline)
{
	u8 reply[512];
	size_t len = strlen(expected);
	int timeout = ws73_boot_remaining(io, deadline);
	int ret;

	if (timeout < 0)
		return timeout;
	ret = io->receive(io->context, reply, sizeof(reply), timeout);
	if (ret < 0)
		return ret;
	if (ret < len || ret > sizeof(reply))
		return -EMSGSIZE;
	if (memcmp(reply, expected, len))
		return -EPROTO;
	/* Match only the observed tag and entire issued command, including its
	 * trailing space. Bare ACKs and NUL padding remain valid.
	 */
	if (echo && ret > len && reply[len] == ':') {
		size_t tagged = strlen(tag), echoed = strlen(echo);

		if (ret - len < tagged + echoed ||
		    memcmp(reply + len, tag, tagged) ||
		    memcmp(reply + len + tagged, echo, echoed))
			return -EPROTO;
		len += tagged + echoed;
	}
	if (memchr_inv(reply + len, 0, ret - len))
		return -EPROTO;
	ret = ws73_boot_remaining(io, deadline);
	return ret < 0 ? ret : 0;
}

int ws73_chip_word_decode(u32 value, struct btws73_chip_identity *chip)
{
	struct btws73_chip_identity decoded = {};

	if (!chip)
		return -EINVAL;
	/* SDK110 U/E firmware use this same mapping of 0x40019000. */
	switch (value & 0x0c) {
	case 0x04:
		decoded.package = 1;
		decoded.bus = 1;
		break;
	case 0x08:
		decoded.package = 2;
		decoded.bus = (value & 0x30) == 0x20 ? 2 : 0;
		break;
	default:
		break;
	}
	*chip = decoded;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_chip_word_decode, "WS73");

int ws73_boot_read_chip(const struct ws73_boot_io *io, u64 deadline_ms,
			struct ws73_chip_observation *observation)
{
	static const u8 command[] = "READM 0x40019000 4 ";
	struct ws73_chip_observation parsed = {};
	u8 reply[512];
	int ret, timeout;

	if (!io || !observation || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive)
		return -EINVAL;
	ret = ws73_boot_send(io, command, sizeof(command) - 1, deadline_ms);
	if (ret)
		return ret;
	timeout = ws73_boot_remaining(io, deadline_ms);
	if (timeout < 0)
		return timeout;
	/* Larger capacity detects oversized/error replies instead of clipping. */
	ret = io->receive(io->context, reply, sizeof(reply), timeout);
	if (ret < 0)
		return ret;
	if (ret != sizeof(u32))
		return -EMSGSIZE;
	ret = ws73_boot_remaining(io, deadline_ms);
	if (ret < 0)
		return ret;
	parsed.raw = get_unaligned_le32(reply);
	ws73_chip_word_decode(parsed.raw, &parsed.chip);
	*observation = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_read_chip, "WS73");

int ws73_boot_read_version(const struct ws73_boot_io *io, u64 deadline_ms,
			   struct ws73_boot_version *version)
{
	static const u8 command[] = "VERSION ";
	struct ws73_boot_version parsed = {};
	u8 reply[64];
	int ret, timeout;

	if (!io || !version || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive)
		return -EINVAL;
	ret = ws73_boot_send(io, command, sizeof(command) - 1, deadline_ms);
	if (ret)
		return ret;
	/* SDK firmware_check_version waits 1 ms before receiving. */
	usleep_range(1000, 2000);
	timeout = ws73_boot_remaining(io, deadline_ms);
	if (timeout < 0)
		return timeout;
	ret = io->receive(io->context, reply, sizeof(reply), timeout);
	if (ret < 0)
		return ret;
	if (!ret || ret > sizeof(parsed.data))
		return -EMSGSIZE;
	parsed.length = ret;
	memcpy(parsed.data, reply, ret);
	ret = ws73_boot_remaining(io, deadline_ms);
	if (ret < 0)
		return ret;
	*version = parsed;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_read_version, "WS73");

static int ws73_boot_images_validate(const struct ws73_fw_view image[WS73_FW_COUNT])
{
	static const u32 addresses[WS73_FW_COUNT] = { 0x400000, 0x430000, 0x440000 };
	unsigned int i;

	if (!image)
		return -EINVAL;
	/* Check every range before the first side effect, including overlap. */
	for (i = 0; i < WS73_FW_COUNT; i++) {
		if (!image[i].data || !image[i].size ||
		    image[i].size > WS73_FW_MAX_SIZE - WS73_FW_HASH_HEADER ||
		    image[i].address != addresses[i] ||
		    (i + 1 < WS73_FW_COUNT && image[i].size > addresses[i + 1] - addresses[i]))
			return -EINVAL;
	}
	return 0;
}

int ws73_boot_download(const struct ws73_boot_io *io,
		       const struct ws73_fw_view image[WS73_FW_COUNT],
		       u64 deadline_ms, struct ws73_boot_progress *progress)
{
	char command[80];
	unsigned int i;
	int ret;

	if (!io || !progress || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive || !io->wait_ready)
		return -EINVAL;
	ret = ws73_boot_images_validate(image);
	if (ret)
		return ret;

	for (i = 0; i < WS73_FW_COUNT; i++) {
		size_t offset = 0;

		progress->image = i;
		progress->acknowledged = 0;
		while (offset < image[i].size) {
			size_t len = min_t(size_t, WS73_BOOT_CHUNK_SIZE, image[i].size - offset);
			bool last = len == image[i].size - offset;
			unsigned int flag;
			int command_len;

			/* SDK verification flags: start=0, middle=1, end=2, single=3. */
			flag = !offset ? (last ? 3 : 0) : (last ? 2 : 1);
			command_len = scnprintf(command, sizeof(command),
						"FILES 1 0x%x 0x%zx 0x%x ",
						image[i].address + (u32)offset, len, flag);
			progress->stage = WS73_BOOT_COMMAND;
			ret = ws73_boot_send(io, command, command_len, deadline_ms);
			if (ret)
				return ret;
			progress->stage = WS73_BOOT_READY;
			ret = ws73_boot_expect(io, "READY", ":FILE01:", command, deadline_ms);
			if (ret)
				return ret;
			io->wait_ready(io->context);
			progress->stage = WS73_BOOT_PAYLOAD;
			ret = ws73_boot_send(io, image[i].data + offset, len, deadline_ms);
			if (ret)
				return ret;
			progress->stage = WS73_BOOT_FILES_OK;
			ret = ws73_boot_expect(io, "FILES OK", ":", command, deadline_ms);
			if (ret)
				return ret;
			offset += len;
			progress->acknowledged = offset;
		}
	}
	progress->stage = WS73_BOOT_QUIT;
	/* SDK exec_quit_type_cmd terminates the command with a space. */
	ret = ws73_boot_send(io, "QUIT ", 5, deadline_ms);
	if (ret)
		return ret;
	/* The USB SDK has an EMPTY QUIT response; runtime probe is the gate. */
	progress->stage = WS73_BOOT_WAIT_RUNTIME;
	return 0;
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_download, "WS73");

int ws73_boot_load(const struct ws73_boot_io *io,
		   const struct ws73_fw_view image[WS73_FW_COUNT],
		   const struct ws73_boot_board *board, u64 deadline_ms,
		   struct ws73_boot_progress *progress)
{
	/* SDK cali_dyn_mem_cfg followed by bsle_cali_dyn_mem_cfg. */
	static const struct {
		u32 address;
		u32 value;
	} ram[] = {
		{ 0x40017750, 0x33 },
		{ 0x40017790, 0 },
		{ 0x40017794, 0 },
		{ 0x4001775c, 0x430000 },
		{ 0x40017760, 0x438000 },
		{ 0x40017750, 0x3f },
		{ 0x40017750, 0x2f },
		{ 0x40017798, 0 },
		{ 0x40017764, 0x440000 },
		{ 0x40017750, 0x3f },
	};
	char command[64];
	unsigned int i;
	int ret, length;

	if (!io || !board || !progress || !io->now_ms || !io->cancelled ||
	    !io->send || !io->receive || !io->wait_ready)
		return -EINVAL;
	if (board->xo_coarse > 15 || board->xo_fine > 127)
		return -ERANGE;
	ret = ws73_boot_images_validate(image);
	if (ret)
		return ret;
	*progress = (struct ws73_boot_progress) { .stage = WS73_BOOT_PREPARE };
	for (i = 0; i <= ARRAY_SIZE(ram); i++) {
		u32 address = i < ARRAY_SIZE(ram) ? ram[i].address : 0x40019408;
		u32 value = i < ARRAY_SIZE(ram) ? ram[i].value :
			board->xo_coarse | ((u32)board->xo_fine << 8);

		/* SDK write_device_reg uses decimal value and a trailing space. */
		length = scnprintf(command, sizeof(command), "WRITEM 4 0x%x %u ",
				   address, value);
		ret = ws73_boot_send(io, command, length, deadline_ms);
		if (ret)
			return ret;
		ret = ws73_boot_expect(io, "WRITEM OK", ":", command, deadline_ms);
		if (ret)
			return ret;
		progress->preparation_writes++;
	}
	return ws73_boot_download(io, image, deadline_ms, progress);
}
EXPORT_SYMBOL_NS_GPL(ws73_boot_load, "WS73");
