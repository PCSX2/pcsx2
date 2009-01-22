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

#ifndef _PCSX2_MEMZERO_H_
#define _PCSX2_MEMZERO_H_

// This stubs out that memzero Windows specific stuff Air seems to have added
// all over, to allow Linux to compile. I may actually try to translate the file at 
// some point, but for now, lets just use memset.

template< size_t bytes >
static __forceinline void memzero_air( void *dest )
{
	memset(dest, 0, bytes);
}

template< u8 data, size_t bytes >
static __forceinline void memset_8( void *dest )
{
	memset(dest, data, bytes);
}

template< u16 data, size_t bytes >
static __forceinline void memset_16( void *dest )
{
	memset(dest, data, bytes);
}

template< u32 data, size_t bytes >
static __forceinline void memset_32( void *dest )
{
	memset(dest, data, bytes);
}

// This method can clear any object-like entity -- which is anything that is not a pointer.
// Structures, static arrays, etc.  No need to include sizeof() crap, this does it automatically
// for you!
template< typename T >
static __forceinline void memzero_obj( T& object )
{
	memzero_air<sizeof(T)>( &object );
}

template< uint data, typename T >
static __forceinline void memset_obj( T& object )
{
	if( data <= 0xff )
		memset_8<(u8)data, sizeof(T)>( &object );
	else if( data <= 0xffff )
		memset_16<(u16)data, sizeof(T)>( &object );
	else
		memset_32<(u32)data, sizeof(T)>( &object );
}

#endif