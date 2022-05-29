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

#include "evdev-ff.h"
#include "USB/usb-pad/lg/lg_ff.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace usb_pad
{
	namespace evdev
	{

#define BITS_TO_UCHAR(x) \
	(((x) + 8 * sizeof(unsigned char) - 1) / (8 * sizeof(unsigned char)))
#define testBit(bit, array) ((((uint8_t*)(array))[(bit) / 8] >> ((bit) % 8)) & 1)

		EvdevFF::EvdevFF(int fd, bool gain_enabled, int gain, bool ac_managed, int ac_strength)
			: mHandle(fd)
			, mUseRumble(false)
			, mLastValue(0)
			, m_gain_enabled(gain_enabled)
			, m_gain(gain)
			//, m_ac_managed(ac_enabled)
			, m_ac_managed(true)
			, m_ac_strength(ac_strength)
		{
			unsigned char features[BITS_TO_UCHAR(FF_MAX)];
			if (ioctl(mHandle, EVIOCGBIT(EV_FF, sizeof(features)), features) < 0)
			{
			}

			int effects = 0;
			if (ioctl(mHandle, EVIOCGEFFECTS, &effects) < 0)
			{
			}

			if (!testBit(FF_CONSTANT, features))
			{
				if (testBit(FF_RUMBLE, features))
					mUseRumble = true;
			}

			if (!testBit(FF_SPRING, features))
			{
			}

			if (!testBit(FF_DAMPER, features))
			{
			}

			if (!testBit(FF_GAIN, features))
			{
			}

			if (!testBit(FF_AUTOCENTER, features))
			{
			}

			memset(&mEffect, 0, sizeof(mEffect));

			// TODO check features and do FF_RUMBLE instead if gamepad?
			// XXX linux status (hid-lg4ff.c) - only constant and autocenter are implemented
			mEffect.u.constant.level = 0; /* Strength : 0x2000 == 25 % */
			// Logitech wheels' force vs turn direction: 255 - left, 127/128 - neutral, 0 - right
			// left direction
			mEffect.direction = 0x4000;
			mEffect.trigger.button = 0;
			mEffect.trigger.interval = 0;
			mEffect.replay.length = 0x7FFFUL; /* mseconds */
			mEffect.replay.delay = 0;

			if (m_gain_enabled)
				SetGain(m_gain);

			m_ac_strength = std::min(100, std::max(0, m_ac_strength));
			if (ac_managed)
				SetAutoCenter(0); // default to off
			else
				SetAutoCenter(m_ac_strength);

			m_ac_managed = ac_managed;
		}

		EvdevFF::~EvdevFF()
		{
			for (int i = 0; i < (int)countof(mEffIds); i++)
			{
				if (mEffIds[i] != -1 && ioctl(mHandle, EVIOCRMFF, mEffIds[i]) == -1)
				{
				}
			}
		}

		void EvdevFF::DisableForce(EffectID force)
		{
			struct input_event play;
			play.type = EV_FF;
			play.code = mEffIds[force];
			play.value = 0;
			if (write(mHandle, (const void*)&play, sizeof(play)) == -1)
			{
			}
		}

		void EvdevFF::SetConstantForce(/*const parsed_ff_data& ff*/ int level)
		{
			struct input_event play;
			play.type = EV_FF;
			play.value = 1;
			mEffect.u = {};

			if (!mUseRumble)
			{
				mEffect.type = FF_CONSTANT;
				mEffect.id = mEffIds[EFF_CONSTANT];
				mEffect.u.constant.level = /*ff.u.constant.*/ level;
				// 		mEffect.u.constant.envelope.attack_length = 0;//0x100;
				// 		mEffect.u.constant.envelope.attack_level = 0;
				// 		mEffect.u.constant.envelope.fade_length = 0;//0x100;
				// 		mEffect.u.constant.envelope.fade_level = 0;

				if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0)
				{
					return;
				}
				play.code = mEffect.id;
				mEffIds[EFF_CONSTANT] = mEffect.id;
			}
			else
			{

				mEffect.type = FF_RUMBLE;
				mEffect.id = mEffIds[EFF_RUMBLE];

				mEffect.replay.length = 500;
				mEffect.replay.delay = 0;
				mEffect.u.rumble.weak_magnitude = 0;
				mEffect.u.rumble.strong_magnitude = 0;

				int mag = std::abs(/*ff.u.constant.*/ level);
				int diff = std::abs(mag - mLastValue);

				// TODO random limits to cull down on too much rumble
				if (diff > 8292 && diff < 32767)
					mEffect.u.rumble.weak_magnitude = mag;
				if (diff / 8192 > 0)
					mEffect.u.rumble.strong_magnitude = mag;

				mLastValue = mag;

				if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0)
				{
					return;
				}
				play.code = mEffect.id;
				mEffIds[EFF_RUMBLE] = mEffect.id;
			}

			if (write(mHandle, (const void*)&play, sizeof(play)) == -1)
			{
			}
		}

		void EvdevFF::SetSpringForce(const parsed_ff_data& ff)
		{
			struct input_event play;
			play.type = EV_FF;
			play.value = 1;

			mEffect.type = FF_SPRING;
			mEffect.id = mEffIds[EFF_SPRING];
			mEffect.u = {};
			mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
			mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
			mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
			mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
			mEffect.u.condition[0].center = ff.u.condition.center;
			mEffect.u.condition[0].deadband = ff.u.condition.deadband;

			if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0)
			{
				return;
			}

			play.code = mEffect.id;
			mEffIds[EFF_SPRING] = mEffect.id;

			if (write(mHandle, (const void*)&play, sizeof(play)) == -1)
			{
			}
		}

		void EvdevFF::SetDamperForce(const parsed_ff_data& ff)
		{
			struct input_event play;
			play.type = EV_FF;
			play.value = 1;

			mEffect.u = {};
			mEffect.type = FF_DAMPER;
			mEffect.id = mEffIds[EFF_DAMPER];
			mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
			mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
			mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
			mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
			mEffect.u.condition[0].center = ff.u.condition.center;
			mEffect.u.condition[0].deadband = ff.u.condition.deadband;


			if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0)
			{
				return;
			}

			play.code = mEffect.id;
			mEffIds[EFF_DAMPER] = mEffect.id;

			if (write(mHandle, (const void*)&play, sizeof(play)) == -1)
			{
			}
		}

		void EvdevFF::SetFrictionForce(const parsed_ff_data& ff)
		{
			struct input_event play;
			play.type = EV_FF;
			play.value = 1;

			mEffect.u = {};
			mEffect.type = FF_FRICTION;
			mEffect.id = mEffIds[EFF_FRICTION];
			mEffect.u.condition[0].left_saturation = ff.u.condition.left_saturation;
			mEffect.u.condition[0].right_saturation = ff.u.condition.right_saturation;
			mEffect.u.condition[0].left_coeff = ff.u.condition.left_coeff;
			mEffect.u.condition[0].right_coeff = ff.u.condition.right_coeff;
			mEffect.u.condition[0].center = ff.u.condition.center;
			mEffect.u.condition[0].deadband = ff.u.condition.deadband;

			if (ioctl(mHandle, EVIOCSFF, &(mEffect)) < 0)
			{
				return;
			}

			play.code = mEffect.id;
			mEffIds[EFF_FRICTION] = mEffect.id;

			if (write(mHandle, (const void*)&play, sizeof(play)) == -1)
			{
			}
		}

		void EvdevFF::SetAutoCenter(int value)
		{
			if (!m_ac_managed)
				return;
			struct input_event ie;
			value = value * m_ac_strength / 100;

			ie.type = EV_FF;
			ie.code = FF_AUTOCENTER;
			ie.value = value * 0xFFFFUL / 100;

			if (write(mHandle, &ie, sizeof(ie)) == -1)
			{
			}
		}

		void EvdevFF::SetGain(int gain /* between 0 and 100 */)
		{
			struct input_event ie;

			ie.type = EV_FF;
			ie.code = FF_GAIN;
			ie.value = 0xFFFFUL * gain / 100;

			if (write(mHandle, &ie, sizeof(ie)) == -1)
			{
			}
		}

	} // namespace evdev
} // namespace usb_pad
