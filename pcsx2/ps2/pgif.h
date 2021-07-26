/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2016-2016  PCSX2 Dev Team
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

//PGPU_STAT 0x1000F300 The GP1 - Status register, which PS1DRV writes (emulates) for the IOP to read.
#define PGPU_STAT 0x1000F300
//PGIF_CTRL 0x1000F380 Main register for PGIF status info & control.
#define PGIF_CTRL 0x1000F380

//PGIF_1-PGIF_4 - "immediate response registers" - hold the return values for commands that require immediate response.
//They correspond to GP0() E2-E5 commands.
#define PGIF_1 0x1000F310
#define PGIF_2 0x1000F320
#define PGIF_3 0x1000F330
#define PGIF_4 0x1000F340

//PGPU_CMD_FIFO FIFO buffer for GPU GP1 (CMD reg) CMDs IOP->EE only (unknown if reverse direction is possible).
#define PGPU_CMD_FIFO 0x1000F3C0
//PGPU_DAT_FIFO FIFO buffer for GPU GP0 (DATA reg) IOP->EE, but also EE->IOP. Direction is controlled by reg. 0x80/bit4 (most likely).
//Official name is "GFIFO", according to PS1DRV.
#define PGPU_DAT_FIFO 0x1000F3E0

//Bit-fields definitions for the PGPU Status register:
#define PGPU_STAT_IRQ1 0x01000000

//Specifies the bits in the PGIF Control (0xf380) register that are not writable from EE side, by the PS1DRV.
#define PGIF_CTRL_RO_MASK 0x00000000

//DMA State
#define DMA_START_BUSY 0x01000000
#define DMA_TRIGGER 0x10000000

//write to peripheral
#define DMA_DIR_FROM_RAM 0x1
#define DMA_LINKED_LIST 0x00000400
#define DMA_LL_END_CODE 0x00FFFFFF

#define PGPU_DMA_MADR 0x1F8010A0
#define PGPU_DMA_BCR 0x1F8010A4
#define PGPU_DMA_CHCR 0x1F8010A8
// Is there a TADR?
#define PGPU_DMA_TADR 0x1F8010AC

#define pgpuDmaMadr HW_DMA2_MADR
#define pgpuDmaBcr  HW_DMA2_BCR
#define pgpuDmaChcr HW_DMA2_CHCR
#define pgpuDmaTadr HW_DMA2_TADR

void pgifInit(void);
void pgifReset(void);

extern void psxGPUw(int, u32);
extern u32 psxGPUr(int);

extern void PGIFw(int, u32);
extern u32 PGIFr(int);

extern void PGIFwQword(u32 addr, void *);
extern void PGIFrQword(u32 addr, void *);

extern u32 psxDma2GpuR(u32 addr);
extern void psxDma2GpuW(u32 addr, u32 data);


extern void ps12PostOut(u32 mem, u8 value);
extern void psDuartW(u32 mem, u8 value);
extern u8 psExp2R8(u32 mem);
extern void kernelTTYFileDescrWrite(u32 mem, u32 data);
extern u32 getIntTmrKReg(u32 mem, u32 data);
extern void testInt(void);
extern void HPCoS_print(u32 mem, u32 data);
extern void anyIopLS(u32 addr, u32 data, int Wr);
extern void dma6_OTClear(void);

/***************************************************************************************************
*** Constants here control code that is either not certainly correct or may affect compatibility ***
***************************************************************************************************/

//Default=1. Enables changing the data FIFO/DMA direction bit in the Status register by the PGIF Ctrl register.
#define STAT_DIR_BY_IF_CTRL 1
//Specifies which bits (29 and 30) from the "Set DMA direction" command to copy to the PGPU Status reg.
//Default(works better)=0x40000000, but makes more sense to be =0x20000000 or =0x60000000.
//Status reg. bits 30:29: DMA Direction (0=Off, 1=FIFO, 2=CPUtoGP0, 3=GPUREADtoCPU) ;GPUSTAT.29-30
#define STAT_DIR_BY_CMD_BITS 0x40000000
//#define STAT_DIR_BY_CMD_BITS 0x60000000

//Enables interrupt on the GP0(1Fh) - Interrupt Request (IRQ1) command. Default=0.
#define PGPU_IRQ1_ENABLE 0
//Default=1.
#define PREVENT_IRQ_ON_NORM_DMA_TO_GPU 1

//When intercepted, GP1() commands are acted-upon and the their effects are emulated by this code, besides PS1DRV.
//This seems to be what the PGIF does, but disabling it (=0) doesn't break too much stuff either.
//Default=0. Do intercept when IOP code writes (sends) to FIFO the CMD.
#define CMD_INTERCEPT_AT_WR 0
//Default=1. Do intercept when PS1DRV reads from FIFO the CMD.
#define CMD_INTERCEPT_AT_RD 1

//PGIF_DAT_RB_LEAVE_FREE - How many elements of the FIFO buffer to leave free in DMA.
//Can be 0 and no faults are observed.
//As the buffer has 32 elements, and normal DMA reads are usually done in 4 qwords (16 words),
//this must be less than 16, otherwise the PS1DRV will never read from the FIFO.
//At one point (in Linked-List DMA), PS1DRV will expect at least a certain number of elements, that is sent as argument to the func.
#define PGIF_DAT_RB_LEAVE_FREE 1

//Default=0. If =1, additional modification to the Status register (0x1F801814) is not done by the PGIF emulator.
//It is unknown if the PGIF emulates it, or all that's necessary is done by PS1DRV, which should have(?) complete write control over it.
#define SKIP_UPD_PGPU_STAT 0
