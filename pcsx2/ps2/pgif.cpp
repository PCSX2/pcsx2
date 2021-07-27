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
	#define REG_LOG DevCon.WriteLn
#else
	#define REG_LOG(...) do {} while(0)
#endif

// Set to 1 to log PGPU DMA
#define LOG_PGPU_DMA 0

#if LOG_PGPU_DMA
	#define PGPU_DMA_LOG DevCon.WriteLn
#else
	#define PGPU_DMA_LOG(...) do {} while(0)
#endif

//HW Registers:
struct Regs_t
{
	struct pgpuRegs_t
	{
		u32 stat; //Read-only to IOP
		u32 data; //Read-only to IOP
	} pgpu;

	struct pgifRegs_t
	{
		//PGifIfStat
		u32 ctrl;
		u32 ctrlOld;

		//Hold the values of the immediate-response registers.
		u32 pgif1;
		u32 pgif2;
		u32 pgif3;
		u32 pgif4;
	} pgif;
} regs;

//Internal dma flags:
struct dma_t
{
	struct dmaState_t
	{
		bool llActive;
		bool toGpuNormalActive;
		bool toIopNormalActive;
	} state;

	struct llDma_t
	{
		u32 pgpuDmaTrAddr;
		u32 totalWords; //total number of words
		u32 currentWord; //current word number
		u32 nextAddr;
	} llDma;

	struct normalDma_t
	{
		u32 totalWords; //total number of words in Normal DMA
		u32 currentWord; //current word number in Normal DMA
		u32 addr;
	} normal;
} dma;

//Generic FIFO-related:
struct ringBuf_t
{
	u32* buf;
	int size;
	int count;
	int head;
	int tail;
};

void fillFifoOnDrain(void);
void drainPgpuDmaLl(void);
void drainPgpuDmaNrToGpu(void);
void drainPgpuDmaNrToIop(void);

void ringBufPut(struct ringBuf_t* rb, u32* data)
{
	if (rb->count < rb->size)
	{ //there is available space
		*(rb->buf + rb->head) = *data;
		if ((++(rb->head)) >= rb->size)
			rb->head = 0; //wrap back when the end is reached
		rb->count++;
	}
	else
	{
		// This should never happen. If it does, the code is bad somewhere.
		Console.WriteLn("############################# PGIF FIFO overflow! sz= %X", rb->size);
	}
}

void ringBufGet(struct ringBuf_t* rb, u32* data)
{
	if (rb->count > 0)
	{ //there is available data
		*data = *(rb->buf + rb->tail);
		if ((++(rb->tail)) >= rb->size)
			rb->tail = 0; //wrap back when the end is reached
		rb->count--;
	}
	else
	{
		// This should never happen. If it does, the code is bad somewhere.
		Console.WriteLn("############################# PGIF FIFO underflow! sz= %X", rb->size);
	}
}

void ringBufClear(struct ringBuf_t* rb)
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
struct ringBuf_t pgifCmdRbC; //Ring buffer control variables.
u32 pgiffCmdRbD[PGIF_CMD_RB_SIZE] = {0}; //Ring buffer data.

//Data (GP0) FIFO - the so-called (in PS1DRV) "GFIFO", (real) size= 0x20 words:
//#define PGIF_DAT_RB_SIZE 0x20
//FIXED: Values 0xD54 and higher cause PS logo to disappair.
//Using small (the real) FIFO size, disturbs MDEC video (and other stuff),
//because the MDEC does DMA instantly, while this emulation drains the FIFO,
//only when the PS1DRV gets data from it, which depends on IOP-EE sync, among other things.
//The reson it works in the real hardware, is that the MDEC DMA would run in the pauses of GPU DMA,
//thus the GPU DMA would never get data, MDEC hasn't yet written to RAM yet (too early).
#define PGIF_DAT_RB_SIZE 0x20000
struct ringBuf_t pgifDatRbC; //Ring buffer control variables.
u32 pgiffDatRbD[PGIF_DAT_RB_SIZE] = {0}; //Ring buffer data.



//TODO: Make this be called by IopHw.cpp / psxHwReset()... but maybe it should be called by the EE reset func,
//given that the PGIF is in the EE ASIC, on the other side of the SBUS.
void pgifInit()
{
	pgifCmdRbC.buf = pgiffCmdRbD;
	pgifCmdRbC.size = PGIF_CMD_RB_SIZE;
	ringBufClear(&pgifCmdRbC);
	pgifDatRbC.buf = pgiffDatRbD;
	pgifDatRbC.size = PGIF_DAT_RB_SIZE;
	ringBufClear(&pgifDatRbC);

	regs.pgpu.stat = 0;
	regs.pgif.pgif1 = 0;
	regs.pgif.pgif2 = 0;
	regs.pgif.pgif3 = 0;
	regs.pgif.pgif4 = 0;
	regs.pgif.ctrl = 0;
	regs.pgpu.data = 0;


	pgpuDmaMadr = 0;
	pgpuDmaBcr = 0;
	pgpuDmaChcr = 0;
	pgpuDmaTadr = 0;

	dma.state.llActive = 0;
	dma.state.toGpuNormalActive = 0;
	dma.state.toIopNormalActive = 0;

	dma.llDma.pgpuDmaTrAddr = 0;
	dma.llDma.currentWord = 0; //
	dma.llDma.totalWords = 0; //total number of words
	dma.llDma.nextAddr = 0;

	dma.normal.totalWords = 0; //total number of words in Normal DMA
	dma.normal.currentWord = 0; //current word number in Normal DMA
	dma.normal.addr = 0;

	return;
}

//Basically resetting the PS1 GPU.
void pgifReset()
{
	//pgifInit();
	//return;

	ringBufClear(&pgifDatRbC);
	ringBufClear(&pgifCmdRbC);

	regs.pgpu.stat = 0;
	regs.pgif.pgif1 = 0;
	regs.pgif.pgif2 = 0;
	regs.pgif.pgif3 = 0;
	regs.pgif.pgif4 = 0;
	regs.pgif.ctrl = 0;
	regs.pgpu.data = 0;

	return;
}

//Interrupt-related (IOP, EE and DMA):

void triggerPgifInt(int subCause)
{
	//For the PGIF on EE side.
	//	Console.WriteLn("TRIGGERRING PGIF IRQ! %08X  %08X ", psHu32(INTC_STAT), psHu32(INTC_MASK));
	//Left-overs from tests:
	//iopCycleEE = -1; //0000;
	//iopBreak = 0;
	//--cpuTestINTCInts();
	//--cpuSetEvent();
	hwIntcIrq(15);
	cpuSetEvent();
	return;
}

void getIrqCmd(u32 data)
{
	//For the IOP GPU. This is triggered by the GP0(1Fh) - Interrupt Request (IRQ1) command.
	//This may break stuff, because it doesn't detect whether this is really a GP0() command or data...
	//WARNIG: Consider disabling this.
	if (PGPU_IRQ1_ENABLE)
	{
		if ((data & 0xFF000000) == 0x1F000000)
		{
			iopIntcIrq(1);
			regs.pgpu.stat |= PGPU_STAT_IRQ1;
		}
	}
}

void ackGpuIrq()
{
	//Acknowledge for the IOP GPU interrupt.
	//This is a "null"-function in PS1DRV...
	//maybe because hardware takes care of it...
	//but does any software use it?
	regs.pgpu.stat &= ~PGPU_STAT_IRQ1;
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
	//Handles the GP1(10h) command, that requires immediate responce.
	//The data argument is the old data of the register (shouldn't be critical what it contains).
	//Descriptions here are taken directly from psx-spx.
	switch ((cmd & 0xFF))
	{
		case 0x00:
		case 0x01:
			break; //Returns Nothing (old value in GPUREAD remains unchanged)
		case 0x02:
			data = regs.pgif.pgif1 & 0x000FFFFF;
			break; //Read Texture Window setting  ;GP0(E2h) ;20bit/MSBs=Nothing
		case 0x03:
			data = regs.pgif.pgif2 & 0x000FFFFF;
			break; //Read Draw area top left      ;GP0(E3h) ;20bit/MSBs=Nothing
		case 0x04:
			data = regs.pgif.pgif3 & 0x000FFFFF;
			break; //Read Draw area bottom right  ;GP0(E4h) ;20bit/MSBs=Nothing
		case 0x05:
			data = regs.pgif.pgif4 & 0x003FFFFF;
			break; //Read Draw offset             ;GP0(E5h) ;22bit
	}
	return data;
}

void ckhCmdSetPgif(u32 cmd)
{
	//Check GP1() command and configure PGIF accordingly.
	//Maybe all below are hanled by PS1DRV and this functions is only wasting time and memory and cusing errors...but that doesn't seem to be the case.
	u32 cmdNr = ((cmd >> 24) & 0xFF) & 0x3F; //Higher commands are known to mirror lower commands.
	u32 data = 0x00000000;
	switch (cmdNr)
	{
		case 0x00:
			ringBufClear(&pgifDatRbC);
			//	ringBufClear(&pgifCmdRbC); //Makes more sense to leave it disabled - if the PS1 program has written a reset command, it would be the last in the FIFO anyway.
			ackGpuIrq();
			//	pgpu1reg = 0; pgpu2reg = 0; pgpu3reg = 0; pgpu4reg = 0;
			//More stuff to clear here....
			pgifReset();
			ringBufPut(&pgifDatRbC, &data); //write a reset command for the PS1DRV to get.
			break;
		case 0x01: //Reset Command Buffer (only?)... This would actually mean the Data (GP0()) register buffer.
			//ringBufClear(&pgifCmdRbC); //as above.
			ringBufClear(&pgifDatRbC);
			break;
		case 0x02: //Acknowledge GPU IRQ
			ackGpuIrq();
			break;
		case 0x04: //DMA Direction / Data Request
			//The PS1DRV doesn't seem to take care of this...
			//Removing the following two, "damages" the PS logo, so they are probably necessary.
			//The PS1 program will not start DMA GPU->IOP (only) if this is missing.
			regs.pgpu.stat &= ~STAT_DIR_BY_CMD_BITS; //TODO: Which bits should be controlled here?
			regs.pgpu.stat |= (cmd << 29) & STAT_DIR_BY_CMD_BITS; //0x60000000; //0x20000000 bit 30 is handled by PS1DRV
			//regs.pgpu.stat &= ~0x60000000; //TODO: Which bits should be controlled here?
			//regs.pgpu.stat |= (cmd <<29) & 0x40000000; //0x60000000; //0x20000000 bit 30 is handled by PS1DRV
			//0-1  DMA Direction (0=Off, 1=FIFO, 2=CPUtoGP0, 3=GPUREADtoCPU) ;GPUSTAT.29-30
			//2-23 Not used (zero)
			drainPgpuDmaLl(); //See comment in this function.
			break;
	}
}

u32 getUpdPgpuStatReg()
{
	if (SKIP_UPD_PGPU_STAT)
	{
		return regs.pgpu.stat;
	}
	//The code below is not necessary! ... but not always...why?
	//Does PS1DRV does set bit 27 - reverse direction (VRAM->CPU) in some conditions... but not always...TODO: Find why.
	//The PS1 program pools this bit to determine if there is data in the FIFO, it can get. Then starts DMA to get it.
	//The PS1 program will not send the DMA direction command (GP1(04h)) and will not start DMA until this bit (27) becomes set.
	//Maybe move this to the regs.pgif.ctrl write handler...?
	regs.pgpu.stat &= ~0x08000000; //VRAM->CPU data ready
	regs.pgpu.stat &= ~0x02000000; //common "ready" flag
		//regs.pgpu.stat |= 0x10000000;  //GPU is ready to receive DMA data. PS1DRV seems to set and clear this bit,
		//but it is cleared in the next code anyway, because it seems safer.
		//regs.pgpu.stat |= 0x04000000;  //GPU is ready to receive GP0() cmds.
		//Setting this bit is best left to the PS1DRV (PS logo polygons start missing otherwise).
#if GPU_STAT_DIR_BY_IF_CTRL == 0
	if ((regs.pgif.ctrl & 0x10) || (regs.pgpu.stat & 0x20000000))
	{ //Reverse direction bit GPU->CPU.
#else
	regs.pgpu.stat &= ~0x20000000;
	if (regs.pgif.ctrl & 0x10)
	{
		regs.pgpu.stat |= 0x20000000;
#endif
		//Or maybe bit 29 of the GPU STATUS reg, should copy the state of bit 4 of the PGIF_CTRL reg?
		regs.pgpu.stat &= ~0x04000000; //Same with receiving GP0() commands.
		if (pgifDatRbC.count > 0) //Data FIFO has data.
			regs.pgpu.stat |= 0x08000000; //Could skip the second check, but that could cause buffer underflow on direct read.
				//Should the above be set only when there are at least 16 words of data?
	}

	switch ((regs.pgpu.stat >> 29) & 0x3)
	{
		case 0x00:
			break; // When GP1(04h)=0 ---> Always zero (0)
		case 0x01: // When GP1(04h)=1 ---> FIFO State  (0=Full, 1=Not Full)
			if (pgifDatRbC.count < (pgifDatRbC.size - 0))
				regs.pgpu.stat |= 0x02000000; //Not (yet) full, so set the bit.
			break;
		case 0x02: // When GP1(04h)=2 ---> Same as GPUSTAT.28
			if (regs.pgpu.stat & 0x10000000) //Ready to receive DMA Block  (0=No, 1=Ready)  ;GP0(...) ;via GP0
				regs.pgpu.stat |= 0x02000000;
			break;
		case 0x03: //When GP1(04h)=3 ---> Same as GPUSTAT.27
			if (regs.pgpu.stat & 0x08000000) //Ready to send VRAM to CPU   (0=No, 1=Ready)  ;GP0(C0h) ;via GPUREAD
				regs.pgpu.stat |= 0x02000000;
			break;
	}
	return regs.pgpu.stat;
}

//This returns "correct" element-in-FIFO count, even if extremely large buffer is used.
int getDatRbC_Count()
{
	return std::min(pgifDatRbC.count, 0x1F);
}

void setPgifCtrlReg(u32 data)
{
	regs.pgif.ctrl = (data & (~PGIF_CTRL_RO_MASK)) | (regs.pgif.ctrl & PGIF_CTRL_RO_MASK);
}

u32 getUpdPgifCtrlReg()
{
	regs.pgif.ctrl &= 0xFFFFE0FF;
	if (pgifCmdRbC.count == 0) //fake that DATA FIFO is empty, so that PS1DRV processes any commands on the GP1() register first.
		regs.pgif.ctrl |= ((getDatRbC_Count() & 0x1F) << 8) & 0x1F00;
	regs.pgif.ctrl &= 0xFFF8FFFF;
	regs.pgif.ctrl |= ((pgifCmdRbC.count & 0x07) << 16) & 0x70000;
	regs.pgif.ctrl &= 0x7FFFFFFF; //bit 31= (DMA?) transfer active. Has to be clear, for PS1DRV to continue. -
	//The above is most likely the 9-th bit of the FIFO counter. It is 0x20 in size most likly... though it could be 0x2F.
	//It doesn't look as it is (ever) used as an overflow flag. It is usually added to the main part of the counter as a 9-th bit
	//and then comaprisons to some number of elements are made.
	//Always fill FIFO (for drain by PS1DRV) at least 0x10 words - it will refuse to drain it.
	if (pgifCmdRbC.count == 0) //fake that DATA FIFO is empty, so that PS1DRV processes any commands on the GP1() register first.
		regs.pgif.ctrl |= ((getDatRbC_Count() & 0x20) << 26) & 0x80000000;
	//if ((dma.state.llActive == 1) || (dma.state.toGpuNormalActive == 1))
	//	regs.pgif.ctrl |= 0x80000000;
	return regs.pgif.ctrl;
}


void cmdRingBufGet(u32* data)
{
	//Used by PGIF/PS1DRV.
	ringBufGet(&pgifCmdRbC, data);

	if (CMD_INTERCEPT_AT_RD)
	{
		ckhCmdSetPgif(*data); //Let's assume that the PGIF hardware processes GPU commands just when the EE reads them from its FIFO.
	}
}

void datRingBufGet(u32* data)
{
	//if (pgifDatRbC.count >= (PGIF_DAT_RB_SIZE -1)) { Console.WriteLn( "PGIF DAT BUF FULL %08X ",  pgifDatRbC.count);
	if (pgifDatRbC.count > 0)
	{
		ringBufGet(&pgifDatRbC, data);
		getIrqCmd(*data); //Checks if an IRQ CMD passes through here a triggers IRQ when it does.
	}
	else
	{ //Return last value in register:
#if 0
		if ((((lastGpuCmd >>24) & 0xFF) & 0x30) == 0x10) { //Handle the immediate-response command.
			//Cmds 0x10 - 0x1F are all 0x10, and 0x40-0xFF mirror 0x00-0x3F.
			regs.pgpu.data = immRespHndl(lastGpuCmd, regs.pgpu.data); //Would it be better if this is done when data is read at datRingBufGet()?
		}
#endif
		*data = regs.pgpu.data;
	}
}

//PS1 GPU registers I/O handlers:

void psxGPUw(int addr, u32 data)
{
	REG_LOG("PGPU write 0x%08X = 0x%08X", addr, data);
	if (addr == HW_PS1_GPU_DATA)
	{
		//regs.pgpu.data = data; //just until it is certain that nothing uses the old value in the latch.
		ringBufPut(&pgifDatRbC, &data);
	}
	else if (addr == HW_PS1_GPU_STATUS)
	{ //Command register GP1 (write-only).
		if ((((data >> 24) & 0xFF) & 0x30) == 0x10)
		{ //Handle the immediate-response command.
			//Cmds 0x10 - 0x1F are all 0x10, and 0x40-0xFF mirror 0x00-0x3F.
			regs.pgpu.data = immRespHndl(data, regs.pgpu.data); //Would it be better if this is done when data is read at datRingBufGet()?
		}
		else
		{ //Should the "immediate-response" command not be sent to the GPU (PS1DRV)..?
			triggerPgifInt(0);
			ringBufPut(&pgifCmdRbC, &data);
		}
		if (CMD_INTERCEPT_AT_WR)
		{
			ckhCmdSetPgif(data);
		}
	}
}

u32 lastGpuCmd = 0;
u32 psxGPUr(int addr)
{
	u32 data = 0;
	if (addr == HW_PS1_GPU_DATA)
	{
		//data = regs.pgpu.data;
		//The following is commented-out, because it seems more logical to return GP1() CMD responces only when Data FIFO is empty.
		//if ((lastGpuCmd & 0xFF000000) == 0x10000000) {
		//	data = immRespHndl(lastGpuCmd, regs.pgpu.data);
		//	lastGpuCmd = 0;
		//} else {
		datRingBufGet(&data);
		//}
	}
	else if (addr == HW_PS1_GPU_STATUS)
	{
		data = getUpdPgpuStatReg();
	}
	if (addr != HW_PS1_GPU_STATUS) //prints way too many times (pools)
		REG_LOG("PGPU read  0x%08X = 0x%08X  EEpc= %08X ", addr, data, cpuRegs.pc);

	return data;
}

// PGIF registers I/O handlers:
// Write PGIF Hardware Registers.
void PGIFw(int addr, u32 data)
{
	if (((addr != PGIF_CTRL) || (addr != PGPU_STAT) || ((addr == PGIF_CTRL) && (getUpdPgifCtrlReg() != data))) && (addr != PGPU_STAT))
		REG_LOG("PGIF write 0x%08X = 0x%08X  0x%08X  EEpc= %08X  IOPpc= %08X ", addr, data, getUpdPgifCtrlReg(), cpuRegs.pc, psxRegs.pc);

	switch (addr)
	{
		case PGPU_STAT:
			regs.pgpu.stat = data; //Should all bits be writable?
			break;
		case PGIF_CTRL:
			setPgifCtrlReg(data);
			regs.pgif.ctrlOld = regs.pgif.ctrl; //data
			fillFifoOnDrain(); //Now this checks the 0x8 bit of the PGIF_CTRL reg, so  it here too,
			break; //so that it gets updated immediately once it is set.
		case PGIF_1:
			regs.pgif.pgif1 = data;
			break;
		case PGIF_2:
			regs.pgif.pgif2 = data;
			break;
		case PGIF_3:
			regs.pgif.pgif3 = data;
			break;
		case PGIF_4:
			regs.pgif.pgif4 = data;
			break;
		case PGPU_CMD_FIFO:
			Console.WriteLn("PGIF CMD FIFO write by EE (SHOULDN'T HAPPEN) 0x%08X = 0x%08X", addr, data);
			break;
		case PGPU_DAT_FIFO:
			//regs.pgpu.data = data;
			ringBufPut(&pgifDatRbC, &data);
			// 	Console.WriteLn( "\n\r PGIF REVERSE !!! DATA write 0x%08X = 0x%08X  IF_CTRL= %08X   PGPU_STAT= %08X  CmdCnt 0x%X \n\r",  addr, data,  getUpdPgifCtrlReg(),  getUpdPgpuStatReg(), pgifCmdRbC.count);
			drainPgpuDmaNrToIop();
			break;
		default:
			DevCon.Warning("PGIF write to unknown location 0xx% , data: %x", addr, data);
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
			data = regs.pgpu.stat;
			break;
		case PGIF_CTRL:
			data = getUpdPgifCtrlReg();
			break;
		case PGIF_1:
			data = regs.pgif.pgif1;
			break;
		case PGIF_2:
			data = regs.pgif.pgif2;
			break;
		case PGIF_3:
			data = regs.pgif.pgif3;
			break;
		case PGIF_4:
			data = regs.pgif.pgif4;
			break;
		case PGPU_CMD_FIFO:
			cmdRingBufGet(&data);
			break;
		case PGPU_DAT_FIFO:
			fillFifoOnDrain();
			datRingBufGet(&data);
			break;
		default:
			DevCon.Warning("PGIF read from unknown location 0xx%", addr);
			break;
	}
	if (addr != PGPU_DAT_FIFO)
		if ((addr != PGIF_CTRL) || (getUpdPgifCtrlReg() != data))
			if (addr != PGPU_STAT)
				REG_LOG("PGIF read %08X = %08X  GPU_ST %08X  IF_STAT %08X IOPpc %08X EEpc= %08X ", addr, data, getUpdPgpuStatReg(), getUpdPgifCtrlReg(), psxRegs.pc, cpuRegs.pc);

	return data;
}

void PGIFrQword(u32 addr, void* dat)
{
	u32* data = (u32*)dat;

	if (addr == PGPU_CMD_FIFO)
	{ //shouldn't happen
		Console.WriteLn("PGIF QW CMD read =ERR!");
	}
	else if (addr == PGPU_DAT_FIFO)
	{
		fillFifoOnDrain();
		datRingBufGet(data + 0);
		datRingBufGet(data + 1);
		datRingBufGet(data + 2);
		datRingBufGet(data + 3);

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
	{ //shouldn't happen
		Console.WriteLn("PGIF QW CMD write =ERR!");
	}
	else if (addr == PGPU_DAT_FIFO)
	{
		ringBufPut(&pgifDatRbC, (u32*)(data + 0));
		ringBufPut(&pgifDatRbC, (u32*)(data + 1));
		ringBufPut(&pgifDatRbC, (u32*)(data + 2));
		ringBufPut(&pgifDatRbC, (u32*)(data + 3));
		drainPgpuDmaNrToIop();
	}
}

//DMA-emulating functions:

//This function is used as a global FIFO-DMA-fill function and both Linked-list normal DMA call it,
//regardless which DMA mode runs.
void fillFifoOnDrain()
{
	if ((getUpdPgifCtrlReg() & 0x8) == 0)
		return; //Skip filing FIFO with elements, if PS1DRV hasn't set this bit.
	//Maybe it could be cleared once FIFO has data?
	//This bit could be an enabler for the DREQ (EE->IOP for SIF2=PGIF) line. The FIFO is filled only when this is set
	//and the IOP acknowledges it.
	drainPgpuDmaLl();
	drainPgpuDmaNrToGpu();
	//This is done here in a loop, rather than recursively in each function, because a very large buffer causes stack oveflow.
	while ((pgifDatRbC.count < ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE)) && ((dma.state.toGpuNormalActive) || (dma.state.llActive)))
	{
		drainPgpuDmaLl();
		drainPgpuDmaNrToGpu();
	}
	//Flags' names: dma.state.llActive is for linked-list, dma.state.toGpuNormalActive is for normal IOP->GPU
	if ((dma.state.llActive) || (dma.state.toGpuNormalActive))
		if (!dma.state.toIopNormalActive)
			regs.pgif.ctrl &= ~0x8;
	//Clear bit as DMA will be run - normally it should be cleared only once the current request finishes, but the IOP won't notice anything anyway.
	//WARNING: Check if GPU->IOP DMA uses this flag, and if it does (it would make sense for it to use it as well),
	//then only clear it here if the mode is not GPU->IOP.
}

u32 dntPrt = 0; //Debug only
void drainPgpuDmaLl()
{
	u32 data = 0;
	if (!dma.state.llActive)
		return; //process only on transfer active

	//Some games (Breath of Fire 3 US) set-up linked-list DMA, but don't immediatelly have the list correctly set-up,
	//so the result is that this function loops indefinitely, because of some links pointing back to themselves, forming a loop.
	//The solution is to only start DMA once the GP1(04h) - DMA Direction / Data Request command has been set to the value 0x2 (CPU->GPU DMA)
	//This func will be caled by this command as well.
	//if ((regs.pgpu.stat & 0x60000000) != 0x40000000) return;

	if (pgifDatRbC.count >= ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE))
		return; //buffer full - needs to be drained first.

	if ((dma.llDma.nextAddr == 0x000C8000) || (dma.llDma.nextAddr == 0x0) || (dma.llDma.nextAddr == 0x3))
		dntPrt++;
	//	if (dntPrt <5)
	//Console.WriteLn( "dma.llDmaFILL nxtAdr %08X  trAddr %08X dma.llDma.totalWords %08X  dma.llDma.currentWord %08X  IF_CTRL= %08X ",  dma.llDma.nextAddr, dma.llDma.pgpuDmaTrAddr, dma.llDma.totalWords, dma.llDma.currentWord, getUpdPgifCtrlReg(),  cpuRegs.pc, psxRegs.pc);

	if (dma.llDma.currentWord >= dma.llDma.totalWords)
	{ //Reached end of sequence, or the beginning of a new one
		//if ((dma.llDma.nextAddr == DMA_LL_END_CODE) || (dma.llDma.nextAddr == 0x00000000)) { //complete
		//why does BoF3 US get to a link pointing to address 0??
		if (dma.llDma.nextAddr == DMA_LL_END_CODE)
		{
			dntPrt = 0;
			dma.state.llActive = 0;
			pgpuDmaMadr = 0x00FFFFFF;
			//pgpuDmaMadr = dma.llDma.nextAddr; //The END_CODE should go in the MADR reg.
			//But instead copy the whole header code, each time one is encuntered - makes a bit more sense.
			pgpuDmaChcr &= ~DMA_TRIGGER; //DMA is no longer active (in transfer)
			pgpuDmaChcr &= ~DMA_START_BUSY; //Transfer completed => clear busy flag
			pgpuDmaIntr(3);
			PGPU_DMA_LOG("PGPU DMA LINKED LIST FINISHED ");
		}
		else
		{
			data = iopMemRead32(dma.llDma.nextAddr);
			//		Console.WriteLn( "PGPU LL DMA HDR= %08X  ", data);
			//data = psxHu32(dma.llDma.nextAddr); //The (next) current header word.
			//Console.WriteLn( "PGPU LL DMA data= %08X  ", data);
			pgpuDmaMadr = data; //Copy the header word in MADR.
			//It is unknown whether the whole word should go here, but because this is a "Memory ADdress Register", leave only the address.
			pgpuDmaMadr &= 0x00FFFFFF; // correct or not?
			dma.llDma.pgpuDmaTrAddr = dma.llDma.nextAddr + 4; //start of data section of packet
			dma.llDma.currentWord = 0;
			dma.llDma.totalWords = (data >> 24) & 0xFF; // Current length of packet and future address of header word.
			dma.llDma.nextAddr = data & 0x00FFFFFF;
		}
	}
	else
	{
		//if (dma.llDma.currentWord < dma.llDma.totalWords) {
		data = iopMemRead32(dma.llDma.pgpuDmaTrAddr); //0; //psxHu32(dma.llDma.pgpuDmaTrAddr); //Get the word of the packet from IOP RAM.
		//PGPU_DMA_LOG( "PGPU LL DMA data= %08X  addr %08X ", data, dma.llDma.pgpuDmaTrAddr);
		ringBufPut(&pgifDatRbC, &data);
		dma.llDma.pgpuDmaTrAddr += 4;
		dma.llDma.currentWord++;
	}
}

void drainPgpuDmaNrToGpu()
{
	u32 data = 0;

	if (!dma.state.toGpuNormalActive)
		return; //process only on transfer active
	if (pgifDatRbC.count >= ((pgifDatRbC.size) - PGIF_DAT_RB_LEAVE_FREE))
		return; //buffer full - needs to be drained first.

	if (dma.normal.currentWord < dma.normal.totalWords)
	{
		data = iopMemRead32(dma.normal.addr); //Get the word of the packet from IOP RAM.
		//	PGPU_DMA_LOG( "PGPU NRM DMA data= %08X  addr %08X ", data, dma.llDma.pgpuDmaTrAddr);

		ringBufPut(&pgifDatRbC, &data);
		dma.normal.addr += 4;
		dma.normal.currentWord++;
		//The following two are common to all DMA channels, and in future, some global hanler may be used... or not.
		//pgpuDmaMadr += 4; //It is unclear if this should be done exactly so...
		pgpuDmaMadr = 4 + pgpuDmaMadr;
		if ((dma.normal.currentWord % (pgpuDmaBcr & 0xFFFF)) == 0) //Because this is actually the "next word number", it can be compared to the size of the block. //End of block reached.
			if (pgpuDmaBcr >= 0x10000)
				pgpuDmaBcr = pgpuDmaBcr - 0x10000; //pgpuDmaBcr -= 0x10000;
					//This is very inoptimized...
	}
	if (dma.normal.currentWord >= dma.normal.totalWords)
	{ //Reached end of sequence = complete
		dma.state.toGpuNormalActive = 0;
		pgpuDmaChcr &= ~DMA_TRIGGER; //DMA is no longer active (in transfer)
		pgpuDmaChcr &= ~DMA_START_BUSY; //Transfer completed => clear busy flag
		pgpuDmaIntr(1);
		PGPU_DMA_LOG("PGPU DMA Norm IOP->GPU FINISHED ");
	}
}

void drainPgpuDmaNrToIop()
{
	u32 data;
	if (!dma.state.toIopNormalActive)
		return;
	if (pgifDatRbC.count <= 0)
		return;
	/*	if (dma.normal.currentWord >= dma.normal.totalWords) {
		dma.state.toIopNormalActive = 0;
		pgpuDmaChcr &= ~DMA_TRIGGER; //DMA is no longer active (in transfer)
		pgpuDmaChcr &= ~DMA_START_BUSY; //Transfer completed => clear busy flag
		psxDmaInterrupt(2);
	} else { */
	if (dma.normal.currentWord < dma.normal.totalWords)
	{ //This is not the best way, but... is there another?
		ringBufGet(&pgifDatRbC, &data);
		iopMemWrite32(dma.normal.addr, data);
		pgpuDmaMadr = 4 + pgpuDmaMadr; //dma.normal.addr += 4;
		dma.normal.currentWord++;
		//The following two are common to all DMA channels, and in future, some global hanler may be used... or not.
		pgpuDmaMadr += 4; //It is unclear if this should be done exactly so...
		if ((dma.normal.currentWord % (pgpuDmaBcr & 0xFFFF)) == 0) //Because this is actually the "next word number", it can be compared to the size of the block. //End of block reached.
			if (pgpuDmaBcr >= 0x10000)
				pgpuDmaBcr = pgpuDmaBcr - 0x10000; // pgpuDmaBcr -= 0x10000;
					//This is very unoptimized... better use separate block counter.
	}
	if (dma.normal.currentWord >= dma.normal.totalWords)
	{
		dma.state.toIopNormalActive = 0;
		pgpuDmaChcr &= ~DMA_TRIGGER; //DMA is no longer active (in transfer)
		pgpuDmaChcr &= ~DMA_START_BUSY; //Transfer completed => clear busy flag
		pgpuDmaIntr(2);
		PGPU_DMA_LOG("PGPU DMA GPU->IOP FINISHED ");
	}

	if (pgifDatRbC.count > 0)
		drainPgpuDmaNrToIop();
}


void processPgpuDma()
{
	//For normal mode & linked list.
	if (!(pgpuDmaChcr & DMA_START_BUSY))
		return; //not really neessary in this case.
	pgpuDmaChcr |= DMA_TRIGGER; //DMA is active (in transfer)
	PGPU_DMA_LOG("PGPU DMA EXEC CHCR %08X  BCR %08X  MADR %08X ", pgpuDmaChcr, pgpuDmaBcr, pgpuDmaMadr);

	if (pgpuDmaChcr & DMA_LINKED_LIST)
	{
		PGPU_DMA_LOG("PGPU DMA LINKED LIST WARNING!!! dir= %d ", (pgpuDmaChcr & 1)); //for (i=0;i<0x0FFFFFFF;i++) {}

		if (pgpuDmaChcr & DMA_DIR_FROM_RAM)
		{ //Linked-list only supported on direction to GPU.
			dma.state.llActive = 1;
			//for the very start, the address of the first word should come for the address, stored at MADR
			dma.llDma.nextAddr = (pgpuDmaMadr & 0x1FFFFFFF); //The address in IOP RAM where to load the first header word from
			//u32 data = iopMemRead32(dma.llDma.nextAddr);
			//Console.WriteLn( "PGPU LL DMA data= %08X  ", data);

			dma.llDma.currentWord = 0; //current word
			dma.llDma.totalWords = 0; //total number of words
			PGPU_DMA_LOG("LL DMA FILL ");

			fillFifoOnDrain(); //drainPgpuDmaLl(); //fill a single word in fifo now, because otherwise PS1DRV won't know that there is aything in it and it wouldn't know that a transfer is pending.
			return;
		}
		else
		{
			Console.WriteLn("ERR: Linked list GPU DMA TO IOP!");
			return;
		}
	}
	PGPU_DMA_LOG("WARNING! NORMAL DMA TO EE "); //for (i=0;i<0x1FFFFFFF;i++) {}
	dma.normal.currentWord = 0;
	dma.normal.addr = pgpuDmaMadr & 0x1FFFFFFF;
	dma.normal.totalWords = ((pgpuDmaBcr & 0xFFFF) * ((pgpuDmaBcr >> 16) & 0xFFFF));

	if (pgpuDmaChcr & DMA_DIR_FROM_RAM)
	{ //to peripheral
		dma.state.toGpuNormalActive = 1;
		fillFifoOnDrain(); //drainPgpuDmaNrToGpu();
	}
	else
	{
		PGPU_DMA_LOG("                 NORMAL DMA TO IOP ");
		dma.state.toIopNormalActive = 1;
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
			data = pgpuDmaMadr;
			break;
		case PGPU_DMA_BCR:
			data = pgpuDmaBcr;
			break;
		case PGPU_DMA_CHCR:
			//When using the memory for registers, rather than the local variables (see the definition of pgpuDmaChcr),
			//bit 0x01000000 gets set again, very soon after transfer ended(by the game) and then when the game pools it to detect dma completion,
			//it times-out. the above condition works around that... but that is an indicator that st. is wrong. -how could the game reset it so soon?
			//maybe it just writes to it twice...? or maybe it comes as a high 16-bit write... - todo: find the writer.

			if ((!dma.state.llActive) && (!dma.state.toGpuNormalActive) && (!dma.state.toIopNormalActive))
				pgpuDmaChcr &= ~0x01000000;
			data = pgpuDmaChcr;
			break;
		case PGPU_DMA_TADR:
			data = pgpuDmaTadr;
			Console.Warning("PGPU DMA read TADR!");
			break;
		default:
			//Possible unaligned read?
			Console.Warning("Unknown PGPU DMA read 0x%08X", addr);
			break;
	}
	if (addr != PGPU_DMA_CHCR)
		Console.WriteLn("PGPU DMA read  0x%08X = 0x%08X", addr, data);
	return data;
}

void psxDma2GpuW(u32 addr, u32 data)
{
	PGPU_DMA_LOG("PGPU DMA write 0x%08X = 0x%08X", addr, data);
	addr &= 0x1FFFFFFF;
	switch (addr)
	{
		case PGPU_DMA_MADR:
			pgpuDmaMadr = (data & 0x00FFFFFF);
			break;
		case PGPU_DMA_BCR:
			pgpuDmaBcr = data;
			break;
		case PGPU_DMA_CHCR:
			pgpuDmaChcr = data;
			if (pgpuDmaChcr & DMA_START_BUSY)
			{
				processPgpuDma();
			}
			break;
		case PGPU_DMA_TADR:
			pgpuDmaTadr = data;
			Console.WriteLn("PGPU DMA write TADR! ");
			break;
		default:
			//Possible unaligned Write?
			Console.Warning("Unknown PGPU DMA write 0x%08X = 0x%08X", addr, data);
			break;
	}
}


///*
//#########################################################################################################
//Other debug & test code, not directly relatedto the PGIF:

//-------------------------
//debug print from IOP:

//BIOS POST
void ps12PostOut(u32 mem, u8 value)
{
	Console.WriteLn("XXXXXXXXXXXXXXXXXXXXXXX PS12 POST = %02X ", value);
	//iopMemWrite32(0x0000B9B0, 1); //a bit of high-level emulation to enable DUART TTY in PS1 kernel...which doesn't work
}


void psDuartW(u32 mem, u8 value)
{
	Console.WriteLn("XXXXXXXXXXXXXXXXXXXXXXX DUART reg %08X = %02X >%c<", mem, value, value);
	if ((mem & 0x1FFFFFFF) == 0x1F802023)
	{
		Console.WriteLn("XXXXXXXXXXXXXXXXXXXXXXX DUART A = %02X >%c<", value, value);
	}
	else if ((mem & 0x1FFFFFFF) == 0x1F80202B)
	{
		Console.WriteLn("XXXXXXXXXXXXXXXXXXXXXXX DUART B = %02X >%c<", value, value);
	}
}

u8 psExp2R8(u32 mem)
{
	if ((mem & 0x1FFFFFFF) == 0x1F802000)
	{
		return 0x18;
	}
	return 0xFF;
}

//0x80051080
u32 getIntTmrKReg(u32 mem, u32 data)
{
	u32 outd = data;
	if (regs.pgif.ctrl != 0)
	{
		Console.WriteLn("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
		//outd = 0;
		outd = 0x7FFFFFFF;
	}
	return outd;
}

void HPCoS_print(u32 mem, u32 data)
{
	int i;
	if ((mem & 0x1FFFFFFF) == 0x1103A0)
	{
		//Console.WriteLn("HPCoS BufPnt = %08X ", data);
		char str[201];
		//if (data == 0) {
		for (i = 0; i < 200; i++)
		{
			str[i] = iopMemRead8(0x117328 + i);
			if (str[i] == 0)
				break;
		}
		str[i] = 0;
		//	Console.WriteLn(" FFFFFFFFFFFFFFFFFFFFFFFFFFFF  >%s<  ", str);
		//}
	} //else if ((mem & 0x1FFFFFFF) == 0x117328) {
	//	Console.WriteLn("HPCoS BufAdr = %08X  >%c<", data, data);

	//}
}

int doAnyIopLs = 0;
int startPrntPc = 0;
void anyIopLS(u32 addr, u32 data, int Wr)
{
	if (doAnyIopLs == 0)
		return;
//#define nrRuns 0xFFFFFF
#define nrRuns 0xFFFFFFF
	//if ((startPrntPc > nrRuns) && (startPrntPc < (nrRuns + 2000)))
	//	Console.WriteLn( "MEM %d  0x%08X = 0x%08X IOPpc= %08X ",  Wr, addr, data, psxRegs.pc);
	//else
	if (startPrntPc <= (nrRuns + 2000))
		startPrntPc++;
}

#define psxHu(mem) (*(u32*)&iopHw[(mem)&0xffff])
void dma6_OTClear()
{
	if ((psxHu(0x1f8010e8) & 0x01000000) == 0)
		return;
	u32 madr = psxHu(0x1f8010e0);
	u32 blkSz = psxHu(0x1f8010e4) & 0xFFFF;
	//u32 blkCnt = (psxHu(0x1f8010e4) >> 16) & 0xFFFF;
	u32 addrPnt = madr;
	u32 pntCnt = 0;
	Console.WriteLn("DMA6 OT clr madr %08X  bcr %08X  chcr %08X", psxHu(0x1f8010e0), psxHu(0x1f8010e4), psxHu(0x1f8010e8));

	while (pntCnt < blkSz)
	{ //Very unoptimized...
		iopMemWrite32(addrPnt, ((addrPnt - 4) & 0x00FFFFFF));
		addrPnt -= 4;
		pntCnt++;
	}
	iopMemWrite32((addrPnt + 4), 0x00FFFFFF);

	//psxHu(0x1f8010e0)
	//psxHu(0x1f8010e4)

	psxHu(0x1f8010e8) &= ~0x01000000;
	psxDmaInterrupt(6);
}

//NOTE: use iopPhysMem(madr) for getting the physical (local) address of IOP RAM stuff.  ... does this account for the complete 16MB range?
//*/
