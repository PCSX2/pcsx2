/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2016-2021  PCSX2 Dev Team
*  Copyright (C) 2016 Wisi
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
#include "ps2/Iop/IopHw_Internal.h"
#include "ps2/HwInternal.h"
#include "ps2/pgif.h"
#include "IopHw.h"
#include "IopDma.h"
#include "Common.h"

//NOTES (TODO):
/*
- 8 and 16 bit access to the PGPU regs is not emulated... is it ever used? Emulating it would be tricky.


------

Much of the code is ("very") unoptimized, because it is a bit cleaner and more complete this way.

All the PS1 GPU info comes from psx-spx: http://problemkaputt.de/psx-spx.htm


*/

//Debug printf:
// Set to 1 to log PGIF HW IO
#define LOG_REG 0

#if LOG_REG
	#define REG_LOG pgifConLog
#else
	#define REG_LOG(...) do {} while(0)
#endif

// Set to 1 to log PGPU DMA
#define LOG_PGPU_DMA 1

#if LOG_PGPU_DMA
	#define PGPU_DMA_LOG pgifConLog
#else
	#define PGPU_DMA_LOG(...) do {} while(0)
#endif

u32 old_gp0_value = 0;
void fillFifoOnDrain(void);
void drainPgpuDmaLl(void);
void drainPgpuDmaNrToGpu(void);
void drainPgpuDmaNrToIop(void);

void ringBufPut(struct ringBuf_t* rb, u32* data)
{
	if (rb->count < rb->size)
	{
		//there is available space
		*(rb->buf + rb->head) = *data;
		if ((++(rb->head)) >= rb->size)
			rb->head = 0; //wrap back when the end is reached
		rb->count++;
	}
	else
	{
		// This should never happen. If it does, the code is bad somewhere.
		Console.Error("PGIF FIFO overflow! sz= %X", rb->size);
	}
}

void ringBufGet(struct ringBuf_t* rb, u32* data)
{
	if (rb->count > 0)
	{
		//there is available data
		*data = *(rb->buf + rb->tail);
		if ((++(rb->tail)) >= rb->size)
			rb->tail = 0; //wrap back when the end is reached
		rb->count--;
	}
	else
	{
		// This should never happen. If it does, the code is bad somewhere.
		Console.Error("PGIF FIFO underflow! sz= %X", rb->size);
	}
}

void ringBufferClear(struct ringBuf_t* rb)
{
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;
	// wisi comment:
	// Yes, the memset should be commented-out. The reason is that it shouldn't really be necessary (but I am not sure).
	// It is better not to be enabled, because clearing the new huge PGIF FIFO, can waste significant time.
	//memset(rb->buf, 0, rb->size * sizeof(u32));
	return;
}


//Ring buffers definition and initialization:
//Command (GP1) FIFO, size= 0x8 words:
#define PGIF_CMD_RB_SIZE 0x8
struct ringBuf_t rb_gp1; //Ring buffer control variables.
u32 pgif_gp1_buffer[PGIF_CMD_RB_SIZE] = {0}; //Ring buffer data.

//Data (GP0) FIFO - the so-called (in PS1DRV) "GFIFO", (real) size= 0x20 words:
//Using small (the real) FIFO size, disturbs MDEC video (and other stuff),
//because the MDEC does DMA instantly, while this emulation drains the FIFO,
//only when the PS1DRV gets data from it, which depends on IOP-EE sync, among other things.
//The reson it works in the real hardware, is that the MDEC DMA would run in the pauses of GPU DMA,
//thus the GPU DMA would never get data, MDEC hasn't yet written to RAM yet (too early).
#define PGIF_DAT_RB_SIZE 0x20000
struct ringBuf_t rb_gp0; //Ring buffer control variables.
u32 pgif_gp0_buffer[PGIF_DAT_RB_SIZE] = {0}; //Ring buffer data.
dma_t dma;


//TODO: Make this be called by IopHw.cpp / psxHwReset()... but maybe it should be called by the EE reset func,
//given that the PGIF is in the EE ASIC, on the other side of the SBUS.
void pgifInit()
{
	rb_gp1.buf = pgif_gp1_buffer;
	rb_gp1.size = PGIF_CMD_RB_SIZE;
	ringBufferClear(&rb_gp1);

	rb_gp0.buf = pgif_gp0_buffer;
	rb_gp0.size = PGIF_DAT_RB_SIZE;
	ringBufferClear(&rb_gp0);

	pgpu.stat.write(0);
	pgif.ctrl.write(0);
	old_gp0_value = 0;


	dmaRegs.madr.address = 0;
	dmaRegs.bcr.write(0);
	dmaRegs.chcr.write(0);
	//pgpuDmaTadr = 0;

	dma.state.ll_active = 0;
	dma.state.to_gpu_active = 0;
	dma.state.to_iop_active = 0;

	dma.ll_dma.data_read_address = 0;
	dma.ll_dma.current_word = 0;
	dma.ll_dma.total_words = 0;
	dma.ll_dma.next_address = 0;

	dma.normal.total_words = 0;
	dma.normal.current_word = 0;
	dma.normal.address = 0;
}

//Interrupt-related (IOP, EE and DMA):

void triggerPgifInt(int subCause)
{
	// Probably we should try to mess with delta here.
	hwIntcIrq(15);
	cpuSetEvent();
}

void getIrqCmd(u32 data)
{
	//For the IOP - GPU. This is triggered by the GP0(1Fh) - Interrupt Request (IRQ1) command.
	//This may break stuff, because it doesn't detect whether this is really a GP0() command or data...
	//Since PS1 HW also didn't recognize that is data or not, we should left it enabled.
	{
		if ((data & 0xFF000000) == 0x1F000000)
		{
			pgpu.stat.bits.IRQ1 = 1;
			iopIntcIrq(1);
		}
	}
}

void ackGpuIrq1()
{
	//Acknowledge for the IOP GPU interrupt.
	pgpu.stat.bits.IRQ1 = 0;
}

void pgpuDmaIntr(int trigDma)
{
	//For the IOP GPU DMA channel.
	//trigDma: 1=normal,ToGPU; 2=normal,FromGPU, 3=LinkedList

	// psxmode: 25.09.2016 at this point, the emulator works even when removing this interrupt call. how? why?
#if PREVENT_IRQ_ON_NORM_DMA_TO_GPU == 1
	if (trigDma != 1) //Interrupt on ToGPU DMA breaks some games. TODO: Why?
#endif
		psxDmaInterrupt(2);
}


//Pass-through & intercepting functions:

u32 immRespHndl(u32 cmd, u32 data)
{
	//Handles the GP1(10h) command, that requires immediate response.
	//The data argument is the old data of the register (shouldn't be critical what it contains).
	switch ((cmd & 0x7))
	{
		case 0:
		case 1:
		case 6:
		case 7:
			break; //Returns Nothing (old value in GPUREAD remains unchanged)
		case 2:
			data = pgif.imm_response.reg.e2 & 0x000FFFFF;
			break; //Read Texture Window setting  ;GP0(E2h) ;20bit/MSBs=Nothing
		case 3:
			data = pgif.imm_response.reg.e3 & 0x0007FFFF;
			break; //Read Draw area top left      ;GP0(E3h) ;19bit/MSBs=Nothing
		case 4:
			data = pgif.imm_response.reg.e4 & 0x0007FFFF;
			break; //Read Draw area bottom right  ;GP0(E4h) ;19bit/MSBs=Nothing
		case 5:
			data = pgif.imm_response.reg.e5 & 0x003FFFFF;
			break; //Read Draw offset             ;GP0(E5h) ;22bit
	}
	return data;
}

void handleGp1Command(u32 cmd)
{
	//Check GP1() command and configure PGIF accordingly.
	//Commands 0x00 - 0x01, 0x03, 0x05 - 0x08 are fully handled in ps1drv.
	const u32 cmdNr = ((cmd >> 24) & 0xFF) & 0x3F;
	switch (cmdNr)
	{
		case 2: //Acknowledge GPU IRQ
			ackGpuIrq1();
			break;
		case 4: //DMA Direction / Data Request. The PS1DRV doesn't care of this... Should we do something on pgif ctrl?
			pgpu.stat.bits.DDIR = cmd & 0x3;
			//Since DREQ bit is dependent on DDIR bits, we should set it as soon as command is processed.
			switch (pgpu.stat.bits.DDIR) //29-30 bit (0=Off, 1=FIFO, 2=CPUtoGP0, 3=GPUREADtoCPU)    ;GP1(04h).0-1
				{
					case 0x00: // When GP1(04h)=0 ---> Always zero (0)
						pgpu.stat.bits.DREQ = 0;
						break;
					case 0x01: // When GP1(04h)=1 ---> FIFO State  (0=Full, 1=Not Full)
						if (rb_gp0.count < (rb_gp0.size - PGIF_DAT_RB_LEAVE_FREE))
						{
							pgpu.stat.bits.DREQ = 1;
						}
						else
						{
							pgpu.stat.bits.DREQ = 0;
						}
						break;
					case 0x02: // When GP1(04h)=2 ---> Same as GPUSTAT.28
						pgpu.stat.bits.DREQ = pgpu.stat.bits.RDMA;
						drainPgpuDmaLl(); //See comment in this function.
						break;
					case 0x03: //When GP1(04h)=3 ---> Same as GPUSTAT.27
						pgpu.stat.bits.DREQ = pgpu.stat.bits.RSEND;
						break;
				}
			break;
		default:
			break;
	}
}

u32 getUpdPgpuStatReg()
{
	//PS1DRV does set bit RSEND on (probably - took from command print) GP0(C0h), should we do something?
	//The PS1 program pools this bit to determine if there is data in the FIFO, it can get. Then starts DMA to get it.

	//The PS1 program will not send the DMA direction command (GP1(04h)) and will not start DMA until this bit (27) becomes set.
	pgpu.stat.bits.RSEND = pgif.ctrl.bits.data_from_gpu_ready;
	return pgpu.stat.get();
}

u8 getGP0RbC_Count()
{
	//Returns "correct" element-in-FIFO count, even if extremely large buffer is used.
	return std::min(rb_gp0.count, 0x1F);
}

u32 getUpdPgifCtrlReg()
{
	//Update fifo counts before returning register value
	pgif.ctrl.bits.GP0_fifo_count = getGP0RbC_Count();
	pgif.ctrl.bits.GP1_fifo_count = rb_gp1.count;
	return pgif.ctrl.get();
}


void rb_gp1_Get(u32* data)
{
	ringBufGet(&rb_gp1, data);
	handleGp1Command(*data); //Setup GP1 right after reading command from FIFO.
}

void rb_gp0_Get(u32* data)
{
	if (rb_gp0.count > 0)
	{
		ringBufGet(&rb_gp0, data);
		getIrqCmd(*data); //Checks if an IRQ CMD passes through here and triggers IRQ when it does.
	}
	else
	{
		*data = old_gp0_value;
	}
}

//PS1 GPU registers I/O handlers:

void psxGPUw(int addr, u32 data)
{
	REG_LOG("PGPU write 0x%08X = 0x%08X", addr, data);
	if (addr == HW_PS1_GPU_DATA)
	{
		ringBufPut(&rb_gp0, &data);
	}
	else if (addr == HW_PS1_GPU_STATUS)
	{
		// Check for Cmd 0x10-0x1F
		u8 imm_check = (data >> 28);
		imm_check &= 0x3;
		if (imm_check == 1)
		{
			//Handle immediate-response command. Commands are NOT sent to the Fifo apparently (PS1DRV).
			old_gp0_value = immRespHndl(data, old_gp0_value);
		}
		else
		{
			triggerPgifInt(0);
			ringBufPut(&rb_gp1, &data);
		}
	}
}

u32 psxGPUr(int addr)
{
	u32 data = 0;
	if (addr == HW_PS1_GPU_DATA)
	{
		rb_gp0_Get(&data);
	}
	else if (addr == HW_PS1_GPU_STATUS)
	{
		data = getUpdPgpuStatReg();
	}
	if (addr != HW_PS1_GPU_STATUS)
		REG_LOG("PGPU read  0x%08X = 0x%08X", addr, data);

	return data;
}

// PGIF registers I/O handlers:

void PGIFw(int addr, u32 data)
{
	//if (((addr != PGIF_CTRL) || (addr != PGPU_STAT) || ((addr == PGIF_CTRL) && (getUpdPgifCtrlReg() != data))) && (addr != PGPU_STAT))
		REG_LOG("PGIF write 0x%08X = 0x%08X  0x%08X  EEpc= %08X  IOPpc= %08X ", addr, data, getUpdPgifCtrlReg(), cpuRegs.pc, psxRegs.pc);

	switch (addr)
	{
		case PGPU_STAT:
			pgpu.stat.write(data); //Should all bits be writable?
			break;
		case PGIF_CTRL:
			pgif.ctrl.write(data);
			fillFifoOnDrain(); //Now this checks the 0x8 bit of the PGIF_CTRL reg, so  it here too,
			break; 				//so that it gets updated immediately once it is set.
		case IMM_E2:
			pgif.imm_response.reg.e2 = data;
			break;
		case IMM_E3:
			pgif.imm_response.reg.e3 = data;
			break;
		case IMM_E4:
			pgif.imm_response.reg.e4 = data;
			break;
		case IMM_E5:
			pgif.imm_response.reg.e5 = data;
			break;
		case PGPU_CMD_FIFO:
			Console.Error("PGIF CMD FIFO write by EE (SHOULDN'T HAPPEN) 0x%08X = 0x%08X", addr, data);
			break;
		case PGPU_DAT_FIFO:
			ringBufPut(&rb_gp0, &data);
			// 	Console.WriteLn( "\n\r PGIF REVERSE !!! DATA write 0x%08X = 0x%08X  IF_CTRL= %08X   PGPU_STAT= %08X  CmdCnt 0x%X \n\r",  addr, data,  getUpdPgifCtrlReg(),  getUpdPgpuStatReg(), rb_gp1.count);
			drainPgpuDmaNrToIop();
			break;
		default:
			DevCon.Error("PGIF write to unknown location 0xx% , data: %x", addr, data);
			break;
	}
}

// Read PGIF Hardware Registers.
u32 PGIFr(int addr)
{
	u32 data = 0;
	switch (addr)
	{
		case PGPU_STAT:
			data = pgpu.stat.get();
			break;
		case PGIF_CTRL:
			data = getUpdPgifCtrlReg();
			break;
		case IMM_E2:
			data = pgif.imm_response.reg.e2;
			break;
		case IMM_E3:
			data = pgif.imm_response.reg.e3;
			break;
		case IMM_E4:
			data = pgif.imm_response.reg.e4;
			break;
		case IMM_E5:
			data = pgif.imm_response.reg.e5;
			break;
		case PGPU_CMD_FIFO:
			rb_gp1_Get(&data);
			break;
		case PGPU_DAT_FIFO:
			fillFifoOnDrain();
			rb_gp0_Get(&data);
			break;
		default:
			DevCon.Error("PGIF read from unknown location 0xx%", addr);
			break;
	}
	//if (addr != PGPU_DAT_FIFO)
		//if ((addr != PGIF_CTRL) || (getUpdPgifCtrlReg() != data))
		//	if (addr != PGPU_STAT)
		//		REG_LOG("PGIF read %08X = %08X  GPU_ST %08X  IF_STAT %08X IOPpc %08X EEpc= %08X ", addr, data, getUpdPgpuStatReg(), getUpdPgifCtrlReg(), psxRegs.pc, cpuRegs.pc);

	return data;
}

void PGIFrQword(u32 addr, void* dat)
{
	u32* data = (u32*)dat;

	if (addr == PGPU_CMD_FIFO)
	{
		//shouldn't happen
		Console.Error("PGIF QW CMD read =ERR!");
	}
	else if (addr == PGPU_DAT_FIFO)
	{
		fillFifoOnDrain();
		rb_gp0_Get(data + 0);
		rb_gp0_Get(data + 1);
		rb_gp0_Get(data + 2);
		rb_gp0_Get(data + 3);

		fillFifoOnDrain();
	}
	else
	{
		Console.WriteLn("PGIF QWord Read from address %08X  ERR - shouldnt happen!", addr);
		Console.WriteLn("Data = %08X %08X %08X %08X ", *(u32*)(data + 0), *(u32*)(data + 1), *(u32*)(data + 2), *(u32*)(data + 3));
	}
}

void PGIFwQword(u32 addr, void* dat)
{
	u32* data = (u32*)dat;
	DevCon.Warning("WARNING PGIF WRITE BY PS1DRV ! - NOT KNOWN TO EVER BE DONE!");
	Console.WriteLn("PGIF QW write  0x%08X = 0x%08X %08X %08X %08X ", addr, *(u32*)(data + 0), *(u32*)(data + 1), *(u32*)(data + 2), *(u32*)(data + 3));

	if (addr == PGPU_CMD_FIFO)
	{
		Console.Error("PGIF QW CMD write!");
	}
	else if (addr == PGPU_DAT_FIFO)
	{
		ringBufPut(&rb_gp0, (u32*)(data + 0));
		ringBufPut(&rb_gp0, (u32*)(data + 1));
		ringBufPut(&rb_gp0, (u32*)(data + 2));
		ringBufPut(&rb_gp0, (u32*)(data + 3));
		drainPgpuDmaNrToIop();
	}
}

//DMA-emulating functions:

//This function is used as a global FIFO-DMA-fill function and both Linked-list normal DMA call it,
void fillFifoOnDrain()
{
	//Skip filing FIFO with elements, if PS1DRV hasn't set this bit.
	//Maybe it could be cleared once FIFO has data?
	if (!pgif.ctrl.bits.fifo_GP0_ready_for_data)
		return;


	//This is done here in a loop, rather than recursively in each function, because a very large buffer causes stack oveflow.
	while ((rb_gp0.count < ((rb_gp0.size) - PGIF_DAT_RB_LEAVE_FREE)) && ((dma.state.to_gpu_active) || (dma.state.ll_active)))
	{
		drainPgpuDmaLl();
		drainPgpuDmaNrToGpu();
	}
	//Clear bit as DMA will be run - normally it should be cleared only once the current request finishes, but the IOP won't notice anything anyway.
	//WARNING: Current implementation assume that GPU->IOP DMA uses this flag, so we only clear it here if the mode is not GPU->IOP.
	if (((dma.state.ll_active) || (dma.state.to_gpu_active)) && (!dma.state.to_iop_active))
	{
		pgif.ctrl.bits.fifo_GP0_ready_for_data = 0;
	}
}

void drainPgpuDmaLl()
{
	u32 data = 0;
	if (!dma.state.ll_active)
		return;

	//Some games (Breath of Fire 3 US) set-up linked-list DMA, but don't immediatelly have the list correctly set-up,
	//so the result is that this function loops indefinitely, because of some links pointing back to themselves, forming a loop.
	//The solution is to only start DMA once the GP1(04h) - DMA Direction / Data Request command has been set to the value 0x2 (CPU->GPU DMA)

	//Buffer full - needs to be drained first.
	if (rb_gp0.count >= ((rb_gp0.size) - PGIF_DAT_RB_LEAVE_FREE))
		return;

	if (dmaRegs.chcr.bits.MAS)
		DevCon.Error("Unimplemented backward memory step on PGPU DMA Linked List");

	if (dma.ll_dma.current_word >= dma.ll_dma.total_words)
	{
		if (dma.ll_dma.next_address == DMA_LL_END_CODE)
		{
			//Reached end of linked list
			dma.state.ll_active = 0;
			dmaRegs.madr.address = 0x00FFFFFF;
			dmaRegs.chcr.bits.BUSY = 0; //Transfer completed => clear busy flag
			pgpuDmaIntr(3);
			PGPU_DMA_LOG("PGPU DMA Linked List Finished");
		}
		else
		{
			//Or the beginning of a new one
			data = iopMemRead32(dma.ll_dma.next_address);
			PGPU_DMA_LOG( "Next PGPU LL DMA header= %08X  ", data);
			dmaRegs.madr.address = data & 0x00FFFFFF; //Copy the address in MADR.
			dma.ll_dma.data_read_address = dma.ll_dma.next_address + 4; //start of data section of packet
			dma.ll_dma.current_word = 0;
			dma.ll_dma.total_words = (data >> 24) & 0xFF; // Current length of packet and future address of header word.
			dma.ll_dma.next_address = dmaRegs.madr.address;
		}
	}
	else
	{
		//We are in the middle of linked list transfer
		data = iopMemRead32(dma.ll_dma.data_read_address);
		PGPU_DMA_LOG( "PGPU LL DMA data= %08X  addr %08X ", data, dma.ll_dma.data_read_address);
		ringBufPut(&rb_gp0, &data);
		dma.ll_dma.data_read_address += 4;
		dma.ll_dma.current_word++;
	}
}

void drainPgpuDmaNrToGpu()
{
	u32 data = 0;
	if (!dma.state.to_gpu_active)
		return;

	//Buffer full - needs to be drained first.
	if (rb_gp0.count >= ((rb_gp0.size) - PGIF_DAT_RB_LEAVE_FREE))
		return;

	if (dma.normal.current_word < dma.normal.total_words)
	{
		data = iopMemRead32(dma.normal.address);
		PGPU_DMA_LOG( "To GPU Normal DMA data= %08X  addr %08X ", data, dma.ll_dma.data_read_address);

		ringBufPut(&rb_gp0, &data);
		if (dmaRegs.chcr.bits.MAS)
		{
			DevCon.Error("Unimplemented backward memory step on TO GPU DMA");
		}
		dmaRegs.madr.address += 4;
		dma.normal.address += 4;
		dma.normal.current_word++;

		// decrease block amount only if full block size were drained.
		if ((dma.normal.current_word % dmaRegs.bcr.bit.block_size) == 0)
			dmaRegs.bcr.bit.block_amount -= 1;
	}
	if (dma.normal.current_word >= dma.normal.total_words)
	{
		//Reached end of sequence = complete
		dma.state.to_gpu_active = 0;
		dmaRegs.chcr.bits.BUSY = 0;
		pgpuDmaIntr(1);
		PGPU_DMA_LOG("To GPU DMA Normal FINISHED");
	}
}

void drainPgpuDmaNrToIop()
{
	u32 data = 0;
	if (!dma.state.to_iop_active || rb_gp0.count <= 0)
		return;

	if (dma.normal.current_word < dma.normal.total_words)
	{
		//This is not the best way, but... is there another?
		ringBufGet(&rb_gp0, &data);
		iopMemWrite32(dma.normal.address, data);
		if (dmaRegs.chcr.bits.MAS)
		{
			DevCon.Error("Unimplemented backward memory step on FROM GPU DMA");
		}
		dmaRegs.madr.address += 4;
		dma.normal.address += 4;
		dma.normal.current_word++;
		//dmaRegs.madr.address += 4; //It is unclear if this should be done exactly so... // kozarovv: WHY EVEN DO THAT AGAIN?
		if ((dma.normal.current_word % dmaRegs.bcr.bit.block_size) == 0)
		{
			// decrease block amount only if full block size were drained.
			dmaRegs.bcr.bit.block_amount -= 1;
		}
		PGPU_DMA_LOG("GPU->IOP ba: %x , cw: %x , tw: %x" ,dmaRegs.bcr.bit.block_amount, dma.normal.current_word, dma.normal.total_words);
	}
	if (dma.normal.current_word >= dma.normal.total_words)
	{
		dma.state.to_iop_active = 0;
		dmaRegs.chcr.bits.BUSY = 0;
		pgpuDmaIntr(2);
	}

	if (rb_gp0.count > 0)
		drainPgpuDmaNrToIop();
}


void processPgpuDma()
{
	if (!dmaRegs.chcr.bits.TSM)
	{
		Console.Error("SyncMode 0 on GPU DMA!");
	}
	if (dmaRegs.chcr.bits.TSM == 3)
	{
		Console.Warning("SyncMode 3! Assuming SyncMode 1");
		dmaRegs.chcr.bits.TSM = 1;
	}
	PGPU_DMA_LOG("Starting GPU DMA! CHCR %08X  BCR %08X  MADR %08X ", dmaRegs.chcr.get(), dmaRegs.bcr.get(), dmaRegs.madr.address);

	//Linked List Mode
	if (dmaRegs.chcr.bits.TSM == 2)
	{
		//To GPU
		if (dmaRegs.chcr.bits.DIR)
		{
			dma.state.ll_active = 1;
			dma.ll_dma.next_address = (dmaRegs.madr.address & 0x00FFFFFF); //The address in IOP RAM where to load the first header word from
			dma.ll_dma.current_word = 0;
			dma.ll_dma.total_words = 0;
			PGPU_DMA_LOG("LL DMA FILL");

			//fill a single word in fifo now, because otherwise PS1DRV won't know that a transfer is pending.
			fillFifoOnDrain();
			return;
		}
		else
		{
			Console.Error("Error: Linked list from GPU DMA!");
			return;
		}
	}
	dma.normal.current_word = 0;
	dma.normal.address = dmaRegs.madr.address & 0x1FFFFFFF; // Sould we allow whole range? Maybe for psx SPR?
	dma.normal.total_words = (dmaRegs.bcr.bit.block_size * dmaRegs.bcr.get_block_amount());

	if (dmaRegs.chcr.bits.DIR) // to gpu
	{
		PGPU_DMA_LOG("NORMAL DMA TO GPU");
		dma.state.to_gpu_active = 1;
		fillFifoOnDrain();
	}
	else
	{
		PGPU_DMA_LOG("NORMAL DMA FROM GPU");
		dma.state.to_iop_active = 1;
		drainPgpuDmaNrToIop();
	}
}

u32 psxDma2GpuR(u32 addr)
{
	u32 data = 0;
	addr &= 0x1FFFFFFF;
	switch (addr)
	{
		case PGPU_DMA_MADR:
			data = dmaRegs.madr.address;
			break;
		case PGPU_DMA_BCR:
			data = dmaRegs.bcr.get();
			break;
		case PGPU_DMA_CHCR:
			data = dmaRegs.chcr.get();
			break;
		case PGPU_DMA_TADR:
			data = pgpuDmaTadr;
			Console.Error("PGPU DMA read TADR!");
			break;
		default:
			Console.Error("Unknown PGPU DMA read 0x%08X", addr);
			break;
	}
	if (addr != PGPU_DMA_CHCR)
		PGPU_DMA_LOG("PGPU DMA read  0x%08X = 0x%08X", addr, data);
	return data;
}

void psxDma2GpuW(u32 addr, u32 data)
{
	PGPU_DMA_LOG("PGPU DMA write 0x%08X = 0x%08X", addr, data);
	addr &= 0x1FFFFFFF;
	switch (addr)
	{
		case PGPU_DMA_MADR:
			dmaRegs.madr.address = (data & 0x00FFFFFF);
			break;
		case PGPU_DMA_BCR:
			dmaRegs.bcr.write(data);
			break;
		case PGPU_DMA_CHCR:
			dmaRegs.chcr.write(data);
			if (dmaRegs.chcr.bits.BUSY)
			{
				processPgpuDma();
			}
			break;
		case PGPU_DMA_TADR:
			pgpuDmaTadr = data;
			Console.Error("PGPU DMA write TADR! ");
			break;
		default:
			Console.Error("Unknown PGPU DMA write 0x%08X = 0x%08X", addr, data);
			break;
	}
}