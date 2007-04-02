#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <sys/time.h>

#include "Linux.h"
#include "GladeGui.h"
#include "GladeFuncs.h"

static int needreset = 1;
int confret;
int confplug=0;

_PS2EgetLibType		PS2EgetLibType = NULL;
_PS2EgetLibVersion2	PS2EgetLibVersion2 = NULL;
_PS2EgetLibName		PS2EgetLibName = NULL;

// Helper Functions
void FindPlugins();

// Functions Callbacks
void OnFile_LoadElf();
void OnFile_Exit();
void OnEmu_Run();
void OnEmu_Reset();
void OnEmu_Arguments();
void OnConf_Gs();
void OnConf_Pads();
void OnConf_Cpu();
void OnConf_Conf();
void OnLanguage(GtkMenuItem *menuitem, gpointer user_data);
void OnHelp_Help();
void OnHelp_About();

GtkWidget *Window;
GtkWidget *CmdLine;	//2002-09-28 (Florin)
GtkWidget *ConfDlg;
GtkWidget *AboutDlg;
GtkWidget *DebugWnd;
GtkWidget *LogDlg;
GtkWidget *FileSel;

GtkAccelGroup *AccelGroup;

typedef struct {
	GtkWidget *Combo;
	GList *glist;
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

void StartGui() {
	GtkWidget *Menu;
	GtkWidget *Item;
	int i;

	Window = create_MainWindow();
	gtk_window_set_title(GTK_WINDOW(Window), "PCSX2 v" PCSX2_VERSION);

#ifndef NEW_LOGGING
	Item = lookup_widget(Window, "GtkMenuItem_Logging");
	gtk_widget_set_sensitive(Item, FALSE);
#endif

	Item = lookup_widget(Window, "GtkMenuItem_Language");
	Menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(Item), Menu);

	for (i=0; i<langsMax; i++) {
		Item = gtk_check_menu_item_new_with_label(ParseLang(langs[i].lang));
		gtk_widget_show(Item);
		gtk_container_add(GTK_CONTAINER(Menu), Item);
		gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(Item), TRUE);
		if (!strcmp(Config.Lang, langs[i].lang))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item), TRUE);

		gtk_signal_connect(GTK_OBJECT(Item), "activate",
                           GTK_SIGNAL_FUNC(OnLanguage),
                           (gpointer)i);
	}

	gtk_widget_show_all(Window);
	gtk_main();
}

void RunGui() {
	StartGui();
}

int destroy=0;

void OnDestroy() {
	if (!destroy) OnFile_Exit();
}

int Pcsx2Configure() {
	if (!UseGui) return 0;
	confplug = 1;
	Window = NULL;
	OnConf_Conf();
	confplug = 0;
	return confret;
}

int efile=0;
char elfname[256];

void OnLanguage(GtkMenuItem *menuitem, gpointer user_data) {
	ChangeLanguage(langs[(int)user_data].lang);
	destroy=1;
	gtk_widget_destroy(Window);
	destroy=0;
	gtk_main_quit();
	while (gtk_events_pending()) gtk_main_iteration();
	StartGui();
}

void SignalExit(int sig) {
	ClosePlugins();
	OnFile_Exit();
}

void RunExecute(int run) {
	destroy=1;
	gtk_widget_destroy(Window);
	destroy=0;
	gtk_main_quit();
	while (gtk_events_pending()) gtk_main_iteration();

	if (OpenPlugins() == -1) {
		RunGui(); return;
	}
	signal(SIGINT, SignalExit);
	signal(SIGPIPE, SignalExit);

	if (needreset) { 
		SysReset();

		cpuExecuteBios();
		if (efile == 2)
			efile=GetPS2ElfName(elfname);
		if (efile)
			loadElfFile(elfname);
		efile=0;
		needreset = 0;
	}
	if (run) Cpu->Execute();
}

void OnFile_RunCD() {
	efile = 2;
	RunExecute(1);
}

void OnRunElf_Ok() {
	gchar *File;

	File = gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(elfname, File);
	gtk_widget_destroy(FileSel);
	needreset = 1;
	efile = 1;
	RunExecute(1);
}

void OnRunElf_Cancel() {
	gtk_widget_destroy(FileSel);
}

void OnFile_LoadElf() {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select Psx Elf File"));

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnRunElf_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnRunElf_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
}

void OnFile_Exit() {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	char plugin[256];

	// with this the problem with plugins that are linked with the pthread
	// library is solved

	dir = opendir(Config.PluginsDir);
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			sprintf (plugin, "%s%s", Config.PluginsDir, ent->d_name);

			if (strstr(plugin, ".so") == NULL) continue;
			Handle = dlopen(plugin, RTLD_NOW);
			if (Handle == NULL) continue;
		}
	}

	printf(_("PCSX2 Quitting\n"));
	if (UseGui) gtk_main_quit();
	SysClose();
	if (UseGui) gtk_exit(0);
	else exit(0);
}

void OnEmu_Run() {
	RunExecute(1);
}

void OnEmu_Reset() {
	needreset = 1;
}

int Slots[5] = { -1, -1, -1, -1, -1 };

void ResetMenuSlots() {
	GtkWidget *Item;
	char str[256];
	int i;

	for (i=0; i<5; i++) {
		sprintf(str, "GtkMenuItem_LoadSlot%d", i+1);
		Item = lookup_widget(Window, str);
		if (Slots[i] == -1) 
			gtk_widget_set_sensitive(Item, FALSE);
		else
			gtk_widget_set_sensitive(Item, TRUE);
	}
}

void UpdateMenuSlots() {
	char str[256];
	int i;

	for (i=0; i<5; i++) {
		sprintf(str, "sstates/%8.8X.%3.3d", ElfCRC, i);
		Slots[i] = CheckState(str);
	}
}

void States_Load(int num) {
	char Text[256];
	int ret;

	efile = 2;
	RunExecute(0);

	sprintf (Text, "sstates/%8.8X.%3.3d", ElfCRC, num);
	ret = LoadState(Text);
/*	if (ret == 0)
		 sprintf (Text, _("*PCSX2*: Loaded State %d"), num+1);
	else sprintf (Text, _("*PCSX2*: Error Loading State %d"), num+1);
	GPU_displayText(Text);*/

	Cpu->Execute();
}

void States_Save(int num) {
	char Text[256];
	int ret;

	RunExecute(0);

	sprintf (Text, "sstates/%8.8X.%3.3d", ElfCRC, num);
	ret = SaveState(Text);
/*	if (ret == 0)
		 sprintf (Text, _("*PCSX*: Saved State %d"), num+1);
	else sprintf (Text, _("*PCSX*: Error Saving State %d"), num+1);
	GPU_displayText(Text);*/

	Cpu->Execute();
}

void OnStates_Load1() { States_Load(0); } 
void OnStates_Load2() { States_Load(1); } 
void OnStates_Load3() { States_Load(2); } 
void OnStates_Load4() { States_Load(3); } 
void OnStates_Load5() { States_Load(4); } 

void OnLoadOther_Ok() {
	gchar *File;
	char str[256];
	char Text[256];
	int ret;

	File = gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(str, File);
	gtk_widget_destroy(FileSel);

	efile = 2;
	RunExecute(0);

	ret = LoadState(str);
/*	if (ret == 0)
		 sprintf (Text, _("*PCSX*: Loaded State %s"), str);
	else sprintf (Text, _("*PCSX*: Error Loading State %s"), str);
	GPU_displayText(Text);*/

	Cpu->Execute();
}

void OnLoadOther_Cancel() {
	gtk_widget_destroy(FileSel);
}

void OnStates_LoadOther() {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select State File"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(FileSel), "sstates/");

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnLoadOther_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnLoadOther_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
} 

void OnStates_Save1() { States_Save(0); } 
void OnStates_Save2() { States_Save(1); } 
void OnStates_Save3() { States_Save(2); } 
void OnStates_Save4() { States_Save(3); } 
void OnStates_Save5() { States_Save(4); } 

void OnSaveOther_Ok() {
	gchar *File;
	char str[256];
	char Text[256];
	int ret;

	File = gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(str, File);
	gtk_widget_destroy(FileSel);
	RunExecute(0);

	ret = SaveState(str);
/*	if (ret == 0)
		 sprintf (Text, _("*PCSX*: Saved State %s"), str);
	else sprintf (Text, _("*PCSX*: Error Saving State %s"), str);
	GPU_displayText(Text);*/

	Cpu->Execute();
}

void OnSaveOther_Cancel() {
	gtk_widget_destroy(FileSel);
}

void OnStates_SaveOther() {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select State File"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(FileSel), "sstates/");

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnSaveOther_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnSaveOther_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
} 

//2002-09-28 (Florin)
void OnArguments_Ok() {
	GtkWidget *widgetCmdLine;
	char *str;

	widgetCmdLine = lookup_widget(CmdLine, "GtkEntry_dCMDLINE");
	str = gtk_entry_get_text(GTK_ENTRY(widgetCmdLine));
	memcpy(args, str, 256);

	gtk_widget_destroy(CmdLine);
	gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnArguments_Cancel() {
	gtk_widget_destroy(CmdLine);
	gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnEmu_Arguments() {
	GtkWidget *widgetCmdLine;

	CmdLine = create_CmdLine();
	gtk_window_set_title(GTK_WINDOW(CmdLine), _("Program arguments"));

	widgetCmdLine = lookup_widget(CmdLine, "GtkEntry_dCMDLINE");
	gtk_entry_set_text(GTK_ENTRY(widgetCmdLine), args);
						//args exported by ElfHeader.h
	gtk_widget_show_all(CmdLine);
	gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}
//-------------------

void OnConf_Gs() {
	gtk_widget_set_sensitive(Window, FALSE);
	GSconfigure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Pads() {
	gtk_widget_set_sensitive(Window, FALSE);
	PAD1configure();
	if (strcmp(Config.PAD1, Config.PAD2)) PAD2configure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Spu2() {
	gtk_widget_set_sensitive(Window, FALSE);
	SPU2configure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Cdvd() {
	gtk_widget_set_sensitive(Window, FALSE);
	CDVDconfigure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Dev9() {
	gtk_widget_set_sensitive(Window, FALSE);
	DEV9configure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Usb() {
	gtk_widget_set_sensitive(Window, FALSE);
	USBconfigure();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnConf_Fw() {
	gtk_widget_set_sensitive(Window, FALSE);
	FWconfigure();
	gtk_widget_set_sensitive(Window, TRUE);
}

GtkWidget *CpuDlg;

void OnCpu_Ok() {
	GtkWidget *Btn;
	long t;

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_RegCaching");
	Config.Regcaching = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_Patches");
	Config.Patch = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));

	t = Config.Cpu;
	Btn = lookup_widget(CpuDlg, "GtkCheckButton_Cpu");
	Config.Cpu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));
	if (t != Config.Cpu) {
		cpuRestartCPU();
	}

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_VUrec");
	Config.VUrec = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_PsxOut");
	Config.PsxOut = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));

	gtk_widget_destroy(CpuDlg);

	SaveConfig();
	if (Window) gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnCpu_Cancel() {
	gtk_widget_destroy(CpuDlg);
	if (Window) gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}


void OnConf_Cpu() {
	GtkWidget *Btn;

	CpuDlg = create_CpuDlg();
	gtk_window_set_title(GTK_WINDOW(CpuDlg), _("Configuration"));

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_Cpu");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Config.Cpu);

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_PsxOut");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Config.PsxOut);

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_RegCaching");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Config.Regcaching);

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_VUrec");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Config.VUrec);

	Btn = lookup_widget(CpuDlg, "GtkCheckButton_Patches");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Config.Patch);

#ifndef CPU_LOG
	Btn = lookup_widget(CpuDlg, "GtkCheckButton_RegCaching");
	gtk_widget_set_sensitive(Btn, FALSE);
	Btn = lookup_widget(CpuDlg, "GtkCheckButton_VUrec");
	gtk_widget_set_sensitive(Btn, FALSE);
#endif

	gtk_widget_show_all(CpuDlg);
	if (Window) gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}

#define FindComboText(combo,list,conf) \
	if (strlen(conf) > 0) { \
		int i; \
		for (i=2;i<255;i+=2) { \
			if (!strcmp(conf, list[i-2])) { \
				gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), list[i-1]); \
				break; \
			} \
		} \
	}

#define GetComboText(combo,list,conf) \
	{ \
	int i; \
	char *tmp = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(combo)->entry)); \
	for (i=2;i<255;i+=2) { \
		if (!strcmp(tmp, list[i-1])) { \
			strcpy(conf, list[i-2]); \
			break; \
		} \
	} \
	}

void OnConfConf_Ok() {
	GetComboText(GSConfS.Combo, GSConfS.plist, Config.GS)
	GetComboText(PAD1ConfS.Combo, PAD1ConfS.plist, Config.PAD1);
	GetComboText(PAD2ConfS.Combo, PAD2ConfS.plist, Config.PAD2);
	GetComboText(SPU2ConfS.Combo, SPU2ConfS.plist, Config.SPU2);
	GetComboText(CDVDConfS.Combo, CDVDConfS.plist, Config.CDVD);
	GetComboText(DEV9ConfS.Combo, DEV9ConfS.plist, Config.DEV9);
	GetComboText(USBConfS.Combo,  USBConfS.plist,  Config.USB);
	GetComboText(FWConfS.Combo,   FWConfS.plist,   Config.FW);
	GetComboText(BiosConfS.Combo, BiosConfS.plist, Config.Bios);

	SaveConfig();

	if (confplug == 0) {
		ReleasePlugins();
		LoadPlugins();
	}

	needreset = 1;
	gtk_widget_destroy(ConfDlg);
	if (Window) gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
	confret = 1;
}

void OnConfConf_Cancel() {
	gtk_widget_destroy(ConfDlg);
	if (Window) gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
	confret = 0;
}

#define ConfPlugin(src, confs, plugin, name) \
	void *drv; \
	src conf; \
	char file[256]; \
	GetComboText(confs.Combo, confs.plist, plugin); \
	strcpy(file, Config.PluginsDir); \
	strcat(file, plugin); \
	drv = SysLoadLibrary(file); \
	if (drv == NULL) return; \
	conf = (src) SysLoadSym(drv, name); \
	if (SysLibError() == NULL) conf(); \
	SysCloseLibrary(drv);

#define TestPlugin(src, confs, plugin, name) \
	void *drv; \
	src conf; \
	int ret = 0; \
	char file[256]; \
	GetComboText(confs.Combo, confs.plist, plugin); \
	strcpy(file, Config.PluginsDir); \
	strcat(file, plugin); \
	drv = SysLoadLibrary(file); \
	if (drv == NULL) return; \
	conf = (src) SysLoadSym(drv, name); \
	if (SysLibError() == NULL) { \
		ret = conf(); \
		if (ret == 0) \
			 SysMessage(_("This plugin reports that should work correctly")); \
		else SysMessage(_("This plugin reports that should not work correctly")); \
	} \
	SysCloseLibrary(drv);

void OnConfConf_GsConf() {
	ConfPlugin(_GSconfigure, GSConfS, Config.GS, "GSconfigure");
}

void OnConfConf_GsTest() {
	TestPlugin(_GStest, GSConfS, Config.GS, "GStest");
}

void OnConfConf_GsAbout() {
	ConfPlugin(_GSabout, GSConfS, Config.GS, "GSabout");
}

void OnConfConf_Pad1Conf() {
	ConfPlugin(_PADconfigure, PAD1ConfS, Config.PAD1, "PADconfigure");
}

void OnConfConf_Pad1Test() {
	TestPlugin(_PADtest, PAD1ConfS, Config.PAD1, "PADtest");
}

void OnConfConf_Pad1About() {
	ConfPlugin(_PADabout, PAD1ConfS, Config.PAD1, "PADabout");
}

void OnConfConf_Pad2Conf() {
	ConfPlugin(_PADconfigure, PAD2ConfS, Config.PAD2, "PADconfigure");
}

void OnConfConf_Pad2Test() {
	TestPlugin(_PADtest, PAD2ConfS, Config.PAD2, "PADtest");
}

void OnConfConf_Pad2About() {
	ConfPlugin(_PADabout, PAD2ConfS, Config.PAD2, "PADabout");
}

void OnConfConf_Spu2Conf() {
	ConfPlugin(_SPU2configure, SPU2ConfS, Config.SPU2, "SPU2configure");
}

void OnConfConf_Spu2Test() {
	TestPlugin(_SPU2test, SPU2ConfS, Config.SPU2, "SPU2test");
}

void OnConfConf_Spu2About() {
	ConfPlugin(_SPU2about, SPU2ConfS, Config.SPU2, "SPU2about");
}

void OnConfConf_CdvdConf() {
	ConfPlugin(_CDVDconfigure, CDVDConfS, Config.CDVD, "CDVDconfigure");
}

void OnConfConf_CdvdTest() {
	TestPlugin(_CDVDtest, CDVDConfS, Config.CDVD, "CDVDtest");
}

void OnConfConf_CdvdAbout() {
	ConfPlugin(_CDVDabout, CDVDConfS, Config.CDVD, "CDVDabout");
}

void OnConfConf_Dev9Conf() {
	ConfPlugin(_DEV9configure, DEV9ConfS, Config.DEV9, "DEV9configure");
}

void OnConfConf_Dev9Test() {
	TestPlugin(_DEV9test, DEV9ConfS, Config.DEV9, "DEV9test");
}

void OnConfConf_Dev9About() {
	ConfPlugin(_DEV9about, DEV9ConfS, Config.DEV9, "DEV9about");
}

void OnConfConf_UsbConf() {
	ConfPlugin(_USBconfigure, USBConfS, Config.USB, "USBconfigure");
}

void OnConfConf_UsbTest() {
	TestPlugin(_USBtest, USBConfS, Config.USB, "USBtest");
}

void OnConfConf_UsbAbout() {
	ConfPlugin(_USBabout, USBConfS, Config.USB, "USBabout");
}

void OnConfConf_FWConf() {
	ConfPlugin(_FWconfigure, FWConfS, Config.FW, "FWconfigure");
}

void OnConfConf_FWTest() {
	TestPlugin(_FWtest, FWConfS, Config.FW, "FWtest");
}

void OnConfConf_FWAbout() {
	ConfPlugin(_FWabout, FWConfS, Config.FW, "FWabout");
}

#define ConfCreatePConf(name, type) \
	type##ConfS.Combo = lookup_widget(ConfDlg, "GtkCombo_" name); \
    gtk_combo_set_popdown_strings(GTK_COMBO(type##ConfS.Combo), type##ConfS.glist); \
	FindComboText(type##ConfS.Combo, type##ConfS.plist, Config.type); \

void UpdateConfDlg() {
	FindPlugins();

	ConfCreatePConf("Gs", GS);
	ConfCreatePConf("Pad1", PAD1);
	ConfCreatePConf("Pad2", PAD2);
	ConfCreatePConf("Spu2", SPU2);
	ConfCreatePConf("Cdvd", CDVD);
	ConfCreatePConf("Dev9", DEV9);
	ConfCreatePConf("Usb",  USB);
	ConfCreatePConf("FW",  FW);
	ConfCreatePConf("Bios", Bios);
}

void OnPluginsPath_Ok() {
	gchar *File;

	File = gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(Config.PluginsDir, File);
	if (Config.PluginsDir[strlen(Config.PluginsDir)-1] != '/')
		strcat(Config.PluginsDir, "/");

	UpdateConfDlg();

	gtk_widget_destroy(FileSel);
}

void OnPluginsPath_Cancel() {
	gtk_widget_destroy(FileSel);
}

void OnConfConf_PluginsPath() {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select Plugins Directory"));

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnPluginsPath_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnPluginsPath_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
}

void OnBiosPath_Ok() {
	gchar *File;

	File = gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(Config.BiosDir, File);
	if (Config.BiosDir[strlen(Config.BiosDir)-1] != '/')
		strcat(Config.BiosDir, "/");

	UpdateConfDlg();

	gtk_widget_destroy(FileSel);
}

void OnBiosPath_Cancel() {
	gtk_widget_destroy(FileSel);
}

void OnConfConf_BiosPath() {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select Bios Directory"));

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnBiosPath_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnBiosPath_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
}

void OnConf_Conf() {
	FindPlugins();

	ConfDlg = create_ConfDlg();
	gtk_window_set_title(GTK_WINDOW(ConfDlg), _("Configuration"));

	UpdateConfDlg();

	gtk_widget_show_all(ConfDlg);
	if (Window) gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}

GtkWidget *CmdLine;
GtkWidget *ListDV;
GtkWidget *SetPCDlg, *SetPCEntry;
GtkWidget *SetBPADlg, *SetBPAEntry;
GtkWidget *SetBPCDlg, *SetBPCEntry;
GtkWidget *DumpCDlg, *DumpCTEntry, *DumpCFEntry;
GtkWidget *DumpRDlg, *DumpRTEntry, *DumpRFEntry;
GtkWidget *MemWriteDlg, *MemEntry, *DataEntry;
GtkAdjustment *DebugAdj;
static u32 dPC;
static u32 dBPA = -1;
static u32 dBPC = -1;
static char nullAddr[256];
int DebugMode; // 0 - EE | 1 - IOP

#include "R3000A.h"
#include "PsxMem.h"

void UpdateDebugger() {
	GtkWidget *item;
	char *str;
	int i;
	GList *list = NULL;
	u32 pc;

	DebugAdj->value = (gfloat)dPC/4;

	gtk_list_clear_items(GTK_LIST(ListDV), 0, 23);

	for (i=0; i<23; i++) {
		u32 *mem;
		pc = dPC + i*4;
		if (DebugMode) {
			mem = (u32*)PSXM(pc);
		} else
			mem = PSM(pc);
		if (mem == NULL) { sprintf(nullAddr, "%8.8lX:\tNULL MEMORY", pc); str = nullAddr; }
		else str = disR5900Fasm(*mem, pc);
		item = gtk_list_item_new_with_label(str);
		gtk_widget_show(item);
		list = g_list_append(list, item);
	}
	gtk_list_append_items(GTK_LIST(ListDV), list);
}

void OnDebug_Close() {
	ClosePlugins();
	gtk_widget_destroy(DebugWnd);
	gtk_main_quit();
	gtk_widget_set_sensitive(Window, TRUE);
}

void OnDebug_ScrollChange(GtkAdjustment *adj) {
	dPC = (u32)adj->value*4;
	dPC&= ~0x3;
	
	UpdateDebugger();
}

void OnSetPC_Ok() {
	char *str = gtk_entry_get_text(GTK_ENTRY(SetPCEntry));

	sscanf(str, "%lx", &dPC);
	dPC&= ~0x3;

	gtk_widget_destroy(SetPCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
	UpdateDebugger();
}

void OnSetPC_Cancel() {
	gtk_widget_destroy(SetPCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_SetPC() {
	SetPCDlg = create_SetPCDlg();
	
	SetPCEntry = lookup_widget(SetPCDlg, "GtkEntry_dPC");

	gtk_widget_show_all(SetPCDlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();
}

void OnSetBPA_Ok() {
	char *str = gtk_entry_get_text(GTK_ENTRY(SetBPAEntry));

	sscanf(str, "%lx", &dBPA);
	dBPA&= ~0x3;

	gtk_widget_destroy(SetBPADlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
	UpdateDebugger();
}

void OnSetBPA_Cancel() {
	gtk_widget_destroy(SetBPADlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_SetBPA() {
	SetBPADlg = create_SetBPADlg();
	
	SetBPAEntry = lookup_widget(SetBPADlg, "GtkEntry_BPA");

	gtk_widget_show_all(SetBPADlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();
}

void OnSetBPC_Ok() {
	char *str = gtk_entry_get_text(GTK_ENTRY(SetBPCEntry));

	sscanf(str, "%lx", &dBPC);

	gtk_widget_destroy(SetBPCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
	UpdateDebugger();
}

void OnSetBPC_Cancel() {
	gtk_widget_destroy(SetBPCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_SetBPC() {
	SetBPCDlg = create_SetBPCDlg();
	
	SetBPCEntry = lookup_widget(SetBPCDlg, "GtkEntry_BPC");

	gtk_widget_show_all(SetBPCDlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();
}

void OnDebug_ClearBPs() {
	dBPA = -1;
	dBPC = -1;
}

void OnDumpC_Ok() {
	FILE *f;
	char *str = gtk_entry_get_text(GTK_ENTRY(DumpCFEntry));
	u32 addrf, addrt;

	sscanf(str, "%lx", &addrf); addrf&=~0x3;
	str = gtk_entry_get_text(GTK_ENTRY(DumpCTEntry));
	sscanf(str, "%lx", &addrt); addrt&=~0x3;

	f = fopen("dump.txt", "w");
	if (f == NULL) return;

	while (addrf != addrt) {
		u32 *mem;

		if (DebugMode) {
			mem = PSXM(addrf);
		} else {
			mem = PSM(addrf);
		}
		if (mem == NULL) { sprintf(nullAddr, "%8.8lX:\tNULL MEMORY", addrf); str = nullAddr; }
		else str = disR5900Fasm(*mem, addrf);

		fprintf(f, "%s\n", str);
		addrf+= 4;
	}

	fclose(f);

	gtk_widget_destroy(DumpCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDumpC_Cancel() {
	gtk_widget_destroy(DumpCDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_DumpCode() {
	DumpCDlg = create_DumpCDlg();
	
	DumpCFEntry = lookup_widget(DumpCDlg, "GtkEntry_DumpCF");
	DumpCTEntry = lookup_widget(DumpCDlg, "GtkEntry_DumpCT");

	gtk_widget_show_all(DumpCDlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();
}

void OnDumpR_Ok() {
	FILE *f;
	char *str = gtk_entry_get_text(GTK_ENTRY(DumpRFEntry));
	u32 addrf, addrt;

	sscanf(str, "%lx", &addrf); addrf&=~0x3;
	str = gtk_entry_get_text(GTK_ENTRY(DumpRTEntry));
	sscanf(str, "%lx", &addrt); addrt&=~0x3;

	f = fopen("dump.txt", "w");
	if (f == NULL) return;

	while (addrf != addrt) {
		u32 *mem;
		u32 out;

		if (DebugMode) {
			mem = PSXM(addrf);
		} else {
			mem = PSM(addrf);
		}
		if (mem == NULL) out = 0;
		else out = *mem;

		fwrite(&out, 4, 1, f);
		addrf+= 4;
	}

	fclose(f);

	gtk_widget_destroy(DumpRDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDumpR_Cancel() {
	gtk_widget_destroy(DumpRDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_RawDump() {
	DumpRDlg = create_DumpRDlg();
	
	DumpRFEntry = lookup_widget(DumpRDlg, "GtkEntry_DumpRF");
	DumpRTEntry = lookup_widget(DumpRDlg, "GtkEntry_DumpRT");

	gtk_widget_show_all(DumpRDlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();
}

void OnDebug_Step() {
	Cpu->Step();
	dPC = cpuRegs.pc;
	UpdateDebugger();
}

void OnDebug_Skip() {
	cpuRegs.pc+= 4;
	dPC = cpuRegs.pc;
	UpdateDebugger();
}

int HasBreakPoint(u32 pc) {
	if (pc == dBPA) return 1;
	if (DebugMode == 0) {
		if ((cpuRegs.cycle - 10) <= dBPC &&
			(cpuRegs.cycle + 10) >= dBPC) return 1;
	} else {
		if ((psxRegs.cycle - 100) <= dBPC &&
			(psxRegs.cycle + 100) >= dBPC) return 1;
	}
	return 0;
}

void OnDebug_Go() {
	for (;;) {
		if (HasBreakPoint(cpuRegs.pc)) break;
		Cpu->Step();
	}
	dPC = cpuRegs.pc;
	UpdateDebugger();
}

void OnDebug_Log() {
	Log = 1 - Log;
}

void OnDebug_EEMode() {
	DebugMode = 0;
	dPC = cpuRegs.pc;
	UpdateDebugger();
}

void OnDebug_IOPMode() {
	DebugMode = 1;
	dPC = psxRegs.pc;
	UpdateDebugger();
}

void OnMemWrite32_Ok() {
	char *mem  = gtk_entry_get_text(GTK_ENTRY(MemEntry));
	char *data = gtk_entry_get_text(GTK_ENTRY(DataEntry));

	printf("memWrite32: %s, %s\n", mem, data);
	memWrite32(strtol(mem, (char**)NULL, 0), strtol(data, (char**)NULL, 0));

	gtk_widget_destroy(MemWriteDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnMemWrite32_Cancel() {
	gtk_widget_destroy(MemWriteDlg);
	gtk_main_quit();
	gtk_widget_set_sensitive(DebugWnd, TRUE);
}

void OnDebug_memWrite32() {
	MemWriteDlg = create_MemWrite32();

	MemEntry = lookup_widget(MemWriteDlg, "GtkEntry_Mem");
	DataEntry = lookup_widget(MemWriteDlg, "GtkEntry_Data");

	gtk_widget_show_all(MemWriteDlg);
	gtk_widget_set_sensitive(DebugWnd, FALSE);
	gtk_main();

	UpdateDebugger();
}

void OnDebug_Debugger() {
	GtkWidget *scroll;

	if (OpenPlugins() == -1) return;
	if (needreset) { SysReset(); needreset = 0; }

	if (!efile)
		efile=GetPS2ElfName(elfname);
	if (efile)
		loadElfFile(elfname);
	efile=0;

	dPC = cpuRegs.pc;

	DebugWnd = create_DebugWnd();

	ListDV = lookup_widget(DebugWnd, "GtkList_DisView");
	scroll = lookup_widget(DebugWnd, "GtkVScrollbar_VList");

	DebugAdj = GTK_RANGE(scroll)->adjustment;
	DebugAdj->lower = (gfloat)0x00000000/4;
	DebugAdj->upper = (gfloat)0xffffffff/4;
	DebugAdj->step_increment = (gfloat)1;
	DebugAdj->page_increment = (gfloat)20;
	DebugAdj->page_size = (gfloat)23;

	gtk_signal_connect(GTK_OBJECT(DebugAdj),
	                   "value_changed", GTK_SIGNAL_FUNC(OnDebug_ScrollChange),
					   NULL);

	UpdateDebugger();

	gtk_widget_show_all(DebugWnd);
	gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}

void OnLogging_Ok() {
	GtkWidget *Btn;
	char str[32];
	int i, ret;

	for (i=0; i<17; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		ret = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));
		if (ret) varLog|= 1<<i;
		else varLog&=~(1<<i);
	}

	for (i=20; i<29; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		ret = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));
		if (ret) varLog|= 1<<i;
		else varLog&=~(1<<i);
	}

	for (i=30; i<32; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		ret = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));
		if (ret) varLog|= 1<<i;
		else varLog&=~(1<<i);
	}

	Btn = lookup_widget(LogDlg, "Log");
	Log = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Btn));

	SaveConfig();

	gtk_widget_destroy(LogDlg);
	gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnLogging_Cancel() {
	gtk_widget_destroy(LogDlg);
	gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnDebug_Logging() {
	GtkWidget *Btn;
	char str[32];
	int i;

	LogDlg = create_Logging();

	for (i=0; i<17; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), varLog & (1<<i));
	}

	for (i=20; i<29; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), varLog & (1<<i));
	}

	for (i=30; i<32; i++) {
		sprintf(str, "Log%d", i);
		Btn = lookup_widget(LogDlg, str);
		gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), varLog & (1<<i));
	}

	Btn = lookup_widget(LogDlg, "Log");
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(Btn), Log);

	gtk_widget_show_all(LogDlg);
	gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}

void OnHelp_Help() {
}

void OnHelpAbout_Ok() {
	gtk_widget_destroy(AboutDlg);
	gtk_widget_set_sensitive(Window, TRUE);
	gtk_main_quit();
}

void OnHelp_About() {
	char str[256];
	GtkWidget *Label;

	AboutDlg = create_AboutDlg();
	gtk_window_set_title(GTK_WINDOW(AboutDlg), _("About"));

	Label = lookup_widget(AboutDlg, "GtkAbout_LabelVersion");
	sprintf(str, _("PCSX2 For Linux\nVersion %s"), PCSX2_VERSION);
	gtk_label_set_text(GTK_LABEL(Label), str);

	Label = lookup_widget(AboutDlg, "GtkAbout_LabelAuthors");
	gtk_label_set_text(GTK_LABEL(Label), _(LabelAuthors));

	Label = lookup_widget(AboutDlg, "GtkAbout_LabelGreets");
	gtk_label_set_text(GTK_LABEL(Label), _(LabelGreets));

	gtk_widget_show_all(AboutDlg);
	gtk_widget_set_sensitive(Window, FALSE);
	gtk_main();
}

#define ComboAddPlugin(type) { \
	type##ConfS.plugins+=2; \
	strcpy(type##ConfS.plist[type##ConfS.plugins-1], name); \
	strcpy(type##ConfS.plist[type##ConfS.plugins-2], ent->d_name); \
	type##ConfS.glist = g_list_append(type##ConfS.glist, type##ConfS.plist[type##ConfS.plugins-1]); \
}

void FindPlugins() {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	char plugin[256],name[256];

	GSConfS.plugins  = 0;  CDVDConfS.plugins = 0; DEV9ConfS.plugins = 0;
	PAD1ConfS.plugins = 0; PAD2ConfS.plugins = 0; SPU2ConfS.plugins = 0;
	USBConfS.plugins = 0;  FWConfS.plugins = 0;  BiosConfS.plugins = 0;
	GSConfS.glist  = NULL;  CDVDConfS.glist = NULL; DEV9ConfS.glist = NULL;
	PAD1ConfS.glist = NULL; PAD2ConfS.glist = NULL; SPU2ConfS.glist = NULL;
	USBConfS.glist = NULL;  FWConfS.glist = NULL;  BiosConfS.glist = NULL;

	dir = opendir(Config.PluginsDir);
	if (dir == NULL) {
		SysMessage(_("Could not open '%s' directory"), Config.PluginsDir);
		return;
	}
	while ((ent = readdir(dir)) != NULL) {
		u32 version;
		u32 type;

		sprintf (plugin, "%s%s", Config.PluginsDir, ent->d_name);

		if (strstr(plugin, ".so") == NULL) continue;
		Handle = dlopen(plugin, RTLD_NOW);
		if (Handle == NULL) {
			printf("%s\n", dlerror()); continue;
		}

		PS2EgetLibType = (_PS2EgetLibType) dlsym(Handle, "PS2EgetLibType");
		PS2EgetLibName = (_PS2EgetLibName) dlsym(Handle, "PS2EgetLibName");
		PS2EgetLibVersion2 = (_PS2EgetLibVersion2) dlsym(Handle, "PS2EgetLibVersion2");
		if (PS2EgetLibType == NULL || PS2EgetLibName == NULL || PS2EgetLibVersion2 == NULL)
			continue;
		type = PS2EgetLibType();

		if (type & PS2E_LT_GS) {
			version = PS2EgetLibVersion2(PS2E_LT_GS);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_GS_VERSION) {
				ComboAddPlugin(GS);
			} else SysPrintf("Plugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_GS_VERSION);
		}
		if (type & PS2E_LT_PAD) {
			_PADquery query;

			query = (_PADquery)dlsym(Handle, "PADquery");
			version = PS2EgetLibVersion2(PS2E_LT_PAD);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_PAD_VERSION && query) {
				if (query() & 0x1)
					ComboAddPlugin(PAD1);
				if (query() & 0x2)
					ComboAddPlugin(PAD2);
			} else SysPrintf("Plugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_PAD_VERSION);
		}
		if (type & PS2E_LT_SPU2) {
			version = PS2EgetLibVersion2(PS2E_LT_SPU2);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_SPU2_VERSION) {
				ComboAddPlugin(SPU2);
			} else SysPrintf("Plugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_SPU2_VERSION);
		}
		if (type & PS2E_LT_CDVD) {
			version = PS2EgetLibVersion2(PS2E_LT_CDVD);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_CDVD_VERSION) {
				ComboAddPlugin(CDVD);
			} else SysPrintf("Plugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_CDVD_VERSION);
		}
		if (type & PS2E_LT_DEV9) {
			version = PS2EgetLibVersion2(PS2E_LT_DEV9);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_DEV9_VERSION) {
				ComboAddPlugin(DEV9);
			} else SysPrintf("DEV9Plugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_DEV9_VERSION);
		}
		if (type & PS2E_LT_USB) {
			version = PS2EgetLibVersion2(PS2E_LT_USB);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_USB_VERSION) {
				ComboAddPlugin(USB);
			} else SysPrintf("USBPlugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_USB_VERSION);
		}
		if (type & PS2E_LT_FW) {
			version = PS2EgetLibVersion2(PS2E_LT_FW);
			sprintf (name, "%s v%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff);
			if ((version >> 16) == PS2E_FW_VERSION) {
				ComboAddPlugin(FW);
			} else SysPrintf("FWPlugin %s: Version %x != %x\n", plugin, version >> 16, PS2E_FW_VERSION);
		}
	}
	closedir(dir);

	dir = opendir(Config.BiosDir);
	if (dir == NULL) {
		SysMessage(_("Could not open '%s' directory"), Config.BiosDir);
		return;
	}

	while ((ent = readdir(dir)) != NULL) {
		struct stat buf;
		char description[50];				//2002-09-28 (Florin)

		sprintf (plugin, "%s%s", Config.BiosDir, ent->d_name);
		if (stat(plugin, &buf) == -1) continue;
//		if (buf.st_size < (1024*512)) continue;
		if (buf.st_size > (1024*4096)) continue;	//2002-09-28 (Florin)
		if (!IsBIOS(ent->d_name, description)) continue;//2002-09-28 (Florin)

		BiosConfS.plugins+=2;
		sprintf(description, "%s (%s)", description, ent->d_name);
		strcpy(BiosConfS.plist[BiosConfS.plugins-1], description);//2002-09-28 (Florin) modified
		strcpy(BiosConfS.plist[BiosConfS.plugins-2], ent->d_name);
		BiosConfS.glist = g_list_append(BiosConfS.glist, BiosConfS.plist[BiosConfS.plugins-1]);
	}
	closedir(dir);
}

GtkWidget *MsgDlg;

void OnMsg_Ok() {
	gtk_widget_destroy(MsgDlg);
	gtk_main_quit();
}

void SysMessage(char *fmt, ...) {
	GtkWidget *Ok,*Txt;
	GtkWidget *Box,*Box1;
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;

	if (!UseGui) { printf("%s\n",msg); return; }

	MsgDlg = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_position(GTK_WINDOW(MsgDlg), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(MsgDlg), _("PCSX2 Msg"));
	gtk_container_set_border_width(GTK_CONTAINER(MsgDlg), 5);

	Box = gtk_vbox_new(5, 0);
	gtk_container_add(GTK_CONTAINER(MsgDlg), Box);
	gtk_widget_show(Box);

	Txt = gtk_label_new(msg);
	
	gtk_box_pack_start(GTK_BOX(Box), Txt, FALSE, FALSE, 5);
	gtk_widget_show(Txt);

	Box1 = gtk_hbutton_box_new();
	gtk_box_pack_start(GTK_BOX(Box), Box1, FALSE, FALSE, 0);
	gtk_widget_show(Box1);

	Ok = gtk_button_new_with_label(_("Ok"));
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnMsg_Ok), NULL);
	gtk_container_add(GTK_CONTAINER(Box1), Ok);
	GTK_WIDGET_SET_FLAGS(Ok, GTK_CAN_DEFAULT);
	gtk_widget_show(Ok);

	gtk_widget_show(MsgDlg);	

	gtk_main();
}
