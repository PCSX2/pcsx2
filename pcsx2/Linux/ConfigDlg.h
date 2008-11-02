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
 
 #ifndef __CONFIGDLG_H__
#define __CONFIGDLG_H__

#include "Linux.h"
#ifdef __cplusplus
extern "C" {
#endif
	
#include "support.h"
#include "callbacks.h"
#include "interface.h"
	
#include "R3000A.h"
#include "PsxMem.h"
	
#ifdef __cplusplus
}
#endif

// Evil Macros - Destroy when possible
#define FindComboText(combo,plist, list, conf) \
	if (strlen(conf) > 0) { \
		SetActiveComboItem(GTK_COMBO_BOX(combo), plist, list, conf); \
	}
	
#define GetComboText(combo,list,conf) \
	{ \
	int i; \
	char *tmp = (char*)gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo)); \
	for (i=2;i<255;i+=2) { \
		if (!strcmp(tmp, list[i-1])) { \
			strcpy(conf, list[i-2]); \
			break; \
		} \
	} \
	}

#define ConfPlugin(src, confs, plugin, name) \
	void *drv; \
	src conf; \
	char file[256]; \
	GetComboText(confs.Combo, confs.plist, plugin); \
    strcpy(file, Config.PluginsDir); \
	strcat(file, plugin); \
	drv = SysLoadLibrary(file); \
    getcwd(file, ARRAYSIZE(file)); /* store current dir */  \
    chdir(Config.PluginsDir); /* change dirs so that plugins can find their config file*/  \
	if (drv == NULL) return; \
	conf = (src) SysLoadSym(drv, (char*)name); \
	if (SysLibError() == NULL) conf(); \
    chdir(file); /* change back*/       \
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
	conf = (src) SysLoadSym(drv, (char*)name); \
	if (SysLibError() == NULL) { \
		ret = conf(); \
		if (ret == 0) \
			 SysMessage(_("This plugin reports that should work correctly")); \
		else SysMessage(_("This plugin reports that should not work correctly")); \
	} \
	SysCloseLibrary(drv);

#define ConfCreatePConf(name, type) \
        type##ConfS.Combo = lookup_widget(ConfDlg, "GtkCombo_" name); \
	SetComboToGList(GTK_COMBO_BOX(type##ConfS.Combo), type##ConfS.PluginNameList);  \
	FindComboText(type##ConfS.Combo, type##ConfS.plist, type##ConfS.PluginNameList, Config.type); 

#define ComboAddPlugin(type) { \
    sprintf (name, "%s %ld.%ld.%ld", PS2EgetLibName(), (version>>8)&0xff ,version&0xff, (version>>24)&0xff); \
	type##ConfS.plugins+=2; \
	strcpy(type##ConfS.plist[type##ConfS.plugins-1], name); \
	strcpy(type##ConfS.plist[type##ConfS.plugins-2], ent->d_name); \
	type##ConfS.PluginNameList = g_list_append(type##ConfS.PluginNameList, type##ConfS.plist[type##ConfS.plugins-1]); \
}

// Helper Functions
void FindPlugins();
void OnConf_Gs(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Pads(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Cpu(GtkMenuItem *menuitem, gpointer user_data);
void OnConf_Conf(GtkMenuItem *menuitem, gpointer user_data);
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

GtkWidget *ConfDlg;

_PS2EgetLibType		PS2EgetLibType = NULL;
_PS2EgetLibVersion2	PS2EgetLibVersion2 = NULL;
_PS2EgetLibName		PS2EgetLibName = NULL;

#endif // __CONFIGDLG_H__