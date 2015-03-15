/*  USBnull
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

#include "USB.h"

static ConfigLogCombination clc;

EXPORT_C_(void) USBsetLogDir(const char *dir)
{
	clc.SetLoggingFolder(dir);
	clc.ReloadLog();
}

EXPORT_C_(s32) USBinit()
{
	clc.SetName("usbnull");
	clc.InitLog();
	clc.Log.WriteLn("usbnull plugin version");
	clc.Log.WriteLn("Initializing usbnull");

	return 0;
}

EXPORT_C_(void) USBshutdown()
{
	clc.Log.Close();
}

EXPORT_C_(s32) USBopen(void *pDsp)
{
	clc.Log.WriteLn("Opening USBnull.");

	return 0;
}

EXPORT_C_(void) USBclose()
{
	clc.Log.WriteLn("Closing USBnull.");
}

EXPORT_C_(u8) USBread8(u32 addr)
{
	clc.Log.WriteLn("*(USBnull) 8 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(u16) USBread16(u32 addr)
{
	clc.Log.WriteLn("(USBnull) 16 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(u32) USBread32(u32 addr)
{
	clc.Log.WriteLn("(USBnull) 32 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(void) USBwrite8(u32 addr, u8 value)
{
	clc.Log.WriteLn("(USBnull) 8 bit write at address %lx value %x", addr, value);
}

EXPORT_C_(void) USBwrite16(u32 addr, u16 value)
{
	clc.Log.WriteLn("(USBnull) 16 bit write at address %lx value %x", addr, value);
}

EXPORT_C_(void) USBwrite32(u32 addr, u32 value)
{
	clc.Log.WriteLn("(USBnull) 32 bit write at address %lx value %x", addr, value);
}

EXPORT_C_(void) USBirqCallback(USBcallback callback)
{
}

static int _USBirqHandler(void)
{
	return 0;
}

EXPORT_C_(USBhandler) USBirqHandler(void)
{

	return (USBhandler)_USBirqHandler;
}

EXPORT_C_(void) USBsetRAM(void *mem)
{
	clc.Log.WriteLn("*Setting ram.");
}

EXPORT_C_(void) USBsetSettingsDir(const char *dir)
{
	clc.SetConfigFolder(dir);
}

EXPORT_C_(s32) USBfreeze(int mode, freezeData *data)
{
	return 0;
}

EXPORT_C_(void) USBasync(u32 cycles)
{
}

EXPORT_C_(s32) USBtest()
{

	return 0;
}

EXPORT_C_(void) USBabout()
{
	clc.AboutGUI();
}

EXPORT_C_(void) USBconfigure()
{
	clc.ConfigureGUI();
}
