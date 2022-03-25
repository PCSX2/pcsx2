/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "Frontend/SDLInputSource.h"
#include "Frontend/InputManager.h"
#include "Host.h"
#include "HostSettings.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"
#include "common/Console.h"
#include <cmath>

static const char* s_sdl_axis_names[] = {
	"LeftX", // SDL_CONTROLLER_AXIS_LEFTX
	"LeftY", // SDL_CONTROLLER_AXIS_LEFTY
	"RightX", // SDL_CONTROLLER_AXIS_RIGHTX
	"RightY", // SDL_CONTROLLER_AXIS_RIGHTY
	"LeftTrigger", // SDL_CONTROLLER_AXIS_TRIGGERLEFT
	"RightTrigger", // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};
static const GenericInputBinding s_sdl_generic_binding_axis_mapping[][2] = {
	{GenericInputBinding::LeftStickLeft, GenericInputBinding::LeftStickRight}, // SDL_CONTROLLER_AXIS_LEFTX
	{GenericInputBinding::LeftStickUp, GenericInputBinding::LeftStickDown}, // SDL_CONTROLLER_AXIS_LEFTY
	{GenericInputBinding::RightStickLeft, GenericInputBinding::RightStickRight}, // SDL_CONTROLLER_AXIS_RIGHTX
	{GenericInputBinding::RightStickUp, GenericInputBinding::RightStickDown}, // SDL_CONTROLLER_AXIS_RIGHTY
	{GenericInputBinding::Unknown, GenericInputBinding::L2}, // SDL_CONTROLLER_AXIS_TRIGGERLEFT
	{GenericInputBinding::Unknown, GenericInputBinding::R2}, // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};

static const char* s_sdl_button_names[] = {
	"A", // SDL_CONTROLLER_BUTTON_A
	"B", // SDL_CONTROLLER_BUTTON_B
	"X", // SDL_CONTROLLER_BUTTON_X
	"Y", // SDL_CONTROLLER_BUTTON_Y
	"Back", // SDL_CONTROLLER_BUTTON_BACK
	"Guide", // SDL_CONTROLLER_BUTTON_GUIDE
	"Start", // SDL_CONTROLLER_BUTTON_START
	"LeftStick", // SDL_CONTROLLER_BUTTON_LEFTSTICK
	"RightStick", // SDL_CONTROLLER_BUTTON_RIGHTSTICK
	"LeftShoulder", // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	"RightShoulder", // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	"DPadUp", // SDL_CONTROLLER_BUTTON_DPAD_UP
	"DPadDown", // SDL_CONTROLLER_BUTTON_DPAD_DOWN
	"DPadLeft", // SDL_CONTROLLER_BUTTON_DPAD_LEFT
	"DPadRight", // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
	"Misc1", // SDL_CONTROLLER_BUTTON_MISC1
	"Paddle1", // SDL_CONTROLLER_BUTTON_PADDLE1
	"Paddle2", // SDL_CONTROLLER_BUTTON_PADDLE2
	"Paddle3", // SDL_CONTROLLER_BUTTON_PADDLE3
	"Paddle4", // SDL_CONTROLLER_BUTTON_PADDLE4
	"Touchpad", // SDL_CONTROLLER_BUTTON_TOUCHPAD
};
static const GenericInputBinding s_sdl_generic_binding_button_mapping[] = {
	GenericInputBinding::Cross, // SDL_CONTROLLER_BUTTON_A
	GenericInputBinding::Circle, // SDL_CONTROLLER_BUTTON_B
	GenericInputBinding::Square, // SDL_CONTROLLER_BUTTON_X
	GenericInputBinding::Triangle, // SDL_CONTROLLER_BUTTON_Y
	GenericInputBinding::Select, // SDL_CONTROLLER_BUTTON_BACK
	GenericInputBinding::System, // SDL_CONTROLLER_BUTTON_GUIDE
	GenericInputBinding::Start, // SDL_CONTROLLER_BUTTON_START
	GenericInputBinding::L3, // SDL_CONTROLLER_BUTTON_LEFTSTICK
	GenericInputBinding::R3, // SDL_CONTROLLER_BUTTON_RIGHTSTICK
	GenericInputBinding::L1, // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	GenericInputBinding::R1, // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	GenericInputBinding::DPadUp, // SDL_CONTROLLER_BUTTON_DPAD_UP
	GenericInputBinding::DPadDown, // SDL_CONTROLLER_BUTTON_DPAD_DOWN
	GenericInputBinding::DPadLeft, // SDL_CONTROLLER_BUTTON_DPAD_LEFT
	GenericInputBinding::DPadRight, // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_MISC1
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_PADDLE1
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_PADDLE2
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_PADDLE3
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_PADDLE4
	GenericInputBinding::Unknown, // SDL_CONTROLLER_BUTTON_TOUCHPAD
};

SDLInputSource::SDLInputSource() = default;

SDLInputSource::~SDLInputSource() { pxAssert(m_controllers.empty()); }

bool SDLInputSource::Initialize(SettingsInterface& si)
{
	std::optional<std::vector<u8>> controller_db_data = Host::ReadResourceFile("game_controller_db.txt");
	if (controller_db_data.has_value())
	{
		SDL_RWops* ops = SDL_RWFromConstMem(controller_db_data->data(), static_cast<int>(controller_db_data->size()));
		if (SDL_GameControllerAddMappingsFromRW(ops, true) < 0)
			Console.Error("SDL_GameControllerAddMappingsFromRW() failed: %s", SDL_GetError());
	}
	else
	{
		Console.Error("Controller database resource is missing.");
	}

	LoadSettings(si);
	SetHints();
	return InitializeSubsystem();
}

void SDLInputSource::UpdateSettings(SettingsInterface& si)
{
	const bool old_controller_enhanced_mode = m_controller_enhanced_mode;

	LoadSettings(si);

	if (m_controller_enhanced_mode != old_controller_enhanced_mode)
	{
		ShutdownSubsystem();
		SetHints();
		InitializeSubsystem();
	}
}

void SDLInputSource::Shutdown()
{
	ShutdownSubsystem();
}

void SDLInputSource::LoadSettings(SettingsInterface& si)
{
	m_controller_enhanced_mode = si.GetBoolValue("InputSources", "SDLControllerEnhancedMode", false);
}

void SDLInputSource::SetHints()
{
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, m_controller_enhanced_mode ? "1" : "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, m_controller_enhanced_mode ? "1" : "0");
}

bool SDLInputSource::InitializeSubsystem()
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0)
	{
		Console.Error("SDL_InitSubSystem(SDL_INIT_JOYSTICK |SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) failed");
		return false;
	}

	// we should open the controllers as the connected events come in, so no need to do any more here
	m_sdl_subsystem_initialized = true;
	return true;
}

void SDLInputSource::ShutdownSubsystem()
{
	while (!m_controllers.empty())
		CloseGameController(m_controllers.begin()->joystick_id);

	if (m_sdl_subsystem_initialized)
	{
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC);
		m_sdl_subsystem_initialized = false;
	}
}

void SDLInputSource::PollEvents()
{
	for (;;)
	{
		SDL_Event ev;
		if (SDL_PollEvent(&ev))
			ProcessSDLEvent(&ev);
		else
			break;
	}
}

std::vector<std::pair<std::string, std::string>> SDLInputSource::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;

	for (const ControllerData& cd : m_controllers)
	{
		std::string id(StringUtil::StdStringFromFormat("SDL-%d", cd.player_id));

		const char* name = SDL_GameControllerName(cd.game_controller);
		if (name)
			ret.emplace_back(std::move(id), name);
		else
			ret.emplace_back(std::move(id), "Unknown Device");
	}

	return ret;
}

std::optional<InputBindingKey> SDLInputSource::ParseKeyString(
	const std::string_view& device, const std::string_view& binding)
{
	if (!StringUtil::StartsWith(device, "SDL-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::SDL;
	key.source_index = static_cast<u32>(player_id.value());

	if (StringUtil::EndsWith(binding, "Motor"))
	{
		key.source_subtype = InputSubclass::ControllerMotor;
		if (binding == "LargeMotor")
		{
			key.data = 0;
			return key;
		}
		else if (binding == "SmallMotor")
		{
			key.data = 1;
			return key;
		}
		else
		{
			return std::nullopt;
		}
	}
	else if (StringUtil::EndsWith(binding, "Haptic"))
	{
		key.source_subtype = InputSubclass::ControllerHaptic;
		key.data = 0;
		return key;
	}
	else if (binding[0] == '+' || binding[0] == '-')
	{
		// likely an axis
		const std::string_view axis_name(binding.substr(1));
		for (u32 i = 0; i < std::size(s_sdl_axis_names); i++)
		{
			if (axis_name == s_sdl_axis_names[i])
			{
				// found an axis!
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = i;
				key.negative = (binding[0] == '-');
				return key;
			}
		}
	}
	else
	{
		// must be a button
		for (u32 i = 0; i < std::size(s_sdl_button_names); i++)
		{
			if (binding == s_sdl_button_names[i])
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = i;
				return key;
			}
		}
	}

	// unknown axis/button
	return std::nullopt;
}

std::string SDLInputSource::ConvertKeyToString(InputBindingKey key)
{
	std::string ret;

	if (key.source_type == InputSourceType::SDL)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis && key.data < std::size(s_sdl_axis_names))
		{
			ret = StringUtil::StdStringFromFormat(
				"SDL-%u/%c%s", key.source_index, key.negative ? '-' : '+', s_sdl_axis_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerButton && key.data < std::size(s_sdl_button_names))
		{
			ret = StringUtil::StdStringFromFormat("SDL-%u/%s", key.source_index, s_sdl_button_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			ret = StringUtil::StdStringFromFormat("SDL-%u/%sMotor", key.source_index, key.data ? "Large" : "Small");
		}
		else if (key.source_subtype == InputSubclass::ControllerHaptic)
		{
			ret = StringUtil::StdStringFromFormat("SDL-%u/Haptic", key.source_index);
		}
	}

	return ret;
}

bool SDLInputSource::ProcessSDLEvent(const SDL_Event* event)
{
	switch (event->type)
	{
		case SDL_CONTROLLERDEVICEADDED:
		{
			Console.WriteLn("(SDLInputSource) Controller %d inserted", event->cdevice.which);
			OpenGameController(event->cdevice.which);
			return true;
		}

		case SDL_CONTROLLERDEVICEREMOVED:
		{
			Console.WriteLn("(SDLInputSource) Controller %d removed", event->cdevice.which);
			CloseGameController(event->cdevice.which);
			return true;
		}

		case SDL_CONTROLLERAXISMOTION:
			return HandleControllerAxisEvent(&event->caxis);

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			return HandleControllerButtonEvent(&event->cbutton);

		default:
			return false;
	}
}

SDLInputSource::ControllerDataVector::iterator SDLInputSource::GetControllerDataForJoystickId(int id)
{
	return std::find_if(
		m_controllers.begin(), m_controllers.end(), [id](const ControllerData& cd) { return cd.joystick_id == id; });
}

SDLInputSource::ControllerDataVector::iterator SDLInputSource::GetControllerDataForPlayerId(int id)
{
	return std::find_if(
		m_controllers.begin(), m_controllers.end(), [id](const ControllerData& cd) { return cd.player_id == id; });
}

int SDLInputSource::GetFreePlayerId() const
{
	for (int player_id = 0;; player_id++)
	{
		size_t i;
		for (i = 0; i < m_controllers.size(); i++)
		{
			if (m_controllers[i].player_id == player_id)
				break;
		}
		if (i == m_controllers.size())
			return player_id;
	}

	return 0;
}

bool SDLInputSource::OpenGameController(int index)
{
	SDL_GameController* gcontroller = SDL_GameControllerOpen(index);
	SDL_Joystick* joystick = gcontroller ? SDL_GameControllerGetJoystick(gcontroller) : nullptr;
	if (!gcontroller || !joystick)
	{
		Console.Error("(SDLInputSource) Failed to open controller %d", index);
		if (gcontroller)
			SDL_GameControllerClose(gcontroller);

		return false;
	}

	const int joystick_id = SDL_JoystickInstanceID(joystick);
	int player_id = SDL_GameControllerGetPlayerIndex(gcontroller);
	if (player_id < 0 || GetControllerDataForPlayerId(player_id) != m_controllers.end())
	{
		const int free_player_id = GetFreePlayerId();
		Console.Warning("(SDLInputSource) Controller %d (joystick %d) returned player ID %d, which is invalid or in "
						"use. Using ID %d instead.",
			index, joystick_id, player_id, free_player_id);
		player_id = free_player_id;
	}

	Console.WriteLn("(SDLInputSource) Opened controller %d (instance id %d, player id %d): %s", index, joystick_id,
		player_id, SDL_GameControllerName(gcontroller));

	ControllerData cd = {};
	cd.player_id = player_id;
	cd.joystick_id = joystick_id;
	cd.haptic_left_right_effect = -1;
	cd.game_controller = gcontroller;

	cd.use_game_controller_rumble = (SDL_GameControllerRumble(gcontroller, 0, 0, 0) == 0);
	if (cd.use_game_controller_rumble)
	{
		Console.WriteLn(
			"(SDLInputSource) Rumble is supported on '%s' via gamecontroller", SDL_GameControllerName(gcontroller));
	}
	else
	{
		SDL_Haptic* haptic = SDL_HapticOpenFromJoystick(joystick);
		if (haptic)
		{
			SDL_HapticEffect ef = {};
			ef.leftright.type = SDL_HAPTIC_LEFTRIGHT;
			ef.leftright.length = 1000;

			int ef_id = SDL_HapticNewEffect(haptic, &ef);
			if (ef_id >= 0)
			{
				cd.haptic = haptic;
				cd.haptic_left_right_effect = ef_id;
			}
			else
			{
				Console.Error("(SDLInputSource) Failed to create haptic left/right effect: %s", SDL_GetError());
				if (SDL_HapticRumbleSupported(haptic) && SDL_HapticRumbleInit(haptic) != 0)
				{
					cd.haptic = haptic;
				}
				else
				{
					Console.Error("(SDLInputSource) No haptic rumble supported: %s", SDL_GetError());
					SDL_HapticClose(haptic);
				}
			}
		}

		if (cd.haptic)
			Console.WriteLn(
				"(SDLInputSource) Rumble is supported on '%s' via haptic", SDL_GameControllerName(gcontroller));
	}

	if (!cd.haptic && !cd.use_game_controller_rumble)
		Console.Warning("(SDLInputSource) Rumble is not supported on '%s'", SDL_GameControllerName(gcontroller));

	m_controllers.push_back(std::move(cd));

	const char* name = SDL_GameControllerName(cd.game_controller);
	Host::OnInputDeviceConnected(StringUtil::StdStringFromFormat("SDL-%d", player_id), name ? name : "Unknown Device");
	return true;
}

bool SDLInputSource::CloseGameController(int joystick_index)
{
	auto it = GetControllerDataForJoystickId(joystick_index);
	if (it == m_controllers.end())
		return false;

	if (it->haptic)
		SDL_HapticClose(static_cast<SDL_Haptic*>(it->haptic));

	SDL_GameControllerClose(static_cast<SDL_GameController*>(it->game_controller));

	const int player_id = it->player_id;
	m_controllers.erase(it);

	Host::OnInputDeviceDisconnected(StringUtil::StdStringFromFormat("SDL-%d", player_id));
	return true;
}

bool SDLInputSource::HandleControllerAxisEvent(const SDL_ControllerAxisEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerAxisKey(InputSourceType::SDL, it->player_id, ev->axis));
	const float value = static_cast<float>(ev->value) / (ev->value < 0 ? 32768.0f : 32767.0f);
	return InputManager::InvokeEvents(key, value);
}

bool SDLInputSource::HandleControllerButtonEvent(const SDL_ControllerButtonEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerButtonKey(InputSourceType::SDL, it->player_id, ev->button));
	return InputManager::InvokeEvents(key, (ev->state == SDL_PRESSED) ? 1.0f : 0.0f);
}

std::vector<InputBindingKey> SDLInputSource::EnumerateMotors()
{
	std::vector<InputBindingKey> ret;

	InputBindingKey key = {};
	key.source_type = InputSourceType::SDL;

	for (ControllerData& cd : m_controllers)
	{
		key.source_index = cd.player_id;

		if (cd.use_game_controller_rumble || cd.haptic_left_right_effect)
		{
			// two motors
			key.source_subtype = InputSubclass::ControllerMotor;
			key.data = 0;
			ret.push_back(key);
			key.data = 1;
			ret.push_back(key);
		}
		else if (cd.haptic)
		{
			// haptic effect
			key.source_subtype = InputSubclass::ControllerHaptic;
			key.data = 0;
			ret.push_back(key);
		}
	}

	return ret;
}

bool SDLInputSource::GetGenericBindingMapping(const std::string_view& device, GenericInputBindingMapping* mapping)
{
	if (!StringUtil::StartsWith(device, "SDL-"))
		return false;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return false;

	ControllerDataVector::iterator it = GetControllerDataForPlayerId(player_id.value());
	if (it == m_controllers.end())
		return false;

	if (it->game_controller)
	{
		// assume all buttons are present.
		const s32 pid = player_id.value();
		for (u32 i = 0; i < std::size(s_sdl_generic_binding_axis_mapping); i++)
		{
			const GenericInputBinding negative = s_sdl_generic_binding_axis_mapping[i][0];
			const GenericInputBinding positive = s_sdl_generic_binding_axis_mapping[i][1];
			if (negative != GenericInputBinding::Unknown)
				mapping->emplace_back(negative, StringUtil::StdStringFromFormat("SDL-%d/-%s", pid, s_sdl_axis_names[i]));

			if (positive != GenericInputBinding::Unknown)
				mapping->emplace_back(positive, StringUtil::StdStringFromFormat("SDL-%d/+%s", pid, s_sdl_axis_names[i]));
		}
		for (u32 i = 0; i < std::size(s_sdl_generic_binding_button_mapping); i++)
		{
			const GenericInputBinding binding = s_sdl_generic_binding_button_mapping[i];
			if (binding != GenericInputBinding::Unknown)
				mapping->emplace_back(binding, StringUtil::StdStringFromFormat("SDL-%d/%s", pid, s_sdl_button_names[i]));
		}

		if (it->use_game_controller_rumble || it->haptic_left_right_effect)
		{
			mapping->emplace_back(GenericInputBinding::SmallMotor, StringUtil::StdStringFromFormat("SDL-%d/SmallMotor", pid));
			mapping->emplace_back(GenericInputBinding::LargeMotor, StringUtil::StdStringFromFormat("SDL-%d/LargeMotor", pid));
		}
		else
		{
			mapping->emplace_back(GenericInputBinding::SmallMotor, StringUtil::StdStringFromFormat("SDL-%d/Haptic", pid));
			mapping->emplace_back(GenericInputBinding::LargeMotor, StringUtil::StdStringFromFormat("SDL-%d/Haptic", pid));
		}

		return true;
	}
	else
	{
		// joysticks, which we haven't implemented yet anyway.
		return false;
	}
}

void SDLInputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
	if (key.source_subtype != InputSubclass::ControllerMotor && key.source_subtype != InputSubclass::ControllerHaptic)
		return;

	auto it = GetControllerDataForPlayerId(key.source_index);
	if (it == m_controllers.end())
		return;

	it->rumble_intensity[key.data] = static_cast<u16>(intensity * 65535.0f);
	SendRumbleUpdate(&(*it));
}

void SDLInputSource::UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
	if (large_key.source_index != small_key.source_index || large_key.source_subtype != InputSubclass::ControllerMotor ||
		small_key.source_subtype != InputSubclass::ControllerMotor)
	{
		// bonkers config where they're mapped to different controllers... who would do such a thing?
		UpdateMotorState(large_key, large_intensity);
		UpdateMotorState(small_key, small_intensity);
		return;
	}

	auto it = GetControllerDataForPlayerId(large_key.source_index);
	if (it == m_controllers.end())
		return;

	it->rumble_intensity[large_key.data] = static_cast<u16>(large_intensity * 65535.0f);
	it->rumble_intensity[small_key.data] = static_cast<u16>(small_intensity * 65535.0f);
	SendRumbleUpdate(&(*it));
}

void SDLInputSource::SendRumbleUpdate(ControllerData* cd)
{
	// we'll update before this duration is elapsed
	static constexpr u32 DURATION = 65535; // SDL_MAX_RUMBLE_DURATION_MS

	if (cd->use_game_controller_rumble)
	{
		SDL_GameControllerRumble(cd->game_controller, cd->rumble_intensity[0], cd->rumble_intensity[1], DURATION);
		return;
	}

	if (cd->haptic_left_right_effect >= 0)
	{
		if ((static_cast<u32>(cd->rumble_intensity[0]) + static_cast<u32>(cd->rumble_intensity[1])) > 0)
		{
			SDL_HapticEffect ef;
			ef.type = SDL_HAPTIC_LEFTRIGHT;
			ef.leftright.large_magnitude = cd->rumble_intensity[0];
			ef.leftright.small_magnitude = cd->rumble_intensity[1];
			ef.leftright.length = DURATION;
			SDL_HapticUpdateEffect(cd->haptic, cd->haptic_left_right_effect, &ef);
			SDL_HapticRunEffect(cd->haptic, cd->haptic_left_right_effect, SDL_HAPTIC_INFINITY);
		}
		else
		{
			SDL_HapticStopEffect(cd->haptic, cd->haptic_left_right_effect);
		}
	}
	else
	{
		const float strength = static_cast<float>(std::max(cd->rumble_intensity[0], cd->rumble_intensity[1])) * (1.0f / 65535.0f);
		if (strength > 0.0f)
			SDL_HapticRumblePlay(cd->haptic, strength, DURATION);
		else
			SDL_HapticRumbleStop(cd->haptic);
	}
}
