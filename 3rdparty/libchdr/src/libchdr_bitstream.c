/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    bitstream.c

    Helper classes for reading/writing at the bit level.

***************************************************************************/

#include <stdlib.h>
#include <libchdr/bitstream.h>

/***************************************************************************
 *  INLINE FUNCTIONS
 ***************************************************************************
 */

int bitstream_overflow(struct bitstream* bitstream) { return ((bitstream->doffset - bitstream->bits / 8) > bitstream->dlength); }

/*-------------------------------------------------
 *  create_bitstream - constructor
 *-------------------------------------------------
 */

struct bitstream* create_bitstream(const void *src, uint32_t srclength)
{
	struct bitstream* bitstream = (struct bitstream*)malloc(sizeof(struct bitstream));
	bitstream->buffer = 0;
	bitstream->bits = 0;
	bitstream->read = (const uint8_t*)src;
	bitstream->doffset = 0;
	bitstream->dlength = srclength;
	return bitstream;
}


/*-----------------------------------------------------
 *  bitstream_peek - fetch the requested number of bits
 *  but don't advance the input pointer
 *-----------------------------------------------------
 */

uint32_t bitstream_peek(struct bitstream* bitstream, int numbits)
{
	if (numbits == 0)
		return 0;

	/* fetch data if we need more */
	if (numbits > bitstream->bits)
	{
		while (bitstream->bits <= 24)
		{
			if (bitstream->doffset < bitstream->dlength)
				bitstream->buffer |= bitstream->read[bitstream->doffset] << (24 - bitstream->bits);
			bitstream->doffset++;
			bitstream->bits += 8;
		}
	}

	/* return the data */
	return bitstream->buffer >> (32 - numbits);
}


/*-----------------------------------------------------
 *  bitstream_remove - advance the input pointer by the
 *  specified number of bits
 *-----------------------------------------------------
 */

void bitstream_remove(struct bitstream* bitstream, int numbits)
{
	bitstream->buffer <<= numbits;
	bitstream->bits -= numbits;
}


/*-----------------------------------------------------
 *  bitstream_read - fetch the requested number of bits
 *-----------------------------------------------------
 */

uint32_t bitstream_read(struct bitstream* bitstream, int numbits)
{
	uint32_t result = bitstream_peek(bitstream, numbits);
	bitstream_remove(bitstream, numbits);
	return result;
}


/*-------------------------------------------------
 *  read_offset - return the current read offset
 *-------------------------------------------------
 */

uint32_t bitstream_read_offset(struct bitstream* bitstream)
{
	uint32_t result = bitstream->doffset;
	int bits = bitstream->bits;
	while (bits >= 8)
	{
		result--;
		bits -= 8;
	}
	return result;
}


/*-------------------------------------------------
 *  flush - flush to the nearest byte
 *-------------------------------------------------
 */

uint32_t bitstream_flush(struct bitstream* bitstream)
{
	while (bitstream->bits >= 8)
	{
		bitstream->doffset--;
		bitstream->bits -= 8;
	}
	bitstream->bits = bitstream->buffer = 0;
	return bitstream->doffset;
}

