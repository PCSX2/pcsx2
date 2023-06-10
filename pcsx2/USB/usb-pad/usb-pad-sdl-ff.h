/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#pragma once

#include "USB/usb-pad/usb-pad.h"

#ifdef SDL_BUILD

#include "Input/SDLInputSource.h"

namespace usb_pad
{
	class SDLFFDevice : public FFDevice
	{
	public:
		~SDLFFDevice() override;

		static std::unique_ptr<SDLFFDevice> Create(const std::string_view& device);

		void SetConstantForce(int level) override;
		void SetSpringForce(const parsed_ff_data& ff) override;
		void SetDamperForce(const parsed_ff_data& ff) override;
		void SetFrictionForce(const parsed_ff_data& ff) override;
		void SetAutoCenter(int value) override;
		void DisableForce(EffectID force) override;

	private:
		SDLFFDevice(SDL_Haptic* haptic);

		void CreateEffects(const std::string_view& device);
		void DestroyEffects();

		SDL_Haptic* m_haptic = nullptr;

		SDL_HapticEffect m_constant_effect;
		int m_constant_effect_id = -1;
		bool m_constant_effect_running = false;

		SDL_HapticEffect m_spring_effect;
		int m_spring_effect_id = -1;
		bool m_spring_effect_running = false;

		SDL_HapticEffect m_damper_effect;
		int m_damper_effect_id = -1;
		bool m_damper_effect_running = false;

		SDL_HapticEffect m_friction_effect;
		int m_friction_effect_id = -1;
		bool m_friction_effect_running = false;

		bool m_autocenter_supported = false;
	};
} // namespace usb_pad

#endif
