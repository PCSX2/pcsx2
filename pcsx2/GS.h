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

#include "Common.h"
#include "Gif.h"
#include "GS/GS.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

extern double GetVerticalFrequency();
alignas(16) extern u8 g_RealGSMem[Ps2MemSize::GSregs];

enum CSR_FifoState
{
	CSR_FIFO_NORMAL = 0, // Somwhere in between (Neither empty or almost full).
	CSR_FIFO_EMPTY, // Empty
	CSR_FIFO_FULL, // Almost Full
	CSR_FIFO_RESERVED // Reserved / Unused.
};

// --------------------------------------------------------------------------------------
//  tGS_CSR
// --------------------------------------------------------------------------------------
// This is the Control Register for the GS.  It is a dual-instance register that returns
// distinctly different values for most fields when read and written.  In PCSX2 we house
// the written version in the gsRegs buffer, and generate the readback version on-demand
// from various other PCSX2 system statuses.
union tGS_CSR
{
	struct
	{
		// Write:
		//   0 - No action;
		//   1 - Old event is cleared and event is enabled.
		// Read:
		//   0 - No SIGNAL pending.
		//   1 - SIGNAL has been generated.
		u64 SIGNAL : 1;

		// Write:
		//   0 - No action;
		//   1 - FINISH event is enabled.
		// Read:
		//   0 - No FINISH event pending.
		//   1 - FINISH event has been generated.
		u64 FINISH : 1;

		// Hsync Interrupt Control
		// Write:
		//   0 - No action;
		//   1 - Hsync interrupt is enabled.
		// Read:
		//   0 - No Hsync interrupt pending.
		//   1 - Hsync interrupt has been generated.
		u64 HSINT : 1;

		// Vsync Interrupt Control
		// Write:
		//   0 - No action;
		//   1 - Vsync interrupt is enabled.
		// Read:
		//   0 - No Vsync interrupt pending.
		//   1 - Vsync interrupt has been generated.
		u64 VSINT : 1;

		// Rect Area Write Termination Control
		//   0 - No action;
		//   1 - Rect area write interrupt is enabled.
		// Read:
		//   0 - No RAWrite interrupt pending.
		//   1 - RAWrite interrupt has been generated.
		u64 EDWINT : 1;

		u64 _zero1 : 1;
		u64 _zero2 : 1;
		u64 pad1 : 1;

		// FLUSH  (write-only!)
		// Write:
		//   0 - Resume drawing if suspended (?)
		//   1 - Flush the GS FIFO and suspend drawing
		// Read: Always returns 0. (?)
		u64 FLUSH : 1;

		// RESET (write-only!)
		// Write:
		//   0 - Do nothing.
		//   1 - GS soft system reset.  Clears FIFOs and resets IMR to all 1's.
		//       (PCSX2 implementation also clears GIFpaths, though that behavior may differ on real HW).
		// Read: Always returns 0. (?)
		u64 RESET : 1;

		u64 _pad2 : 2;

		// (I have no idea what this reg is-- air)
		// Output value is updated by sampling the VSync. (?)
		u64 NFIELD : 1;

		// Current Field of Display [page flipping] (read-only?)
		//  0 - EVEN
		//  1 - ODD
		u64 FIELD : 1;

		// GS FIFO Status (read-only)
		//  00 - Somewhere in between
		//  01 - Empty
		//  10 - Almost Full
		//  11 - Reserved (unused)
		// Assign values using the CSR_FifoState enum.
		u64 FIFO : 2;

		// Revision number of the GS (fairly arbitrary)
		u64 REV : 8;

		// ID of the GS (also fairly arbitrary)
		u64 ID : 8;
	};

	u64 _u64;

	struct
	{
		u32 _u32; // lower 32 bits (all useful content!)
		u32 _unused32; // upper 32 bits (unused -- should probably be 0)
	};

	void SwapField()
	{
		_u32 ^= 0x2000;
	}

	void SetField()
	{
		_u32 |= 0x2000;
	}

	void Reset()
	{
		_u64 = 0;
		FIFO = CSR_FIFO_EMPTY;
		REV = 0x1B; // GS Revision
		ID = 0x55; // GS ID
	}

	bool HasAnyInterrupts() const { return (SIGNAL || FINISH || HSINT || VSINT || EDWINT); }

	u32 GetInterruptMask() const
	{
		return _u32 & 0x1f;
	}

	void SetAllInterrupts(bool value = true)
	{
		SIGNAL = FINISH = HSINT = VSINT = EDWINT = value;
	}

	tGS_CSR(u64 val) { _u64 = val; }
	tGS_CSR(u32 val) { _u32 = val; }
	tGS_CSR() { Reset(); }
};

// --------------------------------------------------------------------------------------
//  tGS_IMR
// --------------------------------------------------------------------------------------
union tGS_IMR
{
	struct
	{
		u32 _reserved1 : 8;
		u32 SIGMSK : 1; // Signal evevnt interrupt mask
		u32 FINISHMSK : 1; // Finish event interrupt mask
		u32 HSMSK : 1; // HSync interrupt mask
		u32 VSMSK : 1; // VSync interrupt mask
		u32 EDWMSK : 1; // Rectangle write termination interrupt mask
		u32 _undefined : 2; // undefined bits should be set to 1.
		u32 _reserved2 : 17;
	};
	u32 _u32;

	void reset()
	{
		_u32 = 0;
		SIGMSK = FINISHMSK = HSMSK = VSMSK = EDWMSK = true;
		_undefined = 0x3;
	}
	void set(u32 value)
	{
		_u32 = (value & 0x1f00); // Set only the interrupt mask fields.
		_undefined = 0x3; // These should always be set.
	}

	bool masked() const { return (SIGMSK || FINISHMSK || HSMSK || VSMSK || EDWMSK); }
};

// --------------------------------------------------------------------------------------
//  GSRegSMODE1
// --------------------------------------------------------------------------------------
// Previously, the union was used to get the CMOD bit of the SMODE1 register
// Commenting it out as it's unused right now. (Might potentially be useful in the future)
//union GSRegSMODE1
//{
//	struct
//	{
//		u32 RC : 3;
//		u32 LC : 7;
//		u32 T1248 : 2;
//		u32 SLCK : 1;
//		u32 CMOD : 2;
//		u32 EX : 1;
//		u32 PRST : 1;
//		u32 SINT : 1;
//		u32 XPCK : 1;
//		u32 PCK2 : 2;
//		u32 SPML : 4;
//		u32 GCONT : 1;
//		u32 PHS : 1;
//		u32 PVS : 1;
//		u32 PEHS : 1;
//		u32 PEVS : 1;
//		u32 CLKSEL : 2;
//		u32 NVCK : 1;
//		u32 SLCK2 : 1;
//		u32 VCKSEL : 2;
//		u32 VHP : 1;
//		u32 _PAD1 : 27;
//	};
//
//	u64 SMODE1;
//};

// --------------------------------------------------------------------------------------
//  GSRegSIGBLID
// --------------------------------------------------------------------------------------
struct GSRegSIGBLID
{
	u32 SIGID;
	u32 LBLID;
};

#define PS2MEM_GS g_RealGSMem
#define PS2GS_BASE(mem) (PS2MEM_GS + (mem & 0x13ff))

#define CSRreg ((tGS_CSR&)*(PS2MEM_GS + 0x1000))

#define GSCSRr ((u32&)*(PS2MEM_GS + 0x1000))
#define GSIMR ((tGS_IMR&)*(PS2MEM_GS + 0x1010))
#define GSSIGLBLID ((GSRegSIGBLID&)*(PS2MEM_GS + 0x1080))

enum class GS_VideoMode : int
{
	Uninitialized,
	Unknown,
	NTSC,
	PAL,
	VESA,
	SDTV_480P,
	SDTV_576P,
	HDTV_720P,
	HDTV_1080I,
	HDTV_1080P,
	DVD_NTSC,
	DVD_PAL
};

extern GS_VideoMode gsVideoMode;
extern bool gsIsInterlaced;

/////////////////////////////////////////////////////////////////////////////
// MTGS Threaded Class Declaration

// Uncomment this to enable the MTGS debug stack, which tracks to ensure reads
// and writes stay synchronized.  Warning: the debug stack is VERY slow.
//#define RINGBUF_DEBUG_STACK

enum MTGS_RingCommand
{
	GS_RINGTYPE_P1,
	GS_RINGTYPE_P2,
	GS_RINGTYPE_P3,
	GS_RINGTYPE_VSYNC,
	GS_RINGTYPE_FRAMESKIP,
	GS_RINGTYPE_FREEZE,
	GS_RINGTYPE_RESET, // issues a GSreset() command.
	GS_RINGTYPE_SOFTRESET, // issues a soft reset for the GIF
	GS_RINGTYPE_CRC,
	GS_RINGTYPE_GSPACKET,
	GS_RINGTYPE_MTVU_GSPACKET,
	GS_RINGTYPE_INIT_AND_READ_FIFO,
	GS_RINGTYPE_ASYNC_CALL,
};


struct MTGS_FreezeData
{
	freezeData* fdata;
	s32 retval; // value returned from the call, valid only after an mtgsWaitGS()
};

struct MTGS_MemoryScreenshotData
{
	u32 width = 0;
	u32 height = 0;
	std::vector<u32> pixels; // width * height
	bool success = false;
};

// --------------------------------------------------------------------------------------
//  SysMtgsThread
// --------------------------------------------------------------------------------------
class SysMtgsThread
{
public:
	using AsyncCallType = std::function<void()>;

	// note: when m_ReadPos == m_WritePos, the fifo is empty
	// Threading info: m_ReadPos is updated by the MTGS thread. m_WritePos is updated by the EE thread
	std::atomic<unsigned int> m_ReadPos; // cur pos gs is reading from
	std::atomic<unsigned int> m_WritePos; // cur pos ee thread is writing to

	std::atomic<bool> m_SignalRingEnable;
	std::atomic<int> m_SignalRingPosition;

	std::atomic<int> m_QueuedFrameCount;
	std::atomic<bool> m_VsyncSignalListener;

	std::mutex m_mtx_RingBufferBusy2; // Gets released on semaXGkick waiting...
	std::mutex m_mtx_WaitGS;
	Threading::WorkSema m_sem_event;
	Threading::KernelSemaphore m_sem_OnRingReset;
	Threading::KernelSemaphore m_sem_Vsync;

	// used to keep multiple threads from sending packets to the ringbuffer concurrently.
	// (currently not used or implemented -- is a planned feature for a future threaded VU1)
	//MutexLockRecursive m_PacketLocker;

	// Used to delay the sending of events.  Performance is better if the ringbuffer
	// has more than one command in it when the thread is kicked.
	int m_CopyDataTally;

	// These vars maintain instance data for sending Data Packets.
	// Only one data packet can be constructed and uploaded at a time.

	uint m_packet_startpos; // size of the packet (data only, ie. not including the 16 byte command!)
	uint m_packet_size; // size of the packet (data only, ie. not including the 16 byte command!)
	uint m_packet_writepos; // index of the data location in the ringbuffer.

#ifdef RINGBUF_DEBUG_STACK
	std::mutex m_lock_Stack;
#endif

	std::thread m_thread;
	Threading::ThreadHandle m_thread_handle;
	std::atomic_bool m_open_flag{false};
	std::atomic_bool m_shutdown_flag{false};
	Threading::KernelSemaphore m_open_or_close_done;

public:
	SysMtgsThread();
	virtual ~SysMtgsThread();

	__fi const Threading::ThreadHandle& GetThreadHandle() const { return m_thread_handle; }
	__fi bool IsOpen() const { return m_open_flag.load(std::memory_order_acquire); }

	/// Starts the thread, if it hasn't already been started.
	void StartThread();

	/// Fully stops the thread, closing in the process if needed.
	void ShutdownThread();

	// Waits for the GS to empty out the entire ring buffer contents.
	void WaitGS(bool syncRegs=true, bool weakWait=false, bool isMTVU=false);
	void ResetGS(bool hardware_reset);

	void PrepDataPacket(MTGS_RingCommand cmd, u32 size);
	void PrepDataPacket(GIF_PATH pathidx, u32 size);
	void SendDataPacket();
	void SendGameCRC(u32 crc);
	bool WaitForOpen();
	void WaitForClose();
	void Freeze(FreezeAction mode, MTGS_FreezeData& data);

	void SendSimpleGSPacket(MTGS_RingCommand type, u32 offset, u32 size, GIF_PATH path);
	void SendSimplePacket(MTGS_RingCommand type, int data0, int data1, int data2);
	void SendPointerPacket(MTGS_RingCommand type, u32 data0, void* data1);

	u8* GetDataPacketPtr() const;
	void SetEvent();
	void PostVsyncStart(bool registers_written);
	void InitAndReadFIFO(u8* mem, u32 qwc);

	void RunOnGSThread(AsyncCallType func);
	void ApplySettings();
	void ResizeDisplayWindow(int width, int height, float scale);
	void UpdateDisplayWindow();
	void SetVSync(VsyncMode mode);
	void SwitchRenderer(GSRendererType renderer, bool display_message = true);
	void SetSoftwareRendering(bool software, bool display_message = true);
	void ToggleSoftwareRendering();
	bool SaveMemorySnapshot(u32 width, u32 height, std::vector<u32>* pixels);

protected:
	bool TryOpenGS();
	void CloseGS();

	void ThreadEntryPoint();
	void MainLoop();

	void GenericStall(uint size);

	// Used internally by SendSimplePacket type functions
	void _FinishSimplePacket();
};

// GetMTGS() is a required external implementation. This function is *NOT* provided
// by the PCSX2 core library.  It provides an interface for the linking User Interface
// apps or DLLs to reference their own instance of SysMtgsThread (also allowing them
// to extend the class and override virtual methods).
//
extern SysMtgsThread& GetMTGS();

/////////////////////////////////////////////////////////////////////////////
// Generalized GS Functions and Stuff

extern s32 gsOpen();
extern void gsClose();
extern void gsReset();
extern void gsSetVideoMode(GS_VideoMode mode);
extern void gsResetFrameSkip();
extern void gsPostVsyncStart();
extern void gsFrameSkip();
extern bool gsIsSkippingCurrentFrame();
extern void gsUpdateFrequency(Pcsx2Config& config);

// Some functions shared by both the GS and MTGS
extern void _gs_ResetFrameskip();

extern void gsWrite8(u32 mem, u8 value);
extern void gsWrite16(u32 mem, u16 value);
extern void gsWrite32(u32 mem, u32 value);

extern void gsWrite64_page_00(u32 mem, const mem64_t* value);
extern void gsWrite64_page_01(u32 mem, const mem64_t* value);
extern void gsWrite64_generic(u32 mem, const mem64_t* value);

extern void gsWrite128_page_00(u32 mem, const mem128_t* value);
extern void gsWrite128_page_01(u32 mem, const mem128_t* value);
extern void gsWrite128_generic(u32 mem, const mem128_t* value);

extern u8 gsRead8(u32 mem);
extern u16 gsRead16(u32 mem);
extern u32 gsRead32(u32 mem);
extern u64 gsRead64(u32 mem);
extern u128 gsNonMirroredRead(u32 mem);

void gsIrq();

extern tGS_CSR CSRr;

// Size of the ringbuffer as a power of 2 -- size is a multiple of simd128s.
// (actual size is 1<<m_RingBufferSizeFactor simd vectors [128-bit values])
// A value of 19 is a 8meg ring buffer.  18 would be 4 megs, and 20 would be 16 megs.
// Default was 2mb, but some games with lots of MTGS activity want 8mb to run fast (rama)
static const uint RingBufferSizeFactor = 19;

// size of the ringbuffer in simd128's.
static const uint RingBufferSize = 1 << RingBufferSizeFactor;

// Mask to apply to ring buffer indices to wrap the pointer from end to
// start (the wrapping is what makes it a ringbuffer, yo!)
static const uint RingBufferMask = RingBufferSize - 1;

struct MTGS_BufferedData
{
	u128 m_Ring[RingBufferSize];
	u8 Regs[Ps2MemSize::GSregs];

	MTGS_BufferedData() {}

	u128& operator[](uint idx)
	{
		pxAssert(idx < RingBufferSize);
		return m_Ring[idx];
	}
};

alignas(32) extern MTGS_BufferedData RingBuffer;

// FIXME: These belong in common with other memcpy tools.  Will move them there later if no one
// else beats me to it.  --air
inline void MemCopy_WrappedDest(const u128* src, u128* destBase, uint& destStart, uint destSize, uint len)
{
	uint endpos = destStart + len;
	if (endpos < destSize)
	{
		memcpy(&destBase[destStart], src, len * 16);
		destStart += len;
	}
	else
	{
		uint firstcopylen = destSize - destStart;
		memcpy(&destBase[destStart], src, firstcopylen * 16);
		destStart = endpos % destSize;
		memcpy(destBase, src + firstcopylen, destStart * 16);
	}
}

inline void MemCopy_WrappedSrc(const u128* srcBase, uint& srcStart, uint srcSize, u128* dest, uint len)
{
	uint endpos = srcStart + len;
	if (endpos < srcSize)
	{
		memcpy(dest, &srcBase[srcStart], len * 16);
		srcStart += len;
	}
	else
	{
		uint firstcopylen = srcSize - srcStart;
		memcpy(dest, &srcBase[srcStart], firstcopylen * 16);
		srcStart = endpos % srcSize;
		memcpy(dest + firstcopylen, srcBase, srcStart * 16);
	}
}
