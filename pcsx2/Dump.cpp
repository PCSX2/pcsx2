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

#include "Counters.h"
#include "iCore.h"
#include "iR5900.h"
#include "IPU/IPU.h"
#include "DebugTools/SymbolMap.h"
#include "Config.h"

#include "common/FileSystem.h"
#include "common/Path.h"

#include "fmt/core.h"

using namespace R5900;

// fixme: currently should not be uncommented.
//#define TEST_BROKEN_DUMP_ROUTINES

#ifdef TEST_BROKEN_DUMP_ROUTINES
extern tIPU_BP g_BP;

#define VF_VAL(x) ((x==0x80000000)?0:(x))
#endif


// iR5900-32.cpp
extern EEINST* s_pInstCache;
extern u32 s_nEndBlock; // what pc the current block ends


void iDumpPsxRegisters(u32 startpc, u32 temp)
{
// [TODO] fixme : this code is broken and has no labels.  Needs a rewrite to be useful.

#ifdef TEST_BROKEN_DUMP_ROUTINES
	int i;
	const char* pstr = temp ? "t" : "";

	// fixme: PSXM doesn't exist any more.
	//__Log("%spsxreg: %x %x ra:%x k0: %x %x", pstr, startpc, psxRegs.cycle, psxRegs.GPR.n.ra, psxRegs.GPR.n.k0, *(int*)PSXM(0x13c128));

	for(i = 0; i < 34; i+=2) __Log("%spsx%s: %x %x", pstr, disRNameGPR[i], psxRegs.GPR.r[i], psxRegs.GPR.r[i+1]);

	DbgCon.WriteLn("%scycle: %x %x %x; counters %x %x", pstr, psxRegs.cycle, g_iopNextEventCycle, EEsCycle,
		psxNextsCounter, psxNextCounter);

	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 2) + hw_dma(2).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 3) + hw_dma(3).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 4) + hw_dma(4).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 6) + hw_dma(6).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 7) + hw_dma(7).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 8) + hw_dma(8).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 9) + hw_dma(9).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 10) + hw_dma(10).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 11) + hw_dma(11).desc());
	DbgCon.WriteLn(wxsFormat(L"psxdma%d ", 12) + hw_dma(12).desc());

	for(i = 0; i < 7; ++i)
		DbgCon.WriteLn("%scounter%d: mode %x count %I64x rate %x scycle %x target %I64x", pstr, i, psxCounters[i].mode, psxCounters[i].count, psxCounters[i].rate, psxCounters[i].sCycleT, psxCounters[i].target);
#endif
}

void iDumpRegisters(u32 startpc, u32 temp)
{
// [TODO] fixme : this code is broken and has no labels.  Needs a rewrite to be useful.

#ifdef TEST_BROKEN_DUMP_ROUTINES

	int i;
	const char* pstr;// = temp ? "t" : "";
	const u32 dmacs[] = {0x8000, 0x9000, 0xa000, 0xb000, 0xb400, 0xc000, 0xc400, 0xc800, 0xd000, 0xd400 };
	const char* psymb;

	if (temp)
		pstr = "t";
	else
		pstr = "";

	psymb = disR5900GetSym(startpc);

	if( psymb != NULL )
		__Log("%sreg(%s): %x %x c:%x", pstr, psymb, startpc, cpuRegs.interrupt, cpuRegs.cycle);
	else
		__Log("%sreg: %x %x c:%x", pstr, startpc, cpuRegs.interrupt, cpuRegs.cycle);

	for(i = 1; i < 32; ++i) __Log("%s: %x_%x_%x_%x", disRNameGPR[i], cpuRegs.GPR.r[i].UL[3], cpuRegs.GPR.r[i].UL[2], cpuRegs.GPR.r[i].UL[1], cpuRegs.GPR.r[i].UL[0]);

	//for(i = 0; i < 32; i+=4) __Log("cp%d: %x_%x_%x_%x", i, cpuRegs.CP0.r[i], cpuRegs.CP0.r[i+1], cpuRegs.CP0.r[i+2], cpuRegs.CP0.r[i+3]);
	//for(i = 0; i < 32; ++i) __Log("%sf%d: %f %x", pstr, i, fpuRegs.fpr[i].f, fpuRegs.fprc[i]);
	//for(i = 1; i < 32; ++i) __Log("%svf%d: %f %f %f %f, vi: %x", pstr, i, VU0.VF[i].F[3], VU0.VF[i].F[2], VU0.VF[i].F[1], VU0.VF[i].F[0], VU0.VI[i].UL);

	for(i = 0; i < 32; ++i) __Log("%sf%d: %x %x", pstr, i, fpuRegs.fpr[i].UL, fpuRegs.fprc[i]);
	for(i = 1; i < 32; ++i) __Log("%svf%d: %x %x %x %x, vi: %x", pstr, i, VU0.VF[i].UL[3], VU0.VF[i].UL[2], VU0.VF[i].UL[1], VU0.VF[i].UL[0], VU0.VI[i].UL);

	__Log("%svfACC: %x %x %x %x", pstr, VU0.ACC.UL[3], VU0.ACC.UL[2], VU0.ACC.UL[1], VU0.ACC.UL[0]);
	__Log("%sLO: %x_%x_%x_%x, HI: %x_%x_%x_%x", pstr, cpuRegs.LO.UL[3], cpuRegs.LO.UL[2], cpuRegs.LO.UL[1], cpuRegs.LO.UL[0],
	cpuRegs.HI.UL[3], cpuRegs.HI.UL[2], cpuRegs.HI.UL[1], cpuRegs.HI.UL[0]);
	__Log("%sCycle: %x %x, Count: %x", pstr, cpuRegs.cycle, g_nextEventCycle, cpuRegs.CP0.n.Count);

	iDumpPsxRegisters(psxRegs.pc, temp);

	__Log("f410,30,40: %x %x %x, %d %d", psHu32(0xf410), psHu32(0xf430), psHu32(0xf440), rdram_sdevid, rdram_devices);
	__Log("cyc11: %x %x; vu0: %x, vu1: %x", cpuRegs.sCycle[1], cpuRegs.eCycle[1], VU0.cycle, VU1.cycle);

	__Log("%scounters: %x %x; psx: %x %x", pstr, nextsCounter, nextCounter, psxNextsCounter, psxNextCounter);

	// fixme: The members of the counters[i] struct are wrong here.
	/*for(i = 0; i < 4; ++i) {
		__Log("eetimer%d: count: %x mode: %x target: %x %x; %x %x; %x %x %x %x", i,
			counters[i].count, counters[i].mode, counters[i].target, counters[i].hold, counters[i].rate,
			counters[i].interrupt, counters[i].Cycle, counters[i].sCycle, counters[i].CycleT, counters[i].sCycleT);
	}*/
	__Log("VIF0_STAT = %x, VIF1_STAT = %x", psHu32(0x3800), psHu32(0x3C00));
	__Log("ipu %x %x %x %x; bp: %x %x %x %x", psHu32(0x2000), psHu32(0x2010), psHu32(0x2020), psHu32(0x2030), g_BP.BP, g_BP.bufferhasnew, g_BP.FP, g_BP.IFC);
	__Log("gif: %x %x %x", psHu32(0x3000), psHu32(0x3010), psHu32(0x3020));

	for(i = 0; i < std::size(dmacs); ++i) {
		DMACh* p = (DMACh*)(&eeHw[dmacs[i]]);
		__Log("dma%d c%x m%x q%x t%x s%x", i, p->chcr._u32, p->madr, p->qwc, p->tadr, p->sadr);
	}
	__Log(L"dmac " + dmacRegs.ctrl.desc() + L" " + dmacRegs.stat.desc() + L" " + dmacRegs.rbsr.desc() + L" " + dmacRegs.rbor.desc());
	__Log(L"intc " + intcRegs->stat.desc() + L" " +  intcRegs->mask.desc());
	__Log("sif: %x %x %x %x %x", psHu32(SBUS_F200), psHu32(SBUS_F220), psHu32(SBUS_F230), psHu32(SBUS_F240), psHu32(SBUS_F260));
#endif
}

void iDumpVU0Registers()
{
	// fixme: This code is outdated, broken, and lacks printed labels.
	// Needs heavy mods to be useful.
#ifdef TEST_BROKEN_DUMP_ROUTINES
	for(int i = 1; i < 32; ++i) {
		__Log("v%d: %x %x %x %x, vi: ", i, VF_VAL(VU0.VF[i].UL[3]), VF_VAL(VU0.VF[i].UL[2]),
			VF_VAL(VU0.VF[i].UL[1]), VF_VAL(VU0.VF[i].UL[0]));

        switch (i)
        {
            case REG_Q:
            case REG_P:
                __Log("%f", VU0.VI[i].F);
                break;
            case REG_MAC_FLAG:
                __Log("%x", 0);//VU0.VI[i].UL&0xff);
                break;
            case REG_STATUS_FLAG:
                __Log("%x", 0);//VU0.VI[i].UL&0x03);
                break;
            case REG_CLIP_FLAG:
                __Log("0");
                break;
            default:
                __Log("%x", VU0.VI[i].UL);
                break;
        }
	}
	__Log("vfACC: %f %f %f %f\n", VU0.ACC.F[3], VU0.ACC.F[2], VU0.ACC.F[1], VU0.ACC.F[0]);
#endif
}

void iDumpVU1Registers()
{
	// fixme: This code is outdated, broken, and lacks printed labels.
	// Needs heavy mods to be useful.
#ifdef TEST_BROKEN_DUMP_ROUTINES
	int i;

//	static int icount = 0;
//	__Log("%x\n", icount);

	for(i = 1; i < 32; ++i) {

//		__Log("v%d: w%f(%x) z%f(%x) y%f(%x) x%f(%x), vi: ", i, VU1.VF[i].F[3], VU1.VF[i].UL[3], VU1.VF[i].F[2], VU1.VF[i].UL[2],
//			VU1.VF[i].F[1], VU1.VF[i].UL[1], VU1.VF[i].F[0], VU1.VF[i].UL[0]);
		//__Log("v%d: %f %f %f %f, vi: ", i, VU1.VF[i].F[3], VU1.VF[i].F[2], VU1.VF[i].F[1], VU1.VF[i].F[0]);

		__Log("v%d: %x %x %x %x, vi: ", i, VF_VAL(VU1.VF[i].UL[3]), VF_VAL(VU1.VF[i].UL[2]), VF_VAL(VU1.VF[i].UL[1]), VF_VAL(VU1.VF[i].UL[0]));

		if( i == REG_Q || i == REG_P ) __Log("%f\n", VU1.VI[i].F);
		//else __Log("%x\n", VU1.VI[i].UL);
		else __Log("%x\n", (i==REG_STATUS_FLAG||i==REG_MAC_FLAG||i==REG_CLIP_FLAG)?0:VU1.VI[i].UL);
	}
	__Log("vfACC: %f %f %f %f\n", VU1.ACC.F[3], VU1.ACC.F[2], VU1.ACC.F[1], VU1.ACC.F[0]);
#endif
}

// This function is close of iDumpBlock but it doesn't rely too much on
// global variable. Beside it doesn't print the flag info.
//
// However you could call it anytime to dump any block. And we have both
// x86 and EE disassembly code
void iDumpBlock(u32 ee_pc, u32 ee_size, uptr x86_pc, u32 x86_size)
{
	u32 ee_end = ee_pc + ee_size;

	DbgCon.WriteLn( Color_Gray, "dump block %x:%x (x86:0x%x)", ee_pc, ee_end, x86_pc );

	FileSystem::CreateDirectoryPath(EmuFolders::Logs.c_str(), false);
	std::string dump_filename(Path::Combine(EmuFolders::Logs, fmt::format("R5900dump_{:.8X}:{:.8X}.txt", ee_pc, ee_end) ));
	std::FILE* eff = FileSystem::OpenCFile(dump_filename.c_str(), "w");
	if (!eff)
		return;

	// Print register content to detect the memory access type. Warning value are taken
	// during the call of this function. There aren't the real value of the block.
	std::fprintf(eff, "Dump register data: 0x%p\n", &cpuRegs.GPR.r[0].UL[0]);
	for (int reg = 0; reg < 32; reg++) {
		// Only lower 32 bits (enough for address)
		std::fprintf(eff, "\t%2s <= 0x%08x_%08x\n", R5900::GPR_REG[reg], cpuRegs.GPR.r[reg].UL[1],cpuRegs.GPR.r[reg].UL[0]);
	}
	std::fprintf(eff, "\n");


	if (!R5900SymbolMap.GetLabelString(ee_pc).empty())
	{
		std::fprintf(eff, "%s\n", R5900SymbolMap.GetLabelString(ee_pc).c_str() );
	}

	for ( u32 i = ee_pc; i < ee_end; i += 4 )
	{
		std::string output;
		//TLB Issue disR5900Fasm( output, memRead32( i ), i, false );
		disR5900Fasm( output, psMu32(i), i, false );
		std::fprintf(eff, "0x%.X : %s\n", i, output.c_str() );
	}

	// Didn't find (search) a better solution
	std::fprintf(eff, "\nRaw x86 dump (https://www.onlinedisassembler.com/odaweb/):\n");
	u8* x86 = (u8*)x86_pc;
	for (u32 i = 0; i < x86_size; i++) {
		std::fprintf(eff, "%.2X", x86[i]);
	}
	std::fprintf(eff, "\n\n");

	std::fclose(eff); // Close the file so it can be appended by objdump

	// handy but slow solution (system call)
#ifdef __linux__
	std::string obj_filename(Path::Combine(EmuFolders::Logs, "objdump_tmp.o"));
	std::FILE* objdump = FileSystem::OpenCFile(obj_filename.c_str(), "wb");
	if (!objdump)
		return;
	std::fwrite(x86, x86_size, 1, objdump);
	std::fclose(objdump);

	int status = std::system(
			fmt::format( "objdump -D -b binary -mi386 --disassembler-options=intel --no-show-raw-insn --adjust-vma=%d %s >> %s",
				   (u32) x86_pc, obj_filename, dump_filename).c_str()
			);

	if (!WIFEXITED(status))
		Console.Error("IOP dump didn't terminate normally");
#endif
}


// Originally from iR5900-32.cpp
void iDumpBlock( int startpc, u8 * ptr )
{
	u8 used[34];
	u8 fpuused[33];
	int numused, fpunumused;

	DbgCon.WriteLn( Color_Gray, "dump1 %x:%x, %x", startpc, pc, cpuRegs.cycle );

	FileSystem::CreateDirectoryPath(EmuFolders::Logs.c_str(), false);
	std::FILE* eff = FileSystem::OpenCFile(Path::Combine(EmuFolders::Logs, fmt::format("R5900dump{:.8X}.txt", startpc)).c_str(), "w");
	if (!eff)
		return;

	if (!R5900SymbolMap.GetLabelString(startpc).empty())
	{
		std::fprintf(eff, "%s\n", R5900SymbolMap.GetLabelString(startpc).c_str() );
	}

	for ( uint i = startpc; i < s_nEndBlock; i += 4 )
	{
		std::string output;
		disR5900Fasm( output, memRead32( i ), i, false );
		std::fprintf(eff, "%s\n", output.c_str() );
	}

	// write the instruction info

	std::fprintf(eff, "\n\nlive0 - %x, live2 - %x, lastuse - %x\nxmm - %x, used - %x\n",
		EEINST_LIVE0, EEINST_LIVE2, EEINST_LASTUSE, EEINST_XMM, EEINST_USED
	);

	memzero(used);
	numused = 0;
	for(uint i = 0; i < std::size(s_pInstCache->regs); ++i) {
		if( s_pInstCache->regs[i] & EEINST_USED ) {
			used[i] = 1;
			numused++;
		}
	}

	memzero(fpuused);
	fpunumused = 0;
	for(uint i = 0; i < std::size(s_pInstCache->fpuregs); ++i) {
		if( s_pInstCache->fpuregs[i] & EEINST_USED ) {
			fpuused[i] = 1;
			fpunumused++;
		}
	}

	std::fprintf(eff, "       " );
	for(uint i = 0; i < std::size(s_pInstCache->regs); ++i) {
		if( used[i] ) std::fprintf(eff, "%2d ", i );
	}
	std::fprintf(eff, "\n" );
	for(uint i = 0; i < std::size(s_pInstCache->fpuregs); ++i) {
		if( fpuused[i] ) std::fprintf(eff, "%2d ", i );
	}

	std::fprintf(eff, "\n" );
	std::fprintf(eff, "       " );

	// TODO : Finish converting this over to wxWidgets wxFile stuff...
	/*
	int count;
	EEINST* pcur;

	for(uint i = 0; i < std::size(s_pInstCache->regs); ++i) {
		if( used[i] ) fprintf(f, "%s ", disRNameGPR[i]);
	}
	for(uint i = 0; i < std::size(s_pInstCache->fpuregs); ++i) {
		if( fpuused[i] ) fprintf(f, "%s ", i<32?"FR":"FA");
	}
	fprintf(f, "\n");

	pcur = s_pInstCache+1;
	for( uint i = 0; i < (s_nEndBlock-startpc)/4; ++i, ++pcur) {
		fprintf(f, "%2d: %2.2x ", i+1, pcur->info);

		count = 1;
		for(uint j = 0; j < std::size(s_pInstCache->regs); j++) {
			if( used[j] ) {
				fprintf(f, "%2.2x%s", pcur->regs[j], ((count%8)&&count<numused)?"_":" ");
				++count;
			}
		}
		count = 1;
		for(uint j = 0; j < std::size(s_pInstCache->fpuregs); j++) {
			if( fpuused[j] ) {
				fprintf(f, "%2.2x%s", pcur->fpuregs[j], ((count%8)&&count<fpunumused)?"_":" ");
				++count;
			}
		}
		fprintf(f, "\n");
	}
	fclose( f );*/
}
