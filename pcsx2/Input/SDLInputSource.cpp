// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Config.h"
#include "Input/SDLInputSource.h"
#include "Input/InputManager.h"
#include "Host.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "IconsPromptFont.h"

#include <bit>
#include <cmath>

static constexpr const char* CONTROLLER_DB_FILENAME = "game_controller_db.txt";

static constexpr const char* s_sdl_axis_names[] = {
	"LeftX", // SDL_CONTROLLER_AXIS_LEFTX
	"LeftY", // SDL_CONTROLLER_AXIS_LEFTY
	"RightX", // SDL_CONTROLLER_AXIS_RIGHTX
	"RightY", // SDL_CONTROLLER_AXIS_RIGHTY
	"LeftTrigger", // SDL_CONTROLLER_AXIS_TRIGGERLEFT
	"RightTrigger", // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};
static constexpr const char* s_sdl_axis_icons[][2] = {
	{ICON_PF_LEFT_ANALOG_LEFT, ICON_PF_LEFT_ANALOG_RIGHT}, // SDL_CONTROLLER_AXIS_LEFTX
	{ICON_PF_LEFT_ANALOG_UP, ICON_PF_LEFT_ANALOG_DOWN}, // SDL_CONTROLLER_AXIS_LEFTY
	{ICON_PF_RIGHT_ANALOG_LEFT, ICON_PF_RIGHT_ANALOG_RIGHT}, // SDL_CONTROLLER_AXIS_RIGHTX
	{ICON_PF_RIGHT_ANALOG_UP, ICON_PF_RIGHT_ANALOG_DOWN}, // SDL_CONTROLLER_AXIS_RIGHTY
	{nullptr, ICON_PF_LEFT_TRIGGER_PULL}, // SDL_CONTROLLER_AXIS_TRIGGERLEFT
	{nullptr, ICON_PF_RIGHT_TRIGGER_PULL}, // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};
static constexpr const GenericInputBinding s_sdl_generic_binding_axis_mapping[][2] = {
	{GenericInputBinding::LeftStickLeft, GenericInputBinding::LeftStickRight}, // SDL_CONTROLLER_AXIS_LEFTX
	{GenericInputBinding::LeftStickUp, GenericInputBinding::LeftStickDown}, // SDL_CONTROLLER_AXIS_LEFTY
	{GenericInputBinding::RightStickLeft, GenericInputBinding::RightStickRight}, // SDL_CONTROLLER_AXIS_RIGHTX
	{GenericInputBinding::RightStickUp, GenericInputBinding::RightStickDown}, // SDL_CONTROLLER_AXIS_RIGHTY
	{GenericInputBinding::Unknown, GenericInputBinding::L2}, // SDL_CONTROLLER_AXIS_TRIGGERLEFT
	{GenericInputBinding::Unknown, GenericInputBinding::R2}, // SDL_CONTROLLER_AXIS_TRIGGERRIGHT
};

static constexpr const char* s_sdl_button_names[] = {
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
static constexpr const char* s_sdl_button_icons[] = {
	ICON_PF_BUTTON_A, // SDL_CONTROLLER_BUTTON_A
	ICON_PF_BUTTON_B, // SDL_CONTROLLER_BUTTON_B
	ICON_PF_BUTTON_X, // SDL_CONTROLLER_BUTTON_X
	ICON_PF_BUTTON_Y, // SDL_CONTROLLER_BUTTON_Y
	ICON_PF_SHARE_CAPTURE, // SDL_CONTROLLER_BUTTON_BACK
	ICON_PF_XBOX, // SDL_CONTROLLER_BUTTON_GUIDE
	ICON_PF_BURGER_MENU, // SDL_CONTROLLER_BUTTON_START
	ICON_PF_LEFT_ANALOG_CLICK, // SDL_CONTROLLER_BUTTON_LEFTSTICK
	ICON_PF_RIGHT_ANALOG_CLICK, // SDL_CONTROLLER_BUTTON_RIGHTSTICK
	ICON_PF_LEFT_SHOULDER_LB, // SDL_CONTROLLER_BUTTON_LEFTSHOULDER
	ICON_PF_RIGHT_SHOULDER_RB, // SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
	ICON_PF_XBOX_DPAD_UP, // SDL_CONTROLLER_BUTTON_DPAD_UP
	ICON_PF_XBOX_DPAD_DOWN, // SDL_CONTROLLER_BUTTON_DPAD_DOWN
	ICON_PF_XBOX_DPAD_LEFT, // SDL_CONTROLLER_BUTTON_DPAD_LEFT
	ICON_PF_XBOX_DPAD_RIGHT, // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};
static constexpr const GenericInputBinding s_sdl_generic_binding_button_mapping[] = {
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

static constexpr const char* s_sdl_hat_direction_names[] = {
	// clang-format off
	"North",
	"East",
	"South",
	"West",
	// clang-format on
};

static constexpr const char* s_sdl_default_led_colors[] = {
	"0000ff", // SDL-0
	"ff0000", // SDL-1
	"00ff00", // SDL-2
	"ffff00", // SDL-3
};

static void SetControllerRGBLED(SDL_GameController* gc, u32 color)
{
	SDL_GameControllerSetLED(gc, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
}

static void SDLLogCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
	if (priority >= SDL_LOG_PRIORITY_INFO)
		Console.WriteLn(fmt::format("SDL: {}", message));
	else
		DevCon.WriteLn(fmt::format("SDL: {}", message));
}

SDLInputSource::SDLInputSource() = default;

SDLInputSource::~SDLInputSource()
{
	pxAssert(m_controllers.empty());
}

bool SDLInputSource::Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	LoadSettings(si);
	settings_lock.unlock();
	SetHints();
	bool result = InitializeSubsystem();
	settings_lock.lock();
	return result;
}

void SDLInputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	const bool old_controller_enhanced_mode = m_controller_enhanced_mode;
	const bool old_controller_raw_mode = m_controller_raw_mode;

	LoadSettings(si);

	if (m_controller_enhanced_mode != old_controller_enhanced_mode || m_controller_raw_mode != old_controller_raw_mode)
	{
		settings_lock.unlock();
		ShutdownSubsystem();
		SetHints();
		InitializeSubsystem();
		settings_lock.lock();
	}
}

bool SDLInputSource::ReloadDevices()
{
	// We'll get a GC added/removed event here.
	PollEvents();
	return false;
}

void SDLInputSource::Shutdown()
{
	ShutdownSubsystem();
}

void SDLInputSource::LoadSettings(SettingsInterface& si)
{
	m_controller_enhanced_mode = si.GetBoolValue("InputSources", "SDLControllerEnhancedMode", false);
	m_controller_raw_mode = si.GetBoolValue("InputSources", "SDLRawInput", false);
	m_sdl_hints = si.GetKeyValueList("SDLHints");

	for (u32 i = 0; i < MAX_LED_COLORS; i++)
	{
		const u32 color = GetRGBForPlayerId(si, i);
		if (m_led_colors[i] == color)
			continue;

		m_led_colors[i] = color;

		const auto it = GetControllerDataForPlayerId(i);
		if (it == m_controllers.end() || !it->game_controller || !SDL_GameControllerHasLED(it->game_controller))
			continue;

		SetControllerRGBLED(it->game_controller, color);
	}
}

u32 SDLInputSource::GetRGBForPlayerId(SettingsInterface& si, u32 player_id)
{
	return ParseRGBForPlayerId(
		si.GetStringValue("SDLExtra", fmt::format("Player{}LED", player_id).c_str(), s_sdl_default_led_colors[player_id]),
		player_id);
}

u32 SDLInputSource::ParseRGBForPlayerId(const std::string_view& str, u32 player_id)
{
	if (player_id >= MAX_LED_COLORS)
		return 0;

	const u32 default_color = StringUtil::FromChars<u32>(s_sdl_default_led_colors[player_id], 16).value_or(0);
	const u32 color = StringUtil::FromChars<u32>(str, 16).value_or(default_color);

	return color;
}

void SDLInputSource::SetHints()
{
	if (const std::string upath = Path::Combine(EmuFolders::DataRoot, CONTROLLER_DB_FILENAME); FileSystem::FileExists(upath.c_str()))
	{
		Console.WriteLn(Color_StrongGreen, fmt::format("SDLInputSource: Using Controller DB from user directory: '{}'", upath));
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, upath.c_str());
	}
	else if (const std::string rpath = Path::Combine(EmuFolders::Resources, CONTROLLER_DB_FILENAME); FileSystem::FileExists(rpath.c_str()))
	{
		Console.WriteLn(Color_StrongGreen, "SDLInputSource: Using Controller DB from resources.");
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, rpath.c_str());
	}
	else
	{
		Console.Error(fmt::format("SDLInputSource: Controller DB not found, it should be named '{}'", CONTROLLER_DB_FILENAME));
	}

	SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, m_controller_raw_mode ? "1" : "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, m_controller_enhanced_mode ? "1" : "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, m_controller_enhanced_mode ? "1" : "0");
	// Enable Wii U Pro Controller support
	// New as of SDL 2.26, so use string
	SDL_SetHint("SDL_JOYSTICK_HIDAPI_WII", "1");
#ifndef _WIN32
	// Gets us pressure sensitive button support on Linux
	// Apparently doesn't work on Windows, so leave it off there
	// New as of SDL 2.26, so use string
	SDL_SetHint("SDL_JOYSTICK_HIDAPI_PS3", "1");
#endif

	for (const std::pair<std::string, std::string>& hint : m_sdl_hints)
		SDL_SetHint(hint.first.c_str(), hint.second.c_str());
}

bool SDLInputSource::InitializeSubsystem()
{
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) < 0)
	{
		Console.Error("SDL_InitSubSystem(SDL_INIT_JOYSTICK |SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) failed");
		return false;
	}

	SDL_LogSetOutputFunction(SDLLogCallback, nullptr);
#ifdef PCSX2_DEVBUILD
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
#else
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
#endif

	// we should open the controllers as the connected events come in, so no need to do any more here
	m_sdl_subsystem_initialized = true;
	Console.WriteLn(Color_StrongGreen, fmt::format("SDLInputSource: {} controller mappings are loaded.", SDL_GameControllerNumMappings()));
	return true;
}

void SDLInputSource::ShutdownSubsystem()
{
	while (!m_controllers.empty())
		CloseDevice(m_controllers.begin()->joystick_id);

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

		const char* name = cd.game_controller ? SDL_GameControllerName(cd.game_controller) : SDL_JoystickName(cd.joystick);
		if (name)
			ret.emplace_back(std::move(id), name);
		else
			ret.emplace_back(std::move(id), "Unknown Device");
	}

	return ret;
}

std::optional<InputBindingKey> SDLInputSource::ParseKeyString(const std::string_view& device, const std::string_view& binding)
{
	if (!device.starts_with("SDL-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::SDL;
	key.source_index = static_cast<u32>(player_id.value());

	if (binding.ends_with("Motor"))
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
	else if (binding.ends_with("Haptic"))
	{
		key.source_subtype = InputSubclass::ControllerHaptic;
		key.data = 0;
		return key;
	}
	else if (binding[0] == '+' || binding[0] == '-')
	{
		// likely an axis
		const std::string_view axis_name(binding.substr(1));

		if (axis_name.starts_with("Axis"))
		{
			std::string_view end;
			if (auto value = StringUtil::FromChars<u32>(axis_name.substr(4), 10, &end))
			{
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = *value;
				key.modifier = (binding[0] == '-') ? InputModifier::Negate : InputModifier::None;
				key.invert = (end == "~");
				return key;
			}
		}
		for (u32 i = 0; i < std::size(s_sdl_axis_names); i++)
		{
			if (axis_name == s_sdl_axis_names[i])
			{
				// found an axis!
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = i;
				key.modifier = (binding[0] == '-') ? InputModifier::Negate : InputModifier::None;
				return key;
			}
		}
	}
	else if (binding.starts_with("FullAxis"))
	{
		std::string_view end;
		if (auto value = StringUtil::FromChars<u32>(binding.substr(8), 10, &end))
		{
			key.source_subtype = InputSubclass::ControllerAxis;
			key.data = *value;
			key.modifier = InputModifier::FullAxis;
			key.invert = (end == "~");
			return key;
		}
	}
	else if (binding.starts_with("Hat"))
	{
		std::string_view hat_dir;
		if (auto value = StringUtil::FromChars<u32>(binding.substr(3), 10, &hat_dir); value.has_value() && !hat_dir.empty())
		{
			for (u8 dir = 0; dir < static_cast<u8>(std::size(s_sdl_hat_direction_names)); dir++)
			{
				if (hat_dir == s_sdl_hat_direction_names[dir])
				{
					key.source_subtype = InputSubclass::ControllerHat;
					key.data = value.value() * std::size(s_sdl_hat_direction_names) + dir;
					return key;
				}
			}
		}
	}
	else
	{
		// must be a button
		if (binding.starts_with("Button"))
		{
			if (auto value = StringUtil::FromChars<u32>(binding.substr(6)))
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = *value;
				return key;
			}
		}
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

TinyString SDLInputSource::ConvertKeyToString(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::SDL)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			const char* modifier = (key.modifier == InputModifier::FullAxis ? "Full" : (key.modifier == InputModifier::Negate ? "-" : "+"));
			if (key.data < std::size(s_sdl_axis_names))
				ret.fmt("SDL-{}/{}{}", static_cast<u32>(key.source_index), modifier, s_sdl_axis_names[key.data]);
			else
				ret.fmt("SDL-{}/{}Axis{}{}", static_cast<u32>(key.source_index), modifier, key.data, key.invert ? "~" : "");
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (key.data < std::size(s_sdl_button_names))
				ret.fmt("SDL-{}/{}", static_cast<u32>(key.source_index), s_sdl_button_names[key.data]);
			else
				ret.fmt("SDL-{}/Button{}", static_cast<u32>(key.source_index), key.data);
		}
		else if (key.source_subtype == InputSubclass::ControllerHat)
		{
			const u32 hat_index = key.data / static_cast<u32>(std::size(s_sdl_hat_direction_names));
			const u32 hat_direction = key.data % static_cast<u32>(std::size(s_sdl_hat_direction_names));
			ret.fmt("SDL-{}/Hat{}{}", static_cast<u32>(key.source_index), hat_index, s_sdl_hat_direction_names[hat_direction]);
		}
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			ret.fmt("SDL-{}/{}Motor", static_cast<u32>(key.source_index), key.data ? "Large" : "Small");
		}
		else if (key.source_subtype == InputSubclass::ControllerHaptic)
		{
			ret.fmt("SDL-{}/Haptic", static_cast<u32>(key.source_index));
		}
	}

	return ret;
}

TinyString SDLInputSource::ConvertKeyToIcon(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::SDL)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			if (key.data < std::size(s_sdl_axis_icons) && key.modifier != InputModifier::FullAxis)
			{
				ret.fmt("SDL-{}  {}", static_cast<u32>(key.source_index),
					s_sdl_axis_icons[key.data][key.modifier == InputModifier::None]);
			}
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (key.data < std::size(s_sdl_button_icons))
				ret.fmt("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_button_icons[key.data]);
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
			OpenDevice(event->cdevice.which, true);
			return true;
		}

		case SDL_CONTROLLERDEVICEREMOVED:
		{
			Console.WriteLn("(SDLInputSource) Controller %d removed", event->cdevice.which);
			CloseDevice(event->cdevice.which);
			return true;
		}

		case SDL_JOYDEVICEADDED:
		{
			// Let game controller handle.. well.. game controllers.
			if (SDL_IsGameController(event->jdevice.which))
				return false;

			Console.WriteLn("(SDLInputSource) Joystick %d inserted", event->jdevice.which);
			OpenDevice(event->cdevice.which, false);
			return true;
		}
		break;

		case SDL_JOYDEVICEREMOVED:
		{
			if (auto it = GetControllerDataForJoystickId(event->cdevice.which); it != m_controllers.end() && it->game_controller)
				return false;

			Console.WriteLn("(SDLInputSource) Joystick %d removed", event->jdevice.which);
			CloseDevice(event->cdevice.which);
			return true;
		}

		case SDL_CONTROLLERAXISMOTION:
			return HandleControllerAxisEvent(&event->caxis);

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			return HandleControllerButtonEvent(&event->cbutton);

		case SDL_JOYAXISMOTION:
			return HandleJoystickAxisEvent(&event->jaxis);

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			return HandleJoystickButtonEvent(&event->jbutton);

		case SDL_JOYHATMOTION:
			return HandleJoystickHatEvent(&event->jhat);

		default:
			return false;
	}
}

SDL_Joystick* SDLInputSource::GetJoystickForDevice(const std::string_view& device)
{
	if (!device.starts_with("SDL-"))
		return nullptr;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return nullptr;

	auto it = GetControllerDataForPlayerId(player_id.value());
	if (it == m_controllers.end())
		return nullptr;

	return it->joystick;
}

SDLInputSource::ControllerDataVector::iterator SDLInputSource::GetControllerDataForJoystickId(int id)
{
	return std::find_if(m_controllers.begin(), m_controllers.end(), [id](const ControllerData& cd) { return cd.joystick_id == id; });
}

SDLInputSource::ControllerDataVector::iterator SDLInputSource::GetControllerDataForPlayerId(int id)
{
	return std::find_if(m_controllers.begin(), m_controllers.end(), [id](const ControllerData& cd) { return cd.player_id == id; });
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

bool SDLInputSource::OpenDevice(int index, bool is_gamecontroller)
{
	SDL_GameController* gcontroller;
	SDL_Joystick* joystick;

	if (is_gamecontroller)
	{
		gcontroller = SDL_GameControllerOpen(index);
		joystick = gcontroller ? SDL_GameControllerGetJoystick(gcontroller) : nullptr;
	}
	else
	{
		gcontroller = nullptr;
		joystick = SDL_JoystickOpen(index);
	}

	if (!gcontroller && !joystick)
	{
		Console.Error("(SDLInputSource) Failed to open controller %d", index);

		return false;
	}

	const int joystick_id = SDL_JoystickInstanceID(joystick);
	int player_id = gcontroller ? SDL_GameControllerGetPlayerIndex(gcontroller) : SDL_JoystickGetPlayerIndex(joystick);
	for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it)
	{
		if (it->joystick_id == joystick_id)
		{
			Console.Error("(SDLInputSource) Controller %d, instance %d, player %d already connected, ignoring.", index, joystick_id, player_id);
			if (gcontroller)
				SDL_GameControllerClose(gcontroller);
			else
				SDL_JoystickClose(joystick);

			return false;
		}
	}

	if (player_id < 0 || GetControllerDataForPlayerId(player_id) != m_controllers.end())
	{
		const int free_player_id = GetFreePlayerId();
		Console.Warning("(SDLInputSource) Controller %d (joystick %d) returned player ID %d, which is invalid or in "
						"use. Using ID %d instead.",
			index, joystick_id, player_id, free_player_id);
		player_id = free_player_id;
	}

	const char* name = gcontroller ? SDL_GameControllerName(gcontroller) : SDL_JoystickName(joystick);
	if (!name)
		name = "Unknown Device";

	Console.WriteLn("(SDLInputSource) Opened %s %d (instance id %d, player id %d): %s", is_gamecontroller ? "game controller" : "joystick",
		index, joystick_id, player_id, name);

	ControllerData cd = {};
	cd.player_id = player_id;
	cd.joystick_id = joystick_id;
	cd.haptic_left_right_effect = -1;
	cd.game_controller = gcontroller;
	cd.joystick = joystick;

	if (gcontroller)
	{
		const int num_axes = SDL_JoystickNumAxes(joystick);
		const int num_buttons = SDL_JoystickNumButtons(joystick);
		cd.joy_axis_used_in_gc.resize(num_axes, false);
		cd.joy_button_used_in_gc.resize(num_buttons, false);
		auto mark_bind = [&](SDL_GameControllerButtonBind bind) {
			if (bind.bindType == SDL_CONTROLLER_BINDTYPE_AXIS && bind.value.axis < num_axes)
				cd.joy_axis_used_in_gc[bind.value.axis] = true;
			if (bind.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bind.value.button < num_buttons)
				cd.joy_button_used_in_gc[bind.value.button] = true;
		};
		for (size_t i = 0; i < std::size(s_sdl_axis_names); i++)
			mark_bind(SDL_GameControllerGetBindForAxis(gcontroller, static_cast<SDL_GameControllerAxis>(i)));
		for (size_t i = 0; i < std::size(s_sdl_button_names); i++)
			mark_bind(SDL_GameControllerGetBindForButton(gcontroller, static_cast<SDL_GameControllerButton>(i)));
	}
	else
	{
		// GC doesn't have the concept of hats, so we only need to do this for joysticks.
		const int num_hats = SDL_JoystickNumHats(joystick);
		if (num_hats > 0)
			cd.last_hat_state.resize(static_cast<size_t>(num_hats), u8(0));
	}

	cd.use_game_controller_rumble = (gcontroller && SDL_GameControllerRumble(gcontroller, 0, 0, 0) == 0);
	if (cd.use_game_controller_rumble)
	{
		Console.WriteLn("(SDLInputSource) Rumble is supported on '%s' via gamecontroller", name);
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
			Console.WriteLn("(SDLInputSource) Rumble is supported on '%s' via haptic", name);
	}

	if (!cd.haptic && !cd.use_game_controller_rumble)
		Console.Warning("(SDLInputSource) Rumble is not supported on '%s'", name);

	if (player_id >= 0 && static_cast<u32>(player_id) < MAX_LED_COLORS && gcontroller && SDL_GameControllerHasLED(gcontroller))
	{
		SetControllerRGBLED(gcontroller, m_led_colors[player_id]);
	}

	m_controllers.push_back(std::move(cd));

	InputManager::OnInputDeviceConnected(StringUtil::StdStringFromFormat("SDL-%d", player_id), name);
	return true;
}

bool SDLInputSource::CloseDevice(int joystick_index)
{
	auto it = GetControllerDataForJoystickId(joystick_index);
	if (it == m_controllers.end())
		return false;

	InputManager::OnInputDeviceDisconnected(StringUtil::StdStringFromFormat("SDL-%d", it->player_id));

	if (it->haptic)
		SDL_HapticClose(it->haptic);

	if (it->game_controller)
		SDL_GameControllerClose(it->game_controller);
	else
		SDL_JoystickClose(it->joystick);

	m_controllers.erase(it);
	return true;
}

static float NormalizeS16(s16 value)
{
	return static_cast<float>(value) / (value < 0 ? 32768.0f : 32767.0f);
}

bool SDLInputSource::HandleControllerAxisEvent(const SDL_ControllerAxisEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerAxisKey(InputSourceType::SDL, it->player_id, ev->axis));
	InputManager::InvokeEvents(key, NormalizeS16(ev->value));
	return true;
}

bool SDLInputSource::HandleControllerButtonEvent(const SDL_ControllerButtonEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerButtonKey(InputSourceType::SDL, it->player_id, ev->button));
	const GenericInputBinding generic_key = (ev->button < std::size(s_sdl_generic_binding_button_mapping)) ?
												s_sdl_generic_binding_button_mapping[ev->button] :
												GenericInputBinding::Unknown;
	InputManager::InvokeEvents(key, (ev->state == SDL_PRESSED) ? 1.0f : 0.0f, generic_key);
	return true;
}

bool SDLInputSource::HandleJoystickAxisEvent(const SDL_JoyAxisEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;
	if (ev->axis < it->joy_axis_used_in_gc.size() && it->joy_axis_used_in_gc[ev->axis])
		return false; // Will get handled by GC event
	const u32 axis = ev->axis + std::size(s_sdl_axis_names); // Ensure we don't conflict with GC axes
	const InputBindingKey key(MakeGenericControllerAxisKey(InputSourceType::SDL, it->player_id, axis));
	InputManager::InvokeEvents(key, NormalizeS16(ev->value));
	return true;
}

bool SDLInputSource::HandleJoystickButtonEvent(const SDL_JoyButtonEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;
	if (ev->button < it->joy_button_used_in_gc.size() && it->joy_button_used_in_gc[ev->button])
		return false; // Will get handled by GC event
	const u32 button = ev->button + std::size(s_sdl_button_names); // Ensure we don't conflict with GC buttons
	const InputBindingKey key(MakeGenericControllerButtonKey(InputSourceType::SDL, it->player_id, button));
	InputManager::InvokeEvents(key, (ev->state == SDL_PRESSED) ? 1.0f : 0.0f);
	return true;
}

bool SDLInputSource::HandleJoystickHatEvent(const SDL_JoyHatEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end() || ev->hat >= it->last_hat_state.size())
		return false;

	const u8 last_direction = it->last_hat_state[ev->hat];
	it->last_hat_state[ev->hat] = ev->value;

	u8 changed_direction = last_direction ^ ev->value;
	while (changed_direction != 0)
	{
		const u8 pos = std::countr_zero(changed_direction);
		const u8 mask = (1u << pos);
		changed_direction &= ~mask;

		const InputBindingKey key(
			MakeGenericControllerHatKey(InputSourceType::SDL, it->player_id, ev->hat, pos, std::size(s_sdl_hat_direction_names)));
		InputManager::InvokeEvents(key, (last_direction & mask) ? 0.0f : 1.0f);
	}

	return true;
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

bool SDLInputSource::GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping)
{
	if (!device.starts_with("SDL-"))
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
		// joysticks have arbitrary axis numbers, so automapping isn't going to work here.
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
