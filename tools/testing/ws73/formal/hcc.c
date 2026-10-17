// SPDX-License-Identifier: GPL-2.0-only
/* CBMC harness: compile the production translation unit and public types. */
#include "../../../../drivers/misc/ws73/hcc.c"

void *malloc(size_t);
void free(void *);
extern size_t nondet_size(void);
extern u8 nondet_byte(void);
extern u32 nondet_word(void);
extern bool nondet_bool(void);
void __CPROVER_havoc_object(void *);

_Static_assert(sizeof(u8) == 1 && sizeof(u16) == 2 && sizeof(u32) == 4,
	       "model must preserve kernel wire scalar widths");

/* Allocate only the inspected prefix, with an unaligned start. For short
 * declarations the allocation ends exactly at the declared boundary. For
 * longer declarations this proves that the suffix is not accessed at all.
 * Non-NULL allocated objects model the caller's valid retained storage.
 */
static u8 *input(size_t length, size_t width)
{
	size_t size = length < width ? length : width;
	u8 *base = malloc(size + 1);

	__CPROVER_assume(base != NULL);
	__CPROVER_havoc_object(base);
	return base;
}

/* Independent byte assembly for the success contract, not the shim helper. */
static u32 word(const u8 *p)
{
	return p[0] + (u32)p[1] * 256 + (u32)p[2] * 65536 + (u32)p[3] * 16777216;
}

void verify_chip_reply(void)
{
	size_t length = nondet_size(), index = nondet_size();
	u8 *base = input(length, 18), *data = base + 1;
	u8 guard = base[0], original = 0;
	bool no_input = nondet_bool(), no_output = nondet_bool(), valid = false;
	struct btws73_chip_identity result = { nondet_byte(), nondet_byte() };
	struct btws73_chip_identity before = result;
	int ret;

	if (index < length && index < 18)
		original = data[index];
	if (!no_input && !no_output && length >= 18 && length <= 4096) {
		u32 record = data[2] + (u32)data[3] * 256;
		u32 message = data[6] + (u32)data[7] * 256;

		valid = data[0] == 0x19 && data[1] == 9 && record >= 18 &&
			record <= length && data[4] == 10 && data[5] == 0 &&
			message >= 10 && message + 8 <= record && word(data + 8) == 0 &&
			word(data + 12) == 2 && data[16] <= 2 && data[17] <= 2;
	}
	ret = btws73_chip_reply_decode(no_input ? NULL : data, length,
				       no_output ? NULL : &result);
	__CPROVER_assert((ret == 0) == valid, "WS73 chip acceptance matches wire contract");
	__CPROVER_assert(ret <= 0, "WS73 chip result is success or error");
	if (ret)
		__CPROVER_assert(result.package == before.package && result.bus == before.bus,
				 "WS73 chip failure leaves output unchanged");
	else
		__CPROVER_assert(result.package == data[16] && result.bus == data[17],
				 "WS73 chip success preserves package and bus");
	__CPROVER_assert(base[0] == guard, "WS73 chip preserves prefix guard");
	if (index < length && index < 18)
		__CPROVER_assert(data[index] == original, "WS73 chip leaves input unchanged");
	free(base);
}

void verify_version_reply(void)
{
	size_t length = nondet_size(), index = nondet_size();
	u8 *base = input(length, 24), *data = base + 1;
	u8 guard = base[0], original = 0;
	bool no_input = nondet_bool(), no_output = nondet_bool(), valid = false;
	struct ws73_version result = { nondet_word(), nondet_word() }, before = result;
	int ret;

	if (index < length && index < 24)
		original = data[index];
	if (!no_input && !no_output && length >= 24 && length <= 4096) {
		u32 record = data[2] + (u32)data[3] * 256;
		u32 message = data[6] + (u32)data[7] * 256;

		valid = data[0] == 0x19 && data[1] == 9 && record >= 24 &&
			record <= length && data[4] == 7 && data[5] == 0 &&
			message >= 16 && message + 8 <= record && word(data + 8) == 0 &&
			word(data + 12) == 8;
	}
	ret = ws73_version_reply_decode(no_input ? NULL : data, length,
					no_output ? NULL : &result);
	__CPROVER_assert((ret == 0) == valid, "WS73 version acceptance matches wire contract");
	__CPROVER_assert(ret <= 0, "WS73 version result is success or error");
	if (ret)
		__CPROVER_assert(result.firmware == before.firmware && result.rom == before.rom,
				 "WS73 version failure leaves output unchanged");
	else
		__CPROVER_assert(result.firmware == word(data + 16) && result.rom == word(data + 20),
				 "WS73 version success preserves all bits");
	__CPROVER_assert(base[0] == guard, "WS73 version preserves prefix guard");
	if (index < length && index < 24)
		__CPROVER_assert(data[index] == original, "WS73 version leaves input unchanged");
	free(base);
}

static void query(bool version)
{
	size_t capacity = nondet_size(), index = nondet_size();
	u8 *base = input(capacity, 32), *out = base + 1;
	u8 guard = base[0], original = 0;
	bool absent = nondet_bool();
	unsigned int i;
	int ret;

	if (index < capacity && index < 32)
		original = out[index];
	ret = version ? ws73_version_query_encode(absent ? NULL : out, capacity) :
		btws73_chip_query_encode(absent ? NULL : out, capacity);
	__CPROVER_assert(ret == (absent ? -EINVAL : capacity < 32 ? -ENOSPC : 32),
			 "WS73 query return follows pointer and capacity contract");
	__CPROVER_assert(base[0] == guard, "WS73 query preserves prefix guard");
	if (ret < 0 && index < capacity && index < 32)
		__CPROVER_assert(out[index] == original, "WS73 query failure leaves output unchanged");
	if (ret == 32)
		for (i = 0; i < 32; i++) {
			u8 expected = i == 0 ? 0x19 : i == 1 ? 9 : i == 2 ? 32 :
				i == 4 ? (version ? 13 : 16) : i == 6 ? 1 :
				i == 8 && version ? 1 : 0;

			__CPROVER_assert(out[i] == expected, "WS73 query bytes match wire contract");
		}
	free(base);
}

void verify_chip_query(void)
{
	query(false);
}

void verify_version_query(void)
{
	query(true);
}
