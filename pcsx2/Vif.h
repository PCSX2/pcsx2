/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "MemoryTypes.h"
#include "R5900.h"

enum vif0_stat_flags
{
	VIF0_STAT_VPS_W 	= (1),
	VIF0_STAT_VPS_D 	= (2),
	VIF0_STAT_VPS_T		= (3),
	VIF0_STAT_VPS 		= (3),
	VIF0_STAT_VEW		= (1<<2),
	VIF0_STAT_MRK		= (1<<6),
	VIF0_STAT_DBF		= (1<<7),
	VIF0_STAT_VSS		= (1<<8),
	VIF0_STAT_VFS		= (1<<9),
	VIF0_STAT_VIS		= (1<<10),
	VIF0_STAT_INT		= (1<<11),
	VIF0_STAT_ER0		= (1<<12),
	VIF0_STAT_ER1		= (1<<13),
	VIF0_STAT_FQC		= (15<<24)
};

enum vif1_stat_flags
{
	VIF1_STAT_VPS_W 	= (1),
	VIF1_STAT_VPS_D 	= (2),
	VIF1_STAT_VPS_T		= (3),
	VIF1_STAT_VPS 		= (3),
	VIF1_STAT_VEW		= (1<<2),
	VIF1_STAT_VGW		= (1<<3),
	VIF1_STAT_MRK		= (1<<6),
	VIF1_STAT_DBF		= (1<<7),
	VIF1_STAT_VSS		= (1<<8),
	VIF1_STAT_VFS		= (1<<9),
	VIF1_STAT_VIS		= (1<<10),
	VIF1_STAT_INT		= (1<<11),
	VIF1_STAT_ER0		= (1<<12),
	VIF1_STAT_ER1		= (1<<13),
	VIF1_STAT_FDR 		= (1<<23),
	VIF1_STAT_FQC		= (31<<24)
};

// These are the stat flags that are the same for vif0 & vif1,
// for occasions where we don't necessarily know which we are using.
enum vif_stat_flags
{
	VIF_STAT_VPS_W		= (1),
	VIF_STAT_VPS_D		= (2),
	VIF_STAT_VPS_T		= (3),
	VIF_STAT_VPS 		= (3),
	VIF_STAT_VEW		= (1<<2),
	VIF_STAT_MRK		= (1<<6),
	VIF_STAT_DBF		= (1<<7),
	VIF_STAT_VSS		= (1<<8),
	VIF_STAT_VFS		= (1<<9),
	VIF_STAT_VIS		= (1<<10),
	VIF_STAT_INT		= (1<<11),
	VIF_STAT_ER0		= (1<<12),
	VIF_STAT_ER1		= (1<<13)
};

// the VIF codes are pretty sparse, so direct assignments are required for several of them.
enum vif_codes
{
	VIFcode_NOP			= 0,
	VIFcode_STCYCL,
	VIFcode_OFFSET,
	VIFcode_BASE,
	VIFcode_ITOP,
	VIFcode_STMOD,
	VIFcode_MSKPATH3,
	VIFcode_MARK,

	VIFcode_FLUSHE		= 0x10,
	VIFcode_FLUSH,

	VIFcode_FLUSHA		= 0x13,
	VIFcode_MSCAL,
	VIFcode_MSCALF,

	VIFcode_MSCNT		= 0x17,

	VIFcode_STMASK		= 0x20,

	VIFcode_STROW		= 0x30,
	VIFcode_STCOL,
	
	VIFcode_MPG			= 0x4b,

	VIFcode_DIRECT		= 0x50,
	VIFcode_DIRECTHL,

	VIFcode_UPK_S_32	= 0x60,
	VIFcode_UPK_S_16,
	VIFcode_UPK_S_8,

	VIFcode_UPK_V2_32	= 0x64,
	VIFcode_UPK_V2_16,
	VIFcode_UPK_V2_8,

	VIFcode_UPK_V3_32	= 0x68,
	VIFcode_UPK_V3_16,
	VIFcode_UPK_V3_8,

	VIFcode_UPK_V4_32	= 0x6c,
	VIFcode_UPK_V4_16,
	VIFcode_UPK_V4_8,
	VIFcode_UPK_V4_5

};

enum vif_status
{
    VPS_IDLE		 = 0,
    
    // Waiting for data.  This status applies to STROW / STCOL and similar commands that
    // have fixed-sized data attached to them.  It may also apply to DIRECT/DIRECTHL when
    // there is not enough data in the FIFO or VIFdma fragment to complete the PATH2 transfer.
    VPS_WAITING		 = 1,
    
    // Applies to commands, but not used in PCSX2 because decoding is not a parallel process
    // (all commands decode instantly from the perspective of the virtual PS2).
    VPS_DECODING	 = 2,
    
    // Transferring and/or decompressing data to VU (program or data) via MPG or UNPACK, or
    // to GS via DIRECT/DIRECTHL (PATH2).  This status is used when VIF has data to transfer
    // and the GIF is either transferring slowly (stalled) or busy on PATH1/PATH3 transfers.
    // If the VIF runs out of data prior to completion of the VIF packet, VPS_WAITING is used.
    VPS_TRANSFERRING = 3
};

//
// Bitfield Structure
//
union tVIF_STAT {
	struct {
		// Vif status.  Values:
		//   00 - idle
		//   01 - waiting for data following vifcode
		//   10 - decoding vifcode
		//   11 - decompressing/transferring data following vifcode.
		u32 VPS : 2;
		u32 VEW : 1; // E-bit wait (1 - wait, 0 - don't wait)

		// Status waiting for the end of gif transfer (Vif1 only).
		// This should be set to 1 for any DIRECT/DIRECTHL transfer that stalls because of pre-existing
		// GIF transfers.  It should also go high if a similar stall occurs during MSCALF or
		// FLUSH/FLUSHA commands.
		u32 VGW : 1;
		
		u32 _reserved : 2;

		u32 MRK : 1; // Mark Detect
		u32 DBF : 1; // Double Buffer Flag
		u32 VSS : 1; // Stopped by STOP
		u32 VFS : 1; // Stopped by ForceBreak
		u32 VIS : 1; // Vif Interrupt Stall
		u32 INT : 1; // Interrupt by the i bit.

		// DmaTag Mismatch error.
		// This 'feature' is not well supported on the PS2 due to a bug that causes certain types
		// of VIFcode packets to trigger false positives.
		u32 ER0 : 1;
		u32 ER1 : 1; // VifCode error

		u32 _reserved2 : 9;

		// VIF/FIFO transfer direction.
		//  0 - memory -> Vif  (drain)
		//  1 - Vif -> memory  (source)
		u32 FDR : 1; 
		u32 FQC : 5; // Amount of data in the FIFO. Up to 8 qwords on Vif0, 16 on Vif1.
	};
	u32 _u32;

	tVIF_STAT(u32 val)			{ _u32 = val; }
	bool test(u32 flags) const	{ return !!(_u32 & flags); }
	void set_flags	(u32 flags)	{ _u32 |=  flags; }
	void clear_flags(u32 flags) { _u32 &= ~flags; }
	void reset()				{ _u32 = 0; }
	wxString desc() const		{ return pxsFmt(L"Stat: 0x%x", _u32); }
	
	bool IsStalled() const
	{
		// uses manually-constructed mask (typically optimizes better, thanks dumb-ass compilers)
		//return VSS || VFS || VIS || ER0 || ER1;
		return (_u32 &
			(VIF_STAT_VSS | VIF_STAT_VFS | VIF_STAT_VIS | VIF_STAT_ER0 | VIF_STAT_ER1)
		) != 0;
	}
};

#define VIF_STAT(value) ((tVIF_STAT)(value))

// See VIF.cpp (vifWrite32) for implementation details of each field of this struct.
union tVIF_FBRST {
	struct {
		u32 RST : 1; // Resets Vif(0/1) when written.
		u32 FBK : 1; // Causes a Forcebreak to Vif((0/1) when true. (Stall)
		u32 STP : 1; // Stops after the end of the Vifcode in progress when true. (Stall)
		u32 STC : 1; // Cancels the Vif(0/1) stall and clears Vif Stats VSS, VFS, VIS, INT, ER0 & ER1.
		u32 _reserved : 28;
	};
	u32 _u32;

	tVIF_FBRST(u32 val)					{ _u32 = val; }
	bool test		(u32 flags) const	{ return !!(_u32 & flags); }
	void set_flags	(u32 flags)			{ _u32 |=  flags; }
	void clear_flags(u32 flags)			{ _u32 &= ~flags; }
	void reset()						{ _u32 = 0; }
	wxString desc() const				{ return pxsFmt(L"Fbrst: 0x%x", _u32); }
};

#define FBRST(value) ((tVIF_FBRST)(value))

union tVIF_ERR {
	struct {
		u32 MII : 1; // Masks Stat INT. (set to 1 to disable)
		u32 ME0 : 1; // Masks Stat Err0. (set to 1 to disable)
		u32 ME1 : 1; // Masks Stat Err1. (set to 1 to disable)
		u32 _reserved : 29;
	};
	u32 _u32;

	tVIF_ERR  (u32 val)					{ _u32 = val; }

	bool test		(u32 flags) const	{ return !!(_u32 & flags); }
	void set_flags	(u32 flags)			{ _u32 |=  flags; }
	void clear_flags(u32 flags)			{ _u32 &= ~flags; }
	void reset()						{ _u32 = 0; }
	wxString desc() const				{ return pxsFmt(L"Err: 0x%x", _u32); }
};

// --------------------------------------------------------------------------------------
//  tVIF_ADDR
// --------------------------------------------------------------------------------------
union tVIF_ADDR {
	struct {
		u32 ADDR : 10;
		u32 _reserved : 22;
	};
	u32 _u32;

	tVIF_ADDR  (u32 val)				{ _u32 = val; }

	void reset()						{ _u32 = 0; }
	wxString desc() const				{ return pxsFmt(L"0x%04x", ADDR); }
	
	tVIF_ADDR operator+( const tVIF_ADDR& src )	{ return ADDR + src.ADDR; }
	tVIF_ADDR operator-( const tVIF_ADDR& src )	{ return ADDR - src.ADDR; }
};

// --------------------------------------------------------------------------------------
//  tVIF_CODE
// --------------------------------------------------------------------------------------
union tVIF_CODE
{
	struct {
		union
		{
			struct  
			{
				// MODE field is available for STMOD codes only.
				u16 MODE	: 2;
				u16 _pad3	: 14;
			};

			struct  
			{
				// MODE field is available for STMOD codes only.
				u16 _pad2		: 15;
				u16 MASKPATH3	: 1;
			};
			
			struct 
			{
				// ADDR field is bits 0-9 (range 0 to 0x3ff), and is effective on specific
				// VIFcodes such as ITOP, BASE, and UNPACKs
				u16 ADDR	: 10;
				u16 _pad	: 4;

				// Unsigned unpack bit.  Indicates the unpack should use unsigned logic.
				// Effective for 8 bit and 16 bit UNPACKs only.
				u16 USN		: 1;

				// Address mode (valid on VIF1 UNPACK commands only)
				//  0 - Does not use VIF1_TOPS register.
				//  1 - Adds VIF1_TOPS register to ADDR
				u16	FLG		: 1;
			};

			// IMMEDIATE is bits 0-15 (range 0->0xffff), and is effective on specific
			// VIFcodes such as MSCAL, DIRECT, etc.
			u16	IMMEDIATE;
		};

		u8	NUM;

		// VIFcode command!  see vif_codes enum for valid values.
		u8	CMD		: 7;
		
		// If 1, the VIF generates an interrupt at the end of processing and stalls.
		u8	IBIT	: 1;
	};
	
	u32 _u32;
	
	tVIF_CODE(u32 val)				{ _u32 = val; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return wxsFormat(L"Code: 0x%x", _u32); }
};

struct vifCycle
{
	u8 cl, wl;
	u8 pad[2];
};

struct VIFregisters {
	tVIF_STAT		stat;
	u32 _pad0[3];
	u32				fbrst;
	u32 _pad1[3];
	tVIF_ERR		err;
	u32 _pad2[3];
	u32				mark;
	u32 _pad3[3];
	vifCycle		cycle; //data write cycle
	u32 _pad4[3];
	u32				mode;
	u32 _pad5[3];

	// Note: Technically num is only a u8, however for efficiency purposes PCSX2 uses the upper
	// bits for representing 256 (which is '0' on the PS2).  The vifRead handler masks off the
	// top bit to ensure games get upper 24 its zeroed (which is what they expect).
	// [Ps2Confirm] We really have no idea how this register should behave when doing large
	//   transfers via DIRECT/HL.  Its only 8 bits, and DIRECT/HL have a 16-bit immediate.  A
	//   test should be written that starts a fragmented VIF unpack and then reads back NUM
	//   at multiple stages.
	u32				num;
	u32  pad6[3];

	u32				mask;
	u32 _pad7[3];
	tVIF_CODE		code;
	u32 _pad8[3];
	tVIF_ADDR		itops;
	u32 _pad9[3];
	tVIF_ADDR		base;      // Not used in VIF0
	u32 _pad10[3];
	tVIF_ADDR		ofst;      // Not used in VIF0
	u32 _pad11[3];
	tVIF_ADDR		tops;      // Not used in VIF0
	u32 _pad12[3];
	tVIF_ADDR		itop;
	u32 _pad13[3];
	tVIF_ADDR		top;       // Not used in VIF0
	u32 _pad14[3];
	u32 _reserved;
	u32 _pad15[3];
	u32				r0;        // row0 register
	u32 _pad16[3];
	u32				r1;        // row1 register
	u32 _pad17[3];
	u32				r2;        // row2 register
	u32 _pad18[3];
	u32				r3;        // row3 register
	u32 _pad19[3];
	u32				c0;        // col0 register
	u32 _pad20[3];
	u32				c1;        // col1 register
	u32 _pad21[3];
	u32				c2;        // col2 register
	u32 _pad22[3];
	u32				c3;        // col3 register
	u32 _pad23[3];


	bool IsStalled()
	{
		return stat.VSS || stat.VIS || stat.VFS || stat.INT;
	}


	// BEGIN PCSX2 internal hooplah (used by legacy VIF/dma only)

	u32 mskpath3;
	u32 offset;
	u32 addr;
};

// --------------------------------------------------------------------------------------
//  VifProcessingUnit
// --------------------------------------------------------------------------------------
struct __aligned16 VifProcessingUnit
{
	u128 MaskRow;
	u128 MaskCol;
	
	// Partial Packet Buffer.
	// Primary use: if for concatenating fragmented UNPACK packets into complete packets
	// prior to processing.
	//
	// Secondary use: Some commands have parameters, such as STROW and STCOL.  This data is
	// only 32-bit aligned and can span QWC boundaries, so we need a place to queue up the
	// data until a fully complete packet is received.
	u128 buffer[256];
	uint running_idx;

	#ifdef PCSX2_DEVBUILD
	uint idx;				// VIF0 or VIF1, that is the question answered here.
	#endif

	const u32* data;
	u32 fragment_size;		// size of the incoming DMA packet fragment, in 32 bit units.
	u32 packet_size;		// total expected size of the VIFcode payload, in 32 bit units.
	u32 stallpos;			// 32-bit position in the last QWC loaded when VIF stalled.

	// Used by MPG and UNPACK commands; represents the target address.
	// (value is in 64-bit units for MPG, and in 128-bit units for UNPACK).
	u32 vu_target_addr;

	bool maskpath3;
	int cl;					// used in processing of UNPACK tags only.

	//template< uint idx, uint pass >
	//void DispatchCommand( u32* data, int size_qwc );
};

typedef void FnType_VifCmdHandler();
typedef FnType_VifCmdHandler* Fnptr_VifCmdHandler;

extern __aligned16 const Fnptr_VifCmdHandler vifCmdHandler[2][128];

extern __aligned16 VifProcessingUnit vifProc[2];

static VIFregisters& vif0Regs = (VIFregisters&)eeHw[0x3800];
static VIFregisters& vif1Regs = (VIFregisters&)eeHw[0x3C00];

#define _vifT			template <int idx>
#define  GetVifXregs	(idx ? (vif1Regs)	: (vif0Regs))

extern void vifReset();

_vifT extern size_t vifTransfer(const u128* data, int size);
_vifT extern u32 vifRead32(u32 mem);
_vifT extern bool vifWrite32(u32 mem, u32 value);

extern void VifUnpackSetMasks(VifProcessingUnit& vpu, const VIFregisters& v);
