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

GtkWidget *Window;
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

#define is_checked(main_widget, widget_name) (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(main_widget, widget_name)))) 
#define set_checked(main_widget,widget_name, state) gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(main_widget, widget_name)), state)

#define set_flag(v, flag, value) if (value == TRUE) v |= flag; else v &= flag;
#define get_flag(v,flag) (v & flag)

#define FLAG_SLOW_DVD 0x1
#define FLAG_VU_CLIP 0x2
#define FLAG_FPU_CLAMP 0x4
#define FLAG_VU_BRANCH 0x8

#define FLAG_VU_NO_OVERFLOW 0x2
#define FLAG_VU_EXTRA_OVERFLOW 0x40
	
#define FLAG_FPU_NO_OVERFLOW 0x800
#define FLAG_FPU_EXTRA_OVERFLOW 0x1000
	
#define FLAG_EE_2_SYNC 0x1
#define FLAG_TIGHT_SPU_SYNC 0x4
#define FLAG_NO_UNDERFLOW 0x8
#define FLAG_IOP_2_SYNC 0x10
#define FLAG_TRIPLE_SYNC 0x20
#define FLAG_FAST_BRANCHES 0x80
#define FLAG_NO_VU_FLAGS 0x100
#define FLAG_NO_FPU_FLAGS 0x200
#define FLAG_ESC 0x400

#define FLAG_ROUND_NEAR 0x0000
#define FLAG_ROUND_NEGATIVE 0x2000
#define FLAG_ROUND_POSITIVE 0x4000
#define FLAG_ROUND_ZERO 0x6000

#define FLAG_FLUSH_ZERO 0x8000
#define FLAG_DENORMAL_ZERO 0x0040
	
#endif

