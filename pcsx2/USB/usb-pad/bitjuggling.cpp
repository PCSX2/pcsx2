/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "USB/qemu-usb/vl.h"

#include <limits.h>
#include <string.h>
#include <stddef.h>

//
// http://stackoverflow.com/questions/3534535/whats-a-time-efficient-algorithm-to-copy-unaligned-bit-arrays
//

#define PREPARE_FIRST_COPY()                                                                             \
	do                                                                                                   \
	{                                                                                                    \
		if (src_len >= (CHAR_BIT - dst_offset_modulo))                                                   \
		{                                                                                                \
			*dst &= reverse_mask[dst_offset_modulo];                                                     \
			src_len -= CHAR_BIT - dst_offset_modulo;                                                     \
		}                                                                                                \
		else                                                                                             \
		{                                                                                                \
			*dst &= reverse_mask[dst_offset_modulo] | reverse_mask_xor[dst_offset_modulo + src_len + 1]; \
			c &= reverse_mask[dst_offset_modulo + src_len];                                              \
			src_len = 0;                                                                                 \
		}                                                                                                \
	} while (0)

//But copies bits in reverse?
void bitarray_copy(const uint8_t* src_org, int src_offset, int src_len,
				   uint8_t* dst_org, int dst_offset)
{
	static const unsigned char mask[] =
		{0x55, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};
	static const unsigned char reverse_mask[] =
		{0x55, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	static const unsigned char reverse_mask_xor[] =
		{0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00};

	if (src_len)
	{
		const unsigned char* src;
		unsigned char* dst;
		int src_offset_modulo,
			dst_offset_modulo;

		src = src_org + (src_offset / CHAR_BIT);
		dst = dst_org + (dst_offset / CHAR_BIT);

		src_offset_modulo = src_offset % CHAR_BIT;
		dst_offset_modulo = dst_offset % CHAR_BIT;

		if (src_offset_modulo == dst_offset_modulo)
		{
			int byte_len;
			int src_len_modulo;
			if (src_offset_modulo)
			{
				unsigned char c;

				c = reverse_mask_xor[dst_offset_modulo] & *src++;

				PREPARE_FIRST_COPY();
				*dst++ |= c;
			}

			byte_len = src_len / CHAR_BIT;
			src_len_modulo = src_len % CHAR_BIT;

			if (byte_len)
			{
				memcpy(dst, src, byte_len);
				src += byte_len;
				dst += byte_len;
			}
			if (src_len_modulo)
			{
				*dst &= reverse_mask_xor[src_len_modulo];
				*dst |= reverse_mask[src_len_modulo] & *src;
			}
		}
		else
		{
			int bit_diff_ls,
				bit_diff_rs;
			int byte_len;
			int src_len_modulo;
			unsigned char c;
			/*
             * Begin: Line things up on destination. 
             */
			if (src_offset_modulo > dst_offset_modulo)
			{
				bit_diff_ls = src_offset_modulo - dst_offset_modulo;
				bit_diff_rs = CHAR_BIT - bit_diff_ls;

				c = *src++ << bit_diff_ls;
				c |= *src >> bit_diff_rs;
				c &= reverse_mask_xor[dst_offset_modulo];
			}
			else
			{
				bit_diff_rs = dst_offset_modulo - src_offset_modulo;
				bit_diff_ls = CHAR_BIT - bit_diff_rs;

				c = *src >> bit_diff_rs &
					reverse_mask_xor[dst_offset_modulo];
			}
			PREPARE_FIRST_COPY();
			*dst++ |= c;

			/*
             * Middle: copy with only shifting the source. 
             */
			byte_len = src_len / CHAR_BIT;

			while (--byte_len >= 0)
			{
				c = *src++ << bit_diff_ls;
				c |= *src >> bit_diff_rs;
				*dst++ = c;
			}

			/*
             * End: copy the remaing bits; 
             */
			src_len_modulo = src_len % CHAR_BIT;
			if (src_len_modulo)
			{
				c = *src++ << bit_diff_ls;
				c |= *src >> bit_diff_rs;
				c &= reverse_mask[src_len_modulo];

				*dst &= reverse_mask_xor[src_len_modulo];
				*dst |= c;
			}
		}
	}
}
