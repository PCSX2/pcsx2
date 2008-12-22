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
/*
15-09-2004 : file rewriten for work with inis (shadow)
*/

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#include "Common.h"
#include "win32.h"
#include "Paths.h"

#include <sys/stat.h>

const char* g_CustomConfigFile;
char g_WorkingFolder[g_MaxPath];		// Working folder at application startup

// Returns TRUE if the user has invoked the -cfg command line option.
BOOLEAN hasCustomConfig()
{
	return (g_CustomConfigFile != NULL) && (g_CustomConfigFile[0] != 0);
}

// Returns the FULL (absolute) path and filename of the configuration file.
void GetConfigFilename( char* dest )
{
	if( hasCustomConfig() )
	{
		// Load a user-specified configuration.
		// If the configuration isn't found, fail outright (see below)

		CombinePaths( dest, g_WorkingFolder, g_CustomConfigFile );
	}
	else
	{
		// use the ini relative to the application's working directory.
		// Our current working directory can change, so we use the one we detected
		// at startup:

		CombinePaths( dest, g_WorkingFolder, CONFIG_DIR "\\pcsx2.ini" );
	}
}

int LoadConfig()
{
	FILE *fp;
	PcsxConfig& Conf = winConfig;

	char szIniFile[g_MaxPath], szValue[g_MaxPath];

	GetConfigFilename( szIniFile );

	if( g_Error_PathTooLong ) return -1;

	fp = fopen( szIniFile, "rt" );
	if( fp == NULL)
	{
		if( hasCustomConfig() )
		{
			// using custom config, so fail outright:
			SysMessage( "User-specified configuration file not found:\n %s\nPCSX2 will now exit." );
			return -1;
		}

		// standard mode operation.  Create the directory.
		// Conf File will be created and saved later.
		CreateDirectory("inis",NULL); 
		return 1;
	}
	fclose(fp);
    //interface
	GetPrivateProfileString("Interface", "Bios", NULL, szValue, g_MaxPath, szIniFile);
	strcpy(Conf.Bios, szValue);
	GetPrivateProfileString("Interface", "Lang", NULL, szValue, g_MaxPath, szIniFile);
	strcpy(Conf.Lang, szValue);
	GetPrivateProfileString("Interface", "Ps2Out", NULL, szValue, 20, szIniFile);
    Conf.PsxOut = !!strtoul(szValue, NULL, 10);
	GetPrivateProfileString("Interface", "Profiler", NULL, szValue, 20, szIniFile);
	Conf.Profiler = !!strtoul(szValue, NULL, 10);
	GetPrivateProfileString("Interface", "ThPriority", NULL, szValue, 20, szIniFile);
    Conf.ThPriority = strtoul(szValue, NULL, 10);
	GetPrivateProfileString("Interface", "PluginsDir", NULL, szValue, g_MaxPath, szIniFile);
	strcpy(Conf.PluginsDir, szValue);
	GetPrivateProfileString("Interface", "BiosDir", NULL, szValue, g_MaxPath, szIniFile);
	strcpy(Conf.BiosDir, szValue);
	GetPrivateProfileString("Interface", "Mcd1", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.Mcd1, szValue);
	GetPrivateProfileString("Interface", "Mcd2", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.Mcd2, szValue); 
	Conf.CustomFps					=	GetPrivateProfileInt("Interface", "CustomFps", 0, szIniFile);
	Conf.CustomFrameSkip			=	GetPrivateProfileInt("Interface", "CustomFrameskip", 0, szIniFile);
	Conf.CustomConsecutiveFrames	=	GetPrivateProfileInt("Interface", "CustomConsecutiveFrames", 0, szIniFile);
	Conf.CustomConsecutiveSkip		=	GetPrivateProfileInt("Interface", "CustomConsecutiveSkip", 0, szIniFile);

	//plugins
	GetPrivateProfileString("Plugins", "GS", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.GS, szValue); 
    GetPrivateProfileString("Plugins", "SPU2", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.SPU2, szValue);
	GetPrivateProfileString("Plugins", "CDVD", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.CDVD, szValue);
	GetPrivateProfileString("Plugins", "PAD1", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.PAD1, szValue);
	GetPrivateProfileString("Plugins", "PAD2", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.PAD2, szValue);
	GetPrivateProfileString("Plugins", "DEV9", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.DEV9, szValue);
	GetPrivateProfileString("Plugins", "USB", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.USB, szValue);
	GetPrivateProfileString("Plugins", "FW", NULL, szValue, g_MaxPath, szIniFile);
    strcpy(Conf.FW, szValue);
	//cpu
	GetPrivateProfileString("Cpu Options", "Options", NULL, szValue, 20, szIniFile);
    Conf.Options= (u32)strtoul(szValue, NULL, 10);

	if ( GetPrivateProfileString("Cpu Options", "sseMXCSR", NULL, szValue, 20, szIniFile) ) {
		Conf.sseMXCSR = strtoul(szValue, NULL, 0);
		g_sseMXCSR = Conf.sseMXCSR;
	}
	else Config.sseMXCSR = g_sseMXCSR;

	if ( GetPrivateProfileString("Cpu Options", "sseVUMXCSR", NULL, szValue, 20, szIniFile) ) {
		Conf.sseVUMXCSR = strtoul(szValue, NULL, 0);
		g_sseVUMXCSR = Conf.sseVUMXCSR;
	}
	else Config.sseVUMXCSR = g_sseVUMXCSR;

	GetPrivateProfileString("Cpu Options", "eeOptions", NULL, szValue, 20, szIniFile);
	Conf.eeOptions = strtoul(szValue, NULL, 0);
	GetPrivateProfileString("Cpu Options", "vuOptions", NULL, szValue, 20, szIniFile);
	Conf.vuOptions = strtoul(szValue, NULL, 0);

	//Misc
	GetPrivateProfileString("Misc", "Patch", NULL, szValue, 20, szIniFile);
    Conf.Patch = !!strtoul(szValue, NULL, 10);

#ifdef PCSX2_DEVBUILD
	GetPrivateProfileString("Misc", "varLog", NULL, szValue, 20, szIniFile);
    varLog = strtoul(szValue, NULL, 16);
#endif
	GetPrivateProfileString("Misc", "Hacks", NULL, szValue, 20, szIniFile);
    Conf.Hacks = strtoul(szValue, NULL, 0);
	GetPrivateProfileString("Misc", "GameFixes", NULL, szValue, 20, szIniFile);
    Conf.GameFixes = strtoul(szValue, NULL, 0);

	// Remove Fast Branches hack for now:
	Conf.Hacks &= ~0x80;

#ifdef ENABLE_NLS
	{
		char text[256];
		extern int _nl_msg_cat_cntr;
		sprintf_s(text, 256, "LANGUAGE=%s", Conf.Lang);
		gettext_putenv(text);
	}
#endif

	return 0;
}

/////////////////////////////////////////////////////////

void SaveConfig()
{
	const PcsxConfig& Conf = Config;
	char szIniFile[g_MaxPath], szValue[g_MaxPath];

	//GetModuleFileName(GetModuleHandle((LPCSTR)gApp.hInstance), szIniFile, 256);
	//szTemp = strrchr(szIniFile, '\\');

	GetConfigFilename( szIniFile );

	// This should never be true anyway since long pathnames would have in theory
	// been caught earlier by LoadConfig -- but no harm in being safe.
	if( g_Error_PathTooLong ) return;

    //interface
    sprintf(szValue,"%s",Conf.Bios);
    WritePrivateProfileString("Interface","Bios",szValue,szIniFile);
    sprintf(szValue,"%s",Conf.Lang);
    WritePrivateProfileString("Interface","Lang",szValue,szIniFile);
    sprintf(szValue,"%s",Conf.PluginsDir);
    WritePrivateProfileString("Interface","PluginsDir",szValue,szIniFile);
    sprintf(szValue,"%s",Conf.BiosDir);
    WritePrivateProfileString("Interface","BiosDir",szValue,szIniFile);
    sprintf(szValue,"%u",(int)Conf.PsxOut);
    WritePrivateProfileString("Interface","Ps2Out",szValue,szIniFile);
	sprintf(szValue,"%u",(int)Conf.Profiler);
    WritePrivateProfileString("Interface","Profiler",szValue,szIniFile);
    sprintf(szValue,"%u",Conf.ThPriority);
	WritePrivateProfileString("Interface","ThPriority",szValue,szIniFile);
    sprintf(szValue,"%s",Conf.Mcd1);
    WritePrivateProfileString("Interface","Mcd1",szValue,szIniFile);
    sprintf(szValue,"%s",Conf.Mcd2);
    WritePrivateProfileString("Interface","Mcd2",szValue,szIniFile);
    sprintf(szValue,"%d",Conf.CustomFps);
	WritePrivateProfileString("Interface", "CustomFps", szValue, szIniFile);
	sprintf(szValue,"%d",Conf.CustomFrameSkip);
	WritePrivateProfileString("Interface", "CustomFrameskip", szValue, szIniFile);
	sprintf(szValue,"%d",Conf.CustomConsecutiveFrames);
	WritePrivateProfileString("Interface", "CustomConsecutiveFrames", szValue, szIniFile);
	sprintf(szValue,"%d",Conf.CustomConsecutiveSkip);
	WritePrivateProfileString("Interface", "CustomConsecutiveSkip", szValue, szIniFile);

	// Plugins are saved from the winConfig struct.
	// It contains the user config settings and not the
	// runtime cmdline overrides.

	sprintf(szValue,"%s",winConfig.GS);
    WritePrivateProfileString("Plugins","GS",szValue,szIniFile);
	sprintf(szValue,"%s",winConfig.SPU2);
    WritePrivateProfileString("Plugins","SPU2",szValue,szIniFile);
    sprintf(szValue,"%s",winConfig.CDVD);
    WritePrivateProfileString("Plugins","CDVD",szValue,szIniFile);
    sprintf(szValue,"%s",winConfig.PAD1);
    WritePrivateProfileString("Plugins","PAD1",szValue,szIniFile);
    sprintf(szValue,"%s",winConfig.PAD2);
    WritePrivateProfileString("Plugins","PAD2",szValue,szIniFile);
    sprintf(szValue,"%s",winConfig.DEV9);
    WritePrivateProfileString("Plugins","DEV9",szValue,szIniFile);
    sprintf(szValue,"%s",winConfig.USB);
    WritePrivateProfileString("Plugins","USB",szValue,szIniFile);
	sprintf(szValue,"%s",winConfig.FW);
    WritePrivateProfileString("Plugins","FW",szValue,szIniFile);

	//cpu
    sprintf(szValue,"%u", Conf.Options);
    WritePrivateProfileString("Cpu Options","Options",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.sseMXCSR);
    WritePrivateProfileString("Cpu Options","sseMXCSR",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.sseVUMXCSR);
    WritePrivateProfileString("Cpu Options","sseVUMXCSR",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.eeOptions);
    WritePrivateProfileString("Cpu Options","eeOptions",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.vuOptions);
    WritePrivateProfileString("Cpu Options","vuOptions",szValue,szIniFile);

	//Misc
	sprintf(szValue,"%u",(int)Conf.Patch);
    WritePrivateProfileString("Misc","Patch",szValue,szIniFile);
	sprintf(szValue,"%x",varLog);
    WritePrivateProfileString("Misc","varLog",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.Hacks);
    WritePrivateProfileString("Misc","Hacks",szValue,szIniFile);
	sprintf(szValue,"%u",Conf.GameFixes);
    WritePrivateProfileString("Misc","GameFixes",szValue,szIniFile);

}

