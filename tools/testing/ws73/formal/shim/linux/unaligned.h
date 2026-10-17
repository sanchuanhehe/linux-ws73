/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_FORMAL_UNALIGNED_H
#define WS73_FORMAL_UNALIGNED_H
#include <linux/types.h>
static inline u16 get_unaligned_le16(const void *ptr)
{
	const u8 *p = ptr;
	return (u16)p[0] | (u16)p[1] << 8;
}
static inline u32 get_unaligned_le32(const void *ptr)
{
	const u8 *p = ptr;
	return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static inline void put_unaligned_le16(u16 value, void *ptr)
{
	u8 *p = ptr;
	p[0] = value & 0xff;
	p[1] = value >> 8;
}
#endif
