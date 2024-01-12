// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS.h"
#include "Gif_Unit.h"
#include "MTGS.h"
#include "MTVU.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "VMManager.h"

#include "common/FPControl.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/WrappedMemCopy.h"

#include <list>
#include <mutex>
#include <thread>

// Uncomment this to enable profiling of the GS RingBufferCopy function.
//#define PCSX2_GSRING_SAMPLING_STATS

#if 0 //PCSX2_DEBUG
#define MTGS_LOG Console.WriteLn
#else
#define MTGS_LOG(...) \
	do                \
	{                 \
	} while (0)
#endif

namespace MTGS
{
	struct BufferedData
	{
		u128 m_Ring[RingBufferSize];
		u8 Regs[Ps2MemSize::GSregs];

		u128& operator[](uint idx)
		{
			pxAssert(idx < RingBufferSize);
			return m_Ring[idx];
		}
	};

	static void ThreadEntryPoint();
	static void MainLoop();

	static void GenericStall(uint size);

	static void PrepDataPacket(Command cmd, u32 size);
	static void PrepDataPacket(GIF_PATH pathidx, u32 size);
	static void SendDataPacket();

	static void SendSimplePacket(Command type, int data0, int data1, int data2);
	static void SendSimpleGSPacket(Command type, u32 offset, u32 size, GIF_PATH path);
	static void SendPointerPacket(Command type, u32 data0, void* data1);
	static void _FinishSimplePacket();
	static u8* GetDataPacketPtr();

	static void SetEvent();

	alignas(32) BufferedData RingBuffer;

	// note: when m_ReadPos == m_WritePos, the fifo is empty
	// Threading info: m_ReadPos is updated by the MTGS thread. m_WritePos is updated by the EE thread
	alignas(64) static std::atomic<unsigned int> s_ReadPos; // cur pos gs is reading from
	alignas(64) static std::atomic<unsigned int> s_WritePos; // cur pos ee thread is writing to

	// These vars maintain instance data for sending Data Packets.
	// Only one data packet can be constructed and uploaded at a time.
	static u32 s_packet_startpos; // size of the packet (data only, ie. not including the 16 byte command!)
	static u32 s_packet_size; // size of the packet (data only, ie. not including the 16 byte command!)
	static u32 s_packet_writepos; // index of the data location in the ringbuffer.

	static std::atomic<bool> s_SignalRingEnable;
	static std::atomic<int> s_SignalRingPosition;

	static std::atomic<int> s_QueuedFrameCount;
	static std::atomic<bool> s_VsyncSignalListener;

	static std::mutex s_mtx_RingBufferBusy2; // Gets released on semaXGkick waiting...
	static Threading::WorkSema s_sem_event;
	static Threading::UserspaceSemaphore s_sem_OnRingReset;
	static Threading::UserspaceSemaphore s_sem_Vsync;

	// Used to delay the sending of events.  Performance is better if the ringbuffer
	// has more than one command in it when the thread is kicked.
	static int s_CopyDataTally;

#ifdef RINGBUF_DEBUG_STACK
	static std::mutex s_lock_Stack;
	static std::list<uint> ringposStack;
#endif

	static Threading::Thread s_thread;
	static std::atomic_bool s_open_flag{false};
	static std::atomic_bool s_shutdown_flag{false};
	static std::atomic_bool s_run_idle_flag{false};
	static Threading::UserspaceSemaphore s_open_or_close_done;
} // namespace MTGS

// =====================================================================================================
//  MTGS Threaded Class Implementation
// =====================================================================================================

const Threading::ThreadHandle& MTGS::GetThreadHandle()
{
	return s_thread;
}

bool MTGS::IsOpen()
{
	return s_open_flag.load(std::memory_order_acquire);
}

void MTGS::StartThread()
{
	if (s_thread.Joinable())
		return;

	pxAssertRel(!s_open_flag.load(), "GS thread should not be opened when starting");
	s_sem_event.Reset();
	s_shutdown_flag.store(false, std::memory_order_release);
	s_thread.Start(&MTGS::ThreadEntryPoint);
}

void MTGS::ShutdownThread()
{
	if (!s_thread.Joinable())
		return;

	// just go straight to shutdown, don't wait-for-open again
	s_shutdown_flag.store(true, std::memory_order_release);
	if (IsOpen())
		WaitForClose();

	// make sure the thread actually exits
	s_sem_event.NotifyOfWork();
	s_thread.Join();
}

void MTGS::ThreadEntryPoint()
{
	Threading::SetNameOfCurrentThread("GS");

	// Explicitly set rounding mode to default (nearest, FTZ off).
	// Otherwise it appears to get inherited from the EE thread on Linux.
	FPControlRegister::SetCurrent(FPControlRegister::GetDefault());

	for (;;)
	{
		// wait until we're actually asked to initialize (and config has been loaded, etc)
		while (!s_open_flag.load(std::memory_order_acquire))
		{
			if (s_shutdown_flag.load(std::memory_order_acquire))
			{
				s_sem_event.Kill();
				return;
			}

			s_sem_event.WaitForWork();
		}

		// try initializing.. this could fail
		std::memcpy(RingBuffer.Regs, PS2MEM_GS, sizeof(PS2MEM_GS));
		const bool opened = GSopen(EmuConfig.GS, EmuConfig.GS.Renderer, RingBuffer.Regs);
		s_open_flag.store(opened, std::memory_order_release);

		// notify emu thread that we finished opening (or failed)
		s_open_or_close_done.Post();

		// are we open?
		if (!opened)
		{
			// wait until we're asked to try again...
			continue;
		}

		// we're ready to go
		MainLoop();

		// when we come back here, it's because we closed (or shutdown)
		// that means the emu thread should be blocked, waiting for us to be done
		pxAssertRel(!s_open_flag.load(std::memory_order_relaxed), "Open flag is clear on close");
		GSclose();
		s_open_or_close_done.Post();

		// we need to reset sem_event here, because MainLoop() kills it.
		s_sem_event.Reset();
	}

	GSshutdown();
}

void MTGS::ResetGS(bool hardware_reset)
{
	// MTGS Reset process:
	//  * clear the ringbuffer.
	//  * Signal a reset.
	//  * clear the path and byRegs structs (used by GIFtagDummy)

	if (hardware_reset)
	{
		s_ReadPos = s_WritePos.load();
		s_QueuedFrameCount = 0;
		s_VsyncSignalListener = 0;
	}

	MTGS_LOG("MTGS: Sending Reset...");
	SendSimplePacket(Command::Reset, static_cast<int>(hardware_reset), 0, 0);

	if (hardware_reset)
		SetEvent();
}

struct RingCmdPacket_Vsync
{
	u8 regset1[0x0f0];
	u32 csr;
	u32 imr;
	GSRegSIGBLID siglblid;

	// must be 16 byte aligned
	u32 registers_written;
	u32 pad[3];
};

void MTGS::PostVsyncStart(bool registers_written)
{
	// Optimization note: Typically regset1 isn't needed.  The regs in that area are typically
	// changed infrequently, usually during video mode changes.  However, on modern systems the
	// 256-byte copy is only a few dozen cycles -- executed 60 times a second -- so probably
	// not worth the effort or overhead of trying to selectively avoid it.

	uint packsize = sizeof(RingCmdPacket_Vsync) / 16;
	PrepDataPacket(Command::VSync, packsize);
	MemCopy_WrappedDest((u128*)PS2MEM_GS, RingBuffer.m_Ring, s_packet_writepos, RingBufferSize, 0xf);

	u32* remainder = (u32*)GetDataPacketPtr();
	remainder[0] = GSCSRr;
	remainder[1] = GSIMR._u32;
	(GSRegSIGBLID&)remainder[2] = GSSIGLBLID;
	remainder[4] = static_cast<u32>(registers_written);
	s_packet_writepos = (s_packet_writepos + 2) & RingBufferMask;

	SendDataPacket();

	// Vsyncs should always start the GS thread, regardless of how little has actually be queued.
	if (s_CopyDataTally != 0)
		SetEvent();

	// If the MTGS is allowed to queue a lot of frames in advance, it creates input lag.
	// Use the Queued FrameCount to stall the EE if another vsync (or two) are already queued
	// in the ringbuffer.  The queue limit is disabled when both FrameLimiting and Vsync are
	// disabled, since the queue can have perverse effects on framerate benchmarking.

	// Edit: It's possible that MTGS is that much faster than GS that it creates so much lag,
	// a game becomes uncontrollable (software rendering for example).
	// For that reason it's better to have the limit always in place, at the cost of a few max FPS in benchmarks.
	// If those are needed back, it's better to increase the VsyncQueueSize via PCSX_vm.ini.
	// (The Xenosaga engine is known to run into this, due to it throwing bulks of data in one frame followed by 2 empty frames.)

	if ((s_QueuedFrameCount.fetch_add(1) < EmuConfig.GS.VsyncQueueSize) /*|| (!EmuConfig.GS.VsyncEnable && !EmuConfig.GS.FrameLimitEnable)*/)
		return;

	s_VsyncSignalListener.store(true, std::memory_order_release);
	//Console.WriteLn( Color_Blue, "(EEcore Sleep) Vsync\t\tringpos=0x%06x, writepos=0x%06x", m_ReadPos.load(), m_WritePos.load() );

	s_sem_Vsync.Wait();
}

void MTGS::InitAndReadFIFO(u8* mem, u32 qwc)
{
	if (EmuConfig.GS.HWDownloadMode >= GSHardwareDownloadMode::Unsynchronized && GSIsHardwareRenderer())
	{
		if (EmuConfig.GS.HWDownloadMode == GSHardwareDownloadMode::Unsynchronized)
			GSReadLocalMemoryUnsync(mem, qwc, vif1.BITBLTBUF._u64, vif1.TRXPOS._u64, vif1.TRXREG._u64);
		else
			std::memset(mem, 0, qwc * 16);

		return;
	}

	SendPointerPacket(Command::InitAndReadFIFO, qwc, mem);
	WaitGS(false, false, false);
}

union PacketTagType
{
	struct
	{
		u32 command;
		u32 data[3];
	};
	struct
	{
		u32 _command;
		u32 _data[1];
		uptr pointer;
	};
};

void MTGS::MainLoop()
{
	// Threading info: run in MTGS thread
	// m_ReadPos is only update by the MTGS thread so it is safe to load it with a relaxed atomic

#ifdef RINGBUF_DEBUG_STACK
	PacketTagType prevCmd;
#endif

	std::unique_lock mtvu_lock(s_mtx_RingBufferBusy2);

	while (true)
	{
		if (s_run_idle_flag.load(std::memory_order_acquire) && VMManager::GetState() != VMState::Running && GSHasDisplayWindow())
		{
			if (!s_sem_event.CheckForWork())
			{
				GSPresentCurrentFrame();
				GSThrottlePresentation();
			}
		}
		else
		{
			mtvu_lock.unlock();
			s_sem_event.WaitForWork();
			mtvu_lock.lock();
		}

		if (!s_open_flag.load(std::memory_order_acquire))
			break;

		// note: m_ReadPos is intentionally not volatile, because it should only
		// ever be modified by this thread.
		while (s_ReadPos.load(std::memory_order_relaxed) != s_WritePos.load(std::memory_order_acquire))
		{
			const unsigned int local_ReadPos = s_ReadPos.load(std::memory_order_relaxed);

			pxAssert(local_ReadPos < RingBufferSize);

			const PacketTagType& tag = (PacketTagType&)RingBuffer[local_ReadPos];
			u32 ringposinc = 1;

#ifdef RINGBUF_DEBUG_STACK
			// pop a ringpos off the stack.  It should match this one!

			s_lock_Stack.Lock();
			uptr stackpos = ringposStack.back();
			if (stackpos != local_ReadPos)
			{
				Console.Error("MTGS Ringbuffer Critical Failure ---> %x to %x (prevCmd: %x)\n", stackpos, local_ReadPos, prevCmd.command);
			}
			pxAssert(stackpos == local_ReadPos);
			prevCmd = tag;
			ringposStack.pop_back();
			s_lock_Stack.Release();
#endif

			switch (static_cast<Command>(tag.command))
			{
#if COPY_GS_PACKET_TO_MTGS == 1
				case Command::GIFPath1:
				{
					uint datapos = (local_ReadPos + 1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG("(MTGS Packet Read) ringtype=P1, qwc=%u", qsize);

					uint endpos = datapos + qsize;
					if (endpos >= RingBufferSize)
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer((u8*)data, firstcopylen);
						datapos = endpos & RingBufferMask;
						GSgifTransfer((u8*)RingBuffer.m_Ring, datapos);
					}
					else
					{
						GSgifTransfer((u8*)data, qsize);
					}

					ringposinc += qsize;
				}
				break;

				case Command::GIFPath2:
				{
					uint datapos = (local_ReadPos + 1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG("(MTGS Packet Read) ringtype=P2, qwc=%u", qsize);

					uint endpos = datapos + qsize;
					if (endpos >= RingBufferSize)
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer2((u32*)data, firstcopylen);
						datapos = endpos & RingBufferMask;
						GSgifTransfer2((u32*)RingBuffer.m_Ring, datapos);
					}
					else
					{
						GSgifTransfer2((u32*)data, qsize);
					}

					ringposinc += qsize;
				}
				break;

				case Command::GIFPath3:
				{
					uint datapos = (local_ReadPos + 1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG("(MTGS Packet Read) ringtype=P3, qwc=%u", qsize);

					uint endpos = datapos + qsize;
					if (endpos >= RingBufferSize)
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer3((u32*)data, firstcopylen);
						datapos = endpos & RingBufferMask;
						GSgifTransfer3((u32*)RingBuffer.m_Ring, datapos);
					}
					else
					{
						GSgifTransfer3((u32*)data, qsize);
					}

					ringposinc += qsize;
				}
				break;
#endif
				case Command::GSPacket:
				{
					Gif_Path& path = gifUnit.gifPath[tag.data[2]];
					u32 offset = tag.data[0];
					u32 size = tag.data[1];
					if (offset != ~0u)
						GSgifTransfer((u8*)&path.buffer[offset], size / 16);
					path.readAmount.fetch_sub(size, std::memory_order_acq_rel);
					break;
				}

				case Command::MTVUGSPacket:
				{
					MTVU_LOG("MTGS - Waiting on semaXGkick!");
					if (!vu1Thread.semaXGkick.TryWait())
					{
						mtvu_lock.unlock();
						// Wait for MTVU to complete vu1 program
						vu1Thread.semaXGkick.Wait();
						mtvu_lock.lock();
					}
					Gif_Path& path = gifUnit.gifPath[GIF_PATH_1];
					GS_Packet gsPack = path.GetGSPacketMTVU(); // Get vu1 program's xgkick packet(s)
					if (gsPack.size)
						GSgifTransfer((u8*)&path.buffer[gsPack.offset], gsPack.size / 16);
					path.readAmount.fetch_sub(gsPack.size + gsPack.readAmount, std::memory_order_acq_rel);
					path.PopGSPacketMTVU(); // Should be done last, for proper Gif_MTGS_Wait()
					break;
				}

				default:
				{
					switch (static_cast<Command>(tag.command))
					{
						case Command::VSync:
						{
							const int qsize = tag.data[0];
							ringposinc += qsize;

							MTGS_LOG("(MTGS Packet Read) ringtype=Vsync, field=%u, skip=%s", !!(((u32&)RingBuffer.Regs[0x1000]) & 0x2000) ? 0 : 1, tag.data[1] ? "true" : "false");

							// Mail in the important GS registers.
							// This seemingly obtuse system is needed in order to handle cases where the vsync data wraps
							// around the edge of the ringbuffer.  If not for that I'd just use a struct. >_<

							uint datapos = (local_ReadPos + 1) & RingBufferMask;
							MemCopy_WrappedSrc(RingBuffer.m_Ring, datapos, RingBufferSize, (u128*)RingBuffer.Regs, 0xf);

							u32* remainder = (u32*)&RingBuffer[datapos];
							((u32&)RingBuffer.Regs[0x1000]) = remainder[0];
							((u32&)RingBuffer.Regs[0x1010]) = remainder[1];
							((GSRegSIGBLID&)RingBuffer.Regs[0x1080]) = (GSRegSIGBLID&)remainder[2];

							// CSR & 0x2000; is the pageflip id.
							GSvsync((((u32&)RingBuffer.Regs[0x1000]) & 0x2000) ? 0 : 1, remainder[4] != 0);

							s_QueuedFrameCount.fetch_sub(1);
							if (s_VsyncSignalListener.exchange(false))
								s_sem_Vsync.Post();

							// Do not StateCheckInThread() here
							// Otherwise we could pause while there's still data in the queue
							// Which could make the MTVU thread wait forever for it to empty
						}
						break;

						case Command::AsyncCall:
							{
								AsyncCallType* const func = (AsyncCallType*)tag.pointer;
								(*func)();
								delete func;
							}
							break;

						case Command::Freeze:
						{
							MTGS::FreezeData* data = (MTGS::FreezeData*)tag.pointer;
							int mode = tag.data[0];
							data->retval = GSfreeze((FreezeAction)mode, (freezeData*)data->fdata);
						}
						break;

						case Command::Reset:
							MTGS_LOG("(MTGS Packet Read) ringtype=Reset");
							GSreset(tag.data[0] != 0);
							break;

						case Command::SoftReset:
						{
							int mask = tag.data[0];
							MTGS_LOG("(MTGS Packet Read) ringtype=SoftReset");
							GSgifSoftReset(mask);
						}
						break;

						case Command::InitAndReadFIFO:
							MTGS_LOG("(MTGS Packet Read) ringtype=Fifo2, size=%d", tag.data[0]);
							GSInitAndReadFIFO((u8*)tag.pointer, tag.data[0]);
							break;

#ifdef PCSX2_DEVBUILD
						default:
							Console.Error("GSThreadProc, bad packet (%x) at m_ReadPos: %x, m_WritePos: %x", tag.command, local_ReadPos, s_WritePos.load());
							pxFail("Bad packet encountered in the MTGS Ringbuffer.");
							s_ReadPos.store(s_WritePos.load(std::memory_order_acquire), std::memory_order_release);
							continue;
#else
							// Optimized performance in non-Dev builds.
							jNO_DEFAULT;
#endif
					}
				}
			}

			uint newringpos = (s_ReadPos.load(std::memory_order_relaxed) + ringposinc) & RingBufferMask;

			if (EmuConfig.GS.SynchronousMTGS)
			{
				pxAssert(s_WritePos == newringpos);
			}

			s_ReadPos.store(newringpos, std::memory_order_release);

			if (s_SignalRingEnable.load(std::memory_order_acquire))
			{
				// The EEcore has requested a signal after some amount of processed data.
				if (s_SignalRingPosition.fetch_sub(ringposinc) <= 0)
				{
					// Make sure to post the signal after the m_ReadPos has been updated...
					s_SignalRingEnable.store(false, std::memory_order_release);
					s_sem_OnRingReset.Post();
					continue;
				}
			}
		}

		// TODO: With the new race-free WorkSema do we still need these?

		// Safety valve in case standard signals fail for some reason -- this ensures the EEcore
		// won't sleep the eternity, even if SignalRingPosition didn't reach 0 for some reason.
		// Important: Need to unlock the MTGS busy signal PRIOR, so that EEcore SetEvent() calls
		// parallel to this handler aren't accidentally blocked.
		if (s_SignalRingEnable.exchange(false))
		{
			//Console.Warning( "(MTGS Thread) Dangling RingSignal on empty buffer!  signalpos=0x%06x", m_SignalRingPosition.exchange(0) ) );
			s_SignalRingPosition.store(0, std::memory_order_release);
			s_sem_OnRingReset.Post();
		}

		if (s_VsyncSignalListener.exchange(false))
			s_sem_Vsync.Post();

		//Console.Warning( "(MTGS Thread) Nothing to do!  ringpos=0x%06x", m_ReadPos );
	}

	// Unblock any threads in WaitGS in case MTGS gets cancelled while still processing work
	s_ReadPos.store(s_WritePos.load(std::memory_order_acquire), std::memory_order_relaxed);
	s_sem_event.Kill();
}

// Waits for the GS to empty out the entire ring buffer contents.
// If syncRegs, then writes pcsx2's gs regs to MTGS's internal copy
// If weakWait, then this function is allowed to exit after MTGS finished a path1 packet
// If isMTVU, then this implies this function is being called from the MTVU thread...
void MTGS::WaitGS(bool syncRegs, bool weakWait, bool isMTVU)
{
	pxAssertMsg(IsOpen(), "MTGS Warning!  WaitGS issued on a closed thread.");
	if (!IsOpen()) [[unlikely]]
		return;

	Gif_Path& path = gifUnit.gifPath[GIF_PATH_1];

	// Both m_ReadPos and m_WritePos can be relaxed as we only want to test if the queue is empty but
	// we don't want to access the content of the queue

	SetEvent();
	if (weakWait && isMTVU)
	{
		// On weakWait we will stop waiting on the MTGS thread if the
		// MTGS thread has processed a vu1 xgkick packet, or is pending on
		// its final vu1 xgkick packet (!curP1Packs)...
		// Note: m_WritePos doesn't seem to have proper atomic write
		// code, so reading it from the MTVU thread might be dangerous;
		// hence it has been avoided...
		u32 startP1Packs = path.GetPendingGSPackets();
		if (startP1Packs)
		{
			while (true)
			{
				// m_mtx_RingBufferBusy2.Wait();
				s_mtx_RingBufferBusy2.lock();
				s_mtx_RingBufferBusy2.unlock();
				if (path.GetPendingGSPackets() != startP1Packs)
					break;
			}
		}
	}
	else
	{
		if (!s_sem_event.WaitForEmpty())
			pxFailRel("MTGS Thread Died");
	}

	pxAssert(!(weakWait && syncRegs) && "No synchronization for this!");

	if (syncRegs)
	{
		// Completely synchronize GS and MTGS register states.
		memcpy(RingBuffer.Regs, PS2MEM_GS, sizeof(RingBuffer.Regs));
	}
}

// Sets the gsEvent flag and releases a timeslice.
// For use in loops that wait on the GS thread to do certain things.
void MTGS::SetEvent()
{
	s_sem_event.NotifyOfWork();
	s_CopyDataTally = 0;
}

u8* MTGS::GetDataPacketPtr()
{
	return (u8*)&RingBuffer[s_packet_writepos & RingBufferMask];
}

// Closes the data packet send command, and initiates the gs thread (if needed).
void MTGS::SendDataPacket()
{
	// make sure a previous copy block has been started somewhere.
	pxAssert(s_packet_size != 0);

	uint actualSize = ((s_packet_writepos - s_packet_startpos) & RingBufferMask) - 1;
	pxAssert(actualSize <= s_packet_size);
	pxAssert(s_packet_writepos < RingBufferSize);

	PacketTagType& tag = (PacketTagType&)RingBuffer[s_packet_startpos];
	tag.data[0] = actualSize;

	s_WritePos.store(s_packet_writepos, std::memory_order_release);

	if (EmuConfig.GS.SynchronousMTGS)
	{
		WaitGS();
	}
	else
	{
		s_CopyDataTally += s_packet_size;
		if (s_CopyDataTally > 0x2000)
			SetEvent();
	}

	s_packet_size = 0;

	//m_PacketLocker.Release();
}

void MTGS::GenericStall(uint size)
{
	// Note on volatiles: m_WritePos is not modified by the GS thread, so there's no need
	// to use volatile reads here.  We do cache it though, since we know it never changes,
	// except for calls to RingbufferRestert() -- handled below.
	const uint writepos = s_WritePos.load(std::memory_order_relaxed);

	// Sanity checks! (within the confines of our ringbuffer please!)
	pxAssert(size < RingBufferSize);
	pxAssert(writepos < RingBufferSize);

	// generic gs wait/stall.
	// if the writepos is past the readpos then we're safe.
	// But if not then we need to make sure the readpos is outside the scope of
	// the block about to be written (writepos + size)

	uint readpos = s_ReadPos.load(std::memory_order_acquire);
	uint freeroom;

	if (writepos < readpos)
		freeroom = readpos - writepos;
	else
		freeroom = RingBufferSize - (writepos - readpos);

	if (freeroom <= size)
	{
		// writepos will overlap readpos if we commit the data, so we need to wait until
		// readpos is out past the end of the future write pos, or until it wraps around
		// (in which case writepos will be >= readpos).

		// Ideally though we want to wait longer, because if we just toss in this packet
		// the next packet will likely stall up too.  So lets set a condition for the MTGS
		// thread to wake up the EE once there's a sizable chunk of the ringbuffer emptied.

		uint somedone = (RingBufferSize - freeroom) / 4;
		if (somedone < size + 1)
			somedone = size + 1;

		// FMV Optimization: FMVs typically send *very* little data to the GS, in some cases
		// every other frame is nothing more than a page swap.  Sleeping the EEcore is a
		// waste of time, and we get better results using a spinwait.

		if (somedone > 0x80)
		{
			pxAssertMsg(s_SignalRingEnable == 0, "MTGS Thread Synchronization Error");
			s_SignalRingPosition.store(somedone, std::memory_order_release);

			//Console.WriteLn( Color_Blue, "(EEcore Sleep) PrepDataPacker \tringpos=0x%06x, writepos=0x%06x, signalpos=0x%06x", readpos, writepos, m_SignalRingPosition );

			while (true)
			{
				s_SignalRingEnable.store(true, std::memory_order_release);
				SetEvent();
				s_sem_OnRingReset.Wait();
				readpos = s_ReadPos.load(std::memory_order_acquire);
				//Console.WriteLn( Color_Blue, "(EEcore Awake) Report!\tringpos=0x%06x", readpos );

				if (writepos < readpos)
					freeroom = readpos - writepos;
				else
					freeroom = RingBufferSize - (writepos - readpos);

				if (freeroom > size)
					break;
			}

			pxAssertMsg(s_SignalRingPosition <= 0, "MTGS Thread Synchronization Error");
		}
		else
		{
			//Console.WriteLn( Color_StrongGray, "(EEcore Spin) PrepDataPacket!" );
			SetEvent();
			while (true)
			{
				Threading::SpinWait();
				readpos = s_ReadPos.load(std::memory_order_acquire);

				if (writepos < readpos)
					freeroom = readpos - writepos;
				else
					freeroom = RingBufferSize - (writepos - readpos);

				if (freeroom > size)
					break;
			}
		}
	}
}

void MTGS::PrepDataPacket(Command cmd, u32 size)
{
	s_packet_size = size;
	++size; // takes into account our RingCommand QWC.
	GenericStall(size);

	// Command qword: Low word is the command, and the high word is the packet
	// length in SIMDs (128 bits).
	const unsigned int local_WritePos = s_WritePos.load(std::memory_order_relaxed);

	PacketTagType& tag = (PacketTagType&)RingBuffer[local_WritePos];
	tag.command = static_cast<u32>(cmd);
	tag.data[0] = s_packet_size;
	s_packet_startpos = local_WritePos;
	s_packet_writepos = (local_WritePos + 1) & RingBufferMask;
}

// Returns the amount of giftag data processed (in simd128 values).
// Return value is used by VU1's XGKICK instruction to wrap the data
// around VU memory instead of having buffer overflow...
// Parameters:
//  size - size of the packet data, in smd128's
void MTGS::PrepDataPacket(GIF_PATH pathidx, u32 size)
{
	//m_PacketLocker.Acquire();

	PrepDataPacket(static_cast<Command>(pathidx), size);
}

__fi void MTGS::_FinishSimplePacket()
{
	uint future_writepos = (s_WritePos.load(std::memory_order_relaxed) + 1) & RingBufferMask;
	pxAssert(future_writepos != s_ReadPos.load(std::memory_order_acquire));
	s_WritePos.store(future_writepos, std::memory_order_release);

	if (EmuConfig.GS.SynchronousMTGS)
		WaitGS();
	else
		++s_CopyDataTally;
}

void MTGS::SendSimplePacket(Command type, int data0, int data1, int data2)
{
	//ScopedLock locker( m_PacketLocker );

	GenericStall(1);
	PacketTagType& tag = (PacketTagType&)RingBuffer[s_WritePos.load(std::memory_order_relaxed)];

	tag.command = static_cast<u32>(type);
	tag.data[0] = data0;
	tag.data[1] = data1;
	tag.data[2] = data2;

	_FinishSimplePacket();
}

void MTGS::SendSimpleGSPacket(Command type, u32 offset, u32 size, GIF_PATH path)
{
	SendSimplePacket(type, (int)offset, (int)size, (int)path);

	if (!EmuConfig.GS.SynchronousMTGS)
	{
		s_CopyDataTally += size / 16;
		if (s_CopyDataTally > 0x2000)
			SetEvent();
	}
}

void MTGS::SendPointerPacket(Command type, u32 data0, void* data1)
{
	//ScopedLock locker( m_PacketLocker );

	GenericStall(1);
	PacketTagType& tag = (PacketTagType&)RingBuffer[s_WritePos.load(std::memory_order_relaxed)];

	tag.command = static_cast<u32>(type);
	tag.data[0] = data0;
	tag.pointer = (uptr)data1;

	_FinishSimplePacket();
}

bool MTGS::WaitForOpen()
{
	if (IsOpen())
		return true;

	StartThread();

	// request open, and kick the thread.
	s_open_flag.store(true, std::memory_order_release);
	s_sem_event.NotifyOfWork();

	// wait for it to finish its stuff
	s_open_or_close_done.Wait();

	// did we succeed?
	const bool result = s_open_flag.load(std::memory_order_acquire);
	if (!result)
		Console.Error("GS failed to open.");

	return result;
}

void MTGS::WaitForClose()
{
	if (!IsOpen())
		return;

	// ask the thread to stop processing work, by clearing the open flag
	s_open_flag.store(false, std::memory_order_release);

	// and kick the thread if it's sleeping
	s_sem_event.NotifyOfWork();

	// and wait for it to finish up..
	s_open_or_close_done.Wait();
}

void MTGS::Freeze(FreezeAction mode, MTGS::FreezeData& data)
{
	pxAssertRel(IsOpen(), "GS thread is open");

	// synchronize regs before loading
	if (mode == FreezeAction::Load)
		WaitGS(true);

	SendPointerPacket(Command::Freeze, (int)mode, &data);
	WaitGS(false);
}

void MTGS::RunOnGSThread(AsyncCallType func)
{
	SendPointerPacket(Command::AsyncCall, 0, new AsyncCallType(std::move(func)));

	// wake the gs thread in case it's sleeping
	SetEvent();
}

void MTGS::GameChanged()
{
	pxAssertRel(IsOpen(), "MTGS is running");
	RunOnGSThread(GSGameChanged);
}

void MTGS::ApplySettings()
{
	pxAssertRel(IsOpen(), "MTGS is running");

	RunOnGSThread([opts = EmuConfig.GS]() {
		GSUpdateConfig(opts);
		GSSetVSyncMode(Host::GetEffectiveVSyncMode());
	});

	// We need to synchronize the thread when changing any settings when the download mode
	// is unsynchronized, because otherwise we might potentially read in the middle of
	// the GS renderer being reopened.
	if (EmuConfig.GS.HWDownloadMode == GSHardwareDownloadMode::Unsynchronized)
		WaitGS(false, false, false);
}

void MTGS::ResizeDisplayWindow(int width, int height, float scale)
{
	pxAssertRel(IsOpen(), "MTGS is running");
	RunOnGSThread([width, height, scale]() {
		GSResizeDisplayWindow(width, height, scale);

		// If we're paused, re-present the current frame at the new window size.
		if (VMManager::GetState() == VMState::Paused)
			GSPresentCurrentFrame();
	});
}

void MTGS::UpdateDisplayWindow()
{
	pxAssertRel(IsOpen(), "MTGS is running");
	RunOnGSThread([]() {
		GSUpdateDisplayWindow();

		// If we're paused, re-present the current frame at the new window size.
		if (VMManager::GetState() == VMState::Paused)
		{
			// Hackity hack, on some systems, presenting a single frame isn't enough to actually get it
			// displayed. Two seems to be good enough. Maybe something to do with direct scanout.
			GSPresentCurrentFrame();
			GSPresentCurrentFrame();
		}
	});
}

void MTGS::SetVSyncMode(VsyncMode mode)
{
	pxAssertRel(IsOpen(), "MTGS is running");

	RunOnGSThread([mode]() {
		Console.WriteLn("Vsync is %s", mode == VsyncMode::Off ? "OFF" : (mode == VsyncMode::Adaptive ? "ADAPTIVE" : "ON"));
		GSSetVSyncMode(mode);
	});
}

void MTGS::UpdateVSyncMode()
{
	SetVSyncMode(Host::GetEffectiveVSyncMode());
}

void MTGS::SetSoftwareRendering(bool software, GSInterlaceMode interlace, bool display_message /* = true */)
{
	pxAssertRel(IsOpen(), "MTGS is running");

	if (display_message)
	{
		Host::AddIconOSDMessage("SwitchRenderer", ICON_FA_MAGIC, software ?
			TRANSLATE_STR("GS", "Switching to Software Renderer...") : TRANSLATE_STR("GS", "Switching to Hardware Renderer..."),
			Host::OSD_QUICK_DURATION);
	}

	RunOnGSThread([software, interlace]() {
		GSSetSoftwareRendering(software, interlace);
	});

	// See note in ApplySettings() for reasoning here.
	if (EmuConfig.GS.HWDownloadMode == GSHardwareDownloadMode::Unsynchronized)
		WaitGS(false, false, false);
}

void MTGS::ToggleSoftwareRendering()
{
	// reading from the GS thread.. but should be okay here
	SetSoftwareRendering(GSIsHardwareRenderer(), EmuConfig.GS.InterlaceMode);
}

bool MTGS::SaveMemorySnapshot(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
	u32* width, u32* height, std::vector<u32>* pixels)
{
	bool result = false;
	RunOnGSThread([window_width, window_height, apply_aspect, crop_borders, width, height, pixels, &result]() {
		result = GSSaveSnapshotToMemory(window_width, window_height, apply_aspect, crop_borders, width, height, pixels);
	});
	WaitGS(false, false, false);
	return result;
}

void MTGS::PresentCurrentFrame()
{
	if (s_run_idle_flag.load(std::memory_order_relaxed))
	{
		// If we're running idle, we're going to re-present anyway.
		return;
	}

	RunOnGSThread([]() {
		GSPresentCurrentFrame();
	});
}

void MTGS::SetRunIdle(bool enabled)
{
	// NOTE: Should only be called on the GS thread.
	s_run_idle_flag.store(enabled, std::memory_order_release);
}

// Used in MTVU mode... MTVU will later complete a real packet
void Gif_AddGSPacketMTVU(GS_Packet& gsPack, GIF_PATH path)
{
	MTGS::SendSimpleGSPacket(MTGS::Command::MTVUGSPacket, 0, 0, path);
}

void Gif_AddCompletedGSPacket(GS_Packet& gsPack, GIF_PATH path)
{
	//DevCon.WriteLn("Adding Completed Gif Packet [size=%x]", gsPack.size);
	if (COPY_GS_PACKET_TO_MTGS)
	{
		MTGS::PrepDataPacket(path, gsPack.size / 16);
		MemCopy_WrappedDest((u128*)&gifUnit.gifPath[path].buffer[gsPack.offset], MTGS::RingBuffer.m_Ring,
							MTGS::s_packet_writepos, MTGS::RingBufferSize, gsPack.size / 16);
		MTGS::SendDataPacket();
	}
	else
	{
		pxAssertMsg(!gsPack.readAmount, "Gif Unit - gsPack.readAmount only valid for MTVU path 1!");
		gifUnit.gifPath[path].readAmount.fetch_add(gsPack.size);
		MTGS::SendSimpleGSPacket(MTGS::Command::GSPacket, gsPack.offset, gsPack.size, path);
	}
}

void Gif_AddBlankGSPacket(u32 size, GIF_PATH path)
{
	//DevCon.WriteLn("Adding Blank Gif Packet [size=%x]", size);
	gifUnit.gifPath[path].readAmount.fetch_add(size);
	MTGS::SendSimpleGSPacket(MTGS::Command::GSPacket, ~0u, size, path);
}

void Gif_MTGS_Wait(bool isMTVU)
{
	MTGS::WaitGS(false, true, isMTVU);
}