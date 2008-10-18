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

#ifndef __LINUX_H__
#define __LINUX_H__

#include <stdio.h> 
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#include <sys/time.h>
#include <pthread.h> 

#include <X11/keysym.h>
#include <gtk/gtk.h>
#include <glib/gthread.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkdialog.h>


#include "../Paths.h"
#include "Common.h"

extern bool UseGui;
extern bool needReset;
extern bool RunExe;

extern GtkWidget *CpuDlg;

/* Config.c */
extern int LoadConfig();
extern void SaveConfig();

/* GtkGui */
extern void StartGui();
extern void RunGui();
extern int Pcsx2Configure();
extern void ChangeLanguage(char *lang);
extern void init_widgets();

/* LnxMain */
extern void InitLanguages();
extern char *GetLanguageNext();
extern void CloseLanguages();
extern void ChangeLanguage(char *lang);

/* Misc.c */
extern void vu0Shutdown();
extern void vu1Shutdown();
extern void SaveConfig();

typedef struct {
	char lang[256];
} _langs;

typedef enum
{
	GS,
	PAD1,
	PAD2,
	SPU,
	CDVD,
	DEV9,
	USB,
	FW,
	BIOS
} plugin_types;

GtkWidget *check_eerec, *check_vu0rec, *check_vu1rec;
GtkWidget *check_mtgs , *check_cpu_dc;
GtkWidget *check_console , *check_patches;
GtkWidget *radio_normal_limit, *radio_limit_limit, *radio_fs_limit, *radio_vuskip_limit;

_langs *langs;
unsigned int langsMax;

extern int g_SaveGSStream;
extern int g_ZeroGSOptions;

char cfgfile[256];


#endif /* __LINUX_H__ */
