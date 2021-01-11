/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
***************************************************************************

    cdrom.h

    Generic MAME cd-rom implementation

***************************************************************************/

#pragma once

#ifndef __CDROM_H__
#define __CDROM_H__

#include <stdint.h>
#include "chdconfig.h"

/***************************************************************************
    CONSTANTS
***************************************************************************/

/* tracks are padded to a multiple of this many frames */
#define CD_TRACK_PADDING   	(4)
#define CD_MAX_TRACKS           (99)    /* AFAIK the theoretical limit */
#define CD_MAX_SECTOR_DATA      (2352)
#define CD_MAX_SUBCODE_DATA     (96)

#define CD_FRAME_SIZE           (CD_MAX_SECTOR_DATA + CD_MAX_SUBCODE_DATA)
#define CD_FRAMES_PER_HUNK      (8)

#define CD_METADATA_WORDS       (1+(CD_MAX_TRACKS * 6))

enum
{
	CD_TRACK_MODE1 = 0,         /* mode 1 2048 bytes/sector */
	CD_TRACK_MODE1_RAW,         /* mode 1 2352 bytes/sector */
	CD_TRACK_MODE2,             /* mode 2 2336 bytes/sector */
	CD_TRACK_MODE2_FORM1,       /* mode 2 2048 bytes/sector */
	CD_TRACK_MODE2_FORM2,       /* mode 2 2324 bytes/sector */
	CD_TRACK_MODE2_FORM_MIX,    /* mode 2 2336 bytes/sector */
	CD_TRACK_MODE2_RAW,         /* mode 2 2352 bytes / sector */
	CD_TRACK_AUDIO,         /* redbook audio track 2352 bytes/sector (588 samples) */

	CD_TRACK_RAW_DONTCARE       /* special flag for cdrom_read_data: just return me whatever is there */
};

enum
{
	CD_SUB_NORMAL = 0,          /* "cooked" 96 bytes per sector */
	CD_SUB_RAW,                 /* raw uninterleaved 96 bytes per sector */
	CD_SUB_NONE                 /* no subcode data stored */
};

#define CD_FLAG_GDROM   0x00000001  /* disc is a GD-ROM, all tracks should be stored with GD-ROM metadata */
#define CD_FLAG_GDROMLE 0x00000002  /* legacy GD-ROM, with little-endian CDDA data */

/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

#ifdef WANT_RAW_DATA_SECTOR
/* ECC utilities */
int ecc_verify(const uint8_t *sector);
void ecc_generate(uint8_t *sector);
void ecc_clear(uint8_t *sector);
#endif



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

static inline uint32_t msf_to_lba(uint32_t msf)
{
	return ( ((msf&0x00ff0000)>>16) * 60 * 75) + (((msf&0x0000ff00)>>8) * 75) + ((msf&0x000000ff)>>0);
}

static inline uint32_t lba_to_msf(uint32_t lba)
{
	uint8_t m, s, f;

	m = lba / (60 * 75);
	lba -= m * (60 * 75);
	s = lba / 75;
	f = lba % 75;

	return ((m / 10) << 20) | ((m % 10) << 16) |
			((s / 10) << 12) | ((s % 10) <<  8) |
			((f / 10) <<  4) | ((f % 10) <<  0);
}

/**
 * segacd needs it like this.. investigate
 * Angelo also says PCE tracks often start playing at the
 * wrong address.. related?
 **/
static inline uint32_t lba_to_msf_alt(int lba)
{
	uint32_t ret = 0;

	ret |= ((lba / (60 * 75))&0xff)<<16;
	ret |= (((lba / 75) % 60)&0xff)<<8;
	ret |= ((lba % 75)&0xff)<<0;

	return ret;
}

#endif  /* __CDROM_H__ */
