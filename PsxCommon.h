/*  Pcsx2 - Pc Ps2 Emulator *  Copyright (C) 2002-2003  Pcsx2 Team
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

#ifndef __PSXCOMMON_H__
#define __PSXCOMMON_H__

#ifdef __WIN32__
#include <windows.h>
#endif

#include "PS2Etypes.h"

#include "System.h"
#include <zlib.h>

long LoadCdBios;
int cdOpenCase;

#include "Plugins.h"
#include "R3000A.h"
#include "PsxMem.h"
#include "PsxHw.h"
#include "PsxBios.h"
#include "PsxDma.h"
#include "PsxCounters.h"
#include "CdRom.h"
#include "Sio.h"
#include "DebugTools/Debug.h"
#include "PsxSio2.h"
#include "CDVD.h"

#endif /* __PSXCOMMON_H__ */
