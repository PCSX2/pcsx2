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
 
 #ifndef __GTKGUI_H__
#define __GTKGUI_H__

#include "support.h"
#include "callbacks.h"
#include "interface.h"
#include "Linux.h"

bool applychanges = FALSE;
bool configuringplug = FALSE;
bool destroy = FALSE;
bool UseGui = TRUE;
bool needReset = TRUE;
bool RunExe = FALSE;
int DebugMode; // 0 - EE | 1 - IOP

static u32 dPC, dBPA = -1, dBPC = -1;
static char nullAddr[256];

int efile = 0;
char elfname[256];
int Slots[5] = { -1, -1, -1, -1, -1 };

GtkWidget *CpuDlg;

_PS2EgetLibType		PS2EgetLibType = NULL;
_PS2EgetLibVersion2	PS2EgetLibVersion2 = NULL;
_PS2EgetLibName		PS2EgetLibName = NULL;

// Helper Functions
void FindPlugins();

// Functions Callbacks
void OnFile_LoadElf(GtkMenuItem *menuitem, gpointer user_data);
void OnFile_Exit(GtkMenuItem *menuitem, gpointer user_data);
void OnEmu_Run(GtkMenuItem *menuitem, gpointer user_data);
void OnEmu_Reset(GtkMenuItem *menuitem, gpointer user_data);
void OnEmu_Arguments(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Gs(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Pads(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Cpu(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Conf(GtkMenuItem *menuitem, gpointer user_data);
void OnLanguage(GtkMenuItem *menuitem, gpointer user_data);
void OnHelp_Help();
void OnHelp_About(GtkMenuItem *menuitem, gpointer user_data);

GtkWidget *MainWindow;
GtkWidget *pStatusBar = NULL, *Status_Box;
GtkWidget *CmdLine;	//2002-09-28 (Florin)
GtkWidget *widgetCmdLine;
GtkWidget *ConfDlg;
GtkWidget *DebugWnd;
GtkWidget *LogDlg;
GtkWidget *FileSel;
GtkWidget *AboutDlg, *about_version , *about_authors, *about_greets;
	
void init_widgets();

GtkAccelGroup *AccelGroup;

typedef struct {
	GtkWidget *Combo;
	GList *PluginNameList;
	char plist[255][255];
	int plugins;
} PluginConf;

PluginConf GSConfS;
PluginConf PAD1ConfS;
PluginConf PAD2ConfS;
PluginConf SPU2ConfS;
PluginConf CDVDConfS;
PluginConf DEV9ConfS;
PluginConf USBConfS;
PluginConf FWConfS;
PluginConf BiosConfS;

GtkWidget *CmdLine;
GtkWidget *ListDV;
GtkListStore *ListDVModel;
GtkWidget *SetPCDlg, *SetPCEntry;
GtkWidget *SetBPADlg, *SetBPAEntry;
GtkWidget *SetBPCDlg, *SetBPCEntry;
GtkWidget *DumpCDlg, *DumpCTEntry, *DumpCFEntry;
GtkWidget *DumpRDlg, *DumpRTEntry, *DumpRFEntry;
GtkWidget *MemWriteDlg, *MemEntry, *DataEntry;
GtkAdjustment *DebugAdj;
GtkWidget *GameFixDlg, *SpeedHacksDlg, *AdvDlg;
	
#endif

