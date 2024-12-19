/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    bitstream.h

    Helper classes for reading/writing at the bit level.

***************************************************************************/

#pragma once

#ifndef __BITSTREAM_H__
#define __BITSTREAM_H__

#include <stdint.h>

/***************************************************************************
 *  TYPE DEFINITIONS
 ***************************************************************************
 */

/* helper class for reading from a bit buffer */
struct bitstream
{
	uint32_t          buffer;       /* current bit accumulator */
	int               bits;         /* number of bits in the accumulator */
	const uint8_t *   read;         /* read pointer */
	uint32_t          doffset;      /* byte offset within the data */
	uint32_t          dlength;      /* length of the data */
};

struct bitstream* 	create_bitstream(const void *src, uint32_t srclength);
int 				bitstream_overflow(struct bitstream* bitstream);
uint32_t 			bitstream_read_offset(struct bitstream* bitstream);

uint32_t 			bitstream_read(struct bitstream* bitstream, int numbits);
uint32_t 			bitstream_peek(struct bitstream* bitstream, int numbits);
void 				bitstream_remove(struct bitstream* bitstream, int numbits);
uint32_t 			bitstream_flush(struct bitstream* bitstream);


#endif
