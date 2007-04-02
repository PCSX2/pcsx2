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

#ifndef __IR5900LOADSTORE_H__
#define __IR5900LOADSTORE_H__

#include "Common.h"
#include "InterTables.h"

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/

void recLB( void );
void recLBU( void );
void recLH( void );
void recLHU( void );
void recLW( void );
void recLWU( void );
void recLWL( void );
void recLWR( void );
void recLD( void );
void recLDR( void );
void recLDL( void );
void recLQ( void );
void recSB( void );
void recSH( void );
void recSW( void );
void recSWL( void );
void recSWR( void );
void recSD( void );
void recSQ( void );
void recSDL( void );
void recSDR( void );
void recLWC1( void );
void recSWC1( void );
void recLQC2( void );
void recSQC2( void );

#endif
