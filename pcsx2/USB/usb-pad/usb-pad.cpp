// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "usb-pad.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/usb-pad/usb-pad-sdl-ff.h"
#include "USB/USB.h"
#include "Host.h"
#include "StateWrapper.h"

#include "common/Console.h"

namespace usb_pad
{
	static const USBDescStrings df_desc_strings = {
		"",
		"Logitech Driving Force",
		"",
		"Logitech",
	};

	static const USBDescStrings dfp_desc_strings = {
		"",
		"Logitech Driving Force Pro",
		"",
		"Logitech",
	};

	static const USBDescStrings gtf_desc_strings = {
		"",
		"Logitech", //actual index @ 0x04
		"Logitech GT Force" //actual index @ 0x20
	};

	static const USBDescStrings rb1_desc_strings = {
		"1234567890AB",
		"Licensed by Sony Computer Entertainment America",
		"Harmonix Drum Kit for PlayStation(R)3"};

	static const USBDescStrings buzz_desc_strings = {
		"",
		"Logitech Buzz(tm) Controller V1",
		"",
		"Logitech"};

	static const USBDescStrings kbm_desc_strings = {
		"",
		"USB Multipurpose Controller",
		"",
		"KONAMI"};

	static int MapSteeringCurveExponentOptionToExponent(const std::string& option)
	{
		int exponent = 0;
		if (option == "Low")
		{
			exponent = 1;
		}
		else if (option == "Medium")
		{
			exponent = 2;
		}
		else if (option == "High")
		{
			exponent = 3;
		}

		return exponent;
	}

	static std::span<const InputBindingInfo> GetWheelBindings(PS2WheelTypes wt)
	{
		switch (wt)
		{
			case WT_GENERIC:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_UP, GenericInputBinding::DPadUp},
					{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_DOWN, GenericInputBinding::DPadDown},
					{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_LEFT, GenericInputBinding::DPadLeft},
					{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::Cross},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::Square},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Triangle},
					{"L1", TRANSLATE_NOOP("USB", "L1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON5, GenericInputBinding::L1},
					{"R1", TRANSLATE_NOOP("USB", "R1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::R1},
					{"L2", TRANSLATE_NOOP("USB", "L2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON7, GenericInputBinding::Unknown}, // used L2 for brake
					{"R2", TRANSLATE_NOOP("USB", "R2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON6, GenericInputBinding::Unknown}, // used R2 for throttle
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON8, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON9, GenericInputBinding::Start},
					{"FFDevice", TRANSLATE_NOOP("USB", "Force Feedback"), nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};

				return bindings;
			}

			case WT_DRIVING_FORCE_PRO:
			case WT_DRIVING_FORCE_PRO_1102:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_UP, GenericInputBinding::DPadUp},
					{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_DOWN, GenericInputBinding::DPadDown},
					{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_LEFT, GenericInputBinding::DPadLeft},
					{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::Cross},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::Square},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Triangle},
					{"R1", TRANSLATE_NOOP("USB", "Shift Up / R1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::R1},
					{"L1", TRANSLATE_NOOP("USB", "Shift Down / L1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON5, GenericInputBinding::L1},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON8, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON9, GenericInputBinding::Start},
					{"L2", TRANSLATE_NOOP("USB", "L2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON7, GenericInputBinding::Unknown}, // used L2 for brake
					{"R2", TRANSLATE_NOOP("USB", "R2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON6, GenericInputBinding::Unknown}, // used R2 for throttle
					{"L3", TRANSLATE_NOOP("USB", "L3"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON11, GenericInputBinding::L3},
					{"R3", TRANSLATE_NOOP("USB", "R3"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON10, GenericInputBinding::R3},
					{"FFDevice", "Force Feedback", nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};

				return bindings;
			}

			case WT_GT_FORCE:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"MenuUp", TRANSLATE_NOOP("USB", "Menu Up"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::DPadUp},
					{"MenuDown", TRANSLATE_NOOP("USB", "Menu Down"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::DPadDown},
					{"X", TRANSLATE_NOOP("USB", "X"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Square},
					{"Y", TRANSLATE_NOOP("USB", "Y"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Triangle},
					{"A", TRANSLATE_NOOP("USB", "A"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::Cross},
					{"B", TRANSLATE_NOOP("USB", "B"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON5, GenericInputBinding::Circle},
					{"FFDevice", TRANSLATE_NOOP("USB", "Force Feedback"), nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};

				return bindings;
			}

			default:
			{
				return {};
			}
		}
	}

	static std::span<const SettingInfo> GetWheelSettings(PS2WheelTypes wt)
	{
		if (wt <= WT_GT_FORCE)
		{
			static constexpr const char* SteeringCurveExponentOptions[] = {TRANSLATE_NOOP("USB", "Off"), TRANSLATE_NOOP("USB", "Low"), TRANSLATE_NOOP("USB", "Medium"), TRANSLATE_NOOP("USB", "High"), nullptr};
			static constexpr const SettingInfo info[] = {
				{SettingInfo::Type::Integer, "SteeringSmoothing", TRANSLATE_NOOP("USB", "Steering Smoothing"),
					TRANSLATE_NOOP("USB", "Smooths out changes in steering to the specified percentage per poll. Needed for using keyboards."),
					"0", "0", "100", "1", TRANSLATE_NOOP("USB", "%d%%"), nullptr, nullptr, 1.0f},
				{SettingInfo::Type::Integer, "SteeringDeadzone", TRANSLATE_NOOP("USB", "Steering Deadzone"),
					TRANSLATE_NOOP("USB", "Steering axis deadzone for pads or non self centering wheels."),
					"0", "0", "100", "1", TRANSLATE_NOOP("USB", "%d%%"), nullptr, nullptr, 1.0f},
				{SettingInfo::Type::StringList, "SteeringCurveExponent", TRANSLATE_NOOP("USB", "Steering Damping"),
					TRANSLATE_NOOP("USB", "Applies power curve filter to steering axis values. Dampens small inputs."),
					"Off", nullptr, nullptr, nullptr, nullptr, SteeringCurveExponentOptions}
			};

			return info;
		}
		else
		{
			return {};
		}
	}

	PadState::PadState(u32 port_, PS2WheelTypes type_)
		: port(port_)
		, type(type_)
	{
		if (type_ == WT_DRIVING_FORCE_PRO || type_ == WT_DRIVING_FORCE_PRO_1102)
			steering_range = 0x3FFF >> 1;
		else if (type_ == WT_SEGA_SEAMIC)
			steering_range = 0xFF >> 1;
		else
			steering_range = 0x3FF >> 1;

		steering_step = std::numeric_limits<u16>::max();

		// steering starts in the center
		data.last_steering = steering_range;
		data.steering = steering_range;

		// throttle/brake start unpressed
		data.throttle = 255;
		data.brake = 255;

		Reset();
	}

	PadState::~PadState() = default;

	void PadState::UpdateSettings(SettingsInterface& si, const char* devname)
	{
		const s32 smoothing_percent = USB::GetConfigInt(si, port, devname, "SteeringSmoothing", 0);
		if (smoothing_percent <= 0)
		{
			// none, allow any amount of change
			steering_step = std::numeric_limits<u16>::max();
		}
		else
		{
			steering_step = static_cast<u16>(std::clamp<s32>((steering_range * smoothing_percent) / 100,
				1, std::numeric_limits<u16>::max()));
		}

		steering_deadzone = (steering_range * USB::GetConfigInt(si, port, devname, "SteeringDeadzone", 0)) / 100;
		steering_curve_exponent = MapSteeringCurveExponentOptionToExponent(USB::GetConfigString(si, port, devname, "SteeringCurveExponent", "Off"));

		if (HasFF())
		{
			const std::string ffdevname(USB::GetConfigString(si, port, devname, "FFDevice"));
			if (ffdevname != mFFdevName)
			{
				mFFdev.reset();
				mFFdevName = std::move(ffdevname);
				OpenFFDevice();
			}
		}
	}

	void PadState::Reset()
	{
		data.steering = steering_range;
		mFFstate = {};
	}

	int PadState::TokenIn(u8* buf, int len)
	{
		// TODO: This is still pretty gross and needs cleaning up.

		struct wheel_lohi
		{
			u32 lo;
			u32 hi;
		};

		wheel_lohi* w = reinterpret_cast<wheel_lohi*>(buf);
		std::memset(w, 0, 8);

		switch (type)
		{
			case WT_GENERIC:
			{
				UpdateSteering();
				UpdateHatSwitch();

				DbgCon.WriteLn("Steering: %d Throttle: %d Brake: %d Buttons: %d",
					data.steering, data.throttle, data.brake, data.buttons);

				w->lo = data.steering & 0x3FF;
				w->lo |= (data.buttons & 0xFFF) << 10;
				w->lo |= 0xFF << 24;

				w->hi = (data.hatswitch & 0xF);
				w->hi |= (data.throttle & 0xFF) << 8;
				w->hi |= (data.brake & 0xFF) << 16;

				return len;
			}

			case WT_GT_FORCE:
			{
				UpdateSteering();
				UpdateHatSwitch();

				w->lo = data.steering & 0x3FF;
				w->lo |= (data.buttons & 0xFFF) << 10;
				w->lo |= 0xFF << 24;

				w->hi = (data.throttle & 0xFF);
				w->hi |= (data.brake & 0xFF) << 8;

				return len;
			}

			case WT_DRIVING_FORCE_PRO:
			{
				UpdateSteering();
				UpdateHatSwitch();

				w->lo = data.steering & 0x3FFF;
				w->lo |= (data.buttons & 0x3FFF) << 14;
				w->lo |= (data.hatswitch & 0xF) << 28;

				w->hi = 0x00;
				w->hi |= data.throttle << 8;
				w->hi |= data.brake << 16; //axis_rz
				w->hi |= 0x11 << 24; //enables wheel and pedals?

				return len;
			}

			case WT_DRIVING_FORCE_PRO_1102:
			{
				UpdateSteering();
				UpdateHatSwitch();

				// what's up with the bitmap?
				// xxxxxxxx xxxxxxbb bbbbbbbb bbbbhhhh ???????? ?01zzzzz 1rrrrrr1 10001000
				w->lo = data.steering & 0x3FFF;
				w->lo |= (data.buttons & 0x3FFF) << 14;
				w->lo |= (data.hatswitch & 0xF) << 28;

				w->hi = 0x00;
				//w->hi |= 0 << 9; //bit 9 must be 0
				w->hi |= (1 | (data.throttle * 0x3F) / 0xFF) << 10; //axis_z
				w->hi |= 1 << 16; //bit 16 must be 1
				w->hi |= ((0x3F - (data.brake * 0x3F) / 0xFF) & 0x3F) << 17; //axis_rz
				w->hi |= 1 << 23; //bit 23 must be 1
				w->hi |= 0x11 << 24; //enables wheel and pedals?

				return len;
			}

			case WT_ROCKBAND1_DRUMKIT:
			{
				UpdateHatSwitch();

				w->lo = (data.buttons & 0xFFF);
				w->lo |= (data.hatswitch & 0xF) << 16;

				return len;
			}

			case WT_BUZZ_CONTROLLER:
			{
				// https://gist.github.com/Lewiscowles1986/eef220dac6f0549e4702393a7b9351f6
				buf[0] = 0x7f;
				buf[1] = 0x7f;
				buf[2] = data.buttons & 0xff;
				buf[3] = (data.buttons >> 8) & 0xff;
				buf[4] = 0xf0 | ((data.buttons >> 16) & 0xf);

				return 5;
			}

			case WT_SEGA_SEAMIC:
			{
				UpdateSteering();
				UpdateHatSwitch();

				buf[0] = data.steering & 0xFF;
				buf[1] = data.throttle & 0xFF;
				buf[2] = data.brake & 0xFF;
				buf[3] = data.hatswitch & 0x0F; // 4bits?
				buf[3] |= (data.buttons & 0x0F) << 4; // 4 bits // TODO Or does it start at buf[4]?
				buf[4] = (data.buttons >> 4) & 0x3F; // 10 - 4 = 6 bits

				return len;
			}

			case WT_KEYBOARDMANIA_CONTROLLER:
			{
				buf[0] = 0x3F;
				buf[1] = data.buttons & 0xFF;
				buf[2] = (data.buttons >> 8) & 0xFF;
				buf[3] = (data.buttons >> 16) & 0xFF;
				buf[4] = (data.buttons >> 24) & 0xFF;

				return len;
			}

			default:
			{
				return len;
			}
		}
	}

	int PadState::TokenOut(const u8* data, int len)
	{
		const ff_data* ffdata = reinterpret_cast<const ff_data*>(data);
		bool hires = (type == WT_DRIVING_FORCE_PRO || type == WT_DRIVING_FORCE_PRO_1102);
		ParseFFData(ffdata, hires);
		return len;
	}

	float PadState::GetBindValue(u32 bind_index) const
	{
		switch (bind_index)
		{
			case CID_STEERING_L:
				return static_cast<float>(data.steering_left) / static_cast<float>(steering_range);

			case CID_STEERING_R:
				return static_cast<float>(data.steering_right) / static_cast<float>(steering_range);

			case CID_THROTTLE:
				return 1.0f - (static_cast<float>(data.throttle) / 255.0f);

			case CID_BRAKE:
				return 1.0f - (static_cast<float>(data.brake) / 255.0f);

			case CID_DPAD_UP:
				return static_cast<float>(data.hat_up);

			case CID_DPAD_DOWN:
				return static_cast<float>(data.hat_down);

			case CID_DPAD_LEFT:
				return static_cast<float>(data.hat_left);

			case CID_DPAD_RIGHT:
				return static_cast<float>(data.hat_right);

			case CID_BUTTON0:
			case CID_BUTTON1:
			case CID_BUTTON2:
			case CID_BUTTON3:
			case CID_BUTTON5:
			case CID_BUTTON4:
			case CID_BUTTON7:
			case CID_BUTTON6:
			case CID_BUTTON8:
			case CID_BUTTON9:
			case CID_BUTTON10:
			case CID_BUTTON11:
			case CID_BUTTON12:
			case CID_BUTTON13:
			case CID_BUTTON14:
			case CID_BUTTON15:
			case CID_BUTTON16:
			case CID_BUTTON17:
			case CID_BUTTON18:
			case CID_BUTTON19:
			case CID_BUTTON20:
			case CID_BUTTON21:
			case CID_BUTTON22:
			case CID_BUTTON23:
			case CID_BUTTON24:
			case CID_BUTTON25:
			case CID_BUTTON26:
			case CID_BUTTON27:
			case CID_BUTTON28:
			case CID_BUTTON29:
			case CID_BUTTON30:
			case CID_BUTTON31:
			{
				const u32 mask = (1u << (bind_index - CID_BUTTON0));
				return ((data.buttons & mask) != 0u) ? 1.0f : 0.0f;
			}

			default:
				return 0.0f;
		}
	}

	s16 PadState::ApplySteeringAxisModifiers(float value)
	{
		const s16 raw_steering = static_cast<s16>(std::lroundf(value * static_cast<float>(steering_range)));
		const s16 deadzone_offset = static_cast<s16>(std::lroundf(value * static_cast<float>(steering_deadzone)));
		const s16 deadzone_modified_steering = std::max((raw_steering - steering_deadzone + deadzone_offset), 0);
		
		if (steering_curve_exponent)
		{
			return std::pow(deadzone_modified_steering, steering_curve_exponent + 1) / std::pow(steering_range, steering_curve_exponent);
		}
		else
		{
			return deadzone_modified_steering;
		}
	}

	void PadState::SetBindValue(u32 bind_index, float value)
	{
		switch (bind_index)
		{
			case CID_STEERING_L:
				data.steering_left = ApplySteeringAxisModifiers(value);
				UpdateSteering();
				break;

			case CID_STEERING_R:
				data.steering_right = ApplySteeringAxisModifiers(value);
				UpdateSteering();
				break;

			case CID_THROTTLE:
				data.throttle = static_cast<u32>(255 - std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_BRAKE:
				data.brake = static_cast<u32>(255 - std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_DPAD_UP:
				data.hat_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_DOWN:
				data.hat_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_LEFT:
				data.hat_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_RIGHT:
				data.hat_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;

			case CID_BUTTON0:
			case CID_BUTTON1:
			case CID_BUTTON2:
			case CID_BUTTON3:
			case CID_BUTTON5:
			case CID_BUTTON4:
			case CID_BUTTON7:
			case CID_BUTTON6:
			case CID_BUTTON8:
			case CID_BUTTON9:
			case CID_BUTTON10:
			case CID_BUTTON11:
			case CID_BUTTON12:
			case CID_BUTTON13:
			case CID_BUTTON14:
			case CID_BUTTON15:
			case CID_BUTTON16:
			case CID_BUTTON17:
			case CID_BUTTON18:
			case CID_BUTTON19:
			case CID_BUTTON20:
			case CID_BUTTON21:
			case CID_BUTTON22:
			case CID_BUTTON23:
			case CID_BUTTON24:
			case CID_BUTTON25:
			case CID_BUTTON26:
			case CID_BUTTON27:
			case CID_BUTTON28:
			case CID_BUTTON29:
			case CID_BUTTON30:
			case CID_BUTTON31:
			{
				const u32 mask = (1u << (bind_index - CID_BUTTON0));
				if (value >= 0.5f)
					data.buttons |= mask;
				else
					data.buttons &= ~mask;
			}
			break;

			default:
				break;
		}
	}

	void PadState::UpdateSteering()
	{
		u16 value;
		if (data.steering_left > 0)
			value = static_cast<u16>(std::max<int>(steering_range - data.steering_left, 0));
		else
			value = static_cast<u16>(std::min<int>(steering_range + data.steering_right, steering_range * 2));

		// TODO: Smoothing, don't jump too much
		//data.steering = value;
		if (value < data.steering)
			data.steering -= std::min<u16>(data.steering - value, steering_step);
		else if (value > data.steering)
			data.steering += std::min<u16>(value - data.steering, steering_step);
	}

	void PadState::UpdateHatSwitch()
	{
		if (data.hat_up && data.hat_right)
			data.hatswitch = 1;
		else if (data.hat_right && data.hat_down)
			data.hatswitch = 3;
		else if (data.hat_down && data.hat_left)
			data.hatswitch = 5;
		else if (data.hat_left && data.hat_up)
			data.hatswitch = 7;
		else if (data.hat_up)
			data.hatswitch = 0;
		else if (data.hat_right)
			data.hatswitch = 2;
		else if (data.hat_down)
			data.hatswitch = 4;
		else if (data.hat_left)
			data.hatswitch = 6;
		else
			data.hatswitch = 8;
	}

	bool PadState::HasFF() const
	{
		// only do force feedback for wheels...
		return (type <= WT_GT_FORCE);
	}

	void PadState::OpenFFDevice()
	{
		if (mFFdevName.empty())
			return;

		mFFdev.reset();
		mFFdev = SDLFFDevice::Create(mFFdevName);
	}

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (p->ep->nr == 1)
				{
					int ret = s->TokenIn(p->buffer_ptr, p->buffer_size);

					if (ret > 0)
						p->actual_length += std::min<u32>(static_cast<u32>(ret), p->buffer_size);
					else
						p->status = ret;
				}
				else
				{
					goto fail;
				}
				break;
			case USB_TOKEN_OUT:
				/*Console.Warning("usb-pad: data token out len=0x%X %X,%X,%X,%X,%X,%X,%X,%X\n",len,
			data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);*/
				//Console.Warning("usb-pad: data token out len=0x%X\n",len);
				s->TokenOut(p->buffer_ptr, p->buffer_size);
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_reset(USBDevice* dev)
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		s->Reset();
	}

	static void pad_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		int ret = 0;

		switch (request)
		{
			case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret < 0)
					goto fail;

				break;
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR: //GT3
				switch (value >> 8)
				{
					// TODO: Move to constructor
					case USB_DT_REPORT:
						if (s->type == WT_DRIVING_FORCE_PRO || s->type == WT_DRIVING_FORCE_PRO_1102)
						{
							ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
							memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
						}
						else if (s->type == WT_GT_FORCE)
						{
							ret = sizeof(pad_gtforce_hid_report_descriptor);
							memcpy(data, pad_gtforce_hid_report_descriptor, ret);
						}
						else if (s->type == WT_KEYBOARDMANIA_CONTROLLER)
						{
							ret = sizeof(kbm_hid_report_descriptor);
							memcpy(data, kbm_hid_report_descriptor, ret);
						}
						else if (s->type == WT_GENERIC)
						{
							ret = sizeof(pad_driving_force_hid_separate_report_descriptor);
							memcpy(data, pad_driving_force_hid_separate_report_descriptor, ret);
						}
						else if (s->type == WT_BUZZ_CONTROLLER)
						{
							ret = sizeof(buzz_hid_report_descriptor);
							memcpy(data, buzz_hid_report_descriptor, ret);
						}
						p->actual_length = ret;
						break;
					default:
						goto fail;
				}
				break;

			/* hid specific requests */
			case SET_REPORT:
				// no idea, Rock Band 2 keeps spamming this
				if (length > 0)
				{
					/* 0x01: Num Lock LED
			 * 0x02: Caps Lock LED
			 * 0x04: Scroll Lock LED
			 * 0x08: Compose LED
			 * 0x10: Kana LED */
					p->actual_length = 0;
					//p->status = USB_RET_SUCCESS;
				}
				break;
			case SET_IDLE:
				break;
			default:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret >= 0)
				{
					return;
				}
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_destroy(USBDevice* dev)
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		delete s;
	}

	static void pad_init(PadState* s)
	{
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset(&s->dev);
	}

	USBDevice* PadDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		if (subtype >= WT_COUNT)
			return nullptr;

		PadState* s = new PadState(port, static_cast<PS2WheelTypes>(subtype));
		s->desc.full = &s->desc_dev;
		s->desc.str = df_desc_strings;

		const uint8_t* dev_desc = df_dev_descriptor;
		int dev_desc_len = sizeof(df_dev_descriptor);
		const uint8_t* config_desc = df_config_descriptor;
		int config_desc_len = sizeof(df_config_descriptor);

		switch (s->type)
		{
			case WT_DRIVING_FORCE_PRO:
			{
				dev_desc = dfp_dev_descriptor;
				dev_desc_len = sizeof(dfp_dev_descriptor);
				config_desc = dfp_config_descriptor;
				config_desc_len = sizeof(dfp_config_descriptor);
				s->desc.str = dfp_desc_strings;
			}
			break;
			case WT_DRIVING_FORCE_PRO_1102:
			{
				dev_desc = dfp_dev_descriptor_1102;
				dev_desc_len = sizeof(dfp_dev_descriptor_1102);
				config_desc = dfp_config_descriptor;
				config_desc_len = sizeof(dfp_config_descriptor);
				s->desc.str = dfp_desc_strings;
			}
			break;
			case WT_GT_FORCE:
			{
				dev_desc = gtf_dev_descriptor;
				dev_desc_len = sizeof(gtf_dev_descriptor);
				config_desc = gtforce_config_descriptor; //TODO
				config_desc_len = sizeof(gtforce_config_descriptor);
				s->desc.str = gtf_desc_strings;
			}
			default:
				break;
		}

		if (usb_desc_parse_dev(dev_desc, dev_desc_len, s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_desc, config_desc_len, s->desc_dev) < 0)
			goto fail;

		s->UpdateSettings(si, TypeName());
		pad_init(s);

		return &s->dev;

	fail:
		pad_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* PadDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Wheel Device");
	}

	const char* PadDevice::TypeName() const
	{
		return "Pad";
	}

	bool PadDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);

		if (!sw.DoMarker("PadDevice"))
			return false;

		sw.Do(&s->data.last_steering);
		sw.DoPOD(&s->mFFstate);
		return true;
	}

	void PadDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		USB_CONTAINER_OF(dev, PadState, dev)->UpdateSettings(si, TypeName());
	}

	float PadDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		const PadState* s = USB_CONTAINER_OF(dev, const PadState, dev);
		return s->GetBindValue(bind_index);
	}

	void PadDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		s->SetBindValue(bind_index, value);
	}

	std::span<const char*> PadDevice::SubTypes() const
	{
		static const char* subtypes[] = {TRANSLATE_NOOP("USB", "Driving Force"),
			TRANSLATE_NOOP("USB", "Driving Force Pro"), TRANSLATE_NOOP("USB", "Driving Force Pro (rev11.02)"),
			TRANSLATE_NOOP("USB", "GT Force")};
		return subtypes;
	}

	std::span<const InputBindingInfo> PadDevice::Bindings(u32 subtype) const
	{
		return GetWheelBindings(static_cast<PS2WheelTypes>(subtype));
	}

	std::span<const SettingInfo> PadDevice::Settings(u32 subtype) const
	{
		return GetWheelSettings(static_cast<PS2WheelTypes>(subtype));
	}

	void PadDevice::InputDeviceConnected(USBDevice* dev, const std::string_view& identifier) const
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		if (s->mFFdevName == identifier && s->HasFF())
			s->OpenFFDevice();
	}

	void PadDevice::InputDeviceDisconnected(USBDevice* dev, const std::string_view& identifier) const
	{
		PadState* s = USB_CONTAINER_OF(dev, PadState, dev);
		if (s->mFFdevName == identifier)
			s->mFFdev.reset();
	}

	// ---- Rock Band drum kit ----

	const char* RBDrumKitDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Rock Band Drum Kit");
	}

	const char* RBDrumKitDevice::TypeName() const
	{
		return "RBDrumKit";
	}

	USBDevice* RBDrumKitDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		PadState* s = new PadState(port, WT_ROCKBAND1_DRUMKIT);

		s->desc.full = &s->desc_dev;
		s->desc.str = rb1_desc_strings;

		if (usb_desc_parse_dev(rb1_dev_descriptor, sizeof(rb1_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(rb1_config_descriptor, sizeof(rb1_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s);

		return &s->dev;

	fail:
		pad_handle_destroy(&s->dev);
		return nullptr;
	}

	std::span<const char*> RBDrumKitDevice::SubTypes() const
	{
		return {};
	}

	std::span<const InputBindingInfo> RBDrumKitDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"Blue", TRANSLATE_NOOP("USB", "Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::R1},
			{"Green", TRANSLATE_NOOP("USB", "Green"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::Triangle},
			{"Red", TRANSLATE_NOOP("USB", "Red"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Circle},
			{"Yellow", TRANSLATE_NOOP("USB", "Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Square},
			{"Orange", TRANSLATE_NOOP("USB", "Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::Cross},
			{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON8, GenericInputBinding::Select},
			{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON9, GenericInputBinding::Start},
		};

		return bindings;
	}

	std::span<const SettingInfo> RBDrumKitDevice::Settings(u32 subtype) const
	{
		return {};
	}

	// ---- Buzz ----

	const char* BuzzDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Buzz Controller");
	}

	const char* BuzzDevice::TypeName() const
	{
		return "BuzzDevice";
	}

	std::span<const char*> BuzzDevice::SubTypes() const
	{
		return {};
	}

	std::span<const InputBindingInfo> BuzzDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"Red1", TRANSLATE_NOOP("USB", "Player 1 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::Circle},
			{"Blue1", TRANSLATE_NOOP("USB", "Player 1 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::R1},
			{"Orange1", TRANSLATE_NOOP("USB", "Player 1 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Cross},
			{"Green1", TRANSLATE_NOOP("USB", "Player 1 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Triangle},
			{"Yellow1", TRANSLATE_NOOP("USB", "Player 1 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::Square},

			{"Red2", TRANSLATE_NOOP("USB", "Player 2 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON5, GenericInputBinding::Unknown},
			{"Blue2", TRANSLATE_NOOP("USB", "Player 2 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON9, GenericInputBinding::Unknown},
			{"Orange2", TRANSLATE_NOOP("USB", "Player 2 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON8, GenericInputBinding::Unknown},
			{"Green2", TRANSLATE_NOOP("USB", "Player 2 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON7, GenericInputBinding::Unknown},
			{"Yellow2", TRANSLATE_NOOP("USB", "Player 2 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON6, GenericInputBinding::Unknown},

			{"Red3", TRANSLATE_NOOP("USB", "Player 3 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON10, GenericInputBinding::Unknown},
			{"Blue3", TRANSLATE_NOOP("USB", "Player 3 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON14, GenericInputBinding::Unknown},
			{"Orange3", TRANSLATE_NOOP("USB", "Player 3 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON13, GenericInputBinding::Unknown},
			{"Green3", TRANSLATE_NOOP("USB", "Player 3 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON12, GenericInputBinding::Unknown},
			{"Yellow3", TRANSLATE_NOOP("USB", "Player 3 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON11, GenericInputBinding::Unknown},

			{"Red4", TRANSLATE_NOOP("USB", "Player 4 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON15, GenericInputBinding::Unknown},
			{"Blue4", TRANSLATE_NOOP("USB", "Player 4 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON19, GenericInputBinding::Unknown},
			{"Orange4", TRANSLATE_NOOP("USB", "Player 4 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON18, GenericInputBinding::Unknown},
			{"Green4", TRANSLATE_NOOP("USB", "Player 4 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON17, GenericInputBinding::Unknown},
			{"Yellow4", TRANSLATE_NOOP("USB", "Player 4 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON16, GenericInputBinding::Unknown},
		};

		return bindings;
	}

	std::span<const SettingInfo> BuzzDevice::Settings(u32 subtype) const
	{
		return {};
	}

	USBDevice* BuzzDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		PadState* s = new PadState(port, WT_BUZZ_CONTROLLER);

		s->desc.full = &s->desc_dev;
		s->desc.str = buzz_desc_strings;

		if (usb_desc_parse_dev(buzz_dev_descriptor, sizeof(buzz_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(buzz_config_descriptor, sizeof(buzz_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s);

		return &s->dev;

	fail:
		pad_handle_destroy(&s->dev);
		return nullptr;
	}

	// ---- Keyboardmania ----

	const char* KeyboardmaniaDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "KeyboardMania");
	}

	const char* KeyboardmaniaDevice::TypeName() const
	{
		return "Keyboardmania";
	}

	std::span<const char*> KeyboardmaniaDevice::SubTypes() const
	{
		return {};
	}

	std::span<const InputBindingInfo> KeyboardmaniaDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"C1", TRANSLATE_NOOP("USB", "C 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON0, GenericInputBinding::Unknown},
			{"CSharp1", TRANSLATE_NOOP("USB", "C# 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON1, GenericInputBinding::Unknown},
			{"D1", TRANSLATE_NOOP("USB", "D 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON2, GenericInputBinding::Unknown},
			{"DSharp1", TRANSLATE_NOOP("USB", "D# 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON3, GenericInputBinding::Unknown},
			{"E1", TRANSLATE_NOOP("USB", "E 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON4, GenericInputBinding::Unknown},
			{"F1", TRANSLATE_NOOP("USB", "F 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON5, GenericInputBinding::Unknown},
			{"FSharp1", TRANSLATE_NOOP("USB", "F# 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON6, GenericInputBinding::Unknown},
			{"G1", TRANSLATE_NOOP("USB", "G 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON8, GenericInputBinding::Unknown},
			{"GSharp1", TRANSLATE_NOOP("USB", "G# 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON9, GenericInputBinding::Unknown},
			{"A1", TRANSLATE_NOOP("USB", "A 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON10, GenericInputBinding::Unknown},
			{"ASharp1", TRANSLATE_NOOP("USB", "A# 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON11, GenericInputBinding::Unknown},
			{"B1", TRANSLATE_NOOP("USB", "B 1"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON12, GenericInputBinding::Unknown},
			{"C2", TRANSLATE_NOOP("USB", "C 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON13, GenericInputBinding::Unknown},
			{"CSharp2", TRANSLATE_NOOP("USB", "C# 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON16, GenericInputBinding::Unknown},
			{"D2", TRANSLATE_NOOP("USB", "D 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON17, GenericInputBinding::Unknown},
			{"DSharp2", TRANSLATE_NOOP("USB", "D# 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON18, GenericInputBinding::Unknown},
			{"E2", TRANSLATE_NOOP("USB", "E 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON19, GenericInputBinding::Unknown},
			{"F2", TRANSLATE_NOOP("USB", "F 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON20, GenericInputBinding::Unknown},
			{"FSharp2", TRANSLATE_NOOP("USB", "F# 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON21, GenericInputBinding::Unknown},
			{"G2", TRANSLATE_NOOP("USB", "G 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON24, GenericInputBinding::Unknown},
			{"GSharp2", TRANSLATE_NOOP("USB", "G# 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON25, GenericInputBinding::Unknown},
			{"A2", TRANSLATE_NOOP("USB", "A 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON26, GenericInputBinding::Unknown},
			{"ASharp2", TRANSLATE_NOOP("USB", "A# 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON27, GenericInputBinding::Unknown},
			{"B2", TRANSLATE_NOOP("USB", "B 2"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON28, GenericInputBinding::Unknown},

			{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON22, GenericInputBinding::Unknown},
			{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON14, GenericInputBinding::Unknown},
			{"WheelUp", TRANSLATE_NOOP("USB", "Wheel Up"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON29, GenericInputBinding::Unknown},
			{"WheelDown", TRANSLATE_NOOP("USB", "Wheel Down"), nullptr, InputBindingInfo::Type::Button, CID_BUTTON30, GenericInputBinding::Unknown},
		};

		return bindings;
	}

	std::span<const SettingInfo> KeyboardmaniaDevice::Settings(u32 subtype) const
	{
		return {};
	}

	USBDevice* KeyboardmaniaDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		PadState* s = new PadState(port, WT_KEYBOARDMANIA_CONTROLLER);

		s->desc.full = &s->desc_dev;
		s->desc.str = kbm_desc_strings;

		if (usb_desc_parse_dev(kbm_dev_descriptor, sizeof(kbm_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(kbm_config_descriptor, sizeof(kbm_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s);

		return &s->dev;

	fail:
		pad_handle_destroy(&s->dev);
		return nullptr;
	}
} // namespace usb_pad
