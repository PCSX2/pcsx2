/*  GSnull
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string>

#include <assert.h>

#include "GS.h"
#include "gsnull_GifTransfer.h"

#define USE_GSOPEN2

ConfigLogCombination clcGS;
GSVars gs;

extern void ResetRegs();
extern void SetMultithreaded();
extern void SetFrameSkip(bool skip);
extern void InitPath();

EXPORT_C_(void) GSprintf(int timeout, char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	clcGS.Log.Write("GSprintf:%s", msg);
}

EXPORT_C_(void) GSsetSettingsDir(const char *dir)
{
	clcGS.SetConfigFolder(dir);
}

EXPORT_C_(void) GSsetLogDir(const char *dir)
{
	clcGS.SetLoggingFolder(dir);
	clcGS.ReloadLog();
}

EXPORT_C_(s32) GSinit()
{
	clcGS.SetName("gsnull");
	clcGS.InitLog();
	clcGS.Log.WriteLn("gsnull plugin version");
	clcGS.Log.WriteLn("Initializing gsnull");
	return 0;
}

EXPORT_C_(void) GSshutdown()
{
	clcGS.Log.WriteLn("Shutting down GSnull.");
	clcGS.Log.Close();
}

EXPORT_C_(s32) GSopen(void *pDsp, char *Title, int multithread)
{
	int err = 0;
	clcGS.Log.WriteLn("GS open.");

	gs.MultiThreaded = multithread;

	ResetRegs();
	SetMultithreaded();
	InitPath();
	clcGS.Log.WriteLn("Opening GSnull.");
	return err;
}

#ifdef USE_GSOPEN2
EXPORT_C_(s32) GSopen2(void *pDsp, u32 flags)
{
	clcGS.Log.WriteLn("GS open2.");

	gs.MultiThreaded = true;

	ResetRegs();
	SetMultithreaded();
	InitPath();
	clcGS.Log.WriteLn("Opening GSnull (2).");
	return 0;
}
#endif

EXPORT_C_(void) GSclose()
{
	clcGS.Log.WriteLn("Closing GSnull.");
}

EXPORT_C_(void) GSirqCallback(void (*callback)())
{
}

EXPORT_C_(s32) GSfreeze(int mode, freezeData *data)
{
	return 0;
}

EXPORT_C_(s32) GStest()
{
	clcGS.Log.WriteLn("Testing GSnull.");
	return 0;
}

EXPORT_C_(void) GSvsync(int field)
{
}

EXPORT_C_(void) GSgetLastTag(u64 *ptag)
{
	*(u32 *)ptag = gs.nPath3Hack;
	gs.nPath3Hack = 0;
}

EXPORT_C_(void) GSgifSoftReset(u32 mask)
{
	clcGS.Log.WriteLn("Doing a soft reset of the GS plugin.");
}

EXPORT_C_(void) GSreadFIFO(u64 *mem)
{
}

EXPORT_C_(void) GSreadFIFO2(u64 *mem, int qwc)
{
}

EXPORT_C_(void) GSkeyEvent(keyEvent *ev)
{
}

EXPORT_C_(void) GSchangeSaveState(int, const char *filename)
{
}

EXPORT_C_(void) GSmakeSnapshot(char *path)
{
	clcGS.Log.WriteLn("Taking a snapshot.");
}

EXPORT_C_(void) GSmakeSnapshot2(char *pathname, int *snapdone, int savejpg)
{
	clcGS.Log.WriteLn("Taking a snapshot to %s.", pathname);
}

EXPORT_C_(void) GSsetBaseMem(void *)
{
}

EXPORT_C_(void) GSsetGameCRC(int crc, int gameoptions)
{
	clcGS.Log.WriteLn("Setting the crc to '%x' with 0x%x for options.", crc, gameoptions);
}

EXPORT_C_(void) GSsetFrameSkip(int frameskip)
{
	SetFrameSkip(frameskip != 0);
	clcGS.Log.WriteLn("Frameskip set to %d.", frameskip);
}

EXPORT_C_(int) GSsetupRecording(int start, void *pData)
{
	if (start)
		clcGS.Log.WriteLn("Pretending to record.");
	else
		clcGS.Log.WriteLn("Pretending to stop recording.");

	return 1;
}

EXPORT_C_(void) GSreset()
{
	clcGS.Log.WriteLn("Doing a reset of the GS plugin.");
}

EXPORT_C_(void) GSwriteCSR(u32 value)
{
}

EXPORT_C_(void) GSgetDriverInfo(GSdriverInfo *info)
{
}

EXPORT_C_(void) GSabout()
{
	clcGS.AboutGUI();
}

EXPORT_C_(void) GSconfigure()
{
	clcGS.ConfigureGUI();
}
