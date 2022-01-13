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

#include "joystick.h"
#include "Host.h"
#include <signal.h> // sigaction

#if !SDL_VERSION_ATLEAST(2, 0, 9)
	#error PCSX2 requires SDL2 2.0.9 or higher
#endif

//////////////////////////
// Joystick definitions //
//////////////////////////

// opens handles to all possible joysticks
void JoystickInfo::EnumerateJoysticks(std::vector<std::unique_ptr<Device>>& vjoysticks)
{
	uint32_t flag = SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER;

	if ((SDL_WasInit(0) & flag) != flag)
	{
		// Tell SDL to catch event even if the windows isn't focussed
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		// Enable rumble on PS4 controllers (note: breaks DirectInput handling)
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
		// We want buttons to come in as positions, not labels, where possible
		// New as of SDL 2.0.12, so use string
		SDL_SetHint("SDL_GAMECONTROLLER_USE_BUTTON_LABELS", "0");
		// Super annoying to have a bright blue LED shining in your face all the time
		// New as of SDL 2.0.18, so use string
		SDL_SetHint("SDL_JOYSTICK_HIDAPI_SWITCH_HOME_LED", "0");

		for (const auto& hint : g_conf.sdl2_hints)
			SDL_SetHint(hint.first.c_str(), hint.second.c_str());

		if (SDL_Init(flag) < 0)
			return;

		// WTF! Give me back the control of my system
		struct sigaction action = {};
		action.sa_handler = SIG_DFL;
		sigaction(SIGINT, &action, nullptr);
		sigaction(SIGTERM, &action, nullptr);

		SDL_JoystickEventState(SDL_QUERY);
		SDL_GameControllerEventState(SDL_QUERY);
		SDL_EventState(SDL_CONTROLLERDEVICEADDED, SDL_ENABLE);
		SDL_EventState(SDL_CONTROLLERDEVICEREMOVED, SDL_ENABLE);

		{ // Support as much Joystick as possible
			auto file = Host::ReadResourceFile("game_controller_db.txt");
			if (file.has_value())
				SDL_GameControllerAddMappingsFromRW(SDL_RWFromMem(file->data(), file->size()), 1);
			else
				Console.Warning("Failed to load SDL Game Controller DB file!");

			// Add user mapping too
			for (auto const& map : g_conf.sdl2_mapping)
				SDL_GameControllerAddMapping(map.c_str());
		}
	}

	vjoysticks.clear();

	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		vjoysticks.push_back(std::unique_ptr<Device>(new JoystickInfo(i)));
		// Something goes wrong in the init, let's drop it
		if (!vjoysticks.back()->IsProperlyInitialized())
			vjoysticks.pop_back();
	}
}

void JoystickInfo::Rumble(unsigned type, unsigned pad)
{
	if (type >= std::size(m_rumble_end))
		return;

	if (!(g_conf.pad_options[pad].forcefeedback))
		return;

	m_rumble_enabled[type] = true;
	m_rumble_end[type] = SDL_GetTicks() + 125; // 125ms feels quite near to original
	UpdateRumble(true);
}

void JoystickInfo::UpdateRumble(bool needs_update)
{
	u16 rumble_amt[2] = {0, 0};
	u32 rumble_time = 0;

	u32 now = SDL_GetTicks();
	for (size_t i = 0; i < std::size(m_rumble_end); i++)
	{
		s32 remaining = static_cast<s32>(m_rumble_end[i] - now);
		if (m_rumble_enabled[i])
		{
			if (remaining > 0)
			{
				rumble_amt[i] = std::min<u32>(g_conf.get_ff_intensity() * UINT16_MAX, UINT16_MAX);
				rumble_time = std::max(rumble_time, static_cast<u32>(remaining));
			}
			else
			{
				m_rumble_enabled[i] = false;
				needs_update = true;
			}
		}
	}

	if (needs_update)
		SDL_GameControllerRumble(m_controller, rumble_amt[1], rumble_amt[0], rumble_time);
}

JoystickInfo::~JoystickInfo()
{
	if (m_controller != nullptr)
	{
		SDL_GameControllerClose(m_controller);
	}
}

JoystickInfo::JoystickInfo(int id)
	: Device()
	, m_controller(nullptr)
	, m_unique_id(0)
{
	SDL_Joystick* joy = nullptr;
	// Values are hardcoded currently but it could be later extended to allow remapping of the buttons
	m_pad_to_sdl[PAD_L2] = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
	m_pad_to_sdl[PAD_R2] = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
	m_pad_to_sdl[PAD_L1] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	m_pad_to_sdl[PAD_R1] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
	m_pad_to_sdl[PAD_TRIANGLE] = SDL_CONTROLLER_BUTTON_Y;
	m_pad_to_sdl[PAD_CIRCLE] = SDL_CONTROLLER_BUTTON_B;
	m_pad_to_sdl[PAD_CROSS] = SDL_CONTROLLER_BUTTON_A;
	m_pad_to_sdl[PAD_SQUARE] = SDL_CONTROLLER_BUTTON_X;
	m_pad_to_sdl[PAD_SELECT] = SDL_CONTROLLER_BUTTON_BACK;
	m_pad_to_sdl[PAD_L3] = SDL_CONTROLLER_BUTTON_LEFTSTICK;
	m_pad_to_sdl[PAD_R3] = SDL_CONTROLLER_BUTTON_RIGHTSTICK;
	m_pad_to_sdl[PAD_START] = SDL_CONTROLLER_BUTTON_START;
	m_pad_to_sdl[PAD_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP;
	m_pad_to_sdl[PAD_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
	m_pad_to_sdl[PAD_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
	m_pad_to_sdl[PAD_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
	m_pad_to_sdl[PAD_L_UP] = SDL_CONTROLLER_AXIS_LEFTY;
	m_pad_to_sdl[PAD_L_RIGHT] = SDL_CONTROLLER_AXIS_LEFTX;
	m_pad_to_sdl[PAD_L_DOWN] = SDL_CONTROLLER_AXIS_LEFTY;
	m_pad_to_sdl[PAD_L_LEFT] = SDL_CONTROLLER_AXIS_LEFTX;
	m_pad_to_sdl[PAD_R_UP] = SDL_CONTROLLER_AXIS_RIGHTY;
	m_pad_to_sdl[PAD_R_RIGHT] = SDL_CONTROLLER_AXIS_RIGHTX;
	m_pad_to_sdl[PAD_R_DOWN] = SDL_CONTROLLER_AXIS_RIGHTY;
	m_pad_to_sdl[PAD_R_LEFT] = SDL_CONTROLLER_AXIS_RIGHTX;

	if (SDL_IsGameController(id))
	{
		m_controller = SDL_GameControllerOpen(id);
		joy = SDL_GameControllerGetJoystick(m_controller);
	}
	else
	{
		joy = SDL_JoystickOpen(id);
	}

	if (joy == nullptr)
	{
		Console.Warning("PAD: failed to open joystick %d", id);
		return;
	}

	// Collect Device Information
	char guid[64];
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 64);
	m_device_name = SDL_JoystickNameForIndex(id);

	if (m_controller == nullptr)
	{
		Console.Warning("PAD: Joystick (%s,GUID:%s) isn't yet supported by the SDL2 game controller API\n"
		                "You can use SDL2 Gamepad Tool (https://www.generalarcade.com/gamepadtool/) or Steam to configure your joystick\n"
		                "The mapping can be stored in PAD.ini as 'SDL2 = <...mapping description...>'\n"
		                "Please post the new generated mapping to (https://github.com/gabomdq/SDL_GameControllerDB) so it can be added to the database.",
		        m_device_name.c_str(), guid);

		SDL_JoystickClose(joy);

		return;
	}

	std::hash<std::string> hash_me;
	m_unique_id = hash_me(std::string(guid));

	bool rumble_support = SDL_GameControllerRumble(m_controller, 0, 0, 1) >= 0;

	Console.WriteLn("PAD: controller (%s) detected%s, GUID:%s",
			m_device_name.c_str(), rumble_support ? " with rumble support" : "", guid);

	m_no_error = true;
}

const char* JoystickInfo::GetName()
{
	return SDL_JoystickName(SDL_GameControllerGetJoystick(m_controller));
}

size_t JoystickInfo::GetUniqueIdentifier()
{
	return m_unique_id;
}

bool JoystickInfo::TestForce(float strength = 0.60)
{
	u16 u16strength = static_cast<u16>(UINT16_MAX * strength);

	return SDL_GameControllerRumble(m_controller, u16strength, u16strength, 400) >= 0;
}

int JoystickInfo::GetInput(gamePadValues input)
{
	float k = g_conf.get_sensibility() / 100.0; // convert sensibility to float

	// Handle analog inputs which range from -32k to +32k. Range conversion is handled later in the controller
	if (IsAnalogKey(input))
	{
		int value = SDL_GameControllerGetAxis(m_controller, (SDL_GameControllerAxis)m_pad_to_sdl[input]);
		value *= k;
		return (abs(value) > m_deadzone) ? value : 0;
	}

	// Handle triggers which range from 0 to +32k. They must be converted to 0-255 range
	if (input == PAD_L2 || input == PAD_R2)
	{
		int value = SDL_GameControllerGetAxis(m_controller, (SDL_GameControllerAxis)m_pad_to_sdl[input]);
		return (value > m_deadzone) ? value / 128 : 0;
	}

	// Remain buttons
	int value = SDL_GameControllerGetButton(m_controller, (SDL_GameControllerButton)m_pad_to_sdl[input]);
	return value ? 0xFF : 0; // Max pressure
}

void JoystickInfo::UpdateDeviceState()
{
	UpdateRumble(false);
	SDL_GameControllerUpdate();
}
