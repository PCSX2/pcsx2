// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

struct vifStruct;

typedef void (*UNPACKFUNCTYPE)(void* dest, const void* src);

#define create_unpack_u_type(bits)		typedef void (*UNPACKFUNCTYPE_u##bits)(u32* dest, const u##bits* src);
#define create_unpack_s_type(bits)		typedef void (*UNPACKFUNCTYPE_s##bits)(u32* dest, const s##bits* src);

#define create_some_unpacks(bits)		\
		create_unpack_u_type(bits);		\
		create_unpack_s_type(bits);		\

create_some_unpacks(32);
create_some_unpacks(16);
create_some_unpacks(8);

alignas(16) extern const u8 nVifT[16];

// Array sub-dimension order: [vifidx] [mode] (VN * VL * USN * doMask)
alignas(16) extern const UNPACKFUNCTYPE VIFfuncTable[2][4][(4 * 4 * 2 * 2)];

_vifT extern int  nVifUnpack (const u8* data);
extern void resetNewVif(int idx);

template< int idx >
extern void vifUnpackSetup(const u32* data);
