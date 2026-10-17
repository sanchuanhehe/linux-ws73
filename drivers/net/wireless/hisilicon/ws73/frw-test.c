// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/unaligned.h>

#include "frw.h"

static const struct ws73_frw_key key = { .id = 0x1234,
					 .vap = 7,
					 .sequence = 0x01020304 };
static const u8 request_bytes[] = {
	0x34, 0x12, 7, 32, 0, 0, 0, 0, 0x34, 0x12, 7,	 1,    0, 0,
	0,    0,    4, 3,  2, 1, 0, 0, 0,    0,	   0,	 0,    0, 0,
	4,    0,    8, 0,  0, 0, 1, 0, 0xde, 0xad, 0xbe, 0xef,
};

/* Independent synthetic RSP; nonzero pointer slots must never be followed. */
static const u8 reply_bytes[] = {
	0x34, 0x12, 0x17, 32,	0,    0,    0,	  0,	0x34, 0x12,
	7,    3,    0xfb, 0xff, 0xff, 0xff, 4,	  3,	2,    1,
	0xff, 0xff, 0xff, 0xff, 0x78, 0x56, 0x34, 0x12, 0xfe, 0xff,
	8,    0,    4,	  0,	0xff, 0xff, 1,	  2,	3,    4,
};

static void expect_error(struct kunit *test, const u8 *wire, size_t size,
			 const struct ws73_frw_key *expected, size_t capacity,
			 int error)
{
	struct ws73_frw_reply reply;
	u8 before[sizeof(reply)];

	memset(&reply, 0xa5, sizeof(reply));
	memcpy(before, &reply, sizeof(reply));
	KUNIT_EXPECT_EQ(test,
			ws73_frw_reply_decode(wire, size, expected, capacity,
					      &reply),
			error);
	KUNIT_EXPECT_MEMEQ(test, &reply, before, sizeof(reply));
}

static void ws73_frw_request_bytes(struct kunit *test)
{
	u8 out[sizeof(request_bytes) + 2];
	const u8 payload[] = { 0xde, 0xad, 0xbe, 0xef };

	memset(out, 0xa5, sizeof(out));
	KUNIT_ASSERT_EQ(test,
			ws73_frw_request_encode(out + 1, sizeof(request_bytes),
						&key, payload, sizeof(payload),
						8),
			40);
	KUNIT_EXPECT_MEMEQ(test, out + 1, request_bytes, sizeof(request_bytes));
	KUNIT_EXPECT_EQ(test, out[0], 0xa5);
	KUNIT_EXPECT_EQ(test, out[sizeof(out) - 1], 0xa5);
}

static void ws73_frw_request_limits(struct kunit *test)
{
	u8 out[40], before[40];
	struct ws73_frw_key bad = key;
	size_t i;

	memset(out, 0xa5, sizeof(out));
	memcpy(before, out, sizeof(out));
	for (i = 0; i < sizeof(out); i++) {
		KUNIT_EXPECT_EQ(test,
				ws73_frw_request_encode(out, i, &key,
							reply_bytes, 4, 8),
				-ENOSPC);
		KUNIT_EXPECT_MEMEQ(test, out, before, sizeof(out));
	}
	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(out, SIZE_MAX, &key,
						reply_bytes, SIZE_MAX, 8),
			-EMSGSIZE);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(out, sizeof(out), &key,
						reply_bytes, 4, SIZE_MAX),
			-EMSGSIZE);
	bad.vap = 16;
	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(out, sizeof(out), &bad, NULL, 0,
						0),
			-EINVAL);
	KUNIT_EXPECT_MEMEQ(test, out, before, sizeof(out));
}

static void ws73_frw_reply_bytes(struct kunit *test)
{
	struct ws73_frw_reply reply;
	u8 wire[sizeof(reply_bytes) + 1];

	memcpy(wire + 1, reply_bytes, sizeof(reply_bytes));
	KUNIT_ASSERT_EQ(test,
			ws73_frw_reply_decode(wire + 1, sizeof(reply_bytes),
					      &key, 8, &reply),
			0);
	KUNIT_EXPECT_EQ(test, reply.device_status, -5);
	KUNIT_EXPECT_PTR_EQ(test, reply.data, wire + 1 + WS73_FRW_HEAD_LEN);
	KUNIT_EXPECT_EQ(test, reply.length, 4);
	KUNIT_EXPECT_MEMEQ(test, reply.data, reply_bytes + WS73_FRW_HEAD_LEN,
			   4);
	KUNIT_EXPECT_MEMEQ(test, wire + 1, reply_bytes, sizeof(reply_bytes));
	put_unaligned_le32(0x7fffffff, wire + 1 + 12);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_reply_decode(wire + 1, sizeof(reply_bytes),
					      &key, 8, &reply),
			0);
	KUNIT_EXPECT_EQ(test, reply.device_status, S32_MAX);
	put_unaligned_le32(0x80000000, wire + 1 + 12);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_reply_decode(wire + 1, sizeof(reply_bytes),
					      &key, 8, &reply),
			0);
	KUNIT_EXPECT_EQ(test, reply.device_status, S32_MIN);
}

static void ws73_frw_reply_truncation(struct kunit *test)
{
	size_t i;
	u8 *wire;

	for (i = 0; i < sizeof(reply_bytes); i++) {
		/* The borrowed unaligned extent ends exactly at the allocation. */
		wire = kmalloc(i + 1, GFP_KERNEL);
		KUNIT_ASSERT_NOT_NULL(test, wire);
		wire[0] = 0x5a;
		memcpy(wire + 1, reply_bytes, i);
		expect_error(test, wire + 1, i, &key, 8, -EMSGSIZE);
		KUNIT_EXPECT_EQ(test, wire[0], 0x5a);
		KUNIT_EXPECT_MEMEQ(test, wire + 1, reply_bytes, i);
		kfree(wire);
	}
}

static void ws73_frw_reply_metadata(struct kunit *test)
{
	static const struct {
		u8 offset, value;
	} mutations[] = {
		{ 2, 7 },    { 11, 1 },	  { 11, 2 },	{ 11, 0x13 },
		{ 5, 1 },    { 5, 0x10 }, { 6, 1 },	{ 7, 1 },
		{ 0, 0x35 }, { 2, 0x16 }, { 10, 0x17 },
	};
	struct ws73_frw_key wrong;
	u8 wire[sizeof(reply_bytes)];
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mutations); i++) {
		memcpy(wire, reply_bytes, sizeof(wire));
		wire[mutations[i].offset] = mutations[i].value;
		expect_error(test, wire, sizeof(wire), &key, 8, -EPROTO);
	}
	wrong = key;
	wrong.id++;
	expect_error(test, reply_bytes, sizeof(reply_bytes), &wrong, 8,
		     -ENOENT);
	wrong = key;
	wrong.vap++;
	expect_error(test, reply_bytes, sizeof(reply_bytes), &wrong, 8,
		     -ENOENT);
	wrong = key;
	wrong.sequence++;
	expect_error(test, reply_bytes, sizeof(reply_bytes), &wrong, 8,
		     -ENOENT);
}

static void ws73_frw_reply_bounds(struct kunit *test)
{
	u8 wire[sizeof(reply_bytes) + 1];

	memcpy(wire, reply_bytes, sizeof(reply_bytes));
	wire[sizeof(reply_bytes)] = 0;
	expect_error(test, wire, sizeof(wire), &key, 8, -EMSGSIZE);
	expect_error(test, reply_bytes, sizeof(reply_bytes), &key, 3,
		     -EMSGSIZE);
	expect_error(test, reply_bytes, SIZE_MAX, &key, 8, -EMSGSIZE);
	expect_error(test, reply_bytes, sizeof(reply_bytes), &key, SIZE_MAX,
		     -EMSGSIZE);
	wire[30] = 3; /* Remote capacity must not be silently enlarged. */
	expect_error(test, wire, sizeof(reply_bytes), &key, 8, -EMSGSIZE);
	memcpy(wire, reply_bytes, sizeof(reply_bytes));
	wire[3] = 31;
	expect_error(test, wire, sizeof(reply_bytes), &key, 8, -EMSGSIZE);
	memcpy(wire, reply_bytes, sizeof(reply_bytes));
	wire[32] = 3;
	expect_error(test, wire, sizeof(reply_bytes), &key, 8, -EMSGSIZE);
}

static void ws73_frw_zero_and_maximum(struct kunit *test)
{
	const size_t size = WS73_FRW_HEAD_LEN + WS73_FRW_DATA_MAX;
	struct ws73_frw_reply reply;
	u8 *wire = kunit_kmalloc(test, size, GFP_KERNEL);
	u8 *payload = kunit_kmalloc(test, WS73_FRW_DATA_MAX, GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, wire);
	KUNIT_ASSERT_NOT_NULL(test, payload);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_request_encode(wire, size, &key, NULL, 0, 0),
			36);
	memset(payload, 0xa7, WS73_FRW_DATA_MAX);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_request_encode(wire, size, &key, payload,
						WS73_FRW_DATA_MAX,
						WS73_FRW_DATA_MAX),
			(int)size);
	KUNIT_EXPECT_MEMEQ(test, wire + WS73_FRW_HEAD_LEN, payload,
			   WS73_FRW_DATA_MAX);
	memcpy(wire, reply_bytes, sizeof(reply_bytes));
	put_unaligned_le16(28 + WS73_FRW_DATA_MAX, wire + 3);
	put_unaligned_le16(WS73_FRW_DATA_MAX, wire + 30);
	put_unaligned_le16(WS73_FRW_DATA_MAX, wire + 32);
	memcpy(wire + WS73_FRW_HEAD_LEN, payload, WS73_FRW_DATA_MAX);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_reply_decode(wire, size, &key,
					      WS73_FRW_DATA_MAX, &reply),
			0);
	KUNIT_EXPECT_EQ(test, reply.length, WS73_FRW_DATA_MAX);
	KUNIT_EXPECT_MEMEQ(test, reply.data, payload, WS73_FRW_DATA_MAX);
	put_unaligned_le16(28, wire + 3);
	put_unaligned_le16(0, wire + 30);
	put_unaligned_le16(0, wire + 32);
	KUNIT_ASSERT_EQ(test,
			ws73_frw_reply_decode(wire, WS73_FRW_HEAD_LEN, &key, 0,
					      &reply),
			0);
	KUNIT_EXPECT_EQ(test, reply.length, 0);
	KUNIT_EXPECT_EQ(test, reply.device_status, -5);
}

static void ws73_frw_null_args(struct kunit *test)
{
	struct ws73_frw_reply reply;
	u8 out[40];

	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(NULL, 0, &key, NULL, 0, 0),
			-EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(out, sizeof(out), NULL, NULL, 0,
						0),
			-EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_request_encode(out, sizeof(out), &key, NULL, 1,
						0),
			-EINVAL);
	expect_error(test, NULL, sizeof(reply_bytes), &key, 8, -EINVAL);
	expect_error(test, reply_bytes, sizeof(reply_bytes), NULL, 8, -EINVAL);
	KUNIT_EXPECT_EQ(test,
			ws73_frw_reply_decode(reply_bytes, sizeof(reply_bytes),
					      &key, 8, NULL),
			-EINVAL);
	/* A later valid record is unaffected by malformed input: parser is stateless. */
	KUNIT_EXPECT_EQ(test,
			ws73_frw_reply_decode(reply_bytes, sizeof(reply_bytes),
					      &key, 8, &reply),
			0);
}

static struct kunit_case ws73_frw_cases[] = {
	KUNIT_CASE(ws73_frw_request_bytes),
	KUNIT_CASE(ws73_frw_request_limits),
	KUNIT_CASE(ws73_frw_reply_bytes),
	KUNIT_CASE(ws73_frw_reply_truncation),
	KUNIT_CASE(ws73_frw_reply_metadata),
	KUNIT_CASE(ws73_frw_reply_bounds),
	KUNIT_CASE(ws73_frw_zero_and_maximum),
	KUNIT_CASE(ws73_frw_null_args),
	{}
};

static struct kunit_suite ws73_frw_suite = {
	.name = "ws73-wifi-frw",
	.test_cases = ws73_frw_cases,
};

kunit_test_suite(ws73_frw_suite);

MODULE_LICENSE("GPL");
