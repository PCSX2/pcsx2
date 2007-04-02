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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>

#include "Common.h"
#include "PsxCommon.h"
#include "Linux.h"


static char Pcsx2Help[] = {
	"Pcsx2 " PCSX2_VERSION "\n"
	" pcsx2 [options] [file]\n"
	"\toptions:\n"
	"\t-nogui\t\tDon't open GtkGui\n"
	"\t-cfg FILE\tLoads desired configuration file (def:Pcsx2.cfg)\n"
	"\t-psxout\t\tEnable psx output\n"
	"\t-h -help\tThis help\n"
	"\tfile\t\tLoads file\n"
};

int UseGui = 1;
int needReset = 1;


int main(int argc, char *argv[]) {
	char *file = NULL;
	char elfname[256];
	char *lang;
	int runcd=0;
	int efile;
	int i;

#ifdef ENABLE_NLS
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, "Langs");
	textdomain(PACKAGE);
#endif

	strcpy(cfgfile, "Pcsx2.cfg");

	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-nogui")) UseGui = 0;
		else if (!strcmp(argv[i], "-runcd")) runcd = 1;
		else if (!strcmp(argv[i], "-psxout")) Config.PsxOut = 1;
		else if (!strcmp(argv[i], "-cfg")) strcpy(cfgfile, argv[++i]);
		else if (!strcmp(argv[i], "-h") ||
			 !strcmp(argv[i], "-help")) { printf ("%s\n", Pcsx2Help); return 0; }
		else file = argv[i];
	}

	if (UseGui) gtk_init(NULL, NULL);

#ifdef EMU_LOG
#ifndef LOG_STDOUT
	mkdir("logs", 0755);
	emuLog = fopen("logs/emuLog.txt","wb");
#else
	emuLog = stdout;
#endif
	if (emuLog == NULL) {
		SysMessage("Can't open emuLog.txt");
		return -1;
	}
	setvbuf(emuLog, NULL, _IONBF, 0);
#endif

	if (LoadConfig() == -1) {
		memset(&Config, 0, sizeof(Config));
		strcpy(Config.BiosDir,    "Bios/");
		strcpy(Config.PluginsDir, "Plugins/");
		Config.Cpu = 1;

		SysMessage(_("Pcsx2 needs to be configured"));
		Pcsx2Configure();

		return 0;
	}
	if (Config.Lang[0] == 0) {
		strcpy(Config.Lang, "en");
	}

	langs = (_langs*)malloc(sizeof(_langs));
	strcpy(langs[0].lang, "en");
	InitLanguages(); i=1;
	while ((lang = GetLanguageNext()) != NULL) {
		langs = (_langs*)realloc(langs, sizeof(_langs)*(i+1));
		strcpy(langs[i].lang, lang);
		i++;
	}
	CloseLanguages();
	langsMax = i;

	if (SysInit() == -1) return 1;

	if (UseGui) {
		StartGui();
		return 0;
	}

	if (OpenPlugins() == -1) {
		return 1;
	}
	SysReset();

	cpuExecuteBios();
	if (file) strcpy(elfname, file);
	if (runcd)
		efile=GetPS2ElfName(elfname);
	if (efile)
		loadElfFile(elfname);

	Cpu->Execute();

	return 0;
}

DIR *dir;

void InitLanguages() {
	dir = opendir("Langs");
}

char *GetLanguageNext() {
	struct dirent *ent;

	if (dir == NULL) return NULL;
	for (;;) {
		ent = readdir(dir);
		if (ent == NULL) return NULL;

		if (!strcmp(ent->d_name, ".")) continue;
		if (!strcmp(ent->d_name, "..")) continue;
		break;
	}

	return ent->d_name;
}

void CloseLanguages() {
	if (dir) closedir(dir);
}

void ChangeLanguage(char *lang) {
	strcpy(Config.Lang, lang);
	SaveConfig();
	LoadConfig();
}

int StatesC = 0;

void KeyEvent(keyEvent* ev) {
	char Text[256];
	int ret;

	if (ev == NULL) return;
	if (ev->event == KEYRELEASE) {
		GSkeyEvent(ev); return;
	}
	if (ev->event != KEYPRESS) return;
	switch (ev->key) {
		case XK_F1:
			sprintf(Text, "sstates/%8.8X.%3.3d", ElfCRC, StatesC);
			ret = SaveState(Text);
/*			if (ret == 0)
				 sprintf(Text, _("*PCSX*: Saved State %d"), StatesC+1);
			else sprintf(Text, _("*PCSX*: Error Saving State %d"), StatesC+1);
			GPU_displayText(Text);
			if (ShowPic) { ShowPic = 0; gpuShowPic(); }*/
			break;
		case XK_F2:
			if (StatesC < 4) StatesC++;
			else StatesC = 0;
			SysPrintf("*PCSX2*: Selected State %ld\n", StatesC);
/*			GPU_freeze(2, (GPUFreeze_t *)&StatesC);
			if (ShowPic) { ShowPic = 0; gpuShowPic(); }*/
			break;
		case XK_F3:			
			sprintf (Text, "sstates/%8.8X.%3.3d", ElfCRC, StatesC);
			ret = LoadState(Text);
/*			if (ret == 0)
				 sprintf(Text, _("*PCSX*: Loaded State %d"), StatesC+1);
			else sprintf(Text, _("*PCSX*: Error Loading State %d"), StatesC+1);
			GPU_displayText(Text);*/
			break;
		case XK_F5:
//			if (sio2.hackedRecv==0x1D100) sio2.hackedRecv=0x1100;
//			else sio2.hackedRecv=0x1D100;
//			SysPrintf("hackedRecv : %x\n", sio2.hackedRecv);
			break;

		case XK_F8:
			GSmakeSnapshot("snap/");
			break;
#ifdef CPU_LOG
		case XK_F9:
			Log = 1 - Log;
			SysPrintf("Log : %d\n", Log);
			break;
		case XK_F10:
/*			if (varLog & 0x40000000) varLog&=~0x40000000;
			else varLog|= 0x40000000;
			SysPrintf("varLog %x\n", varLog);*/
			SysPrintf("hack\n");
			psxSu32(0x30) = 0x20000;
//			psMu32(0x8010c904) = 0;
			break;
#endif
		case XK_F11:
			SysPrintf("Open\n");
			cdCaseopen = 1;
			break;
		case XK_F12:
			SysPrintf("Close\n");
			cdCaseopen = 0;
			break;
		case XK_Escape:
			signal(SIGINT, SIG_DFL);
			signal(SIGPIPE, SIG_DFL);
			ClosePlugins();
			if (!UseGui) exit(0);
			RunGui();
			break;
		default:
			GSkeyEvent(ev);
			break;
	}
}

int SysInit() {
	mkdir("sstates", 0755);
	mkdir("memcards", 0755);

	cpuInit();
	while (LoadPlugins() == -1) {
		if (Pcsx2Configure() == 0) exit(1);
	}

	return 0;
}

void SysReset() {
	cpuReset();
}

void SysClose() {
	cpuShutdown();
	ReleasePlugins();

	if (emuLog != NULL) fclose(emuLog);
}

void SysPrintf(char *fmt, ...) {
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (Config.PsxOut) { printf("%s", msg); fflush(stdout); }
#ifdef EMU_LOG
	fprintf(emuLog, "%s", msg);
#endif
}

void *SysLoadLibrary(char *lib) {
	return dlopen(lib, RTLD_NOW);
}

void *SysLoadSym(void *lib, char *sym) {
	return dlsym(lib, sym);
}

char *SysLibError() {
	return dlerror();
}

void SysCloseLibrary(void *lib) {
	dlclose(lib);
}

void SysUpdate() {
	KeyEvent(PAD1keyEvent());
	KeyEvent(PAD1keyEvent());
}

void SysRunGui() {
	RunGui();
}

void *SysMmap(uptr base, u32 size) {
	return mmap((uptr*)base, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
}

void SysMunmap(uptr base, u32 size) {
	munmap((uptr*)base, size);
}

