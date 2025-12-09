// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Config.h"
#include "Input/SDLInputSource.h"
#include "Input/InputManager.h"
#include "Host.h"

#include "ImGui/FullscreenUI.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "IconsPromptFont.h"

#include <bit>
#include <cmath>
#include <VMManager.h>

static constexpr const char* CONTROLLER_DB_FILENAME = "game_controller_db.txt";

static constexpr const char* s_sdl_axis_setting_names[] = {
	"LeftX", // SDL_GAMEPAD_AXIS_LEFTX
	"LeftY", // SDL_GAMEPAD_AXIS_LEFTY
	"RightX", // SDL_GAMEPAD_AXIS_RIGHTX
	"RightY", // SDL_GAMEPAD_AXIS_RIGHTY
	"LeftTrigger", // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	"RightTrigger", // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static_assert(std::size(s_sdl_axis_setting_names) == SDL_GAMEPAD_AXIS_COUNT);

static constexpr const char* s_sdl_axis_names[] = {
	"Left X", // SDL_GAMEPAD_AXIS_LEFTX
	"Left Y", // SDL_GAMEPAD_AXIS_LEFTY
	"Right X", // SDL_GAMEPAD_AXIS_RIGHTX
	"Right Y", // SDL_GAMEPAD_AXIS_RIGHTY
};

static constexpr const char* s_sdl_trigger_names[] = {
	"Left Trigger", // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	"Right Trigger", // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static constexpr const char* s_sdl_trigger_ps_names[] = {
	"L2", // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	"R2", // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};

static const char* const* s_sdl_trigger_names_list[] = {
	s_sdl_trigger_names, // SDL_GAMEPAD_TYPE_UNKNOWN
	s_sdl_trigger_names, // SDL_GAMEPAD_TYPE_STANDARD
	s_sdl_trigger_names, // SDL_GAMEPAD_TYPE_XBOX360
	s_sdl_trigger_names, // SDL_GAMEPAD_TYPE_XBOXONE
	s_sdl_trigger_ps_names, // SDL_GAMEPAD_TYPE_PS3
	s_sdl_trigger_ps_names, // SDL_GAMEPAD_TYPE_PS4
	s_sdl_trigger_ps_names, // SDL_GAMEPAD_TYPE_PS5
	// Switch
};

static constexpr const char* s_sdl_ps3_sxs_pressure_names[] = {
	nullptr, // JoyAxis0
	nullptr, // JoyAxis1
	nullptr, // JoyAxis2
	nullptr, // JoyAxis3
	nullptr, // JoyAxis4
	nullptr, // JoyAxis5
	"Cross (Pressure)", // JoyAxis6
	"Circle (Pressure)", // JoyAxis7
	"Square (Pressure)", // JoyAxis8
	"Triangle (Pressure)", // JoyAxis9
	"L1 (Pressure)", // JoyAxis10
	"R1 (Pressure)", // JoyAxis11
	"D-Pad Up (Pressure)", // JoyAxis12
	"D-Pad Down (Pressure)", // JoyAxis13
	"D-Pad Left (Pressure)", // JoyAxis14
	"D-Pad Right (Pressure)", // JoyAxis15
};

static constexpr const char* s_sdl_axis_icons[][2] = {
	{ICON_PF_LEFT_ANALOG_LEFT, ICON_PF_LEFT_ANALOG_RIGHT}, // SDL_GAMEPAD_AXIS_LEFTX
	{ICON_PF_LEFT_ANALOG_UP, ICON_PF_LEFT_ANALOG_DOWN}, // SDL_GAMEPAD_AXIS_LEFTY
	{ICON_PF_RIGHT_ANALOG_LEFT, ICON_PF_RIGHT_ANALOG_RIGHT}, // SDL_GAMEPAD_AXIS_RIGHTX
	{ICON_PF_RIGHT_ANALOG_UP, ICON_PF_RIGHT_ANALOG_DOWN}, // SDL_GAMEPAD_AXIS_RIGHTY
};

static constexpr const char* s_sdl_trigger_icons[] = {
	ICON_PF_LEFT_TRIGGER_PULL, // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	ICON_PF_RIGHT_TRIGGER_PULL, // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static constexpr const char* s_sdl_trigger_ps_icons[] = {
	ICON_PF_LEFT_TRIGGER_L2, // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	ICON_PF_RIGHT_TRIGGER_R2, // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};

static const char* const* s_sdl_trigger_icons_list[] = {
	s_sdl_trigger_icons, // SDL_GAMEPAD_TYPE_UNKNOWN
	s_sdl_trigger_icons, // SDL_GAMEPAD_TYPE_STANDARD
	s_sdl_trigger_icons, // SDL_GAMEPAD_TYPE_XBOX360
	s_sdl_trigger_icons, // SDL_GAMEPAD_TYPE_XBOXONE
	s_sdl_trigger_ps_icons, // SDL_GAMEPAD_TYPE_PS3
	s_sdl_trigger_ps_icons, // SDL_GAMEPAD_TYPE_PS4
	s_sdl_trigger_ps_icons, // SDL_GAMEPAD_TYPE_PS5
	// Switch
};

static constexpr const char* s_sdl_ps3_pressure_icons[] = {
	nullptr, // JoyAxis0
	nullptr, // JoyAxis1
	nullptr, // JoyAxis2
	nullptr, // JoyAxis3
	nullptr, // JoyAxis4
	nullptr, // JoyAxis5
	"P" ICON_PF_BUTTON_CROSS, // JoyAxis6
	"P" ICON_PF_BUTTON_CIRCLE, // JoyAxis7
	"P" ICON_PF_BUTTON_SQUARE, // JoyAxis8
	"P" ICON_PF_BUTTON_TRIANGLE, // JoyAxis9
	"P" ICON_PF_LEFT_SHOULDER_L1, // JoyAxis10
	"P" ICON_PF_RIGHT_SHOULDER_R1, // JoyAxis11
	"P" ICON_PF_DPAD_UP, // JoyAxis12
	"P" ICON_PF_DPAD_DOWN, // JoyAxis13
	"P" ICON_PF_DPAD_LEFT, // JoyAxis14
	"P" ICON_PF_DPAD_RIGHT, // JoyAxis15
};

static constexpr const GenericInputBinding s_sdl_generic_binding_axis_mapping[][2] = {
	{GenericInputBinding::LeftStickLeft, GenericInputBinding::LeftStickRight}, // SDL_GAMEPAD_AXIS_LEFTX
	{GenericInputBinding::LeftStickUp, GenericInputBinding::LeftStickDown}, // SDL_GAMEPAD_AXIS_LEFTY
	{GenericInputBinding::RightStickLeft, GenericInputBinding::RightStickRight}, // SDL_GAMEPAD_AXIS_RIGHTX
	{GenericInputBinding::RightStickUp, GenericInputBinding::RightStickDown}, // SDL_GAMEPAD_AXIS_RIGHTY
	{GenericInputBinding::Unknown, GenericInputBinding::L2}, // SDL_GAMEPAD_AXIS_LEFT_TRIGGER
	{GenericInputBinding::Unknown, GenericInputBinding::R2}, // SDL_GAMEPAD_AXIS_RIGHT_TRIGGER
};
static constexpr const GenericInputBinding s_sdl_ps3_binding_pressure_mapping[] = {
	GenericInputBinding::Unknown, // JoyAxis0
	GenericInputBinding::Unknown, // JoyAxis1
	GenericInputBinding::Unknown, // JoyAxis2
	GenericInputBinding::Unknown, // JoyAxis3
	GenericInputBinding::Unknown, // JoyAxis4
	GenericInputBinding::Unknown, // JoyAxis5
	GenericInputBinding::Cross, // JoyAxis6
	GenericInputBinding::Circle, // JoyAxis7
	GenericInputBinding::Square, // JoyAxis8
	GenericInputBinding::Triangle, // JoyAxis9
	GenericInputBinding::L1, // JoyAxis10
	GenericInputBinding::R1, // JoyAxis11
	GenericInputBinding::DPadUp, // JoyAxis12
	GenericInputBinding::DPadDown, // JoyAxis13
	GenericInputBinding::DPadLeft, // JoyAxis14
	GenericInputBinding::DPadRight, // JoyAxis15
};

static constexpr const char* s_sdl_button_setting_names[] = {
	"FaceSouth", // SDL_GAMEPAD_BUTTON_SOUTH
	"FaceEast", // SDL_GAMEPAD_BUTTON_EAST
	"FaceWest", // SDL_GAMEPAD_BUTTON_WEST
	"FaceNorth", // SDL_GAMEPAD_BUTTON_NORTH
	"Back", // SDL_GAMEPAD_BUTTON_BACK
	"Guide", // SDL_GAMEPAD_BUTTON_GUIDE
	"Start", // SDL_GAMEPAD_BUTTON_START
	"LeftStick", // SDL_GAMEPAD_BUTTON_LEFT_STICK
	"RightStick", // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	"LeftShoulder", // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	"RightShoulder", // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	"DPadUp", // SDL_GAMEPAD_BUTTON_DPAD_UP
	"DPadDown", // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	"DPadLeft", // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	"DPadRight", // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	"Misc1", // SDL_GAMEPAD_BUTTON_MISC1
	"Paddle1", // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
	"Paddle2", // SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
	"Paddle3", // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
	"Paddle4", // SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
	"Touchpad", // SDL_GAMEPAD_BUTTON_TOUCHPAD
	"Misc2", // SDL_GAMEPAD_BUTTON_MISC2
	"Misc3", // SDL_GAMEPAD_BUTTON_MISC3
	"Misc4", // SDL_GAMEPAD_BUTTON_MISC4
	"Misc5", // SDL_GAMEPAD_BUTTON_MISC5
	"Misc6", // SDL_GAMEPAD_BUTTON_MISC6
};
static_assert(std::size(s_sdl_button_setting_names) == SDL_GAMEPAD_BUTTON_COUNT);

static constexpr const char* s_sdl_face_button_names[] = {
	nullptr, // SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN
	"A", // SDL_GAMEPAD_BUTTON_LABEL_A
	"B", // SDL_GAMEPAD_BUTTON_LABEL_B
	"X", // SDL_GAMEPAD_BUTTON_LABEL_X
	"Y", // SDL_GAMEPAD_BUTTON_LABEL_Y
	"Cross", // SDL_GAMEPAD_BUTTON_LABEL_CROSS
	"Circle", // SDL_GAMEPAD_BUTTON_LABEL_CIRCLE
	"Square", // SDL_GAMEPAD_BUTTON_LABEL_SQUARE
	"Triangle", // SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE
};
static constexpr const char* s_sdl_button_names[] = {
	"Face South", // SDL_GAMEPAD_BUTTON_SOUTH
	"Face East", // SDL_GAMEPAD_BUTTON_EAST
	"Face West", // SDL_GAMEPAD_BUTTON_WEST
	"Face North", // SDL_GAMEPAD_BUTTON_NORTH
	"Back", // SDL_GAMEPAD_BUTTON_BACK
	"Guide", // SDL_GAMEPAD_BUTTON_GUIDE
	"Start", // SDL_GAMEPAD_BUTTON_START
	"Left Stick", // SDL_GAMEPAD_BUTTON_LEFT_STICK
	"Right Stick", // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	"Left Shoulder", // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	"Right Shoulder", // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	"D-Pad Up", // SDL_GAMEPAD_BUTTON_DPAD_UP
	"D-Pad Down", // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	"D-Pad Left", // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	"D-Pad Right", // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	"Misc 1", // SDL_GAMEPAD_BUTTON_MISC1
	"Paddle 1", // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
	"Paddle 2", // SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
	"Paddle 3", // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
	"Paddle 4", // SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
	"Touchpad", // SDL_GAMEPAD_BUTTON_TOUCHPAD
	"Misc 2", // SDL_GAMEPAD_BUTTON_MISC2
	"Misc 3", // SDL_GAMEPAD_BUTTON_MISC3
	"Misc 4", // SDL_GAMEPAD_BUTTON_MISC4
	"Misc 5", // SDL_GAMEPAD_BUTTON_MISC5
	"Misc 6", // SDL_GAMEPAD_BUTTON_MISC6
};
static constexpr const char* s_sdl_button_ps3_names[] = {
	"Cross", // SDL_GAMEPAD_BUTTON_SOUTH
	"Circle", // SDL_GAMEPAD_BUTTON_EAST
	"Square", // SDL_GAMEPAD_BUTTON_WEST
	"Triangle", // SDL_GAMEPAD_BUTTON_NORTH
	"Select", // SDL_GAMEPAD_BUTTON_BACK
	"PS", // SDL_GAMEPAD_BUTTON_GUIDE
	"Start", // SDL_GAMEPAD_BUTTON_START
	"Left Stick", // SDL_GAMEPAD_BUTTON_LEFT_STICK
	"Right Stick", // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	"L1", // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	"R1", // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
};
static constexpr const char* s_sdl_button_ps4_names[] = {
	"Cross", // SDL_GAMEPAD_BUTTON_SOUTH
	"Circle", // SDL_GAMEPAD_BUTTON_EAST
	"Square", // SDL_GAMEPAD_BUTTON_WEST
	"Triangle", // SDL_GAMEPAD_BUTTON_NORTH
	"Share", // SDL_GAMEPAD_BUTTON_BACK
	"PS", // SDL_GAMEPAD_BUTTON_GUIDE
	"Options", // SDL_GAMEPAD_BUTTON_START
	"Left Stick", // SDL_GAMEPAD_BUTTON_LEFT_STICK
	"Right Stick", // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	"L1", // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	"R1", // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
};
static constexpr const char* s_sdl_button_ps5_names[] = {
	"Cross", // SDL_GAMEPAD_BUTTON_SOUTH
	"Circle", // SDL_GAMEPAD_BUTTON_EAST
	"Square", // SDL_GAMEPAD_BUTTON_WEST
	"Triangle", // SDL_GAMEPAD_BUTTON_NORTH
	"Create", // SDL_GAMEPAD_BUTTON_BACK
	"PS", // SDL_GAMEPAD_BUTTON_GUIDE
	"Options", // SDL_GAMEPAD_BUTTON_START
	"Left Stick", // SDL_GAMEPAD_BUTTON_LEFT_STICK
	"Right Stick", // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	"L1", // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	"R1", // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	nullptr, // SDL_GAMEPAD_BUTTON_DPAD_UP
	nullptr, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	nullptr, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	nullptr, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	"Mute", // SDL_GAMEPAD_BUTTON_MISC1
};

static constexpr const char* const* s_sdl_button_names_list[] = {
	s_sdl_button_names, // SDL_GAMEPAD_TYPE_UNKNOWN
	s_sdl_button_names, // SDL_GAMEPAD_TYPE_STANDARD
	s_sdl_button_names, // SDL_GAMEPAD_TYPE_XBOX360
	s_sdl_button_names, // SDL_GAMEPAD_TYPE_XBOXONE
	s_sdl_button_ps3_names, // SDL_GAMEPAD_TYPE_PS3
	s_sdl_button_ps4_names, // SDL_GAMEPAD_TYPE_PS4
	s_sdl_button_ps5_names, // SDL_GAMEPAD_TYPE_PS5
	// Switch
};
static constexpr size_t s_sdl_button_namesize_list[] = {
	std::size(s_sdl_button_names), // SDL_GAMEPAD_TYPE_UNKNOWN
	std::size(s_sdl_button_names), // SDL_GAMEPAD_TYPE_STANDARD
	std::size(s_sdl_button_names), // SDL_GAMEPAD_TYPE_XBOX360
	std::size(s_sdl_button_names), // SDL_GAMEPAD_TYPE_XBOXONE
	std::size(s_sdl_button_ps3_names), // SDL_GAMEPAD_TYPE_PS3
	std::size(s_sdl_button_ps4_names), // SDL_GAMEPAD_TYPE_PS4
	std::size(s_sdl_button_ps5_names), // SDL_GAMEPAD_TYPE_PS5
	// Switch
};

static constexpr const char* s_sdl_face_button_icons[] = {
	nullptr, // SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN
	ICON_PF_BUTTON_A, // SDL_GAMEPAD_BUTTON_LABEL_A
	ICON_PF_BUTTON_B, // SDL_GAMEPAD_BUTTON_LABEL_B
	ICON_PF_BUTTON_X, // SDL_GAMEPAD_BUTTON_LABEL_X
	ICON_PF_BUTTON_Y, // SDL_GAMEPAD_BUTTON_LABEL_Y
	ICON_PF_BUTTON_CROSS, // SDL_GAMEPAD_BUTTON_LABEL_CROSS
	ICON_PF_BUTTON_CIRCLE, // SDL_GAMEPAD_BUTTON_LABEL_CIRCLE
	ICON_PF_BUTTON_SQUARE, // SDL_GAMEPAD_BUTTON_LABEL_SQUARE
	ICON_PF_BUTTON_TRIANGLE, // SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE
};
static constexpr const char* s_sdl_button_icons[] = {
	ICON_PF_BUTTON_DOWN_A, // SDL_GAMEPAD_BUTTON_SOUTH
	ICON_PF_BUTTON_RIGHT_B, // SDL_GAMEPAD_BUTTON_EAST
	ICON_PF_BUTTON_LEFT_X, // SDL_GAMEPAD_BUTTON_WEST
	ICON_PF_BUTTON_UP_Y, // SDL_GAMEPAD_BUTTON_NORTH
	ICON_PF_SHARE_CAPTURE, // SDL_GAMEPAD_BUTTON_BACK
	ICON_PF_XBOX, // SDL_GAMEPAD_BUTTON_GUIDE
	ICON_PF_BURGER_MENU, // SDL_GAMEPAD_BUTTON_START
	ICON_PF_LEFT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	ICON_PF_RIGHT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	ICON_PF_LEFT_SHOULDER_LB, // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	ICON_PF_RIGHT_SHOULDER_RB, // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	ICON_PF_XBOX_DPAD_UP, // SDL_GAMEPAD_BUTTON_DPAD_UP
	ICON_PF_XBOX_DPAD_DOWN, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	ICON_PF_XBOX_DPAD_LEFT, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	ICON_PF_XBOX_DPAD_RIGHT, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
};
static constexpr const char* s_sdl_button_ps3_icons[] = {
	ICON_PF_BUTTON_CROSS, // SDL_GAMEPAD_BUTTON_SOUTH
	ICON_PF_BUTTON_CIRCLE, // SDL_GAMEPAD_BUTTON_EAST
	ICON_PF_BUTTON_SQUARE, // SDL_GAMEPAD_BUTTON_WEST
	ICON_PF_BUTTON_TRIANGLE, // SDL_GAMEPAD_BUTTON_NORTH
	ICON_PF_SELECT_SHARE, // SDL_GAMEPAD_BUTTON_BACK
	ICON_PF_PLAYSTATION, // SDL_GAMEPAD_BUTTON_GUIDE
	ICON_PF_START, // SDL_GAMEPAD_BUTTON_START
	ICON_PF_LEFT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	ICON_PF_RIGHT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	ICON_PF_LEFT_SHOULDER_L1, // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	ICON_PF_RIGHT_SHOULDER_R1, // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	ICON_PF_DPAD_UP, // SDL_GAMEPAD_BUTTON_DPAD_UP
	ICON_PF_DPAD_DOWN, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	ICON_PF_DPAD_LEFT, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	ICON_PF_DPAD_RIGHT, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
};
static constexpr const char* s_sdl_button_ps4_icons[] = {
	ICON_PF_BUTTON_CROSS, // SDL_GAMEPAD_BUTTON_SOUTH
	ICON_PF_BUTTON_CIRCLE, // SDL_GAMEPAD_BUTTON_EAST
	ICON_PF_BUTTON_SQUARE, // SDL_GAMEPAD_BUTTON_WEST
	ICON_PF_BUTTON_TRIANGLE, // SDL_GAMEPAD_BUTTON_NORTH
	ICON_PF_DUALSHOCK_SHARE, // SDL_GAMEPAD_BUTTON_BACK
	ICON_PF_PLAYSTATION, // SDL_GAMEPAD_BUTTON_GUIDE
	ICON_PF_DUALSHOCK_OPTIONS, // SDL_GAMEPAD_BUTTON_START
	ICON_PF_LEFT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	ICON_PF_RIGHT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	ICON_PF_LEFT_SHOULDER_L1, // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	ICON_PF_RIGHT_SHOULDER_R1, // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	ICON_PF_DPAD_UP, // SDL_GAMEPAD_BUTTON_DPAD_UP
	ICON_PF_DPAD_DOWN, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	ICON_PF_DPAD_LEFT, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	ICON_PF_DPAD_RIGHT, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	nullptr, // SDL_GAMEPAD_BUTTON_MISC1
	nullptr, // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
	nullptr, // SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
	nullptr, // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
	nullptr, // SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
	ICON_PF_DUALSHOCK_TOUCHPAD, // SDL_GAMEPAD_BUTTON_TOUCHPAD
};
static constexpr const char* s_sdl_button_ps5_icons[] = {
	ICON_PF_BUTTON_CROSS, // SDL_GAMEPAD_BUTTON_SOUTH
	ICON_PF_BUTTON_CIRCLE, // SDL_GAMEPAD_BUTTON_EAST
	ICON_PF_BUTTON_SQUARE, // SDL_GAMEPAD_BUTTON_WEST
	ICON_PF_BUTTON_TRIANGLE, // SDL_GAMEPAD_BUTTON_NORTH
	ICON_PF_DUALSENSE_SHARE, // SDL_GAMEPAD_BUTTON_BACK
	ICON_PF_PLAYSTATION, // SDL_GAMEPAD_BUTTON_GUIDE
	ICON_PF_DUALSENSE_OPTIONS, // SDL_GAMEPAD_BUTTON_START
	ICON_PF_LEFT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	ICON_PF_RIGHT_ANALOG_CLICK, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	ICON_PF_LEFT_SHOULDER_L1, // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	ICON_PF_RIGHT_SHOULDER_R1, // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	ICON_PF_DPAD_UP, // SDL_GAMEPAD_BUTTON_DPAD_UP
	ICON_PF_DPAD_DOWN, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	ICON_PF_DPAD_LEFT, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	ICON_PF_DPAD_RIGHT, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
	nullptr, // SDL_GAMEPAD_BUTTON_MISC1
	nullptr, // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1
	nullptr, // SDL_GAMEPAD_BUTTON_LEFT_PADDLE1
	nullptr, // SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2
	nullptr, // SDL_GAMEPAD_BUTTON_LEFT_PADDLE2
	ICON_PF_DUALSENSE_TOUCHPAD, // SDL_GAMEPAD_BUTTON_TOUCHPAD
};

static constexpr const char* const* s_sdl_button_icons_list[] = {
	s_sdl_button_icons, // SDL_GAMEPAD_TYPE_UNKNOWN
	s_sdl_button_icons, // SDL_GAMEPAD_TYPE_STANDARD
	s_sdl_button_icons, // SDL_GAMEPAD_TYPE_XBOX360
	s_sdl_button_icons, // SDL_GAMEPAD_TYPE_XBOXONE
	s_sdl_button_ps3_icons, // SDL_GAMEPAD_TYPE_PS3
	s_sdl_button_ps4_icons, // SDL_GAMEPAD_TYPE_PS4
	s_sdl_button_ps5_icons, // SDL_GAMEPAD_TYPE_PS5
	// Switch
};
static constexpr size_t s_sdl_button_iconsize_list[] = {
	std::size(s_sdl_button_icons), // SDL_GAMEPAD_TYPE_UNKNOWN
	std::size(s_sdl_button_icons), // SDL_GAMEPAD_TYPE_STANDARD
	std::size(s_sdl_button_icons), // SDL_GAMEPAD_TYPE_XBOX360
	std::size(s_sdl_button_icons), // SDL_GAMEPAD_TYPE_XBOXONE
	std::size(s_sdl_button_ps3_icons), // SDL_GAMEPAD_TYPE_PS3
	std::size(s_sdl_button_ps4_icons), // SDL_GAMEPAD_TYPE_PS4
	std::size(s_sdl_button_ps5_icons), // SDL_GAMEPAD_TYPE_PS5
	// Switch
};

static constexpr const GenericInputBinding s_sdl_generic_binding_button_mapping[] = {
	GenericInputBinding::Cross, // SDL_GAMEPAD_BUTTON_SOUTH
	GenericInputBinding::Circle, // SDL_GAMEPAD_BUTTON_EAST
	GenericInputBinding::Square, // SDL_GAMEPAD_BUTTON_WEST
	GenericInputBinding::Triangle, // SDL_GAMEPAD_BUTTON_NORTH
	GenericInputBinding::Select, // SDL_GAMEPAD_BUTTON_BACK
	GenericInputBinding::System, // SDL_GAMEPAD_BUTTON_GUIDE
	GenericInputBinding::Start, // SDL_GAMEPAD_BUTTON_START
	GenericInputBinding::L3, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	GenericInputBinding::R3, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
	GenericInputBinding::L1, // SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
	GenericInputBinding::R1, // SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
	GenericInputBinding::DPadUp, // SDL_GAMEPAD_BUTTON_DPAD_UP
	GenericInputBinding::DPadDown, // SDL_GAMEPAD_BUTTON_DPAD_DOWN
	GenericInputBinding::DPadLeft, // SDL_GAMEPAD_BUTTON_DPAD_LEFT
	GenericInputBinding::DPadRight, // SDL_GAMEPAD_BUTTON_DPAD_RIGHT
};
static constexpr const GenericInputBinding s_sdl_ps3_binding_button_mapping[] = {
	GenericInputBinding::Unknown, // SDL_GAMEPAD_BUTTON_SOUTH
	GenericInputBinding::Unknown, // SDL_GAMEPAD_BUTTON_EAST
	GenericInputBinding::Unknown, // SDL_GAMEPAD_BUTTON_WEST
	GenericInputBinding::Unknown, // SDL_GAMEPAD_BUTTON_NORTH
	GenericInputBinding::Select, // SDL_GAMEPAD_BUTTON_BACK
	GenericInputBinding::System, // SDL_GAMEPAD_BUTTON_GUIDE
	GenericInputBinding::Start, // SDL_GAMEPAD_BUTTON_START
	GenericInputBinding::L3, // SDL_GAMEPAD_BUTTON_LEFT_STICK
	GenericInputBinding::R3, // SDL_GAMEPAD_BUTTON_RIGHT_STICK
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
	"000080", // SDL-0
	"800000", // SDL-1
	"008000", // SDL-2
	"808000", // SDL-3
};

static void SetGamepadRGBLED(SDL_Gamepad* pad, u32 color)
{
	SDL_SetGamepadLED(pad, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
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

bool SDLInputSource::IsInitialized()
{
	return m_sdl_subsystem_initialized;
}

void SDLInputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	const bool old_enable_enhanced_reports = m_enable_enhanced_reports;
	const bool old_enable_ps5_player_leds = m_enable_ps5_player_leds;
	const bool old_use_raw_input = m_use_raw_input;

#ifdef __APPLE__
	const bool old_enable_iokit_driver = m_enable_iokit_driver;
	const bool old_enable_mfi_driver = m_enable_mfi_driver;
#endif

	LoadSettings(si);

#ifdef __APPLE__
	const bool drivers_changed =
		(m_enable_iokit_driver != old_enable_iokit_driver || m_enable_mfi_driver != old_enable_mfi_driver);
#else
	constexpr bool drivers_changed = false;
#endif

	if (m_enable_enhanced_reports != old_enable_enhanced_reports ||
		m_enable_ps5_player_leds != old_enable_ps5_player_leds ||
		m_use_raw_input != old_use_raw_input ||
		drivers_changed)
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
	// We'll get a device added/removed event here.
	PollEvents();
	return false;
}

void SDLInputSource::Shutdown()
{
	ShutdownSubsystem();
}

void SDLInputSource::LoadSettings(SettingsInterface& si)
{
	for (u32 i = 0; i < MAX_LED_COLORS; i++)
	{
		const u32 color = GetRGBForPlayerId(si, i);
		if (m_led_colors[i] == color)
			continue;

		m_led_colors[i] = color;

		const auto it = GetControllerDataForPlayerId(i);
		if (it == m_controllers.end() || !it->gamepad)
			continue;

		const SDL_PropertiesID props = SDL_GetGamepadProperties(it->gamepad);
		if (props == 0)
		{
			ERROR_LOG("SDLInputSource: SDL_GetGamepadProperties() failed");
			continue;
		}

		if (!SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false))
			continue;

		SetGamepadRGBLED(it->gamepad, color);
	}

	m_sdl_hints = si.GetKeyValueList("SDLHints");

	m_enable_enhanced_reports = si.GetBoolValue("InputSources", "SDLControllerEnhancedMode", true);
	m_enable_ps5_player_leds = si.GetBoolValue("InputSources", "SDLPS5PlayerLED", true);
	m_use_raw_input = si.GetBoolValue("InputSources", "SDLRawInput", false);

#ifdef __APPLE__
	m_enable_iokit_driver = si.GetBoolValue("InputSources", "SDLIOKitDriver", true);
	m_enable_mfi_driver = si.GetBoolValue("InputSources", "SDLMFIDriver", true);
#endif
}

u32 SDLInputSource::GetRGBForPlayerId(SettingsInterface& si, u32 player_id)
{
	return ParseRGBForPlayerId(
		si.GetStringValue("SDLExtra", fmt::format("Player{}LED", player_id).c_str(), s_sdl_default_led_colors[player_id]),
		player_id);
}

u32 SDLInputSource::ParseRGBForPlayerId(const std::string_view str, u32 player_id)
{
	if (player_id >= MAX_LED_COLORS)
		return 0;

	const u32 default_color = StringUtil::FromChars<u32>(s_sdl_default_led_colors[player_id], 16).value_or(0);
	const u32 color = StringUtil::FromChars<u32>(str, 16).value_or(default_color);

	return color;
}

void SDLInputSource::ResetRGBForAllPlayers(SettingsInterface& si)
{
	for (u32 player_id = 0; player_id < MAX_LED_COLORS; player_id++)
	{
		si.DeleteValue("SDLExtra", fmt::format("Player{}LED", player_id).c_str());
	}
}

void SDLInputSource::SetHints()
{
	if (const std::string upath = Path::Combine(EmuFolders::DataRoot, CONTROLLER_DB_FILENAME); FileSystem::FileExists(upath.c_str()))
	{
		Console.WriteLn(Color_StrongGreen, fmt::format("SDLInputSource: Using Controller DB from user directory: '{}'", upath));
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, upath.c_str());
	}
	else if (const std::string rpath = EmuFolders::GetOverridableResourcePath(CONTROLLER_DB_FILENAME); FileSystem::FileExists(rpath.c_str()))
	{
		Console.WriteLn(Color_StrongGreen, "SDLInputSource: Using Controller DB from resources.");
		SDL_SetHint(SDL_HINT_GAMECONTROLLERCONFIG_FILE, rpath.c_str());
	}
	else
	{
		Console.Error(fmt::format("SDLInputSource: Controller DB not found, it should be named '{}'", CONTROLLER_DB_FILENAME));
	}

	SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, m_use_raw_input ? "1" : "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, m_enable_enhanced_reports ? "auto" : "0"); // PS4/PS5 Rumble
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, m_enable_ps5_player_leds ? "1" : "0");
	// Enable Wii U Pro Controller support
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_WII, "1");
#ifndef _WIN32
	// Gets us pressure sensitive button support on Linux
	// Apparently doesn't work on Windows, so leave it off there
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, "1");
#else
	// Use the Sixaxis driver (or DsHidMini in SXS mode).
	// We don't support DsHidMini's SDF mode as none of the
	// PS3 hints allow accessing all the pressure sense axis.
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3_SIXAXIS_DRIVER, "1");
#endif

#ifdef __APPLE__
	Console.WriteLnFmt("IOKit is {}, MFI is {}.", m_enable_iokit_driver ? "enabled" : "disabled", m_enable_mfi_driver ? "enabled" : "disabled");
	SDL_SetHint(SDL_HINT_JOYSTICK_IOKIT, m_enable_iokit_driver ? "1" : "0");
	SDL_SetHint(SDL_HINT_JOYSTICK_MFI, m_enable_mfi_driver ? "1" : "0");
#endif

	for (const std::pair<std::string, std::string>& hint : m_sdl_hints)
		SDL_SetHint(hint.first.c_str(), hint.second.c_str());
}

bool SDLInputSource::InitializeSubsystem()
{
	if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC))
	{
		Console.Error("SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC) failed");
		return false;
	}

	SDL_SetLogOutputFunction(SDLLogCallback, nullptr);
#ifdef PCSX2_DEVBUILD
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#else
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif

	// we should open the controllers as the connected events come in, so no need to do any more here
	m_sdl_subsystem_initialized = true;

	int count;
	char** mappings = SDL_GetGamepadMappings(&count);
	if (mappings != nullptr)
	{
		SDL_free(mappings);
		Console.WriteLnFmt(Color_StrongGreen, "SDLInputSource: {} gamepad mappings are loaded.", count);
	}
	else
		Console.Error("SDL_GetGamepadMappings() failed {}", SDL_GetError());

	return true;
}

void SDLInputSource::ShutdownSubsystem()
{
	while (!m_controllers.empty())
		CloseDevice(m_controllers.begin()->joystick_id);

	if (m_sdl_subsystem_initialized)
	{
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC);
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

		const char* name = cd.gamepad ? SDL_GetGamepadName(cd.gamepad) : SDL_GetJoystickName(cd.joystick);
		if (name)
			ret.emplace_back(std::move(id), name);
		else
			ret.emplace_back(std::move(id), "Unknown Device");
	}

	return ret;
}

std::optional<InputBindingKey> SDLInputSource::ParseKeyString(const std::string_view device, const std::string_view binding)
{
	if (!device.starts_with("SDL-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::SDL;
	key.source_index = static_cast<u32>(player_id.value());

	// SDL2-SDL3 migrations
	static constexpr const char* sdl_button_legacy_names[] = {
		"A", // SDL_CONTROLLER_BUTTON_A
		"B", // SDL_CONTROLLER_BUTTON_B
		"X", // SDL_CONTROLLER_BUTTON_X
		"Y", // SDL_CONTROLLER_BUTTON_Y
	};

	for (u32 i = 0; i < std::size(sdl_button_legacy_names); i++)
	{
		if (binding == sdl_button_legacy_names[i])
		{
			key.source_subtype = InputSubclass::ControllerButton;
			key.data = i;

			// SDL2 would map A/B/X/Y based on the button's label
			// We need to convert this to a positional binding for SDL3
			static constexpr SDL_GamepadButton face_button_pos[] = {
				SDL_GAMEPAD_BUTTON_SOUTH,
				SDL_GAMEPAD_BUTTON_EAST,
				SDL_GAMEPAD_BUTTON_WEST,
				SDL_GAMEPAD_BUTTON_NORTH,
			};

			// This migrations needs to inspect the controller
			{
				std::lock_guard lock(m_controllers_key_mutex);

				auto it = GetControllerDataForPlayerId(key.source_index);
				if (it != m_controllers.end() && it->gamepad)
				{
					static bool shown_prompt = false;
					for (u32 pos = 0; pos < std::size(face_button_pos); pos++)
					{
						// A/B/X/Y are equal to 1/2/3/4 in SDL_GamepadButtonLabel
						// PS controllers have positional A/B/X/Y, so don't need adjusting
						// Controllers with unknown labels are assumed to have positional A/B/X/Y
						const SDL_GamepadButtonLabel label = SDL_GetGamepadButtonLabel(it->gamepad, face_button_pos[pos]);
						if (key.data == (label - 1))
						{
							if (key.data != pos)
							{
								if (!shown_prompt)
								{
									shown_prompt = true;
									Host::ReportInfoAsync(TRANSLATE("SDLInputSource", "SDL3 Migration"),
										TRANSLATE("SDLInputSource", "As part of our upgrade to SDL3, we've had to migrate your binds.\n"
																	"Your controller did not match the Xbox layout and may need rebinding.\n"
																	"Please verify your controller settings and amend if required."));

									// Also apply BPM setting for legacy binds
									// We assume this is a Nintendo controller, BPM will check if it is
									// Defer this, as we are probably under a setting lock
									Host::RunOnCPUThread([] {
										if (!Host::ContainsBaseSettingValue("UI", "SDL2NintendoLayout"))
										{
											Host::SetBaseStringSettingValue("UI", "SDL2NintendoLayout", "auto");
											Host::CommitBaseSettingChanges();
											// Get FSUI to recheck setting
											if (FullscreenUI::IsInitialized())
												FullscreenUI::GamepadLayoutChanged();
										}
									});
								}
								key.data = pos;
							}
							break;
						}
					}

					key.needs_migration = true;
					return key;
				}
				else if (std::find(m_gamepads_needing_migration.begin(), m_gamepads_needing_migration.end(), key.source_index) ==
						 m_gamepads_needing_migration.end())
				{
					// flag the device to migrate later
					m_gamepads_needing_migration.push_back(key.source_index);
					return std::nullopt;
				}
			}
		}
	}

	if (binding.starts_with("+Axis") || binding.starts_with("-Axis"))
	{
		const std::string_view axis_name(binding.substr(1));

		std::string_view end;
		if (auto value = StringUtil::FromChars<u32>(axis_name.substr(4), 10, &end))
		{
			key.source_subtype = InputSubclass::ControllerAxis;
			key.data = *value - 6 + std::size(s_sdl_axis_setting_names);
			key.modifier = (binding[0] == '-') ? InputModifier::Negate : InputModifier::None;
			key.invert = (end == "~");

			key.needs_migration = true;
			return key;
		}
	}
	else if (binding.starts_with("FullAxis"))
	{
		std::string_view end;
		if (auto value = StringUtil::FromChars<u32>(binding.substr(8), 10, &end))
		{
			key.source_subtype = InputSubclass::ControllerAxis;
			key.data = *value - 6 + std::size(s_sdl_axis_setting_names);
			key.modifier = InputModifier::FullAxis;
			key.invert = (end == "~");

			key.needs_migration = true;
			return key;
		}
	}
	else if (binding.starts_with("Button"))
	{
		if (auto value = StringUtil::FromChars<u32>(binding.substr(6)))
		{
			key.source_subtype = InputSubclass::ControllerButton;
			key.data = *value - 21 + std::size(s_sdl_button_setting_names);

			key.needs_migration = true;
			return key;
		}
	}
	// End Migrations

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
	else if (binding[0] == '+' || binding[0] == '-' || binding.starts_with("Full"))
	{
		// likely an axis
		const std::string_view axis_name(binding.substr(binding[0] == 'F' ? 4 : 1));

		if (axis_name.starts_with("JoyAxis"))
		{
			std::string_view end;
			if (auto value = StringUtil::FromChars<u32>(axis_name.substr(7), 10, &end))
			{
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = *value + std::size(s_sdl_axis_setting_names);
				key.modifier = (binding[0] == 'F') ? InputModifier::FullAxis :
				               (binding[0] == '-') ? InputModifier::Negate :
				                                     InputModifier::None;
				key.invert = (end == "~");
				return key;
			}
		}
		for (u32 i = 0; i < std::size(s_sdl_axis_setting_names); i++)
		{
			if (axis_name == s_sdl_axis_setting_names[i])
			{
				// found an axis!
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = i;
				key.modifier = (binding[0] == 'F') ? InputModifier::FullAxis :
				               (binding[0] == '-') ? InputModifier::Negate :
				                                     InputModifier::None;
				return key;
			}
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
		if (binding.starts_with("JoyButton"))
		{
			if (auto value = StringUtil::FromChars<u32>(binding.substr(9)))
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = *value + std::size(s_sdl_button_setting_names);
				return key;
			}
		}
		for (u32 i = 0; i < std::size(s_sdl_button_setting_names); i++)
		{
			if (binding == s_sdl_button_setting_names[i])
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

TinyString SDLInputSource::ConvertKeyToString(InputBindingKey key, bool display, bool migration)
{
	TinyString ret;

	if (key.source_type == InputSourceType::SDL)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			const char* modifier = (key.modifier == InputModifier::FullAxis ? (display ? "Full " : "Full") : (key.modifier == InputModifier::Negate ? "-" : "+"));
			if (display)
			{
				std::lock_guard lock(m_controllers_key_mutex);

				SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
				auto it = GetControllerDataForPlayerId(key.source_index);
				if (it != m_controllers.end())
					type = SDL_GetRealGamepadType(it->gamepad);

				if (key.data < std::size(s_sdl_axis_names))
				{
					ret.format("SDL-{} {}{}", static_cast<u32>(key.source_index), modifier, s_sdl_axis_names[key.data]);
				}
				else if (key.data - std::size(s_sdl_axis_names) < std::size(s_sdl_trigger_names))
				{
					const u32 trigger_index = key.data - std::size(s_sdl_axis_names);

					if (type < std::size(s_sdl_trigger_names_list))
						ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_trigger_names_list[type][trigger_index]);
					else
						ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_trigger_names[trigger_index]);
				}
				else
				{
					bool is_sixaxis = false;
					if (it != m_controllers.end())
						is_sixaxis = IsControllerSixaxis(*it);

					const size_t joy_axis_Index = key.data - std::size(s_sdl_axis_setting_names);

					if (is_sixaxis && key.modifier == InputModifier::FullAxis && key.invert == false &&
						joy_axis_Index < std::size(s_sdl_ps3_sxs_pressure_names) && s_sdl_ps3_sxs_pressure_names[joy_axis_Index] != nullptr)
					{
						ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_ps3_sxs_pressure_names[joy_axis_Index]);
					}
					else
						ret.format("SDL-{} {}Axis {}{}", static_cast<u32>(key.source_index), modifier, joy_axis_Index + 1, key.invert ? "~" : "");
				}
			}
			else
			{
				if (key.data < std::size(s_sdl_axis_setting_names))
					ret.format("SDL-{}/{}{}", static_cast<u32>(key.source_index), modifier, s_sdl_axis_setting_names[key.data]);
				else
					ret.format("SDL-{}/{}JoyAxis{}{}", static_cast<u32>(key.source_index), modifier, key.data - std::size(s_sdl_axis_setting_names), (key.invert && (migration || !ShouldIgnoreInversion())) ? "~" : "");
			}
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (display)
			{
				std::lock_guard lock(m_controllers_key_mutex);

				SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
				auto it = GetControllerDataForPlayerId(key.source_index);
				if (it != m_controllers.end())
					type = SDL_GetRealGamepadType(it->gamepad);

				if (type > SDL_GAMEPAD_TYPE_STANDARD && type < std::size(s_sdl_button_names_list) &&
					key.data < s_sdl_button_namesize_list[type] && s_sdl_button_names_list[type][key.data] != nullptr)
				{
					ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_button_names_list[type][key.data]);
				}
				else if (key.data < 4)
				{
					SDL_GamepadButtonLabel label = SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
					if (it != m_controllers.end() && it->gamepad)
						label = SDL_GetGamepadButtonLabel(it->gamepad, static_cast<SDL_GamepadButton>(key.data));

					if (label > SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN && label < std::size(s_sdl_face_button_names))
						ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_face_button_names[label]);
					else
						ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_button_names[key.data]);
				}
				else if (key.data < std::size(s_sdl_button_names))
					ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_button_names[key.data]);
				else
					ret.format("SDL-{} Button {}", static_cast<u32>(key.source_index), key.data - std::size(s_sdl_button_setting_names) + 1);
			}
			else
			{
				if (key.data < std::size(s_sdl_button_setting_names))
					ret.format("SDL-{}/{}", static_cast<u32>(key.source_index), s_sdl_button_setting_names[key.data]);
				else
					ret.format("SDL-{}/JoyButton{}", static_cast<u32>(key.source_index), key.data - std::size(s_sdl_button_setting_names));
			}
		}
		else if (key.source_subtype == InputSubclass::ControllerHat)
		{
			const u32 hat_index = key.data / static_cast<u32>(std::size(s_sdl_hat_direction_names));
			const u32 hat_direction = key.data % static_cast<u32>(std::size(s_sdl_hat_direction_names));
			if (display)
				ret.format("SDL-{} Hat {} {}", static_cast<u32>(key.source_index), hat_index + 1, s_sdl_hat_direction_names[hat_direction]);
			else
				ret.format("SDL-{}/Hat{}{}", static_cast<u32>(key.source_index), hat_index, s_sdl_hat_direction_names[hat_direction]);
		}
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			if (display)
				ret.format("SDL-{} {} Motor", static_cast<u32>(key.source_index), key.data ? "Small" : "Large");
			else
				ret.format("SDL-{}/{}Motor", static_cast<u32>(key.source_index), key.data ? "Small" : "Large");
		}
		else if (key.source_subtype == InputSubclass::ControllerHaptic)
		{
			if (display)
				ret.format("SDL-{} Haptic", static_cast<u32>(key.source_index));
			else
				ret.format("SDL-{}/Haptic", static_cast<u32>(key.source_index));
		}
	}

	return ret;
}

TinyString SDLInputSource::ConvertKeyToIcon(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::SDL)
	{
		std::lock_guard lock(m_controllers_key_mutex);

		SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
		auto it = GetControllerDataForPlayerId(key.source_index);
		if (it != m_controllers.end())
			type = SDL_GetRealGamepadType(it->gamepad);

		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			if (key.modifier != InputModifier::FullAxis)
			{
				if (key.data < std::size(s_sdl_axis_icons))
				{
					ret.format("SDL-{}  {}", static_cast<u32>(key.source_index),
						s_sdl_axis_icons[key.data][key.modifier == InputModifier::None]);
				}
				else if (key.data - std::size(s_sdl_axis_icons) < std::size(s_sdl_trigger_icons))
				{
					const u32 trigger_index = key.data - std::size(s_sdl_axis_icons);

					if (type < std::size(s_sdl_trigger_icons_list))
						ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_trigger_icons_list[type][trigger_index]);
					else
						ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_trigger_icons[trigger_index]);
				}
			}
			else if (it != m_controllers.end() && IsControllerSixaxis(*it) && key.invert == false)
			{
				const size_t joy_axis_Index = key.data - std::size(s_sdl_axis_setting_names);

				if (joy_axis_Index < std::size(s_sdl_ps3_pressure_icons) && s_sdl_ps3_pressure_icons[joy_axis_Index] != nullptr)
					ret.format("SDL-{} {}", static_cast<u32>(key.source_index), s_sdl_ps3_pressure_icons[joy_axis_Index]);
			}
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (type > SDL_GAMEPAD_TYPE_STANDARD && type < std::size(s_sdl_button_icons_list) &&
				key.data < s_sdl_button_iconsize_list[type] && s_sdl_button_icons_list[type][key.data] != nullptr)
			{
				ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_button_icons_list[type][key.data]);
			}
			else if (key.data < 4)
			{
				SDL_GamepadButtonLabel label = SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
				if (it != m_controllers.end() && it->gamepad)
					label = SDL_GetGamepadButtonLabel(it->gamepad, static_cast<SDL_GamepadButton>(key.data));

				if (label > SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN && label < std::size(s_sdl_face_button_icons))
					ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_face_button_icons[label]);
				else
					ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_button_icons[key.data]);
			}
			else if (key.data < std::size(s_sdl_button_icons))
				ret.format("SDL-{}  {}", static_cast<u32>(key.source_index), s_sdl_button_icons[key.data]);
		}
	}

	return ret;
}

bool SDLInputSource::ProcessSDLEvent(const SDL_Event* event)
{
	switch (event->type)
	{
		case SDL_EVENT_GAMEPAD_ADDED:
		{
			Console.WriteLn("SDLInputSource: Gamepad %d inserted", event->gdevice.which);
			OpenDevice(event->gdevice.which, true);
			return true;
		}

		case SDL_EVENT_GAMEPAD_REMOVED:
		{
			Console.WriteLn("SDLInputSource: Gamepad %d removed", event->gdevice.which);
			CloseDevice(event->gdevice.which);
			return true;
		}

		case SDL_EVENT_JOYSTICK_ADDED:
		{
			// Let gamepad handle.. well.. gamepads.
			if (SDL_IsGamepad(event->jdevice.which))
				return false;

			Console.WriteLn("SDLInputSource: Joystick %d inserted", event->jdevice.which);
			OpenDevice(event->jdevice.which, false);
			return true;
		}
		break;

		case SDL_EVENT_JOYSTICK_REMOVED:
		{
			if (auto it = GetControllerDataForJoystickId(event->jdevice.which); it != m_controllers.end() && it->gamepad)
				return false;

			Console.WriteLn("SDLInputSource: Joystick %d removed", event->jdevice.which);
			CloseDevice(event->jdevice.which);
			return true;
		}

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			return HandleGamepadAxisEvent(&event->gaxis);

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			return HandleGamepadButtonEvent(&event->gbutton);

		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			return HandleJoystickAxisEvent(&event->jaxis);

		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
		case SDL_EVENT_JOYSTICK_BUTTON_UP:
			return HandleJoystickButtonEvent(&event->jbutton);

		case SDL_EVENT_JOYSTICK_HAT_MOTION:
			return HandleJoystickHatEvent(&event->jhat);

		default:
			return false;
	}
}

SDL_Joystick* SDLInputSource::GetJoystickForDevice(const std::string_view device)
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

SDLInputSource::ControllerDataVector::iterator SDLInputSource::GetControllerDataForJoystickId(SDL_JoystickID id)
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

bool SDLInputSource::OpenDevice(SDL_JoystickID index, bool is_gamepad)
{
	SDL_Gamepad* gamepad;
	SDL_Joystick* joystick;

	if (is_gamepad)
	{
		gamepad = SDL_OpenGamepad(index);
		joystick = gamepad ? SDL_GetGamepadJoystick(gamepad) : nullptr;
	}
	else
	{
		gamepad = nullptr;
		joystick = SDL_OpenJoystick(index);
	}

	if (!gamepad && !joystick)
	{
		ERROR_LOG("SDLInputSource: Failed to open controller {}", index);
		return false;
	}

	const SDL_JoystickID joystick_id = SDL_GetJoystickID(joystick);
	int player_id = gamepad ? SDL_GetGamepadPlayerIndex(gamepad) : SDL_GetJoystickPlayerIndex(joystick);
	for (auto it = m_controllers.begin(); it != m_controllers.end(); ++it)
	{
		if (it->joystick_id == joystick_id)
		{
			ERROR_LOG("SDLInputSource: Controller {}, instance {}, player {} already connected, ignoring.", index, joystick_id, player_id);
			if (gamepad)
				SDL_CloseGamepad(gamepad);
			else
				SDL_CloseJoystick(joystick);

			return false;
		}
	}

	if (player_id < 0 || GetControllerDataForPlayerId(player_id) != m_controllers.end())
	{
		const int free_player_id = GetFreePlayerId();
		WARNING_LOG("SDLInputSource: Controller {} (joystick {}) returned player ID {}, which is invalid or in "
					"use. Using ID {} instead.",
			index, joystick_id, player_id, free_player_id);
		player_id = free_player_id;
	}

	const char* name = gamepad ? SDL_GetGamepadName(gamepad) : SDL_GetJoystickName(joystick);
	if (!name)
		name = "Unknown Device";

	INFO_LOG("SDLInputSource: Opened {} {} (instance id {}, player id {}): {}", is_gamepad ? "gamepad" : "joystick",
		index, joystick_id, player_id, name);

	ControllerData cd = {};
	cd.player_id = player_id;
	cd.joystick_id = joystick_id;
	cd.haptic_left_right_effect = -1;
	cd.gamepad = gamepad;
	cd.joystick = joystick;

	if (gamepad)
	{
		int binding_count;
		SDL_GamepadBinding** bindings = SDL_GetGamepadBindings(gamepad, &binding_count);
		if (bindings)
		{
			const int num_axes = SDL_GetNumJoystickAxes(joystick);
			const int num_buttons = SDL_GetNumJoystickButtons(joystick);
			cd.joy_axis_used_in_pad.resize(num_axes, false);
			cd.joy_button_used_in_pad.resize(num_buttons, false);
			auto mark_bind = [&](SDL_GamepadBinding* bind) {
				if (bind->input_type == SDL_GAMEPAD_BINDTYPE_AXIS && bind->input.axis.axis < num_axes)
					cd.joy_axis_used_in_pad[bind->input.axis.axis] = true;
				if (bind->input_type == SDL_GAMEPAD_BINDTYPE_BUTTON && bind->input.button < num_buttons)
					cd.joy_button_used_in_pad[bind->input.button] = true;
			};

			for (int i = 0; i < binding_count; i++)
				mark_bind(bindings[i]);

			SDL_free(bindings);

			INFO_LOG("SDLInputSource: Gamepad {} has {} axes and {} buttons", player_id, num_axes, num_buttons);
		}
		else
			ERROR_LOG("SDLInputSource: Failed to get gamepad bindings {}", SDL_GetError());
	}
	else
	{
		// Gamepad doesn't have the concept of hats, so we only need to do this for joysticks.
		const int num_hats = SDL_GetNumJoystickHats(joystick);
		if (num_hats > 0)
			cd.last_hat_state.resize(static_cast<size_t>(num_hats), u8{0});

		INFO_LOG("SDLInputSource: Joystick {} has {} axes, {} buttons and {} hats", player_id,
			SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickButtons(joystick), num_hats);
	}

	cd.use_gamepad_rumble = (gamepad && SDL_RumbleGamepad(gamepad, 0, 0, 0));
	if (cd.use_gamepad_rumble)
	{
		INFO_LOG("SDLInputSource: Rumble is supported on '{}' via gamepad", name);
	}
	else
	{
		SDL_Haptic* haptic = SDL_OpenHapticFromJoystick(joystick);
		if (haptic)
		{
			SDL_HapticEffect ef = {};
			ef.leftright.type = SDL_HAPTIC_LEFTRIGHT;
			ef.leftright.length = 1000;

			int ef_id = SDL_CreateHapticEffect(haptic, &ef);
			if (ef_id >= 0)
			{
				cd.haptic = haptic;
				cd.haptic_left_right_effect = ef_id;
			}
			else
			{
				ERROR_LOG("SDLInputSource: Failed to create haptic left/right effect: {}", SDL_GetError());
				if (SDL_HapticRumbleSupported(haptic) && SDL_InitHapticRumble(haptic))
				{
					cd.haptic = haptic;
				}
				else
				{
					ERROR_LOG("SDLInputSource: No haptic rumble supported: {}", SDL_GetError());
					SDL_CloseHaptic(haptic);
				}
			}
		}

		if (cd.haptic)
			INFO_LOG("SDLInputSource: Rumble is supported on '{}' via haptic", name);
	}

	if (!cd.haptic && !cd.use_gamepad_rumble)
		WARNING_LOG("SDLInputSource: Rumble is not supported on '{}'", name);

	if (gamepad)
	{
		const SDL_PropertiesID props = SDL_GetGamepadProperties(gamepad);
		bool hasLED = false;
		if (props == 0)
			ERROR_LOG("SDLInputSource: SDL_GetGamepadProperties() failed");
		else
			hasLED = SDL_GetBooleanProperty(props, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);

		if (player_id >= 0 && static_cast<u32>(player_id) < MAX_LED_COLORS && hasLED)
		{
			SetGamepadRGBLED(gamepad, m_led_colors[player_id]);
		}
	}

	{
		std::unique_lock lock(m_controllers_key_mutex);
		m_controllers.push_back(std::move(cd));

		if (gamepad)
		{
			// Perform SDL2-SDL3 migrations that require inspecting the gamepad
			auto idx = std::find(m_gamepads_needing_migration.begin(), m_gamepads_needing_migration.end(), player_id);
			if (idx != m_gamepads_needing_migration.end())
			{
				m_gamepads_needing_migration.erase(idx);

				// ParseKeyString will need the lock when migrating
				// unlock here so we don't deadlock reloading binds
				lock.unlock();
				// Reload bindings to perform migration
				VMManager::ReloadInputBindings(true);
			}
		}
	}

	InputManager::OnInputDeviceConnected(fmt::format("SDL-{}", player_id), name);
	return true;
}

bool SDLInputSource::CloseDevice(SDL_JoystickID joystick_index)
{
	auto it = GetControllerDataForJoystickId(joystick_index);
	if (it == m_controllers.end())
		return false;

	{
		std::lock_guard lock(m_controllers_key_mutex);
		InputManager::OnInputDeviceDisconnected(
			{InputBindingKey{.source_type = InputSourceType::SDL, .source_index = static_cast<u32>(it->player_id)}},
			fmt::format("SDL-{}", it->player_id));

		if (it->haptic)
			SDL_CloseHaptic(it->haptic);

		if (it->gamepad)
			SDL_CloseGamepad(it->gamepad);
		else
			SDL_CloseJoystick(it->joystick);

		m_controllers.erase(it);
	}

	return true;
}

static float NormalizeS16(s16 value)
{
	return static_cast<float>(value) / (value < 0 ? 32768.0f : 32767.0f);
}

bool SDLInputSource::HandleGamepadAxisEvent(const SDL_GamepadAxisEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerAxisKey(InputSourceType::SDL, it->player_id, ev->axis));
	InputManager::InvokeEvents(key, NormalizeS16(ev->value));
	return true;
}

bool SDLInputSource::HandleGamepadButtonEvent(const SDL_GamepadButtonEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;

	const InputBindingKey key(MakeGenericControllerButtonKey(InputSourceType::SDL, it->player_id, ev->button));
	const GenericInputBinding generic_key = (ev->button < std::size(s_sdl_generic_binding_button_mapping)) ?
												s_sdl_generic_binding_button_mapping[ev->button] :
												GenericInputBinding::Unknown;
	InputManager::InvokeEvents(key, static_cast<float>(ev->down), generic_key);
	return true;
}

bool SDLInputSource::HandleJoystickAxisEvent(const SDL_JoyAxisEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;
	if (ev->axis < it->joy_axis_used_in_pad.size() && it->joy_axis_used_in_pad[ev->axis])
		return false; // Will get handled by Gamepad event
	const u32 axis = ev->axis + std::size(s_sdl_axis_setting_names); // Ensure we don't conflict with Gamepad axes
	const InputBindingKey key(MakeGenericControllerAxisKey(InputSourceType::SDL, it->player_id, axis));
	InputManager::InvokeEvents(key, NormalizeS16(ev->value));
	return true;
}

bool SDLInputSource::HandleJoystickButtonEvent(const SDL_JoyButtonEvent* ev)
{
	auto it = GetControllerDataForJoystickId(ev->which);
	if (it == m_controllers.end())
		return false;
	if (ev->button < it->joy_button_used_in_pad.size() && it->joy_button_used_in_pad[ev->button])
		return false; // Will get handled by Gamepad event
	const u32 button = ev->button + std::size(s_sdl_button_setting_names); // Ensure we don't conflict with Gamepad buttons
	const InputBindingKey key(MakeGenericControllerButtonKey(InputSourceType::SDL, it->player_id, button));
	InputManager::InvokeEvents(key, static_cast<float>(ev->down));
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

		if (cd.use_gamepad_rumble || cd.haptic_left_right_effect)
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

bool SDLInputSource::GetGenericBindingMapping(const std::string_view device, InputManager::GenericInputBindingMapping* mapping)
{
	if (!device.starts_with("SDL-"))
		return false;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(4));
	if (!player_id.has_value() || player_id.value() < 0)
		return false;

	ControllerDataVector::iterator it = GetControllerDataForPlayerId(player_id.value());
	if (it == m_controllers.end())
		return false;

	if (it->gamepad)
	{
		// assume all buttons are present.
		const s32 pid = player_id.value();
		for (u32 i = 0; i < std::size(s_sdl_generic_binding_axis_mapping); i++)
		{
			const GenericInputBinding negative = s_sdl_generic_binding_axis_mapping[i][0];
			const GenericInputBinding positive = s_sdl_generic_binding_axis_mapping[i][1];
			if (negative != GenericInputBinding::Unknown)
				mapping->emplace_back(negative, fmt::format("SDL-{}/-{}", pid, s_sdl_axis_setting_names[i]));

			if (positive != GenericInputBinding::Unknown)
				mapping->emplace_back(positive, fmt::format("SDL-{}/+{}", pid, s_sdl_axis_setting_names[i]));
		}

		if (IsControllerSixaxis(*it))
		{
			// PS3 with pressure sensitive support
			for (u32 i = 0; i < std::size(s_sdl_ps3_binding_pressure_mapping); i++)
			{
				const GenericInputBinding binding = s_sdl_ps3_binding_pressure_mapping[i];
				if (binding != GenericInputBinding::Unknown)
					mapping->emplace_back(binding, fmt::format("SDL-{}/FullJoyAxis{}", pid, i));
			}

			// PS3 non pressure sensitive buttons
			for (u32 i = 0; i < std::size(s_sdl_ps3_binding_button_mapping); i++)
			{
				const GenericInputBinding binding = s_sdl_ps3_binding_button_mapping[i];
				if (binding != GenericInputBinding::Unknown)
					mapping->emplace_back(binding, fmt::format("SDL-{}/{}", pid, s_sdl_button_setting_names[i]));
			}
		}
		else
		{
			// Standard buttons
			for (u32 i = 0; i < std::size(s_sdl_generic_binding_button_mapping); i++)
			{
				const GenericInputBinding binding = s_sdl_generic_binding_button_mapping[i];
				if (binding != GenericInputBinding::Unknown)
					mapping->emplace_back(binding, fmt::format("SDL-{}/{}", pid, s_sdl_button_setting_names[i]));
			}
		}

		if (it->use_gamepad_rumble || it->haptic_left_right_effect)
		{
			mapping->emplace_back(GenericInputBinding::SmallMotor, fmt::format("SDL-{}/SmallMotor", pid));
			mapping->emplace_back(GenericInputBinding::LargeMotor, fmt::format("SDL-{}/LargeMotor", pid));
		}
		else
		{
			mapping->emplace_back(GenericInputBinding::SmallMotor, fmt::format("SDL-{}/Haptic", pid));
			mapping->emplace_back(GenericInputBinding::LargeMotor, fmt::format("SDL-{}/Haptic", pid));
		}

		return true;
	}
	else
	{
		// joysticks have arbitrary axis numbers, so automapping isn't going to work here.
		return false;
	}
}

InputLayout SDLInputSource::GetControllerLayout(u32 index)
{
	auto it = GetControllerDataForPlayerId(index);
	if (it == m_controllers.end())
		return InputLayout::Unknown;

	// Infer layout based on face button label to avoid having
	// to maintain a long switch statement of gamepad types
	// clang-format off
	switch (SDL_GetGamepadButtonLabel(it->gamepad, SDL_GAMEPAD_BUTTON_EAST))
	{
		case SDL_GAMEPAD_BUTTON_LABEL_B:      return InputLayout::Xbox;
		case SDL_GAMEPAD_BUTTON_LABEL_A:      return InputLayout::Nintendo;
		case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return InputLayout::Playstation;
		default:                              return InputLayout::Unknown;
	}
	// clang-format on
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

	if (cd->use_gamepad_rumble)
	{
		SDL_RumbleGamepad(cd->gamepad, cd->rumble_intensity[0], cd->rumble_intensity[1], DURATION);
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
			SDL_UpdateHapticEffect(cd->haptic, cd->haptic_left_right_effect, &ef);
			SDL_RunHapticEffect(cd->haptic, cd->haptic_left_right_effect, SDL_HAPTIC_INFINITY);
		}
		else
		{
			SDL_StopHapticEffect(cd->haptic, cd->haptic_left_right_effect);
		}
	}
	else
	{
		const float strength = static_cast<float>(std::max(cd->rumble_intensity[0], cd->rumble_intensity[1])) * (1.0f / 65535.0f);
		if (strength > 0.0f)
			SDL_PlayHapticRumble(cd->haptic, strength, DURATION);
		else
			SDL_StopHapticRumble(cd->haptic);
	}
}

bool SDLInputSource::IsControllerSixaxis(const ControllerData& cd)
{
	const SDL_GamepadType type = SDL_GetRealGamepadType(cd.gamepad);

	// We check the number of buttons to exclude DsHidMini's SDF mode (which has 17 buttons??)
	// SDF's input layout differs from the sixaxis or linux drivers, we only support the latter layout.
	// This differing layout also isn't mapped correctly in SDL, I think due to how L2/R2 are exposed.
	// Also see SetHints regarding reading the pressure sense from DsHidMini's SDF mode.
	return type == SDL_GAMEPAD_TYPE_PS3 &&
		   SDL_GetNumJoystickAxes(cd.joystick) == 16 &&
		   SDL_GetNumJoystickButtons(cd.joystick) == 11;
}
