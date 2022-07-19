#pragma once

#include <vector>

#include "pcsx2/PAD/Host/PAD.h"

struct Python2KeyMapping
{
    uint32_t uniqueId;
    std::string inputKey;
    std::string keybind;
    PAD::ControllerBindingType input_type;
    double analogDeadzone;
    double analogSensitivity;
    double motorScale;
    bool isOneshot; // Immediately trigger an off after on
};

struct Python2BindingInfo
{
    const char* name;
    const char* display_name;
    PAD::ControllerBindingType type;
    bool is_oneshot;
};

struct Python2SystemInfo
{
    const char* name;
    const Python2BindingInfo* bindings;
    u32 num_bindings;
};

static const Python2BindingInfo s_python2_system_binds[] = {
	{"Test", "Test", PAD::ControllerBindingType::Button, false},
	{"Service", "Service", PAD::ControllerBindingType::Button, false},
	{"Coin1", "Coin 1", PAD::ControllerBindingType::Button, false},
	{"Coin2", "Coin 2", PAD::ControllerBindingType::Button, false},
};

static const Python2BindingInfo s_python2_guitarfreaks_binds[] = {
	{"GfP1Start", "P1 Start", PAD::ControllerBindingType::Button, false},
	{"GfP1Pick", "P1 Pick", PAD::ControllerBindingType::Button, true},
	{"GfP1Wail", "P1 Wail", PAD::ControllerBindingType::Button, false},
	{"GfP1EffectInc", "P1 Effect+", PAD::ControllerBindingType::Button, false},
	{"GfP1EffectDec", "P1 Effect-", PAD::ControllerBindingType::Button, false},
	{"GfP1NeckR", "P1 Neck R", PAD::ControllerBindingType::Button, false},
	{"GfP1NeckG", "P1 Neck G", PAD::ControllerBindingType::Button, false},
	{"GfP1NeckB", "P1 Neck B", PAD::ControllerBindingType::Button, false},

	{"GfP2Start", "P2 Start", PAD::ControllerBindingType::Button, false},
	{"GfP2Pick", "P2 Pick", PAD::ControllerBindingType::Button, true},
	{"GfP2Wail", "P2 Wail", PAD::ControllerBindingType::Button, false},
	{"GfP2EffectInc", "P2 Effect+", PAD::ControllerBindingType::Button, false},
	{"GfP2EffectDec", "P2 Effect-", PAD::ControllerBindingType::Button, false},
	{"GfP2NeckR", "P2 Neck R", PAD::ControllerBindingType::Button, false},
	{"GfP2NeckG", "P2 Neck G", PAD::ControllerBindingType::Button, false},
	{"GfP2NeckB", "P2 Neck B", PAD::ControllerBindingType::Button, false},
};

static const Python2BindingInfo s_python2_drummania_binds[] = {
	{"DmStart", "Start", PAD::ControllerBindingType::Button, false},
	{"DmSelectL", "Select L", PAD::ControllerBindingType::Button, false},
	{"DmSelectR", "Select R", PAD::ControllerBindingType::Button, false},
	{"DmHihat", "Hihat", PAD::ControllerBindingType::Button, true},
	{"DmSnare", "Snare", PAD::ControllerBindingType::Button, true},
	{"DmHighTom", "High Tom", PAD::ControllerBindingType::Button, true},
	{"DmLowTom", "Low Tom", PAD::ControllerBindingType::Button, true},
	{"DmCymbal", "Cymbal", PAD::ControllerBindingType::Button, true},
	{"DmBassDrum", "Bass Drum", PAD::ControllerBindingType::Button, true},
};

static const Python2BindingInfo s_python2_ddr_binds[] = {
	{"DdrP1Start", "P1 Start", PAD::ControllerBindingType::Button, false},
	{"DdrP1SelectL", "P1 Select L", PAD::ControllerBindingType::Button, false},
	{"DdrP1SelectR", "P1 Select R", PAD::ControllerBindingType::Button, false},
	{"DdrP1FootLeft", "P1 Left", PAD::ControllerBindingType::Button, false},
	{"DdrP1FootDown", "P1 Down", PAD::ControllerBindingType::Button, false},
	{"DdrP1FootUp", "P1 Up", PAD::ControllerBindingType::Button, false},
	{"DdrP1FootRight", "P1 Right", PAD::ControllerBindingType::Button, false},

	{"DdrP2Start", "P2 Start", PAD::ControllerBindingType::Button, false},
	{"DdrP2SelectL", "P2 Select L", PAD::ControllerBindingType::Button, false},
	{"DdrP2SelectR", "P2 Select R", PAD::ControllerBindingType::Button, false},
	{"DdrP2FootLeft", "P2 Left", PAD::ControllerBindingType::Button, false},
	{"DdrP2FootDown", "P2 Down", PAD::ControllerBindingType::Button, false},
	{"DdrP2FootUp", "P2 Up", PAD::ControllerBindingType::Button, false},
	{"DdrP2FootRight", "P2 Right", PAD::ControllerBindingType::Button, false},
};

static const Python2BindingInfo s_python2_thrilldrive_binds[] = {
	{"ThrillDriveStart", "Start", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveGearUp", "Gear Up", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveGearDown", "Gear Down", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveWheelAnalog", "Wheel", PAD::ControllerBindingType::Axis, false},
	{"ThrillDriveWheelLeft", "Wheel Left", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveWheelRight", "Wheel Right", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveAccelAnalog", "Acceleration", PAD::ControllerBindingType::HalfAxis, false},
	{"ThrillDriveAccel", "Acceleration", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveBrake", "Brake", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveBrakeAnalog", "Brake", PAD::ControllerBindingType::HalfAxis, false},
	{"ThrillDriveSeatbelt", "Seatbelt", PAD::ControllerBindingType::Button, false},
	{"ThrillDriveSeatbeltMotor", "Seatbelt", PAD::ControllerBindingType::Motor, false},
	{"ThrillDriveWheelMotor", "Wheel", PAD::ControllerBindingType::Motor, false},
};

static const Python2BindingInfo s_python2_dance864_binds[] = {
	{"Dance864P1Start", "P1 Start", PAD::ControllerBindingType::Button, false},
	{"Dance864P1Left", "P1 Select L", PAD::ControllerBindingType::Button, false},
	{"Dance864P1Right", "P1 Select R", PAD::ControllerBindingType::Button, false},
	{"Dance864P1PadLeft", "P1 Left", PAD::ControllerBindingType::Button, false},
	{"Dance864P1PadCenter", "P1 Center", PAD::ControllerBindingType::Button, false},
	{"Dance864P1PadRight", "P1 Right", PAD::ControllerBindingType::Button, false},

	{"Dance864P2Start", "P2 Start", PAD::ControllerBindingType::Button, false},
	{"Dance864P2Left", "P2 Select L", PAD::ControllerBindingType::Button, false},
	{"Dance864P2Right", "P2 Select R", PAD::ControllerBindingType::Button, false},
	{"Dance864P2PadLeft", "P2 Left", PAD::ControllerBindingType::Button, false},
	{"Dance864P2PadCenter", "P2 Center", PAD::ControllerBindingType::Button, false},
	{"Dance864P2PadRight", "P2 Right", PAD::ControllerBindingType::Button, false},
};

static const Python2BindingInfo s_python2_toysmarch_binds[] = {
	{"ToysMarchP1Start", "P1 Start", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP1SelectL", "P1 Select L", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP1SelectR", "P1 Select R", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP1DrumL", "P1 Drum L", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP1DrumR", "P1 Drum R", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP1Cymbal", "P1 Cymbal", PAD::ControllerBindingType::Button, false},

	{"ToysMarchP2Start", "P2 Start", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP2SelectL", "P2 Select L", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP2SelectR", "P2 Select R", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP2DrumL", "P2 Drum L", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP2DrumR", "P2 Drum R", PAD::ControllerBindingType::Button, false},
	{"ToysMarchP2Cymbal", "P2 Cymbal", PAD::ControllerBindingType::Button, false},
};

static const Python2BindingInfo s_python2_icca_binds[] = {
	{"KeypadP1_0", "P1 Keypad 0", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_1", "P1 Keypad 1", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_2", "P1 Keypad 2", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_3", "P1 Keypad 3", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_4", "P1 Keypad 4", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_5", "P1 Keypad 5", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_6", "P1 Keypad 6", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_7", "P1 Keypad 7", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_8", "P1 Keypad 8", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_9", "P1 Keypad 9", PAD::ControllerBindingType::Button, false},
	{"KeypadP1_00", "P1 Keypad 00", PAD::ControllerBindingType::Button, false},
	{"KeypadP1InsertEject", "P1 Insert/Eject Card", PAD::ControllerBindingType::Button, false},

	{"KeypadP2_0", "P2 Keypad 0", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_1", "P2 Keypad 1", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_2", "P2 Keypad 2", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_3", "P2 Keypad 3", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_4", "P2 Keypad 4", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_5", "P2 Keypad 5", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_6", "P2 Keypad 6", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_7", "P2 Keypad 7", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_8", "P2 Keypad 8", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_9", "P2 Keypad 9", PAD::ControllerBindingType::Button, false},
	{"KeypadP2_00", "P2 Keypad  00", PAD::ControllerBindingType::Button, false},
	{"KeypadP2InsertEject", "P2 Insert/Eject Card", PAD::ControllerBindingType::Button, false},
};

static const Python2SystemInfo s_python2_system_info[] = {
	{"All", nullptr, 0},
	{"System", s_python2_system_binds, std::size(s_python2_system_binds)},
	{"Dance 86.4", s_python2_dance864_binds, std::size(s_python2_dance864_binds)},
	{"Dance Dance Revolution", s_python2_ddr_binds, std::size(s_python2_ddr_binds)},
	{"Drummania", s_python2_drummania_binds, std::size(s_python2_drummania_binds)},
	{"Guitar Freaks", s_python2_guitarfreaks_binds, std::size(s_python2_guitarfreaks_binds)},
	{"Thrill Drive", s_python2_thrilldrive_binds, std::size(s_python2_thrilldrive_binds)},
	{"Toy's March", s_python2_toysmarch_binds, std::size(s_python2_toysmarch_binds)},
	{"Card Reader", s_python2_icca_binds, std::size(s_python2_icca_binds)},
};


namespace Python2QtInputManager
{
    std::vector<Python2KeyMapping> GetCurrentMappings();
    std::vector<Python2KeyMapping> GetMappingsByInputKey(std::string keyBindStr);

    bool AddNewBinding(std::string full_key, std::string new_binding, double analogDeadzone, double analogSensitivity, double motorScale);
	void RemoveMappingByUniqueId(uint32_t uniqueId);

	void LoadMapping();
}
