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

#ifndef _LNX_MEMZERO_H_
#define _LNX_MEMZERO_H_

// This header contains non-optimized implementation of memzero_ptr and memset8, etc

template< u32 data, typename T >
static __fi void memset32( T& obj )
{
	// this function works on 32-bit aligned lengths of data only.
	// If the data length is not a factor of 32 bits, the C++ optimizing compiler will
	// probably just generate mysteriously broken code in Release builds. ;)

	jASSUME( (sizeof(T) & 0x3) == 0 );

	u32* dest = (u32*)&obj;
	for( int i=sizeof(T)>>2; i; --i, ++dest )
		*dest = data;
}

template< typename T >
static __fi void memzero( T& obj )
{
	memset( &obj, 0, sizeof( T ) );
}

template< u8 data, typename T >
static __fi void memset8( T& obj )
{
	// Aligned sizes use the optimized 32 bit inline memset.  Unaligned sizes use memset.
	if( (sizeof(T) & 0x3) != 0 )
		memset( &obj, data, sizeof( T ) );
	else {
		const u32 data32 = data + (data<<8) + (data<<16) + (data<<24);
		memset32<data32>( obj );
	}
}

// Code is only called in the init so no need to bother with ASM
template< u8 data, size_t bytes >
static __fi void memset_8( void *dest )
{
	memset(dest, data, bytes);
}

#endif
