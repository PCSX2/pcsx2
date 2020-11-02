/*
 Copyright (c) 2016 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#ifndef FF_LG_H_
#define FF_LG_H_
#include <climits>
#include <cstdint>

#define FF_LG_CAPS_HIGH_RES_COEF (1 << 0)
#define FF_LG_CAPS_OLD_LOW_RES_COEF (1 << 1)
#define FF_LG_CAPS_HIGH_RES_DEADBAND (1 << 2)
#define FF_LG_CAPS_DAMPER_CLIP (1 << 3)

/*
 * Convert a Logitech wheel position to a signed 16-bit value.
 *
 * input values 127 and 128 are center positions and are translated to output value 0
 */
static inline int16_t ff_lg_u8_to_s16(uint8_t c, int16_t max = SHRT_MAX)
{
	// 127 and 128 are center positions
	if (c < 128)
	{
		++c;
	}
	int value = (c - 128) * max / 127;
	return value;
}

static inline uint16_t ff_lg_u8_to_u16(uint8_t c, uint16_t max = USHRT_MAX)
{
	return c * max / UCHAR_MAX;
}

static inline int16_t ff_lg_u16_to_s16(uint16_t s)
{
	// 32767 and 32768 are center positions
	int value = s - 32768;
	if (value < 0)
	{
		++value;
	}
	return value;
}

int16_t ff_lg_get_condition_coef(uint8_t caps, unsigned char k, unsigned char s, int16_t max = SHRT_MAX);
uint16_t ff_lg_get_spring_deadband(uint8_t caps, unsigned char d, unsigned char dL, uint16_t max = USHRT_MAX);
uint16_t ff_lg_get_damper_clip(uint8_t caps, unsigned char c);

#endif