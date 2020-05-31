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

#include "PrecompiledHeader.h"
#include "Common.h"

#include <list>
#include <wx/datetime.h>

#include "GS.h"
#include "Gif_Unit.h"
#include "MTVU.h"
#include "Elfheader.h"


// Uncomment this to enable profiling of the GS RingBufferCopy function.
//#define PCSX2_GSRING_SAMPLING_STATS

using namespace Threading;

#if 0 //PCSX2_DEBUG
#	define MTGS_LOG Console.WriteLn
#else
#	define MTGS_LOG(...) do {} while (0)
#endif

// =====================================================================================================
//  MTGS Threaded Class Implementation
// =====================================================================================================

__aligned(32) MTGS_BufferedData RingBuffer;
extern bool renderswitch;


#ifdef RINGBUF_DEBUG_STACK
#include <list>
std::list<uint> ringposStack;
#endif

SysMtgsThread::SysMtgsThread() :
	SysThreadBase()
#ifdef RINGBUF_DEBUG_STACK
,	m_lock_Stack()
#endif
{
	m_name = L"MTGS";

	// All other state vars are initialized by OnStart().
}

void SysMtgsThread::OnStart()
{
	m_PluginOpened		= false;

	m_ReadPos			= 0;
	m_WritePos			= 0;
	m_RingBufferIsBusy  = false;
	m_packet_size		= 0;
	m_packet_writepos	= 0;

	m_QueuedFrameCount    = 0;
	m_VsyncSignalListener = false;
	m_SignalRingEnable    = false;
	m_SignalRingPosition  = 0;

	m_CopyDataTally		= 0;

	_parent::OnStart();
}

SysMtgsThread::~SysMtgsThread()
{
	try {
		_parent::Cancel();
	}
	DESTRUCTOR_CATCHALL
}

void SysMtgsThread::OnResumeReady()
{
	m_sem_OpenDone.Reset();
}

void SysMtgsThread::ResetGS()
{
	pxAssertDev( !IsOpen() || (m_ReadPos == m_WritePos), "Must close or terminate the GS thread prior to gsReset." );

	// MTGS Reset process:
	//  * clear the ringbuffer.
	//  * Signal a reset.
	//  * clear the path and byRegs structs (used by GIFtagDummy)

	m_ReadPos             = m_WritePos.load();
	m_QueuedFrameCount    = 0;
	m_VsyncSignalListener = 0;

	MTGS_LOG( "MTGS: Sending Reset..." );
	SendSimplePacket( GS_RINGTYPE_RESET, 0, 0, 0 );
	SendSimplePacket( GS_RINGTYPE_FRAMESKIP, 0, 0, 0 );
	SetEvent();
}

struct RingCmdPacket_Vsync
{
	u8				regset1[0x0f0];
	u32				csr;
	u32				imr;
	GSRegSIGBLID	siglblid;
};

void SysMtgsThread::PostVsyncStart()
{
	// Optimization note: Typically regset1 isn't needed.  The regs in that area are typically
	// changed infrequently, usually during video mode changes.  However, on modern systems the
	// 256-byte copy is only a few dozen cycles -- executed 60 times a second -- so probably
	// not worth the effort or overhead of trying to selectively avoid it.

	uint packsize = sizeof(RingCmdPacket_Vsync) / 16;
	PrepDataPacket(GS_RINGTYPE_VSYNC, packsize);
	MemCopy_WrappedDest( (u128*)PS2MEM_GS, RingBuffer.m_Ring, m_packet_writepos, RingBufferSize, 0xf );

	u32* remainder = (u32*)GetDataPacketPtr();
	remainder[0] = GSCSRr;
	remainder[1] = GSIMR._u32;
	(GSRegSIGBLID&)remainder[2] = GSSIGLBLID;
	m_packet_writepos = (m_packet_writepos + 1) & RingBufferMask;

	SendDataPacket();

	// Vsyncs should always start the GS thread, regardless of how little has actually be queued.
	if (m_CopyDataTally != 0) SetEvent();

	// If the MTGS is allowed to queue a lot of frames in advance, it creates input lag.
	// Use the Queued FrameCount to stall the EE if another vsync (or two) are already queued
	// in the ringbuffer.  The queue limit is disabled when both FrameLimiting and Vsync are
	// disabled, since the queue can have perverse effects on framerate benchmarking.

	// Edit: It's possible that MTGS is that much faster than the GS plugin that it creates so much lag,
	// a game becomes uncontrollable (software rendering for example).
	// For that reason it's better to have the limit always in place, at the cost of a few max FPS in benchmarks.
	// If those are needed back, it's better to increase the VsyncQueueSize via PCSX_vm.ini.
	// (The Xenosaga engine is known to run into this, due to it throwing bulks of data in one frame followed by 2 empty frames.)

	if ((m_QueuedFrameCount.fetch_add(1) < EmuConfig.GS.VsyncQueueSize) /*|| (!EmuConfig.GS.VsyncEnable && !EmuConfig.GS.FrameLimitEnable)*/) return;

	m_VsyncSignalListener.store(true, std::memory_order_release);
	//Console.WriteLn( Color_Blue, "(EEcore Sleep) Vsync\t\tringpos=0x%06x, writepos=0x%06x", m_ReadPos.load(), m_WritePos.load() );

	// We will wait a vsync event from the MTGS ring. If the ring is already purged, the event will never come !
	// To avoid this potential deadlock, ring must be wake up after m_VsyncSignalListener
	// Note: potentially we can also miss the previous wake up if we optimize away the post just before the release of busy signal of the ring
	// So let's ensure the ring doesn't sleep
	m_sem_event.Post();

	m_sem_Vsync.WaitNoCancel();
}

union PacketTagType
{
	struct {
		u32 command;
		u32 data[3];
	};
	struct {
		u32 _command;
		u32 _data[1];
		uptr pointer;
	};
};

static void dummyIrqCallback()
{
	// dummy, because MTGS doesn't need this mess!
	// (and zerogs does >_<)
}

void SysMtgsThread::OpenPlugin()
{

	if( m_PluginOpened ) return;

	memcpy( RingBuffer.Regs, PS2MEM_GS, sizeof(PS2MEM_GS) );
	GSsetBaseMem( RingBuffer.Regs );
	GSirqCallback( dummyIrqCallback );

	int result;

	if( GSopen2 != NULL )
		result = GSopen2( (void*)pDsp, 1 | (renderswitch ? 4 : 0) );
	else
		result = GSopen( (void*)pDsp, "PCSX2", renderswitch ? 2 : 1 );


	GSsetVsync(EmuConfig.GS.GetVsync());

	if( result != 0 )
	{
		DevCon.WriteLn( "GSopen Failed: return code: 0x%x", result );
		throw Exception::PluginOpenError( PluginId_GS );
	}

// This is the preferred place to implement DXGI fullscreen overrides, using LoadLibrary.
// But I hate COM, I don't know to make this work, and I don't have DX10, so I give up
// and enjoy my working DX9 alt-enter instead. Someone else can fix this mess. --air

// Also: Prolly needs some DX10 header includes?  Which ones?  Too many, I gave up.

#if 0    // defined(__WXMSW__) && defined(_MSC_VER)
	wxDynamicLibrary dynlib( L"dxgi.dll" );
	SomeFuncTypeIDunno isThisEvenTheRightFunctionNameIDunno = dynlib.GetSymbol("CreateDXGIFactory");
	if( isThisEvenTheRightFunctionNameIDunno )
	{
		// Is this how LoadLibrary for COM works?  I dunno.  I dont care.

		IDXGIFactory* pFactory;
		hr = isThisEvenTheRightFunctionNameIDunno(__uuidof(IDXGIFactory), (void**)(&pFactory) );
		pFactory->MakeWindowAssociation((HWND)&pDsp, DXGI_MWA_NO_WINDOW_CHANGES);
		pFactory->Release();
	}
#endif

	m_PluginOpened = true;
	m_sem_OpenDone.Post();

	GSsetGameCRC( ElfCRC, 0 );
}

class RingBufferLock {
	ScopedLock     m_lock1;
	ScopedLock     m_lock2;
	SysMtgsThread& m_mtgs;

	public:

	RingBufferLock(SysMtgsThread& mtgs)
		: m_lock1(mtgs.m_mtx_RingBufferBusy),
		  m_lock2(mtgs.m_mtx_RingBufferBusy2),
		  m_mtgs(mtgs) {
		m_mtgs.m_RingBufferIsBusy.store(true, std::memory_order_relaxed);
	}
	virtual ~RingBufferLock() {
		m_mtgs.m_RingBufferIsBusy.store(false, std::memory_order_relaxed);
	}
	void Acquire() {
		m_lock1.Acquire();
		m_lock2.Acquire();
		m_mtgs.m_RingBufferIsBusy.store(true, std::memory_order_relaxed);
	}
	void Release() {
		m_mtgs.m_RingBufferIsBusy.store(false, std::memory_order_relaxed);
		m_lock2.Release();
		m_lock1.Release();
	}
	void PartialAcquire() {
		m_lock2.Acquire();
	}
	void PartialRelease() {
		m_lock2.Release();
	}
};

void SysMtgsThread::ExecuteTaskInThread()
{
	// Threading info: run in MTGS thread
	// m_ReadPos is only update by the MTGS thread so it is safe to load it with a relaxed atomic

#ifdef RINGBUF_DEBUG_STACK
	PacketTagType prevCmd;
#endif

	RingBufferLock busy (*this);

	while(true) {
		busy.Release();

		// Performance note: Both of these perform cancellation tests, but pthread_testcancel
		// is very optimized (only 1 instruction test in most cases), so no point in trying
		// to avoid it.

		m_sem_event.WaitWithoutYield();
		StateCheckInThread();
		busy.Acquire();

		// note: m_ReadPos is intentionally not volatile, because it should only
		// ever be modified by this thread.
		while( m_ReadPos.load(std::memory_order_relaxed) != m_WritePos.load(std::memory_order_acquire))
		{
			const unsigned int local_ReadPos = m_ReadPos.load(std::memory_order_relaxed);

			pxAssert( local_ReadPos < RingBufferSize );

			const PacketTagType& tag = (PacketTagType&)RingBuffer[local_ReadPos];
			u32 ringposinc = 1;

#ifdef RINGBUF_DEBUG_STACK
			// pop a ringpos off the stack.  It should match this one!

			m_lock_Stack.Lock();
			uptr stackpos = ringposStack.back();
			if( stackpos != local_ReadPos )
			{
				Console.Error( "MTGS Ringbuffer Critical Failure ---> %x to %x (prevCmd: %x)\n", stackpos, local_ReadPos, prevCmd.command );
			}
			pxAssert( stackpos == local_ReadPos );
			prevCmd = tag;
			ringposStack.pop_back();
			m_lock_Stack.Release();
#endif

			switch( tag.command )
			{
#if COPY_GS_PACKET_TO_MTGS == 1
				case GS_RINGTYPE_P1:
				{
					uint datapos = (local_ReadPos+1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG( "(MTGS Packet Read) ringtype=P1, qwc=%u", qsize );

					uint endpos = datapos + qsize;
					if( endpos >= RingBufferSize )
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer( (u32*)data, firstcopylen );
						datapos = endpos & RingBufferMask;
						GSgifTransfer( (u32*)RingBuffer.m_Ring, datapos );
					}
					else
					{
						GSgifTransfer( (u32*)data, qsize );
					}

					ringposinc += qsize;
				}
				break;

				case GS_RINGTYPE_P2:
				{
					uint datapos = (local_ReadPos+1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG( "(MTGS Packet Read) ringtype=P2, qwc=%u", qsize );

					uint endpos = datapos + qsize;
					if( endpos >= RingBufferSize )
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer2( (u32*)data, firstcopylen );
						datapos = endpos & RingBufferMask;
						GSgifTransfer2( (u32*)RingBuffer.m_Ring, datapos );
					}
					else
					{
						GSgifTransfer2( (u32*)data, qsize );
					}

					ringposinc += qsize;
				}
				break;

				case GS_RINGTYPE_P3:
				{
					uint datapos = (local_ReadPos+1) & RingBufferMask;
					const int qsize = tag.data[0];
					const u128* data = &RingBuffer[datapos];

					MTGS_LOG( "(MTGS Packet Read) ringtype=P3, qwc=%u", qsize );

					uint endpos = datapos + qsize;
					if( endpos >= RingBufferSize )
					{
						uint firstcopylen = RingBufferSize - datapos;
						GSgifTransfer3( (u32*)data, firstcopylen );
						datapos = endpos & RingBufferMask;
						GSgifTransfer3( (u32*)RingBuffer.m_Ring, datapos );
					}
					else
					{
						GSgifTransfer3( (u32*)data, qsize );
					}

					ringposinc += qsize;
				}
				break;
#endif
				case GS_RINGTYPE_GSPACKET: {
					Gif_Path& path   = gifUnit.gifPath[tag.data[2]];
					u32       offset = tag.data[0];
					u32       size   = tag.data[1];
					if (offset != ~0u) GSgifTransfer((u32*)&path.buffer[offset], size/16);
					path.readAmount.fetch_sub(size, std::memory_order_acq_rel);
					break;
				}

				case GS_RINGTYPE_MTVU_GSPACKET: {
					MTVU_LOG("MTGS - Waiting on semaXGkick!");
					vu1Thread.KickStart(true);
					busy.PartialRelease();
					// Wait for MTVU to complete vu1 program
					vu1Thread.semaXGkick.WaitWithoutYield();
					busy.PartialAcquire();
					Gif_Path& path   = gifUnit.gifPath[GIF_PATH_1];
					GS_Packet gsPack = path.GetGSPacketMTVU(); // Get vu1 program's xgkick packet(s)
					if (gsPack.size) GSgifTransfer((u32*)&path.buffer[gsPack.offset], gsPack.size/16);
					path.readAmount.fetch_sub(gsPack.size + gsPack.readAmount, std::memory_order_acq_rel);
					path.PopGSPacketMTVU(); // Should be done last, for proper Gif_MTGS_Wait()
					break;
				}

				default:
				{
					switch( tag.command )
					{
						case GS_RINGTYPE_VSYNC:
						{
							const int qsize = tag.data[0];
							ringposinc += qsize;

							MTGS_LOG( "(MTGS Packet Read) ringtype=Vsync, field=%u, skip=%s", !!(((u32&)RingBuffer.Regs[0x1000]) & 0x2000) ? 0 : 1, tag.data[1] ? "true" : "false" );

							// Mail in the important GS registers.
							// This seemingly obtuse system is needed in order to handle cases where the vsync data wraps
							// around the edge of the ringbuffer.  If not for that I'd just use a struct. >_<

							uint datapos = (local_ReadPos+1) & RingBufferMask;
							MemCopy_WrappedSrc( RingBuffer.m_Ring, datapos, RingBufferSize, (u128*)RingBuffer.Regs, 0xf );

							u32* remainder = (u32*)&RingBuffer[datapos];
							((u32&)RingBuffer.Regs[0x1000])				= remainder[0];
							((u32&)RingBuffer.Regs[0x1010])				= remainder[1];
							((GSRegSIGBLID&)RingBuffer.Regs[0x1080])	= (GSRegSIGBLID&)remainder[2];

							// CSR & 0x2000; is the pageflip id.
							GSvsync(((u32&)RingBuffer.Regs[0x1000]) & 0x2000);
							gsFrameSkip();

							// if we're not using GSOpen2, then the GS window is on this thread (MTGS thread),
							// so we need to call PADupdate from here.
							if( (GSopen2 == NULL) && (PADupdate != NULL) )
								PADupdate(0);

							m_QueuedFrameCount.fetch_sub(1);
							if (m_VsyncSignalListener.exchange(false))
								m_sem_Vsync.Post();

							busy.Release();
							StateCheckInThread();
							busy.Acquire();
						}
						break;

						case GS_RINGTYPE_FRAMESKIP:
							MTGS_LOG( "(MTGS Packet Read) ringtype=Frameskip" );
							_gs_ResetFrameskip();
						break;

						case GS_RINGTYPE_FREEZE:
						{
							MTGS_FreezeData* data = (MTGS_FreezeData*)tag.pointer;
							int mode = tag.data[0];
							data->retval = GetCorePlugins().DoFreeze( PluginId_GS, mode, data->fdata );
						}
						break;

						case GS_RINGTYPE_RESET:
							MTGS_LOG( "(MTGS Packet Read) ringtype=Reset" );
							if( GSreset != NULL ) GSreset();
						break;

						case GS_RINGTYPE_SOFTRESET:
						{
							int mask = tag.data[0];
							MTGS_LOG( "(MTGS Packet Read) ringtype=SoftReset" );
							GSgifSoftReset( mask );
						}
						break;

						case GS_RINGTYPE_MODECHANGE:
							// [TODO] some frameskip sync logic might be needed here!
						break;

						case GS_RINGTYPE_CRC:
							GSsetGameCRC( tag.data[0], 0 );
						break;

						case GS_RINGTYPE_INIT_READ_FIFO1:
							MTGS_LOG( "(MTGS Packet Read) ringtype=Fifo1" );
							if (GSinitReadFIFO)
								GSinitReadFIFO( (u64*)tag.pointer);
						break;

						case GS_RINGTYPE_INIT_READ_FIFO2:
							MTGS_LOG( "(MTGS Packet Read) ringtype=Fifo2, size=%d", tag.data[0] );
							if (GSinitReadFIFO2)
								GSinitReadFIFO2( (u64*)tag.pointer, tag.data[0]);
						break;

#ifdef PCSX2_DEVBUILD
						default:
							Console.Error("GSThreadProc, bad packet (%x) at m_ReadPos: %x, m_WritePos: %x", tag.command, local_ReadPos, m_WritePos.load());
							pxFail( "Bad packet encountered in the MTGS Ringbuffer." );
							m_ReadPos.store(m_WritePos.load(std::memory_order_acquire), std::memory_order_release);
						continue;
#else
						// Optimized performance in non-Dev builds.
						jNO_DEFAULT;
#endif
					}
				}
			}

			uint newringpos = (m_ReadPos.load(std::memory_order_relaxed) + ringposinc) & RingBufferMask;

			if( EmuConfig.GS.SynchronousMTGS )
			{
				pxAssert( m_WritePos == newringpos );
			}

			m_ReadPos.store(newringpos, std::memory_order_release);

			if(m_SignalRingEnable.load(std::memory_order_acquire))
			{
				// The EEcore has requested a signal after some amount of processed data.
				if( m_SignalRingPosition.fetch_sub( ringposinc ) <= 0 )
				{
					// Make sure to post the signal after the m_ReadPos has been updated...
					m_SignalRingEnable.store(false, std::memory_order_release);
					m_sem_OnRingReset.Post();
					continue;
				}
			}
		}

		busy.Release();

		// Safety valve in case standard signals fail for some reason -- this ensures the EEcore
		// won't sleep the eternity, even if SignalRingPosition didn't reach 0 for some reason.
		// Important: Need to unlock the MTGS busy signal PRIOR, so that EEcore SetEvent() calls
		// parallel to this handler aren't accidentally blocked.
		if( m_SignalRingEnable.exchange(false) )
		{
			//Console.Warning( "(MTGS Thread) Dangling RingSignal on empty buffer!  signalpos=0x%06x", m_SignalRingPosition.exchange(0) ) );
			m_SignalRingPosition.store(0, std::memory_order_release);
			m_sem_OnRingReset.Post();
		}

		if (m_VsyncSignalListener.exchange(false))
			m_sem_Vsync.Post();

		//Console.Warning( "(MTGS Thread) Nothing to do!  ringpos=0x%06x", m_ReadPos );
	}
}

void SysMtgsThread::ClosePlugin()
{
	if( !m_PluginOpened ) return;
	m_PluginOpened = false;
	GetCorePlugins().Close( PluginId_GS );
}

void SysMtgsThread::OnSuspendInThread()
{
	ClosePlugin();
	_parent::OnSuspendInThread();
}

void SysMtgsThread::OnResumeInThread( bool isSuspended )
{
	if( isSuspended )
		OpenPlugin();

	_parent::OnResumeInThread( isSuspended );
}

void SysMtgsThread::OnCleanupInThread()
{
	ClosePlugin();
	_parent::OnCleanupInThread();
}

// Waits for the GS to empty out the entire ring buffer contents.
// If syncRegs, then writes pcsx2's gs regs to MTGS's internal copy
// If weakWait, then this function is allowed to exit after MTGS finished a path1 packet
// If isMTVU, then this implies this function is being called from the MTVU thread...
void SysMtgsThread::WaitGS(bool syncRegs, bool weakWait, bool isMTVU)
{
	pxAssertDev( !IsSelf(), "This method is only allowed from threads *not* named MTGS." );

	if( m_ExecMode == ExecMode_NoThreadYet || !IsRunning() ) return;
	if( !pxAssertDev( IsOpen(), "MTGS Warning!  WaitGS issued on a closed thread." ) ) return;

	Gif_Path&   path = gifUnit.gifPath[GIF_PATH_1];
	u32 startP1Packs = weakWait ? path.GetPendingGSPackets() : 0;

	// Both m_ReadPos and m_WritePos can be relaxed as we only want to test if the queue is empty but
	// we don't want to access the content of the queue

	if (isMTVU || m_ReadPos.load(std::memory_order_relaxed) != m_WritePos.load(std::memory_order_relaxed)) {
		SetEvent();
		RethrowException();
		for(;;) {
			if (weakWait) m_mtx_RingBufferBusy2.Wait();
			else          m_mtx_RingBufferBusy .Wait();
			RethrowException();
			if(!isMTVU && m_ReadPos.load(std::memory_order_relaxed) == m_WritePos.load(std::memory_order_relaxed)) break;
			u32 curP1Packs = weakWait ? path.GetPendingGSPackets() : 0;
			if (weakWait && ((startP1Packs-curP1Packs) || !curP1Packs)) break;
			// On weakWait we will stop waiting on the MTGS thread if the
			// MTGS thread has processed a vu1 xgkick packet, or is pending on
			// its final vu1 xgkick packet (!curP1Packs)...
			// Note: m_WritePos doesn't seem to have proper atomic write
			// code, so reading it from the MTVU thread might be dangerous;
			// hence it has been avoided...
		}
	}

	if (syncRegs) {
		ScopedLock lock(m_mtx_WaitGS);
		// Completely synchronize GS and MTGS register states.
		memcpy(RingBuffer.Regs, PS2MEM_GS, sizeof(RingBuffer.Regs));
	}
}

// Sets the gsEvent flag and releases a timeslice.
// For use in loops that wait on the GS thread to do certain things.
void SysMtgsThread::SetEvent()
{
	if(!m_RingBufferIsBusy.load(std::memory_order_relaxed))
		m_sem_event.Post();

	m_CopyDataTally = 0;
}

u8* SysMtgsThread::GetDataPacketPtr() const
{
	return (u8*)&RingBuffer[m_packet_writepos & RingBufferMask];
}

// Closes the data packet send command, and initiates the gs thread (if needed).
void SysMtgsThread::SendDataPacket()
{
	// make sure a previous copy block has been started somewhere.
	pxAssert( m_packet_size != 0 );

	uint actualSize = ((m_packet_writepos - m_packet_startpos) & RingBufferMask)-1;
	pxAssert( actualSize <= m_packet_size );
	pxAssert( m_packet_writepos < RingBufferSize );

	PacketTagType& tag = (PacketTagType&)RingBuffer[m_packet_startpos];
	tag.data[0] = actualSize;

	m_WritePos.store(m_packet_writepos, std::memory_order_release);

	if(EmuConfig.GS.SynchronousMTGS)
	{
		WaitGS();
	}
	else if(!m_RingBufferIsBusy.load(std::memory_order_relaxed))
	{
		m_CopyDataTally += m_packet_size;
		if( m_CopyDataTally > 0x2000 ) SetEvent();
	}

	m_packet_size = 0;

	//m_PacketLocker.Release();
}

void SysMtgsThread::GenericStall( uint size )
{
	// Note on volatiles: m_WritePos is not modified by the GS thread, so there's no need
	// to use volatile reads here.  We do cache it though, since we know it never changes,
	// except for calls to RingbufferRestert() -- handled below.
	const uint writepos = m_WritePos.load(std::memory_order_relaxed);

	// Sanity checks! (within the confines of our ringbuffer please!)
	pxAssert( size < RingBufferSize );
	pxAssert( writepos < RingBufferSize );

	// generic gs wait/stall.
	// if the writepos is past the readpos then we're safe.
	// But if not then we need to make sure the readpos is outside the scope of
	// the block about to be written (writepos + size)

	uint readpos = m_ReadPos.load(std::memory_order_acquire);
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

		uint somedone	= (RingBufferSize - freeroom) / 4;
		if( somedone < size+1 ) somedone = size + 1;

		// FMV Optimization: FMVs typically send *very* little data to the GS, in some cases
		// every other frame is nothing more than a page swap.  Sleeping the EEcore is a
		// waste of time, and we get better results using a spinwait.

		if( somedone > 0x80 )
		{
			pxAssertDev( m_SignalRingEnable == 0, "MTGS Thread Synchronization Error" );
			m_SignalRingPosition.store(somedone, std::memory_order_release);

			//Console.WriteLn( Color_Blue, "(EEcore Sleep) PrepDataPacker \tringpos=0x%06x, writepos=0x%06x, signalpos=0x%06x", readpos, writepos, m_SignalRingPosition );

			while(true) {
				m_SignalRingEnable.store(true, std::memory_order_release);
				SetEvent();
				m_sem_OnRingReset.WaitWithoutYield();
				readpos = m_ReadPos.load(std::memory_order_acquire);
				//Console.WriteLn( Color_Blue, "(EEcore Awake) Report!\tringpos=0x%06x", readpos );

				if (writepos < readpos)
					freeroom = readpos - writepos;
				else
					freeroom = RingBufferSize - (writepos - readpos);

				if (freeroom > size) break;
			}

			pxAssertDev( m_SignalRingPosition <= 0, "MTGS Thread Synchronization Error" );
		}
		else
		{
			//Console.WriteLn( Color_StrongGray, "(EEcore Spin) PrepDataPacket!" );
			SetEvent();
			while(true) {
				SpinWait();
				readpos = m_ReadPos.load(std::memory_order_acquire);

				if (writepos < readpos)
					freeroom = readpos - writepos;
				else
					freeroom = RingBufferSize - (writepos - readpos);

				if (freeroom > size) break;
			}
		}
	}
}

void SysMtgsThread::PrepDataPacket( MTGS_RingCommand cmd, u32 size )
{
	m_packet_size = size;
	++size;			// takes into account our RingCommand QWC.
	GenericStall(size);

	// Command qword: Low word is the command, and the high word is the packet
	// length in SIMDs (128 bits).
	const unsigned int local_WritePos = m_WritePos.load(std::memory_order_relaxed);

	PacketTagType& tag = (PacketTagType&)RingBuffer[local_WritePos];
	tag.command = cmd;
	tag.data[0] = m_packet_size;
	m_packet_startpos = local_WritePos;
	m_packet_writepos = (local_WritePos + 1) & RingBufferMask;
}

// Returns the amount of giftag data processed (in simd128 values).
// Return value is used by VU1's XGKICK instruction to wrap the data
// around VU memory instead of having buffer overflow...
// Parameters:
//  size - size of the packet data, in smd128's
void SysMtgsThread::PrepDataPacket( GIF_PATH pathidx, u32 size )
{
	//m_PacketLocker.Acquire();

	PrepDataPacket( (MTGS_RingCommand)pathidx, size );
}

__fi void SysMtgsThread::_FinishSimplePacket()
{
	uint future_writepos = (m_WritePos.load(std::memory_order_relaxed) +1) & RingBufferMask;
	pxAssert( future_writepos != m_ReadPos.load(std::memory_order_acquire) );
	m_WritePos.store(future_writepos, std::memory_order_release);

	if( EmuConfig.GS.SynchronousMTGS )
		WaitGS();
	else
		++m_CopyDataTally;
}

void SysMtgsThread::SendSimplePacket( MTGS_RingCommand type, int data0, int data1, int data2 )
{
	//ScopedLock locker( m_PacketLocker );

	GenericStall(1);
	PacketTagType& tag = (PacketTagType&)RingBuffer[m_WritePos.load(std::memory_order_relaxed)];

	tag.command = type;
	tag.data[0] = data0;
	tag.data[1] = data1;
	tag.data[2] = data2;

	_FinishSimplePacket();
}

void SysMtgsThread::SendSimpleGSPacket(MTGS_RingCommand type, u32 offset, u32 size, GIF_PATH path)
{
	SendSimplePacket(type, (int)offset, (int)size, (int)path);

	if(!EmuConfig.GS.SynchronousMTGS) {
		if(!m_RingBufferIsBusy.load(std::memory_order_relaxed)) {
			m_CopyDataTally += size / 16;
			if (m_CopyDataTally > 0x2000) SetEvent();
		}
	}
}

void SysMtgsThread::SendPointerPacket( MTGS_RingCommand type, u32 data0, void* data1 )
{
	//ScopedLock locker( m_PacketLocker );

	GenericStall(1);
	PacketTagType& tag = (PacketTagType&)RingBuffer[m_WritePos.load(std::memory_order_relaxed)];

	tag.command = type;
	tag.data[0] = data0;
	tag.pointer = (uptr)data1;

	_FinishSimplePacket();
}

void SysMtgsThread::SendGameCRC( u32 crc )
{
	SendSimplePacket( GS_RINGTYPE_CRC, crc, 0, 0 );
}

void SysMtgsThread::WaitForOpen()
{
	if( m_PluginOpened ) return;
	Resume();

	// Two-phase timeout on MTGS opening, so that possible errors are handled
	// in a timely fashion.  We check for errors after 2 seconds, and then give it
	// another 12 seconds if no errors occurred (this might seem long, but sometimes a
	// GS plugin can be very stubborned, especially in debug mode builds).

	if( !m_sem_OpenDone.Wait( wxTimeSpan(0, 0, 2, 0) ) )
	{
		RethrowException();

		if( !m_sem_OpenDone.Wait( wxTimeSpan(0, 0, 12, 0) ) )
		{
			RethrowException();

			// Not opened yet, and no exceptions.  Weird?  You decide!
			// [TODO] : implement a user confirmation to cancel the action and exit the
			//   emulator forcefully, or to continue waiting on the GS.

			throw Exception::PluginOpenError( PluginId_GS )
				.SetBothMsgs(pxLt("The MTGS thread has become unresponsive while waiting for the GS plugin to open."));
		}
	}

	RethrowException();
}

void SysMtgsThread::Freeze( int mode, MTGS_FreezeData& data )
{
	GetCorePlugins().Open( PluginId_GS );
	SendPointerPacket( GS_RINGTYPE_FREEZE, mode, &data );
	Resume();
	WaitGS();
}
