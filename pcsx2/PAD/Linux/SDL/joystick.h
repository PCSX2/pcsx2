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

#pragma once

#include <SDL.h>
#include <SDL_haptic.h>

#include "../Global.h"
#include "../Device.h"

#define NB_EFFECT 2 // Don't use more than two, ps2 only has one for big motor and one for small(like most systems)

// holds all joystick info
class JoystickInfo : public Device
{
public:
	JoystickInfo(int id);
	~JoystickInfo();

	JoystickInfo(const JoystickInfo&) = delete;            // copy constructor
	JoystickInfo& operator=(const JoystickInfo&) = delete; // assignment


	// opens handles to all possible joysticks
	static void EnumerateJoysticks(std::vector<std::unique_ptr<Device>>& vjoysticks);

	void Rumble(unsigned type, unsigned pad) override;
	void UpdateRumble(bool needs_update);

	bool TestForce(float) override;

	const char* GetName() final;

	int GetInput(gamePadValues input) final;

	void UpdateDeviceState() final;

	size_t GetUniqueIdentifier() final;

private:
	SDL_GameController* m_controller;
	u32 m_rumble_end[NB_EFFECT] = {};
	bool m_rumble_enabled[NB_EFFECT] = {};
	size_t m_unique_id;
	std::array<int, MAX_KEYS> m_pad_to_sdl;
};
