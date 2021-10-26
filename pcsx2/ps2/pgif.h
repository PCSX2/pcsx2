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

#pragma once

//HW Registers
union tPGIF_CTRL
{

	struct pgifCtrl_t
	{
		//Please keep in mind, that not all of values are 100% confirmed.
		u32 UNK1					: 2;	// 0-1
		u32 fifo_GP1_ready_for_data : 1;	// 2
		u32 fifo_GP0_ready_for_data : 1;	// 3
		u32 data_from_gpu_ready		: 1;	// 4 sets in ps1drv same time as DMA RSEND
		u32 UNK2					: 1;	// 5
		u32 UNK3					: 2;	// 6-7
		u32 GP0_fifo_count			: 5;	// 8-12
		u32 UNK4					: 3;	// 13-15
		u32 GP1_fifo_count			: 3;	// 16 - 18
		u32 UNK5					: 1;	// 19
		u32 GP0_fifo_empty			: 1;	// 20
		u32 UNK6					: 1;	// 21
		u32 UNK7					: 1;	// 22
		u32 UNK8					: 8;	// 23-30
		u32 BUSY					: 1;	// Busy
	}bits;

	u32 _u32;
	tPGIF_CTRL( u32 val ) { _u32 = val; }
	void write(u32 value) { _u32 = value; }
	u32 get() { return _u32; }
};

union tPGIF_IMM
{
	struct imm_t
	{

		u32 e2;
		u32 dummy1[3];
		u32 e3;
		u32 dummy2[3];
		u32 e4;
		u32 dummy3[3];
		u32 e5;
		u32 dummy4[3];

	}reg;
	void reset() { reg.e2 = reg.e3 = reg.e4 = reg.e5 = 0; }
};

struct PGIFregisters
{
	tPGIF_IMM	imm_response;
	u128 		dummy1[2];
	tPGIF_CTRL	ctrl;
};
static PGIFregisters& pgif = (PGIFregisters&)eeHw[0xf310];

union tPGPU_REGS
{
	struct Bits_t
	{
		u32 TPXB	: 4;	// 0-3   Texture page X Base   (N*64)
		u32 TPYB	: 1;	// 4     Texture page Y Base   (N*256) (ie. 0 or 256)
		u32 ST		: 2;	// 5-6   Semi Transparency     (0=B/2+F/2, 1=B+F, 2=B-F, 3=B+F/4)
		u32 TPC		: 2;	// 7-8   Texture page colors   (0=4bit, 1=8bit, 2=15bit, 3=Reserved)
		u32 DITH	: 1;	// 9     Dither 24bit to 15bit (0=Off/strip LSBs, 1=Dither Enabled)
		u32 DRAW	: 1;	// 10    Drawing to display area
		u32 DMSK	: 1;	// 11    Set Mask-bit when drawing pixels (0=No, 1=Yes/Mask)
		u32 DPIX	: 1;	// 12    Draw Pixels           (0=Always, 1=Not to Masked areas)
		u32 ILAC	: 1;	// 13    Interlace Field       (or, always 1 when GP1(08h).5=0)
		u32 RFLG	: 1;	// 14    "Reverseflag"         (0=Normal, 1=Distorted)
		u32 TDIS	: 1;	// 15    Texture Disable       (0=Normal, 1=Disable Textures)
		u32 HR2		: 1;	// 16    Horizontal Resolution 2     (0=256/320/512/640, 1=368)
		u32 HR1		: 2;	// 17-18 Horizontal Resolution 1     (0=256, 1=320, 2=512, 3=640)
		u32 VRES	: 1;	// 19    Vertical Resolution         (0=240, 1=480, when Bit22=1)
		u32 VMOD	: 1;	// 20    Video Mode                  (0=NTSC/60Hz, 1=PAL/50Hz)
		u32 COLD	: 1;	// 21    Display Area Color Depth    (0=15bit, 1=24bit)
		u32 VILAC	: 1;	// 22    Vertical Interlace          (0=Off, 1=On)
		u32 DE		: 1;	// 23    Display Enable              (0=Enabled, 1=Disabled)
		u32 IRQ1	: 1;	// 24    Interrupt Request (IRQ1)    (0=Off, 1=IRQ)       ;GP0(1Fh)/GP1(02h)
		u32 DREQ	: 1; 	// 25    DMA / Data Request, meaning depends on GP1(04h) DMA Direction:
							// When GP1(04h)=0 ---> Always zero (0)
							// When GP1(04h)=1 ---> FIFO State  (0=Full, 1=Not Full)
							// When GP1(04h)=2 ---> Same as GPUSTAT.28
							// When GP1(04h)=3 ---> Same as GPUSTAT.27
		u32 RCMD	: 1;	// 26    Ready to receive Cmd Word   (0=No, 1=Ready)  ;GP0(...) ;via GP0
		u32 RSEND	: 1;	// 27    Ready to send VRAM to CPU   (0=No, 1=Ready)  ;GP0(C0h) ;via GPUREAD
		u32 RDMA	: 1;	// 28    Ready to receive DMA Block  (0=No, 1=Ready)  ;GP0(...) ;via GP0
		u32 DDIR	: 2;	// 29-30 DMA Direction (0=Off, 1=?, 2=CPUtoGP0, 3=GPUREADtoCPU)
		u32 DEO		: 1;	// 31    Drawing even/odd lines in interlace mode (0=Even or Vblank, 1=Odd)
	}bits;

	u32 _u32;
	tPGPU_REGS( u32 val ) { _u32 = val; }
	void write(u32 value) { _u32 = value; }
	u32 get() { return _u32; }
};

struct PGPUregisters
{
	tPGPU_REGS	stat;
};
static PGPUregisters& pgpu = (PGPUregisters&)eeHw[0xf300];

//Internal dma flags:
struct dma_t
{
	struct dmaState_t
	{
		bool ll_active;
		bool to_gpu_active;
		bool to_iop_active;
	} state;

	struct ll_dma_t
	{
		u32 data_read_address;
		u32 total_words; //total number of words
		u32 current_word; //current word number
		u32 next_address;
	} ll_dma;

	struct normalDma_t
	{
		u32 total_words; //total number of words in Normal DMA
		u32 current_word; //current word number in Normal DMA
		u32 address;
	} normal;
};

union tCHCR_DMA
{
	struct chcrDma_t
	{
		u32 DIR		: 1;	//0 Transfer Direction    (0=To Main RAM, 1=From Main RAM)
		u32 MAS		: 1;	//1 Memory Address Step   (0=Forward;+4, 1=Backward;-4)
		u32 resv0	: 6;
		u32 CHE		: 1;	//8 Chopping Enable       (0=Normal, 1=Chopping; run CPU during DMA gaps)
		u32 TSM		: 2;	//9-10    SyncMode, Transfer Synchronisation/Mode (0-3):
							//0  Start immediately and transfer all at once (used for CDROM, OTC)
							//1  Sync blocks to DMA requests   (used for MDEC, SPU, and GPU-data)
							//2  Linked-List mode              (used for GPU-command-lists)
							//3  Reserved                      (not used)
		u32 resv1	: 5;
		u32 CDWS	: 3;	// 16-18   Chopping DMA Window Size (1 SHL N words)
		u32 resv2	: 1;
		u32 CCWS	: 3;	// 20-22   Chopping CPU Window Size (1 SHL N clks)
		u32 resv3	: 1;
		u32 BUSY	: 1;	// 24      Start/Busy            (0=Stopped/Completed, 1=Start/Enable/Busy)
		u32 resv4	: 3;
		u32 TRIG	: 1;	// 28      Start/Trigger         (0=Normal, 1=Manual Start; use for SyncMode=0)
		u32 UKN1	: 1;	// 29      Unknown (R/W) Pause?  (0=No, 1=Pause?)     (For SyncMode=0 only?)
		u32 UNK2	: 1;	// 30      Unknown (R/W)
		u32 resv5	: 1;
	}bits;
	u32 _u32;
	tCHCR_DMA( u32 val ) { _u32 = val; }
	void write(u32 value) { _u32 = value; }
	u32 get() { return _u32; }
};

union tBCR_DMA
{
	struct bcrDma_t
	{
		u32 block_size : 16;
		u32 block_amount : 16;
	}bit;

	u32 _u32;
	tBCR_DMA( u32 val ) { _u32 = val; }
	u32 get_block_amount()  { return bit.block_amount ? bit.block_amount : 0x10000; }
	u32 get_block_size()  { return bit.block_size; }
	void write(u32 value) { _u32 = value; }
	u32 get() { return _u32; }
};

union tMADR_DMA
{
	u32 address;

	tMADR_DMA( u32 val ) { address = val; }
	void write(u32 value) { address = value; }
	u32 get() { return address; }
};

struct DMAregisters
{
	tMADR_DMA	madr;
	tBCR_DMA	bcr;
	tCHCR_DMA	chcr;
};
static DMAregisters& dmaRegs = (DMAregisters&)iopHw[0x10a0];

//Generic FIFO-related:
struct ringBuf_t
{
	u32* buf;
	int size;
	int count;
	int head;
	int tail;
};

//Defines for address labels:

//PGPU_STAT 0x1000F300 The GP1 - Status register, which PS1DRV writes (emulates) for the IOP to read.
#define PGPU_STAT 0x1000F300

//IMM_E2-IMM_E5 - "immediate response registers" - hold the return values for commands that require immediate response.
//They correspond to GP0() E2-E5 commands.
#define IMM_E2 0x1000F310
#define IMM_E3 0x1000F320
#define IMM_E4 0x1000F330
#define IMM_E5 0x1000F340

//PGIF_CTRL 0x1000F380 Main register for PGIF status info & control.
#define PGIF_CTRL 0x1000F380

//PGPU_CMD_FIFO FIFO buffer for GPU GP1 (CMD reg) CMDs IOP->EE only (unknown if reverse direction is possible).
#define PGPU_CMD_FIFO 0x1000F3C0
//PGPU_DAT_FIFO FIFO buffer for GPU GP0 (DATA reg) IOP->EE, but also EE->IOP. Direction is controlled by reg. 0x80/bit4 (most likely).
//Official name is "GFIFO", according to PS1DRV.
#define PGPU_DAT_FIFO 0x1000F3E0

//write to peripheral
#define DMA_LL_END_CODE 0x00FFFFFF

#define PGPU_DMA_MADR 0x1F8010A0
#define PGPU_DMA_BCR 0x1F8010A4
#define PGPU_DMA_CHCR 0x1F8010A8
#define PGPU_DMA_TADR 0x1F8010AC

#define pgpuDmaTadr HW_DMA2_TADR

void pgifInit(void);

extern void psxGPUw(int, u32);
extern u32 psxGPUr(int);

extern void PGIFw(int, u32);
extern u32 PGIFr(int);

extern void PGIFwQword(u32 addr, void *);
extern void PGIFrQword(u32 addr, void *);

extern u32 psxDma2GpuR(u32 addr);
extern void psxDma2GpuW(u32 addr, u32 data);

/***************************************************************************************************
*** Constants here control code that is either not certainly correct or may affect compatibility ***
***************************************************************************************************/

//Default=1. Is unknown why we need this, but we need this..
#define PREVENT_IRQ_ON_NORM_DMA_TO_GPU 1

//PGIF_DAT_RB_LEAVE_FREE - How many elements of the FIFO buffer to leave free in DMA.
//Can be 0 and no faults are observed.
//As the buffer has 32 elements, and normal DMA reads are usually done in 4 qwords (16 words),
//this must be less than 16, otherwise the PS1DRV will never read from the FIFO.
//At one point (in Linked-List DMA), PS1DRV will expect at least a certain number of elements, that is sent as argument to the func.
#define PGIF_DAT_RB_LEAVE_FREE 1
