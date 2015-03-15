/*  PadNull
 *  Copyright (C) 2004-2015 PCSX2 Dev Team
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

#include "Pad.h"

EXPORT_C_(void) PADsetSettingsDir(const char *dir)
{
}

EXPORT_C_(void) PADsetLogDir(const char *dir)
{
}

EXPORT_C_(s32) PADinit(u32 flags)
{
	return 0;
}

EXPORT_C_(void) PADshutdown()
{
}

EXPORT_C_(s32) PADopen(void *pDsp)
{
	return 0;
}

EXPORT_C_(void) PADclose()
{
}

EXPORT_C_(keyEvent *) PADkeyEvent()
{

	return NULL;
}

EXPORT_C_(u8) PADstartPoll(int pad)
{
	return 0;
}

EXPORT_C_(u8) PADpoll(u8 value)
{
	return 0;
}

EXPORT_C_(u32) PADquery()
{
	return 3;
}

EXPORT_C_(void) PADupdate(int pad)
{
}

EXPORT_C_(void) PADgsDriverInfo(GSdriverInfo *info)
{
}

EXPORT_C_(s32) PADfreeze(int mode, freezeData *data)
{
	return 0;
}

EXPORT_C_(s32) PADtest()
{
	return 0;
}

EXPORT_C_(void) PADabout()
{
}

EXPORT_C_(void) PADconfigure()
{
}
