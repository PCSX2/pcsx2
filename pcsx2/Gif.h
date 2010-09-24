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

union tGIF_P3TAG;
union tGIF_TAG0;

enum gifstate_t
{
	GIF_STATE_READY = 0,
	GIF_STATE_STALL = 1,
	GIF_STATE_DONE = 2,
	GIF_STATE_EMPTY = 0x10
};

enum gif_stat_flg
{
	GIF_FLG_PACKED	= 0,
	GIF_FLG_REGLIST	= 1,
	GIF_FLG_IMAGE	= 2,
	GIF_FLG_IMAGE2	= 3
};

union tGSTransferStatus {
	struct {
		u32 PTH1 : 4; // Resets Vif(0/1) when written.
		u32 PTH2 : 4; // Causes a Forcebreak to Vif((0/1) when true. (Stall)
		u32 PTH3 : 4; // Stops after the end of the Vifcode in progress when true. (Stall)
		u32 reserved : 20;
	};
	u32 _u32;

	tGSTransferStatus(u32 val)			{ _u32 = val; }
	bool test		(u32 flags) const	{ return !!(_u32 & flags); }
	void set_flags	(u32 flags)			{ _u32 |=  flags; }
	void clear_flags(u32 flags)			{ _u32 &= ~flags; }
	void reset()						{ _u32 = 0; }
	wxString desc() const				{ return pxsFmt(L"GSTransferStatus.PTH3: 0x%x", _u32); }
};
//GIF_STAT
enum gif_stat_flags
{
	GIF_STAT_M3R		= (1),		// GIF_MODE Mask
	GIF_STAT_M3P		= (1<<1),	// VIF PATH3 Mask
	GIF_STAT_IMT		= (1<<2),	// Intermittent Transfer Mode
	GIF_STAT_PSE		= (1<<3),	// Temporary Transfer Stop
	GIF_STAT_IP3		= (1<<5),	// Interrupted PATH3
	GIF_STAT_P3Q		= (1<<6),	// PATH3 request Queued
	GIF_STAT_P2Q		= (1<<7),	// PATH2 request Queued
	GIF_STAT_P1Q		= (1<<8),	// PATH1 request Queued
	GIF_STAT_OPH		= (1<<9),	// Output Path (Outputting Data)
	GIF_STAT_APATH1		= (1<<10),	// Data Transfer Path 1 (In progress)
	GIF_STAT_APATH2		= (2<<10),	// Data Transfer Path 2 (In progress)
	GIF_STAT_APATH3		= (3<<10),	// Data Transfer Path 3 (In progress) (Mask too)
	GIF_STAT_DIR		= (1<<12),	// Transfer Direction
	GIF_STAT_FQC		= (31<<24)	// QWC in GIF-FIFO
};

enum gif_mode_flags
{
	GIF_MODE_M3R	= (1),
	GIF_MODE_IMT	= (1<<2)
};

union tGIF_CTRL
{
	struct
	{
		u32 RST : 1;
		u32 _reserved1 : 2;
		u32 PSE : 1;
		u32 _reserved2 : 28;
	};
	u32 _u32;

	tGIF_CTRL(u32 val) { _u32 = val; }

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return pxsFmt(L"Ctrl: 0x%x", _u32); }
};

union tGIF_MODE
{
	struct
	{
		// PATH3 Mask Register.
		//  0 - Unmasked (transfer allowed)
		//  1 - masked (transfer disabled)
		u32 M3R : 1;
		u32 _reserved1 : 1;
		u32 IMT : 1;
		u32 _reserved2 : 29;
	};
	u32 _u32;

	tGIF_MODE(u32 val) { _u32 = val; }

	void write(u32 val)				{ _u32 = val; }
	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return pxsFmt(L"Mode: 0x%x", _u32); }
};

enum gif_active_path
{
    GIF_APATH_IDLE = 0,
    GIF_APATH1,
    GIF_APATH2,
    GIF_APATH3
};

union tGIF_STAT
{
	struct
	{
		u32 M3R : 1;
		u32 M3P : 1;
		u32 IMT : 1;
		u32 PSE : 1;
		u32 _reserved1 : 1;
		
		// Interrupted PATH3 status?
		//  Set to 1 when PATH3 is interrupted midst an IMAGE transfer (arbitration granted to
		//    either PATH1 or PATH2).
		u32 IP3 : 1;

		u32 P3Q : 1;		// PATH3 transfer request in the queue?
		u32 P2Q : 1;		// PATH2 transfer request int he queue?
		u32 P1Q : 1;		// PATH1 transfer request in the queue?
		u32 OPH : 1;
		
		// Indicates the currently active GIF path, either "idle", 1, 2, or 3.  See enum
		// gif_active_path for valid values.
		u32 APATH : 2;

		u32 DIR : 1;
		u32 _reserved2 : 11;
		u32 FQC : 5;		// Amount of data in the FIFO (0 to 16 QWC)
		u32 _reserved3 : 3;
	};
	u32 _u32;

	tGIF_STAT(u32 val) { _u32 = val; }

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return pxsFmt(L"Stat: 0x%x", _u32); }

	// Sets the active GIF path (path that has arbitration right over the GIFpath).
	// The OPH flag (output path active) is updated along with APATH (set false if the
	// APATH is IDLE, true for any other condition).
	void SetActivePath( gif_active_path path )
	{
		APATH	= path;
		OPH		= !(path == GIF_APATH_IDLE);
	}
};

union tGIF_TAG0
{
	struct
	{
		u32 NLOOP : 15;
		u32 EOP : 1;
		u32 TAG : 16;
	};
	u32 _u32;

	tGIF_TAG0(u32 val) { _u32 = val; }
	inline tGIF_TAG0(const tGIF_P3TAG& val);

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return wxsFormat(L"Tag0: 0x%x", _u32); }
};

union tGIF_TAG1
{
	struct
	{
		u32 TAG		: 14;
		u32 PRE		: 1;
		u32 PRIM	: 11;
		u32 FLG		: 2;
		u32 NREG	: 4;
	};
	u32 _u32;

	tGIF_TAG1(u32 val) { _u32 = val; }

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return wxsFormat(L"Tag1: 0x%x", _u32); }
};

union tGIF_CNT
{
	struct
	{
		u32 LOOPCNT : 15;
		u32 _reserved1 : 1;
		u32 REGCNT : 4;
		u32 VUADDR : 2;
		u32 _reserved2 : 10;

	};
	u32 _u32;

	tGIF_CNT(u32 val) { _u32 = val; }

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return wxsFormat(L"CNT: 0x%x", _u32); }
};

union tGIF_P3CNT
{
	struct
	{
		u32 P3CNT : 15;
		u32 _reserved1 : 17;
	};
	u32 _u32;

	tGIF_P3CNT(u32 val) { _u32 = val; }

	void reset()				{ _u32 = 0; }
	wxString desc() const		{ return wxsFormat(L"P3CNT: 0x%x", _u32); }
};

union tGIF_P3TAG
{
	struct
	{
		u32 LOOPCNT : 15;
		u32 EOP : 1;
		u32 _reserved1 : 16;
	};
	u32 _u32;

	tGIF_P3TAG(u32 val) { _u32 = val; }
	inline tGIF_P3TAG(const tGIF_TAG0& src);

	bool test(u32 flags) const		{ return !!(_u32 & flags); }
	void set_flags(u32 flags)		{ _u32 |= flags; }
	void clear_flags(u32 flags)		{ _u32 &= ~flags; }
	void reset()					{ _u32 = 0; }
	wxString desc() const			{ return wxsFormat(L"P3Tag: 0x%x", _u32); }
};

tGIF_TAG0::tGIF_TAG0(const tGIF_P3TAG& src) { _u32 = src._u32; }
tGIF_P3TAG::tGIF_P3TAG(const tGIF_TAG0& src) { _u32 = src._u32; }

// --------------------------------------------------------------------------------------
//  GIFregisters
// --------------------------------------------------------------------------------------
struct GIFregisters
{
	tGIF_CTRL 	ctrl;
	u32 _padding[3];
	tGIF_MODE 	mode;
	u32 _padding1[3];
	tGIF_STAT	stat;
	u32 _padding2[7];

	tGIF_TAG0	tag0;
	u32 _padding3[3];
	tGIF_TAG1	tag1;
	u32 _padding4[3];
	u32			tag2;
	u32 _padding5[3];
	u32			tag3;
	u32 _padding6[3];

	tGIF_CNT	cnt;
	u32 _padding7[3];
	tGIF_P3CNT	p3cnt;
	u32 _padding8[3];
	tGIF_P3TAG	p3tag;
	u32 _padding9[3];

	// Sets the active GIF path (path that has arbitration right over the GIFpath).
	// The OPH flag (output path active) is updated along with APATH (set false if the
	// APATH is IDLE, true for any other condition).
	void SetActivePath( gif_active_path path )
	{
		stat.SetActivePath(path);
	}

	gif_active_path GetActivePath() const
	{
		return (gif_active_path)stat.APATH;
	}
	
	bool HasPendingPaths() const
	{
		// gifRegs.stat.P1Q || gifRegs.stat.P2Q || gifRegs.stat.P3Q || gifRegs.stat.IP3

		return (stat._u32 & (0x7<<5)) != 0;		// Translation: Bits 5-8!
	}
};

namespace Exception {
	class SignalDoubleIMR {};
}

static GIFregisters& gifRegs = (GIFregisters&)eeHw[0x3000];

enum GIF_PathQueueResult
{
	GIFpath_Acquired,
	GIFpath_Queued,
	GIFpath_Busy,
};

extern bool GIF_InterruptPath3( gif_active_path apath );
extern bool __fastcall GIF_QueuePath1( u32 addr );
extern GIF_PathQueueResult GIF_QueuePath2();
extern GIF_PathQueueResult GIF_QueuePath3();

