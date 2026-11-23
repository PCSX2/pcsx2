// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "PS2Float.h"

#include <cmath>
#include <float.h>

#include "VUmicro.h"

/*****************************************/
/*          NEW FLAGS                    */ //By asadr. Thnkx F|RES :p
/*****************************************/

static __ri u32 VU_MAC_UPDATE( s32 shift, VURegs* VU, float f)
{
	u32 v = *(u32*)&f;
	s32 exp = (v >> 23) & 0xff;
	u32 s = v & 0x80000000;

	if (s)
		VU->macflag |= 0x0010<<shift;
	else
		VU->macflag &= ~(0x0010<<shift);

	if( f == 0 )
	{
		VU->macflag = (VU->macflag & ~(0x1100<<shift)) | (0x0001<<shift);
		return v;
	}

	switch(exp)
	{
		case 0:
			VU->macflag = (VU->macflag&~(0x1000<<shift)) | (0x0101<<shift);
			return s;
		case 255:
			VU->macflag = (VU->macflag&~(0x0101<<shift)) | (0x1000<<shift);
			if (CHECK_VU_OVERFLOW((VU == &VU1) ? 1 : 0))
				return s | 0x7f7fffff; /* max allowed */
			else
				return v;
		default:
			VU->macflag = (VU->macflag & ~(0x1101<<shift));
			return v;
	}
}

static __ri u32 VU_MAC_UPDATE(s32 shift, VURegs* VU, PS2Float f)
{
	bool isUnderflow = false;
	
	u32 v = f.raw;

	if (v & PS2Float::SIGNMASK)
		VU->macflag |= 0x0010 << shift;
	else
		VU->macflag &= ~(0x0010 << shift);
	
	if (f.uf)
	{
		isUnderflow = true;
		VU->macflag = (VU->macflag & ~(0x1000 << shift)) | (0x0101 << shift);
	}

	if (f.IsZero())
	{
		VU->macflag = (VU->macflag & ~(0x1100 << shift)) | (0x0001 << shift);
		return v;
	}
	else if (f.of) { VU->macflag = (VU->macflag & ~(0x0101 << shift)) | (0x1000 << shift); }
	else if (!isUnderflow) { VU->macflag = (VU->macflag & ~(0x1101 << shift)); }

	return v;
}

__fi bool IsOverflowSet(VURegs* VU, s32 shift)
{
	return (VU->macflag & (0x1000 << shift));
}

__fi u32 VU_MACx_UPDATE(VURegs * VU, float x)
{
	return VU_MAC_UPDATE(3, VU, x);
}

__fi u32 VU_MACy_UPDATE(VURegs * VU, float y)
{
	return VU_MAC_UPDATE(2, VU, y);
}

__fi u32 VU_MACz_UPDATE(VURegs * VU, float z)
{
	return VU_MAC_UPDATE(1, VU, z);
}

__fi u32 VU_MACw_UPDATE(VURegs * VU, float w)
{
	return VU_MAC_UPDATE(0, VU, w);
}

__fi u32 VU_MACx_UPDATE(VURegs* VU, PS2Float x)
{
	return VU_MAC_UPDATE(3, VU, x);
}

__fi u32 VU_MACy_UPDATE(VURegs* VU, PS2Float y)
{
	return VU_MAC_UPDATE(2, VU, y);
}

__fi u32 VU_MACz_UPDATE(VURegs* VU, PS2Float z)
{
	return VU_MAC_UPDATE(1, VU, z);
}

__fi u32 VU_MACw_UPDATE(VURegs* VU, PS2Float w)
{
	return VU_MAC_UPDATE(0, VU, w);
}

__fi void VU_MACx_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<3);
}

__fi void VU_MACy_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<2);
}

__fi void VU_MACz_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<1);
}

__fi void VU_MACw_CLEAR(VURegs * VU)
{
	VU->macflag&= ~(0x1111<<0);
}

__ri void VU_STAT_UPDATE(VURegs * VU) {
	int newflag = 0 ;
	if (VU->macflag & 0x000F) newflag = 0x1;
	if (VU->macflag & 0x00F0) newflag |= 0x2;
	if (VU->macflag & 0x0F00) newflag |= 0x4;
	if (VU->macflag & 0xF000) newflag |= 0x8;
	// Save old sticky flags and D/I settings, everthing else is the new flags only
	VU->statusflag = newflag;
}
