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

#include "GtkGui.h"

void StartGui() {
	GtkWidget *Menu;
	GtkWidget *Item;
	
	u32 i;

	add_pixmap_directory(".pixmaps");
	MainWindow = create_MainWindow();
	
#ifdef PCSX2_VIRTUAL_MEM
	gtk_window_set_title(GTK_WINDOW(MainWindow), "PCSX2 "PCSX2_VERSION" Watermoose VM");
#else
	gtk_window_set_title(GTK_WINDOW(MainWindow), "PCSX2 "PCSX2_VERSION" Watermoose");
#endif

	// status bar
	pStatusBar = gtk_statusbar_new ();
	gtk_box_pack_start (GTK_BOX(lookup_widget(MainWindow, "status_box")), pStatusBar, TRUE, TRUE, 0);
	gtk_widget_show (pStatusBar);

	gtk_statusbar_push(GTK_STATUSBAR(pStatusBar),0,
                       "F1 - save, F2 - next state, Shift+F2 - prev state, F3 - load, F8 - snapshot");

	// add all the languages
	Item = lookup_widget(MainWindow, "GtkMenuItem_Language");
	Menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(Item), Menu);

	for (i=0; i < langsMax; i++) {
		Item = gtk_check_menu_item_new_with_label(ParseLang(langs[i].lang));
		gtk_widget_show(Item);
		gtk_container_add(GTK_CONTAINER(Menu), Item);
		gtk_check_menu_item_set_show_toggle(GTK_CHECK_MENU_ITEM(Item), TRUE);
		if (!strcmp(Config.Lang, langs[i].lang))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(Item), TRUE);

		gtk_signal_connect(GTK_OBJECT(Item), "activate",
                           GTK_SIGNAL_FUNC(OnLanguage),
                           (gpointer)(uptr)i);
	}

	// check the appropriate menu items
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(MainWindow, "enable_console1")), Config.PsxOut);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(MainWindow, "enable_patches1")), Config.Patch);
	
	// disable anything not implemented or not working properly.
	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(MainWindow, "patch_browser1")), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(MainWindow, "patch_finder2")), FALSE);
	
	gtk_widget_show_all(MainWindow);
	gtk_window_activate_focus(GTK_WINDOW(MainWindow));
	gtk_main();
}

void RunGui() {
	StartGui();
}

void OnDestroy(GtkObject *object, gpointer user_data) {
	if (!destroy) OnFile_Exit(NULL, user_data);
}

int Pcsx2Configure() {
	if (!UseGui) 
		return 0;
	
	configuringplug = TRUE;
	MainWindow = NULL;
	OnConf_Conf(NULL, 0);
	configuringplug = FALSE;
	
	return applychanges;
}

void OnLanguage(GtkMenuItem *menuitem, gpointer user_data) {
	ChangeLanguage(langs[(int)(uptr)user_data].lang);
	destroy = TRUE;
	gtk_widget_destroy(MainWindow);
	destroy = FALSE;
	gtk_main_quit();
	while (gtk_events_pending()) gtk_main_iteration();
	StartGui();
}

void SignalExit(int sig) {
	ClosePlugins();
	OnFile_Exit(NULL, 0);
}

void RunExecute(int run)
{
    if (needReset == TRUE) {
		SysReset();
	}

	destroy= TRUE;
	gtk_widget_destroy(MainWindow);
	destroy=FALSE;
	gtk_main_quit();
	while (gtk_events_pending()) gtk_main_iteration();

	if (OpenPlugins(NULL) == -1) {
		RunGui(); 
		return;
	}
	signal(SIGINT, SignalExit);
	signal(SIGPIPE, SignalExit);

	if (needReset == TRUE) { 
		
		if( RunExe == 0 )
			cpuExecuteBios();
		if(!efile)
			efile=GetPS2ElfName(elfname);
		loadElfFile(elfname);

		RunExe = 0;
		efile = 0;
		needReset = FALSE;
	}

    // this needs to be called for every new game! (note: sometimes launching games through bios will give a crc of 0)
	if( GSsetGameCRC != NULL )
		GSsetGameCRC(ElfCRC, g_ZeroGSOptions);

	if (run) Cpu->Execute();
}

void OnFile_RunCD(GtkMenuItem *menuitem, gpointer user_data) {
	needReset = TRUE;
	efile = 0;
	RunExecute(1);
}

void OnRunElf_Ok(GtkButton* button, gpointer user_data) {
	gchar *File;

	File = (gchar*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(elfname, File);
	gtk_widget_destroy(FileSel);
	needReset = TRUE;
	efile = 1;
	RunExecute(1);
}

void OnRunElf_Cancel(GtkButton* button, gpointer user_data) {
	gtk_widget_destroy(FileSel);
}

void OnFile_LoadElf(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new("Select Psx Elf File");

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnRunElf_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnRunElf_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
}

void OnFile_Exit(GtkMenuItem *menuitem, gpointer user_data) {
	DIR *dir;
	struct dirent *ent;
	void *Handle;
	char plugin[g_MaxPath];

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
	
	if (UseGui)
	{
		gtk_main_quit();
		SysClose();
		gtk_exit(0);
	}
	else
	{
		SysClose();
		exit(0);
	}
}

void OnEmu_Run(GtkMenuItem *menuitem, gpointer user_data)
{
	if(needReset == TRUE)
		RunExe = 1;
	efile = 0;
	RunExecute(1);
}

void OnEmu_Reset(GtkMenuItem *menuitem, gpointer user_data)
{
	ResetPlugins();
	needReset = TRUE;
	efile = 0;
}

 
 void ResetMenuSlots(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget *Item;
	char str[g_MaxPath];
	int i;
 
	for (i=0; i<5; i++) {
		sprintf(str, "GtkMenuItem_LoadSlot%d", i+1);
		Item = lookup_widget(MainWindow, str);
		if (Slots[i] == -1) 
			gtk_widget_set_sensitive(Item, FALSE);
		else
			gtk_widget_set_sensitive(Item, TRUE);
	}
 }

void UpdateMenuSlots(GtkMenuItem *menuitem, gpointer user_data) {
	char str[g_MaxPath];
	int i = 0;

	for (i=0; i<5; i++) {
		sprintf(str, SSTATES_DIR "/%8.8X.%3.3d", ElfCRC, i);
		Slots[i] = CheckState(str);
	}
}

void States_Load(int num) {
	char Text[g_MaxPath];
	int ret;

	efile = 2;
	RunExecute(0);

	sprintf (Text, SSTATES_DIR "/%8.8X.%3.3d", ElfCRC, num);
	ret = LoadState(Text);

	Cpu->Execute();
}

void States_Save(int num) {
	char Text[g_MaxPath];
	int ret;

	sprintf (Text, SSTATES_DIR "/%8.8X.%3.3d", ElfCRC, num);
	ret = SaveState(Text);
	if (ret == 0)
		sprintf(Text, _("*PCSX2*: Saving State %d"), num+1);
	else 
		sprintf(Text, _("*PCSX2*: Error Saving State %d"), num+1);

    RunExecute(1);
}

void OnStates_Load1(GtkMenuItem *menuitem, gpointer user_data) { States_Load(0); } 
void OnStates_Load2(GtkMenuItem *menuitem, gpointer user_data) { States_Load(1); } 
void OnStates_Load3(GtkMenuItem *menuitem, gpointer user_data) { States_Load(2); } 
void OnStates_Load4(GtkMenuItem *menuitem, gpointer user_data) { States_Load(3); } 
void OnStates_Load5(GtkMenuItem *menuitem, gpointer user_data) { States_Load(4); } 

void OnLoadOther_Ok(GtkButton* button, gpointer user_data) {
	gchar *File;
	char str[g_MaxPath];
	int ret;

	File = (gchar*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(str, File);
	gtk_widget_destroy(FileSel);

	efile = 2;
	RunExecute(0);

	ret = LoadState(str);

	Cpu->Execute();
}

void OnLoadOther_Cancel(GtkButton* button, gpointer user_data) {
	gtk_widget_destroy(FileSel);
}

void OnStates_LoadOther(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select State File"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(FileSel), SSTATES_DIR "/");

	Ok = GTK_FILE_SELECTION(FileSel)->ok_button;
	gtk_signal_connect (GTK_OBJECT(Ok), "clicked", GTK_SIGNAL_FUNC(OnLoadOther_Ok), NULL);
	gtk_widget_show(Ok);

	Cancel = GTK_FILE_SELECTION(FileSel)->cancel_button;
	gtk_signal_connect (GTK_OBJECT(Cancel), "clicked", GTK_SIGNAL_FUNC(OnLoadOther_Cancel), NULL);
	gtk_widget_show(Cancel);

	gtk_widget_show(FileSel);
	gdk_window_raise(FileSel->window);
} 

void OnStates_Save1(GtkMenuItem *menuitem, gpointer user_data) { States_Save(0); } 
void OnStates_Save2(GtkMenuItem *menuitem, gpointer user_data) { States_Save(1); } 
void OnStates_Save3(GtkMenuItem *menuitem, gpointer user_data) { States_Save(2); } 
void OnStates_Save4(GtkMenuItem *menuitem, gpointer user_data) { States_Save(3); } 
void OnStates_Save5(GtkMenuItem *menuitem, gpointer user_data) { States_Save(4); } 

void OnSaveOther_Ok(GtkButton* button, gpointer user_data) {
	gchar *File;
	char str[g_MaxPath];
	int ret;

	File = (gchar*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(FileSel));
	strcpy(str, File);
	gtk_widget_destroy(FileSel);
	RunExecute(0);

	ret = SaveState(str);

	Cpu->Execute();
}

void OnSaveOther_Cancel(GtkButton* button, gpointer user_data) {
	gtk_widget_destroy(FileSel);
}

void OnStates_SaveOther(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget *Ok,*Cancel;

	FileSel = gtk_file_selection_new(_("Select State File"));
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(FileSel), SSTATES_DIR "/");

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
void OnArguments_Ok(GtkButton *button, gpointer user_data) {
	char *str;

	str = (char*)gtk_entry_get_text(GTK_ENTRY(widgetCmdLine));
	memcpy(args, str, g_MaxPath);

	gtk_widget_destroy(CmdLine);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnArguments_Cancel(GtkButton* button, gpointer user_data) {
	gtk_widget_destroy(CmdLine);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnEmu_Arguments(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget *widgetCmdLine;

	CmdLine = create_CmdLine();
	gtk_window_set_title(GTK_WINDOW(CmdLine), _("Program arguments"));

	widgetCmdLine = lookup_widget(CmdLine, "GtkEntry_dCMDLINE");
	
	gtk_entry_set_text(GTK_ENTRY(widgetCmdLine), args);
						//args exported by ElfHeader.h
	gtk_widget_show_all(CmdLine);
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
}

void OnCpu_Ok(GtkButton *button, gpointer user_data) {
	u32 newopts = 0;

	Cpu->Shutdown();
	vu0Shutdown();
	vu1Shutdown();
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_EERec"))))
		newopts |= PCSX2_EEREC;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_VU0rec"))))
		newopts |= PCSX2_VU0REC;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_VU1rec"))))
		newopts |= PCSX2_VU1REC;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_MTGS"))))
		newopts |= PCSX2_GSMULTITHREAD;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_CpuDC"))))
		newopts |= PCSX2_DUALCORE;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitNormal"))))
		newopts |= PCSX2_FRAMELIMIT_NORMAL;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitLimit"))))
		newopts |= PCSX2_FRAMELIMIT_LIMIT;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitFS"))))
		newopts |= PCSX2_FRAMELIMIT_SKIP;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_VUSkip"))))
		newopts |= PCSX2_FRAMELIMIT_VUSKIP;

	if ((Config.Options&PCSX2_GSMULTITHREAD) ^ (newopts&PCSX2_GSMULTITHREAD)) {
		Config.Options = newopts;
		SaveConfig();
		SysMessage("Restart Pcsx2");
		exit(0);
	}
	
	if (newopts & PCSX2_EEREC ) newopts |= PCSX2_COP2REC;
	
	Config.Options = newopts;
	
	UpdateVSyncRate();
	SaveConfig();
	
	cpuRestartCPU();
	
	gtk_widget_destroy(CpuDlg);
	if (MainWindow) gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnCpu_Cancel(GtkButton *button, gpointer user_data) {
	gtk_widget_destroy(CpuDlg);
	if (MainWindow) gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnConf_Cpu(GtkMenuItem *menuitem, gpointer user_data)
{
	char str[512];

	CpuDlg = create_CpuDlg();
	gtk_window_set_title(GTK_WINDOW(CpuDlg), _("Configuration"));
	
        gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_EERec")), !!CHECK_EEREC);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_VU0rec")), !!CHECK_VU0REC);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_VU1rec")), !!CHECK_VU1REC);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_MTGS")), !!CHECK_MULTIGS);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkCheckButton_CpuDC")), !!CHECK_DUALCORE);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitNormal")), CHECK_FRAMELIMIT==PCSX2_FRAMELIMIT_NORMAL);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitLimit")), CHECK_FRAMELIMIT==PCSX2_FRAMELIMIT_LIMIT);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_LimitFS")), CHECK_FRAMELIMIT==PCSX2_FRAMELIMIT_SKIP);
	gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(lookup_widget(CpuDlg, "GtkRadioButton_VUSkip")), CHECK_FRAMELIMIT==PCSX2_FRAMELIMIT_VUSKIP);
    
	sprintf(str, "Cpu Vendor:     %s", cpuinfo.x86ID);
	gtk_label_set_text(GTK_LABEL(lookup_widget(CpuDlg, "GtkLabel_CpuVendor")), str);
	sprintf(str, "Familly:   %s", cpuinfo.x86Fam);
	gtk_label_set_text(GTK_LABEL(lookup_widget(CpuDlg, "GtkLabel_Family")), str);
	sprintf(str, "Cpu Speed:   %d MHZ", cpuinfo.cpuspeed);
	gtk_label_set_text(GTK_LABEL(lookup_widget(CpuDlg, "GtkLabel_CpuSpeed")), str);

	strcpy(str,"Features:    ");
	if(cpucaps.hasMultimediaExtensions) strcat(str,"MMX");
	if(cpucaps.hasStreamingSIMDExtensions) strcat(str,",SSE");
	if(cpucaps.hasStreamingSIMD2Extensions) strcat(str,",SSE2");
	if(cpucaps.hasStreamingSIMD3Extensions) strcat(str,",SSE3");
	if(cpucaps.hasAMD64BitArchitecture) strcat(str,",x86-64");
	gtk_label_set_text(GTK_LABEL(lookup_widget(CpuDlg, "GtkLabel_Features")), str);

	gtk_widget_show_all(CpuDlg);
	if (MainWindow) gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
}

void OnLogging_Ok(GtkButton *button, gpointer user_data) {
#ifdef PCSX2_DEVBUILD
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
#endif

	gtk_widget_destroy(LogDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnLogging_Cancel(GtkButton *button, gpointer user_data) {
	gtk_widget_destroy(LogDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnDebug_Logging(GtkMenuItem *menuitem, gpointer user_data) {
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
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
}

void OnHelp_Help() {
}

void OnHelpAbout_Ok(GtkButton *button, gpointer user_data) {
	gtk_widget_destroy(AboutDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void OnHelp_About(GtkMenuItem *menuitem, gpointer user_data) {
	char str[g_MaxPath];
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
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
}

void on_patch_browser1_activate(GtkMenuItem *menuitem, gpointer user_data) {}

void on_patch_finder2_activate(GtkMenuItem *menuitem, gpointer user_data) {}

void on_enable_console1_activate (GtkMenuItem *menuitem, gpointer user_data)
{
    Config.PsxOut=(int)gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem);
    SaveConfig();
}

void on_enable_patches1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    Config.Patch=(int)gtk_check_menu_item_get_active((GtkCheckMenuItem*)menuitem);
    SaveConfig();
}

void on_Game_Fixes(GtkMenuItem *menuitem, gpointer user_data) 
{
	GameFixDlg = create_GameFixDlg();
	
        set_checked(GameFixDlg, "check_Slow_DVD", (Config.GameFixes & FLAG_SLOW_DVD));
	set_checked(GameFixDlg, "check_VU_Clip", (Config.GameFixes & FLAG_VU_CLIP));
	set_checked(GameFixDlg, "check_FPU_Clamp", (Config.GameFixes & FLAG_FPU_CLAMP));
	set_checked(GameFixDlg, "check_VU_Branch", (Config.GameFixes & FLAG_VU_BRANCH));
	
	gtk_widget_show_all(GameFixDlg);
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
	}
void on_Game_Fix_Cancel(GtkButton *button, gpointer user_data)
{
	gtk_widget_destroy(GameFixDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}
void on_Game_Fix_OK(GtkButton *button, gpointer user_data) 
{
	
	Config.GameFixes = 0;
	Config.GameFixes |= is_checked(GameFixDlg, "check_Slow_DVD") ? FLAG_SLOW_DVD : 0;
	Config.GameFixes |= is_checked(GameFixDlg, "check_VU_Clip") ? FLAG_VU_CLIP : 0;
	Config.GameFixes |= is_checked(GameFixDlg, "check_FPU_Clamp") ? FLAG_FPU_CLAMP : 0;
	Config.GameFixes |= is_checked(GameFixDlg, "check_VU_Branch") ? FLAG_VU_BRANCH : 0;
	
	SaveConfig();
	gtk_widget_destroy(GameFixDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}

void on_Speed_Hacks(GtkMenuItem *menuitem, gpointer user_data)
{
	int index;
	SpeedHacksDlg = create_SpeedHacksDlg();
	
	index = 1; //Default to normal
	if get_flag(Config.Hacks, FLAG_VU_EXTRA_OVERFLOW) index = 2;
	if  get_flag(Config.Hacks, FLAG_VU_NO_OVERFLOW) index = 0;
	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboVUOverflow")), index);
	
	index = 1; //Default to normal
	if get_flag(Config.Hacks, FLAG_FPU_EXTRA_OVERFLOW) index = 2;
	if get_flag(Config.Hacks, FLAG_FPU_NO_OVERFLOW) index = 0;

	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboFPUOverflow")), index);
	
        set_checked(SpeedHacksDlg, "check_EE_Double_Sync", (Config.Hacks & FLAG_EE_2_SYNC));
	set_checked(SpeedHacksDlg, "check_Tight_SPU_Sync", (Config.Hacks & FLAG_TIGHT_SPU_SYNC));
	set_checked(SpeedHacksDlg, "check_Disable_Underflow", (Config.Hacks & FLAG_NO_UNDERFLOW));
	set_checked(SpeedHacksDlg, "check_IOP_Double_Sync", (Config.Hacks & FLAG_IOP_2_SYNC));
	set_checked(SpeedHacksDlg, "check_Triple_Sync",(Config.Hacks & FLAG_TRIPLE_SYNC));
        set_checked(SpeedHacksDlg, "check_EE_Fast_Branches", (Config.Hacks & FLAG_FAST_BRANCHES));
        set_checked(SpeedHacksDlg, "check_Disable_VU_Flags", (Config.Hacks & FLAG_NO_VU_FLAGS));
	set_checked(SpeedHacksDlg, "check_Disable_FPU_Flags", (Config.Hacks & FLAG_NO_FPU_FLAGS));
	set_checked(SpeedHacksDlg, "check_ESC_Hack", (Config.Hacks & FLAG_ESC));
	
	gtk_widget_show_all(SpeedHacksDlg);
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
	}
void on_Speed_Hack_Compatability(GtkButton *button, gpointer user_data)
{
        set_checked(SpeedHacksDlg, "check_EE_Double_Sync", FALSE);
       
	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboVUOverflow")), 2);
	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboFPUOverflow")), 2);
	
	set_checked(SpeedHacksDlg, "check_Disable_Underflow", TRUE);
	set_checked(SpeedHacksDlg, "check_IOP_Double_Sync", FALSE);
	set_checked(SpeedHacksDlg, "check_Triple_Sync", FALSE);
        set_checked(SpeedHacksDlg, "check_EE_Fast_Branches", FALSE);
        set_checked(SpeedHacksDlg, "check_Disable_VU_Flags", TRUE);
	set_checked(SpeedHacksDlg, "check_Disable_FPU_Flags", TRUE);
	
}
void on_Speed_Hack_Speed(GtkButton *button, gpointer user_data)
{
        set_checked(SpeedHacksDlg, "check_EE_Double_Sync", TRUE);
        
	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboVUOverflow")), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboFPUOverflow")), 0);
	
	set_checked(SpeedHacksDlg, "check_Tight_SPU_Sync", FALSE);
	set_checked(SpeedHacksDlg, "check_Disable_Underflow", TRUE);
	set_checked(SpeedHacksDlg, "check_IOP_Double_Sync", TRUE);
	set_checked(SpeedHacksDlg, "check_Triple_Sync", TRUE);
        set_checked(SpeedHacksDlg, "check_EE_Fast_Branches", FALSE);
        set_checked(SpeedHacksDlg, "check_Disable_VU_Flags", TRUE);
	set_checked(SpeedHacksDlg, "check_Disable_FPU_Flags", TRUE);
	
}
void on_Speed_Hack_Cancel(GtkButton *button, gpointer user_data)
{
	gtk_widget_destroy(SpeedHacksDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}
void on_Speed_Hack_OK(GtkButton *button, gpointer user_data)
{
	Config.Hacks = 0;

	Config.Hacks |= is_checked(SpeedHacksDlg, "check_EE_Double_Sync") ? FLAG_EE_2_SYNC : 0;  
	
	switch (gtk_combo_box_get_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboVUOverflow")))) {
		case 0: //Disabled
			set_flag(Config.Hacks, FLAG_VU_NO_OVERFLOW, TRUE);
			break;
		case 1: //Normal
			// Not having either flag set to true is normal
			break;
		case 2: //Extra
			set_flag(Config.Hacks, FLAG_VU_EXTRA_OVERFLOW, TRUE);
			break;
	}
	
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_Tight_SPU_Sync") ? FLAG_TIGHT_SPU_SYNC : 0;	
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_Disable_Underflow") ? FLAG_NO_UNDERFLOW : 0;  
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_IOP_Double_Sync") ? FLAG_IOP_2_SYNC : 0;  
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_Triple_Sync") ? FLAG_TRIPLE_SYNC : 0;  
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_EE_Fast_Branches") ? FLAG_FAST_BRANCHES : 0; 
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_Disable_VU_Flags") ? FLAG_NO_VU_FLAGS : 0;  
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_Disable_FPU_Flags")? FLAG_NO_FPU_FLAGS : 0;  
	Config.Hacks |= is_checked(SpeedHacksDlg, "check_ESC_Hack") ? FLAG_ESC : 0;  
	
	switch (gtk_combo_box_get_active(GTK_COMBO_BOX(lookup_widget(SpeedHacksDlg, "ComboFPUOverflow")))) {
		case 0: //Disabled
			set_flag(Config.Hacks, FLAG_FPU_NO_OVERFLOW, TRUE);
			break;
		case 1: //Normal
			// Not having either flag set to true is normal
			break;
		case 2: //Extra
			set_flag(Config.Hacks, FLAG_FPU_EXTRA_OVERFLOW, TRUE);
			break;
	}
	
	SaveConfig();

	gtk_widget_destroy(SpeedHacksDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}
void setAdvancedOptions()
{
	switch((Config.sseMXCSR & 0x6000) >> 13)
	{
		case 0:
			set_checked(AdvDlg, "radio_EE_Round_Near", TRUE);
			break;
		case 1:
			set_checked(AdvDlg, "radio_EE_Round_Negative",TRUE);
			break;
		case 2:
			set_checked(AdvDlg, "radio_EE_Round_Positive", TRUE);
			break;
		case 3:
			set_checked(AdvDlg, "radio_EE_Round_Zero", TRUE);
			break;
	}
	
	switch((Config.sseVUMXCSR & 0x6000) >> 13)
	{
		case 0:
			set_checked(AdvDlg, "radio_VU_Round_Near", TRUE);
			break;
		case 1:
			set_checked(AdvDlg, "radio_VU_Round_Negative",TRUE);
			break;
		case 2:
			set_checked(AdvDlg, "radio_VU_Round_Positive", TRUE);
			break;
		case 3:
			set_checked(AdvDlg, "radio_VU_Round_Zero", TRUE);
			break;
	}
	
	set_checked(AdvDlg, "check_EE_Flush_Zero", (Config.sseMXCSR & FLAG_FLUSH_ZERO) ? TRUE : FALSE);
	set_checked(AdvDlg, "check_EE_Denormal_Zero", (Config.sseMXCSR & FLAG_DENORMAL_ZERO) ? TRUE : FALSE);
	
	set_checked(AdvDlg, "check_VU_Flush_Zero", (Config.sseVUMXCSR & FLAG_FLUSH_ZERO) ? TRUE : FALSE);
	set_checked(AdvDlg, "check_VU_Denormal_Zero", (Config.sseVUMXCSR & FLAG_DENORMAL_ZERO) ? TRUE : FALSE);
}
void on_Advanced(GtkMenuItem *menuitem, gpointer user_data)
{
	AdvDlg = create_AdvDlg();
	
	setAdvancedOptions();
	
	gtk_widget_show_all(AdvDlg);
	gtk_widget_set_sensitive(MainWindow, FALSE);
	gtk_main();
	}

void on_Advanced_Defaults(GtkButton *button, gpointer user_data)
{
	Config.sseMXCSR = DEFAULT_sseMXCSR;
	Config.sseVUMXCSR = DEFAULT_sseVUMXCSR;
	
	setAdvancedOptions();
      }
void on_Advanced_Cancel(GtkButton *button, gpointer user_data)
{
	gtk_widget_destroy(AdvDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}
void on_Advanced_OK(GtkButton *button, gpointer user_data) 
{
	Config.sseMXCSR &= 0x1fbf;
	Config.sseVUMXCSR &= 0x1fbf;
	
	
	Config.sseMXCSR |= is_checked(AdvDlg, "radio_EE_Round_Near") ? FLAG_ROUND_NEAR : 0; 
	Config.sseMXCSR |= is_checked(AdvDlg, "radio_EE_Round_Negative") ? FLAG_ROUND_NEGATIVE : 0;
	Config.sseMXCSR |= is_checked(AdvDlg, "radio_EE_Round_Positive") ? FLAG_ROUND_POSITIVE : 0; 
	Config.sseMXCSR |= is_checked(AdvDlg, "radio_EE_Round_Zero") ? FLAG_ROUND_ZERO : 0; 

	Config.sseVUMXCSR |= is_checked(AdvDlg, "radio_VU_Round_Near") ? FLAG_ROUND_NEAR : 0; 
	Config.sseVUMXCSR |= is_checked(AdvDlg, "radio_VU_Round_Negative") ? FLAG_ROUND_NEGATIVE : 0;
	Config.sseVUMXCSR |= is_checked(AdvDlg, "radio_VU_Round_Positive") ? FLAG_ROUND_POSITIVE : 0; 
	Config.sseVUMXCSR |= is_checked(AdvDlg, "radio_VU_Round_Zero") ? FLAG_ROUND_ZERO : 0; 
	
	Config.sseMXCSR |= is_checked(AdvDlg, "check_EE_Flush_Zero") ? FLAG_FLUSH_ZERO : 0;
	Config.sseVUMXCSR |= is_checked(AdvDlg, "check_VU_Flush_Zero") ? FLAG_FLUSH_ZERO : 0;
	
	Config.sseMXCSR |= is_checked(AdvDlg, "check_EE_Denormal_Zero") ? FLAG_DENORMAL_ZERO : 0;
	Config.sseVUMXCSR |= is_checked(AdvDlg, "check_VU_Denormal_Zero") ? FLAG_DENORMAL_ZERO : 0;
	
	SetCPUState(Config.sseMXCSR, Config.sseVUMXCSR);
	SaveConfig();
	
	gtk_widget_destroy(AdvDlg);
	gtk_widget_set_sensitive(MainWindow, TRUE);
	gtk_main_quit();
}