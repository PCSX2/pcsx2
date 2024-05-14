// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "USB/usb-pad/usb-pad.h"
#include "Input/SDLInputSource.h"

namespace usb_pad
{
	class SDLFFDevice : public FFDevice
	{
	public:
		~SDLFFDevice() override;

		static std::unique_ptr<SDLFFDevice> Create(const std::string_view device);

		void SetConstantForce(int level) override;
		void SetSpringForce(const parsed_ff_data& ff) override;
		void SetDamperForce(const parsed_ff_data& ff) override;
		void SetFrictionForce(const parsed_ff_data& ff) override;
		void SetAutoCenter(int value) override;
		void DisableForce(EffectID force) override;

	private:
		SDLFFDevice(SDL_Haptic* haptic);

		void CreateEffects(const std::string_view device);
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
