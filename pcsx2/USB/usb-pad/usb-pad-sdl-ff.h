// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "USB/usb-pad/usb-pad.h"
#include "Input/SDLInputSource.h"
#ifdef __WIN32__
#include <sdl_haptic.h>
#include <dinput.h>
#endif

#ifdef __WIN32__
// Copied some internal structure definitions from SDL's source
// in order to scoop out their inner bits that are excluded from
// public-facing headers.

// It's ugly, but it works for the purposes of this hack/PoC

struct haptic_hweffect
{
	DIEFFECT effect;
	LPDIRECTINPUTEFFECT ref;
};

struct haptic_effect
{
	SDL_HapticEffect effect; // The current event
	struct haptic_hweffect* hweffect; // The hardware behind the event
};

struct _SDL_Haptic
{
	Uint8 index; /* Stores index it is attached to */

	struct haptic_effect* effects; /* Allocated effects */
	int neffects; /* Maximum amount of effects */
	int nplaying; /* Maximum amount of effects to play at the same time */
	unsigned int supported; /* Supported effects */
	int naxes; /* Number of axes on the device. */

	struct haptic_hwdata* hwdata; /* Driver dependent */
	int ref_count; /* Count for multiple opens */

	int rumble_id; /* ID of rumble effect for simple rumble API. */
	SDL_HapticEffect rumble_effect; /* Rumble effect. */
	struct _SDL_Haptic* next; /* pointer to next haptic we have allocated */
};

#endif



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
