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

#ifndef EVDEV_FF_H
#define EVDEV_FF_H

#include <linux/input.h>
#include "USB/usb-pad/usb-pad.h"

namespace usb_pad
{
	namespace evdev
	{

		class EvdevFF : public FFDevice
		{
		public:
			EvdevFF(int fd, bool gain_enabled, int gain, bool ac_managed, int ac_strength);
			~EvdevFF();

			void SetConstantForce(int level);
			void SetSpringForce(const parsed_ff_data& ff);
			void SetDamperForce(const parsed_ff_data& ff);
			void SetFrictionForce(const parsed_ff_data& ff);
			void SetAutoCenter(int value);
			void SetGain(int gain);
			void DisableForce(EffectID force);

		private:
			int mHandle;
			ff_effect mEffect;
			int mEffIds[5] = {-1, -1, -1, -1, -1}; //save ids just in case

			bool mUseRumble;
			int mLastValue;
			bool m_gain_enabled;
			int m_gain;
			bool m_ac_managed;
			int m_ac_strength;
		};

	} // namespace evdev
} // namespace usb_pad
#endif
