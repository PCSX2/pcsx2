// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host.h"
#include "IconsFontAwesome6.h"
#include "IconsPromptFont.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-pad/usb-turntable.h"
#include "USB/usb-pad/usb-pad.h"

namespace usb_pad
{
	static const USBDescStrings desc_strings = {
		"",
		"RedOctane DJ",
		"Guitar Hero5 for PlayStation (R) 3"};

	// Should be usb 2.0, but seems to make no difference with DJ Hero games
	static const uint8_t dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0110), //USB 1.1
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x40, //(64)
		/* idVendor            */ WBVAL(0x12ba),
		/* idProduct           */ WBVAL(0x0140),
		/* bcdDevice           */ WBVAL(0x1000), //(10.00)
		/* iManufacturer       */ 0x01, //(1)
		/* iProduct            */ 0x02, //(2)
		/* iSerialNumber       */ 0x00, //(0)
		/* bNumConfigurations  */ 0x01, //(1)
	};

	TurntableState::TurntableState(u32 port_)
		: port(port_)
	{

		turntable_multiplier = 1;
	}

	TurntableState::~TurntableState() = default;

	int TurntableState::TokenIn(u8* buf, int len)
	{	
		std::memset(buf, 0, 8);

		UpdateHatSwitch();
		
		u32* buttons = reinterpret_cast<u32*>(buf);
		*buttons = (data.buttons & ((1 << (CID_DJ_START + 1)) - 1));
		*buttons |= (data.hatswitch & 0xF) << 16;

		// Map platter buttons to standard buttons for menus
		if (data.buttons & (1 << CID_DJ_LEFT_GREEN) || data.buttons & (1 << CID_DJ_RIGHT_GREEN))
		{
			*buttons |= (1 << CID_DJ_CROSS);
		}
		if (data.buttons & (1 << CID_DJ_LEFT_RED) || data.buttons & (1 << CID_DJ_RIGHT_RED))
		{
			*buttons |= (1 << CID_DJ_CIRCLE);
		}
		if (data.buttons & (1 << CID_DJ_LEFT_BLUE) || data.buttons & (1 << CID_DJ_RIGHT_BLUE))
		{
			*buttons |= (1 << CID_DJ_SQUARE);
		}
		// Crossfader and Effects Knob are put into "accelerometer" bits in the PS3 reports
		// So they need to be scaled to 1024
		u16 crossfader = 0x0200;
		u16 effectsknob = 0x0200;
		if (data.crossfader_left > 0)
		{
			crossfader -= data.crossfader_left;
		}
		else
		{
			crossfader += data.crossfader_right;
		}
		if (data.effectsknob_left > 0)
		{
			effectsknob -= data.effectsknob_left;
		}
		else
		{
			effectsknob += data.effectsknob_right;
		}

		u8 left_turntable = 0x80;
		u8 right_turntable = 0x80;
		if (data.left_turntable_ccw > 0)
		{
			left_turntable -= static_cast<u8>(std::min<int>(data.left_turntable_ccw, 0x7F));
		}
		else
		{
			left_turntable += static_cast<u8>(std::min<int>(data.left_turntable_cw, 0x7F));
		}
		if (data.right_turntable_ccw > 0)
		{
			right_turntable -= static_cast<u8>(std::min<int>(data.right_turntable_ccw, 0x7F));
		}
		else
		{
			right_turntable += static_cast<u8>(std::min<int>(data.right_turntable_cw, 0x7F));
		}
		buf[3] = 0x80;
		buf[4] = 0x80;
		buf[5] = left_turntable;
		buf[6] = right_turntable;

		buf[19] = effectsknob & 0xFF;
		buf[20] = effectsknob >> 8;
		buf[21] = crossfader & 0xFF;
		buf[22] = crossfader >> 8;

		// Platter buttons
		buf[23] = (data.buttons >> CID_DJ_RIGHT_GREEN) & 0xFF;
		buf[24] = 0x02;
		buf[26] = 0x02;

		return len;
	}

	void TurntableState::UpdateSettings(SettingsInterface& si, const char* devname)
	{
		turntable_multiplier = USB::GetConfigFloat(si, port, devname, "TurntableMultiplier", 1);
	}

	void TurntableState::UpdateHatSwitch()
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

	void TurntableState::SetEuphoriaLedState(bool state)
	{
		data.euphoria_led_state = state;
	}

	static void turntable_handle_control(USBDevice* dev, USBPacket* p,
		int request, int value, int index, int length, uint8_t* data)
	{
		TurntableState* s = USB_CONTAINER_OF(dev, TurntableState, dev);
		int ret;

		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			case SET_REPORT:
				if (data[0] == 0x91) {
					s->SetEuphoriaLedState(data[2]);
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
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void turntable_handle_data(USBDevice* dev, USBPacket* p)
	{
		TurntableState* s = USB_CONTAINER_OF(dev, TurntableState, dev);

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
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void turntable_unrealize(USBDevice* dev)
	{
		TurntableState* s = USB_CONTAINER_OF(dev, TurntableState, dev);
		delete s;
	}

	const char* DJTurntableDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "DJ Hero Turntable");
	}

	const char* DJTurntableDevice::TypeName() const
	{
		return "DJTurntable";
	}

	const char* DJTurntableDevice::IconName() const
	{
		return ICON_PF_DJ_HERO_TURNTABLE;
	}

	USBDevice* DJTurntableDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		TurntableState* s = new TurntableState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;

		// PS3 / wii instruments all share the same config descriptors and hid reports
		if (usb_desc_parse_config(rb1_config_descriptor, sizeof(rb1_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = nullptr;
		s->dev.klass.handle_control = turntable_handle_control;
		s->dev.klass.handle_data = turntable_handle_data;
		s->dev.klass.unrealize = turntable_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		return &s->dev;

	fail:
		turntable_unrealize(&s->dev);
		return nullptr;
	}

	bool DJTurntableDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		TurntableState* s = USB_CONTAINER_OF(dev, TurntableState, dev);
		
		if (!sw.DoMarker("DJTurntableDevice"))
			return false;

		sw.Do(&s->data.euphoria_led_state);
		return !sw.HasError();
	}

	void DJTurntableDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		USB_CONTAINER_OF(dev, TurntableState, dev)->UpdateSettings(si, TypeName());
	}

	float DJTurntableDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		const TurntableState* s = USB_CONTAINER_OF(dev, const TurntableState, dev);

		switch (bind_index)
		{
			case CID_DJ_CROSSFADER_LEFT:
				return static_cast<float>(s->data.crossfader_left) / 512.0f;

			case CID_DJ_CROSSFADER_RIGHT:
				return static_cast<float>(s->data.crossfader_right) / 512.0f;

			case CID_DJ_EFFECTSKNOB_RIGHT:
				return static_cast<float>(s->data.effectsknob_right) / 512.0f;

			case CID_DJ_EFFECTSKNOB_LEFT:
				return static_cast<float>(s->data.effectsknob_left) / 512.0f;

			case CID_DJ_LEFT_TURNTABLE_CW:
				return static_cast<float>(s->data.left_turntable_cw) / 128.0f;

			case CID_DJ_LEFT_TURNTABLE_CCW:
				return static_cast<float>(s->data.left_turntable_ccw) / 128.0f;

			case CID_DJ_RIGHT_TURNTABLE_CW:
				return static_cast<float>(s->data.right_turntable_cw) / 128.0f;

			case CID_DJ_RIGHT_TURNTABLE_CCW:
				return static_cast<float>(s->data.right_turntable_ccw) / 128.0f;

			case CID_DJ_DPAD_UP:
				return static_cast<float>(s->data.hat_up);

			case CID_DJ_DPAD_DOWN:
				return static_cast<float>(s->data.hat_down);

			case CID_DJ_DPAD_LEFT:
				return static_cast<float>(s->data.hat_left);

			case CID_DJ_DPAD_RIGHT:
				return static_cast<float>(s->data.hat_right);

			case CID_DJ_SQUARE:
			case CID_DJ_CROSS:
			case CID_DJ_CIRCLE:
			case CID_DJ_TRIANGLE:
			case CID_DJ_SELECT:
			case CID_DJ_START:
			case CID_DJ_RIGHT_GREEN:
			case CID_DJ_RIGHT_RED:
			case CID_DJ_RIGHT_BLUE:
			case CID_DJ_LEFT_GREEN:
			case CID_DJ_LEFT_RED:
			case CID_DJ_LEFT_BLUE:
			{
				const u32 mask = (1u << bind_index);
				return ((s->data.buttons & mask) != 0u) ? 1.0f : 0.0f;
			}
			default:
				return 0.0f;
		}
	}

	void DJTurntableDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		TurntableState* s = USB_CONTAINER_OF(dev, TurntableState, dev);
		switch (bind_index)
		{
			case CID_DJ_CROSSFADER_LEFT:
				s->data.crossfader_left = static_cast<u32>(std::clamp<long>(std::lroundf(value * 512.0f), 0, 512));
				break;

			case CID_DJ_CROSSFADER_RIGHT:
				s->data.crossfader_right = static_cast<u32>(std::clamp<long>(std::lroundf(value * 512.0f), 0, 512));
				break;

			case CID_DJ_EFFECTSKNOB_LEFT:
				s->data.effectsknob_left = static_cast<u32>(std::clamp<long>(std::lroundf(value * 512.0f), 0, 512));
				break;

			case CID_DJ_EFFECTSKNOB_RIGHT:
				s->data.effectsknob_right = static_cast<u32>(std::clamp<long>(std::lroundf(value * 512.0f), 0, 512));
				break;

			case CID_DJ_LEFT_TURNTABLE_CW:
				s->data.left_turntable_cw = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->turntable_multiplier * 128.0f), 0, 128));
				break;

			case CID_DJ_LEFT_TURNTABLE_CCW:
				s->data.left_turntable_ccw = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->turntable_multiplier * 128.0f), 0, 128));
				break;

			case CID_DJ_RIGHT_TURNTABLE_CW:
				s->data.right_turntable_cw = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->turntable_multiplier * 128.0f), 0, 128));
				break;

			case CID_DJ_RIGHT_TURNTABLE_CCW:
				s->data.right_turntable_ccw = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->turntable_multiplier * 128.0f), 0, 128));
				break;

			case CID_DJ_DPAD_UP:
				s->data.hat_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_DJ_DPAD_DOWN:
				s->data.hat_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_DJ_DPAD_LEFT:
				s->data.hat_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_DJ_DPAD_RIGHT:
				s->data.hat_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;

			case CID_DJ_SQUARE:
			case CID_DJ_CROSS:
			case CID_DJ_CIRCLE:
			case CID_DJ_TRIANGLE:
			case CID_DJ_SELECT:
			case CID_DJ_START:
			case CID_DJ_RIGHT_GREEN:
			case CID_DJ_RIGHT_RED:
			case CID_DJ_RIGHT_BLUE:
			case CID_DJ_LEFT_GREEN:
			case CID_DJ_LEFT_RED:
			case CID_DJ_LEFT_BLUE:
			{
				const u32 mask = (1u << bind_index);
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

	std::span<const InputBindingInfo> DJTurntableDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_DJ_DPAD_UP, GenericInputBinding::DPadUp},
			{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_DJ_DPAD_DOWN, GenericInputBinding::DPadDown},
			{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_DJ_DPAD_LEFT, GenericInputBinding::DPadLeft},
			{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_DJ_DPAD_RIGHT, GenericInputBinding::DPadRight},
			{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, CID_DJ_SQUARE, GenericInputBinding::Unknown},
			{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, CID_DJ_CROSS, GenericInputBinding::Unknown},
			{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, CID_DJ_CIRCLE, GenericInputBinding::Unknown},
			{"Triangle", TRANSLATE_NOOP("USB", "Triangle / Euphoria"), nullptr, InputBindingInfo::Type::Button, CID_DJ_TRIANGLE, GenericInputBinding::Triangle},
			{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_DJ_SELECT, GenericInputBinding::Select},
			{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_DJ_START, GenericInputBinding::Start},
			{"CrossFaderLeft", TRANSLATE_NOOP("USB", "Crossfader Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_CROSSFADER_LEFT, GenericInputBinding::RightStickDown},
			{"CrossFaderRight", TRANSLATE_NOOP("USB", "Crossfader Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_CROSSFADER_RIGHT, GenericInputBinding::RightStickUp},
			{"EffectsKnobLeft", TRANSLATE_NOOP("USB", "Effects Knob Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_EFFECTSKNOB_LEFT, GenericInputBinding::RightStickLeft},
			{"EffectsKnobRight", TRANSLATE_NOOP("USB", "Effects Knob Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_EFFECTSKNOB_RIGHT, GenericInputBinding::RightStickRight},
			{"LeftTurntableCW", TRANSLATE_NOOP("USB", "Left Turntable Clockwise"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_LEFT_TURNTABLE_CW, GenericInputBinding::LeftStickRight},
			{"LeftTurntableCCW", TRANSLATE_NOOP("USB", "Left Turntable Counterclockwise"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_LEFT_TURNTABLE_CCW, GenericInputBinding::LeftStickLeft},
			{"RightTurntableCW", TRANSLATE_NOOP("USB", "Right Turntable Clockwise"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_RIGHT_TURNTABLE_CW, GenericInputBinding::LeftStickUp},
			{"RightTurntableCCW", TRANSLATE_NOOP("USB", "Right Turntable Counterclockwise"), nullptr, InputBindingInfo::Type::HalfAxis, CID_DJ_RIGHT_TURNTABLE_CCW, GenericInputBinding::LeftStickDown},
			{"LeftTurntableGreen", TRANSLATE_NOOP("USB", "Left Turntable Green"), nullptr, InputBindingInfo::Type::Button, CID_DJ_LEFT_GREEN, GenericInputBinding::Unknown},
			{"LeftTurntableRed", TRANSLATE_NOOP("USB", "Left Turntable Red"), nullptr, InputBindingInfo::Type::Button, CID_DJ_LEFT_RED, GenericInputBinding::Unknown},
			{"LeftTurntableBlue", TRANSLATE_NOOP("USB", "Left Turntable Blue"), nullptr, InputBindingInfo::Type::Button, CID_DJ_LEFT_BLUE, GenericInputBinding::Unknown},
			{"RightTurntableGreen", TRANSLATE_NOOP("USB", "Right Turntable Green"), nullptr, InputBindingInfo::Type::Button, CID_DJ_RIGHT_GREEN, GenericInputBinding::Cross},
			{"RightTurntableRed", TRANSLATE_NOOP("USB", "Right Turntable Red "), nullptr, InputBindingInfo::Type::Button, CID_DJ_RIGHT_RED, GenericInputBinding::Circle},
			{"RightTurntableBlue", TRANSLATE_NOOP("USB", "Right Turntable Blue"), nullptr, InputBindingInfo::Type::Button, CID_DJ_RIGHT_BLUE, GenericInputBinding::Square}

		};

		return bindings;
	}

	std::span<const SettingInfo> DJTurntableDevice::Settings(u32 subtype) const
	{
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::Float, "TurntableMultiplier", TRANSLATE_NOOP("USB", "Turntable Multiplier"),
				TRANSLATE_NOOP("USB", "Apply a sensitivity multiplier to turntable rotation.\nXbox 360 turntables require a 256x multiplier, most other turntables can use the default 1x multiplier."),
				"1.00", "0.00", "512.0", "1.0", "%.0fx", nullptr, nullptr, 1.0f}};

		return info;
	}

} // namespace usb_pad
