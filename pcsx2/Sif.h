/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

#include "ps2/NewDmac.h"

static EE_DMAC::ChannelRegisters& sif0dma = (EE_DMAC::ChannelRegisters&)eeHw[0xc000];
static EE_DMAC::ChannelRegisters& sif1dma = (EE_DMAC::ChannelRegisters&)eeHw[0xc400];
static EE_DMAC::ChannelRegisters& sif2dma = (EE_DMAC::ChannelRegisters&)eeHw[0xc800];

extern void sifInit();
