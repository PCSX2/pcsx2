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
#include "resources_pad.h"
#include <signal.h> // sigaction

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
			GBytes* bytes = g_resource_lookup_data(PAD_res_get_resource(), "/PAD/res/game_controller_db.txt", G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);

			size_t size = 0;
			// SDL forget to add const for SDL_RWFromMem API...
			void* data = const_cast<void*>(g_bytes_get_data(bytes, &size));

			SDL_GameControllerAddMappingsFromRW(SDL_RWFromMem(data, size), 1);

			g_bytes_unref(bytes);

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
	if (type >= m_effects_id.size())
		return;

	if (!(g_conf.pad_options[pad].forcefeedback))
		return;

	if (m_haptic == nullptr)
		return;

	int id = m_effects_id[type];
	if (SDL_HapticRunEffect(m_haptic, id, 1) != 0)
	{
		fprintf(stderr, "ERROR: Effect is not working! %s, id is %d\n", SDL_GetError(), id);
	}
}

JoystickInfo::~JoystickInfo()
{
	// Haptic must be closed before the joystick
	if (m_haptic != nullptr)
	{
		for (const auto& eid : m_effects_id)
		{
			if (eid >= 0)
				SDL_HapticDestroyEffect(m_haptic, eid);
		}

		SDL_HapticClose(m_haptic);
	}

	if (m_controller != nullptr)
	{
#if SDL_MINOR_VERSION >= 4
		// Version before 2.0.4 are bugged, JoystickClose crashes randomly
		// Note: GameControllerClose calls JoystickClose)
		SDL_GameControllerClose(m_controller);
#endif
	}
}

JoystickInfo::JoystickInfo(int id)
	: Device()
	, m_controller(nullptr)
	, m_haptic(nullptr)
	, m_unique_id(0)
{
	SDL_Joystick* joy = nullptr;
	m_effects_id.fill(-1);
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
		fprintf(stderr, "PAD: failed to open joystick %d\n", id);
		return;
	}

	// Collect Device Information
	char guid[64];
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 64);
	m_device_name = SDL_JoystickNameForIndex(id);

	if (m_controller == nullptr)
	{
		fprintf(stderr, "PAD: Joystick (%s,GUID:%s) isn't yet supported by the SDL2 game controller API\n"
						"You can use SDL2 Gamepad Tool (https://www.generalarcade.com/gamepadtool/) or Steam to configure your joystick\n"
						"The mapping can be stored in PAD.ini as 'SDL2 = <...mapping description...>'\n"
						"Please post the new generated mapping to (https://github.com/gabomdq/SDL_GameControllerDB) so it can be added to the database.",
				m_device_name.c_str(), guid);

#if SDL_MINOR_VERSION >= 4 // Version before 2.0.4 are bugged, JoystickClose crashes randomly
		SDL_JoystickClose(joy);
#endif

		return;
	}

	std::hash<std::string> hash_me;
	m_unique_id = hash_me(std::string(guid));

	// Default haptic effect
	SDL_HapticEffect effects[NB_EFFECT];
	for (int i = 0; i < NB_EFFECT; i++)
	{
		SDL_HapticEffect effect;
		memset(&effect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
		SDL_HapticDirection direction;
		direction.type = SDL_HAPTIC_POLAR; // We'll be using polar direction encoding.
		direction.dir[0] = 18000;
		effect.periodic.direction = direction;
		effect.periodic.period = 10;
		effect.periodic.magnitude = (Sint16)(g_conf.get_ff_intensity()); // Effect at maximum instensity
		effect.periodic.offset = 0;
		effect.periodic.phase = 18000;
		effect.periodic.length = 125; // 125ms feels quite near to original
		effect.periodic.delay = 0;
		effect.periodic.attack_length = 0;
		/* Sine and triangle are quite probably the best, don't change that lightly and if you do
         * keep effects ordered by type
         */
		if (i == 0)
		{
			/* Effect for small motor */
			/* Sine seems to be the only effect making little motor from DS3/4 react
             * Intensity has pretty much no effect either(which is coherent with what is explain in hid_sony driver
             */
			effect.type = SDL_HAPTIC_SINE;
		}
		else
		{
			/** Effect for big motor **/
			effect.type = SDL_HAPTIC_TRIANGLE;
		}

		effects[i] = effect;
	}

	if (SDL_JoystickIsHaptic(joy))
	{
		m_haptic = SDL_HapticOpenFromJoystick(joy);

		for (auto& eid : m_effects_id)
		{
			eid = SDL_HapticNewEffect(m_haptic, &effects[0]);
			if (eid < 0)
			{
				fprintf(stderr, "ERROR: Effect is not uploaded! %s\n", SDL_GetError());
				m_haptic = nullptr;
				break;
			}
		}
	}

	fprintf(stdout, "PAD: controller (%s) detected%s, GUID:%s\n",
			m_device_name.c_str(), m_haptic ? " with rumble support" : "", guid);

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
	// This code just use standard rumble to check that SDL handles the pad correctly! --3kinox
	if (m_haptic == nullptr)
		return false; // Otherwise, core dump!

	SDL_HapticRumbleInit(m_haptic);

	// Make the haptic pad rumble 60% strength for half a second, shoudld be enough for user to see if it works or not
	if (SDL_HapticRumblePlay(m_haptic, strength, 400) != 0)
	{
		fprintf(stderr, "ERROR: Rumble is not working! %s\n", SDL_GetError());
		return false;
	}

	return true;
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
	SDL_GameControllerUpdate();
}
