/*
 Copyright (c) 2016 Mathieu Laurendeau <mat.lau@laposte.net>
 License: GPLv3
 */

#include "PrecompiledHeader.h"
#include "lg_ff.h"

typedef struct
{
	unsigned char num;
	unsigned char den;
} s_coef;

/*
 * \brief Get the spring or damper force coefficient, normalized to [0..1].
 *
 * \param caps the capabilities of the wheel (bitfield of FF_LG_CAPS)
 * \param k    the constant selector
 *
 * \return the force coefficient
 */
static s_coef ff_lg_get_force_coefficient(uint8_t caps, unsigned char k)
{

	s_coef coef;

	if (caps & FF_LG_CAPS_HIGH_RES_COEF)
	{
		coef.num = k;
		coef.den = 0x0F;
	}
	else
	{
		if (caps & FF_LG_CAPS_OLD_LOW_RES_COEF)
		{
			static const s_coef old_coefs[] = {{1, 16}, {1, 8}, {3, 16}, {1, 4}, {3, 8}, {3, 4}, {2, 4}, {4, 4}};
			coef = old_coefs[k];
		}
		else
		{
			static const s_coef coefs[] = {{1, 16}, {1, 8}, {3, 16}, {1, 4}, {3, 8}, {2, 4}, {3, 4}, {4, 4}};
			coef = coefs[k];
		}
	}
	return coef;
}

int16_t ff_lg_get_condition_coef(uint8_t caps, unsigned char k, unsigned char s, int16_t max /*= SHRT_MAX*/)
{

	s_coef coef = ff_lg_get_force_coefficient(caps, k);
	int value = (s ? -max : max) * coef.num / coef.den;
	return value;
}

uint16_t ff_lg_get_spring_deadband(uint8_t caps, unsigned char d, unsigned char dL, uint16_t max /*= USHRT_MAX*/)
{

	uint16_t deadband;
	if (caps & FF_LG_CAPS_HIGH_RES_DEADBAND)
	{
		deadband = ((d << 3) | dL) * max / 0x7FF;
	}
	else
	{
		deadband = d * max / UCHAR_MAX;
	}
	return deadband;
}

uint16_t ff_lg_get_damper_clip(uint8_t caps, unsigned char c)
{

	uint16_t clip;
	if (caps & FF_LG_CAPS_DAMPER_CLIP)
	{
		clip = c * USHRT_MAX / UCHAR_MAX;
	}
	else
	{
		clip = USHRT_MAX;
	}
	return clip;
}