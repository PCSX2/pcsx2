/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#include "Common.h"
#include "PsxCommon.h"
#include "CDVDisodrv.h"
#include "VUmicro.h"
#ifdef _WIN32
#include <windows.h>
#include "RDebug/deci2.h"
#else
#include <sys/time.h>
#endif

#include "VU.h"
#include "iCore.h"
#include "iVUzerorec.h"

#include "GS.h"
#include "COP0.h"
#include "Cache.h"

#include "Paths.h"

u32 dwCurSaveStateVer = 0;
extern u32 s_iLastCOP0Cycle;
extern u32 s_iLastPERFCycle[2];
extern int g_psxWriteOk;

PcsxConfig Config;
u32 BiosVersion;
char CdromId[12];
static int g_Pcsx2Recording = 0; // true 1 if recording video and sound

const char *LabelAuthors = { N_(
	"PCSX2, a PS2 emulator\n\n"
	"originally written by:\n"
	"saqib, refraction, zerofrog,\n"
	"shadow, linuzappz, florin,\n"
	"nachbrenner, auMatt, loser, \n"
	"alexey silinov, goldfinger\n"
	"\n"
	"Playground Mod Devs:\n"
	"Arcum42, drkIIRaziel, Cottonvibes, \n"
	"Jake.Stine, Rama\n\n"
	"Playground Testing:\n"
	"Krakatos\n"
	"\n"
	"Webmasters: CKemu, Falcon4ever")
};

const char *LabelGreets = { N_(
	"Greets to: Bobbi, Keith, CpUMasteR, Nave, Snake785\n\n"
	"Special thanks to: Gigaherz, Gabest, Sjeep, Dreamtime, F|RES, BGnome, MrBrown, \n"
	"Seta-San, Skarmeth, blackd_wd, _Demo_\n"
	"\n"
	"Credits: Hiryu && Sjeep for their libcdvd (iso parsing and filesystem driver code)\n"
	"\n"
	"Some betatester/support dudes: Belmont, bositman, ChaosCode, CKemu, crushtest,"
	"Falcon4ever, GeneralPlot, jegHegy, parotaku, Prafull, Razorblade, Rudy_X, Seta-san")
};

static struct {
	const char	*name;
	u32		size;
} ioprps[]={
	{"IOPRP14",    43845},
	{"IOPRP142",   48109},
	{"IOPRP143",   58317},
	{"IOPRP144",   58525},
	{"IOPRP15",    82741},
	{"IOPRP151",   82917},
	{"IOPRP153",   82949},
	{"IOPRP16",    91909},
	{"IOPRP165",   98901},
	{"IOPRP20",   109809},
	{"IOPRP202",  110993},
	{"IOPRP205",  119797},
	{"IOPRP21",   126857},
	{"IOPRP211",  129577},
	{"IOPRP213",  129577},
	{"IOPRP214",  140945},
	{"IOPRP22",   199257},
	{"IOPRP221",  196937},
	{"IOPRP222",  198233},
	{"IOPRP224",  201065},
	{"IOPRP23",   230329},
	{"IOPRP234",  247641},
	{"IOPRP24",   251065},
	{"IOPRP241",  251049},
	{"IOPRP242",  252409},
	{"IOPRP243",  253201},
	{"IOPRP250",  264897},
	{"IOPRP252",  265233},
	{"IOPRP253",  267217},
	{"IOPRP254",  264449},
	{"IOPRP255",  264449},
	{"IOPRP260",  248945},
	{"IOPRP270",  249121},
	{"IOPRP271",  266817},
	{"IOPRP280",  269889},
	{"IOPRP300",  275345},
	{"DNAS280",   272753},
	{"DNAS270",   251729},
	{"DNAS271",   268977},
	{"DNAS300",   278641},
	{"DNAS280",   272705},
	{"DNAS255",   264945},
	{NULL,             0}
};

void GetRPCVersion(char *ioprp, char *rpcver){
	char	*p=ioprp; 
	int i;
	struct TocEntry te;
	
	if (p && (CDVD_findfile(p+strlen("cdromN:"), &te) != -1)){
		for (i=0; ioprps[i].size>0; i++)
			if (te.fileSize==ioprps[i].size)
				break;
			if (ioprps[i].size>0)
				p=(char *)ioprps[i].name;
	}
	// fixme - Is p really supposed to be set in the middle of an if statement?
	if (p && (p=strstr(p, "IOPRP")+strlen("IOPRP"))){
		for (i=0;(i<4) && p && (*p>='0') && (*p<='9');i++, p++)	rpcver[i]=*p;
		for (   ; i<4								 ;i++     ) rpcver[i]='0';
	}
}

u32 GetBiosVersion() {
	unsigned int fileOffset=0;
	s8 *ROMVER;
	char vermaj[8];
	char vermin[8];
	struct romdir *rd;
	u32 version;
	int i;

	for (i=0; i<512*1024; i++) {
		rd = (struct romdir*)&psRu8(i);
		if (strncmp(rd->fileName, "RESET", 5) == 0)
			break; /* found romdir */
	}
	if (i == 512*1024) return -1;

	while(strlen(rd->fileName) > 0){
		if (strcmp(rd->fileName, "ROMVER") == 0){	// found romver
			ROMVER = &psRs8(fileOffset);

			strncpy(vermaj, (char *)(ROMVER+ 0), 2); vermaj[2] = 0;
			strncpy(vermin, (char *)(ROMVER+ 2), 2); vermin[2] = 0;
			version = strtol(vermaj, (char**)NULL, 0) << 8;
			version|= strtol(vermin, (char**)NULL, 0);

			return version;
		}

		if ((rd->fileSize % 0x10)==0)
			fileOffset += rd->fileSize;
		else
			fileOffset += (rd->fileSize + 0x10) & 0xfffffff0;

		rd++;
	}

	return -1;
}

//2002-09-22 (Florin)
int IsBIOS(char *filename, char *description){
	struct stat buf;
	char Bios[260], ROMVER[14+1], zone[12+1];
	FILE *fp;
	unsigned int fileOffset=0, found=FALSE;
	struct romdir rd;

	strcpy(Bios, Config.BiosDir);
	strcat(Bios, filename);

	if (stat(Bios, &buf) == -1) return FALSE;	

	fp = fopen(Bios, "rb");
	if (fp == NULL) return FALSE;

	while ((ftell(fp)<512*1024) && (fread(&rd, DIRENTRY_SIZE, 1, fp)==1))
		if (strcmp(rd.fileName, "RESET") == 0)
			break; /* found romdir */

	if ((strcmp(rd.fileName, "RESET") != 0) || (rd.fileSize == 0)) {
		fclose(fp);
		return FALSE;	//Unable to locate ROMDIR structure in file or a ioprpXXX.img
	}

	while(strlen(rd.fileName) > 0){
		if (strcmp(rd.fileName, "ROMVER") == 0){	// found romver
			unsigned int filepos=ftell(fp);
			fseek(fp, fileOffset, SEEK_SET);
			if (fread(&ROMVER, 14, 1, fp) == 0) break;
			fseek(fp, filepos, SEEK_SET);//go back
				
			switch(ROMVER[4]){
				case 'T':sprintf(zone, "T10K  "); break;
				case 'X':sprintf(zone, "Test  ");break;
				case 'J':sprintf(zone, "Japan "); break;
				case 'A':sprintf(zone, "USA   "); break;
				case 'E':sprintf(zone, "Europe"); break;
				case 'H':sprintf(zone, "HK    "); break;
				case 'P':sprintf(zone, "Free  "); break;
				case 'C':sprintf(zone, "China "); break;
				default: sprintf(zone, "%c     ",ROMVER[4]); break;//shoudn't show
			}
			sprintf(description, "%s vXX.XX(XX/XX/XXXX) %s", zone,
				ROMVER[5]=='C'?"Console":ROMVER[5]=='D'?"Devel":"");
			strncpy(description+ 8, ROMVER+ 0, 2);//ver major
			strncpy(description+11, ROMVER+ 2, 2);//ver minor
			strncpy(description+14, ROMVER+12, 2);//day
			strncpy(description+17, ROMVER+10, 2);//month
			strncpy(description+20, ROMVER+ 6, 4);//year
			found = TRUE;
		}

		if ((rd.fileSize % 0x10)==0)
			fileOffset += rd.fileSize;
		else
			fileOffset += (rd.fileSize + 0x10) & 0xfffffff0;

		if (fread(&rd, DIRENTRY_SIZE, 1, fp)==0) break;
	}
	fileOffset-=((rd.fileSize + 0x10) & 0xfffffff0) - rd.fileSize;

	fclose(fp);
	
	if (found) {
		char percent[6];

		if (buf.st_size<(int)fileOffset){
			sprintf(percent, " %d%%", buf.st_size*100/(int)fileOffset);
			strcat(description, percent);//we force users to have correct bioses,
											//not that lame scph10000 of 513KB ;-)
		}
		return TRUE;
	}

	return FALSE;	//fail quietly
}

// LOAD STUFF

// fixme - Is there any reason why we shouldn't delete this define, and replace the array lengths
// with the actual numbers?
#define ISODCL(from, to) (to - from + 1)

struct iso_directory_record {
	char length			[ISODCL (1, 1)]; /* length[1];  711 */
	char ext_attr_length		[ISODCL (2, 2)]; /* ext_attr_length[1]; 711 */
	char extent			[ISODCL (3, 10)]; /* extent[8]; 733 */
	char size			[ISODCL (11, 18)]; /* size[8]; 733 */
	char date			[ISODCL (19, 25)]; /* date[7]; 7 by 711 */
	char flags			[ISODCL (26, 26)]; /* flags[1]; */
	char file_unit_size		[ISODCL (27, 27)]; /* file_unit_size[1]; 711 */
	char interleave			[ISODCL (28, 28)]; /* interleave[1]; 711 */
	char volume_sequence_number	[ISODCL (29, 32)]; /*  volume_sequence_number[3]; 723 */
	unsigned char name_len		[ISODCL (33, 33)]; /* name_len[1]; 711 */
	char name			[1];
};

int LoadCdrom() {
	return 0;
}

int CheckCdrom() {
	u8 *buf;

	if (CDVDreadTrack(16, CDVD_MODE_2352) == -1) 
		return -1; 
	buf = CDVDgetBuffer();
	if (buf == NULL) 
		return -1;
	
	strncpy(CdromId, (char*)buf+52, 10);

	return 0;
}

int GetPS2ElfName(char *name){
	FILE *fp;
	int		f;
	char	buffer[g_MaxPath];//if a file is longer...it should be shorter :D
	char	*pos;
	static struct TocEntry tocEntry;
	int i;

	CDVDFS_init();

	// check if the file exists
	if (CDVD_findfile("SYSTEM.CNF;1", &tocEntry) != TRUE){
		SysPrintf("SYSTEM.CNF not found\n");
		return 0;//could not find; not a PS/PS2 cdvd
	}
	
	f=CDVDFS_open("SYSTEM.CNF;1", 1);
	CDVDFS_read(f, buffer, g_MaxPath);
	CDVDFS_close(f);
	
	buffer[tocEntry.fileSize]='\0';
//     SysPrintf(                                                                                                           
//             "---------------------SYSTEM.CNF---------------------\n"                                                     
//             "%s"                                                                                                         
//             "----------------------------------------------------\n", buffer);  
	
	pos=strstr(buffer, "BOOT2");
	if (pos==NULL){
		pos=strstr(buffer, "BOOT");
		if (pos==NULL) {
			SysPrintf("This is not a PS2 game!\n");
			return 0;
		}
		return 1;
	}
	pos+=strlen("BOOT2");
	while (pos && *pos && pos<=&buffer[255] 
		&& (*pos<'A' || (*pos>'Z' && *pos<'a') || *pos>'z'))
		pos++;
	if (!pos || *pos==0)
		return 0;

	sscanf(pos, "%s", name);

	if (strncmp("cdrom0:\\", name, 8) == 0) {
		strncpy(CdromId, name+8, 11); CdromId[11] = 0;
	}
	
	// inifile_read(CdromId);
	fp = fopen("System.map", "r");
	if (fp) {
		u32 addr;

		SysPrintf("Loading System.map\n", fp);
		while (!feof(fp)) {
			fseek(fp, 8, SEEK_CUR);
			buffer[0] = '0'; buffer[1] = 'x';
			for (i=2; i<10; i++) buffer[i] = fgetc(fp); buffer[i] = 0;
			addr = strtoul(buffer, (char**)NULL, 0);
			fseek(fp, 3, SEEK_CUR);
			for (i=0; i<g_MaxPath; i++) {
				buffer[i] = fgetc(fp);
				if (buffer[i] == '\n' || buffer[i] == 0) break;
			}
			if (buffer[i] == 0) break;
			buffer[i] = 0;
	
			disR5900AddSym(addr, buffer);
		}
		fclose(fp);
	}

	return 2;
}

FILE *emuLog;

#ifdef PCSX2_DEVBUILD
int Log;
u32 varLog;
#endif

u16 logProtocol;
u8  logSource;
int connected=0;

#define SYNC_LOGGING

void __Log(const char *fmt, ...) {
#ifdef EMU_LOG 
	va_list list;
	char tmp[2024];	//hm, should be enough

	va_start(list, fmt);
#ifdef _WIN32
	if (connected && logProtocol>=0 && logProtocol<0x10){
		vsprintf(tmp, fmt, list);
		sendTTYP(logProtocol, logSource, tmp);
	}//else	//!!!!! is disabled, so the text goes to ttyp AND log
#endif
	{
#ifndef LOG_STDOUT
		if (varLog & 0x80000000) {
			vsprintf(tmp, fmt, list);
			SysPrintf(tmp);
		} else if( emuLog != NULL ) {
			vfprintf(emuLog, fmt, list);
			fflush( emuLog );
		}
#else	//i assume that this will not be used (Florin)
		vsprintf(tmp, fmt, list);
		SysPrintf(tmp);
#endif
	}
	va_end(list);
#endif
}

// STATES

#define STATE_VERSION "STv6"
const char Pcsx2Header[32] = STATE_VERSION " PCSX2 " PCSX2_VERSION;

#define _PS2Esave(type) \
	if (type##freeze(FREEZE_SIZE, &fP) == -1) { \
		gzclose(f); \
		return -1; \
	} \
	fP.data = (s8*)malloc(fP.size); \
	if (fP.data == NULL) return -1; \
 \
	if (type##freeze(FREEZE_SAVE, &fP) == -1) { \
		gzclose(f); \
		return -1; \
	} \
 \
	gzwrite(f, &fP.size, sizeof(fP.size)); \
	if (fP.size) { \
		gzwrite(f, fP.data, fP.size); \
		free(fP.data); \
	}

#define _PS2Eload(type) \
	gzread(f, &fP.size, sizeof(fP.size)); \
	if (fP.size) { \
		fP.data = (s8*)malloc(fP.size); \
		if (fP.data == NULL) return -1; \
		gzread(f, fP.data, fP.size); \
	} \
	if (type##freeze(FREEZE_LOAD, &fP) == -1) { \
		/* skip */ \
		/*if (fP.size) free(fP.data); \
		gzclose(f); \
		return -1;*/ \
	} \
	if (fP.size) free(fP.data);


int SaveState(const char *file) {

	gzFile f;
	freezeData fP;

	gsWaitGS();

	SysPrintf("SaveState: %s\n", file);
	f = gzopen(file, "wb");
	if (f == NULL) return -1;

	gzwrite(f, &g_SaveVersion, 4);

	gzwrite(f, PS2MEM_BASE, 0x02000000);         // 32 MB main memory   
	gzwrite(f, PS2MEM_ROM, 0x00400000);         // 4 mb rom memory
	gzwrite(f, PS2MEM_ROM1,0x00040000);         // 256kb rom1 memory
	gzwrite(f, PS2MEM_SCRATCH, 0x00004000);         // scratch pad 

	gzwrite(f, PS2MEM_HW, 0x00010000);         // hardware memory

	gzwrite(f, (void*)&cpuRegs, sizeof(cpuRegs));   // cpu regs + COP0
	gzwrite(f, (void*)&psxRegs, sizeof(psxRegs));   // iop regs]
	gzwrite(f, (void*)&fpuRegs, sizeof(fpuRegs));   // fpu regs
	gzwrite(f, (void*)&tlb, sizeof(tlb));           // tlbs

	gzwrite(f, &EEsCycle, sizeof(EEsCycle));
	gzwrite(f, &EEoCycle, sizeof(EEoCycle));
	gzwrite(f, &psxRegs.cycle, sizeof(u32));		// used to be IOPoCycle.  This retains compatibility.
	gzwrite(f, &g_nextBranchCycle, sizeof(g_nextBranchCycle));
	gzwrite(f, &g_psxNextBranchCycle, sizeof(g_psxNextBranchCycle));

	gzwrite(f, &s_iLastCOP0Cycle, sizeof(s_iLastCOP0Cycle));
	gzwrite(f, s_iLastPERFCycle, sizeof(u32)*2);
	gzwrite(f, &g_psxWriteOk, sizeof(g_psxWriteOk));

	//hope didn't forgot any cpu....

	rcntFreeze(f, 1);
	gsFreeze(f, 1);
	vu0Freeze(f, 1);
	vu1Freeze(f, 1);
	vif0Freeze(f, 1);
	vif1Freeze(f, 1);
	sifFreeze(f, 1);
	ipuFreeze(f, 1);

	// iop now
	gzwrite(f, psxM, 0x00200000);        // 2 MB main memory
	//gzwrite(f, psxP, 0x00010000);        // pralell memory
	gzwrite(f, psxH, 0x00010000);        // hardware memory
	//gzwrite(f, psxS, 0x00010000);        // sif memory	

	sioFreeze(f, 1);
	cdrFreeze(f, 1);
	cdvdFreeze(f, 1);
	psxRcntFreeze(f, 1);
	//mdecFreeze(f, 1);
	sio2Freeze(f, 1);

	SysPrintf("Saving GS\n");
    if( CHECK_MULTIGS ) {
        // have to call in thread, otherwise weird stuff will start happening
        u64 uf = (uptr)f;
        GSRingBufSimplePacket(GS_RINGTYPE_SAVE, (u32)(uf&0xffffffff), (u32)(uf>>32), 0);
        gsWaitGS();
    }
    else {
        _PS2Esave(GS);
    }
	SysPrintf("Saving SPU2\n");
	_PS2Esave(SPU2);
	SysPrintf("Saving DEV9\n");
	_PS2Esave(DEV9);
	SysPrintf("Saving USB\n");
	_PS2Esave(USB);
	SysPrintf("Saving ok\n");

	gzclose(f);

	return 0;
}

extern u32 dumplog;
u32 s_vucount=0;

int LoadState(const char *file) {

	gzFile f;
	freezeData fP;
	int i;
	u32 dud;		// for loading unused vars.
#ifdef PCSX2_VIRTUAL_MEM
	DWORD OldProtect;
#endif

#ifdef _DEBUG
	s_vucount = 0;
#endif

	SysPrintf("LoadState: %s\n", file);
	f = gzopen(file, "rb");
	if (f == NULL) return -1;
	
	gzread(f, &dwCurSaveStateVer, 4);

	if( dwCurSaveStateVer != g_SaveVersion ) {

#ifdef PCSX2_VIRTUAL_MEM
		// pcsx2 vm supports opening these formats
		if( dwCurSaveStateVer != 0x7a30000d && dwCurSaveStateVer != 0x7a30000e  && dwCurSaveStateVer != 0x7a30000f) {
			gzclose(f);
			SysPrintf("Unsupported savestate version: %x.\n", dwCurSaveStateVer );
			return 0;
		}
#else
		gzclose(f);
		SysPrintf(
			"Savestate load aborted:\n"
			"  vTlb edition cannot safely load savestates created by the VM edition.\n" );
		return 0;
#endif
	}

	// stop and reset the system first
	gsWaitGS();

	for (i=0; i<48; i++) UnmapTLB(i);

	Cpu->Reset();
#ifndef PCSX2_NORECBUILD
	recResetVU0();
	recResetVU1();
#endif
	psxCpu->Reset();

	SysPrintf("Loading memory\n");

#ifdef PCSX2_VIRTUAL_MEM
	// make sure can write
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READWRITE, &OldProtect);
#endif

	gzread(f, PS2MEM_BASE, 0x02000000);         // 32 MB main memory   
	gzread(f, PS2MEM_ROM, 0x00400000);         // 4 mb rom memory
	gzread(f, PS2MEM_ROM1,0x00040000);         // 256kb rom1 memory
	gzread(f, PS2MEM_SCRATCH, 0x00004000);         // scratch pad 

#ifdef PCSX2_VIRTUAL_MEM
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READONLY, &OldProtect);
#endif

	gzread(f, PS2MEM_HW, 0x00010000);         // hardware memory

	SysPrintf("Loading structs\n");
	gzread(f, (void*)&cpuRegs, sizeof(cpuRegs));   // cpu regs + COP0
	gzread(f, (void*)&psxRegs, sizeof(psxRegs));   // iop regs
	gzread(f, (void*)&fpuRegs, sizeof(fpuRegs));   // fpu regs
	gzread(f, (void*)&tlb, sizeof(tlb));           // tlbs
	gzread(f, &EEsCycle, sizeof(EEsCycle));
	gzread(f, &EEoCycle, sizeof(EEoCycle));
	gzread(f, &dud, sizeof(u32));			// was IOPoCycle
	gzread(f, &g_nextBranchCycle, sizeof(g_nextBranchCycle));
	gzread(f, &g_psxNextBranchCycle, sizeof(g_psxNextBranchCycle));

	gzread(f, &s_iLastCOP0Cycle, sizeof(s_iLastCOP0Cycle));
	if( dwCurSaveStateVer >= 0x7a30000e ) {
		gzread(f, s_iLastPERFCycle, sizeof(u32)*2);
	}
	gzread(f, &g_psxWriteOk, sizeof(g_psxWriteOk));

	rcntFreeze(f, 0);
	gsFreeze(f, 0);
	vu0Freeze(f, 0);
	vu1Freeze(f, 0);
	vif0Freeze(f, 0);
	vif1Freeze(f, 0);
	sifFreeze(f, 0);
	ipuFreeze(f, 0);

	// iop now
	SysPrintf("Loading iop mem\n");
	gzread(f, psxM, 0x00200000);        // 2 MB main memory
	//gzread(f, psxP, 0x00010000);        // pralell memory
	gzread(f, psxH, 0x00010000);        // hardware memory
	//gzread(f, psxS, 0x00010000);        // sif memory	

	SysPrintf("Loading iop stuff\n");
	sioFreeze(f, 0);
	cdrFreeze(f, 0);
	cdvdFreeze(f, 0);
	psxRcntFreeze(f, 0);
	//mdecFreeze(f, 0);
	sio2Freeze(f, 0);

	SysPrintf("Loading GS\n");
    if( CHECK_MULTIGS ) {
        // have to call in thread, otherwise weird stuff will start happening
        u64 uf = (uptr)f;
        GSRingBufSimplePacket(GS_RINGTYPE_LOAD, (u32)(uf&0xffffffff), (u32)(uf>>32), 0);
        gsWaitGS();
    }
    else {
        _PS2Eload(GS);
    }
	SysPrintf("Loading SPU2\n");
	_PS2Eload(SPU2);
	SysPrintf("Loading DEV9\n");
	_PS2Eload(DEV9);
	SysPrintf("Loading USB\n");
	_PS2Eload(USB);

	SysPrintf("Loading ok\n");

	gzclose(f);
    memset(pCache,0,sizeof(_cacheS)*64);

	//dumplog |= 4;
	WriteCP0Status(cpuRegs.CP0.n.Status.val);
	for (i=0; i<48; i++) MapTLB(i);

	return 0;
}

#ifdef PCSX2_DEVBUILD

int SaveGSState(const char *file)
{
	if( g_SaveGSStream ) return -1;

	SysPrintf("SaveGSState: %s\n", file);
	g_fGSSave = gzopen(file, "wb");
	if (g_fGSSave == NULL) return -1;
	
	g_SaveGSStream = 1;
	g_nLeftGSFrames = 2;

	gzwrite(g_fGSSave, &g_nLeftGSFrames, sizeof(g_nLeftGSFrames));

	return 0;
}

extern uptr pDsp;
int LoadGSState(const char *file)
{
	int ret;
	gzFile f;
	freezeData fP;

	f = gzopen(file, "rb");
	if( f == NULL )
	{
		if( !isPathRooted( file ) )
		{
			// file not found? try prefixing with sstates folder:
			char strfile[g_MaxPath];
			CombinePaths( strfile, SSTATES_DIR, file );
			f = gzopen(strfile, "rb");
			file = strfile;
		}
	}

	if( f == NULL ) {
		SysPrintf("Failed to find gs state\n");
		return -1;
	}

	SysPrintf("LoadGSState: %s\n", file);
	
	// Always set gsIrq callback -- GS States are always exclusionary of MTGS mode
	GSirqCallback( gsIrq );

	ret = GSopen(&pDsp, "PCSX2", 0);
	if (ret != 0) {
		SysMessage (_("Error Opening GS Plugin"));
		return -1;
	}

	ret = PAD1open((void *)&pDsp);

	gzread(f, &g_nLeftGSFrames, sizeof(g_nLeftGSFrames));

	gsFreeze(f, 0);
	_PS2Eload(GS);

	RunGSState(f);
	gzclose(f);

	GSclose();
	PAD1close();

	return 0;
}

#endif

int CheckState(const char *file) {
	gzFile f;
	char header[32];

	f = gzopen(file, "rb");
	if (f == NULL) return -1;

	gzread(f, header, 32);

	gzclose(f);

	if (strncmp(STATE_VERSION " PCSX2", header, 10)) return -1;

	return 0;
}


struct LangDef {
	char id[8];
	char name[64];
};

LangDef sLangs[] = {
	{ "ar_AR", N_("Arabic") },
	{ "bg_BG", N_("Bulgarian") },
	{ "ca_CA", N_("Catalan") },
	{ "cz_CZ", N_("Czech") },
	{ "du_DU",  N_("Dutch")  },
	{ "de_DE", N_("German") },
	{ "el_EL", N_("Greek") },
	{ "en_US", N_("English") },
	{ "fr_FR", N_("French") },
	{ "hb_HB" , N_("Hebrew") },
	{ "hu_HU", N_("Hungarian") },
	{ "it_IT", N_("Italian") },
	{ "ja_JA", N_("Japanese") },
	{ "pe_PE", N_("Persian") },
	{ "po_PO", N_("Portuguese") },
	{ "po_BR", N_("Portuguese BR") },
	{ "pl_PL" , N_("Polish") },
	{ "ro_RO", N_("Romanian") },
	{ "ru_RU", N_("Russian") },
	{ "es_ES", N_("Spanish") },
	{ "sh_SH" , N_("S-Chinese") },
    { "sw_SW", N_("Swedish") },
	{ "tc_TC", N_("T-Chinese") },
	{ "tr_TR", N_("Turkish") },
	{ "", "" },
};


char *ParseLang(char *id) {
	int i=0;

	while (sLangs[i].id[0] != 0) {
		if (!strcmp(id, sLangs[i].id))
			return _(sLangs[i].name);
		i++;
	}

	return id;
}

#define NUM_STATES 10
int StatesC = 0;
extern void iDumpRegisters(u32 startpc, u32 temp);
extern void recExecuteVU0Block(void);
extern void recExecuteVU1Block(void);
extern char strgametitle[256];

char* mystrlwr( char* string )
{
	assert( string != NULL );
	while ( 0 != ( *string++ = (char)tolower( *string ) ) );
    return string;
}

static void GetSaveStateFilename( char* dest )
{
	char elfcrcText[72];
	sprintf( elfcrcText, "%8.8X.%3.3d", ElfCRC, StatesC );
	CombinePaths( dest, SSTATES_DIR, elfcrcText );
}

static void GetGSStateFilename( char* dest )
{
	char gsText[72];
	sprintf( gsText, "/%8.8X.%d.gs", ElfCRC, StatesC);
	CombinePaths( dest, SSTATES_DIR, gsText );
}

void ProcessFKeys(int fkey, int shift)
{
	int ret;
    char Text[g_MaxPath];

    assert(fkey >= 1 && fkey <= 12 );

    switch(fkey) {
        case 1:
			GetSaveStateFilename(Text);
			ret = SaveState(Text);
			break;
		case 2:
			if( shift )
				StatesC = (StatesC+NUM_STATES-1)%NUM_STATES;
			else
				StatesC = (StatesC+1)%NUM_STATES;
			SysPrintf("*PCSX2*: Selected State %ld\n", StatesC);
			if( GSchangeSaveState != NULL ) {
				GetSaveStateFilename(Text);
				GSchangeSaveState(StatesC, Text);
			}
			break;
		case 3:	
			GetSaveStateFilename(Text);
			ret = LoadState(Text);
			break;	

		case 4:
		{
			const char* limitMsg;
			u32 newOptions;
#ifdef PCSX2_NORECBUILD
            SysPrintf("frame skipping only valid for recompiler build\n");
#else
			// cycle
            if( shift ) {
                // previous
                newOptions = (Config.Options&~PCSX2_FRAMELIMIT_MASK)|(((Config.Options&PCSX2_FRAMELIMIT_MASK)+PCSX2_FRAMELIMIT_VUSKIP)&PCSX2_FRAMELIMIT_MASK);
            }
            else {
                // next
                newOptions = (Config.Options&~PCSX2_FRAMELIMIT_MASK)|(((Config.Options&PCSX2_FRAMELIMIT_MASK)+PCSX2_FRAMELIMIT_LIMIT)&PCSX2_FRAMELIMIT_MASK);
            }

			gsResetFrameSkip();

			switch(newOptions & PCSX2_FRAMELIMIT_MASK) {
				case PCSX2_FRAMELIMIT_NORMAL:
					limitMsg = "None/Normal";
					break;
				case PCSX2_FRAMELIMIT_LIMIT:
					limitMsg = "Limit";
					break;
				case PCSX2_FRAMELIMIT_SKIP:
				case PCSX2_FRAMELIMIT_VUSKIP:
					if( GSsetFrameSkip == NULL )
					{
						newOptions &= ~PCSX2_FRAMELIMIT_MASK;
						SysPrintf("Notice: GS Plugin does not support frameskipping.\n");
						limitMsg = "None/Normal";
					}
					else
					{
						// When enabling Skipping we have to make sure Skipper (GS) and Limiter (EE)
						// are properly synchronized.
						gsDynamicSkipEnable();
						limitMsg = ((newOptions & PCSX2_FRAMELIMIT_MASK) == PCSX2_FRAMELIMIT_SKIP) ? "Skip" : "VUSkip";
					}

					break;
			}
			AtomicExchange( Config.Options, newOptions );

			SysPrintf("Frame Limit Mode Changed: %s\n", limitMsg );

			// [Air]: Do we really want to save runtime changes to frameskipping?
			//SaveConfig();
		}
#endif
			break;
		// note: VK_F5-VK_F7 are reserved for GS
		case 8:
			GSmakeSnapshot("snap/");
			break;

#ifdef PCSX2_DEVBUILD
		case 10:
		{
#ifdef PCSX2_NORECBUILD
            SysPrintf("Block performances times only valid for recompiler builds\n");
#else
			int num;
			FILE* f;
			BASEBLOCKEX** ppblocks = GetAllBaseBlocks(&num, 0);

			f = fopen("perflog.txt", "w");
			while(num-- > 0 ) {
				if( ppblocks[0]->visited > 0 ) {
					fprintf(f, "%u %u %u %u\n", ppblocks[0]->startpc, (u32)(ppblocks[0]->ltime.QuadPart / ppblocks[0]->visited), ppblocks[0]->visited, ppblocks[0]->size);
				}
				ppblocks[0]->visited = 0;
				ppblocks[0]->ltime.QuadPart = 0;
				ppblocks++;
			}
			fclose(f);
			SysPrintf("perflog.txt written\n");
#endif
			break;
		}
		
		case 11:
			if( CHECK_MULTIGS ) {
				SysPrintf("Cannot make gsstates in MTGS mode\n");
			}
			else {
				if( strgametitle[0] != 0 ) {
					// only take the first two words
					char name[256], *tok;
					char gsText[256];

					tok = strtok(strgametitle, " ");
					sprintf(name, "%s_", mystrlwr(tok));
					tok = strtok(NULL, " ");
					if( tok != NULL ) strcat(name, tok);

					sprintf( gsText, "%s.%d.gs", name, StatesC);
					CombinePaths( Text, SSTATES_DIR, gsText );
				}
				else
					GetGSStateFilename( Text );

				SaveGSState(Text);
			}
			break;
#endif

		case 12:
            if( shift ) {
#ifdef PCSX2_DEVBUILD
#ifndef PCSX2_NORECBUILD
			    iDumpRegisters(cpuRegs.pc, 0);
			    SysPrintf("hardware registers dumped EE:%x, IOP:%x\n", cpuRegs.pc, psxRegs.pc);
#endif
#endif
            }
            else {
                g_Pcsx2Recording ^= 1;
                if( CHECK_MULTIGS ) {
                    GSRingBufSimplePacket(GS_RINGTYPE_RECORD, g_Pcsx2Recording, 0, 0);
                }
                else {
                    if( GSsetupRecording != NULL ) GSsetupRecording(g_Pcsx2Recording, NULL);
                    if( SPU2setupRecording != NULL ) SPU2setupRecording(g_Pcsx2Recording, NULL);  
                }
            }
			break;
    }
}

void injectIRX(char *filename){
	struct stat buf;
	char path[260], name[260], *p, *q;
	struct romdir *rd;
	int iROMDIR=-1, iIOPBTCONF=-1, iBLANK=-1, i, filesize;
	FILE *fp;

	strcpy(name, filename);
	for (i=0; name[i] && name[i]!='.' && i<10; i++) name[i]=toupper(name[i]);name[i]=0;

	//phase 1: find ROMDIR in bios
	for (p=(char*)PS2MEM_ROM; p<(char*)PS2MEM_ROM+0x80000; p++)
		if (strncmp(p, "RESET", 5)==0)
			break;
	rd=(struct romdir*)p;

	for (i=0; rd[i].fileName[0]; i++)if (strncmp(rd[i].fileName, name, strlen(name))==0)break;
	if (rd[i].fileName[0])return;//already in;)

	//phase 2: make room in IOPBTCONF & ROMDIR
	for (i=0; rd[i].fileName[0]; i++)if (strncmp(rd[i].fileName, "ROMDIR",    6)==0)iROMDIR=i;
	for (i=0; rd[i].fileName[0]; i++)if (strncmp(rd[i].fileName, "IOPBTCONF", 9)==0)iIOPBTCONF=i;
	
	for (i=0; rd[i].fileName[0]; i++)if (rd[i].fileName[0]=='-')break;				iBLANK=i;
	rd[iBLANK].fileSize-=DIRENTRY_SIZE+DIRENTRY_SIZE;
	p=(char*)PS2MEM_ROM;for (i=0; i<iBLANK; i++)p+=(rd[i].fileSize+0xF)&(~0xF);p+=DIRENTRY_SIZE;

	// fixme - brevity, yes, but at the expense of readability?
	q=(char*)PS2MEM_ROM;for (i=0; i<=iIOPBTCONF; i++)	q+=(rd[i].fileSize+0xF)&(~0xF);
	while (p-16>q){*((u64*)p)=*((u64*)p-4);*((u64*)p+1)=*((u64*)p-3);p-=DIRENTRY_SIZE;}
	*((u64*)p)=*((u64*)p+1)=0;p-=DIRENTRY_SIZE;rd[iIOPBTCONF].fileSize+=DIRENTRY_SIZE;
	
	q=(char*)PS2MEM_ROM;for (i=0; i<=iROMDIR; i++)	q+=(rd[i].fileSize+0xF)&(~0xF);
	while (p   >q){*((u64*)p)=*((u64*)p-2);*((u64*)p+1)=*((u64*)p-1);p-=DIRENTRY_SIZE;}
	*((u64*)p)=*((u64*)p+1)=0;p-=DIRENTRY_SIZE;rd[iROMDIR].fileSize+=DIRENTRY_SIZE;
	
	//phase 3: add the name to the end of IOPBTCONF
	p=(char*)PS2MEM_ROM;for (i=0; i<iIOPBTCONF; i++)	p+=(rd[i].fileSize+0xF)&(~0xF);while(*p) p++;//go to end of file
	strcpy(p, name);p[strlen(name)]=0xA;

	//phase 4: find file
	strcpy(path, Config.BiosDir);
	strcat(path, filename);

	if (stat(path, &buf) == -1){
		SysMessage(_("Unable to hack in %s%s\n"), Config.BiosDir, filename);
		return;
	}

	//phase 5: add the file to the end of the bios
	p=(char*)PS2MEM_ROM;for (i=0; rd[i].fileName[0]; i++)p+=(rd[i].fileSize+0xF)&(~0xF);

	fp=fopen(path, "rb");
	fseek(fp, 0, SEEK_END);
	filesize=ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(p, 1, filesize, fp);
	fclose(fp);

	//phase 6: register it in ROMDIR
	memset(rd[i].fileName, 0, 10);
	memcpy(rd[i].fileName, name, strlen(name));
	rd[i].fileSize=filesize;
	rd[i].extInfoSize=0;
}

// [TODO] I'd like to move the following functions to their own module eventually.
// It might even be a good idea to just go ahead and move them into Win32/Linux
// specific files since they're all #ifdef'd that way anyways.

static LARGE_INTEGER lfreq;

void InitCPUTicks()
{
#ifdef _WIN32
    QueryPerformanceFrequency(&lfreq);
#endif
}

u64 GetTickFrequency()
{
#ifdef _WIN32
	return lfreq.QuadPart;
#else
    return 1000000;		// unix measures in microseconds
#endif
}

u64 GetCPUTicks()
{
#ifdef _WIN32
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
#else
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((u64)t.tv_sec*GetTickFrequency())+t.tv_usec;
#endif
}

__forceinline void _TIMESLICE()
{
#ifdef _WIN32
	    Sleep(0);
#else
	    usleep(500);
#endif
}
