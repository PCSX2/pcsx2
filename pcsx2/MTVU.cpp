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
#include "MTVU.h"
#include "newVif.h"
#include "Gif_Unit.h"
#include "common/Threading.h"
#include <thread>

VU_Thread vu1Thread;

#define MTVU_ALWAYS_KICK 0
#define MTVU_SYNC_MODE 0

// Rounds up a size in bytes for size in u32's
static __fi u32 size_u32(u32 x) { return (x + 3) >> 2; }

enum MTVU_EVENT
{
	MTVU_VU_EXECUTE,     // Execute VU program
	MTVU_VU_WRITE_MICRO, // Write to VU micro-mem
	MTVU_VU_WRITE_DATA,  // Write to VU data-mem
	MTVU_VU_WRITE_VIREGS,// Write to VU registers
	MTVU_VU_WRITE_VFREGS,// Write to VU registers
	MTVU_VIF_WRITE_COL,  // Write to Vif col reg
	MTVU_VIF_WRITE_ROW,  // Write to Vif row reg
	MTVU_VIF_UNPACK,     // Execute Vif Unpack
	MTVU_NULL_PACKET,    // Go back to beginning of buffer
	MTVU_RESET
};

// Calls the vif unpack functions from the MTVU thread
static void MTVU_Unpack(void* data, VIFregisters& vifRegs)
{
	u16 wl = vifRegs.cycle.wl > 0 ? vifRegs.cycle.wl : 256;
	bool isFill = vifRegs.cycle.cl < wl;
	if (newVifDynaRec)
		dVifUnpack<1>((u8*)data, isFill);
	else
		_nVifUnpack(1, (u8*)data, vifRegs.mode, isFill);
}

// Called on Saving/Loading states...
void SaveStateBase::mtvuFreeze()
{
	FreezeTag("MTVU");
	pxAssert(vu1Thread.IsDone());
	if (!IsSaving())
	{
		vu1Thread.Reset();
		vu1Thread.WriteCol(vif1);
		vu1Thread.WriteRow(vif1);
		vu1Thread.WriteMicroMem(0, VU1.Micro, 0x4000);
		vu1Thread.WriteDataMem(0, VU1.Mem, 0x4000);
		vu1Thread.WriteVIRegs(&VU1.VI[0]);
		vu1Thread.WriteVFRegs(&VU1.VF[0]);
	}
	for (size_t i = 0; i < 4; ++i)
	{
		unsigned int v = vu1Thread.vuCycles[i].load();
		Freeze(v);
	}

	u32 gsInterrupts = vu1Thread.mtvuInterrupts.load();
	Freeze(gsInterrupts);
	vu1Thread.mtvuInterrupts.store(gsInterrupts);
	u64 gsSignal = vu1Thread.gsSignal.load();
	Freeze(gsSignal);
	vu1Thread.gsSignal.store(gsSignal);
	u64 gsLabel = vu1Thread.gsLabel.load();
	Freeze(gsLabel);
	vu1Thread.gsLabel.store(gsLabel);

	Freeze(vu1Thread.vuCycleIdx);
}

VU_Thread::VU_Thread()
{
	Reset();
}

VU_Thread::~VU_Thread()
{
	Close();
}

void VU_Thread::Open()
{
	if (m_thread.joinable())
		return;

	Reset();
	semaEvent.Reset();
	m_shutdown_flag.store(false, std::memory_order_release);
	m_thread = std::thread(&VU_Thread::ExecuteRingBuffer, this);
}

void VU_Thread::Close()
{
	if (!m_thread.joinable())
		return;

	m_shutdown_flag.store(true, std::memory_order_release);
	semaEvent.NotifyOfWork();
	m_thread.join();
}

void VU_Thread::Reset()
{
	vuCycleIdx = 0;
	m_ato_write_pos = 0;
	m_write_pos = 0;
	m_ato_read_pos = 0;
	m_read_pos = 0;
	memzero(vif);
	memzero(vifRegs);
	for (size_t i = 0; i < 4; ++i)
		vu1Thread.vuCycles[i] = 0;
	vu1Thread.mtvuInterrupts = 0;
}

void VU_Thread::ExecuteRingBuffer()
{
	m_thread_handle = Threading::ThreadHandle::GetForCallingThread();
	Threading::SetNameOfCurrentThread("MTVU");

	for (;;)
	{
		semaEvent.WaitForWork();
		if (m_shutdown_flag.load(std::memory_order_acquire))
			break;

		while (m_ato_read_pos.load(std::memory_order_relaxed) != GetWritePos())
		{
			u32 tag = Read();
			switch (tag)
			{
				case MTVU_VU_EXECUTE:
				{
					VU1.cycle = 0;
					s32 addr = Read();
					vifRegs.top = Read();
					vifRegs.itop = Read();
					vuFBRST = Read();
					if (addr != -1)
						VU1.VI[REG_TPC].UL = addr & 0x7FF;
					CpuVU1->SetStartPC(VU1.VI[REG_TPC].UL << 3);
					CpuVU1->Execute(vu1RunCycles);
					gifUnit.gifPath[GIF_PATH_1].FinishGSPacketMTVU();
					semaXGkick.Post(); // Tell MTGS a path1 packet is complete
					vuCycles[vuCycleIdx].store(VU1.cycle, std::memory_order_release);
					vuCycleIdx = (vuCycleIdx + 1) & 3;
					break;
				}
				case MTVU_VU_WRITE_MICRO:
				{
					u32 vu_micro_addr = Read();
					u32 size = Read();
					CpuVU1->Clear(vu_micro_addr, size);
					Read(&VU1.Micro[vu_micro_addr], size);
					break;
				}
				case MTVU_VU_WRITE_DATA:
				{
					u32 vu_data_addr = Read();
					u32 size = Read();
					Read(&VU1.Mem[vu_data_addr], size);
					break;
				}
				case MTVU_VU_WRITE_VIREGS:
					Read(&VU1.VI, size_u32(32));
					break;
				case MTVU_VU_WRITE_VFREGS:
					Read(&VU1.VF, size_u32(4*32));
					break;
				case MTVU_VIF_WRITE_COL:
					Read(&vif.MaskCol, sizeof(vif.MaskCol));
					break;
				case MTVU_VIF_WRITE_ROW:
					Read(&vif.MaskRow, sizeof(vif.MaskRow));
					break;
				case MTVU_VIF_UNPACK:
				{
					u32 vif_copy_size = (uptr)&vif.StructEnd - (uptr)&vif.tag;
					Read(&vif.tag, vif_copy_size);
					ReadRegs(&vifRegs);
					u32 size = Read();
					MTVU_Unpack(&buffer[m_read_pos], vifRegs);
					m_read_pos += size_u32(size);
					break;
				}
				case MTVU_NULL_PACKET:
					m_read_pos = 0;
					break;
					jNO_DEFAULT;
			}

			CommitReadPos();
		}
	}

	m_thread_handle = {};
	semaEvent.Kill();
}


// Should only be called by ReserveSpace()
__ri void VU_Thread::WaitOnSize(s32 size)
{
	for (;;)
	{
		s32 readPos = GetReadPos();
		if (readPos <= m_write_pos)
			break; // MTVU is reading in back of write_pos
		// FIXME greg: there is a bug somewhere in the queue pointer
		// management. It creates a deadlock/corruption in SotC intro (before
		// the first menu). I added a 4KB safety net which seem to avoid to
		// trigger the bug.
		// Note: a wait lock instead of a yield also helps to avoid the bug.
		if (readPos > m_write_pos + size + _4kb)
			break; // Enough free front space
		{          // Let MTVU run to free up buffer space
			KickStart();
			// Locking might trigger a full flush of the ring buffer. Yield
			// will be more aggressive, and only flush the minimal size.
			// Performance will be smoother but it will consume extra CPU cycle
			// on the EE thread (not an issue on 4 cores).
			std::this_thread::yield();
		}
	}
}

// Makes sure theres enough room in the ring buffer
// to write a continuous 'size * sizeof(u32)' bytes
void VU_Thread::ReserveSpace(s32 size)
{
	pxAssert(m_write_pos < buffer_size);
	pxAssert(size < buffer_size);
	pxAssert(size > 0);

	if (m_write_pos + size > (buffer_size - 1))
	{
		WaitOnSize(1); // Size of MTVU_NULL_PACKET
		Write(MTVU_NULL_PACKET);
		// Reset local write pointer/position
		m_write_pos = 0;
		CommitWritePos();
	}

	WaitOnSize(size);
}

// Use this when reading read_pos from ee thread
__fi s32 VU_Thread::GetReadPos()
{
	return m_ato_read_pos.load(std::memory_order_acquire);
}

// Use this when reading write_pos from vu thread
__fi s32 VU_Thread::GetWritePos()
{
	return m_ato_write_pos.load(std::memory_order_acquire);
}

// Gets the effective write pointer after
__fi u32* VU_Thread::GetWritePtr()
{
	pxAssert(m_write_pos < buffer_size);
	return &buffer[m_write_pos];
}

__fi void VU_Thread::CommitWritePos()
{
	m_ato_write_pos.store(m_write_pos, std::memory_order_release);

	if (MTVU_ALWAYS_KICK)
		KickStart();
	if (MTVU_SYNC_MODE)
		WaitVU();
}

__fi void VU_Thread::CommitReadPos()
{
	m_ato_read_pos.store(m_read_pos, std::memory_order_release);
}

__fi u32 VU_Thread::Read()
{
	u32 ret = buffer[m_read_pos];
	m_read_pos++;
	return ret;
}

__fi void VU_Thread::Read(void* dest, u32 size)
{
	memcpy(dest, &buffer[m_read_pos], size);
	m_read_pos += size_u32(size);
}

__fi void VU_Thread::ReadRegs(VIFregisters* dest)
{
	VIFregistersMTVU* src = (VIFregistersMTVU*)&buffer[m_read_pos];
	dest->cycle = src->cycle;
	dest->mode = src->mode;
	dest->num = src->num;
	dest->mask = src->mask;
	dest->itop = src->itop;
	dest->top = src->top;
	m_read_pos += size_u32(sizeof(VIFregistersMTVU));
}

__fi void VU_Thread::Write(u32 val)
{
	GetWritePtr()[0] = val;
	m_write_pos += 1;
}

__fi void VU_Thread::Write(void* src, u32 size)
{
	memcpy(GetWritePtr(), src, size);
	m_write_pos += size_u32(size);
}

__fi void VU_Thread::WriteRegs(VIFregisters* src)
{
	VIFregistersMTVU* dest = (VIFregistersMTVU*)GetWritePtr();
	dest->cycle = src->cycle;
	dest->mode = src->mode;
	dest->num = src->num;
	dest->mask = src->mask;
	dest->top = src->top;
	dest->itop = src->itop;
	m_write_pos += size_u32(sizeof(VIFregistersMTVU));
}

// Returns Average number of vu Cycles from last 4 runs
// Used for vu cycle stealing hack
u32 VU_Thread::Get_vuCycles()
{
	return (vuCycles[0].load(std::memory_order_acquire) +
			vuCycles[1].load(std::memory_order_acquire) +
			vuCycles[2].load(std::memory_order_acquire) +
			vuCycles[3].load(std::memory_order_acquire)) >>
		   2;
}

void VU_Thread::Get_MTVUChanges()
{
	// Note: Atomic communication is with Gif_Unit.cpp Gif_HandlerAD_MTVU
	u32 interrupts = mtvuInterrupts.load(std::memory_order_relaxed);
	if (!interrupts)
		return;
	
	if (interrupts & InterruptFlagSignal)
	{
		std::atomic_thread_fence(std::memory_order_acquire);
		const u64 signal = gsSignal.load(std::memory_order_relaxed);
		// If load of signal was moved after clearing the flag, the other thread could write a new value before we load without noticing the double signal
		// Prevent that with release semantics
		mtvuInterrupts.fetch_and(~InterruptFlagSignal, std::memory_order_release);
		GUNIT_WARN("SIGNAL firing");
		const u32 signalMsk = (u32)(signal >> 32);
		const u32 signalData = (u32)signal;
		if (CSRreg.SIGNAL)
		{
			GUNIT_WARN("Queue SIGNAL");
			gifUnit.gsSIGNAL.queued = true;
			//DevCon.Warning("Firing pending signal");
			gifUnit.gsSIGNAL.data[0] = signalData;
			gifUnit.gsSIGNAL.data[1] = signalMsk;
		}
		else
		{
			CSRreg.SIGNAL = true;
			GSSIGLBLID.SIGID = (GSSIGLBLID.SIGID & ~signalMsk) | (signalData & signalMsk);

			if (!GSIMR.SIGMSK)
				gsIrq();
		}
	}
	if (interrupts & InterruptFlagFinish)
	{
		mtvuInterrupts.fetch_and(~InterruptFlagFinish, std::memory_order_relaxed);
		GUNIT_WARN("Finish firing");
		CSRreg.FINISH = true;
		gifUnit.gsFINISH.gsFINISHFired = false;

		if (!gifRegs.stat.APATH)
			Gif_FinishIRQ();
	}
	if (interrupts & InterruptFlagLabel)
	{
		mtvuInterrupts.fetch_and(~InterruptFlagLabel, std::memory_order_acquire);
		// If other thread updates gsLabel for a second interrupt, that's okay.  Worst case we think there's a label interrupt but gsLabel is 0
		// We do not want the exchange of gsLabel to move ahead of clearing the flag, or the other thread could add more work before we clear the flag, resulting in an update with the flag unset
		// acquire semantics should supply that guarantee
		const u64 label = gsLabel.exchange(0, std::memory_order_relaxed);
		GUNIT_WARN("LABEL firing");
		const u32 labelMsk = (u32)(label >> 32);
		const u32 labelData = (u32)label;
		GSSIGLBLID.LBLID = (GSSIGLBLID.LBLID & ~labelMsk) | (labelData & labelMsk);
	}
	if (interrupts & InterruptFlagVUEBit)
	{
		mtvuInterrupts.fetch_and(~InterruptFlagVUEBit, std::memory_order_relaxed);
		
		VU0.VI[REG_VPU_STAT].UL &= ~0xFF00;
		//DevCon.Warning("E-Bit registered %x", VU0.VI[REG_VPU_STAT].UL);
	}
	if (interrupts & InterruptFlagVUTBit)
	{
		mtvuInterrupts.fetch_and(~InterruptFlagVUTBit, std::memory_order_relaxed);
		VU0.VI[REG_VPU_STAT].UL &= ~0xFF00;
		VU0.VI[REG_VPU_STAT].UL |= 0x0400;
		//DevCon.Warning("T-Bit registered %x", VU0.VI[REG_VPU_STAT].UL);
		hwIntcIrq(7);
	}
}

void VU_Thread::KickStart()
{
	semaEvent.NotifyOfWork();
}

bool VU_Thread::IsDone()
{
	return GetReadPos() == GetWritePos();
}

void VU_Thread::WaitVU()
{
	MTVU_LOG("MTVU - WaitVU!");
	semaEvent.WaitForEmpty();
}

void VU_Thread::ExecuteVU(u32 vu_addr, u32 vif_top, u32 vif_itop, u32 fbrst)
{
	MTVU_LOG("MTVU - ExecuteVU!");
	Get_MTVUChanges(); // Clear any pending interrupts
	ReserveSpace(5);
	Write(MTVU_VU_EXECUTE);
	Write(vu_addr);
	Write(vif_top);
	Write(vif_itop);
	Write(fbrst);
	CommitWritePos();
	gifUnit.TransferGSPacketData(GIF_TRANS_MTVU, NULL, 0);
	KickStart();
	u32 cycles = std::min(Get_vuCycles(), 3000u);
	cpuRegs.cycle += cycles * EmuConfig.Speedhacks.EECycleSkip;
	VU0.cycle += cycles * EmuConfig.Speedhacks.EECycleSkip;
	Get_MTVUChanges();
}

void VU_Thread::VifUnpack(vifStruct& _vif, VIFregisters& _vifRegs, u8* data, u32 size)
{
	MTVU_LOG("MTVU - VifUnpack!");
	u32 vif_copy_size = (uptr)&_vif.StructEnd - (uptr)&_vif.tag;
	ReserveSpace(1 + size_u32(vif_copy_size) + size_u32(sizeof(VIFregistersMTVU)) + 1 + size_u32(size));
	Write(MTVU_VIF_UNPACK);
	Write(&_vif.tag, vif_copy_size);
	WriteRegs(&_vifRegs);
	Write(size);
	Write(data, size);
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteMicroMem(u32 vu_micro_addr, void* data, u32 size)
{
	MTVU_LOG("MTVU - WriteMicroMem!");
	ReserveSpace(3 + size_u32(size));
	Write(MTVU_VU_WRITE_MICRO);
	Write(vu_micro_addr);
	Write(size);
	Write(data, size);
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteDataMem(u32 vu_data_addr, void* data, u32 size)
{
	MTVU_LOG("MTVU - WriteDataMem!");
	ReserveSpace(3 + size_u32(size));
	Write(MTVU_VU_WRITE_DATA);
	Write(vu_data_addr);
	Write(size);
	Write(data, size);
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteVIRegs(REG_VI* viRegs)
{
	MTVU_LOG("MTVU - WriteRegs!");
	ReserveSpace(1 + size_u32(32));
	Write(MTVU_VU_WRITE_VIREGS);
	Write(viRegs, size_u32(32));
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteVFRegs(VECTOR* vfRegs)
{
	MTVU_LOG("MTVU - WriteRegs!");
	ReserveSpace(1 + size_u32(32*4));
	Write(MTVU_VU_WRITE_VFREGS);
	Write(vfRegs, size_u32(32*4));
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteCol(vifStruct& _vif)
{
	MTVU_LOG("MTVU - WriteCol!");
	ReserveSpace(1 + size_u32(sizeof(_vif.MaskCol)));
	Write(MTVU_VIF_WRITE_COL);
	Write(&_vif.MaskCol, sizeof(_vif.MaskCol));
	CommitWritePos();
	KickStart();
}

void VU_Thread::WriteRow(vifStruct& _vif)
{
	MTVU_LOG("MTVU - WriteRow!");
	ReserveSpace(1 + size_u32(sizeof(_vif.MaskRow)));
	Write(MTVU_VIF_WRITE_ROW);
	Write(&_vif.MaskRow, sizeof(_vif.MaskRow));
	CommitWritePos();
	KickStart();
}
