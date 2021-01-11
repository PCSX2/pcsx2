/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    flac.c

    FLAC compression wrappers

***************************************************************************/

#include <assert.h>
#include <string.h>

#include "flac.h"

/***************************************************************************
 *  FLAC DECODER
 ***************************************************************************
 */

static FLAC__StreamDecoderReadStatus flac_decoder_read_callback_static(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
FLAC__StreamDecoderReadStatus flac_decoder_read_callback(void* client_data, FLAC__byte buffer[], size_t *bytes);
static void flac_decoder_metadata_callback_static(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static FLAC__StreamDecoderTellStatus flac_decoder_tell_callback_static(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderWriteStatus flac_decoder_write_callback_static(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
FLAC__StreamDecoderWriteStatus flac_decoder_write_callback(void* client_data, const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
static void flac_decoder_error_callback_static(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

/* getters (valid after reset) */
static uint32_t sample_rate(flac_decoder *decoder)  { return decoder->sample_rate; }
static uint8_t channels(flac_decoder *decoder)  { return decoder->channels; }
static uint8_t bits_per_sample(flac_decoder *decoder) { return decoder->bits_per_sample; }
static uint32_t total_samples(flac_decoder *decoder)  { return FLAC__stream_decoder_get_total_samples(decoder->decoder); }
static FLAC__StreamDecoderState state(flac_decoder *decoder) { return FLAC__stream_decoder_get_state(decoder->decoder); }
static const char *state_string(flac_decoder *decoder) { return FLAC__stream_decoder_get_resolved_state_string(decoder->decoder); }

/*-------------------------------------------------
 *  flac_decoder - constructor
 *-------------------------------------------------
 */

void flac_decoder_init(flac_decoder *decoder)
{
	decoder->decoder = FLAC__stream_decoder_new();
	decoder->sample_rate = 0;
	decoder->channels = 0;
	decoder->bits_per_sample = 0;
	decoder->compressed_offset = 0;
	decoder->compressed_start = NULL;
	decoder->compressed_length = 0;
	decoder->compressed2_start = NULL;
	decoder->compressed2_length = 0;
	decoder->uncompressed_offset = 0;
	decoder->uncompressed_length = 0;
	decoder->uncompressed_swap = 0;
}

/*-------------------------------------------------
 *  flac_decoder - destructor
 *-------------------------------------------------
 */

void flac_decoder_free(flac_decoder* decoder)
{
	if ((decoder != NULL) && (decoder->decoder != NULL))
		FLAC__stream_decoder_delete(decoder->decoder);
}

/*-------------------------------------------------
 *  reset - reset state with the original
 *  parameters
 *-------------------------------------------------
 */

static int flac_decoder_internal_reset(flac_decoder* decoder)
{
	decoder->compressed_offset = 0;
	if (FLAC__stream_decoder_init_stream(decoder->decoder,
				&flac_decoder_read_callback_static,
				NULL,
				&flac_decoder_tell_callback_static,
				NULL,
				NULL,
				&flac_decoder_write_callback_static,
				&flac_decoder_metadata_callback_static,
				&flac_decoder_error_callback_static, decoder) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		return 0;
	return FLAC__stream_decoder_process_until_end_of_metadata(decoder->decoder);
}

/*-------------------------------------------------
 *  reset - reset state with new memory parameters
 *  and a custom-generated header
 *-------------------------------------------------
 */

int flac_decoder_reset(flac_decoder* decoder, uint32_t sample_rate, uint8_t num_channels, uint32_t block_size, const void *buffer, uint32_t length)
{
	/* modify the template header with our parameters */
	static const uint8_t s_header_template[0x2a] =
	{
		0x66, 0x4C, 0x61, 0x43,                         /* +00: 'fLaC' stream header */
		0x80,                                           /* +04: metadata block type 0 (STREAMINFO), */
								/*      flagged as last block */
		0x00, 0x00, 0x22,                               /* +05: metadata block length = 0x22 */
		0x00, 0x00,                                     /* +08: minimum block size */
		0x00, 0x00,                                     /* +0A: maximum block size */
		0x00, 0x00, 0x00,                               /* +0C: minimum frame size (0 == unknown) */
		0x00, 0x00, 0x00,                               /* +0F: maximum frame size (0 == unknown) */
		0x0A, 0xC4, 0x42, 0xF0, 0x00, 0x00, 0x00, 0x00, /* +12: sample rate (0x0ac44 == 44100), */
								/*      numchannels (2), sample bits (16), */
								/*      samples in stream (0 == unknown) */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* +1A: MD5 signature (0 == none) */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* +2A: start of stream data */
	};
	memcpy(decoder->custom_header, s_header_template, sizeof(s_header_template));
	decoder->custom_header[0x08] = decoder->custom_header[0x0a] = block_size >> 8;
	decoder->custom_header[0x09] = decoder->custom_header[0x0b] = block_size & 0xff;
	decoder->custom_header[0x12] = sample_rate >> 12;
	decoder->custom_header[0x13] = sample_rate >> 4;
	decoder->custom_header[0x14] = (sample_rate << 4) | ((num_channels - 1) << 1);

	/* configure the header ahead of the provided buffer */
	decoder->compressed_start = (const FLAC__byte *)(decoder->custom_header);
	decoder->compressed_length = sizeof(decoder->custom_header);
	decoder->compressed2_start = (const FLAC__byte *)(buffer);
	decoder->compressed2_length = length;
	return flac_decoder_internal_reset(decoder);
}

/*-------------------------------------------------
 *  decode_interleaved - decode to an interleaved
 *  sound stream
 *-------------------------------------------------
 */

int flac_decoder_decode_interleaved(flac_decoder* decoder, int16_t *samples, uint32_t num_samples, int swap_endian)
{
	/* configure the uncompressed buffer */
	memset(decoder->uncompressed_start, 0, sizeof(decoder->uncompressed_start));
	decoder->uncompressed_start[0] = samples;
	decoder->uncompressed_offset = 0;
	decoder->uncompressed_length = num_samples;
	decoder->uncompressed_swap = swap_endian;

	/* loop until we get everything we want */
	while (decoder->uncompressed_offset < decoder->uncompressed_length)
		if (!FLAC__stream_decoder_process_single(decoder->decoder))
			return 0;
	return 1;
}

#if 0
/*-------------------------------------------------
 *  decode - decode to an multiple independent
 *  data streams
 *-------------------------------------------------
 */

bool flac_decoder::decode(int16_t **samples, uint32_t num_samples, bool swap_endian)
{
	/* make sure we don't have too many channels */
	int chans = channels();
	if (chans > ARRAY_LENGTH(m_uncompressed_start))
		return false;

	/* configure the uncompressed buffer */
	memset(m_uncompressed_start, 0, sizeof(m_uncompressed_start));
	for (int curchan = 0; curchan < chans; curchan++)
		m_uncompressed_start[curchan] = samples[curchan];
	m_uncompressed_offset = 0;
	m_uncompressed_length = num_samples;
	m_uncompressed_swap = swap_endian;

	/* loop until we get everything we want */
	while (m_uncompressed_offset < m_uncompressed_length)
		if (!FLAC__stream_decoder_process_single(m_decoder))
			return false;
	return true;
}
#endif

/*-------------------------------------------------
 *  finish - finish up the decode
 *-------------------------------------------------
 */

uint32_t flac_decoder_finish(flac_decoder* decoder)
{
	/* get the final decoding position and move forward */
	FLAC__uint64 position = 0;
	FLAC__stream_decoder_get_decode_position(decoder->decoder, &position);
	FLAC__stream_decoder_finish(decoder->decoder);

	/* adjust position if we provided the header */
	if (position == 0)
		return 0;
	if (decoder->compressed_start == (const FLAC__byte *)(decoder->custom_header))
		position -= decoder->compressed_length;
	return position;
}

/*-------------------------------------------------
 *  read_callback - handle reads from the input
 *  stream
 *-------------------------------------------------
 */

#define MIN(x, y) ((x) < (y) ? (x) : (y))

FLAC__StreamDecoderReadStatus flac_decoder_read_callback_static(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	return flac_decoder_read_callback(client_data, buffer, bytes);
}

FLAC__StreamDecoderReadStatus flac_decoder_read_callback(void* client_data, FLAC__byte buffer[], size_t *bytes)
{
	flac_decoder* decoder = (flac_decoder*)client_data;

	uint32_t expected = *bytes;

	/* copy from primary buffer first */
	uint32_t outputpos = 0;
	if (outputpos < *bytes && decoder->compressed_offset < decoder->compressed_length)
	{
		uint32_t bytes_to_copy = MIN(*bytes - outputpos, decoder->compressed_length - decoder->compressed_offset);
		memcpy(&buffer[outputpos], decoder->compressed_start + decoder->compressed_offset, bytes_to_copy);
		outputpos += bytes_to_copy;
		decoder->compressed_offset += bytes_to_copy;
	}

	/* once we're out of that, copy from the secondary buffer */
	if (outputpos < *bytes && decoder->compressed_offset < decoder->compressed_length + decoder->compressed2_length)
	{
		uint32_t bytes_to_copy = MIN(*bytes - outputpos, decoder->compressed2_length - (decoder->compressed_offset - decoder->compressed_length));
		memcpy(&buffer[outputpos], decoder->compressed2_start + decoder->compressed_offset - decoder->compressed_length, bytes_to_copy);
		outputpos += bytes_to_copy;
		decoder->compressed_offset += bytes_to_copy;
	}
	*bytes = outputpos;

	/* return based on whether we ran out of data */
	return (*bytes < expected) ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

/*-------------------------------------------------
 *  metadata_callback - handle STREAMINFO metadata
 *-------------------------------------------------
 */

void flac_decoder_metadata_callback_static(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	flac_decoder *fldecoder;
	/* ignore all but STREAMINFO metadata */
	if (metadata->type != FLAC__METADATA_TYPE_STREAMINFO)
		return;

	/* parse out the data we care about */
	fldecoder = (flac_decoder *)(client_data);
	fldecoder->sample_rate = metadata->data.stream_info.sample_rate;
	fldecoder->bits_per_sample = metadata->data.stream_info.bits_per_sample;
	fldecoder->channels = metadata->data.stream_info.channels;
}

/*-------------------------------------------------
 *  tell_callback - handle requests to find out
 *  where in the input stream we are
 *-------------------------------------------------
 */

FLAC__StreamDecoderTellStatus flac_decoder_tell_callback_static(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	*absolute_byte_offset = ((flac_decoder *)client_data)->compressed_offset;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

/*-------------------------------------------------
 *  write_callback - handle writes to the output
 *  stream
 *-------------------------------------------------
 */

FLAC__StreamDecoderWriteStatus flac_decoder_write_callback_static(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	return flac_decoder_write_callback(client_data, frame, buffer);
}

FLAC__StreamDecoderWriteStatus flac_decoder_write_callback(void *client_data, const FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
	int sampnum, chan;
	int shift, blocksize;
	flac_decoder * decoder = (flac_decoder *)client_data;

	assert(frame->header.channels == channels(decoder));

	/* interleaved case */
	shift = decoder->uncompressed_swap ? 8 : 0;
	blocksize = frame->header.blocksize;
	if (decoder->uncompressed_start[1] == NULL)
	{
		int16_t *dest = decoder->uncompressed_start[0] + decoder->uncompressed_offset * frame->header.channels;
		for (sampnum = 0; sampnum < blocksize && decoder->uncompressed_offset < decoder->uncompressed_length; sampnum++, decoder->uncompressed_offset++)
			for (chan = 0; chan < frame->header.channels; chan++)
				*dest++ = (int16_t)((((uint16_t)buffer[chan][sampnum]) << shift) | (((uint16_t)buffer[chan][sampnum]) >> shift));
	}

	/* non-interleaved case */
	else
	{
		for (sampnum = 0; sampnum < blocksize && decoder->uncompressed_offset < decoder->uncompressed_length; sampnum++, decoder->uncompressed_offset++)
			for (chan = 0; chan < frame->header.channels; chan++)
				if (decoder->uncompressed_start[chan] != NULL)
					decoder->uncompressed_start[chan][decoder->uncompressed_offset] = (int16_t) ( (((uint16_t)(buffer[chan][sampnum])) << shift) | ( ((uint16_t)(buffer[chan][sampnum])) >> shift) );
	}
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/**
 * @fn  void flac_decoder::error_callback_static(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
 *
 * @brief   -------------------------------------------------
 *            error_callback - handle errors (ignore them)
 *          -------------------------------------------------.
 *
 * @param   decoder             The decoder.
 * @param   status              The status.
 * @param [in,out]  client_data If non-null, information describing the client.
 */

void flac_decoder_error_callback_static(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
}
