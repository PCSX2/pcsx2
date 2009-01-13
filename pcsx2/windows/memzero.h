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

// This is an implementation of the memzero_air fast memset routine (for zero-clears only).
// It uses templates so that it generates very efficient and compact inline code for clears.

template< size_t bytes >
static __forceinline void memzero_air( void *dest )
{
	if( bytes == 0 ) return;

	u64 _xmm_backup[2];

	enum half_local
	{
		remainder = bytes & 127,
		bytes128 = bytes / 128
	};

	// Initial check -- if the length is not a multiple of 16 then fall back on
	// using rep movsd methods.  Handling these unaligned writes in a more efficient
	// manner isn't necessary in pcsx2.

	if( (bytes & 0xf) == 0 )
	{
		if( ((uptr)dest & 0xf) != 0 )
		{
			// UNALIGNED COPY MODE.
			// For unaligned copies we have a threshold of at least 128 vectors.  Anything
			// less and it's probably better off just falling back on the rep movsd.
			if( bytes128 >128 )
			{
				__asm
				{
					movups _xmm_backup,xmm0;
					mov eax,bytes128
					mov ecx,dest
					pxor xmm0,xmm0

					align 16

				_loop_6:
					movups [ecx],xmm0;
					movups [ecx+0x10],xmm0;
					movups [ecx+0x20],xmm0;
					movups [ecx+0x30],xmm0;
					movups [ecx+0x40],xmm0;
					movups [ecx+0x50],xmm0;
					movups [ecx+0x60],xmm0;
					movups [ecx+0x70],xmm0;
					sub ecx,-128
					dec eax;
					jnz _loop_6;
				}
				if( remainder != 0 )
				{
					// Copy the remainder in reverse (using the decrementing eax as our indexer)
					__asm
					{
						mov eax, remainder

					_loop_5:
						movups [ecx+eax],xmm0;
						sub eax,16;
						jnz _loop_5;
					}
				}
				__asm
				{
					movups xmm0,[_xmm_backup];
				}
				return;
			}
		}
		else if( bytes128 > 48 )
		{
			// ALIGNED COPY MODE
			// Data is aligned and the size of data is large enough to merit a nice
			// fancy chunk of unrolled goodness:

			__asm
			{
				movups _xmm_backup,xmm0;
				mov eax,bytes128
				mov ecx,dest
				pxor xmm0,xmm0

				align 16

			_loop_8:
				movaps [ecx],xmm0;
				movaps [ecx+0x10],xmm0;
				movaps [ecx+0x20],xmm0;
				movaps [ecx+0x30],xmm0;
				movaps [ecx+0x40],xmm0;
				movaps [ecx+0x50],xmm0;
				movaps [ecx+0x60],xmm0;
				movaps [ecx+0x70],xmm0;
				sub ecx,-128
				dec eax;
				jnz _loop_8;
			}
			if( remainder != 0 )
			{
				// Copy the remainder in reverse (using the decrementing eax as our indexer)
				__asm
				{
					mov eax, remainder

				_loop_10:
					movaps [ecx+eax],xmm0;
					sub eax,16;
					jnz _loop_10;
				}
			}
			__asm
			{
				movups xmm0,[_xmm_backup];
			}
			return;
		}
	}

	// This function only works on 32-bit alignments.
	jASSUME( (bytes & 0x3) == 0 );
	jASSUME( ((uptr)dest & 0x3) == 0 );

	enum __local
	{
		remdat = bytes>>2
	};

	// This case statement handles 5 special-case sizes (small blocks)
	// in addition to the generic large block.

	switch( remdat )
	{
		case 1:
			__asm
			{
				mov edi, dest
				xor eax, eax
				mov edi, eax
			}
		return;

		case 2:
			_asm
			{
				mov edi, dest
				xor eax, eax
				stosd
				stosd
			}
		return;

		case 3:
			_asm
			{
				mov edi, dest
				xor eax, eax
				stosd
				stosd
				stosd
			}
		return;

		case 4:
			_asm
			{
				mov edi, dest
				xor eax, eax
				stosd
				stosd
				stosd
				stosd
			}
		return;

		case 5:
			_asm
			{
				mov edi, dest
				xor eax, eax
				stosd
				stosd
				stosd
				stosd
				stosd
			}
		return;

		default:
			__asm
			{
				mov ecx, remdat
				mov edi, dest
				xor eax, eax
				rep stosd
			}
		return;
	}
}

template< u8 data, size_t bytes >
static __forceinline void memset_8( void *dest )
{
	if( bytes == 0 ) return;

	//u64 _xmm_backup[2];

	/*static const size_t remainder = bytes & 127;
	static const size_t bytes128 = bytes / 128;
	if( bytes128 > 32 )
	{
		// This function only works on 128-bit alignments.
		jASSUME( (bytes & 0xf) == 0 );
		jASSUME( ((uptr)dest & 0xf) == 0 );

		__asm
		{
			movups _xmm_backup,xmm0;
			mov eax,bytes128
			mov ecx,dest
			movss xmm0,data

			align 16

		_loop_8:
			movaps [ecx],xmm0;
			movaps [ecx+0x10],xmm0;
			movaps [ecx+0x20],xmm0;
			movaps [ecx+0x30],xmm0;
			movaps [ecx+0x40],xmm0;
			movaps [ecx+0x50],xmm0;
			movaps [ecx+0x60],xmm0;
			movaps [ecx+0x70],xmm0;
			sub ecx,-128
			dec eax;
			jnz _loop_8;
		}
		if( remainder != 0 )
		{
			// Copy the remainder in reverse (using the decrementing eax as our indexer)
			__asm
			{
				mov eax, remainder

			_loop_10:
				movaps [ecx+eax],xmm0;
				sub eax,16;
				jnz _loop_10;
			}
		}
		__asm
		{
			movups xmm0,[_xmm_backup];
		}
	}
	else*/
	{
		// This function only works on 32-bit alignments of data copied.
		jASSUME( (bytes & 0x3) == 0 );

		enum local
		{
			remdat = bytes>>2,
			data32 = data + (data<<8) + (data<<16) + (data<<24)
		};

		__asm
		{
			mov eax, data32
			mov ecx, remdat
			mov edi, dest
			rep stosd
		}
	}
}

template< u16 data, size_t bytes >
static __forceinline void memset_16( void *dest )
{
	if( bytes == 0 ) return;

	//u64 _xmm_backup[2];

	{
		// This function only works on 32-bit alignments of data copied.
		jASSUME( (bytes & 0x3) == 0 );

		enum local
		{
			remdat = bytes>>2,
			data32 = data + (data<<16)
		};
		__asm
		{
			mov eax, data32
			mov ecx, remdat
			mov edi, dest
			rep stosd
		}
	}
}

template< u32 data, size_t bytes >
static __forceinline void memset_32( void *dest )
{
	if( bytes == 0 ) return;

	//u64 _xmm_backup[2];

	{
		// This function only works on 32-bit alignments of data copied.
		jASSUME( (bytes & 0x3) == 0 );

		enum local
		{
			remdat = bytes>>2,
			data32 = data
		};
		__asm
		{
			mov eax, data32
			mov ecx, remdat
			mov edi, dest
			rep stosd
		}
	}
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