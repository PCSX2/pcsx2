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

#ifdef _WIN32
#include <windows.h>
#include "RDebug/deci2.h"
#else
#include <sys/time.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <sys/stat.h>
#include <ctype.h>

#include "Common.h"
#include "PsxCommon.h"
#include "CDVDisodrv.h"
#include "VUmicro.h"

#include "VU.h"
#include "iCore.h"
#include "iVUzerorec.h"

#include "GS.h"
#include "COP0.h"
#include "Cache.h"

#include "Paths.h"

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
		Console::Error("Boot Error > SYSTEM.CNF not found");
		return 0;//could not find; not a PS/PS2 cdvd
	}
	
	f=CDVDFS_open("SYSTEM.CNF;1", 1);
	CDVDFS_read(f, buffer, g_MaxPath);
	CDVDFS_close(f);
	
	buffer[tocEntry.fileSize]='\0';
	
	pos=strstr(buffer, "BOOT2");
	if (pos==NULL){
		pos=strstr(buffer, "BOOT");
		if (pos==NULL) {
			Console::Error("Boot Error > This is not a PS2 game!");
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

		Console::WriteLn("Loading System.map...");
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

extern u32 dumplog;

#ifdef PCSX2_DEVBUILD

void SaveGSState(const char *file)
{
	if( g_SaveGSStream ) return;

	Console::Write( "SaveGSState: " ); Console::WriteLn( file );
	g_fGSSave = new gzSavingState( file );
	
	g_SaveGSStream = 1;
	g_nLeftGSFrames = 2;

	g_fGSSave->Freeze( g_nLeftGSFrames );
}

extern uptr pDsp;
void LoadGSState(const char *file)
{
	int ret;
	gzLoadingState* f;

	Console::Write( "LoadGSState: " );

	try
	{
		f = new gzLoadingState( file );
		Console::WriteLn( file );
	}
	catch( Exception::FileNotFound& )
	{
		// file not found? try prefixing with sstates folder:
		if( !isPathRooted( file ) )
		{
			char strfile[g_MaxPath];
			CombinePaths( strfile, SSTATES_DIR, file );
			f = new gzLoadingState( file );
			Console::WriteLn( strfile );

			// If this load attempt fails, then let the exception bubble up to
			// the caller to deal with...
		}
	}

	// Always set gsIrq callback -- GS States are always exclusionary of MTGS mode
	GSirqCallback( gsIrq );

	ret = GSopen(&pDsp, "PCSX2", 0);
	if (ret != 0)
	{
		delete f;
		throw Exception::PluginFailure( "GS", "Error opening" );
	}

	ret = PAD1open((void *)&pDsp);

	f->Freeze(g_nLeftGSFrames);
	f->gsFreeze();

	f->FreezePlugin( "GS", GSfreeze );

	RunGSState( *f );

	delete( f );

	GSclose();
	PAD1close();
}

#endif

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

static void GetGSStateFilename( char* dest )
{
	char gsText[72];
	sprintf( gsText, "/%8.8X.%d.gs", ElfCRC, StatesC);
	CombinePaths( dest, SSTATES_DIR, gsText );
}

void ProcessFKeys(int fkey, int shift)
{
    char Text[g_MaxPath];

    assert(fkey >= 1 && fkey <= 12 );

    switch(fkey) {
        case 1:
			try
			{
				SaveState::GetFilename( Text, StatesC );
				gzSavingState(Text).FreezeAll();
			}
			catch( std::exception& ex )
			{
				// 99% of the time this is a file permission error and the
				// cpu state is intact so just display a passive msg to console.

				Console::Error( _( "Error > Could not save state to slot %d" ), StatesC );
				Console::Error( ex.what() );
			}
			break;

		case 2:
			if( shift )
				StatesC = (StatesC+NUM_STATES-1) % NUM_STATES;
			else
				StatesC = (StatesC+1) % NUM_STATES;

			Console::Notice( _( " > Selected savestate slot %d" ), StatesC+1);

			if( GSchangeSaveState != NULL ) {
				SaveState::GetFilename(Text, StatesC);
				GSchangeSaveState(StatesC, Text);
			}
			break;

		case 3:	
			try
			{
				SaveState::GetFilename( Text, StatesC );
				gzLoadingState joe(Text);	// throws exception on version mismatch
				cpuReset();
				joe.FreezeAll();
			}
			catch( Exception::UnsupportedStateVersion& )
			{
				// At this point the cpu hasn't been reset, so we can return
				// control to the user safely...
			}
			catch( Exception::FileNotFound& )
			{
				Console::Notice( _("Saveslot %d cannot be loaded; slot does not exist (file not found)"), StatesC );
			}
			catch( std::runtime_error& ex )
			{
				// This is the bad one.  Chances are the cpu has been reset, so emulation has
				// to be aborted.  Sorry user!  We'll give you some info for your trouble:

				Console::Error( _("An error occured while trying to load saveslot %d"), StatesC );
				Console::Error( ex.what() );
				SysMessage(
					"Pcsx2 encountered an error while trying to load the savestate\n"
					"and emulation had to be aborted." );

				cpuShutdown();
				ClosePlugins();

				throw Exception::CpuStateShutdown(
					"Saveslot load failed; PS2 emulated state had to be shut down." );	// let the GUI handle the error "gracefully"
			}
			break;

		case 4:
		{
			const char* limitMsg;
			u32 newOptions;
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
						Console::Notice("Notice: GS Plugin does not support frameskipping.");
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
			Threading::AtomicExchange( Config.Options, newOptions );

			Console::Notice("Frame Limit Mode Changed: %s", limitMsg );

			// [Air]: Do we really want to save runtime changes to frameskipping?
			//SaveConfig();
		}
			break;
		// note: VK_F5-VK_F7 are reserved for GS
		case 8:
			GSmakeSnapshot("snap/");
			break;

#ifdef PCSX2_DEVBUILD
		case 10:
		{
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
			break;
		}
		
		case 11:
			if( mtgsThread != NULL ) {
				Console::Notice( "Cannot make gsstates in MTGS mode" );
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
			    iDumpRegisters(cpuRegs.pc, 0);
				Console::Notice("hardware registers dumped EE:%x, IOP:%x\n", cpuRegs.pc, psxRegs.pc);
#endif
            }
            else {
                g_Pcsx2Recording ^= 1;
                if( mtgsThread != NULL ) {
					mtgsThread->SendSimplePacket(GS_RINGTYPE_RECORD, g_Pcsx2Recording, 0, 0);
                }
                else {
                    if( GSsetupRecording != NULL ) GSsetupRecording(g_Pcsx2Recording, NULL);
                    if( SPU2setupRecording != NULL ) SPU2setupRecording(g_Pcsx2Recording, NULL);  
                }
            }
			break;
    }
}

void injectIRX(const char *filename){
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

MemoryAlloc::MemoryAlloc() : 
  m_ptr( NULL )
, m_alloc( 0 )
, ChunkSize( DefaultChunkSize )
{
}

MemoryAlloc::MemoryAlloc( int initialSize ) : 
  m_ptr( (u8*)malloc( initialSize ) )
, m_alloc( initialSize )
, ChunkSize( DefaultChunkSize )
{
	if( m_ptr == NULL )
		throw std::bad_alloc();
}

MemoryAlloc::~MemoryAlloc()
{
	safe_free( m_ptr );
}

void MemoryAlloc::MakeRoomFor( int blockSize )
{
	if( blockSize > m_alloc )
	{
		m_alloc += blockSize + ChunkSize;
		m_ptr = (u8*)realloc( m_ptr, m_alloc );
	}
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
