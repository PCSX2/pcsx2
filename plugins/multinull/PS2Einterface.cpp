/*  Multinull
 *  Copyright (C) 2004-2015  PCSX2 Dev Team
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

#include "PS2Einterface.h"

EXPORT_C_(u32) PS2EgetLibVersion2(u32 type)
{
	switch (type)
	{
	case PS2E_LT_GS:
		return PS2E_GS_VERSION << 16;
	case PS2E_LT_PAD:
		return PS2E_PAD_VERSION << 16;
	case PS2E_LT_SPU2:
		return PS2E_SPU2_VERSION << 16;
	case PS2E_LT_CDVD:
		return PS2E_CDVD_VERSION << 16;
	case PS2E_LT_DEV9:
		return PS2E_DEV9_VERSION << 16;
	case PS2E_LT_USB:
		return PS2E_USB_VERSION << 16;
	case PS2E_LT_FW:
		return PS2E_FW_VERSION << 16;
		// 		case PS2E_LT_SIO:
		// 			return PS2E_SIO_VERSION << 16;
	}
	return 0;
}

EXPORT_C_(u32) PS2EgetLibType()
{
	return PS2E_LT_GS | PS2E_LT_PAD | PS2E_LT_SPU2 | PS2E_LT_CDVD | PS2E_LT_DEV9 | PS2E_LT_USB |
	       PS2E_LT_FW; //| PS2E_LT_SIO;
}

EXPORT_C_(const char *) PS2EgetLibName()
{
	return "Multinull";
}

ENTRY_POINT;
