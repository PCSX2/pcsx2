/***************************************************************************

    chd.c

    MAME Compressed Hunks of Data file format

****************************************************************************

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <libchdr/chd.h>
#include <libchdr/cdrom.h>
#include <libchdr/flac.h>
#include <libchdr/huffman.h>
#include <zstd.h>

#include "LzmaEnc.h"
#include "LzmaDec.h"
#if defined(__PS3__) || defined(__PSL1GHT__)
#define __MACTYPES__
#endif
#include <zlib.h>

#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

#undef MAX
#undef MIN
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SHA1_DIGEST_SIZE 20

/***************************************************************************
    DEBUGGING
***************************************************************************/

#define PRINTF_MAX_HUNK				(0)

/***************************************************************************
    CONSTANTS
***************************************************************************/

#define MAP_STACK_ENTRIES			512			/* max number of entries to use on the stack */
#define MAP_ENTRY_SIZE				16			/* V3 and later */
#define OLD_MAP_ENTRY_SIZE			8			/* V1-V2 */
#define METADATA_HEADER_SIZE		16			/* metadata header size */

#define MAP_ENTRY_FLAG_TYPE_MASK	0x0f		/* what type of hunk */
#define MAP_ENTRY_FLAG_NO_CRC		0x10		/* no CRC is present */

#define CHD_V1_SECTOR_SIZE			512			/* size of a "sector" in the V1 header */

#define COOKIE_VALUE				0xbaadf00d
#define MAX_ZLIB_ALLOCS				64

#define END_OF_LIST_COOKIE			"EndOfListCookie"

#define NO_MATCH					(~0)

#ifdef WANT_RAW_DATA_SECTOR
static const uint8_t s_cd_sync_header[12] = { 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00 };
#endif

/* V3-V4 entry types */
enum
{
	V34_MAP_ENTRY_TYPE_INVALID = 0,             /* invalid type */
	V34_MAP_ENTRY_TYPE_COMPRESSED = 1,          /* standard compression */
	V34_MAP_ENTRY_TYPE_UNCOMPRESSED = 2,        /* uncompressed data */
	V34_MAP_ENTRY_TYPE_MINI = 3,                /* mini: use offset as raw data */
	V34_MAP_ENTRY_TYPE_SELF_HUNK = 4,           /* same as another hunk in this file */
	V34_MAP_ENTRY_TYPE_PARENT_HUNK = 5,         /* same as a hunk in the parent file */
	V34_MAP_ENTRY_TYPE_2ND_COMPRESSED = 6       /* compressed with secondary algorithm (usually FLAC CDDA) */
};

/* V5 compression types */
enum
{
	/* codec #0
	 * these types are live when running */
	COMPRESSION_TYPE_0 = 0,
	/* codec #1 */
	COMPRESSION_TYPE_1 = 1,
	/* codec #2 */
	COMPRESSION_TYPE_2 = 2,
	/* codec #3 */
	COMPRESSION_TYPE_3 = 3,
	/* no compression; implicit length = hunkbytes */
	COMPRESSION_NONE = 4,
	/* same as another block in this chd */
	COMPRESSION_SELF = 5,
	/* same as a hunk's worth of units in the parent chd */
	COMPRESSION_PARENT = 6,

	/* start of small RLE run (4-bit length)
	 * these additional pseudo-types are used for compressed encodings: */
	COMPRESSION_RLE_SMALL,
	/* start of large RLE run (8-bit length) */
	COMPRESSION_RLE_LARGE,
	/* same as the last COMPRESSION_SELF block */
	COMPRESSION_SELF_0,
	/* same as the last COMPRESSION_SELF block + 1 */
	COMPRESSION_SELF_1,
	/* same block in the parent */
	COMPRESSION_PARENT_SELF,
	/* same as the last COMPRESSION_PARENT block */
	COMPRESSION_PARENT_0,
	/* same as the last COMPRESSION_PARENT block + 1 */
	COMPRESSION_PARENT_1
};

/***************************************************************************
    MACROS
***************************************************************************/

#define EARLY_EXIT(x)				do { (void)(x); goto cleanup; } while (0)

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

/* interface to a codec */
typedef struct _codec_interface codec_interface;
struct _codec_interface
{
	UINT32		compression;								/* type of compression */
	const char *compname;									/* name of the algorithm */
	UINT8		lossy;										/* is this a lossy algorithm? */
	chd_error	(*init)(void *codec, UINT32 hunkbytes);		/* codec initialize */
	void		(*free)(void *codec);						/* codec free */
	chd_error	(*decompress)(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen); /* decompress data */
	chd_error	(*config)(void *codec, int param, void *config); /* configure */
};

/* a single map entry */
typedef struct _map_entry map_entry;
struct _map_entry
{
	UINT64					offset;			/* offset within the file of the data */
	UINT32					crc;			/* 32-bit CRC of the data */
	UINT32					length;			/* length of the data */
	UINT8					flags;			/* misc flags */
};

/* a single metadata entry */
typedef struct _metadata_entry metadata_entry;
struct _metadata_entry
{
	UINT64					offset;			/* offset within the file of the header */
	UINT64					next;			/* offset within the file of the next header */
	UINT64					prev;			/* offset within the file of the previous header */
	UINT32					length;			/* length of the metadata */
	UINT32					metatag;		/* metadata tag */
	UINT8					flags;			/* flag bits */
};

/* codec-private data for the ZLIB codec */

typedef struct _zlib_allocator zlib_allocator;
struct _zlib_allocator
{
	UINT32 *				allocptr[MAX_ZLIB_ALLOCS];
	UINT32 *				allocptr2[MAX_ZLIB_ALLOCS];
};

typedef struct _zlib_codec_data zlib_codec_data;
struct _zlib_codec_data
{
	z_stream				inflater;
	zlib_allocator			allocator;
};

/* codec-private data for the LZMA codec */
#define MAX_LZMA_ALLOCS 64

typedef struct _lzma_allocator lzma_allocator;
struct _lzma_allocator
{
	void *(*Alloc)(void *p, size_t size);
 	void (*Free)(void *p, void *address); /* address can be 0 */
	void (*FreeSz)(void *p, void *address, size_t size); /* address can be 0 */
	uint32_t*	allocptr[MAX_LZMA_ALLOCS];
	uint32_t*	allocptr2[MAX_LZMA_ALLOCS];
};

typedef struct _lzma_codec_data lzma_codec_data;
struct _lzma_codec_data
{
	CLzmaDec		decoder;
	lzma_allocator	allocator;
};

typedef struct _huff_codec_data huff_codec_data;
struct _huff_codec_data
{
	struct huffman_decoder* decoder;
};

typedef struct _zstd_codec_data zstd_codec_data;
struct _zstd_codec_data
{
	ZSTD_DStream *dstream;
};

/* codec-private data for the CDZL codec */
typedef struct _cdzl_codec_data cdzl_codec_data;
struct _cdzl_codec_data {
	/* internal state */
	zlib_codec_data		base_decompressor;
#ifdef WANT_SUBCODE
	zlib_codec_data		subcode_decompressor;
#endif
	uint8_t*			buffer;
};

/* codec-private data for the CDLZ codec */
typedef struct _cdlz_codec_data cdlz_codec_data;
struct _cdlz_codec_data {
	/* internal state */
	lzma_codec_data		base_decompressor;
#ifdef WANT_SUBCODE
	zlib_codec_data		subcode_decompressor;
#endif
	uint8_t*			buffer;
};

/* codec-private data for the FLAC codec */
typedef struct _flac_codec_data flac_codec_data;
struct _flac_codec_data {
	/* internal state */
	int		native_endian;
	flac_decoder	decoder;
};

/* codec-private data for the CDFL codec */
typedef struct _cdfl_codec_data cdfl_codec_data;
struct _cdfl_codec_data {
	/* internal state */
	int		swap_endian;
	flac_decoder	decoder;
#ifdef WANT_SUBCODE
	zlib_codec_data		subcode_decompressor;
#endif
	uint8_t*	buffer;
};

typedef struct _cdzs_codec_data cdzs_codec_data;
struct _cdzs_codec_data 
{
	zstd_codec_data base_decompressor;
#ifdef WANT_SUBCODE
	zstd_codec_data subcode_decompressor;
#endif
	uint8_t*				buffer;
};

/* internal representation of an open CHD file */
struct _chd_file
{
	UINT32					cookie;			/* cookie, should equal COOKIE_VALUE */

	core_file *				file;			/* handle to the open core file */
	chd_header				header;			/* header, extracted from file */

	chd_file *				parent;			/* pointer to parent file, or NULL */

	map_entry *				map;			/* array of map entries */

#ifdef NEED_CACHE_HUNK
	UINT8 *					cache;			/* hunk cache pointer */
	UINT32					cachehunk;		/* index of currently cached hunk */

	UINT8 *					compare;		/* hunk compare pointer */
	UINT32					comparehunk;	/* index of current compare data */
#endif

	UINT8 *					compressed;		/* pointer to buffer for compressed data */
	const codec_interface *	codecintf[4];	/* interface to the codec */

	zlib_codec_data			zlib_codec_data;		/* zlib codec data */
	lzma_codec_data			lzma_codec_data;		/* lzma codec data */
	huff_codec_data			huff_codec_data;		/* huff codec data */
	flac_codec_data			flac_codec_data;		/* flac codec data */
	zstd_codec_data			zstd_codec_data;		/* zstd codec data */
	cdzl_codec_data			cdzl_codec_data;		/* cdzl codec data */
	cdlz_codec_data			cdlz_codec_data;		/* cdlz codec data */
	cdfl_codec_data			cdfl_codec_data;		/* cdfl codec data */
	cdzs_codec_data			cdzs_codec_data;		/* cdzs codec data */

#ifdef NEED_CACHE_HUNK
	UINT32					maxhunk;		/* maximum hunk accessed */
#endif

	UINT8 *					file_cache;		/* cache of underlying file */
};


/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

static const UINT8 nullmd5[CHD_MD5_BYTES] = { 0 };
static const UINT8 nullsha1[CHD_SHA1_BYTES] = { 0 };

/***************************************************************************
    PROTOTYPES
***************************************************************************/

/* core_file wrappers over stdio */
static core_file *core_stdio_fopen(char const *path);
static UINT64 core_stdio_fsize(core_file *file);
static size_t core_stdio_fread(void *ptr, size_t size, size_t nmemb, core_file *file);
static int core_stdio_fclose(core_file *file);
static int core_stdio_fclose_nonowner(core_file *file); // alternate fclose used by chd_open_file
static int core_stdio_fseek(core_file* file, INT64 offset, int whence);

/* internal header operations */
static chd_error header_validate(const chd_header *header);
static chd_error header_read(chd_file *chd, chd_header *header);

/* internal hunk read/write */
#ifdef NEED_CACHE_HUNK
static chd_error hunk_read_into_cache(chd_file *chd, UINT32 hunknum);
#endif
static chd_error hunk_read_into_memory(chd_file *chd, UINT32 hunknum, UINT8 *dest);

/* internal map access */
static chd_error map_read(chd_file *chd);

/* metadata management */
static chd_error metadata_find_entry(chd_file *chd, UINT32 metatag, UINT32 metaindex, metadata_entry *metaentry);

/* zlib compression codec */
static chd_error zlib_codec_init(void *codec, uint32_t hunkbytes);
static void zlib_codec_free(void *codec);
static chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);
static voidpf zlib_fast_alloc(voidpf opaque, uInt items, uInt size);
static void zlib_fast_free(voidpf opaque, voidpf address);
static void zlib_allocator_free(voidpf opaque);

/* lzma compression codec */
static chd_error lzma_codec_init(void *codec, uint32_t hunkbytes);
static void lzma_codec_free(void *codec);
static chd_error lzma_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* huff compression codec */
static chd_error huff_codec_init(void *codec, uint32_t hunkbytes);
static void huff_codec_free(void *codec);
static chd_error huff_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* flac compression codec */
static chd_error flac_codec_init(void *codec, uint32_t hunkbytes);
static void flac_codec_free(void *codec);
static chd_error flac_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* zstd compression codec */
static chd_error zstd_codec_init(void *codec, uint32_t hunkbytes);
static void zstd_codec_free(void *codec);
static chd_error zstd_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);


/* cdzl compression codec */
static chd_error cdzl_codec_init(void* codec, uint32_t hunkbytes);
static void cdzl_codec_free(void* codec);
static chd_error cdzl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* cdlz compression codec */
static chd_error cdlz_codec_init(void* codec, uint32_t hunkbytes);
static void cdlz_codec_free(void* codec);
static chd_error cdlz_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* cdfl compression codec */
static chd_error cdfl_codec_init(void* codec, uint32_t hunkbytes);
static void cdfl_codec_free(void* codec);
static chd_error cdfl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/* cdzs compression codec */
static chd_error cdzs_codec_init(void *codec, uint32_t hunkbytes);
static void cdzs_codec_free(void *codec);
static chd_error cdzs_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen);

/***************************************************************************
 *  LZMA ALLOCATOR HELPER
 ***************************************************************************
 */

static void *lzma_fast_alloc(void *p, size_t size);
static void lzma_fast_free(void *p, void *address);

/*-------------------------------------------------
 *  lzma_allocator_init
 *-------------------------------------------------
 */

static void lzma_allocator_init(void* p)
{
	lzma_allocator *codec = (lzma_allocator *)(p);

	/* reset pointer list */
	memset(codec->allocptr, 0, sizeof(codec->allocptr));
	memset(codec->allocptr2, 0, sizeof(codec->allocptr2));
	codec->Alloc = lzma_fast_alloc;
	codec->Free = lzma_fast_free;
}

/*-------------------------------------------------
 *  lzma_allocator_free
 *-------------------------------------------------
 */

static void lzma_allocator_free(void* p )
{
	int i;
	lzma_allocator *codec = (lzma_allocator *)(p);

	/* free our memory */
	for (i = 0 ; i < MAX_LZMA_ALLOCS ; i++)
	{
		if (codec->allocptr[i] != NULL)
			free(codec->allocptr[i]);
	}
}

/*-------------------------------------------------
 *  lzma_fast_alloc - fast malloc for lzma, which
 *  allocates and frees memory frequently
 *-------------------------------------------------
 */

/* Huge alignment values for possible SIMD optimization by compiler (NEON, SSE, AVX) */
#define LZMA_MIN_ALIGNMENT_BITS 512
#define LZMA_MIN_ALIGNMENT_BYTES (LZMA_MIN_ALIGNMENT_BITS / 8)

static void *lzma_fast_alloc(void *p, size_t size)
{
	int scan;
	uint32_t *addr        = NULL;
	lzma_allocator *codec = (lzma_allocator *)(p);
	uintptr_t vaddr = 0;

	/* compute the size, rounding to the nearest 1k */
	size = (size + 0x3ff) & ~0x3ff;

	/* reuse a hunk if we can */
	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		uint32_t *ptr = codec->allocptr[scan];
		if (ptr != NULL && size == *ptr)
		{
			/* set the low bit of the size so we don't match next time */
			*ptr |= 1;

			/* return aligned address of the block */
			return codec->allocptr2[scan];
		}
	}

	/* alloc a new one and put it into the list */
	addr = (uint32_t *)malloc(size + sizeof(uint32_t) + LZMA_MIN_ALIGNMENT_BYTES);
	if (addr==NULL)
		return NULL;
	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		if (codec->allocptr[scan] == NULL)
		{
			/* store block address */
			codec->allocptr[scan] = addr;

			/* compute aligned address, store it */
			vaddr = (uintptr_t)addr;
			vaddr = (vaddr + sizeof(uint32_t) + (LZMA_MIN_ALIGNMENT_BYTES-1)) & (~(LZMA_MIN_ALIGNMENT_BYTES-1));
			codec->allocptr2[scan] = (uint32_t*)vaddr;
			break;
		}
	}

	/* set the low bit of the size so we don't match next time */
	*addr = size | 1;

	/* return aligned address */
	return (void*)vaddr;
}

/*-------------------------------------------------
 *  lzma_fast_free - fast free for lzma, which
 *  allocates and frees memory frequently
 *-------------------------------------------------
 */

static void lzma_fast_free(void *p, void *address)
{
	int scan;
	uint32_t *ptr = NULL;
	lzma_allocator *codec = NULL;

	if (address == NULL)
		return;

	codec = (lzma_allocator *)(p);

	/* find the hunk */
	ptr = (uint32_t *)address;
	for (scan = 0; scan < MAX_LZMA_ALLOCS; scan++)
	{
		if (ptr == codec->allocptr2[scan])
		{
			/* clear the low bit of the size to allow matches */
			*codec->allocptr[scan] &= ~1;
			return;
		}
	}
}

/***************************************************************************
 *  LZMA DECOMPRESSOR
 ***************************************************************************
 */

/*-------------------------------------------------
 *  lzma_codec_init - constructor
 *-------------------------------------------------
 */

static chd_error lzma_codec_init(void* codec, uint32_t hunkbytes)
{
	CLzmaEncHandle enc;
	CLzmaEncProps encoder_props;
	Byte decoder_props[LZMA_PROPS_SIZE];
	SizeT props_size;
	lzma_allocator* alloc;
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;

	/* construct the decoder */
	LzmaDec_Construct(&lzma_codec->decoder);

	/* FIXME: this code is written in a way that makes it impossible to safely upgrade the LZMA SDK
	 * This code assumes that the current version of the encoder imposes the same requirements on the
	 * decoder as the encoder used to produce the file.  This is not necessarily true.  The format
	 * needs to be changed so the encoder properties are written to the file.

	 * configure the properties like the compressor did */
	LzmaEncProps_Init(&encoder_props);
	encoder_props.level = 9;
	encoder_props.reduceSize = hunkbytes;
	LzmaEncProps_Normalize(&encoder_props);

	/* convert to decoder properties */
	alloc = &lzma_codec->allocator;
	lzma_allocator_init(alloc);
	enc = LzmaEnc_Create((ISzAlloc*)alloc);
	if (!enc)
		return CHDERR_DECOMPRESSION_ERROR;
	if (LzmaEnc_SetProps(enc, &encoder_props) != SZ_OK)
	{
		LzmaEnc_Destroy(enc, (ISzAlloc*)&alloc, (ISzAlloc*)&alloc);
		return CHDERR_DECOMPRESSION_ERROR;
	}
	props_size = sizeof(decoder_props);
	if (LzmaEnc_WriteProperties(enc, decoder_props, &props_size) != SZ_OK)
	{
		LzmaEnc_Destroy(enc, (ISzAlloc*)alloc, (ISzAlloc*)alloc);
		return CHDERR_DECOMPRESSION_ERROR;
	}
	LzmaEnc_Destroy(enc, (ISzAlloc*)alloc, (ISzAlloc*)alloc);

	/* do memory allocations */
	if (LzmaDec_Allocate(&lzma_codec->decoder, decoder_props, LZMA_PROPS_SIZE, (ISzAlloc*)alloc) != SZ_OK)
		return CHDERR_DECOMPRESSION_ERROR;

	/* Okay */
	return CHDERR_NONE;
}

/*-------------------------------------------------
 *  lzma_codec_free
 *-------------------------------------------------
 */

static void lzma_codec_free(void* codec)
{
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;

	/* free memory */
	LzmaDec_Free(&lzma_codec->decoder, (ISzAlloc*)&lzma_codec->allocator);
	lzma_allocator_free(&lzma_codec->allocator);
}

/*-------------------------------------------------
 *  decompress - decompress data using the LZMA
 *  codec
 *-------------------------------------------------
 */

static chd_error lzma_codec_decompress(void* codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	ELzmaStatus status;
	SRes res;
	SizeT consumedlen, decodedlen;
	/* initialize */
	lzma_codec_data* lzma_codec = (lzma_codec_data*) codec;
	LzmaDec_Init(&lzma_codec->decoder);

	/* decode */
	consumedlen = complen;
	decodedlen = destlen;
	res = LzmaDec_DecodeToBuf(&lzma_codec->decoder, dest, &decodedlen, src, &consumedlen, LZMA_FINISH_END, &status);
	if ((res != SZ_OK && res != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK) || consumedlen != complen || decodedlen != destlen)
		return CHDERR_DECOMPRESSION_ERROR;
	return CHDERR_NONE;
}

/* cdlz */
static chd_error cdlz_codec_init(void* codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdlz_codec_data* cdlz = (cdlz_codec_data*) codec;

	/* allocate buffer */
	cdlz->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdlz->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	ret = lzma_codec_init(&cdlz->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zlib_codec_init(&cdlz->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	return CHDERR_NONE;
}

static void cdlz_codec_free(void* codec)
{
	cdlz_codec_data* cdlz = (cdlz_codec_data*) codec;
	free(cdlz->buffer);
	lzma_codec_free(&cdlz->base_decompressor);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdlz->subcode_decompressor);
#endif
}

static chd_error cdlz_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	uint32_t framenum;
	cdlz_codec_data* cdlz = (cdlz_codec_data*)codec;

	/* determine header bytes */
	uint32_t frames = destlen / CD_FRAME_SIZE;
	uint32_t complen_bytes = (destlen < 65536) ? 2 : 3;
	uint32_t ecc_bytes = (frames + 7) / 8;
	uint32_t header_bytes = ecc_bytes + complen_bytes;

	/* extract compressed length of base */
	uint32_t complen_base = (src[ecc_bytes + 0] << 8) | src[ecc_bytes + 1];
	if (complen_bytes > 2)
		complen_base = (complen_base << 8) | src[ecc_bytes + 2];

	/* reset and decode */
	lzma_codec_decompress(&cdlz->base_decompressor, &src[header_bytes], complen_base, &cdlz->buffer[0], frames * CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
	zlib_codec_decompress(&cdlz->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdlz->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		uint8_t *sector;

		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdlz->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdlz->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif

#ifdef WANT_RAW_DATA_SECTOR
		/* reconstitute the ECC data and sync header */
		sector = (uint8_t *)&dest[framenum * CD_FRAME_SIZE];
		if ((src[framenum / 8] & (1 << (framenum % 8))) != 0)
		{
			memcpy(sector, s_cd_sync_header, sizeof(s_cd_sync_header));
			ecc_generate(sector);
		}
#endif
	}
	return CHDERR_NONE;
}

/* cdzl */

static chd_error cdzl_codec_init(void *codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	cdzl->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdzl->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	ret = zlib_codec_init(&cdzl->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zlib_codec_init(&cdzl->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	return CHDERR_NONE;
}

static void cdzl_codec_free(void *codec)
{
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;
	zlib_codec_free(&cdzl->base_decompressor);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdzl->subcode_decompressor);
#endif
	free(cdzl->buffer);
}

static chd_error cdzl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	uint32_t framenum;
	cdzl_codec_data* cdzl = (cdzl_codec_data*)codec;

	/* determine header bytes */
	uint32_t frames = destlen / CD_FRAME_SIZE;
	uint32_t complen_bytes = (destlen < 65536) ? 2 : 3;
	uint32_t ecc_bytes = (frames + 7) / 8;
	uint32_t header_bytes = ecc_bytes + complen_bytes;

	/* extract compressed length of base */
	uint32_t complen_base = (src[ecc_bytes + 0] << 8) | src[ecc_bytes + 1];
	if (complen_bytes > 2)
		complen_base = (complen_base << 8) | src[ecc_bytes + 2];

	/* reset and decode */
	zlib_codec_decompress(&cdzl->base_decompressor, &src[header_bytes], complen_base, &cdzl->buffer[0], frames * CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
	zlib_codec_decompress(&cdzl->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdzl->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		uint8_t *sector;

		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdzl->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdzl->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif

#ifdef WANT_RAW_DATA_SECTOR
		/* reconstitute the ECC data and sync header */
		sector = (uint8_t *)&dest[framenum * CD_FRAME_SIZE];
		if ((src[framenum / 8] & (1 << (framenum % 8))) != 0)
		{
			memcpy(sector, s_cd_sync_header, sizeof(s_cd_sync_header));
			ecc_generate(sector);
		}
#endif
	}
	return CHDERR_NONE;
}

/***************************************************************************
 *  HUFFMAN DECOMPRESSOR
 ***************************************************************************
 */

static chd_error huff_codec_init(void* codec, uint32_t hunkbytes)
{
	huff_codec_data* huff_codec = (huff_codec_data*) codec;
	huff_codec->decoder = create_huffman_decoder(256, 16);
	return CHDERR_NONE;
}

static void huff_codec_free(void *codec)
{
	huff_codec_data* huff_codec = (huff_codec_data*) codec;
	delete_huffman_decoder(huff_codec->decoder);
}

static chd_error huff_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	huff_codec_data* huff_codec = (huff_codec_data*) codec;
	struct bitstream* bitbuf = create_bitstream(src, complen);

	// first import the tree
	enum huffman_error err = huffman_import_tree_huffman(huff_codec->decoder, bitbuf);
	if (err != HUFFERR_NONE)
	{
		free(bitbuf);
		return CHDERR_DECOMPRESSION_ERROR;
	}

	// then decode the data
	uint32_t cur;
	for (cur = 0; cur < destlen; cur++)
		dest[cur] = huffman_decode_one(huff_codec->decoder, bitbuf);
	bitstream_flush(bitbuf);
	chd_error result = bitstream_overflow(bitbuf) ? CHDERR_DECOMPRESSION_ERROR : CHDERR_NONE;

	free(bitbuf);
	return result;
}
 
/***************************************************************************
 *  CD FLAC DECOMPRESSOR
 ***************************************************************************
 */

/*------------------------------------------------------
 *  flac_codec_blocksize - return the optimal block size
 *------------------------------------------------------
 */

static uint32_t flac_codec_blocksize(uint32_t bytes)
{
	/* determine FLAC block size, which must be 16-65535
	 * clamp to 2k since that's supposed to be the sweet spot */
	uint32_t blocksize = bytes / 4;
	while (blocksize > 2048)
		blocksize /= 2;
	return blocksize;
}

static chd_error flac_codec_init(void *codec, uint32_t hunkbytes)
{
	uint16_t native_endian = 0;
	flac_codec_data *flac = (flac_codec_data*)codec;

	/* make sure the CHD's hunk size is an even multiple of the sample size */
	if (hunkbytes % 4 != 0)
		return CHDERR_CODEC_ERROR;

	/* determine whether we want native or swapped samples */
	*(uint8_t *)(&native_endian) = 1;
	flac->native_endian = (native_endian & 1);

	/* flac decoder init */
	if (flac_decoder_init(&flac->decoder))
		return CHDERR_OUT_OF_MEMORY;

	return CHDERR_NONE;
}

static void flac_codec_free(void *codec)
{
	flac_codec_data *flac = (flac_codec_data*)codec;
	flac_decoder_free(&flac->decoder);
}

static chd_error flac_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	flac_codec_data *flac = (flac_codec_data*)codec;
	int swap_endian;

	if (src[0] == 'L')
		swap_endian = !flac->native_endian;
	else if (src[0] == 'B')
		swap_endian = flac->native_endian;
	else
		return CHDERR_DECOMPRESSION_ERROR;

	if (!flac_decoder_reset(&flac->decoder, 44100, 2, flac_codec_blocksize(destlen), src + 1, complen - 1))
		return CHDERR_DECOMPRESSION_ERROR;
	if (!flac_decoder_decode_interleaved(&flac->decoder, (int16_t *)(dest), destlen/4, swap_endian))
		return CHDERR_DECOMPRESSION_ERROR;
	flac_decoder_finish(&flac->decoder);

	return CHDERR_NONE;
}

static uint32_t cdfl_codec_blocksize(uint32_t bytes)
{
	// for CDs it seems that CD_MAX_SECTOR_DATA is the right target
	uint32_t blocksize = bytes / 4;
	while (blocksize > CD_MAX_SECTOR_DATA)
		blocksize /= 2;
	return blocksize;
}

static chd_error cdfl_codec_init(void *codec, uint32_t hunkbytes)
{
#ifdef WANT_SUBCODE
	chd_error ret;
#endif
	uint16_t native_endian = 0;
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	cdfl->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdfl->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	/* determine whether we want native or swapped samples */
	*(uint8_t *)(&native_endian) = 1;
	cdfl->swap_endian = (native_endian & 1);

#ifdef WANT_SUBCODE
	/* init zlib inflater */
	ret = zlib_codec_init(&cdfl->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	/* flac decoder init */
	if (flac_decoder_init(&cdfl->decoder))
		return CHDERR_OUT_OF_MEMORY;

	return CHDERR_NONE;
}

static void cdfl_codec_free(void *codec)
{
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;
	flac_decoder_free(&cdfl->decoder);
#ifdef WANT_SUBCODE
	zlib_codec_free(&cdfl->subcode_decompressor);
#endif
	if (cdfl->buffer)
		free(cdfl->buffer);
}

static chd_error cdfl_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	uint32_t framenum;
	uint8_t *buffer;
#ifdef WANT_SUBCODE
	uint32_t offset;
	chd_error ret;
#endif
	cdfl_codec_data *cdfl = (cdfl_codec_data*)codec;

	/* reset and decode */
	uint32_t frames = destlen / CD_FRAME_SIZE;

	if (!flac_decoder_reset(&cdfl->decoder, 44100, 2, cdfl_codec_blocksize(frames * CD_MAX_SECTOR_DATA), src, complen))
		return CHDERR_DECOMPRESSION_ERROR;
	buffer = &cdfl->buffer[0];
	if (!flac_decoder_decode_interleaved(&cdfl->decoder, (int16_t *)(buffer), frames * CD_MAX_SECTOR_DATA/4, cdfl->swap_endian))
		return CHDERR_DECOMPRESSION_ERROR;

#ifdef WANT_SUBCODE
	/* inflate the subcode data */
	offset = flac_decoder_finish(&cdfl->decoder);
	ret = zlib_codec_decompress(&cdfl->subcode_decompressor, src + offset, complen - offset, &cdfl->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#else
	flac_decoder_finish(&cdfl->decoder);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdfl->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdfl->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif
	}

	return CHDERR_NONE;
}


/***************************************************************************
 *  ZSTD DECOMPRESSOR
 ***************************************************************************
 */

/*-------------------------------------------------
 *  zstd_codec_init - constructor
 *-------------------------------------------------
 */

static chd_error zstd_codec_init(void* codec, uint32_t hunkbytes)
{
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;

	zstd_codec->dstream = ZSTD_createDStream();
	if (!zstd_codec->dstream) {
		printf("NO DSTREAM CREATED!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}
	return CHDERR_NONE;
}

/*-------------------------------------------------
 *  zstd_codec_free
 *-------------------------------------------------
 */

static void zstd_codec_free(void* codec)
{
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;

	ZSTD_freeDStream(zstd_codec->dstream);
}

/*-------------------------------------------------
 *  decompress - decompress data using the ZSTD 
 *  codec
 *-------------------------------------------------
 */
static chd_error zstd_codec_decompress(void* codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	/* initialize */
	zstd_codec_data* zstd_codec = (zstd_codec_data*) codec;
	//reset decompressor
	size_t zstd_res =  ZSTD_initDStream(zstd_codec->dstream);
	if (ZSTD_isError(zstd_res)) 
	{
		printf("INITI DSTREAM FAILED!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}

	ZSTD_inBuffer input =  {src, complen, 0};
	ZSTD_outBuffer output = {dest, destlen, 0 };

	while ((input.pos < input.size) && (output.pos < output.size))
	{
		zstd_res = ZSTD_decompressStream(zstd_codec->dstream, &output, &input);
		if (ZSTD_isError(zstd_res))
		{
			printf("DECOMPRESSION ERROR IN LOOP\n");
			return CHDERR_DECOMPRESSION_ERROR;
		}
	}
	if (output.pos != output.size)
	{
		printf("OUTPUT DOESN'T MATCH!\n");
		return CHDERR_DECOMPRESSION_ERROR;
	}
	return CHDERR_NONE;

}

/* cdzs */
static chd_error cdzs_codec_init(void* codec, uint32_t hunkbytes)
{
	chd_error ret;
	cdzs_codec_data* cdzs = (cdzs_codec_data*) codec;

	/* allocate buffer */
	cdzs->buffer = (uint8_t*)malloc(sizeof(uint8_t) * hunkbytes);
	if (cdzs->buffer == NULL)
		return CHDERR_OUT_OF_MEMORY;

	/* make sure the CHD's hunk size is an even multiple of the frame size */
	ret = zstd_codec_init(&cdzs->base_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SECTOR_DATA);
	if (ret != CHDERR_NONE)
		return ret;

#ifdef WANT_SUBCODE
	ret = zstd_codec_init(&cdzs->subcode_decompressor, (hunkbytes / CD_FRAME_SIZE) * CD_MAX_SUBCODE_DATA);
	if (ret != CHDERR_NONE)
		return ret;
#endif

	if (hunkbytes % CD_FRAME_SIZE != 0)
		return CHDERR_CODEC_ERROR;

	return CHDERR_NONE;
}

static void cdzs_codec_free(void* codec)
{
	cdzs_codec_data* cdzs = (cdzs_codec_data*) codec;
	free(cdzs->buffer);
	zstd_codec_free(&cdzs->base_decompressor);
#ifdef WANT_SUBCODE
	zstd_codec_free(&cdzs->subcode_decompressor);
#endif
}

static chd_error cdzs_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	uint32_t framenum;
	cdzs_codec_data* cdzs = (cdzs_codec_data*)codec;

	/* determine header bytes */
	uint32_t frames = destlen / CD_FRAME_SIZE;
	uint32_t complen_bytes = (destlen < 65536) ? 2 : 3;
	uint32_t ecc_bytes = (frames + 7) / 8;
	uint32_t header_bytes = ecc_bytes + complen_bytes;

	/* extract compressed length of base */
	uint32_t complen_base = (src[ecc_bytes + 0] << 8) | src[ecc_bytes + 1];
	if (complen_bytes > 2)
		complen_base = (complen_base << 8) | src[ecc_bytes + 2];

	/* reset and decode */
	zstd_codec_decompress(&cdzs->base_decompressor, &src[header_bytes], complen_base, &cdzs->buffer[0], frames * CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
	zstd_codec_decompress(&cdzs->subcode_decompressor, &src[header_bytes + complen_base], complen - complen_base - header_bytes, &cdzs->buffer[frames * CD_MAX_SECTOR_DATA], frames * CD_MAX_SUBCODE_DATA);
#endif

	/* reassemble the data */
	for (framenum = 0; framenum < frames; framenum++)
	{
		uint8_t *sector;

		memcpy(&dest[framenum * CD_FRAME_SIZE], &cdzs->buffer[framenum * CD_MAX_SECTOR_DATA], CD_MAX_SECTOR_DATA);
#ifdef WANT_SUBCODE
		memcpy(&dest[framenum * CD_FRAME_SIZE + CD_MAX_SECTOR_DATA], &cdzs->buffer[frames * CD_MAX_SECTOR_DATA + framenum * CD_MAX_SUBCODE_DATA], CD_MAX_SUBCODE_DATA);
#endif

#ifdef WANT_RAW_DATA_SECTOR
		/* reconstitute the ECC data and sync header */
		sector = (uint8_t *)&dest[framenum * CD_FRAME_SIZE];
		if ((src[framenum / 8] & (1 << (framenum % 8))) != 0)
		{
			memcpy(sector, s_cd_sync_header, sizeof(s_cd_sync_header));
			ecc_generate(sector);
		}
#endif
	}
	return CHDERR_NONE;
}

/***************************************************************************
    CODEC INTERFACES
***************************************************************************/

static const codec_interface codec_interfaces[] =
{
	/* "none" or no compression */
	{
		CHDCOMPRESSION_NONE,
		"none",
		FALSE,
		NULL,
		NULL,
		NULL,
		NULL
	},

	/* standard zlib compression */
	{
		CHDCOMPRESSION_ZLIB,
		"zlib",
		FALSE,
		zlib_codec_init,
		zlib_codec_free,
		zlib_codec_decompress,
		NULL
	},

	/* zlib+ compression */
	{
		CHDCOMPRESSION_ZLIB_PLUS,
		"zlib+",
		FALSE,
		zlib_codec_init,
		zlib_codec_free,
		zlib_codec_decompress,
		NULL
	},

	/* V5 zlib compression */
	{
		CHD_CODEC_ZLIB,
		"zlib (Deflate)",
		FALSE,
		zlib_codec_init,
		zlib_codec_free,
		zlib_codec_decompress,
		NULL
	},

	/* V5 lzma compression */
	{
		CHD_CODEC_LZMA,
		"lzma (LZMA)",
		FALSE,
		lzma_codec_init,
		lzma_codec_free,
		lzma_codec_decompress,
		NULL
	},

	/* V5 huffman compression */
	{
		CHD_CODEC_HUFFMAN,
		"Huffman",
		FALSE,
		huff_codec_init,
		huff_codec_free,
		huff_codec_decompress,
		NULL
	},

	/* V5 flac compression */
	{
		CHD_CODEC_FLAC,
		"flac (FLAC)",
		FALSE,
		flac_codec_init,
		flac_codec_free,
		flac_codec_decompress,
		NULL
	},
	/* V5 zstd compression */
	{
		CHD_CODEC_ZSTD,
		"ZStandard",
		FALSE,
		zstd_codec_init,
		zstd_codec_free,
		zstd_codec_decompress,
		NULL
	},

	/* V5 CD zlib compression */
	{
		CHD_CODEC_CD_ZLIB,
		"cdzl (CD Deflate)",
		FALSE,
		cdzl_codec_init,
		cdzl_codec_free,
		cdzl_codec_decompress,
		NULL
	},

	/* V5 CD lzma compression */
	{
		CHD_CODEC_CD_LZMA,
		"cdlz (CD LZMA)",
		FALSE,
		cdlz_codec_init,
		cdlz_codec_free,
		cdlz_codec_decompress,
		NULL
	},

	/* V5 CD flac compression */
	{
		CHD_CODEC_CD_FLAC,
		"cdfl (CD FLAC)",
		FALSE,
		cdfl_codec_init,
		cdfl_codec_free,
		cdfl_codec_decompress,
		NULL
	},
	/* V5 CD zstd compression */
	{
		CHD_CODEC_CD_ZSTD,
		"cdzs (CD ZStandard)",
		FALSE,
		cdzs_codec_init,
		cdzs_codec_free,
		cdzs_codec_decompress,
		NULL
	}
	
};

/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    get_bigendian_uint64 - fetch a UINT64 from
    the data stream in bigendian order
-------------------------------------------------*/

static inline UINT64 get_bigendian_uint64(const UINT8 *base)
{
	return ((UINT64)base[0] << 56) | ((UINT64)base[1] << 48) | ((UINT64)base[2] << 40) | ((UINT64)base[3] << 32) |
			((UINT64)base[4] << 24) | ((UINT64)base[5] << 16) | ((UINT64)base[6] << 8) | (UINT64)base[7];
}

/*-------------------------------------------------
    put_bigendian_uint64 - write a UINT64 to
    the data stream in bigendian order
-------------------------------------------------*/

static inline void put_bigendian_uint64(UINT8 *base, UINT64 value)
{
	base[0] = value >> 56;
	base[1] = value >> 48;
	base[2] = value >> 40;
	base[3] = value >> 32;
	base[4] = value >> 24;
	base[5] = value >> 16;
	base[6] = value >> 8;
	base[7] = value;
}

/*-------------------------------------------------
    get_bigendian_uint48 - fetch a UINT48 from
    the data stream in bigendian order
-------------------------------------------------*/

static inline UINT64 get_bigendian_uint48(const UINT8 *base)
{
	return  ((UINT64)base[0] << 40) | ((UINT64)base[1] << 32) |
			((UINT64)base[2] << 24) | ((UINT64)base[3] << 16) | ((UINT64)base[4] << 8) | (UINT64)base[5];
}

/*-------------------------------------------------
    put_bigendian_uint48 - write a UINT48 to
    the data stream in bigendian order
-------------------------------------------------*/

static inline void put_bigendian_uint48(UINT8 *base, UINT64 value)
{
	value &= 0xffffffffffff;
	base[0] = value >> 40;
	base[1] = value >> 32;
	base[2] = value >> 24;
	base[3] = value >> 16;
	base[4] = value >> 8;
	base[5] = value;
}
/*-------------------------------------------------
    get_bigendian_uint32 - fetch a UINT32 from
    the data stream in bigendian order
-------------------------------------------------*/

static inline UINT32 get_bigendian_uint32(const UINT8 *base)
{
	return (base[0] << 24) | (base[1] << 16) | (base[2] << 8) | base[3];
}

/*-------------------------------------------------
    put_bigendian_uint32 - write a UINT32 to
    the data stream in bigendian order
-------------------------------------------------*/

static inline void put_bigendian_uint32(UINT8 *base, UINT32 value)
{
	base[0] = value >> 24;
	base[1] = value >> 16;
	base[2] = value >> 8;
	base[3] = value;
}

/*-------------------------------------------------
    put_bigendian_uint24 - write a UINT24 to
    the data stream in bigendian order
-------------------------------------------------*/

static inline void put_bigendian_uint24(UINT8 *base, UINT32 value)
{
	value &= 0xffffff;
	base[0] = value >> 16;
	base[1] = value >> 8;
	base[2] = value;
}

/*-------------------------------------------------
    get_bigendian_uint24 - fetch a UINT24 from
    the data stream in bigendian order
-------------------------------------------------*/

static inline UINT32 get_bigendian_uint24(const UINT8 *base)
{
	return (base[0] << 16) | (base[1] << 8) | base[2];
}

/*-------------------------------------------------
    get_bigendian_uint16 - fetch a UINT16 from
    the data stream in bigendian order
-------------------------------------------------*/

static inline UINT16 get_bigendian_uint16(const UINT8 *base)
{
	return (base[0] << 8) | base[1];
}

/*-------------------------------------------------
    put_bigendian_uint16 - write a UINT16 to
    the data stream in bigendian order
-------------------------------------------------*/

static inline void put_bigendian_uint16(UINT8 *base, UINT16 value)
{
	base[0] = value >> 8;
	base[1] = value;
}

/*-------------------------------------------------
    map_extract - extract a single map
    entry from the datastream
-------------------------------------------------*/

static inline void map_extract(const UINT8 *base, map_entry *entry)
{
	entry->offset = get_bigendian_uint64(&base[0]);
	entry->crc = get_bigendian_uint32(&base[8]);
	entry->length = get_bigendian_uint16(&base[12]) | (base[14] << 16);
	entry->flags = base[15];
}

/*-------------------------------------------------
    map_assemble - write a single map
    entry to the datastream
-------------------------------------------------*/

static inline void map_assemble(UINT8 *base, map_entry *entry)
{
	put_bigendian_uint64(&base[0], entry->offset);
	put_bigendian_uint32(&base[8], entry->crc);
	put_bigendian_uint16(&base[12], entry->length);
	base[14] = entry->length >> 16;
	base[15] = entry->flags;
}

/*-------------------------------------------------
    map_size_v5 - calculate CHDv5 map size
-------------------------------------------------*/
static inline int map_size_v5(chd_header* header)
{
	return header->hunkcount * header->mapentrybytes;
}

/*-------------------------------------------------
    crc16 - calculate CRC16 (from hashing.cpp)
-------------------------------------------------*/
uint16_t crc16(const void *data, uint32_t length)
{
	uint16_t crc = 0xffff;

	static const uint16_t s_table[256] =
	{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
	};

	const uint8_t *src = (uint8_t*)data;

	/* fetch the current value into a local and rip through the source data */
	while (length-- != 0)
		crc = (crc << 8) ^ s_table[(crc >> 8) ^ *src++];
	return crc;
}

/*-------------------------------------------------
	compressed - test if CHD file is compressed
+-------------------------------------------------*/
static inline int chd_compressed(chd_header* header) {
	return header->compression[0] != CHD_CODEC_NONE;
}

/*-------------------------------------------------
	decompress_v5_map - decompress the v5 map
-------------------------------------------------*/

static chd_error decompress_v5_map(chd_file* chd, chd_header* header)
{
	int result = 0;
	uint32_t hunknum;
	int repcount = 0;
	uint8_t lastcomp = 0;
	uint32_t last_self = 0;
	uint64_t last_parent = 0;
	struct bitstream* bitbuf;
	uint32_t mapbytes;
	uint64_t firstoffs;
	uint16_t mapcrc;
	uint8_t lengthbits;
	uint8_t selfbits;
	uint8_t parentbits;
	uint8_t *compressed_ptr;
	uint8_t rawbuf[16];
	struct huffman_decoder* decoder;
	enum huffman_error err;
	uint64_t curoffset;	
	int rawmapsize = map_size_v5(header);

	if (!chd_compressed(header))
	{
		header->rawmap = (uint8_t*)malloc(rawmapsize);
		if (header->rawmap == NULL)
			return CHDERR_OUT_OF_MEMORY;
		core_fseek(chd->file, header->mapoffset, SEEK_SET);
		result = core_fread(chd->file, header->rawmap, rawmapsize);
		return CHDERR_NONE;
	}

	/* read the reader */
	core_fseek(chd->file, header->mapoffset, SEEK_SET);
	result = core_fread(chd->file, rawbuf, sizeof(rawbuf));
	mapbytes = get_bigendian_uint32(&rawbuf[0]);
	firstoffs = get_bigendian_uint48(&rawbuf[4]);
	mapcrc = get_bigendian_uint16(&rawbuf[10]);
	lengthbits = rawbuf[12];
	selfbits = rawbuf[13];
	parentbits = rawbuf[14];

	/* now read the map */
	compressed_ptr = (uint8_t*)malloc(sizeof(uint8_t) * mapbytes);
	if (compressed_ptr == NULL)
		return CHDERR_OUT_OF_MEMORY;
	core_fseek(chd->file, header->mapoffset + 16, SEEK_SET);
	result = core_fread(chd->file, compressed_ptr, mapbytes);
	bitbuf = create_bitstream(compressed_ptr, sizeof(uint8_t) * mapbytes);
	header->rawmap = (uint8_t*)malloc(rawmapsize);
	if (header->rawmap == NULL)
	{
		free(compressed_ptr);
		free(bitbuf);
		return CHDERR_OUT_OF_MEMORY;
	}

	/* first decode the compression types */
	decoder = create_huffman_decoder(16, 8);
	if (decoder == NULL)
	{
		free(compressed_ptr);
		free(bitbuf);
		return CHDERR_OUT_OF_MEMORY;
	}

	err = huffman_import_tree_rle(decoder, bitbuf);
	if (err != HUFFERR_NONE)
	{
		free(compressed_ptr);
		free(bitbuf);
		delete_huffman_decoder(decoder);
		return CHDERR_DECOMPRESSION_ERROR;
	}

	for (hunknum = 0; hunknum < header->hunkcount; hunknum++)
	{
		uint8_t *rawmap = header->rawmap + (hunknum * 12);
		if (repcount > 0)
			rawmap[0] = lastcomp, repcount--;
		else
		{
			uint8_t val = huffman_decode_one(decoder, bitbuf);
			if (val == COMPRESSION_RLE_SMALL)
				rawmap[0] = lastcomp, repcount = 2 + huffman_decode_one(decoder, bitbuf);
			else if (val == COMPRESSION_RLE_LARGE)
				rawmap[0] = lastcomp, repcount = 2 + 16 + (huffman_decode_one(decoder, bitbuf) << 4), repcount += huffman_decode_one(decoder, bitbuf);
			else
				rawmap[0] = lastcomp = val;
		}
	}

	/* then iterate through the hunks and extract the needed data */
	curoffset = firstoffs;
	for (hunknum = 0; hunknum < header->hunkcount; hunknum++)
	{
		uint8_t *rawmap = header->rawmap + (hunknum * 12);
		uint64_t offset = curoffset;
		uint32_t length = 0;
		uint16_t crc = 0;
		switch (rawmap[0])
		{
			/* base types */
			case COMPRESSION_TYPE_0:
			case COMPRESSION_TYPE_1:
			case COMPRESSION_TYPE_2:
			case COMPRESSION_TYPE_3:
				curoffset += length = bitstream_read(bitbuf, lengthbits);
				crc = bitstream_read(bitbuf, 16);
				break;

			case COMPRESSION_NONE:
				curoffset += length = header->hunkbytes;
				crc = bitstream_read(bitbuf, 16);
				break;

			case COMPRESSION_SELF:
				last_self = offset = bitstream_read(bitbuf, selfbits);
				break;

			case COMPRESSION_PARENT:
				offset = bitstream_read(bitbuf, parentbits);
				last_parent = offset;
				break;

			/* pseudo-types; convert into base types */
			case COMPRESSION_SELF_1:
				last_self++;
			case COMPRESSION_SELF_0:
				rawmap[0] = COMPRESSION_SELF;
				offset = last_self;
				break;

			case COMPRESSION_PARENT_SELF:
				rawmap[0] = COMPRESSION_PARENT;
				last_parent = offset = ( ((uint64_t)hunknum) * ((uint64_t)header->hunkbytes) ) / header->unitbytes;
				break;

			case COMPRESSION_PARENT_1:
				last_parent += header->hunkbytes / header->unitbytes;
			case COMPRESSION_PARENT_0:
				rawmap[0] = COMPRESSION_PARENT;
				offset = last_parent;
				break;
		}
		/* UINT24 length */
		put_bigendian_uint24(&rawmap[1], length);

		/* UINT48 offset */
		put_bigendian_uint48(&rawmap[4], offset);

		/* crc16 */
		put_bigendian_uint16(&rawmap[10], crc);
	}

	/* free memory */
	free(compressed_ptr);
	free(bitbuf);
	delete_huffman_decoder(decoder);

	/* verify the final CRC */
	if (crc16(&header->rawmap[0], header->hunkcount * 12) != mapcrc)
		return CHDERR_DECOMPRESSION_ERROR;

	return CHDERR_NONE;
}

/*-------------------------------------------------
    map_extract_old - extract a single map
    entry in old format from the datastream
-------------------------------------------------*/

static inline void map_extract_old(const UINT8 *base, map_entry *entry, UINT32 hunkbytes)
{
	entry->offset = get_bigendian_uint64(&base[0]);
	entry->crc = 0;
	entry->length = entry->offset >> 44;
	entry->flags = MAP_ENTRY_FLAG_NO_CRC | ((entry->length == hunkbytes) ? V34_MAP_ENTRY_TYPE_UNCOMPRESSED : V34_MAP_ENTRY_TYPE_COMPRESSED);
#ifdef __MWERKS__
	entry->offset = entry->offset & 0x00000FFFFFFFFFFFLL;
#else
	entry->offset = (entry->offset << 20) >> 20;
#endif
}

/***************************************************************************
    CHD FILE MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_open_file - open a CHD file for access
-------------------------------------------------*/

CHD_EXPORT chd_error chd_open_file(FILE *file, int mode, chd_file *parent, chd_file **chd) {
	core_file *stream = malloc(sizeof(core_file));
	if (!stream)
		return CHDERR_OUT_OF_MEMORY;
	stream->argp = file;
	stream->fsize = core_stdio_fsize;
	stream->fread = core_stdio_fread;
	stream->fclose = core_stdio_fclose_nonowner;
	stream->fseek = core_stdio_fseek;

	chd_error err = chd_open_core_file(stream, mode, parent, chd);
	if (err != CHDERR_NONE)
		return err;

	// swap out the fclose so that we close it on chd clost
	stream->fclose = core_stdio_fclose;
	return CHDERR_NONE;
}

/*-------------------------------------------------
    chd_open_core_file - open a CHD file for access
-------------------------------------------------*/

CHD_EXPORT chd_error chd_open_core_file(core_file *file, int mode, chd_file *parent, chd_file **chd)
{
	chd_file *newchd = NULL;
	chd_error err;
	int intfnum;

	/* verify parameters */
	if (file == NULL)
		EARLY_EXIT(err = CHDERR_INVALID_PARAMETER);

	/* punt if invalid parent */
	if (parent != NULL && parent->cookie != COOKIE_VALUE)
		EARLY_EXIT(err = CHDERR_INVALID_PARAMETER);

	/* allocate memory for the final result */
	newchd = (chd_file *)malloc(sizeof(**chd));
	if (newchd == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);
	memset(newchd, 0, sizeof(*newchd));
	newchd->cookie = COOKIE_VALUE;
	newchd->parent = parent;
	newchd->file = file;

	/* now attempt to read the header */
	err = header_read(newchd, &newchd->header);
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);

	/* validate the header */
	err = header_validate(&newchd->header);
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);

	/* make sure we don't open a read-only file writeable */
	if (mode == CHD_OPEN_READWRITE && !(newchd->header.flags & CHDFLAGS_IS_WRITEABLE))
		EARLY_EXIT(err = CHDERR_FILE_NOT_WRITEABLE);

	/* also, never open an older version writeable */
	if (mode == CHD_OPEN_READWRITE && newchd->header.version < CHD_HEADER_VERSION)
		EARLY_EXIT(err = CHDERR_UNSUPPORTED_VERSION);

	/* if we need a parent, make sure we have one */
	if (parent == NULL)
	{
		/* Detect parent requirement for versions below 5 */
		if (newchd->header.version < 5 && newchd->header.flags & CHDFLAGS_HAS_PARENT)
			EARLY_EXIT(err = CHDERR_REQUIRES_PARENT);
		/* Detection for version 5 and above - if parentsha1 != 0, we have a parent */
		else if (newchd->header.version >= 5 && memcmp(nullsha1, newchd->header.parentsha1, sizeof(newchd->header.parentsha1)) != 0)
			EARLY_EXIT(err = CHDERR_REQUIRES_PARENT);
	}

	/* make sure we have a valid parent */
	if (parent != NULL)
	{
		/* check MD5 if it isn't empty */
		if (memcmp(nullmd5, newchd->header.parentmd5, sizeof(newchd->header.parentmd5)) != 0 &&
			memcmp(nullmd5, newchd->parent->header.md5, sizeof(newchd->parent->header.md5)) != 0 &&
			memcmp(newchd->parent->header.md5, newchd->header.parentmd5, sizeof(newchd->header.parentmd5)) != 0)
			EARLY_EXIT(err = CHDERR_INVALID_PARENT);

		/* check SHA1 if it isn't empty */
		if (memcmp(nullsha1, newchd->header.parentsha1, sizeof(newchd->header.parentsha1)) != 0 &&
			memcmp(nullsha1, newchd->parent->header.sha1, sizeof(newchd->parent->header.sha1)) != 0 &&
			memcmp(newchd->parent->header.sha1, newchd->header.parentsha1, sizeof(newchd->header.parentsha1)) != 0)
			EARLY_EXIT(err = CHDERR_INVALID_PARENT);
	}

	/* now read the hunk map */
	if (newchd->header.version < 5)
	{
		err = map_read(newchd);
		if (err != CHDERR_NONE)
			EARLY_EXIT(err);
	}
	else
	{
		err = decompress_v5_map(newchd, &(newchd->header));
	}
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);

#ifdef NEED_CACHE_HUNK
	/* allocate and init the hunk cache */
	newchd->cache = (UINT8 *)malloc(newchd->header.hunkbytes);
	newchd->compare = (UINT8 *)malloc(newchd->header.hunkbytes);
	if (newchd->cache == NULL || newchd->compare == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);
	newchd->cachehunk = ~0;
	newchd->comparehunk = ~0;
#endif

	/* allocate the temporary compressed buffer */
	newchd->compressed = (UINT8 *)malloc(newchd->header.hunkbytes);
	if (newchd->compressed == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);

	/* find the codec interface */
	if (newchd->header.version < 5)
	{
		for (intfnum = 0; intfnum < ARRAY_LENGTH(codec_interfaces); intfnum++)
		{
			if (codec_interfaces[intfnum].compression == newchd->header.compression[0])
			{
				newchd->codecintf[0] = &codec_interfaces[intfnum];
				break;
			}
		}

		if (intfnum == ARRAY_LENGTH(codec_interfaces))
			EARLY_EXIT(err = CHDERR_UNSUPPORTED_FORMAT);

		/* initialize the codec */
		if (newchd->codecintf[0]->init != NULL)
		{
			err = (*newchd->codecintf[0]->init)(&newchd->zlib_codec_data, newchd->header.hunkbytes);
			if (err != CHDERR_NONE)
				EARLY_EXIT(err);
		}
	}
	else
	{
		int decompnum;
		/* verify the compression types and initialize the codecs */
		for (decompnum = 0; decompnum < ARRAY_LENGTH(newchd->header.compression); decompnum++)
		{
			int i;
			for (i = 0 ; i < ARRAY_LENGTH(codec_interfaces) ; i++)
			{
				if (codec_interfaces[i].compression == newchd->header.compression[decompnum])
				{
					newchd->codecintf[decompnum] = &codec_interfaces[i];
					break;
				}
			}

			if (newchd->codecintf[decompnum] == NULL && newchd->header.compression[decompnum] != 0)
				EARLY_EXIT(err = CHDERR_UNSUPPORTED_FORMAT);

			/* initialize the codec */
			if (newchd->codecintf[decompnum]->init != NULL)
			{
				void* codec = NULL;
				switch (newchd->header.compression[decompnum])
				{
					case CHD_CODEC_ZLIB:
						codec = &newchd->zlib_codec_data;
						break;

					case CHD_CODEC_LZMA:
						codec = &newchd->lzma_codec_data;
						break;

					case CHD_CODEC_HUFFMAN:
						codec = &newchd->huff_codec_data;
						break;

					case CHD_CODEC_FLAC:
						codec = &newchd->flac_codec_data;
						break;

					case CHD_CODEC_ZSTD:
						codec = &newchd->zstd_codec_data;
						break;

					case CHD_CODEC_CD_ZLIB:
						codec = &newchd->cdzl_codec_data;
						break;

					case CHD_CODEC_CD_LZMA:
						codec = &newchd->cdlz_codec_data;
						break;

					case CHD_CODEC_CD_FLAC:
						codec = &newchd->cdfl_codec_data;
						break;

					case CHD_CODEC_CD_ZSTD:
						codec = &newchd->cdzs_codec_data;
						break;
				}

				if (codec == NULL)
					EARLY_EXIT(err = CHDERR_UNSUPPORTED_FORMAT);

				err = (*newchd->codecintf[decompnum]->init)(codec, newchd->header.hunkbytes);
				if (err != CHDERR_NONE)
					EARLY_EXIT(err);
			}
		}
	}

	/* all done */
	*chd = newchd;
	return CHDERR_NONE;

cleanup:
	if (newchd != NULL)
		chd_close(newchd);
	return err;
}

/*-------------------------------------------------
    chd_precache - precache underlying file in
    memory
-------------------------------------------------*/

CHD_EXPORT chd_error chd_precache(chd_file* chd)
{
  return chd_precache_progress(chd, NULL, NULL);
}

CHD_EXPORT chd_error chd_precache_progress(chd_file* chd, void(*progress)(size_t pos, size_t total, void* param), void* param)
{
#define PRECACHE_CHUNK_SIZE 16 * 1024 * 1024

  size_t count;
  UINT64 size, done, req_count, last_update_done, update_interval;

  if (chd->file_cache == NULL)
  {
    size = core_fsize(chd->file);
    if ((INT64)size <= 0)
      return CHDERR_INVALID_DATA;

		if (size > SIZE_MAX)
			return CHDERR_OUT_OF_MEMORY;

    chd->file_cache = malloc(size);
    if (chd->file_cache == NULL)
      return CHDERR_OUT_OF_MEMORY;
    core_fseek(chd->file, 0, SEEK_SET);

		done = 0;
    last_update_done = 0;
    update_interval = ((size + 99) / 100);

		while (done < size)
		{
			req_count = size - done;
			if (req_count > PRECACHE_CHUNK_SIZE)
				req_count = PRECACHE_CHUNK_SIZE;

			count = core_fread(chd->file, chd->file_cache + (size_t)done, (size_t)req_count);
			if (count != (size_t)req_count)
			{
				free(chd->file_cache);
				chd->file_cache = NULL;
				return CHDERR_READ_ERROR;
			}

			done += req_count;
			if (progress != NULL && (done - last_update_done) >= update_interval && done != size)
			{
				last_update_done = done;
				progress(done, size, param);
			}
		}
  }

  return CHDERR_NONE;
}

/*-------------------------------------------------
    chd_open - open a CHD file by
    filename
-------------------------------------------------*/

CHD_EXPORT chd_error chd_open(const char *filename, int mode, chd_file *parent, chd_file **chd)
{
	chd_error err;
	core_file *file = NULL;

	if (filename == NULL)
	{
		err = CHDERR_INVALID_PARAMETER;
		goto cleanup;
	}

	/* choose the proper mode */
	switch(mode)
	{
		case CHD_OPEN_READ:
			break;

		default:
			err = CHDERR_INVALID_PARAMETER;
			goto cleanup;
	}

	/* open the file */
	file = core_stdio_fopen(filename);
	if (file == 0)
	{
		err = CHDERR_FILE_NOT_FOUND;
		goto cleanup;
	}

	/* now open the CHD */
	return chd_open_core_file(file, mode, parent, chd);

cleanup:
	if ((err != CHDERR_NONE) && (file != NULL))
		core_fclose(file);
	return err;
}

/*-------------------------------------------------
    chd_close - close a CHD file for access
-------------------------------------------------*/

CHD_EXPORT void chd_close(chd_file *chd)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return;

	/* deinit the codec */
	if (chd->header.version < 5)
	{
		if (chd->codecintf[0] != NULL && chd->codecintf[0]->free != NULL)
			(*chd->codecintf[0]->free)(&chd->zlib_codec_data);
	}
	else
	{
		int i;
		/* Free the codecs */
		for (i = 0 ; i < ARRAY_LENGTH(chd->codecintf); i++)
		{
			void* codec = NULL;

			if (chd->codecintf[i] == NULL)
				continue;

			switch (chd->codecintf[i]->compression)
			{
				case CHD_CODEC_ZLIB:
					codec = &chd->zlib_codec_data;
					break;

				case CHD_CODEC_LZMA:
					codec = &chd->lzma_codec_data;
					break;

				case CHD_CODEC_HUFFMAN:
					codec = &chd->huff_codec_data;
					break;

				case CHD_CODEC_FLAC:
					codec = &chd->flac_codec_data;
					break;

				case CHD_CODEC_ZSTD:
					codec = &chd->zstd_codec_data;
					break;

				case CHD_CODEC_CD_ZLIB:
					codec = &chd->cdzl_codec_data;
					break;

				case CHD_CODEC_CD_LZMA:
					codec = &chd->cdlz_codec_data;
					break;

				case CHD_CODEC_CD_FLAC:
					codec = &chd->cdfl_codec_data;
					break;

				case CHD_CODEC_CD_ZSTD:
					codec = &chd->cdzs_codec_data;
					break;
			}

			if (codec)
			{
				(*chd->codecintf[i]->free)(codec);
			}
		}

		/* Free the raw map */
		if (chd->header.rawmap != NULL)
			free(chd->header.rawmap);
	}

	/* free the compressed data buffer */
	if (chd->compressed != NULL)
		free(chd->compressed);

#ifdef NEED_CACHE_HUNK
	/* free the hunk cache and compare data */
	if (chd->compare != NULL)
		free(chd->compare);
	if (chd->cache != NULL)
		free(chd->cache);
#endif

	/* free the hunk map */
	if (chd->map != NULL)
		free(chd->map);

	/* close the file */
	if (chd->file != NULL)
		core_fclose(chd->file);

#ifdef NEED_CACHE_HUNK
	if (PRINTF_MAX_HUNK) printf("Max hunk = %d/%d\n", chd->maxhunk, chd->header.totalhunks);
#endif
	if (chd->file_cache)
		free(chd->file_cache);

	if (chd->parent)
		chd_close(chd->parent);

	/* free our memory */
	free(chd);
}

/*-------------------------------------------------
    chd_core_file - return the associated
    core_file
-------------------------------------------------*/

CHD_EXPORT core_file *chd_core_file(chd_file *chd)
{
	return chd->file;
}

CHD_EXPORT UINT64 chd_get_compressed_size(chd_file *chd)
{
	UINT64 size = chd->file->fsize(chd->file);
	if (chd->parent)
		size += chd_get_compressed_size(chd->parent);
	return size;
}

/*-------------------------------------------------
    chd_error_string - return an error string for
    the given CHD error
-------------------------------------------------*/

CHD_EXPORT const char *chd_error_string(chd_error err)
{
	switch (err)
	{
		case CHDERR_NONE:						return "no error";
		case CHDERR_NO_INTERFACE:				return "no drive interface";
		case CHDERR_OUT_OF_MEMORY:				return "out of memory";
		case CHDERR_INVALID_FILE:				return "invalid file";
		case CHDERR_INVALID_PARAMETER:			return "invalid parameter";
		case CHDERR_INVALID_DATA:				return "invalid data";
		case CHDERR_FILE_NOT_FOUND:				return "file not found";
		case CHDERR_REQUIRES_PARENT:			return "requires parent";
		case CHDERR_FILE_NOT_WRITEABLE:			return "file not writeable";
		case CHDERR_READ_ERROR:					return "read error";
		case CHDERR_WRITE_ERROR:				return "write error";
		case CHDERR_CODEC_ERROR:				return "codec error";
		case CHDERR_INVALID_PARENT:				return "invalid parent";
		case CHDERR_HUNK_OUT_OF_RANGE:			return "hunk out of range";
		case CHDERR_DECOMPRESSION_ERROR:		return "decompression error";
		case CHDERR_COMPRESSION_ERROR:			return "compression error";
		case CHDERR_CANT_CREATE_FILE:			return "can't create file";
		case CHDERR_CANT_VERIFY:				return "can't verify file";
		case CHDERR_NOT_SUPPORTED:				return "operation not supported";
		case CHDERR_METADATA_NOT_FOUND:			return "can't find metadata";
		case CHDERR_INVALID_METADATA_SIZE:		return "invalid metadata size";
		case CHDERR_UNSUPPORTED_VERSION:		return "unsupported CHD version";
		case CHDERR_VERIFY_INCOMPLETE:			return "incomplete verify";
		case CHDERR_INVALID_METADATA:			return "invalid metadata";
		case CHDERR_INVALID_STATE:				return "invalid state";
		case CHDERR_OPERATION_PENDING:			return "operation pending";
		case CHDERR_NO_ASYNC_OPERATION:			return "no async operation in progress";
		case CHDERR_UNSUPPORTED_FORMAT:			return "unsupported format";
		default:								return "undocumented error";
	}
}

/***************************************************************************
    CHD HEADER MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_get_header - return a pointer to the
    extracted header data
-------------------------------------------------*/

CHD_EXPORT const chd_header *chd_get_header(chd_file *chd)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return NULL;

	return &chd->header;
}

/*-------------------------------------------------
    chd_read_header - read CHD header data
	from file into the pointed struct
-------------------------------------------------*/
CHD_EXPORT chd_error chd_read_header_core_file(core_file* file, chd_header* header)
{
	chd_file chd;
	chd.file = file;

	/* attempt to read the header */
	const chd_error err = header_read(&chd, header);
	if (err != CHDERR_NONE)
		return err;

	/* validate the header */
	return header_validate(header);
}

CHD_EXPORT chd_error chd_read_header_file(FILE *file, chd_header *header)
{
  core_file stream;
  stream.argp = file;
  stream.fsize = core_stdio_fsize;
  stream.fread = core_stdio_fread;
  stream.fclose = core_stdio_fclose_nonowner;
  stream.fseek = core_stdio_fseek;

	return chd_read_header_core_file(&stream, header);
}

CHD_EXPORT chd_error chd_read_header(const char *filename, chd_header *header)
{
	if (filename == NULL)
		return CHDERR_INVALID_PARAMETER;

	core_file* file = core_stdio_fopen(filename);
	if (file == NULL)
		return CHDERR_FILE_NOT_FOUND;

	chd_error err = chd_read_header_core_file(file, header);
	
	core_fclose(file);
	return err;
}

CHD_EXPORT bool chd_is_matching_parent(const chd_header* header, const chd_header* parent_header)
{
  /* check MD5 if it isn't empty */
  if (memcmp(nullmd5, header->parentmd5, sizeof(header->parentmd5)) != 0 &&
      memcmp(nullmd5, parent_header->md5, sizeof(parent_header->md5)) != 0 &&
      memcmp(parent_header->md5, header->parentmd5, sizeof(header->parentmd5)) != 0)
	{
		return false;
	}

  /* check SHA1 if it isn't empty */
  if (memcmp(nullsha1, header->parentsha1, sizeof(header->parentsha1)) != 0 &&
      memcmp(nullsha1, parent_header->sha1, sizeof(parent_header->sha1)) != 0 &&
      memcmp(parent_header->sha1, header->parentsha1, sizeof(header->parentsha1)) != 0)
	{
		return false;
	}

	return true;
}

/***************************************************************************
    CORE DATA READ/WRITE
***************************************************************************/

/*-------------------------------------------------
    chd_read - read a single hunk from the CHD
    file
-------------------------------------------------*/

CHD_EXPORT chd_error chd_read(chd_file *chd, UINT32 hunknum, void *buffer)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return CHDERR_INVALID_PARAMETER;

	/* if we're past the end, fail */
	if (hunknum >= chd->header.totalhunks)
		return CHDERR_HUNK_OUT_OF_RANGE;

	/* perform the read */
	return hunk_read_into_memory(chd, hunknum, (UINT8 *)buffer);
}

/***************************************************************************
    METADATA MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_get_metadata - get the indexed metadata
    of the given type
-------------------------------------------------*/

CHD_EXPORT chd_error chd_get_metadata(chd_file *chd, UINT32 searchtag, UINT32 searchindex, void *output, UINT32 outputlen, UINT32 *resultlen, UINT32 *resulttag, UINT8 *resultflags)
{
	metadata_entry metaentry;
	chd_error err;
	UINT32 count;

	/* if we didn't find it, just return */
	err = metadata_find_entry(chd, searchtag, searchindex, &metaentry);
	if (err != CHDERR_NONE)
	{
		/* unless we're an old version and they are requesting hard disk metadata */
		if (chd->header.version < 3 && (searchtag == HARD_DISK_METADATA_TAG || searchtag == CHDMETATAG_WILDCARD) && searchindex == 0)
		{
			char faux_metadata[256];
			UINT32 faux_length;

			/* fill in the faux metadata */
			sprintf(faux_metadata, HARD_DISK_METADATA_FORMAT, chd->header.obsolete_cylinders, chd->header.obsolete_heads, chd->header.obsolete_sectors, chd->header.hunkbytes / chd->header.obsolete_hunksize);
			faux_length = (UINT32)strlen(faux_metadata) + 1;

			/* copy the metadata itself */
			memcpy(output, faux_metadata, MIN(outputlen, faux_length));

			/* return the length of the data and the tag */
			if (resultlen != NULL)
				*resultlen = faux_length;
			if (resulttag != NULL)
				*resulttag = HARD_DISK_METADATA_TAG;
			return CHDERR_NONE;
		}
		return err;
	}

	/* read the metadata */
	outputlen = MIN(outputlen, metaentry.length);
	core_fseek(chd->file, metaentry.offset + METADATA_HEADER_SIZE, SEEK_SET);
	count = core_fread(chd->file, output, outputlen);
	if (count != outputlen)
		return CHDERR_READ_ERROR;

	/* return the length of the data and the tag */
	if (resultlen != NULL)
		*resultlen = metaentry.length;
	if (resulttag != NULL)
		*resulttag = metaentry.metatag;
	if (resultflags != NULL)
		*resultflags = metaentry.flags;
	return CHDERR_NONE;
}

/***************************************************************************
    CODEC INTERFACES
***************************************************************************/

/*-------------------------------------------------
    chd_codec_config - set internal codec
    parameters
-------------------------------------------------*/

CHD_EXPORT chd_error chd_codec_config(chd_file *chd, int param, void *config)
{
	return CHDERR_INVALID_PARAMETER;
}

/*-------------------------------------------------
    chd_get_codec_name - get the name of a
    particular codec
-------------------------------------------------*/

CHD_EXPORT const char *chd_get_codec_name(UINT32 codec)
{
	return "Unknown";
}

/***************************************************************************
    INTERNAL HEADER OPERATIONS
***************************************************************************/

/*-------------------------------------------------
    header_validate - check the validity of a
    CHD header
-------------------------------------------------*/

static chd_error header_validate(const chd_header *header)
{
	int intfnum;

	/* require a valid version */
	if (header->version == 0 || header->version > CHD_HEADER_VERSION)
		return CHDERR_UNSUPPORTED_VERSION;

	/* require a valid length */
	if ((header->version == 1 && header->length != CHD_V1_HEADER_SIZE) ||
		(header->version == 2 && header->length != CHD_V2_HEADER_SIZE) ||
		(header->version == 3 && header->length != CHD_V3_HEADER_SIZE) ||
		(header->version == 4 && header->length != CHD_V4_HEADER_SIZE) ||
		(header->version == 5 && header->length != CHD_V5_HEADER_SIZE))
		return CHDERR_INVALID_PARAMETER;

	/* Do not validate v5 header */
	if (header->version <= 4)
	{
		/* require valid flags */
		if (header->flags & CHDFLAGS_UNDEFINED)
			return CHDERR_INVALID_PARAMETER;

		/* require a supported compression mechanism */
		for (intfnum = 0; intfnum < ARRAY_LENGTH(codec_interfaces); intfnum++)
			if (codec_interfaces[intfnum].compression == header->compression[0])
				break;

		if (intfnum == ARRAY_LENGTH(codec_interfaces))
			return CHDERR_INVALID_PARAMETER;

		/* require a valid hunksize */
		if (header->hunkbytes == 0 || header->hunkbytes >= 65536 * 256)
			return CHDERR_INVALID_PARAMETER;

		/* require a valid hunk count */
		if (header->totalhunks == 0)
			return CHDERR_INVALID_PARAMETER;

		/* require a valid MD5 and/or SHA1 if we're using a parent */
		if ((header->flags & CHDFLAGS_HAS_PARENT) && memcmp(header->parentmd5, nullmd5, sizeof(nullmd5)) == 0 && memcmp(header->parentsha1, nullsha1, sizeof(nullsha1)) == 0)
			return CHDERR_INVALID_PARAMETER;

		/* if we're V3 or later, the obsolete fields must be 0 */
		if (header->version >= 3 &&
			(header->obsolete_cylinders != 0 || header->obsolete_sectors != 0 ||
			 header->obsolete_heads != 0 || header->obsolete_hunksize != 0))
			return CHDERR_INVALID_PARAMETER;

		/* if we're pre-V3, the obsolete fields must NOT be 0 */
		if (header->version < 3 &&
			(header->obsolete_cylinders == 0 || header->obsolete_sectors == 0 ||
			 header->obsolete_heads == 0 || header->obsolete_hunksize == 0))
			return CHDERR_INVALID_PARAMETER;
	}

	return CHDERR_NONE;
}

/*-------------------------------------------------
    header_guess_unitbytes - for older CHD formats,
    guess at the bytes/unit based on metadata
-------------------------------------------------*/

static UINT32 header_guess_unitbytes(chd_file *chd)
{
	/* look for hard disk metadata; if found, then the unit size == sector size */
	char metadata[512];
	int i0, i1, i2, i3;
	if (chd_get_metadata(chd, HARD_DISK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE &&
		sscanf(metadata, HARD_DISK_METADATA_FORMAT, &i0, &i1, &i2, &i3) == 4)
		return i3;

	/* look for CD-ROM metadata; if found, then the unit size == CD frame size */
	if (chd_get_metadata(chd, CDROM_OLD_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, GDROM_OLD_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE)
		return CD_FRAME_SIZE;

	/* otherwise, just map 1:1 with the hunk size */
	return chd->header.hunkbytes;
}

/*-------------------------------------------------
    header_read - read a CHD header into the
    internal data structure
-------------------------------------------------*/

static chd_error header_read(chd_file *chd, chd_header *header)
{
	UINT8 rawheader[CHD_MAX_HEADER_SIZE];
	UINT32 count;

	/* punt if NULL */
	if (header == NULL)
		return CHDERR_INVALID_PARAMETER;

	/* punt if invalid file */
	if (chd->file == NULL)
		return CHDERR_INVALID_FILE;

	/* seek and read */
	core_fseek(chd->file, 0, SEEK_SET);
	count = core_fread(chd->file, rawheader, sizeof(rawheader));
	if (count != sizeof(rawheader))
		return CHDERR_READ_ERROR;

	/* verify the tag */
	if (strncmp((char *)rawheader, "MComprHD", 8) != 0)
		return CHDERR_INVALID_DATA;

	/* extract the direct data */
	memset(header, 0, sizeof(*header));
	header->length        = get_bigendian_uint32(&rawheader[8]);
	header->version       = get_bigendian_uint32(&rawheader[12]);

	/* make sure it's a version we understand */
	if (header->version == 0 || header->version > CHD_HEADER_VERSION)
		return CHDERR_UNSUPPORTED_VERSION;

	/* make sure the length is expected */
	if ((header->version == 1 && header->length != CHD_V1_HEADER_SIZE) ||
		(header->version == 2 && header->length != CHD_V2_HEADER_SIZE) ||
		(header->version == 3 && header->length != CHD_V3_HEADER_SIZE) ||
		(header->version == 4 && header->length != CHD_V4_HEADER_SIZE) ||
		(header->version == 5 && header->length != CHD_V5_HEADER_SIZE))

		return CHDERR_INVALID_DATA;

	/* extract the common data */
	header->flags         	= get_bigendian_uint32(&rawheader[16]);
	header->compression[0]	= get_bigendian_uint32(&rawheader[20]);
	header->compression[1]	= CHD_CODEC_NONE;
	header->compression[2]	= CHD_CODEC_NONE;
	header->compression[3]	= CHD_CODEC_NONE;

	/* extract the V1/V2-specific data */
	if (header->version < 3)
	{
		int seclen = (header->version == 1) ? CHD_V1_SECTOR_SIZE : get_bigendian_uint32(&rawheader[76]);
		header->obsolete_hunksize  = get_bigendian_uint32(&rawheader[24]);
		header->totalhunks         = get_bigendian_uint32(&rawheader[28]);
		header->obsolete_cylinders = get_bigendian_uint32(&rawheader[32]);
		header->obsolete_heads     = get_bigendian_uint32(&rawheader[36]);
		header->obsolete_sectors   = get_bigendian_uint32(&rawheader[40]);
		memcpy(header->md5, &rawheader[44], CHD_MD5_BYTES);
		memcpy(header->parentmd5, &rawheader[60], CHD_MD5_BYTES);
		header->logicalbytes = (UINT64)header->obsolete_cylinders * (UINT64)header->obsolete_heads * (UINT64)header->obsolete_sectors * (UINT64)seclen;
		header->hunkbytes = seclen * header->obsolete_hunksize;
		header->unitbytes          = header_guess_unitbytes(chd);
		if (header->unitbytes == 0)
			return CHDERR_INVALID_DATA;
		header->unitcount          = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		header->metaoffset = 0;
	}

	/* extract the V3-specific data */
	else if (header->version == 3)
	{
		header->totalhunks   = get_bigendian_uint32(&rawheader[24]);
		header->logicalbytes = get_bigendian_uint64(&rawheader[28]);
		header->metaoffset   = get_bigendian_uint64(&rawheader[36]);
		memcpy(header->md5, &rawheader[44], CHD_MD5_BYTES);
		memcpy(header->parentmd5, &rawheader[60], CHD_MD5_BYTES);
		header->hunkbytes    = get_bigendian_uint32(&rawheader[76]);
		header->unitbytes    = header_guess_unitbytes(chd);
		if (header->unitbytes == 0)
			return CHDERR_INVALID_DATA;
		header->unitcount    = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[80], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[100], CHD_SHA1_BYTES);
	}

	/* extract the V4-specific data */
	else if (header->version == 4)
	{
		header->totalhunks   = get_bigendian_uint32(&rawheader[24]);
		header->logicalbytes = get_bigendian_uint64(&rawheader[28]);
		header->metaoffset   = get_bigendian_uint64(&rawheader[36]);
		header->hunkbytes    = get_bigendian_uint32(&rawheader[44]);
		header->unitbytes    = header_guess_unitbytes(chd);
		if (header->unitbytes == 0)
			return CHDERR_INVALID_DATA;
		header->unitcount    = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[48], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[68], CHD_SHA1_BYTES);
		memcpy(header->rawsha1, &rawheader[88], CHD_SHA1_BYTES);
	}

	/* extract the V5-specific data */
	else if (header->version == 5)
	{
		/* TODO */
		header->compression[0]  = get_bigendian_uint32(&rawheader[16]);
		header->compression[1]  = get_bigendian_uint32(&rawheader[20]);
		header->compression[2]  = get_bigendian_uint32(&rawheader[24]);
		header->compression[3]  = get_bigendian_uint32(&rawheader[28]);
		header->logicalbytes    = get_bigendian_uint64(&rawheader[32]);
		header->mapoffset       = get_bigendian_uint64(&rawheader[40]);
		header->metaoffset      = get_bigendian_uint64(&rawheader[48]);
		header->hunkbytes       = get_bigendian_uint32(&rawheader[56]);
		if (header->hunkbytes == 0)
			return CHDERR_INVALID_DATA;
		header->hunkcount       = (header->logicalbytes + header->hunkbytes - 1) / header->hunkbytes;
		header->unitbytes       = get_bigendian_uint32(&rawheader[60]);
		if (header->unitbytes == 0)
			return CHDERR_INVALID_DATA;
		header->unitcount       = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[84], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[104], CHD_SHA1_BYTES);
		memcpy(header->rawsha1, &rawheader[64], CHD_SHA1_BYTES);

		/* determine properties of map entries */
		header->mapentrybytes = chd_compressed(header) ? 12 : 4;

		/* hack */
		header->totalhunks 		= header->hunkcount;
	}

	/* Unknown version */
	else
	{
		/* TODO */
	}

	/* guess it worked */
	return CHDERR_NONE;
}

/***************************************************************************
    INTERNAL HUNK READ/WRITE
***************************************************************************/

/*-------------------------------------------------
    hunk_read_compressed - read a compressed
    hunk
-------------------------------------------------*/

static UINT8* hunk_read_compressed(chd_file *chd, UINT64 offset, size_t size)
{
#ifdef _MSC_VER
	size_t bytes;
#else
	ssize_t bytes;
#endif
	if (chd->file_cache != NULL)
	{
		return chd->file_cache + offset;
	}
	else
	{
		core_fseek(chd->file, offset, SEEK_SET);
		bytes = core_fread(chd->file, chd->compressed, size);
		if (bytes != size)
			return NULL;
		return chd->compressed;
	}
}

/*-------------------------------------------------
    hunk_read_uncompressed - read an uncompressed
    hunk
-------------------------------------------------*/

static chd_error hunk_read_uncompressed(chd_file *chd, UINT64 offset, size_t size, UINT8 *dest)
{
#ifdef _MSC_VER
	size_t bytes;
#else
	ssize_t bytes;
#endif
	if (chd->file_cache != NULL)
	{
		memcpy(dest, chd->file_cache + offset, size);
	}
	else
	{
		core_fseek(chd->file, offset, SEEK_SET);
		bytes = core_fread(chd->file, dest, size);
		if (bytes != size)
			return CHDERR_READ_ERROR;
	}
	return CHDERR_NONE;
}

#ifdef NEED_CACHE_HUNK
/*-------------------------------------------------
    hunk_read_into_cache - read a hunk into
    the CHD's hunk cache
-------------------------------------------------*/

static chd_error hunk_read_into_cache(chd_file *chd, UINT32 hunknum)
{
	chd_error err;

	/* track the max */
	if (hunknum > chd->maxhunk)
		chd->maxhunk = hunknum;

	/* if we're already in the cache, we're done */
	if (chd->cachehunk == hunknum)
		return CHDERR_NONE;
	chd->cachehunk = ~0;

	/* otherwise, read the data */
	err = hunk_read_into_memory(chd, hunknum, chd->cache);
	if (err != CHDERR_NONE)
		return err;

	/* mark the hunk successfully cached in */
	chd->cachehunk = hunknum;
	return CHDERR_NONE;
}
#endif

/*-------------------------------------------------
    hunk_read_into_memory - read a hunk into
    memory at the given location
-------------------------------------------------*/

static chd_error hunk_read_into_memory(chd_file *chd, UINT32 hunknum, UINT8 *dest)
{
	chd_error err;

	/* punt if no file */
	if (chd->file == NULL)
		return CHDERR_INVALID_FILE;

	/* return an error if out of range */
	if (hunknum >= chd->header.totalhunks)
		return CHDERR_HUNK_OUT_OF_RANGE;

	if (dest == NULL)
		return CHDERR_INVALID_PARAMETER;

	if (chd->header.version < 5)
	{
		map_entry *entry = &chd->map[hunknum];
		UINT32 bytes;
		UINT8* compressed_bytes;

		/* switch off the entry type */
		switch (entry->flags & MAP_ENTRY_FLAG_TYPE_MASK)
		{
			/* compressed data */
			case V34_MAP_ENTRY_TYPE_COMPRESSED:
            {
               void *codec = NULL;

				/* read it into the decompression buffer */
				compressed_bytes = hunk_read_compressed(chd, entry->offset, entry->length);
				if (compressed_bytes == NULL)
					{
					return CHDERR_READ_ERROR;
					}

				/* now decompress using the codec */
				err = CHDERR_NONE;
				codec = &chd->zlib_codec_data;
				if (chd->codecintf[0]->decompress != NULL)
					err = (*chd->codecintf[0]->decompress)(codec, compressed_bytes, entry->length, dest, chd->header.hunkbytes);
				if (err != CHDERR_NONE)
					return err;
				break;
			}

			/* uncompressed data */
			case V34_MAP_ENTRY_TYPE_UNCOMPRESSED:
				err = hunk_read_uncompressed(chd, entry->offset, chd->header.hunkbytes, dest);
				if (err != CHDERR_NONE)
					return err;
				break;

			/* mini-compressed data */
			case V34_MAP_ENTRY_TYPE_MINI:
				put_bigendian_uint64(&dest[0], entry->offset);
				for (bytes = 8; bytes < chd->header.hunkbytes; bytes++)
					dest[bytes] = dest[bytes - 8];
				break;

			/* self-referenced data */
			case V34_MAP_ENTRY_TYPE_SELF_HUNK:
#ifdef NEED_CACHE_HUNK
				if (chd->cachehunk == entry->offset && dest == chd->cache)
					break;
#endif
				return hunk_read_into_memory(chd, entry->offset, dest);

			/* parent-referenced data */
			case V34_MAP_ENTRY_TYPE_PARENT_HUNK:
				err = hunk_read_into_memory(chd->parent, entry->offset, dest);
				if (err != CHDERR_NONE)
					return err;
				break;
		}
		return CHDERR_NONE;
	}
	else
	{
		void* codec = NULL;
		/* get a pointer to the map entry */
		uint64_t blockoffs;
		uint32_t blocklen;
#ifdef VERIFY_BLOCK_CRC
		uint16_t blockcrc;
#endif
		uint8_t *rawmap = &chd->header.rawmap[chd->header.mapentrybytes * hunknum];
		UINT8* compressed_bytes;

		/* uncompressed case */
		if (!chd_compressed(&chd->header))
		{
			blockoffs = (uint64_t)get_bigendian_uint32(rawmap) * (uint64_t)chd->header.hunkbytes;
			if (blockoffs != 0) {
				core_fseek(chd->file, blockoffs, SEEK_SET);
				int result = core_fread(chd->file, dest, chd->header.hunkbytes);
			/* TODO
			else if (m_parent_missing)
				throw CHDERR_REQUIRES_PARENT; */
			} else if (chd->parent) {
				err = hunk_read_into_memory(chd->parent, hunknum, dest);
				if (err != CHDERR_NONE)
					return err;
			} else {
				memset(dest, 0, chd->header.hunkbytes);
			}

			return CHDERR_NONE;
		}

		/* compressed case */
		blocklen = get_bigendian_uint24(&rawmap[1]);
		blockoffs = get_bigendian_uint48(&rawmap[4]);
#ifdef VERIFY_BLOCK_CRC
		blockcrc = get_bigendian_uint16(&rawmap[10]);
#endif
		codec = NULL;
		switch (rawmap[0])
		{
			case COMPRESSION_TYPE_0:
			case COMPRESSION_TYPE_1:
			case COMPRESSION_TYPE_2:
			case COMPRESSION_TYPE_3:
				compressed_bytes = hunk_read_compressed(chd, blockoffs, blocklen);
				if (compressed_bytes == NULL)
					return CHDERR_READ_ERROR;
				switch (chd->codecintf[rawmap[0]]->compression)
				{
					case CHD_CODEC_ZLIB:
						codec = &chd->zlib_codec_data;
						break;

					case CHD_CODEC_LZMA:
						codec = &chd->lzma_codec_data;
						break;

					case CHD_CODEC_HUFFMAN:
						codec = &chd->huff_codec_data;
						break;

					case CHD_CODEC_FLAC:
						codec = &chd->flac_codec_data;
						break;

					case CHD_CODEC_ZSTD:
						codec = &chd->zstd_codec_data;
						break;

					case CHD_CODEC_CD_ZLIB:
						codec = &chd->cdzl_codec_data;
						break;

					case CHD_CODEC_CD_LZMA:
						codec = &chd->cdlz_codec_data;
						break;

					case CHD_CODEC_CD_FLAC:
						codec = &chd->cdfl_codec_data;
						break;

					case CHD_CODEC_CD_ZSTD:
						codec = &chd->cdzs_codec_data;
						break;
				}
				if (codec==NULL)
					return CHDERR_CODEC_ERROR;
				err = chd->codecintf[rawmap[0]]->decompress(codec, compressed_bytes, blocklen, dest, chd->header.hunkbytes);
				if (err != CHDERR_NONE)
					return err;
#ifdef VERIFY_BLOCK_CRC
				if (crc16(dest, chd->header.hunkbytes) != blockcrc)
					return CHDERR_DECOMPRESSION_ERROR;
#endif
				return CHDERR_NONE;

			case COMPRESSION_NONE:
				err = hunk_read_uncompressed(chd, blockoffs, blocklen, dest);
				if (err != CHDERR_NONE)
					return err;
#ifdef VERIFY_BLOCK_CRC
				if (crc16(dest, chd->header.hunkbytes) != blockcrc)
					return CHDERR_DECOMPRESSION_ERROR;
#endif
				return CHDERR_NONE;

			case COMPRESSION_SELF:
				return hunk_read_into_memory(chd, blockoffs, dest);

			case COMPRESSION_PARENT:
				if (chd->parent == NULL)
					return CHDERR_REQUIRES_PARENT;
				UINT8 units_in_hunk = chd->header.hunkbytes / chd->header.unitbytes;

				/* blockoffs is aligned to units_in_hunk */
				if (blockoffs % units_in_hunk == 0) {
					return hunk_read_into_memory(chd->parent, blockoffs / units_in_hunk, dest);
				/* blockoffs is not aligned to units_in_hunk */
				} else {
					UINT32 unit_in_hunk = blockoffs % units_in_hunk;
					UINT8 *buf = malloc(chd->header.hunkbytes);
					/* Read first half of hunk which contains blockoffs */
					err = hunk_read_into_memory(chd->parent, blockoffs / units_in_hunk, buf);
					if (err != CHDERR_NONE) {
						free(buf);
						return err;
					}
					memcpy(dest, buf + unit_in_hunk * chd->header.unitbytes, (units_in_hunk - unit_in_hunk) * chd->header.unitbytes);
					/* Read second half of hunk which contains blockoffs */
					err = hunk_read_into_memory(chd->parent, (blockoffs / units_in_hunk) + 1, buf);
					if (err != CHDERR_NONE) {
						free(buf);
						return err;
					}
					memcpy(dest + (units_in_hunk - unit_in_hunk) * chd->header.unitbytes, buf, unit_in_hunk * chd->header.unitbytes);
					free(buf);
				}
		}
		return CHDERR_NONE;
	}

	/* We should not reach this code */
	return CHDERR_DECOMPRESSION_ERROR;
}

/***************************************************************************
    INTERNAL MAP ACCESS
***************************************************************************/

/*-------------------------------------------------
    map_read - read the initial sector map
-------------------------------------------------*/

static chd_error map_read(chd_file *chd)
{
	UINT32 entrysize = (chd->header.version < 3) ? OLD_MAP_ENTRY_SIZE : MAP_ENTRY_SIZE;
	UINT8 raw_map_entries[MAP_STACK_ENTRIES * MAP_ENTRY_SIZE];
	UINT64 fileoffset, maxoffset = 0;
	UINT8 cookie[MAP_ENTRY_SIZE];
	UINT32 count;
	chd_error err;
	UINT32 i;

	/* first allocate memory */
	chd->map = (map_entry *)malloc(sizeof(chd->map[0]) * chd->header.totalhunks);
	if (!chd->map)
		return CHDERR_OUT_OF_MEMORY;

	/* read the map entries in in chunks and extract to the map list */
	fileoffset = chd->header.length;
	for (i = 0; i < chd->header.totalhunks; i += MAP_STACK_ENTRIES)
	{
		/* compute how many entries this time */
		int entries = chd->header.totalhunks - i, j;
		if (entries > MAP_STACK_ENTRIES)
			entries = MAP_STACK_ENTRIES;

		/* read that many */
		core_fseek(chd->file, fileoffset, SEEK_SET);
		count = core_fread(chd->file, raw_map_entries, entries * entrysize);
		if (count != entries * entrysize)
		{
			err = CHDERR_READ_ERROR;
			goto cleanup;
		}
		fileoffset += entries * entrysize;

		/* process that many */
		if (entrysize == MAP_ENTRY_SIZE)
		{
			for (j = 0; j < entries; j++)
				map_extract(&raw_map_entries[j * MAP_ENTRY_SIZE], &chd->map[i + j]);
		}
		else
		{
			for (j = 0; j < entries; j++)
				map_extract_old(&raw_map_entries[j * OLD_MAP_ENTRY_SIZE], &chd->map[i + j], chd->header.hunkbytes);
		}

		/* track the maximum offset */
		for (j = 0; j < entries; j++)
			if ((chd->map[i + j].flags & MAP_ENTRY_FLAG_TYPE_MASK) == V34_MAP_ENTRY_TYPE_COMPRESSED ||
				(chd->map[i + j].flags & MAP_ENTRY_FLAG_TYPE_MASK) == V34_MAP_ENTRY_TYPE_UNCOMPRESSED)
				maxoffset = MAX(maxoffset, chd->map[i + j].offset + chd->map[i + j].length);
	}

	/* verify the cookie */
	core_fseek(chd->file, fileoffset, SEEK_SET);
	count = core_fread(chd->file, &cookie, entrysize);
	if (count != entrysize || memcmp(&cookie, END_OF_LIST_COOKIE, entrysize))
	{
		err = CHDERR_INVALID_FILE;
		goto cleanup;
	}

	/* verify the length */
	if (maxoffset > core_fsize(chd->file))
	{
		err = CHDERR_INVALID_FILE;
		goto cleanup;
	}
	return CHDERR_NONE;

cleanup:
	if (chd->map)
		free(chd->map);
	chd->map = NULL;
	return err;
}

/***************************************************************************
    INTERNAL METADATA ACCESS
***************************************************************************/

/*-------------------------------------------------
    metadata_find_entry - find a metadata entry
-------------------------------------------------*/

static chd_error metadata_find_entry(chd_file *chd, UINT32 metatag, UINT32 metaindex, metadata_entry *metaentry)
{
	/* start at the beginning */
	metaentry->offset = chd->header.metaoffset;
	metaentry->prev = 0;

	/* loop until we run out of options */
	while (metaentry->offset != 0)
	{
		UINT8	raw_meta_header[METADATA_HEADER_SIZE];
		UINT32	count;

		/* read the raw header */
		core_fseek(chd->file, metaentry->offset, SEEK_SET);
		count = core_fread(chd->file, raw_meta_header, sizeof(raw_meta_header));
		if (count != sizeof(raw_meta_header))
			break;

		/* extract the data */
		metaentry->metatag = get_bigendian_uint32(&raw_meta_header[0]);
		metaentry->length = get_bigendian_uint32(&raw_meta_header[4]);
		metaentry->next = get_bigendian_uint64(&raw_meta_header[8]);

		/* flags are encoded in the high byte of length */
		metaentry->flags = metaentry->length >> 24;
		metaentry->length &= 0x00ffffff;

		/* if we got a match, proceed */
		if (metatag == CHDMETATAG_WILDCARD || metaentry->metatag == metatag)
			if (metaindex-- == 0)
				return CHDERR_NONE;

		/* no match, fetch the next link */
		metaentry->prev = metaentry->offset;
		metaentry->offset = metaentry->next;
	}

	/* if we get here, we didn't find it */
	return CHDERR_METADATA_NOT_FOUND;
}

/***************************************************************************
    ZLIB COMPRESSION CODEC
***************************************************************************/

/*-------------------------------------------------
    zlib_codec_init - initialize the ZLIB codec
-------------------------------------------------*/

static chd_error zlib_codec_init(void *codec, uint32_t hunkbytes)
{
	int zerr;
	chd_error err;
	zlib_codec_data *data = (zlib_codec_data*)codec;

	/* clear the buffers */
	memset(data, 0, sizeof(zlib_codec_data));

	/* init the inflater first */
	data->inflater.next_in = (Bytef *)data;	/* bogus, but that's ok */
	data->inflater.avail_in = 0;
	data->inflater.zalloc = zlib_fast_alloc;
	data->inflater.zfree = zlib_fast_free;
	data->inflater.opaque = &data->allocator;
	zerr = inflateInit2(&data->inflater, -MAX_WBITS);

	/* convert errors */
	if (zerr == Z_MEM_ERROR)
		err = CHDERR_OUT_OF_MEMORY;
	else if (zerr != Z_OK)
		err = CHDERR_CODEC_ERROR;
	else
		err = CHDERR_NONE;

	return err;
}

/*-------------------------------------------------
    zlib_codec_free - free data for the ZLIB
    codec
-------------------------------------------------*/

static void zlib_codec_free(void *codec)
{
	zlib_codec_data *data = (zlib_codec_data *)codec;

	/* deinit the streams */
	if (data != NULL)
	{
		inflateEnd(&data->inflater);

		/* free our fast memory */
		zlib_allocator_free(&data->allocator);
	}
}

/*-------------------------------------------------
    zlib_codec_decompress - decompress data using
    the ZLIB codec
-------------------------------------------------*/

static chd_error zlib_codec_decompress(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen)
{
	zlib_codec_data *data = (zlib_codec_data *)codec;
	int zerr;

	/* reset the decompressor */
	data->inflater.next_in = (Bytef *)src;
	data->inflater.avail_in = complen;
	data->inflater.total_in = 0;
	data->inflater.next_out = (Bytef *)dest;
	data->inflater.avail_out = destlen;
	data->inflater.total_out = 0;
	zerr = inflateReset(&data->inflater);
	if (zerr != Z_OK)
		return CHDERR_DECOMPRESSION_ERROR;

	/* do it */
	zerr = inflate(&data->inflater, Z_FINISH);
	if (data->inflater.total_out != destlen)
		return CHDERR_DECOMPRESSION_ERROR;

	return CHDERR_NONE;
}

/*-------------------------------------------------
    zlib_fast_alloc - fast malloc for ZLIB, which
    allocates and frees memory frequently
-------------------------------------------------*/

/* Huge alignment values for possible SIMD optimization by compiler (NEON, SSE, AVX) */
#define ZLIB_MIN_ALIGNMENT_BITS 512
#define ZLIB_MIN_ALIGNMENT_BYTES (ZLIB_MIN_ALIGNMENT_BITS / 8)

static voidpf zlib_fast_alloc(voidpf opaque, uInt items, uInt size)
{
	zlib_allocator *alloc = (zlib_allocator *)opaque;
	uintptr_t paddr = 0;
	UINT32 *ptr;
	int i;

	/* compute the size, rounding to the nearest 1k */
	size = (size * items + 0x3ff) & ~0x3ff;

	/* reuse a hunk if we can */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
	{
		ptr = alloc->allocptr[i];
		if (ptr && size == *ptr)
		{
			/* set the low bit of the size so we don't match next time */
			*ptr |= 1;

			/* return aligned block address */
			return (voidpf)(alloc->allocptr2[i]);
		}
	}

	/* alloc a new one */
    ptr = (UINT32 *)malloc(size + sizeof(UINT32) + ZLIB_MIN_ALIGNMENT_BYTES);
	if (!ptr)
		return NULL;

	/* put it into the list */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
		if (!alloc->allocptr[i])
		{
			alloc->allocptr[i] = ptr;
			paddr = (((uintptr_t)ptr) + sizeof(UINT32) + (ZLIB_MIN_ALIGNMENT_BYTES-1)) & (~(ZLIB_MIN_ALIGNMENT_BYTES-1));
			alloc->allocptr2[i] = (uint32_t*)paddr;
			break;
		}

	/* set the low bit of the size so we don't match next time */
	*ptr = size | 1;

	/* return aligned block address */
	return (voidpf)paddr;
}

/*-------------------------------------------------
    zlib_fast_free - fast free for ZLIB, which
    allocates and frees memory frequently
-------------------------------------------------*/

static void zlib_fast_free(voidpf opaque, voidpf address)
{
	zlib_allocator *alloc = (zlib_allocator *)opaque;
	UINT32 *ptr = (UINT32 *)address;
	int i;

	/* find the hunk */
	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
		if (ptr == alloc->allocptr2[i])
		{
			/* clear the low bit of the size to allow matches */
			*(alloc->allocptr[i]) &= ~1;
			return;
		}
}

/*-------------------------------------------------
    zlib_allocator_free
-------------------------------------------------*/
static void zlib_allocator_free(voidpf opaque)
{
	zlib_allocator *alloc = (zlib_allocator *)opaque;
	int i;

	for (i = 0; i < MAX_ZLIB_ALLOCS; i++)
		if (alloc->allocptr[i])
			free(alloc->allocptr[i]);
}

/*-------------------------------------------------
	core_stdio_fopen - core_file wrapper over fopen
-------------------------------------------------*/
static core_file *core_stdio_fopen(char const *path) {
	core_file *file = malloc(sizeof(core_file));
	if (!file)
		return NULL;
	if (!(file->argp = fopen(path, "rb"))) {
		free(file);
		return NULL;
	}
	file->fsize = core_stdio_fsize;
	file->fread = core_stdio_fread;
	file->fclose = core_stdio_fclose;
	file->fseek = core_stdio_fseek;
	return file;
}

/*-------------------------------------------------
	core_stdio_fsize - core_file function for
	getting file size with stdio
-------------------------------------------------*/
static UINT64 core_stdio_fsize(core_file *file) {
#if defined USE_LIBRETRO_VFS
	#define core_stdio_fseek_impl fseek
	#define core_stdio_ftell_impl ftell
#elif defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(__WIN64__)
	#define core_stdio_fseek_impl _fseeki64
	#define core_stdio_ftell_impl _ftelli64
#elif defined(_LARGEFILE_SOURCE) && defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
	#define core_stdio_fseek_impl fseeko64
	#define core_stdio_ftell_impl ftello64
#elif defined(__PS3__) && !defined(__PSL1GHT__) || defined(__SWITCH__) || defined(__vita__)
	#define core_stdio_fseek_impl(x,y,z) fseek(x,(off_t)y,z)
	#define core_stdio_ftell_impl(x) (off_t)ftell(x)
#else
	#define core_stdio_fseek_impl fseeko
	#define core_stdio_ftell_impl ftello
#endif
	FILE *fp;
	UINT64 p, rv;
	fp = (FILE*)file->argp;

	p = core_stdio_ftell_impl(fp);
	core_stdio_fseek_impl(fp, 0, SEEK_END);
	rv = core_stdio_ftell_impl(fp);
	core_stdio_fseek_impl(fp, p, SEEK_SET);
	return rv;
}

/*-------------------------------------------------
	core_stdio_fread - core_file wrapper over fread
-------------------------------------------------*/
static size_t core_stdio_fread(void *ptr, size_t size, size_t nmemb, core_file *file) {
	return fread(ptr, size, nmemb, (FILE*)file->argp);
}

/*-------------------------------------------------
	core_stdio_fclose - core_file wrapper over fclose
-------------------------------------------------*/
static int core_stdio_fclose(core_file *file) {
	int err = fclose((FILE*)file->argp);
	if (err == 0)
		free(file);
	return err;
}

/*-------------------------------------------------
	core_stdio_fclose_nonowner - don't call fclose because
		we don't own the underlying file, but do free the
		core_file because libchdr did allocate that itself.
-------------------------------------------------*/
static int core_stdio_fclose_nonowner(core_file *file) {
	free(file);
	return 0;
}

/*-------------------------------------------------
	core_stdio_fseek - core_file wrapper over fclose
-------------------------------------------------*/
static int core_stdio_fseek(core_file* file, INT64 offset, int whence) {
	return core_stdio_fseek_impl((FILE*)file->argp, offset, whence);
}
