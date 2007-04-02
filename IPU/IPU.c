/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "IPU.h"
#include "mpeg2lib/Mpeg.h"
#include "yuv2rgb.h"

#include <assert.h>
#include <time.h>

#include <xmmintrin.h>
#include <emmintrin.h>

#include "ir5900.h"
#include "pcl.h"

#ifdef __WIN32__
#define FASTCALL	__fastcall
#else
#define FASTCALL
#endif

#ifndef WIN32_VIRTUAL_MEM
IPUregisters g_ipuRegsReal;
#endif

#define ipu0dma ((DMACh *)&PS2MEM_HW[0xb000])
#define ipu1dma ((DMACh *)&PS2MEM_HW[0xb400])

#define IPU_DMA_GIFSTALL 1
#define IPU_DMA_TIE0 2
#define IPU_DMA_TIE1 4
#define IPU_DMA_ACTV1 8
#define IPU_DMA_DOTIE1 16
#define IPU_DMA_FIREINT0 32
#define IPU_DMA_FIREINT1 64

static int g_nDMATransfer = 0;
int g_nIPU0Data = 0; // data left to transfer
u8* g_pIPU0Pointer = NULL;
int g_nCmdPos[2] = {0}, g_nCmdIndex = 0;
static int ipuCurCmd = 0xffffffff;

// returns number of qw read
int fifo_wread(void *value, int size);
int fifo_wread1(void *value);
int fifo_wwrite(u32* pMem, int size);
void fifo_wclear();

static int readwpos = 0, writewpos = 0;
__declspec(align(16)) u32 fifo_input[32];

void ReorderBitstream();
u8 FillInternalBuffer(u32 * pointer, u32 advance);

// the BP doesn't advance and returns -1 if there is no data to be read
tIPU_BP g_BP;
static coroutine_t s_routine; // used for executing BDEC/IDEC
static int s_RoutineDone = 0;
static u32 s_tempstack[0x1000]; // 16k

void IPUCMD_WRITE(u32 val);
void IPUWorker();
int IPU0dma(const void* pMem, int size);
int IPU1dma();

#define BigEndian _byteswap_ulong
//__forceinline u32 FASTCALL BigEndian(u32 a){
//	return ((((a >> 24) & 0xFF) <<  0) + (((a >> 16) & 0xFF) <<  8) +
//		(((a >>  8) & 0xFF) << 16) + (((a >>  0) & 0xFF) << 24));
//}

// Color conversion stuff
char convert_data_buffer[0x1C];
convert_init_t convert_init={convert_data_buffer, 0x1C};
convert_t *convert;

// Quantization matrix
static u8 niq[64],			//non-intraquant matrix
		iq[64];			//intraquant matrix
u16 vqclut[16];			//clut conversion table
static u8 s_thresh[2];		//thresholds for color conversions
int coded_block_pattern=0;
__declspec(align(16)) struct macroblock_8  mb8;
__declspec(align(16)) struct macroblock_16 mb16;
__declspec(align(16)) struct macroblock_rgb32 rgb32;
__declspec(align(16)) struct macroblock_rgb16 rgb16;

u8 indx4[16*16/2];
u32	mpeg2_inited;		//mpeg2_idct_init() must be called only once
u8 PCT[]={'r', 'I', 'P', 'B', 'D', '-', '-', '-'};
decoder_t g_decoder;						//static, only to place it in bss
decoder_t tempdec;

extern u8 mpeg2_scan_norm[64];
extern u8 mpeg2_scan_alt[64];

__declspec(align(16)) u8 _readbits[80];	//local buffer (ring buffer)
u8* readbits = _readbits; // always can decrement by one 1qw

#define SATURATE_4BITS(val)		((val)>15 ? 15 : (val))

void IPUProcessInterrupt()
{
	if( ipuRegs->ctrl.BUSY ) {
		IPUWorker();
	}
}

/////////////////////////////////////////////////////////
// Register accesses (run on EE thread)
int ipuInit()
{
	memset(ipuRegs, 0, sizeof(IPUregisters));
	memset(&g_BP, 0, sizeof(g_BP));

	//other stuff
	g_decoder.intra_quantizer_matrix		=(u8*)iq;
	g_decoder.non_intra_quantizer_matrix	=(u8*)niq;
	g_decoder.picture_structure = FRAME_PICTURE;	//default: progressive...my guess:P
	g_decoder.mb8	=&mb8;
	g_decoder.mb16=&mb16;
	g_decoder.rgb32=&rgb32;
	g_decoder.rgb16=&rgb16;
	g_decoder.stride=16;

	return 0;
}

void ipuReset()
{
	memset(ipuRegs, 0, sizeof(IPUregisters));
	g_nDMATransfer = 0;
}

void ipuShutdown()
{
}

int  ipuFreeze(gzFile f, int Mode) {
	IPUProcessInterrupt();

	gzfreeze(ipuRegs, sizeof(IPUregisters));
	gzfreeze(&g_nDMATransfer, sizeof(g_nDMATransfer));
	gzfreeze(&readwpos, sizeof(readwpos));
	gzfreeze(&writewpos, sizeof(writewpos));
	gzfreeze(fifo_input, sizeof(fifo_input));
	gzfreeze(&g_BP, sizeof(g_BP));
	gzfreeze(niq, sizeof(niq));
	gzfreeze(iq, sizeof(niq));
	gzfreeze(vqclut, sizeof(vqclut));
	gzfreeze(s_thresh, sizeof(s_thresh));
	gzfreeze(&coded_block_pattern, sizeof(coded_block_pattern));
	gzfreeze(&g_decoder, sizeof(g_decoder));
	gzfreeze(mpeg2_scan_norm, sizeof(mpeg2_scan_norm));
	gzfreeze(mpeg2_scan_alt, sizeof(mpeg2_scan_alt));
	gzfreeze(g_nCmdPos, sizeof(g_nCmdPos));
	gzfreeze(&g_nCmdIndex, sizeof(g_nCmdIndex));
	gzfreeze(&ipuCurCmd, sizeof(ipuCurCmd));
	
	gzfreeze(_readbits, sizeof(_readbits));

	if( Mode == 0 ) {
		int temp = readbits-_readbits;
		gzfreeze(&temp, sizeof(temp));
	}
	else {
		int temp;
		gzfreeze(&temp, sizeof(temp));
		readbits = _readbits;
	}

	//other stuff
	g_decoder.intra_quantizer_matrix		=(u8*)iq;
	g_decoder.non_intra_quantizer_matrix	=(u8*)niq;
	g_decoder.picture_structure = FRAME_PICTURE;	//default: progressive...my guess:P
	g_decoder.mb8	=&mb8;
	g_decoder.mb16=&mb16;
	g_decoder.rgb32=&rgb32;
	g_decoder.rgb16=&rgb16;
	g_decoder.stride=16;

	if (!mpeg2_inited){
        mpeg2_idct_init();
		convert=convert_rgb (CONVERT_RGB, 32);
		convert(16, 16, 0, NULL, &convert_init);
		memset(mb8.Y,0,sizeof(mb8.Y));
		memset(mb8.Cb,0,sizeof(mb8.Cb));
		memset(mb8.Cr,0,sizeof(mb8.Cr));
		memset(mb16.Y,0,sizeof(mb16.Y));
		memset(mb16.Cb,0,sizeof(mb16.Cb));
		memset(mb16.Cr,0,sizeof(mb16.Cr));
		mpeg2_inited=1;
	}

	return 0;
}

BOOL ipuCanFreeze()
{
	return ipuCurCmd == 0xffffffff;
}

u32 ipuRead32(u32 mem)
{
	IPUProcessInterrupt();

	switch (mem){

		case 0x10002010: // IPU_CTRL
			ipuRegs->ctrl.IFC = g_BP.IFC;
			ipuRegs->ctrl.OFC = min(g_nIPU0Data, 8); // check if transfering to ipu0
			ipuRegs->ctrl.CBP = coded_block_pattern;

#ifdef IPU_LOG
			if( !ipuRegs->ctrl.BUSY ) {
				IPU_LOG("Ipu read32: IPU_CTRL=0x%08X %x\n", ipuRegs->ctrl._u32, cpuRegs.pc);
			}
#endif
			return ipuRegs->ctrl._u32;

		case 0x10002020: // IPU_BP

			ipuRegs->ipubp = g_BP.BP & 0x7f;
			ipuRegs->ipubp |= g_BP.IFC<<8;
			ipuRegs->ipubp |= (g_BP.FP+g_BP.bufferhasnew) << 16;

#ifdef IPU_LOG
			IPU_LOG("Ipu read32: IPU_BP=0x%08X\n", *(u32*)&g_BP);
#endif
			return ipuRegs->ipubp;
	}

	return *(u32*)(((u8*)ipuRegs)+(mem&0xff)); // ipu repeats every 0x100
}

int ipuConstRead32(u32 x86reg, u32 mem)
{
	int workingreg, tempreg, tempreg2;
	iFlushCall(0);
	CALLFunc((u32)IPUProcessInterrupt);

//	if( !(x86reg&(MEM_XMMTAG|MEM_MMXTAG)) ) {
//		if( x86reg == EAX ) {
//			tempreg =  ECX;
//			tempreg2 = EDX;
//		}
//		else if( x86reg == ECX ) {
//			tempreg =  EAX;
//			tempreg2 = EDX;
//		}
//		else if( x86reg == EDX ) {
//			tempreg =  EAX;
//			tempreg2 = ECX;
//		}
//
//		workingreg = x86reg;
//	}
//	else {
		workingreg = EAX;
		tempreg =  ECX;
		tempreg2 = EDX;
//	}

	switch (mem){

		case 0x10002010: // IPU_CTRL
	
			MOV32MtoR(workingreg, (u32)&ipuRegs->ctrl._u32);
			AND32ItoR(workingreg, ~0x3fff);
			MOV32MtoR(tempreg, (u32)&g_nIPU0Data);
			MOV8MtoR(workingreg, (u32)&g_BP.IFC);

			CMP32ItoR(tempreg, 8);
			j8Ptr[5] = JLE8(0);
			MOV32ItoR(tempreg, 8);
			x86SetJ8( j8Ptr[5] );
			SHL32ItoR(tempreg, 4);

			OR8MtoR(EAX+4, (u32)&coded_block_pattern); // or ah, mem
			OR8RtoR(workingreg, tempreg);

#ifdef _DEBUG
			MOV32RtoM((u32)&ipuRegs->ctrl._u32, workingreg);
#endif
			// NOTE: not updating ipuRegs->ctrl
//			if( x86reg & MEM_XMMTAG ) SSE2_MOVD_R_to_XMM(x86reg&0xf, workingreg);
//			else if( x86reg & MEM_MMXTAG ) MOVD32RtoMMX(x86reg&0xf, workingreg);
			return 1;

		case 0x10002020: // IPU_BP

			assert( (u32)&g_BP.FP + 1 == (u32)&g_BP.bufferhasnew );

			MOVZX32M8toR(workingreg, (u32)&g_BP.BP);
			MOVZX32M8toR(tempreg, (u32)&g_BP.FP);
			AND8ItoR(workingreg, 0x7f);
			ADD8MtoR(tempreg, (u32)&g_BP.bufferhasnew);
			MOV8MtoR(workingreg+4, (u32)&g_BP.IFC);

			SHL32ItoR(tempreg, 16);
			OR32RtoR(workingreg, tempreg);

#ifdef _DEBUG
			MOV32RtoM((u32)&ipuRegs->ipubp, workingreg);
#endif
			// NOTE: not updating ipuRegs->ipubp
//			if( x86reg & MEM_XMMTAG ) SSE2_MOVD_R_to_XMM(x86reg&0xf, workingreg);
//			else if( x86reg & MEM_MMXTAG ) MOVD32RtoMMX(x86reg&0xf, workingreg);

			return 1;

		default:
			// ipu repeats every 0x100
			_eeReadConstMem32(x86reg, (u32)(((u8*)ipuRegs)+(mem&0xff)));
			return 0;
	}

	return 0;
}

u64 ipuRead64(u32 mem)
{
	IPUProcessInterrupt();

#ifdef PCSX2_DEVBUILD
	if( mem == 0x10002010 ) {
		SysPrintf("reading 64bit IPU ctrl\n");
	}
	if( mem == 0x10002020 ) {
		SysPrintf("reading 64bit IPU top\n");
	}
#endif

	switch (mem){
		case 0x10002000: // IPU_CMD
#ifdef IPU_LOG
			//if(!ipuRegs->cmd.BUSY){
			if( ipuRegs->cmd.DATA&0xffffff ) {
				IPU_LOG("Ipu read64: IPU_CMD=BUSY=%x, DATA=%08X\n", ipuRegs->cmd.BUSY?1:0,ipuRegs->cmd.DATA);
			}
#endif
			//return *(u64*)&ipuRegs->cmd;
			break;

		case 0x10002030: // IPU_TOP
#ifdef IPU_LOG
			IPU_LOG("Ipu read64: IPU_TOP=%x,  bp = %d\n",ipuRegs->top,g_BP.BP);
#endif
			//return *(u64*)&ipuRegs->top;
			break;

		default:
#ifdef IPU_LOG
			IPU_LOG("Ipu read64: Unknown=%x\n", mem);
#endif
			break;

	}
	return *(u64*)(((u8*)ipuRegs)+(mem&0xff));
}

void ipuConstRead64(u32 mem, int mmreg)
{
	iFlushCall(0);
	CALLFunc((u32)IPUProcessInterrupt);

	if( IS_XMMREG(mmreg) ) SSE_MOVLPS_M64_to_XMM(mmreg&0xff, (u32)(((u8*)ipuRegs)+(mem&0xff)));
	else {
		MOVQMtoR(mmreg, (u32)(((u8*)ipuRegs)+(mem&0xff)));
		SetMMXstate();
	}
}

void ipuSoftReset()
{
	if (!mpeg2_inited){
        mpeg2_idct_init();
		convert=convert_rgb (CONVERT_RGB, 32);
		convert(16, 16, 0, NULL, &convert_init);
		memset(mb8.Y,0,sizeof(mb8.Y));
		memset(mb8.Cb,0,sizeof(mb8.Cb));
		memset(mb8.Cr,0,sizeof(mb8.Cr));
		memset(mb16.Y,0,sizeof(mb16.Y));
		memset(mb16.Cb,0,sizeof(mb16.Cb));
		memset(mb16.Cr,0,sizeof(mb16.Cr));
		mpeg2_inited=1;
	}
	fifo_wclear();
	coded_block_pattern = 0;

	ipuRegs->ctrl._u32 = 0;
	g_BP.BP     = 0;
	g_BP.IFC    = 0;
	g_BP.FP     = 0;
	g_BP.bufferhasnew = 0;
	ipuRegs->top = 0;
	g_nCmdIndex = 0;
	ipuCurCmd = 0xffffffff;
	g_nCmdPos[0] = 0; g_nCmdPos[1] = 0;
}

void ipuWrite32(u32 mem,u32 value)
{
	IPUProcessInterrupt();

	switch (mem){
		case 0x10002000: // IPU_CMD
#ifdef IPU_LOG
	        IPU_LOG("Ipu write32: IPU_CMD=0x%08X\n",value);
#endif
			IPUCMD_WRITE(value);
			break;
		case 0x10002010: // IPU_CTRL
			ipuRegs->ctrl._u32 = (value&0x47f30000)|(ipuRegs->ctrl._u32&0x8000ffff);
			if (ipuRegs->ctrl.RST & 0x1) { // RESET
				ipuSoftReset();
			}

#ifdef IPU_LOG
	        IPU_LOG("Ipu write32: IPU_CTRL=0x%08X\n",value);
#endif

			break;
		default:
#ifdef IPU_LOG
			IPU_LOG("Ipu write32: Unknown=%x\n", mem);
#endif
			*(u32*)((u8*)ipuRegs + (mem&0xfff)) = value;
			break;
	}
}

void ipuConstWrite32(u32 mem, int mmreg)
{
	iFlushCall(0);
	if( !(mmreg & (MEM_XMMTAG|MEM_MMXTAG|MEM_EECONSTTAG)) ) PUSH32R(mmreg);
	CALLFunc((u32)IPUProcessInterrupt);

	switch (mem){
		case 0x10002000: // IPU_CMD
			if( (mmreg & (MEM_XMMTAG|MEM_MMXTAG|MEM_EECONSTTAG)) ) _recPushReg(mmreg);
			CALLFunc((u32)IPUCMD_WRITE);
			ADD32ItoR(ESP, 4);
			break;
		case 0x10002010: // IPU_CTRL
			if( mmreg & MEM_EECONSTTAG ) {
				u32 c = g_cpuConstRegs[(mmreg>>16)&0x1f].UL[0]&0x47f30000;

				if( c & 0x40000000 ) {
					CALLFunc((u32)ipuSoftReset);
				}
				else {
					AND32ItoM((u32)&ipuRegs->ctrl._u32, 0x8000ffff);
					OR32ItoM((u32)&ipuRegs->ctrl._u32, c);
				}
			}
			else {
				if( mmreg & MEM_XMMTAG ) SSE2_MOVD_XMM_to_R(EAX, mmreg&0xf);
				else if( mmreg & MEM_MMXTAG ) MOVD32MMXtoR(EAX, mmreg&0xf);
				else POP32R(EAX);

				MOV32MtoR(ECX, (u32)&ipuRegs->ctrl._u32);
				AND32ItoR(EAX, 0x47f30000);
				AND32ItoR(ECX, 0x8000ffff);
				OR32RtoR(EAX, ECX);
				MOV32RtoM((u32)&ipuRegs->ctrl._u32, EAX);

				TEST32ItoR(EAX, 0x40000000);
				j8Ptr[5] = JZ8(0);

				// reset
				CALLFunc((u32)ipuSoftReset);
				
				x86SetJ8( j8Ptr[5] );
			}

			break;
		default:
			if( !(mmreg & (MEM_XMMTAG|MEM_MMXTAG|MEM_EECONSTTAG)) ) POP32R(mmreg);
			_eeWriteConstMem32((u32)((u8*)ipuRegs + (mem&0xfff)), mmreg);
			break;
	}
}

void ipuWrite64(u32 mem, u64 value)
{
	IPUProcessInterrupt();

	switch (mem){
		case 0x10002000:
#ifdef IPU_LOG
	        IPU_LOG("Ipu write64: IPU_CMD=0x%08X\n",value);
#endif
			IPUCMD_WRITE((u32)value);
			break;

		default:
#ifdef IPU_LOG
			IPU_LOG("Ipu write64: Unknown=%x\n", mem);
#endif
			*(u64*)((u8*)ipuRegs + (mem&0xfff)) = value;
			break;
	}
}

void ipuConstWrite64(u32 mem, int mmreg)
{
	iFlushCall(0);
	CALLFunc((u32)IPUProcessInterrupt);

	switch (mem){
		case 0x10002000:
			_recPushReg(mmreg);
			CALLFunc((u32)IPUCMD_WRITE);
			ADD32ItoR(ESP, 4);
			break;

		default:
			_eeWriteConstMem64( (u32)((u8*)ipuRegs + (mem&0xfff)), mmreg);
			break;
	}
}

///////////////////////////////////////////
// IPU Commands (exec on worker thread only)

static void ipuBCLR(u32 val) {
    fifo_wclear();
	g_BP.BP = val & 0x7F;
	g_BP.FP = 0;
	g_BP.IFC = 0;
#ifdef IPU_LOG
	IPU_LOG("Clear IPU input FIFO. Set Bit offset=0x%X\n", g_BP.BP);
#endif
}

static BOOL ipuIDEC(u32 val)
{
	tIPU_CMD_IDEC idec={0, 0, 0, 0, 0, 0, 0, 0, 0};
	
	*(u32*)&idec=val;
#ifdef IPU_LOG
						IPU_LOG("IPU IDEC command.\n");
	if (idec.FB){		IPU_LOG(" Skip %d bits.",idec.FB);}
						IPU_LOG(" Quantizer step code=0x%X.",idec.QSC);
	if (idec.DTD==0){	IPU_LOG(" Does not decode DT.");
	}else{				IPU_LOG(" Decodes DT.");}
	if (idec.SGN==0){	IPU_LOG(" No bias.");
	}else{				IPU_LOG(" Bias=128.");}
	if (idec.DTE==1){	IPU_LOG(" Dither Enabled.");}
	if (idec.OFM==0){	IPU_LOG(" Output format is RGB32.");
	}else{				IPU_LOG(" Output format is RGB16.");}
						IPU_LOG("\n");
#endif
	g_BP.BP+= idec.FB;//skip FB bits
	//from IPU_CTRL
	ipuRegs->ctrl.PCT = I_TYPE; //Intra DECoding;)
	g_decoder.coding_type		=ipuRegs->ctrl.PCT;
	g_decoder.mpeg1			=ipuRegs->ctrl.MP1;
	g_decoder.q_scale_type	=ipuRegs->ctrl.QST;
	g_decoder.intra_vlc_format=ipuRegs->ctrl.IVF;
	g_decoder.scan			=ipuRegs->ctrl.AS ? mpeg2_scan_alt: mpeg2_scan_norm;
	g_decoder.intra_dc_precision=ipuRegs->ctrl.IDP;
	//from IDEC value
	g_decoder.quantizer_scale	=idec.QSC;
	g_decoder.frame_pred_frame_dct=!idec.DTD;
	g_decoder.sgn				=idec.SGN;
	g_decoder.dte				=idec.DTE;
	g_decoder.ofm				=idec.OFM;
	//other stuff
	g_decoder.dcr				  =1;//resets DC prediction value

	s_routine = co_create(mpeg2sliceIDEC, &s_RoutineDone, s_tempstack, sizeof(s_tempstack));
	assert( s_routine != NULL );
	co_call(s_routine);
	return s_RoutineDone;
}

static BOOL ipuBDEC(u32 val)
{
	tIPU_CMD_BDEC bdec={0, 0, 0, 0, 0, 0, 0, 0};
	*(u32*)&bdec=val;

#ifdef IPU_LOG
							IPU_LOG("IPU BDEC(macroblock decode) command.\n");
	if (bdec.FB){			IPU_LOG(" Skip 0x%X bits.", bdec.FB);}
	if (bdec.MBI){			IPU_LOG(" Intra MB.");}
	else{					IPU_LOG(" Non-intra MB.");}
	if (bdec.DCR){			IPU_LOG(" Resets DC prediction value.");}
	else{					IPU_LOG(" Doesn't reset DC prediction value.");}
	if (bdec.DT){			IPU_LOG(" Use field DCT.");}
	else{					IPU_LOG(" Use frame DCT.");}
							IPU_LOG(" Quantiser step=0x%X\n",bdec.QSC);
#endif
	g_BP.BP+= bdec.FB;//skip FB bits
	g_decoder.coding_type		= I_TYPE;
	g_decoder.mpeg1			=ipuRegs->ctrl.MP1;
	g_decoder.q_scale_type	=ipuRegs->ctrl.QST;
	g_decoder.intra_vlc_format=ipuRegs->ctrl.IVF;
	g_decoder.scan			=ipuRegs->ctrl.AS ? mpeg2_scan_alt: mpeg2_scan_norm;
	g_decoder.intra_dc_precision=ipuRegs->ctrl.IDP;
	//from BDEC value
	/* JayteeMaster: the quantizer (linear/non linear) depends on the q_scale_type */
	g_decoder.quantizer_scale  =g_decoder.q_scale_type?non_linear_quantizer_scale [bdec.QSC]:bdec.QSC<<1;
	g_decoder.macroblock_modes =bdec.DT ? DCT_TYPE_INTERLACED : 0;
	g_decoder.dcr				 =bdec.DCR;
	g_decoder.macroblock_modes|=bdec.MBI ? MACROBLOCK_INTRA : MACROBLOCK_PATTERN;	

	memset(&mb8, 0, sizeof(struct macroblock_8));
	memset(&mb16, 0, sizeof(struct macroblock_16));
	
	s_routine = co_create(mpeg2_slice, &s_RoutineDone, s_tempstack, sizeof(s_tempstack));
	assert( s_routine != NULL );
	co_call(s_routine);
	return s_RoutineDone;
}

static BOOL ipuVDEC(u32 val) {
	
	switch( g_nCmdPos[0] ) {
		case 0:
			ipuRegs->cmd.DATA = 0;
 			if( !getBits32((u8*)&g_decoder.bitstream_buf, 0) )
				return FALSE;
			
			g_decoder.bitstream_bits = -16;	 
			g_decoder.bitstream_buf=BigEndian(g_decoder.bitstream_buf);
		 
			//value = BigEndian(value);
			switch((val >> 26) & 3){
				case 0://Macroblock Address Increment
					g_decoder.mpeg1			=ipuRegs->ctrl.MP1;
					ipuRegs->cmd.DATA = get_macroblock_address_increment(&g_decoder);
					break;
				case 1://Macroblock Type	//known issues: no error detected
					g_decoder.frame_pred_frame_dct=1;//prevent DCT_TYPE_INTERLACED
					g_decoder.coding_type		=ipuRegs->ctrl.PCT;
					ipuRegs->cmd.DATA=get_macroblock_modes(&g_decoder);
					break;
				case 2://Motion Code		//known issues: no error detected
					ipuRegs->cmd.DATA=get_motion_delta(&g_decoder,0);
					//g_BP.BP += ipuRegs->cmd.DATA >> 16;
					break;
				case 3://DMVector
					ipuRegs->cmd.DATA=get_dmv(&g_decoder);
					//g_BP.BP += ipuRegs->cmd.DATA >> 16;
					break;
			}

			//g_BP.BP += ipuRegs->cmd.DATA >> 16;
			g_BP.BP+=(g_decoder.bitstream_bits+16);
			if((int)g_BP.BP < 0) {
				g_BP.BP += 128;
				ReorderBitstream();
			}

			ipuRegs->cmd.DATA = (ipuRegs->cmd.DATA & 0xFFFF) | ((g_decoder.bitstream_bits+16) << 16);
			ipuRegs->ctrl.ECD = (ipuRegs->cmd.DATA==0);

		case 1:	
			if( !FillInternalBuffer(&g_BP.BP,1) ) {
				g_nCmdPos[0] = 1;
				return FALSE;
			}
			
		case 2:
	
			if( !getBits32((u8*)&ipuRegs->top, 0) ) {
				g_nCmdPos[0] = 2;
				return FALSE;
			}
	
			ipuRegs->top = BigEndian(ipuRegs->top);

#ifdef IPU_LOG
			IPU_LOG("IPU VDEC command data 0x%x(0x%x). Skip 0x%X bits/Table=%d (%s), pct %d\n",
				ipuRegs->cmd.DATA,ipuRegs->cmd.DATA >> 16,val & 0x3f, (val >> 26) & 3, (val >> 26) & 1 ?
				((val >> 26) & 2 ? "DMV" : "MBT") : (((val >> 26) & 2 ? "MC" : "MBAI")),ipuRegs->ctrl.PCT);
#endif

			return TRUE;
	}

	assert(0);
	return FALSE;
}

static BOOL ipuFDEC(u32 val)
{
	if( !getBits32((u8*)&ipuRegs->cmd.DATA, 0) )
		return FALSE;

	ipuRegs->cmd.DATA = BigEndian(ipuRegs->cmd.DATA);
	ipuRegs->top = ipuRegs->cmd.DATA;
	return TRUE;
}

static BOOL ipuSETIQ(u32 val)
{
	int i;

 	if ((val >> 27) & 1){
		g_nCmdPos[0] += getBits((u8*)niq + g_nCmdPos[0], 512-8*g_nCmdPos[0], 1); // 8*8*8

#ifdef IPU_LOG
		IPU_LOG("Read non-intra quantisation matrix from IPU FIFO.\n");
		for (i=0; i<8; i++){
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X\n",
				niq[i*8+0], niq[i*8+1], niq[i*8+2], niq[i*8+3],
				niq[i*8+4], niq[i*8+5], niq[i*8+6], niq[i*8+7]);
		}
#endif
	}else{
		g_nCmdPos[0] += getBits((u8*)iq+8*g_nCmdPos[0], 512-8*g_nCmdPos[0], 1);
#ifdef IPU_LOG
		IPU_LOG("Read intra quantisation matrix from IPU FIFO.\n");
		for (i=0; i<8; i++){
			IPU_LOG("%02X %02X %02X %02X %02X %02X %02X %02X\n",
				iq[i*8+0], iq[i*8+1], iq[i*8+2], iq[i*8+3],
				iq[i*8+4], iq[i*8+5], iq[i*8+6], iq[i*8+7]);
		}
#endif
	}

	return g_nCmdPos[0] == 64;
}

static BOOL ipuSETVQ(u32 val)
{	
	g_nCmdPos[0] += getBits((u8*)vqclut+g_nCmdPos[0], 256-8*g_nCmdPos[0], 1); // 16*2*8

	if( g_nCmdPos[0] == 32 ) {
#ifdef IPU_LOG
	IPU_LOG("IPU SETVQ command.\nRead VQCLUT table from IPU FIFO.\n");
	IPU_LOG(
		"%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d "
		"%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n"
		"%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d "
		"%02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d %02d:%02d:%02d\n",
		vqclut[0] >> 10, (vqclut[0] >> 5) & 0x1F, vqclut[0] & 0x1F,
		vqclut[1] >> 10, (vqclut[1] >> 5) & 0x1F, vqclut[1] & 0x1F,
		vqclut[2] >> 10, (vqclut[2] >> 5) & 0x1F, vqclut[2] & 0x1F,
		vqclut[3] >> 10, (vqclut[3] >> 5) & 0x1F, vqclut[3] & 0x1F,
		vqclut[4] >> 10, (vqclut[4] >> 5) & 0x1F, vqclut[4] & 0x1F,
		vqclut[5] >> 10, (vqclut[5] >> 5) & 0x1F, vqclut[5] & 0x1F,
		vqclut[6] >> 10, (vqclut[6] >> 5) & 0x1F, vqclut[6] & 0x1F,
		vqclut[7] >> 10, (vqclut[7] >> 5) & 0x1F, vqclut[7] & 0x1F,
		vqclut[8] >> 10, (vqclut[8] >> 5) & 0x1F, vqclut[8] & 0x1F,
		vqclut[9] >> 10, (vqclut[9] >> 5) & 0x1F, vqclut[9] & 0x1F,
		vqclut[10] >> 10, (vqclut[10] >> 5) & 0x1F, vqclut[10] & 0x1F,
		vqclut[11] >> 10, (vqclut[11] >> 5) & 0x1F, vqclut[11] & 0x1F,
		vqclut[12] >> 10, (vqclut[12] >> 5) & 0x1F, vqclut[12] & 0x1F,
		vqclut[13] >> 10, (vqclut[13] >> 5) & 0x1F, vqclut[13] & 0x1F,
		vqclut[14] >> 10, (vqclut[14] >> 5) & 0x1F, vqclut[14] & 0x1F,
		vqclut[15] >> 10, (vqclut[15] >> 5) & 0x1F, vqclut[15] & 0x1F);
#endif
	}

	return g_nCmdPos[0] == 32;
}

// IPU Transfers are split into 8Qwords so we need to send ALL the data
static BOOL ipuCSC(u32 val)
{
	tIPU_CMD_CSC csc ={0, 0, 0, 0, 0};
	*(u32*)&csc=val;

#ifdef IPU_LOG
	IPU_LOG("IPU CSC(Colorspace conversion from YCbCr) command (%d).\n",csc.MBC);
	if (csc.OFM){	IPU_LOG("Output format is RGB16. ");}
	else{			IPU_LOG("Output format is RGB32. ");}
	if (csc.DTE){	IPU_LOG("Dithering enabled.");	}
#endif
	//SysPrintf("CSC\n");
	for (;g_nCmdIndex<(int)csc.MBC; g_nCmdIndex++){

		if( g_nCmdPos[0] < 3072/8 ) {
			g_nCmdPos[0] += getBits((u8*)&mb8+g_nCmdPos[0], 3072-8*g_nCmdPos[0], 1);

			if( g_nCmdPos[0] < 3072/8 )
				return FALSE;

			ipu_csc(&mb8, &rgb32, 0);
			if (csc.OFM){
				ipu_dither(&mb8, &rgb16, csc.DTE);
			}
		}
	
		if (csc.OFM){
			g_nCmdPos[1] += IPU0dma(((u32*)&rgb16)+4*g_nCmdPos[1], 32-g_nCmdPos[1]);

			if( g_nCmdPos[1] < 32 )
				return FALSE;
		}
		else {
			g_nCmdPos[1] += IPU0dma(((u32*)&rgb32)+4*g_nCmdPos[1], 64-g_nCmdPos[1]);

			if( g_nCmdPos[1] < 64 )
				return FALSE;
		}

		g_nCmdPos[0] = 0;
		g_nCmdPos[1] = 0;
	}

	return TRUE;
}

// Todo - Need to add the same stop and start code as CSC
static BOOL ipuPACK(u32 val) {
	tIPU_CMD_CSC  csc ={0, 0, 0, 0, 0};
 
	*(u32*)&csc=val;
#ifdef IPU_LOG
	IPU_LOG("IPU PACK (Colorspace conversion from RGB32) command.\n");
	if (csc.OFM){	IPU_LOG("Output format is RGB16. ");}
	else{			IPU_LOG("Output format is INDX4. ");}
	if (csc.DTE){	IPU_LOG("Dithering enabled.");	}
	IPU_LOG("Number of macroblocks to be converted: %d\n", csc.MBC);
#endif
	for (;g_nCmdIndex<(int)csc.MBC; g_nCmdIndex++){

		if( g_nCmdPos[0] < 512 ) {
			g_nCmdPos[0] += getBits((u8*)&mb8+g_nCmdPos[0], 512-8*g_nCmdPos[0], 1);

			if( g_nCmdPos[0] < 64 )
				return FALSE;

			ipu_dither(&mb8, &rgb16, csc.DTE);
			if (csc.OFM){
				ipu_vq(&rgb16, indx4);
			}
		}
	
		if (csc.OFM) {
			g_nCmdPos[1] += IPU0dma(((u32*)&rgb16)+4*g_nCmdPos[1], 32-g_nCmdPos[1]);

			if( g_nCmdPos[1] < 32 )
				return FALSE;
		}
		else {
			g_nCmdPos[1] += IPU0dma(((u32*)indx4)+4*g_nCmdPos[1], 8-g_nCmdPos[1]);

			if( g_nCmdPos[1] < 8 )
				return FALSE;
		}

		g_nCmdPos[0] = 0;
		g_nCmdPos[1] = 0;
	}

	return TRUE;
}

static void ipuSETTH(u32 val) {
	s_thresh[0] = (val & 0xff);
	s_thresh[1] = ((val>>16) & 0xff);
#ifdef IPU_LOG
	IPU_LOG("IPU SETTH (Set threshold value)command %x.\n", val&0xff00ff);
#endif
}

///////////////////////
// IPU Worker Thread //
///////////////////////
#define IPU_INTERRUPT(dma) { \
	if( (cpuRegs.interrupt & (1<<dma)) ) { \
		g_nDMATransfer |= dma == DMAC_TO_IPU ? IPU_DMA_FIREINT1 : IPU_DMA_FIREINT0; \
	} \
	else { \
		hwIntcIrq(INTC_IPU); \
	} \
} \

void IPUCMD_WRITE(u32 val) {

	// don't process anything if currently busy
	if( ipuRegs->ctrl.BUSY ) {
		// wait for thread
		SysPrintf("IPU BUSY!\n");
	}

	ipuRegs->ctrl.ECD = 0;
	ipuRegs->ctrl.SCD = 0; //clear ECD/SCD
	ipuRegs->cmd.DATA = val;
	g_nCmdPos[0] = 0;

	switch (ipuRegs->cmd.CMD) {
		case SCE_IPU_BCLR:
			ipuBCLR(val);
			IPU_INTERRUPT(DMAC_TO_IPU);
			return;

		case SCE_IPU_VDEC:

			g_BP.BP+= val & 0x3F;
	
			// check if enough data in queue
			if( ipuVDEC(val) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				return;
			}

			ipuRegs->cmd.BUSY = 0x80000000;
			ipuRegs->topbusy = 0x80000000;

			break;

		case SCE_IPU_FDEC:

#ifdef IPU_LOG
			IPU_LOG("IPU FDEC command. Skip 0x%X bits, FIFO 0x%X bytes, BP 0x%X, FP %d, CHCR 0x%x\n",
				val & 0x3f,g_BP.IFC,(int)g_BP.BP,g_BP.FP,((DMACh*)&PS2MEM_HW[0xb400])->chcr);
#endif

			g_BP.BP+= val & 0x3F;

			if( ipuFDEC(val) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				return;
			}

			ipuRegs->cmd.BUSY = 0x80000000;
			ipuRegs->topbusy = 0x80000000;
	
			break;
		
		case SCE_IPU_SETTH:
			ipuSETTH(val);
			hwIntcIrq(INTC_IPU);
			return;

		case SCE_IPU_SETIQ:
#ifdef IPU_LOG
			IPU_LOG("IPU SETIQ command.\n");
#endif
#ifdef IPU_LOG
			if (val & 0x3f){
				IPU_LOG("Skip %d bits.\n", val & 0x3f);
			}
#endif
			g_BP.BP+= val & 0x3F;

			if( ipuSETIQ(ipuRegs->cmd.DATA) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				return;
			}

			break;
		case SCE_IPU_SETVQ:
			if( ipuSETVQ(ipuRegs->cmd.DATA) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				return;
			}

			break;
		case SCE_IPU_CSC:
			g_nCmdPos[1] = 0;
			g_nCmdIndex = 0;
			FreezeMMXRegs(1);
			
			if( ipuCSC(ipuRegs->cmd.DATA) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				FreezeMMXRegs(0);
				return;
			}

			FreezeMMXRegs(0);
			break;
		case SCE_IPU_PACK:

			g_nCmdPos[1] = 0;
			g_nCmdIndex = 0;
			FreezeMMXRegs(1);

			if( ipuPACK(ipuRegs->cmd.DATA) ) {
				IPU_INTERRUPT(DMAC_TO_IPU);
				FreezeMMXRegs(0);
				return;
			}

			FreezeMMXRegs(0);
			break;

		case SCE_IPU_IDEC:
			FreezeMMXRegs(1);
			if( ipuIDEC(val) ) {
				// idec done, ipu0 done too
				IPU_INTERRUPT(DMAC_FROM_IPU);
				FreezeMMXRegs(0);
				return;
			}

			ipuRegs->topbusy = 0x80000000;
			FreezeMMXRegs(0);

			break;

		case SCE_IPU_BDEC:
			FreezeMMXRegs(1);
			if( ipuBDEC(val) ) {
				// bdec done, wait for ipu0
				IPU_INTERRUPT(DMAC_FROM_IPU);
				FreezeMMXRegs(0);
				return;
			}

			ipuRegs->topbusy = 0x80000000;
			FreezeMMXRegs(0);
			
			break;
	}

	// have to resort to the thread
	ipuCurCmd = val>>28;
	ipuRegs->ctrl.BUSY = 1;
}

void IPUWorker()
{
	assert( ipuRegs->ctrl.BUSY );

	switch (ipuCurCmd) {
		case SCE_IPU_VDEC: 
			if( !ipuVDEC(ipuRegs->cmd.DATA) )
				return;

			ipuRegs->cmd.BUSY = 0;
			ipuRegs->topbusy = 0;
	
			break;

		case SCE_IPU_FDEC:
			if( !ipuFDEC(ipuRegs->cmd.DATA) )
				return;

			ipuRegs->cmd.BUSY = 0;
			ipuRegs->topbusy = 0;
	
			break;

		case SCE_IPU_SETIQ:
			if( !ipuSETIQ(ipuRegs->cmd.DATA) )
				return;

			break;
		case SCE_IPU_SETVQ:
			if( !ipuSETVQ(ipuRegs->cmd.DATA) )
				return;

			break;
		case SCE_IPU_CSC:
			if( !ipuCSC(ipuRegs->cmd.DATA) )
				return;

			break;		
		case SCE_IPU_PACK:
			if( !ipuPACK(ipuRegs->cmd.DATA) )
				return;

			break;

		case SCE_IPU_BDEC:
		case SCE_IPU_IDEC:

			FreezeMMXRegs(1);
			co_call(s_routine);
			if( !s_RoutineDone ) {
				FreezeMMXRegs(0);
				return;
			}

			ipuRegs->ctrl.BUSY = 0;
			ipuRegs->topbusy = 0;
			ipuRegs->cmd.BUSY = 0;
			ipuCurCmd = 0xffffffff;
			IPU_INTERRUPT(DMAC_FROM_IPU);
			FreezeMMXRegs(0);
			return;

		default:
			SysPrintf("Unknown IPU command: %x\n", ipuRegs->cmd.CMD);
			break;
	}

	// success
	ipuRegs->ctrl.BUSY = 0;
	ipuCurCmd = 0xffffffff;
	IPU_INTERRUPT(DMAC_TO_IPU);
}

/////////////////
// Buffer reader

// move the readbits queue
__forceinline void inc_readbits()
{
	readbits += 16;
	if( readbits >= _readbits+64 ) {

		// move back
		*(u64*)(_readbits) = *(u64*)(_readbits+64);
		*(u64*)(_readbits+8) = *(u64*)(_readbits+72);
		readbits = _readbits;
	}
}

// returns the pointer of readbits moved by 1 qword
__forceinline u8* next_readbits()
{
	return readbits + 16;
}

// returns the pointer of readbits moved by 1 qword
u8* prev_readbits()
{
	if( readbits < _readbits+16 ) {
		return _readbits+48-(readbits-_readbits);
	}
	
	return readbits-16;
}

void ReorderBitstream()
{
	readbits = prev_readbits();
	g_BP.bufferhasnew = 1;
	//g_BP.FP++;
}

// IPU has a 2qword internal buffer whose status is pointed by FP.
// If FP is 1, there's 1 qword in buffer. Second qword is only loaded
// incase there are less than 32bits available in the first qword.
// \return Number of bits available (clamps at 16 bits)
u8 FillInternalBuffer(u32 * pointer, u32 advance)
{
	if(g_BP.FP == 0)
	{
		if( fifo_wread1(next_readbits()) == 0 )
			return 0;

		g_BP.bufferhasnew = 0;
		inc_readbits();
		g_BP.FP = 1;
	}
   	else if(g_BP.FP < 2 && g_BP.bufferhasnew == 0 ) 
	{
		// in reality, need only > 96, but IPU reads ahead
		if( *(int*)pointer > 96) {
			if( !fifo_wread1(next_readbits()) )
				return 0;
			g_BP.FP += 1;
		}
	}

	if(*(int*)pointer >= 128)
	{
		assert( g_BP.FP >= 1 || g_BP.bufferhasnew);
		inc_readbits();

		if(advance)
		{
			// Incase BDEC reorders bits, we need to make sure we have the old
			// data backed up. So we store the last read qword into the 2nd slot
			// of the internal buffer. After that we copy the new qword read in
			// the 2nd slot to the 1st slot to be read.
			if( !g_BP.bufferhasnew )
				g_BP.FP--;
			g_BP.bufferhasnew = 0;
		}

		*pointer &= 127;
	}

	return (g_BP.FP+g_BP.bufferhasnew) == 1 ? g_BP.FP*128-pointer[0] : 128;
}

// whenever reading fractions of bytes. The low bits always come from the next byte
// while the high bits come from the current byte
u8 getBits32(u8 *address, u32 advance)
{
	register u32 mask, shift=0;
	u8* readpos;

	// Check if the current BP has exceeded or reached the limit of 128
	if( FillInternalBuffer(&g_BP.BP,1) < 32 )
		return 0;

	readpos = readbits+(int)g_BP.BP/8;

	if (g_BP.BP & 7) {
		
		shift = g_BP.BP&7;
		mask = (0xff>>shift);
		mask = mask|(mask<<8)|(mask<<16)|(mask<<24);
			
		*(u32*)address = ((~mask&*(u32*)(readpos+1))>>(8-shift)) | (((mask)&*(u32*)readpos)<<shift);
	}
	else
		*(u32*)address = *(u32*)readpos;

	if (advance) g_BP.BP+=32;
	return 1;
}

u8 getBits16(u8 *address, u32 advance)
{
	register u32 mask, shift=0;
	u8* readpos;

	// Check if the current BP has exceeded or reached the limit of 128
	if( FillInternalBuffer(&g_BP.BP,1) < 16 )
		return 0;

	readpos = readbits+(int)g_BP.BP/8;

	if (g_BP.BP & 7) {
		
		shift = g_BP.BP&7;
		mask = (0xff>>shift);
		mask = mask|(mask<<8);
			
		*(u16*)address = ((~mask&*(u16*)(readpos+1))>>(8-shift)) | (((mask)&*(u16*)readpos)<<shift);
	}
	else
		*(u16*)address = *(u16*)readpos;

	if (advance) g_BP.BP+=16;
	return 1;
}

u8 getBits8(u8 *address, u32 advance)
{
	register u32 mask, shift=0;
	u8* readpos;

	// Check if the current BP has exceeded or reached the limit of 128
	if( FillInternalBuffer(&g_BP.BP,1) < 8 )
		return 0;

	readpos = readbits+(int)g_BP.BP/8;

	if (g_BP.BP & 7) {
		
		shift = g_BP.BP&7;
		mask = (0xff>>shift);
			
		*(u8*)address = (((~mask)&readpos[1])>>(8-shift)) | (((mask)&*readpos)<<shift);
	}
	else
		*(u8*)address = *(u8*)readpos;

	if (advance) g_BP.BP+=8;
	return 1;
}

int getBits(u8 *address, u32 size, u32 advance)
{
	register u32 mask = 0, shift=0, howmuch;
	u8* oldbits, *oldaddr = address;
	u32 pointer = 0;

	// Check if the current BP has exceeded or reached the limit of 128
	if( FillInternalBuffer(&g_BP.BP,1) < 8 )
		return 0;
	
	oldbits = readbits;
    // Backup the current BP in case of VDEC/FDEC
	pointer = g_BP.BP;

	if (pointer & 7)
	{
		address--;
		while (size)
		{
			if (shift==0)
			{
				*++address=0;
				shift=8;
			}
			if( FillInternalBuffer(&pointer,advance) < 8 )
				return address-oldaddr;
			
			howmuch   = min(min(8-(pointer&7), 128-pointer),
						min(size, shift));
			mask	  = ((0xFF >> (pointer&7)) << 
					    (8-howmuch-(pointer&7))) & 0xFF;
			mask     &= readbits[((pointer)>>3)];
			mask    >>= 8-howmuch-(pointer&7);
			pointer  += howmuch;
			size     -= howmuch;
			shift	 -= howmuch;
			*address |= mask << shift;
		}

		++address;
	}
	else
	{
		u8* readmem;
		while (size)
		{
			if( FillInternalBuffer(&pointer,advance) < 8 )
				return address-oldaddr;
			
			howmuch  = min(128-pointer, size);
			size    -= howmuch;

			readmem = readbits + (pointer>>3);
			pointer += howmuch;
			howmuch >>= 3;
			
			while(howmuch >= 4) {
				*(u32*)address = *(u32*)readmem;
				howmuch -= 4;
				address += 4;
				readmem += 4;
			}

			switch(howmuch) {
				case 3: address[2] = readmem[2];
				case 2: address[1] = readmem[1];
				case 1: address[0] = readmem[0];
				case 0:
					break;
				default: __assume(0);
			}

			address += howmuch;
		}
	}

	// If not advance then reset the Reading buffer value
	if(advance) g_BP.BP = pointer;
	else readbits = oldbits; // restore the last pointer

	return address-oldaddr;
}

///////////////////// CORE FUNCTIONS /////////////////
void Skl_YUV_To_RGB32_MMX(u8 *RGB, const int Dst_BpS, const u8 *Y, const u8 *U, const u8 *V,
	const int Src_BpS, const int Width, const int Height);

void ipu_csc(struct macroblock_8 *mb8, struct macroblock_rgb32 *rgb32, int sgn){
	int i;
	u8* p = (u8*)rgb32;

	convert_init.start(convert_init.id, (u8*)rgb32, CONVERT_FRAME);
	convert_init.copy(convert_init.id, (u8*)mb8->Y, (u8*)mb8->Cr, (u8*)mb8->Cb, 0);

	// do alpha processing
//	if( cpucaps.hasStreamingSIMD2Extensions ) {
//		int i;
//		u8* p = (u8*)rgb32;
//
//		__asm {
//			movaps xmm6, s_thresh
//			pshufd xmm7, xmm6, 0xee
//			pshufd xmm6, xmm6, 0x44
//			pxor xmm5, xmm5
//		}
//
//		for(i = 0; i < 64; i += 4, p += 64) {
//			// process 2 qws at a time
//			__asm {
//				// extract 8 dwords
//				mov edi, p
//				movaps xmm0, qword ptr [edi]
//				movaps xmm1, qword ptr [edi+16]
//				movaps xmm2, qword ptr [edi+32]
//				movaps xmm3, qword ptr [edi+48]
//
//
//	}
	// fixes suikoden5
	if( s_thresh[0] > 0 ) {
		for(i = 0; i < 64*4; i++, p += 4) {
			if( p[0] < s_thresh[0] && p[1] < s_thresh[0] && p[2] < s_thresh[0] )
				*(u32*)p = 0;
			else
				p[3] = (p[1] < s_thresh[1] && p[2] < s_thresh[1] && p[3] < s_thresh[1]) ? 0x40 : 0x80;
		}
	}
	else if( s_thresh[1] > 0 ) {
		for(i = 0; i < 64*4; i++, p += 4) {
			p[3] = (p[1] < s_thresh[1] && p[2] < s_thresh[1] && p[3] < s_thresh[1]) ? 0x40 : 0x80;
		}
	}
	else {
		for(i = 0; i < 64; i++, p += 16) {
			p[3] = p[7] = p[11] = p[15] = 0x80;
		}
	}
}

void ipu_dither(struct macroblock_8 *mb8, struct macroblock_rgb16 *rgb16, int dte){
	SysPrintf("IPU: Dither not implemented");
}

void ipu_vq(struct macroblock_rgb16 *rgb16, u8* indx4){
	SysPrintf("IPU: VQ not implemented");
}

void ipu_copy(struct macroblock_8 *mb8, struct macroblock_16 *mb16){
	unsigned char	*s=(unsigned char*)mb8;
	signed short	*d=(signed short*)mb16;
	int i;
	for (i=0; i< 256; i++)	*d++ = *s++;		//Y  bias	- 16
	for (i=0; i< 64; i++)	*d++ = *s++;		//Cr bias	- 128
	for (i=0; i< 64; i++)	*d++ = *s++;		//Cb bias	- 128
	/*for(i = 0; i < 384/(16*6); ++i, s += 16*4, d += 16*4) {
		__m128i r0, r1, r2, r3, r4, r5, r6, r7;

		r0 = _mm_load_si128((__m128i*)s);
		r2 = _mm_load_si128((__m128i*)s+1);
		r4 = _mm_load_si128((__m128i*)s+2);
		r6 = _mm_load_si128((__m128i*)s+3);

		// signed shift
		r1 = _mm_srai_epi16(_mm_unpackhi_epi8(r0, r0), 8);
		r0 = _mm_srai_epi16(_mm_unpacklo_epi8(r0, r0), 8);
		r3 = _mm_srai_epi16(_mm_unpackhi_epi8(r2, r2), 8);
		r2 = _mm_srai_epi16(_mm_unpacklo_epi8(r2, r2), 8);
		r5 = _mm_srai_epi16(_mm_unpackhi_epi8(r4, r4), 8);
		r4 = _mm_srai_epi16(_mm_unpacklo_epi8(r4, r4), 8);
		r7 = _mm_srai_epi16(_mm_unpackhi_epi8(r6, r6), 8);
		r6 = _mm_srai_epi16(_mm_unpacklo_epi8(r6, r6), 8);

		_mm_store_si128((__m128i*)d, r0);
		_mm_store_si128((__m128i*)d+1, r1);
		_mm_store_si128((__m128i*)d+2, r2);
		_mm_store_si128((__m128i*)d+3, r3);
		_mm_store_si128((__m128i*)d+4, r4);
		_mm_store_si128((__m128i*)d+5, r5);
		_mm_store_si128((__m128i*)d+6, r6);
		_mm_store_si128((__m128i*)d+7, r7);
	}*/
}

///////////////////// IPU DMA ////////////////////////
void fifo_wclear()
{
	//assert( g_BP.IFC == 0 );
	//memset(fifo_input,0,sizeof(fifo_input));
	g_BP.IFC = 0;
	readwpos = 0;
	writewpos = 0; 
}

int fifo_wread(void *value, int size)
{
	int transsize, firstsize;

	transsize = size;

	// transfer what is left in fifo
	firstsize = min((int)g_BP.IFC, size);
	g_BP.IFC -= firstsize;
	size -= firstsize;

	while( firstsize-- > 0) {
		
		// transfer firstsize qwords, split into two transfers
		((u32*)value)[0] = fifo_input[readwpos];
		((u32*)value)[1] = fifo_input[readwpos+1];
		((u32*)value)[2] = fifo_input[readwpos+2];
		((u32*)value)[3] = fifo_input[readwpos+3];
		readwpos = (readwpos + 4) & 31;
		value = (u32*)value + 4;
	}

	if(size > 0) {

		// fifo is too small, so have to get from DMA
		while( IPU1dma() > 0 ) {

			firstsize = min(size, (int)g_BP.IFC);
			size -= firstsize;
			g_BP.IFC -= firstsize;
			
			while( firstsize-- > 0) {
		
				// transfer firstsize qwords, split into two transfers
				((u32*)value)[0] = fifo_input[readwpos];
				((u32*)value)[1] = fifo_input[readwpos+1];
				((u32*)value)[2] = fifo_input[readwpos+2];
				((u32*)value)[3] = fifo_input[readwpos+3];
				readwpos = (readwpos + 4) & 31;
				value = (u32*)value + 4;
			}

			if( size == 0 )
				return transsize;
		}
	}

	return transsize-size;
}

int fifo_wread1(void *value)
{
	// wait until enough data
	if( g_BP.IFC == 0 ) {
		if( IPU1dma() == 0 )
			return 0;

		assert( g_BP.IFC > 0 );
	}

	// transfer 1 qword, split into two transfers
	((u32*)value)[0] = fifo_input[readwpos];
	((u32*)value)[1] = fifo_input[readwpos+1];
	((u32*)value)[2] = fifo_input[readwpos+2];
	((u32*)value)[3] = fifo_input[readwpos+3];
	readwpos = (readwpos + 4) & 31;
	g_BP.IFC--;
	return 1;
}

int fifo_wwrite(u32* pMem, int size)
{
	int transsize;
	int firsttrans = min(size, 8-(int)g_BP.IFC);

	g_BP.IFC+=firsttrans;
	transsize = firsttrans;

	while(transsize-- > 0) {
		fifo_input[writewpos] = pMem[0];
		fifo_input[writewpos+1] = pMem[1];
		fifo_input[writewpos+2] = pMem[2];
		fifo_input[writewpos+3] = pMem[3];
		writewpos = (writewpos+4)&31;
		pMem +=4;
	}

	return firsttrans;
}

#define IPU1chain() { \
	int qwc = ipu1dma->qwc; \
	pMem = (u32*)dmaGetAddr(ipu1dma->madr); \
	if (pMem == NULL) { SysPrintf("ipu1dma NULL!\n"); return totalqwc; } \
	qwc = fifo_wwrite(pMem, qwc); \
	ipu1dma->madr += qwc<< 4; \
	ipu1dma->qwc -= qwc; \
	totalqwc += qwc; \
	if( ipu1dma->qwc > 0 ) { \
		g_nDMATransfer |= IPU_DMA_ACTV1; \
		return totalqwc; \
	} \
}

int IPU1dma()
{
	u32 *ptag, *pMem;
	int done=0;
	int ipu1cycles = 0;
	int totalqwc = 0;

	assert( !(ipu1dma->chcr&0x40) );

	if( !(ipu1dma->chcr & 0x100) || (cpuRegs.interrupt & (1<<DMAC_TO_IPU)) ) {
		// wait for ipu interrupt
		// just in case
		if( cpuRegs.interrupt & (1<<DMAC_TO_IPU) )
			g_nDMATransfer |= IPU_DMA_TIE1;
		return 0;
	}

	assert( !(g_nDMATransfer & IPU_DMA_TIE1) );

	// in kh, qwc == 0 when dma_actv1 is set
	if( (g_nDMATransfer & IPU_DMA_ACTV1) && ipu1dma->qwc > 0 ) {
		IPU1chain();

		if ((ipu1dma->chcr & 0x80) && (g_nDMATransfer&IPU_DMA_DOTIE1)) {			 //Check TIE bit of CHCR and IRQ bit of tag
			SysPrintf("IPU1 TIE\n");

			INT(DMAC_TO_IPU, totalqwc*BIAS);
			g_nDMATransfer &= ~(IPU_DMA_ACTV1|IPU_DMA_DOTIE1);
			g_nDMATransfer |= IPU_DMA_TIE1;
			return totalqwc;
		}

		g_nDMATransfer &= ~(IPU_DMA_ACTV1|IPU_DMA_DOTIE1);

		if( (ipu1dma->chcr&0xc) == 0 ) {
			INT(DMAC_TO_IPU, totalqwc*BIAS);
			return totalqwc;
		}
		else {
			u32 tag = ipu1dma->chcr; // upper bits describe current tag
			
			if ((ipu1dma->chcr & 0x80) && (tag&0x80000000)) {
				ptag = (u32*)dmaGetAddr(ipu1dma->tadr);

				switch(tag&0x70000000) {
					case 0x00000000: ipu1dma->tadr += 16; break;
					case 0x70000000: ipu1dma->tadr = ipu1dma->madr; break;
				}

				ipu1dma->chcr = (ipu1dma->chcr & 0xFFFF) | ( (*ptag) & 0xFFFF0000 );
#ifdef IPU_LOG
				IPU_LOG("dmaIrq Set\n"); 
#endif
				INT(DMAC_TO_IPU, totalqwc*BIAS);
				g_nDMATransfer |= IPU_DMA_TIE1;
				return totalqwc;
			}

			switch( tag&0x70000000 )
			{
			case 0x00000000:
				ipu1dma->tadr += 16;
				INT(DMAC_TO_IPU, totalqwc*BIAS);
				return totalqwc;

			case 0x70000000:
				ipu1dma->tadr = ipu1dma->madr;
				INT(DMAC_TO_IPU, totalqwc*BIAS);
				return totalqwc;
			}
		}
	}

	if ((ipu1dma->chcr & 0xc) == 0 && ipu1dma->qwc == 0) { // Normal Mode
		//SysPrintf("ipu1 normal empty qwc?\n");
		return totalqwc;
	}

	// Transfer Dn_QWC from Dn_MADR to GIF
	if( ipu1dma->qwc > 0 ) {
		IPU1chain();
	}
	if ((ipu1dma->chcr & 0xc) == 0 ) { // Normal Mode
	}
	else {
		// Chain Mode
		while (done == 0) {						 // Loop while Dn_CHCR.STR is 1
			ptag = (u32*)dmaGetAddr(ipu1dma->tadr);  //Set memory pointer to TADR
			if (ptag == NULL) {					 //Is ptag empty?
				psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register
				break;
			}
			ipu1cycles+=1; // Add 1 cycles from the QW read for the tag
			
			ipu1dma->chcr = ( ipu1dma->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
			ipu1dma->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
			//ipu1dma->madr = ptag[1];				    //MADR = ADDR field

#ifdef IPU_LOG
			IPU_LOG("dmaIPU1 dmaChain %8.8x_%8.8x size=%d, addr=%lx, fifosize=%x\n",
				ptag[1], ptag[0], ipu1dma->qwc, ipu1dma->madr, 8 - g_BP.IFC);
#endif
			
			//done = hwDmacSrcChainWithStack(ipu1dma, id);
			switch(ptag[0] & 0x70000000) {
				case 0x00000000: // refe
					// do not change tadr
					ipu1dma->madr = ptag[1];
					done = 1;
					break;

				case 0x10000000: // cnt
					ipu1dma->madr = ipu1dma->tadr + 16;
					// Set the taddr to the next tag
					ipu1dma->tadr += 16 + (ipu1dma->qwc << 4);
					break;

				case 0x20000000: // next
					ipu1dma->madr = ipu1dma->tadr + 16;
					ipu1dma->tadr = ptag[1];
					break;

				case 0x30000000: // ref
					ipu1dma->madr = ptag[1];
					ipu1dma->tadr += 16;
					break;

				case 0x70000000: // end
					// do not change tadr
					ipu1dma->madr = ipu1dma->tadr + 16;
					done = 1;
					break;

				default:
	#ifdef IPU_LOG
					IPU_LOG("ERROR: different transfer mode!, Please report to PCSX2 Team\n");
	#endif
					break;
			}

			if( ptag[0] & 0x80000000 ) 
				g_nDMATransfer |= IPU_DMA_DOTIE1;
			else
				g_nDMATransfer &= ~IPU_DMA_DOTIE1;

			IPU1chain();

			if ((ipu1dma->chcr & 0x80) && (ptag[0]&0x80000000) ) {			 //Check TIE bit of CHCR and IRQ bit of tag
				SysPrintf("IPU1 TIE\n");

				if( done ) {
					ptag = (u32*)dmaGetAddr(ipu1dma->tadr);

					switch(ptag[0]&0x70000000) {
						case 0x00000000: ipu1dma->tadr += 16; break;
						case 0x70000000: ipu1dma->tadr = ipu1dma->madr; break;
					}

					ipu1dma->chcr = (ipu1dma->chcr & 0xFFFF) | ( (*ptag) & 0xFFFF0000 );
				}

				INT(DMAC_TO_IPU, ipu1cycles+totalqwc*BIAS);
				g_nDMATransfer |= IPU_DMA_TIE1;
				return totalqwc;
			}
		}

		switch( ptag[0]&0x70000000 )
		{
		case 0x00000000:
			ipu1dma->tadr += 16;
			INT(DMAC_TO_IPU, totalqwc*BIAS);
			return totalqwc;

		case 0x70000000:
			ipu1dma->tadr = ipu1dma->madr;
			INT(DMAC_TO_IPU, totalqwc*BIAS);
			return totalqwc;
		}
	}

	INT(DMAC_TO_IPU, ipu1cycles+totalqwc*BIAS);
	return totalqwc;
}

int IPU0dma(const void* ptr, int size)
{
	int readsize;
	void* pMem;

	if( !(ipu0dma->chcr & 0x100) || (cpuRegs.interrupt & (1<<DMAC_FROM_IPU)) ) {
		// wait for ipu interrupt
		// just in case
		if( cpuRegs.interrupt & (1<<DMAC_FROM_IPU) ) {
			g_nDMATransfer |= IPU_DMA_TIE0;
		}
		return 0;
	}

	if(ipu0dma->qwc == 0)
		return 0;

	assert( !(ipu0dma->chcr&0x40) );

#ifdef IPU_LOG
	IPU_LOG("dmaIPU0 chcr = %lx, madr = %lx, qwc  = %lx, ipuout_fifo.count=%x\n",
			ipu0dma->chcr, ipu0dma->madr, ipu0dma->qwc, size);
#endif

	assert((ipu0dma->chcr & 0xC) == 0 );
	pMem = (u32*)dmaGetAddr(ipu0dma->madr);

	readsize = min(ipu0dma->qwc, size);
		
	memcpy_amd(pMem, ptr, readsize<<4);
	ipu0dma->madr += readsize<< 4;
	ipu0dma->qwc -= readsize; // note: qwc is u16
	
	if(ipu0dma->qwc == 0) {
		if ((psHu32(DMAC_CTRL) & 0x30) == 0x30) { // STS == fromIPU
			psHu32(DMAC_STADR) = ipu0dma->madr;
			switch (psHu32(DMAC_CTRL) & 0xC0) {
				case 0x80: // GIF
					g_nDMATransfer |= IPU_DMA_GIFSTALL;
					break;
			}
		}

		INT(DMAC_FROM_IPU, (readsize*BIAS));
	}

	return readsize;
}

void dmaIPU0() // fromIPU
{
	if( ipuRegs->ctrl.BUSY )
		IPUWorker();
}

void dmaIPU1() // toIPU
{
	if( ipuRegs->ctrl.BUSY )
		IPUWorker();
}

int ipu0Interrupt() {
#ifdef IPU_LOG 
	IPU_LOG("ipu0Interrupt:\n", cpuRegs.cycle);
#endif

	if( g_nDMATransfer & IPU_DMA_FIREINT0 ) {
		hwIntcIrq(INTC_IPU);
		g_nDMATransfer &= ~IPU_DMA_FIREINT0;
	}

	if( g_nDMATransfer & IPU_DMA_GIFSTALL ) {
		// gif
		g_nDMATransfer &= ~IPU_DMA_GIFSTALL;
		dmaGIF();
	}

	if( g_nDMATransfer & IPU_DMA_TIE0 ) {
		g_nDMATransfer &= ~IPU_DMA_TIE0;
		hwDmacIrq(DMAC_FROM_IPU);
	}
	else {
		ipu0dma->chcr &= ~0x100;
		hwDmacIrq(DMAC_FROM_IPU);
	}

	return 1;
}

int ipu1Interrupt() {
#ifdef IPU_LOG 
	IPU_LOG("ipu1Interrupt:\n", cpuRegs.cycle);
#endif

	if( g_nDMATransfer & IPU_DMA_FIREINT1 ) {
		hwIntcIrq(INTC_IPU);
		g_nDMATransfer &= ~IPU_DMA_FIREINT1;
	}

	if( g_nDMATransfer & IPU_DMA_TIE1 ) {
		g_nDMATransfer &= ~IPU_DMA_TIE1;
		hwDmacIrq(DMAC_TO_IPU);
	}
	else {
		ipu1dma->chcr &= ~0x100;
		hwDmacIrq(DMAC_TO_IPU);
	}

	return 1;
}
