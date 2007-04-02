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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Linux.h"

#define GetValue(name, var) \
	tmp = strstr(data, name); \
	if (tmp != NULL) { \
		tmp+=strlen(name); \
		while ((*tmp == ' ') || (*tmp == '=')) tmp++; \
		if (*tmp != '\n') sscanf(tmp, "%s", var); \
	}

#define GetValuel(name, var) \
	tmp = strstr(data, name); \
	if (tmp != NULL) { \
		tmp+=strlen(name); \
		while ((*tmp == ' ') || (*tmp == '=')) tmp++; \
		if (*tmp != '\n') sscanf(tmp, "%x", &var); \
	}

#define SetValue(name, var) \
	fprintf (f,"%s = %s\n", name, var);

#define SetValuel(name, var) \
	fprintf (f,"%s = %x\n", name, var);

int LoadConfig(PcsxConfig *Conf) {
	struct stat buf;
	FILE *f;
	int size;
	char *data,*tmp;

	if (stat(cfgfile, &buf) == -1) return -1;
	size = buf.st_size;

	f = fopen(cfgfile,"r");
	if (f == NULL) return -1;

	data = (char*)malloc(size);
	if (data == NULL) return -1;

	fread(data, 1, size, f);
	fclose(f);

	GetValue("Bios", Config.Bios);
	GetValue("GS",   Config.GS);
	GetValue("PAD1", Config.PAD1);
	GetValue("PAD2", Config.PAD2);
	GetValue("SPU2", Config.SPU2);
	GetValue("CDVD", Config.CDVD);
	GetValue("DEV9", Config.DEV9);
	GetValue("USB",  Config.USB);
	GetValue("FW",  Config.FW);
	GetValue("Mcd1", Config.Mcd1);
	GetValue("Mcd2", Config.Mcd2);
	GetValue("PluginsDir", Config.PluginsDir);
	GetValue("BiosDir",    Config.BiosDir);
	GetValuel("Cpu",        Config.Cpu);
	GetValuel("PsxOut",     Config.PsxOut);
	GetValuel("RegCaching", Config.Regcaching);
	GetValuel("Patch",      Config.Patch);
	GetValuel("VUrec",      Config.VUrec);
// 	GetValuel("PadHack",    Config.PadHack);
	GetValuel("varLog", varLog);
	Config.Lang[0] = 0;
	GetValue("Lang", Config.Lang);

	free(data);

#ifdef ENABLE_NLS
	if (Config.Lang[0]) {
		extern int _nl_msg_cat_cntr;

		setenv("LANGUAGE", Config.Lang, 1);
		++_nl_msg_cat_cntr;
	}
#endif

	return 0;
}

/////////////////////////////////////////////////////////

void SaveConfig() {
	FILE *f;

	f = fopen(cfgfile,"w");
	if (f == NULL) return;

	SetValue("Bios", Config.Bios);
	SetValue("GS",   Config.GS);
	SetValue("PAD1", Config.PAD1);
	SetValue("PAD2", Config.PAD2);
	SetValue("SPU2", Config.SPU2);
	SetValue("CDVD", Config.CDVD);
	SetValue("DEV9", Config.DEV9);
	SetValue("USB",  Config.USB);
	SetValue("FW",  Config.FW);
	SetValue("Mcd1", Config.Mcd1);
	SetValue("Mcd2", Config.Mcd2);
	SetValue("PluginsDir", Config.PluginsDir);
	SetValue("BiosDir",    Config.BiosDir);
	SetValuel("Cpu",        Config.Cpu);
	SetValuel("PsxOut",     Config.PsxOut);
	SetValuel("RegCaching", Config.Regcaching);
	SetValuel("Patch",      Config.Patch);
	SetValuel("VUrec",      Config.VUrec);
// 	SetValuel("PadHack",    Config.PadHack);
	SetValuel("varLog", varLog);
	SetValue("Lang",    Config.Lang);

	fclose(f);

	return;
}
