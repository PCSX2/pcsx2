/*  DEV9null
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

#include "DEV9.h"

static ConfigLogCombination clc;

EXPORT_C_(void) DEV9setLogDir(const char *dir)
{
	clc.SetLoggingFolder(dir);
	clc.ReloadLog();
}

EXPORT_C_(s32) DEV9init()
{
	clc.SetName("dev9null");
	clc.InitLog();
	clc.Log.WriteLn("dev9null plugin version");
	clc.Log.WriteLn("Initializing dev9null");
	return 0;
}

EXPORT_C_(void) DEV9shutdown()
{
	clc.Log.WriteLn("Shutting down Dev9null.");
	clc.Log.Close();
}

EXPORT_C_(s32) DEV9open(void *pDsp)
{
	clc.Log.WriteLn("Opening Dev9null.");
	return 0;
}

EXPORT_C_(void) DEV9close()
{
	clc.Log.WriteLn("Closing Dev9null.");
}

EXPORT_C_(u8) DEV9read8(u32 addr)
{
	clc.Log.WriteLn("*Unknown 8 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(u16) DEV9read16(u32 addr)
{
	clc.Log.WriteLn("*Unknown 16 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(u32) DEV9read32(u32 addr)
{
	clc.Log.WriteLn("*Unknown 32 bit read at address %lx", addr);
	return 0;
}

EXPORT_C_(void) DEV9write8(u32 addr, u8 value)
{
	clc.Log.WriteLn("*Unknown 8 bit write; address %lx = %x", addr, value);
}

EXPORT_C_(void) DEV9write16(u32 addr, u16 value)
{
	clc.Log.WriteLn("*Unknown 16 bit write; address %lx = %x", addr, value);
}

EXPORT_C_(void) DEV9write32(u32 addr, u32 value)
{
	clc.Log.WriteLn("*Unknown 32 bit write; address %lx = %x", addr, value);
}

EXPORT_C_(s32) DEV9dmaRead(s32 channel, u32 *data, u32 bytesLeft, u32 *bytesProcessed)
{
	clc.Log.WriteLn("Reading DMA8 Mem.");
	*bytesProcessed = bytesLeft;
	return 0;
}

EXPORT_C_(s32) DEV9dmaWrite(s32 channel, u32 *data, u32 bytesLeft, u32 *bytesProcessed)
{
	clc.Log.WriteLn("Writing DMA8 Mem.");
	*bytesProcessed = bytesLeft;
	return 0;
}

EXPORT_C_(void) DEV9dmaInterrupt(s32 channel)
{
}

EXPORT_C_(void) DEV9readDMA8Mem(u32 *pMem, int size)
{
	clc.Log.WriteLn("Reading DMA8 Mem.");
}

EXPORT_C_(void) DEV9writeDMA8Mem(u32 *pMem, int size)
{
	clc.Log.WriteLn("Writing DMA8 Mem.");
}

EXPORT_C_(void) DEV9irqCallback(DEV9callback callback)
{
}

int _DEV9irqHandler(void)
{
	return 0;
}

EXPORT_C_(DEV9handler) DEV9irqHandler(void)
{
	return (DEV9handler)_DEV9irqHandler;
}

EXPORT_C_(void) DEV9setSettingsDir(const char *dir)
{
	clc.SetConfigFolder(dir);
}

EXPORT_C_(void) DEV9about()
{
	clc.AboutGUI();
}

EXPORT_C_(void) DEV9configure()
{
	clc.ConfigureGUI();
}

EXPORT_C_(s32) DEV9test()
{
	return 0;
}

EXPORT_C_(s32) DEV9freeze(int mode, freezeData *data)
{
	return 0;
}
