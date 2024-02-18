/***************************************************************************

    chd.h

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

#pragma once

#ifndef __CHD_H__
#define __CHD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libchdr/coretypes.h>
#include <libchdr/chdconfig.h>
#include <stdbool.h>

/***************************************************************************

    Compressed Hunks of Data header format. All numbers are stored in
    Motorola (big-endian) byte ordering. The header is 76 (V1) or 80 (V2)
    bytes long.

    V1 header:

    [  0] char   tag[8];        // 'MComprHD'
    [  8] UINT32 length;        // length of header (including tag and length fields)
    [ 12] UINT32 version;       // drive format version
    [ 16] UINT32 flags;         // flags (see below)
    [ 20] UINT32 compression;   // compression type
    [ 24] UINT32 hunksize;      // 512-byte sectors per hunk
    [ 28] UINT32 totalhunks;    // total # of hunks represented
    [ 32] UINT32 cylinders;     // number of cylinders on hard disk
    [ 36] UINT32 heads;         // number of heads on hard disk
    [ 40] UINT32 sectors;       // number of sectors on hard disk
    [ 44] UINT8  md5[16];       // MD5 checksum of raw data
    [ 60] UINT8  parentmd5[16]; // MD5 checksum of parent file
    [ 76] (V1 header length)

    V2 header:

    [  0] char   tag[8];        // 'MComprHD'
    [  8] UINT32 length;        // length of header (including tag and length fields)
    [ 12] UINT32 version;       // drive format version
    [ 16] UINT32 flags;         // flags (see below)
    [ 20] UINT32 compression;   // compression type
    [ 24] UINT32 hunksize;      // seclen-byte sectors per hunk
    [ 28] UINT32 totalhunks;    // total # of hunks represented
    [ 32] UINT32 cylinders;     // number of cylinders on hard disk
    [ 36] UINT32 heads;         // number of heads on hard disk
    [ 40] UINT32 sectors;       // number of sectors on hard disk
    [ 44] UINT8  md5[16];       // MD5 checksum of raw data
    [ 60] UINT8  parentmd5[16]; // MD5 checksum of parent file
    [ 76] UINT32 seclen;        // number of bytes per sector
    [ 80] (V2 header length)

    V3 header:

    [  0] char   tag[8];        // 'MComprHD'
    [  8] UINT32 length;        // length of header (including tag and length fields)
    [ 12] UINT32 version;       // drive format version
    [ 16] UINT32 flags;         // flags (see below)
    [ 20] UINT32 compression;   // compression type
    [ 24] UINT32 totalhunks;    // total # of hunks represented
    [ 28] UINT64 logicalbytes;  // logical size of the data (in bytes)
    [ 36] UINT64 metaoffset;    // offset to the first blob of metadata
    [ 44] UINT8  md5[16];       // MD5 checksum of raw data
    [ 60] UINT8  parentmd5[16]; // MD5 checksum of parent file
    [ 76] UINT32 hunkbytes;     // number of bytes per hunk
    [ 80] UINT8  sha1[20];      // SHA1 checksum of raw data
    [100] UINT8  parentsha1[20];// SHA1 checksum of parent file
    [120] (V3 header length)

    V4 header:

    [  0] char   tag[8];        // 'MComprHD'
    [  8] UINT32 length;        // length of header (including tag and length fields)
    [ 12] UINT32 version;       // drive format version
    [ 16] UINT32 flags;         // flags (see below)
    [ 20] UINT32 compression;   // compression type
    [ 24] UINT32 totalhunks;    // total # of hunks represented
    [ 28] UINT64 logicalbytes;  // logical size of the data (in bytes)
    [ 36] UINT64 metaoffset;    // offset to the first blob of metadata
    [ 44] UINT32 hunkbytes;     // number of bytes per hunk
    [ 48] UINT8  sha1[20];      // combined raw+meta SHA1
    [ 68] UINT8  parentsha1[20];// combined raw+meta SHA1 of parent
    [ 88] UINT8  rawsha1[20];   // raw data SHA1
    [108] (V4 header length)

    Flags:
        0x00000001 - set if this drive has a parent
        0x00000002 - set if this drive allows writes

   =========================================================================

    V5 header:

    [  0] char   tag[8];        // 'MComprHD'
    [  8] uint32_t length;        // length of header (including tag and length fields)
    [ 12] uint32_t version;       // drive format version
    [ 16] uint32_t compressors[4];// which custom compressors are used?
    [ 32] uint64_t logicalbytes;  // logical size of the data (in bytes)
    [ 40] uint64_t mapoffset;     // offset to the map
    [ 48] uint64_t metaoffset;    // offset to the first blob of metadata
    [ 56] uint32_t hunkbytes;     // number of bytes per hunk (512k maximum)
    [ 60] uint32_t unitbytes;     // number of bytes per unit within each hunk
    [ 64] uint8_t  rawsha1[20];   // raw data SHA1
    [ 84] uint8_t  sha1[20];      // combined raw+meta SHA1
    [104] uint8_t  parentsha1[20];// combined raw+meta SHA1 of parent
    [124] (V5 header length)

    If parentsha1 != 0, we have a parent (no need for flags)
    If compressors[0] == 0, we are uncompressed (including maps)

    V5 uncompressed map format:

    [  0] uint32_t offset;        // starting offset / hunk size

    V5 compressed map format header:

    [  0] uint32_t length;        // length of compressed map
    [  4] UINT48 datastart;     // offset of first block
    [ 10] uint16_t crc;           // crc-16 of the map
    [ 12] uint8_t lengthbits;     // bits used to encode complength
    [ 13] uint8_t hunkbits;       // bits used to encode self-refs
    [ 14] uint8_t parentunitbits; // bits used to encode parent unit refs
    [ 15] uint8_t reserved;       // future use
    [ 16] (compressed header length)

    Each compressed map entry, once expanded, looks like:

    [  0] uint8_t compression;    // compression type
    [  1] UINT24 complength;    // compressed length
    [  4] UINT48 offset;        // offset
    [ 10] uint16_t crc;           // crc-16 of the data

***************************************************************************/


/***************************************************************************
    CONSTANTS
***************************************************************************/

/* header information */
#define CHD_HEADER_VERSION			5
#define CHD_V1_HEADER_SIZE			76
#define CHD_V2_HEADER_SIZE			80
#define CHD_V3_HEADER_SIZE			120
#define CHD_V4_HEADER_SIZE			108
#define CHD_V5_HEADER_SIZE          124

#define CHD_MAX_HEADER_SIZE			CHD_V5_HEADER_SIZE

/* checksumming information */
#define CHD_MD5_BYTES				16
#define CHD_SHA1_BYTES				20

/* CHD global flags */
#define CHDFLAGS_HAS_PARENT			0x00000001
#define CHDFLAGS_IS_WRITEABLE		0x00000002
#define CHDFLAGS_UNDEFINED			0xfffffffc

#define CHD_MAKE_TAG(a,b,c,d)       (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

/* compression types */
#define CHDCOMPRESSION_NONE			0
#define CHDCOMPRESSION_ZLIB			1
#define CHDCOMPRESSION_ZLIB_PLUS	2
#define CHDCOMPRESSION_AV			3

#define CHD_CODEC_NONE 0
#define CHD_CODEC_ZLIB				CHD_MAKE_TAG('z','l','i','b')
#define CHD_CODEC_LZMA				CHD_MAKE_TAG('l','z','m','a')
#define CHD_CODEC_HUFFMAN 			CHD_MAKE_TAG('h','u','f','f')
#define CHD_CODEC_FLAC				CHD_MAKE_TAG('f','l','a','c')
#define CHD_CODEC_ZSTD				CHD_MAKE_TAG('z', 's', 't', 'd')
/* general codecs with CD frontend */
#define CHD_CODEC_CD_ZLIB			CHD_MAKE_TAG('c','d','z','l')
#define CHD_CODEC_CD_LZMA			CHD_MAKE_TAG('c','d','l','z')
#define CHD_CODEC_CD_FLAC			CHD_MAKE_TAG('c','d','f','l')
#define CHD_CODEC_CD_ZSTD			CHD_MAKE_TAG('c','d','z','s')

/* A/V codec configuration parameters */
#define AV_CODEC_COMPRESS_CONFIG	1
#define AV_CODEC_DECOMPRESS_CONFIG	2

/* metadata parameters */
#define CHDMETATAG_WILDCARD			0
#define CHD_METAINDEX_APPEND		((UINT32)-1)

/* metadata flags */
#define CHD_MDFLAGS_CHECKSUM		0x01		/* indicates data is checksummed */

/* standard hard disk metadata */
#define HARD_DISK_METADATA_TAG		CHD_MAKE_TAG('G','D','D','D')
#define HARD_DISK_METADATA_FORMAT	"CYLS:%d,HEADS:%d,SECS:%d,BPS:%d"

/* hard disk identify information */
#define HARD_DISK_IDENT_METADATA_TAG CHD_MAKE_TAG('I','D','N','T')

/* hard disk key information */
#define HARD_DISK_KEY_METADATA_TAG	CHD_MAKE_TAG('K','E','Y',' ')

/* pcmcia CIS information */
#define PCMCIA_CIS_METADATA_TAG		CHD_MAKE_TAG('C','I','S',' ')

/* standard CD-ROM metadata */
#define CDROM_OLD_METADATA_TAG		CHD_MAKE_TAG('C','H','C','D')
#define CDROM_TRACK_METADATA_TAG	CHD_MAKE_TAG('C','H','T','R')
#define CDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d"
#define CDROM_TRACK_METADATA2_TAG	CHD_MAKE_TAG('C','H','T','2')
#define CDROM_TRACK_METADATA2_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"
#define GDROM_OLD_METADATA_TAG		CHD_MAKE_TAG('C','H','G','T')
#define GDROM_TRACK_METADATA_TAG	CHD_MAKE_TAG('C', 'H', 'G', 'D')
#define GDROM_TRACK_METADATA_FORMAT	"TRACK:%d TYPE:%s SUBTYPE:%s FRAMES:%d PAD:%d PREGAP:%d PGTYPE:%s PGSUB:%s POSTGAP:%d"

/* standard A/V metadata */
#define AV_METADATA_TAG				CHD_MAKE_TAG('A','V','A','V')
#define AV_METADATA_FORMAT			"FPS:%d.%06d WIDTH:%d HEIGHT:%d INTERLACED:%d CHANNELS:%d SAMPLERATE:%d"

/* A/V laserdisc frame metadata */
#define AV_LD_METADATA_TAG			CHD_MAKE_TAG('A','V','L','D')

/* CHD open values */
#define CHD_OPEN_READ				1
#define CHD_OPEN_READWRITE			2
#define CHD_OPEN_TRANSFER_FILE 4 /* Freeing of the FILE* is now libchdr's responsibility if open was successful */

/* error types */
enum _chd_error
{
	CHDERR_NONE,
	CHDERR_NO_INTERFACE,
	CHDERR_OUT_OF_MEMORY,
	CHDERR_INVALID_FILE,
	CHDERR_INVALID_PARAMETER,
	CHDERR_INVALID_DATA,
	CHDERR_FILE_NOT_FOUND,
	CHDERR_REQUIRES_PARENT,
	CHDERR_FILE_NOT_WRITEABLE,
	CHDERR_READ_ERROR,
	CHDERR_WRITE_ERROR,
	CHDERR_CODEC_ERROR,
	CHDERR_INVALID_PARENT,
	CHDERR_HUNK_OUT_OF_RANGE,
	CHDERR_DECOMPRESSION_ERROR,
	CHDERR_COMPRESSION_ERROR,
	CHDERR_CANT_CREATE_FILE,
	CHDERR_CANT_VERIFY,
	CHDERR_NOT_SUPPORTED,
	CHDERR_METADATA_NOT_FOUND,
	CHDERR_INVALID_METADATA_SIZE,
	CHDERR_UNSUPPORTED_VERSION,
	CHDERR_VERIFY_INCOMPLETE,
	CHDERR_INVALID_METADATA,
	CHDERR_INVALID_STATE,
	CHDERR_OPERATION_PENDING,
	CHDERR_NO_ASYNC_OPERATION,
	CHDERR_UNSUPPORTED_FORMAT
};
typedef enum _chd_error chd_error;



/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

/* opaque types */
typedef struct _chd_file chd_file;


/* extract header structure (NOT the on-disk header structure) */
typedef struct _chd_header chd_header;
struct _chd_header
{
	UINT32		length;						/* length of header data */
	UINT32		version;					/* drive format version */
	UINT32		flags;						/* flags field */
	UINT32		compression[4];				/* compression type */
	UINT32		hunkbytes;					/* number of bytes per hunk */
	UINT32		totalhunks;					/* total # of hunks represented */
	UINT64		logicalbytes;				/* logical size of the data */
	UINT64		metaoffset;					/* offset in file of first metadata */
	UINT64		mapoffset;					/* TOOD V5 */
	UINT8		md5[CHD_MD5_BYTES];			/* overall MD5 checksum */
	UINT8		parentmd5[CHD_MD5_BYTES];	/* overall MD5 checksum of parent */
	UINT8		sha1[CHD_SHA1_BYTES];		/* overall SHA1 checksum */
	UINT8		rawsha1[CHD_SHA1_BYTES];	/* SHA1 checksum of raw data */
	UINT8		parentsha1[CHD_SHA1_BYTES];	/* overall SHA1 checksum of parent */
	UINT32		unitbytes;					/* TODO V5 */
	UINT64		unitcount;					/* TODO V5 */
    UINT32      hunkcount;                  /* TODO V5 */

    /* map information */
    UINT32      mapentrybytes;              /* length of each entry in a map (V5) */
    UINT8*      rawmap;                     /* raw map data */

	UINT32		obsolete_cylinders;			/* obsolete field -- do not use! */
	UINT32		obsolete_sectors;			/* obsolete field -- do not use! */
	UINT32		obsolete_heads;				/* obsolete field -- do not use! */
	UINT32		obsolete_hunksize;			/* obsolete field -- do not use! */
};


/* structure for returning information about a verification pass */
typedef struct _chd_verify_result chd_verify_result;
struct _chd_verify_result
{
	UINT8		md5[CHD_MD5_BYTES];			/* overall MD5 checksum */
	UINT8		sha1[CHD_SHA1_BYTES];		/* overall SHA1 checksum */
	UINT8		rawsha1[CHD_SHA1_BYTES];	/* SHA1 checksum of raw data */
	UINT8		metasha1[CHD_SHA1_BYTES];	/* SHA1 checksum of metadata */
};



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

#ifdef _MSC_VER
#ifdef CHD_DLL
#ifdef CHD_DLL_EXPORTS
#define CHD_EXPORT __declspec(dllexport)
#else
#define CHD_EXPORT __declspec(dllimport)
#endif
#else
#define CHD_EXPORT
#endif
#else
#define CHD_EXPORT __attribute__ ((visibility("default")))
#endif

/* ----- CHD file management ----- */

/* create a new CHD file fitting the given description */
/* chd_error chd_create(const char *filename, UINT64 logicalbytes, UINT32 hunkbytes, UINT32 compression, chd_file *parent); */

/* same as chd_create(), but accepts an already-opened core_file object */
/* chd_error chd_create_file(core_file *file, UINT64 logicalbytes, UINT32 hunkbytes, UINT32 compression, chd_file *parent); */

/* open an existing CHD file */
CHD_EXPORT chd_error chd_open_core_file(core_file *file, int mode, chd_file *parent, chd_file **chd);
CHD_EXPORT chd_error chd_open_file(FILE *file, int mode, chd_file *parent, chd_file **chd);
CHD_EXPORT chd_error chd_open(const char *filename, int mode, chd_file *parent, chd_file **chd);

/* precache underlying file */
CHD_EXPORT chd_error chd_precache(chd_file *chd);
CHD_EXPORT chd_error chd_precache_progress(chd_file* chd, void(*progress)(size_t pos, size_t total, void* param), void* param);

/* close a CHD file */
CHD_EXPORT void chd_close(chd_file *chd);

/* return the associated core_file */
CHD_EXPORT core_file *chd_core_file(chd_file *chd);

/* return the overall size of a CHD, and any of its parents */
CHD_EXPORT UINT64 chd_get_compressed_size(chd_file* chd);

/* return an error string for the given CHD error */
CHD_EXPORT const char *chd_error_string(chd_error err);


/* ----- CHD header management ----- */

/* return a pointer to the extracted CHD header data */
CHD_EXPORT const chd_header *chd_get_header(chd_file *chd);

/* read CHD header data from file into the pointed struct */
CHD_EXPORT chd_error chd_read_header_core_file(core_file *file, chd_header *header);
CHD_EXPORT chd_error chd_read_header_file(FILE *file, chd_header *header);
CHD_EXPORT chd_error chd_read_header(const char *filename, chd_header *header);
CHD_EXPORT bool chd_is_matching_parent(const chd_header* header, const chd_header* parent_header);



/* ----- core data read/write ----- */

/* read one hunk from the CHD file */
CHD_EXPORT chd_error chd_read(chd_file *chd, UINT32 hunknum, void *buffer);



/* ----- metadata management ----- */

/* get indexed metadata of a particular sort */
CHD_EXPORT chd_error chd_get_metadata(chd_file *chd, UINT32 searchtag, UINT32 searchindex, void *output, UINT32 outputlen, UINT32 *resultlen, UINT32 *resulttag, UINT8 *resultflags);




/* ----- codec interfaces ----- */

/* set internal codec parameters */
CHD_EXPORT chd_error chd_codec_config(chd_file *chd, int param, void *config);

/* return a string description of a codec */
CHD_EXPORT const char *chd_get_codec_name(UINT32 codec);

#ifdef __cplusplus
}
#endif

#endif /* __CHD_H__ */
