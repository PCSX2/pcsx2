/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    flac.h

    FLAC compression wrappers

***************************************************************************/

#pragma once

#ifndef __FLAC_H__
#define __FLAC_H__

#include <stdint.h>

/***************************************************************************
 *  TYPE DEFINITIONS
 ***************************************************************************
 */

typedef struct _flac_decoder flac_decoder;
struct _flac_decoder {
		/* output state */
	void *                  decoder;				/* actual encoder */
	uint32_t                sample_rate;			/* decoded sample rate */
	uint8_t                 channels;				/* decoded number of channels */
	uint8_t                 bits_per_sample;		/* decoded bits per sample */
	uint32_t                compressed_offset;		/* current offset in compressed data */
	const uint8_t *         compressed_start;		/* start of compressed data */
	uint32_t                compressed_length;		/* length of compressed data */
	const uint8_t *         compressed2_start;		/* start of compressed data */
	uint32_t                compressed2_length;		/* length of compressed data */
	int16_t *               uncompressed_start[8];	/* pointer to start of uncompressed data (up to 8 streams) */
	uint32_t                uncompressed_offset;	/* current position in uncompressed data */
	uint32_t                uncompressed_length;	/* length of uncompressed data */
	int                    	uncompressed_swap;		/* swap uncompressed sample data */
	uint8_t                 custom_header[0x2a];	/* custom header */
};

/* ======================> flac_decoder */

int 		flac_decoder_init(flac_decoder* decoder);
void 		flac_decoder_free(flac_decoder* decoder);
int 		flac_decoder_reset(flac_decoder* decoder, uint32_t sample_rate, uint8_t num_channels, uint32_t block_size, const void *buffer, uint32_t length);
int 		flac_decoder_decode_interleaved(flac_decoder* decoder, int16_t *samples, uint32_t num_samples, int swap_endian);
uint32_t 	flac_decoder_finish(flac_decoder* decoder);

#endif /* __FLAC_H__ */
