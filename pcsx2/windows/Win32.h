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

#ifndef _PCSX2_WIN32_H__
#define _PCSX2_WIN32_H__

#include <windowsx.h>
#include <tchar.h>

#include "Misc.h"
#include "resource.h"

// --->>  Ini Configuration [ini.c]

extern char g_WorkingFolder[g_MaxPath];
extern const char* g_CustomConfigFile;

int  LoadConfig();
void SaveConfig();

// <<--- END Ini Configuration [ini.c]

// --->>  Patch Browser Stuff (in the event we ever use it

void ListPatches (HWND hW);
int ReadPatch (HWND hW, char fileName[1024]);
char * lTrim (char *s);
BOOL Save_Patch_Proc( char * filename );

// <<--- END Patch Browser

struct AppData
{
	HWND hWnd;           // Main window handle
	HINSTANCE hInstance; // Application instance
	HMENU hMenu;         // Main window menu
};

extern AppData gApp;
extern HWND hStatusWnd;
extern PcsxConfig winConfig;		// local storage of the configuration options.
extern bool g_GameInProgress;

LRESULT WINAPI MainWndProc(HWND, UINT, WPARAM, LPARAM);
void CreateMainWindow(int nCmdShow);
void RunGui();

BOOL Pcsx2Configure(HWND hWnd);
void InitLanguages();
char *GetLanguageNext();
void CloseLanguages();
void ChangeLanguage(char *lang);
#define StatusSet(text) SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)(text));

void SysRestorableReset();

BOOL CALLBACK PatchBDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CpuDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AdvancedOptionsProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam);


#endif

