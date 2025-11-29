// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "usb-flightstick.h"

#include "common/Console.h"
#include "Host.h"
#include "IconsFontAwesome6.h"
#include "IconsPromptFont.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/deviceproxy.h"
#include "USB/USB.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"

namespace usb_pad
{
	const char* FlightStickDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Flight Stick Controller");
	}

	const char* FlightStickDevice::TypeName() const
	{
		return "FlightStickController";
	}

	const char* FlightStickDevice::IconName() const
	{
		return ICON_FA_GAMEPAD;
	}

	std::span<const char*> FlightStickDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "HP2-13 (FS1)"),
			TRANSLATE_NOOP("USB", "HP2-217 (FS2)"),
		};
		return subtypes;
	}

	enum FlightStickControlID
	{
		// analog data
		CID_FS_STICK_L,
		CID_FS_STICK_R,
		CID_FS_STICK_U,
		CID_FS_STICK_D,
		CID_FS_RUDDER_L,
		CID_FS_RUDDER_R,
		CID_FS_THROTTLE_U,
		CID_FS_THROTTLE_D,
		CID_FS_ANALOG_HAT_L,
		CID_FS_ANALOG_HAT_R,
		CID_FS_ANALOG_HAT_U,
		CID_FS_ANALOG_HAT_D,
		CID_FS_TRIANGLE_A,
		CID_FS_SQUARE_B,

		// digital data
		CID_FS_DPAD_1_L,
		CID_FS_DPAD_1_R,
		CID_FS_DPAD_1_U,
		CID_FS_DPAD_1_D,
		CID_FS_CROSS_TRIGGER,
		CID_FS_CIRCLE_LAUNCH,
		CID_FS_SELECT_C,
		CID_FS_START,
		CID_FS_ANALOG_HAT_CLICK,

		// extra digital for FS2
		CID_FS_D,
		CID_FS_SW1,
		CID_FS_DPAD_2_L,
		CID_FS_DPAD_2_R,
		CID_FS_DPAD_2_D,
		CID_FS_DPAD_2_U,
		CID_FS_DPAD_3_L,
		CID_FS_DPAD_3_M,
		CID_FS_DPAD_3_R,

		BUTTONS_OFFSET = CID_FS_DPAD_1_L,
	};

	std::span<const InputBindingInfo> FlightStickDevice::Bindings(u32 subtype) const
	{
//using macros for shared data
#define BINDINGS_FLIGHTSTICK_SHARED_ANALOG \
		{"StickLeft", TRANSLATE_NOOP("USB", "Stick Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_STICK_L, GenericInputBinding::LeftStickLeft}, \
		{"StickRight", TRANSLATE_NOOP("USB", "Stick Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_STICK_R, GenericInputBinding::LeftStickRight}, \
		{"StickUp", TRANSLATE_NOOP("USB", "Stick Up"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_STICK_U, GenericInputBinding::LeftStickUp}, \
		{"StickDown", TRANSLATE_NOOP("USB", "Stick Down"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_STICK_D, GenericInputBinding::LeftStickDown}, \
		{"RudderLeft", TRANSLATE_NOOP("USB", "Rudder Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_RUDDER_L, GenericInputBinding::L1}, \
		{"RudderRight", TRANSLATE_NOOP("USB", "Rudder Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_RUDDER_R, GenericInputBinding::R1}, \
		{"ThrottleUp", TRANSLATE_NOOP("USB", "Throttle Up"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_THROTTLE_U, GenericInputBinding::R2}, \
		{"ThrottleDown", TRANSLATE_NOOP("USB", "Throttle Down"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_THROTTLE_D, GenericInputBinding::L2}, \
		{"HatLeft", TRANSLATE_NOOP("USB", "Stick Hat Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_ANALOG_HAT_L, GenericInputBinding::RightStickLeft}, \
		{"HatRight", TRANSLATE_NOOP("USB", "Stick Hat Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_ANALOG_HAT_R, GenericInputBinding::RightStickRight}, \
		{"HatkUp", TRANSLATE_NOOP("USB", "Stick Hat Up"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_ANALOG_HAT_U, GenericInputBinding::RightStickUp}, \
		{"HatDown", TRANSLATE_NOOP("USB", "Stick Hat Down"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_ANALOG_HAT_D, GenericInputBinding::RightStickDown}, \
		{"TriangleA", TRANSLATE_NOOP("USB", "Triangle (A)"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_TRIANGLE_A, GenericInputBinding::Triangle}, \
		{"SquareB", TRANSLATE_NOOP("USB", "Square (B)"), nullptr, InputBindingInfo::Type::HalfAxis, CID_FS_SQUARE_B, GenericInputBinding::Square}, \

#define BINDINGS_FLIGHTSTICK_SHARED_DPAD \
		{"Dpad1Left", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_1_L, GenericInputBinding::DPadLeft}, \
		{"Dpad1Right", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_1_R, GenericInputBinding::DPadRight}, \
		{"Dpad1Up", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_1_U, GenericInputBinding::DPadUp}, \
		{"Dpad1Down", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_1_D, GenericInputBinding::DPadDown}, \

#define BINDINGS_FLIGHTSTICK_SHARED_BUTTONS \
		{"CrossTrigger", TRANSLATE_NOOP("USB", "Cross (Trigger)"), nullptr, InputBindingInfo::Type::Button, CID_FS_CROSS_TRIGGER, GenericInputBinding::Cross}, \
		{"CircleLaunch", TRANSLATE_NOOP("USB", "Circle (Launch)"), nullptr, InputBindingInfo::Type::Button, CID_FS_CIRCLE_LAUNCH, GenericInputBinding::Circle}, \
		{"Select", TRANSLATE_NOOP("USB", "Select (Fire C)"), nullptr, InputBindingInfo::Type::Button, CID_FS_SELECT_C, GenericInputBinding::Select}, \
		{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_FS_START, GenericInputBinding::Start}, \
		{"HatClick", TRANSLATE_NOOP("USB", "Hat Click"), nullptr, InputBindingInfo::Type::Button, CID_FS_ANALOG_HAT_CLICK, GenericInputBinding::R3}, \

		switch (subtype)
		{
			case FLIGHTSTICK_FS1:
			{
				static constexpr const InputBindingInfo bindings_fs1[] = {
					BINDINGS_FLIGHTSTICK_SHARED_ANALOG
					BINDINGS_FLIGHTSTICK_SHARED_DPAD
					BINDINGS_FLIGHTSTICK_SHARED_BUTTONS
				};
				return bindings_fs1;
			}
			case FLIGHTSTICK_FS2:
			{
				static constexpr const InputBindingInfo bindings_fs2[] = {
					BINDINGS_FLIGHTSTICK_SHARED_ANALOG
					BINDINGS_FLIGHTSTICK_SHARED_DPAD
					{"Dpad2Left", TRANSLATE_NOOP("USB", "D-Pad 2 Left"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_2_L, GenericInputBinding::Unknown},
					{"Dpad2Right", TRANSLATE_NOOP("USB", "D-Pad 2 Right"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_2_R, GenericInputBinding::Unknown},
					{"Dpad2Up", TRANSLATE_NOOP("USB", "D-Pad 2 Up"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_2_U, GenericInputBinding::Unknown},
					{"Dpad2Down", TRANSLATE_NOOP("USB", "D-Pad 2 Down"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_2_D, GenericInputBinding::Unknown},
					BINDINGS_FLIGHTSTICK_SHARED_BUTTONS
					{"D", TRANSLATE_NOOP("USB", "D"), nullptr, InputBindingInfo::Type::Button, CID_FS_D, GenericInputBinding::L3},
					{"SW1", TRANSLATE_NOOP("USB", "SW1 (Pinky Trigger)"), nullptr, InputBindingInfo::Type::Button, CID_FS_D, GenericInputBinding::Unknown},
					{"Dpad3Left", TRANSLATE_NOOP("USB", "D-Pad 3 Left"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_3_L, GenericInputBinding::Unknown},
					{"Dpad3Middle", TRANSLATE_NOOP("USB", "D-Pad 3 Middle"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_3_M, GenericInputBinding::Unknown},
					{"Dpad3Right", TRANSLATE_NOOP("USB", "D-Pad 3 Right"), nullptr, InputBindingInfo::Type::Button, CID_FS_DPAD_3_R, GenericInputBinding::Unknown},
					{"Motor", TRANSLATE_NOOP("Pad", "Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::LargeMotor},
				};
				return bindings_fs2;
			}
			default:
				break;
		}
		return {};
//remove the macros
#undef BINDINGS_FLIGHTSTICK_SHARED_ANALOG
#undef BINDINGS_FLIGHTSTICK_SHARED_DPAD
#undef BINDINGS_FLIGHTSTICK_SHARED_BUTTONS
	}

	static constexpr u32 button_mask(u32 bind_index)
	{
		return (1u << (bind_index - FlightStickControlID::BUTTONS_OFFSET));
	}

	static constexpr u32 button_at(u32 value, u32 index)
	{
		return value & button_mask(index);
	}

	static void flightstick_handle_reset(USBDevice* dev)
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);
		s->Reset();
	}

	static void flightstick_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		const FlightStickDeviceState* s = USB_CONTAINER_OF(dev, const FlightStickDeviceState, dev);

		int ret = 0;
		switch (request)
		{
			case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
			{
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret < 0)
					goto fail;
				break;
			}
			case VendorDeviceRequest | 0x00:
			{
				FlightStickConData_VR00 vendordata_00{};
				ret = sizeof(vendordata_00);
				std::memset(&vendordata_00, 0xff, ret);

				vendordata_00.fire_c = button_at(s->data.buttons, CID_FS_SELECT_C) ? 0 : 1;
				//vendordata_00.button_d = 0x1;
				vendordata_00.hat_btn = button_at(s->data.buttons, CID_FS_ANALOG_HAT_CLICK) ? 0 : 1;
				vendordata_00.button_st = button_at(s->data.buttons, CID_FS_START) ? 0 : 1;
				vendordata_00.hat1_u = button_at(s->data.buttons, CID_FS_DPAD_1_U) ? 0 : 1;
				vendordata_00.hat1_r = button_at(s->data.buttons, CID_FS_DPAD_1_R) ? 0 : 1;
				vendordata_00.hat1_d = button_at(s->data.buttons, CID_FS_DPAD_1_D) ? 0 : 1;
				vendordata_00.hat1_l = button_at(s->data.buttons, CID_FS_DPAD_1_L) ? 0 : 1;
				//vendordata_00.reserved1 = 0xf;
				//vendordata_00.reserved2 : 0x1;
				vendordata_00.launch = button_at(s->data.buttons, CID_FS_CIRCLE_LAUNCH) ? 0 : 1;
				vendordata_00.trigger = button_at(s->data.buttons, CID_FS_CROSS_TRIGGER) ? 0 : 1;
				//vendordata_00.reserved3 = 0x1;

				if (s->type == FLIGHTSTICK_FS2)
				{
					vendordata_00.button_d = button_at(s->data.buttons, CID_FS_D) ? 0 : 1;
				}

				std::memcpy(data, &vendordata_00, ret);
				p->actual_length = ret;
				break;
			}
			case VendorDeviceRequest | 0x01:
			{
				FlightStickConData_VR01 vendordata_01{};
				ret = sizeof(vendordata_01);
				std::memset(&vendordata_01, 0xff, ret);

				if (s->type == FLIGHTSTICK_FS2)
				{
					//vendordata_01.reserved4 = 0xf;
					vendordata_01.hat3_r = button_at(s->data.buttons, CID_FS_DPAD_3_R) ? 0 : 1;
					vendordata_01.hat3_m = button_at(s->data.buttons, CID_FS_DPAD_3_M) ? 0 : 1;
					vendordata_01.hat3_l = button_at(s->data.buttons, CID_FS_DPAD_3_L) ? 0 : 1;
					vendordata_01.reserved5 = 0x0;
					vendordata_01.mode_select = s->mode; // stored on settings page
					//vendordata_01.reserved6 = 0x1;
					vendordata_01.button_sw1 = button_at(s->data.buttons, CID_FS_SW1) ? 0 : 1;
					vendordata_01.hat2_u = button_at(s->data.buttons, CID_FS_DPAD_2_U) ? 0 : 1;
					vendordata_01.hat2_r = button_at(s->data.buttons, CID_FS_DPAD_2_R) ? 0 : 1;
					vendordata_01.hat2_d = button_at(s->data.buttons, CID_FS_DPAD_2_D) ? 0 : 1;
					vendordata_01.hat2_l = button_at(s->data.buttons, CID_FS_DPAD_2_L) ? 0 : 1;
				}

				std::memcpy(data, &vendordata_01, ret);
				p->actual_length = ret;
				break;
			}
			case VendorDeviceOutRequest | 0x0C:
			{
				//rumble (only possible on FS2)
				if (index == 0 && length == 1 && s->type == FLIGHTSTICK_FS2)
				{
					InputManager::SetUSBVibrationIntensity(s->port, std::min(static_cast<float>(data[0]) * (1.0f / 255.0f), 1.0f), 0);
				}
				ret = length;
				p->actual_length = length;
				break;
			}
			default:
			{
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret >= 0)
				{
					return;
				}
			}
			fail:

				p->status = USB_RET_STALL;
				break;
		}

		//if (usb_desc_handle_control(dev, p, request, value, index, length, data) < 0)
		//	p->status = USB_RET_STALL;
	}

	static void flightstick_handle_destroy(USBDevice* dev) noexcept
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);
		delete s;
	}

	bool FlightStickDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);

		if (!sw.DoMarker("FlightStickController"))
			return false;

		sw.Do(&s->data.stick_left);
		sw.Do(&s->data.stick_right);
		sw.Do(&s->data.stick_up);
		sw.Do(&s->data.stick_down);
		sw.Do(&s->data.rudder_left);
		sw.Do(&s->data.rudder_right);
		sw.Do(&s->data.throttle_up);
		sw.Do(&s->data.throttle_down);
		sw.Do(&s->data.stick_hat_left);
		sw.Do(&s->data.stick_hat_right);
		sw.Do(&s->data.stick_hat_up);
		sw.Do(&s->data.stick_hat_down);

		sw.Do(&s->data.stick_x);
		sw.Do(&s->data.stick_y);
		sw.Do(&s->data.rudder);
		sw.Do(&s->data.throttle);
		sw.Do(&s->data.hatstick_x);
		sw.Do(&s->data.hatstick_y);
		sw.Do(&s->data.button_a);
		sw.Do(&s->data.button_b);

		sw.DoBytes(&s->data.buttons, sizeof(u32));
		return true;
	}

	void FlightStickDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);
		s->mode = USB::GetConfigInt(si, s->port, TypeName(), "Mode", 3);
		if (s->type == FLIGHTSTICK_FS2)
		{
			Host::AddKeyedOSDMessage("Pad", fmt::format("FlightStick Mode: {}", s->mode), Host::OSD_QUICK_DURATION);
		}
	}

	std::span<const SettingInfo> FlightStickDevice::Settings(u32 subtype) const
	{
		static const char* s_mode_options[] = {
			TRANSLATE_NOOP("Pad", "1"),
			TRANSLATE_NOOP("Pad", "2"),
			TRANSLATE_NOOP("Pad", "3"),
			nullptr};

		static constexpr const SettingInfo mode = {
			SettingInfo::Type::IntegerList, // type
			"Mode", // name
			TRANSLATE_NOOP("Pad", "Mode switch"), // display name
			TRANSLATE_NOOP("Pad", "Set the stick mode switch position"), // description
			"3", // default value
			"1", // min value
			"3", // max value
			nullptr, // step value
			nullptr, // format
			s_mode_options, // options for integer lists
			nullptr, // options for string lists
			0.0f // multiplier
		};

		static constexpr const SettingInfo info[] = {mode};

		if (subtype == FLIGHTSTICK_FS2)
			return info;	
		else
			return {};
	}

	float FlightStickDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		const FlightStickDeviceState* s = USB_CONTAINER_OF(dev, const FlightStickDeviceState, dev);

		switch (bind_index)
		{
			case CID_FS_STICK_L:
				return (static_cast<float>(s->data.stick_left) / 255.0f);
			case CID_FS_STICK_R:
				return (static_cast<float>(s->data.stick_right) / 255.0f);
			case CID_FS_STICK_U:
				return (static_cast<float>(s->data.stick_up) / 255.0f);
			case CID_FS_STICK_D:
				return (static_cast<float>(s->data.stick_down) / 255.0f);

			case CID_FS_RUDDER_L:
				return (static_cast<float>(s->data.rudder_left) / 255.0f);
			case CID_FS_RUDDER_R:
				return (static_cast<float>(s->data.rudder_right) / 255.0f);

			case CID_FS_THROTTLE_U:
				return (static_cast<float>(s->data.throttle_up) / 255.0f);
			case CID_FS_THROTTLE_D:
				return (static_cast<float>(s->data.throttle_down) / 255.0f);


			case CID_FS_ANALOG_HAT_L:
				return (static_cast<float>(s->data.stick_hat_left) / 255.0f);
			case CID_FS_ANALOG_HAT_R:
				return (static_cast<float>(s->data.stick_hat_right) / 255.0f);
			case CID_FS_ANALOG_HAT_U:
				return (static_cast<float>(s->data.stick_hat_up) / 255.0f);
			case CID_FS_ANALOG_HAT_D:
				return (static_cast<float>(s->data.stick_hat_down) / 255.0f);

			case CID_FS_TRIANGLE_A:
				return (static_cast<float>(s->data.button_a) / 255.0f);
			case CID_FS_SQUARE_B:
				return (static_cast<float>(s->data.button_b) / 255.0f);

			case CID_FS_DPAD_1_L:
			case CID_FS_DPAD_1_R:
			case CID_FS_DPAD_1_U:
			case CID_FS_DPAD_1_D:
			case CID_FS_CROSS_TRIGGER:
			case CID_FS_CIRCLE_LAUNCH:
			case CID_FS_SELECT_C:
			case CID_FS_START:
			case CID_FS_ANALOG_HAT_CLICK:
			case CID_FS_D:
			case CID_FS_SW1:
			case CID_FS_DPAD_2_L:
			case CID_FS_DPAD_2_R:
			case CID_FS_DPAD_2_D:
			case CID_FS_DPAD_2_U:
			case CID_FS_DPAD_3_R:
			case CID_FS_DPAD_3_M:
			case CID_FS_DPAD_3_L:
			{
				return (button_at(s->data.buttons, bind_index) != 0u) ? 1.0f : 0.0f;
			}

			default:
				return 0.0f;
		}
	}

	void FlightStickDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);

		switch (bind_index)
		{
			case CID_FS_STICK_L:
				s->data.stick_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case CID_FS_STICK_R:
				s->data.stick_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;

			case CID_FS_STICK_U:
				s->data.stick_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case CID_FS_STICK_D:
				s->data.stick_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;

			case CID_FS_RUDDER_L:
				s->data.rudder_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateRudder();
				break;
			case CID_FS_RUDDER_R:
				s->data.rudder_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateRudder();
				break;

			case CID_FS_THROTTLE_U:
				s->data.throttle_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateThrottle();
				break;
			case CID_FS_THROTTLE_D:
				s->data.throttle_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateThrottle();
				break;

			case CID_FS_ANALOG_HAT_L:
				s->data.stick_hat_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStickHat();
				break;
			case CID_FS_ANALOG_HAT_R:
				s->data.stick_hat_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStickHat();
				break;

			case CID_FS_ANALOG_HAT_U:
				s->data.stick_hat_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStickHat();
				break;
			case CID_FS_ANALOG_HAT_D:
				s->data.stick_hat_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStickHat();
				break;

			case CID_FS_TRIANGLE_A:
				s->data.button_a = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_FS_SQUARE_B:
				s->data.button_b = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_FS_DPAD_1_L:
			case CID_FS_DPAD_1_R:
			case CID_FS_DPAD_1_U:
			case CID_FS_DPAD_1_D:
			case CID_FS_CROSS_TRIGGER:
			case CID_FS_CIRCLE_LAUNCH:
			case CID_FS_SELECT_C:
			case CID_FS_START:
			case CID_FS_ANALOG_HAT_CLICK:
			case CID_FS_D:
			case CID_FS_SW1:
			case CID_FS_DPAD_2_L:
			case CID_FS_DPAD_2_R:
			case CID_FS_DPAD_2_D:
			case CID_FS_DPAD_2_U:
			case CID_FS_DPAD_3_R:
			case CID_FS_DPAD_3_M:
			case CID_FS_DPAD_3_L:
			{
				const u32 mask = button_mask(bind_index);
				if (value >= 0.5f)
					s->data.buttons |= mask;
				else
					s->data.buttons &= ~mask;
			}
			break;

			default:
				break;
		}
	}

	FlightStickDeviceState::FlightStickDeviceState(u32 port_, FlightStickDeviceTypes type_)
		: port(port_)
		, type(type_)
	{
		Reset();
	}

	FlightStickDeviceState::~FlightStickDeviceState() = default;

	void FlightStickDeviceState::Reset()
	{
		data.stick_left = 0;
		data.stick_right = 0;
		data.stick_up = 0;
		data.stick_down = 0;
		data.rudder_left = 0;
		data.rudder_right = 0;
		data.throttle_up = 0;
		data.throttle_down = 0;
		data.stick_hat_left = 0;
		data.stick_hat_right = 0;
		data.stick_hat_up = 0;
		data.stick_hat_down = 0;

		data.stick_x = analog_center;
		data.stick_y = analog_center;
		data.rudder = analog_center;
		data.throttle = analog_center;
		data.hatstick_x = analog_center;
		data.hatstick_y = analog_center;
		data.button_a = 0x00;
		data.button_b = 0x00;
		data.buttons = 0;
	}

	void FlightStickDeviceState::UpdateStick() noexcept
	{
		if (data.stick_left > 0)
			data.stick_x = static_cast<u8>(std::max<int>(analog_range - data.stick_left, 0));
		else if (data.stick_right > 0)
			data.stick_x = static_cast<u8>(std::min<int>(analog_range + data.stick_right, analog_range * 2));
		else
			data.stick_x = 0x80;

		if (data.stick_up > 0)
			data.stick_y = static_cast<u8>(std::max<int>(analog_range - data.stick_up, 0));
		else if (data.stick_down > 0)
			data.stick_y = static_cast<u8>(std::min<int>(analog_range + data.stick_down, analog_range * 2));
		else
			data.stick_y = 0x80;
	}
	void FlightStickDeviceState::UpdateRudder() noexcept
	{
		if (data.rudder_left > 0)
			data.rudder = static_cast<u8>(std::max<int>(analog_range - data.rudder_left, 0));
		else if (data.rudder_right > 0)
			data.rudder = static_cast<u8>(std::min<int>(analog_range + data.rudder_right, analog_range * 2));
		else
			data.rudder = 0x80;
	}
	void FlightStickDeviceState::UpdateThrottle() noexcept
	{
		if (data.throttle_up > 0)
			data.throttle = static_cast<u8>(std::min<int>(analog_range + data.throttle_up, analog_range * 2));
		else if (data.throttle_down > 0)
			data.throttle = static_cast<u8>(std::max<int>(analog_range - data.throttle_down, 0));
		else
			data.throttle = 0x80;
	}

	void FlightStickDeviceState::UpdateStickHat() noexcept
	{
		if (data.stick_hat_left > 0)
			data.hatstick_x = static_cast<u8>(std::max<int>(analog_range - data.stick_hat_left, 0));
		else if (data.stick_hat_right > 0)
			data.hatstick_x = static_cast<u8>(std::min<int>(analog_range + data.stick_hat_right, analog_range * 2));
		else
			data.hatstick_x = 0x80;

		if (data.stick_hat_up > 0)
			data.hatstick_y = static_cast<u8>(std::max<int>(analog_range - data.stick_hat_up, 0));
		else if (data.stick_hat_down > 0)
			data.hatstick_y = static_cast<u8>(std::min<int>(analog_range + data.stick_hat_down, analog_range * 2));
		else
			data.hatstick_y = 0x80;
	}

	static void flightstick_handle_data(USBDevice* dev, USBPacket* p)
	{
		FlightStickDeviceState* s = USB_CONTAINER_OF(dev, FlightStickDeviceState, dev);

		if (p->pid != USB_TOKEN_IN || p->ep->nr != 1)
		{
			Console.Error("Unhandled FlightStickController request pid=%d ep=%u", p->pid, p->ep->nr);
			p->status = USB_RET_STALL;
			return;
		}

		switch (s->type)
		{
			case FLIGHTSTICK_FS1:
			case FLIGHTSTICK_FS2:
			{
				//interrupt input data
				FlightStickConData out = {};

				out.stick_x = s->data.stick_x;
				out.stick_y = s->data.stick_y;
				out.rudder = s->data.rudder;
				out.throttle = s->data.throttle;
				out.hat_x = s->data.hatstick_x;
				out.hat_y = s->data.hatstick_y;
				out.button_a = static_cast<u8>(~(s->data.button_a));
				out.button_b = static_cast<u8>(~(s->data.button_b));
				
				usb_packet_copy(p, &out, sizeof(out));
				break;
			}
			default:
				Console.Error("Unhandled FlightStickController USB_TOKEN_IN pid=%d ep=%u type=%u", p->pid, p->ep->nr, s->type);
				p->status = USB_RET_IOERROR;
				return;
		}
	}

	USBDevice* FlightStickDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		FlightStickDeviceState* s = new FlightStickDeviceState(port, static_cast<FlightStickDeviceTypes>(subtype));

		s->desc.full = &s->desc_dev;

		switch (subtype)
		{
			case FLIGHTSTICK_FS1:
				s->desc.str = fst01_desc_strings;
				if (usb_desc_parse_dev(fst01_dev_descriptor, sizeof(fst01_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				break;
			case FLIGHTSTICK_FS2:
				s->desc.str = fst02_desc_strings;
				if (usb_desc_parse_dev(fst02_dev_descriptor, sizeof(fst02_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				break;

			default:
				goto fail;
		}

		if (usb_desc_parse_config(flightstick_config_descriptor, sizeof(flightstick_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = flightstick_handle_reset;
		s->dev.klass.handle_control = flightstick_handle_control;
		s->dev.klass.handle_data = flightstick_handle_data;
		s->dev.klass.unrealize = flightstick_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		flightstick_handle_reset(&s->dev);
		UpdateSettings(&s->dev, si);

		return &s->dev;

	fail:
		flightstick_handle_destroy(&s->dev);
		return nullptr;
	}
} // namespace usb_pad
