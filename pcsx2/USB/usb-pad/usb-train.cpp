// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "usb-train.h"

#include "common/Console.h"
#include "Host.h"
#include "IconsPromptFont.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/deviceproxy.h"
#include "USB/USB.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"

namespace usb_pad
{
	const char* TrainDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Train Controller");
	}

	const char* TrainDevice::TypeName() const
	{
		return "TrainController";
	}

	std::span<const char*> TrainDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Type 2"),
			TRANSLATE_NOOP("USB", "Shinkansen"),
		};
		return subtypes;
	}

	enum TrainControlID
	{
		CID_TC_POWER,
		CID_TC_BRAKE,
		CID_TC_UP,
		CID_TC_RIGHT,
		CID_TC_DOWN,
		CID_TC_LEFT,

		// TCPP20009 sends the buttons in this order in the relevant byte
		CID_TC_B,
		CID_TC_A,
		CID_TC_C,
		CID_TC_D,
		CID_TC_SELECT,
		CID_TC_START,

		BUTTONS_OFFSET = CID_TC_B,
	};

	std::span<const InputBindingInfo> TrainDevice::Bindings(u32 subtype) const
	{
		switch (subtype)
		{
			case TRAIN_TYPE2:
			case TRAIN_SHINKANSEN:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"Power", TRANSLATE_NOOP("USB", "Power"), ICON_PF_LEFT_ANALOG_DOWN, InputBindingInfo::Type::Axis, CID_TC_POWER, GenericInputBinding::LeftStickDown},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), ICON_PF_LEFT_ANALOG_UP, InputBindingInfo::Type::Axis, CID_TC_BRAKE, GenericInputBinding::LeftStickUp},
					{"Up", TRANSLATE_NOOP("USB", "D-Pad Up"), ICON_PF_DPAD_UP, InputBindingInfo::Type::Button, CID_TC_UP, GenericInputBinding::DPadUp},
					{"Down", TRANSLATE_NOOP("USB", "D-Pad Down"), ICON_PF_DPAD_DOWN, InputBindingInfo::Type::Button, CID_TC_DOWN, GenericInputBinding::DPadDown},
					{"Left", TRANSLATE_NOOP("USB", "D-Pad Left"), ICON_PF_DPAD_LEFT, InputBindingInfo::Type::Button, CID_TC_LEFT, GenericInputBinding::DPadLeft},
					{"Right", TRANSLATE_NOOP("USB", "D-Pad Right"), ICON_PF_DPAD_RIGHT, InputBindingInfo::Type::Button, CID_TC_RIGHT, GenericInputBinding::DPadRight},
					{"A", TRANSLATE_NOOP("USB", "A Button"), ICON_PF_KEY_A, InputBindingInfo::Type::Button, CID_TC_A, GenericInputBinding::Square},
					{"B", TRANSLATE_NOOP("USB", "B Button"), ICON_PF_KEY_B, InputBindingInfo::Type::Button, CID_TC_B, GenericInputBinding::Cross},
					{"C", TRANSLATE_NOOP("USB", "C Button"), ICON_PF_KEY_C, InputBindingInfo::Type::Button, CID_TC_C, GenericInputBinding::Circle},
					{"D", TRANSLATE_NOOP("USB", "D Button"), ICON_PF_KEY_D, InputBindingInfo::Type::Button, CID_TC_D, GenericInputBinding::Triangle},
					{"Select", TRANSLATE_NOOP("USB", "Select"), ICON_PF_SELECT_SHARE, InputBindingInfo::Type::Button, CID_TC_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), ICON_PF_START, InputBindingInfo::Type::Button, CID_TC_START, GenericInputBinding::Start},
				};

				return bindings;
			}
			default:
				break;
		}
		return {};
	}

	static void train_handle_reset(USBDevice* dev)
	{
		TrainDeviceState* s = USB_CONTAINER_OF(dev, TrainDeviceState, dev);
		s->Reset();
	}

	static void train_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		if (usb_desc_handle_control(dev, p, request, value, index, length, data) < 0)
			p->status = USB_RET_STALL;
	}

	static void train_handle_destroy(USBDevice* dev) noexcept
	{
		TrainDeviceState* s = USB_CONTAINER_OF(dev, TrainDeviceState, dev);
		delete s;
	}

	bool TrainDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		TrainDeviceState* s = USB_CONTAINER_OF(dev, TrainDeviceState, dev);

		if (!sw.DoMarker("TrainController"))
			return false;

		sw.Do(&s->data.power);
		sw.Do(&s->data.brake);
		return true;
	}

	static constexpr u32 button_mask(u32 bind_index)
	{
		return (1u << (bind_index - TrainControlID::BUTTONS_OFFSET));
	}

	static constexpr u8 button_at(u8 value, u32 index)
	{
		return value & button_mask(index);
	}

	float TrainDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		const TrainDeviceState* s = USB_CONTAINER_OF(dev, const TrainDeviceState, dev);

		switch (bind_index)
		{
			case CID_TC_POWER:
				return (static_cast<float>(s->data.power) / 255.0f);
			case CID_TC_BRAKE:
				return (static_cast<float>(s->data.brake) / 255.0f);

			case CID_TC_UP:
				return static_cast<float>(s->data.hat_up);
			case CID_TC_DOWN:
				return static_cast<float>(s->data.hat_down);
			case CID_TC_LEFT:
				return static_cast<float>(s->data.hat_left);
			case CID_TC_RIGHT:
				return static_cast<float>(s->data.hat_right);

			case CID_TC_A:
			case CID_TC_B:
			case CID_TC_C:
			case CID_TC_D:
			case CID_TC_SELECT:
			case CID_TC_START:
			{
				return (button_at(s->data.buttons, bind_index) != 0u) ? 1.0f : 0.0f;
			}

			default:
				return 0.0f;
		}
	}

	void TrainDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		TrainDeviceState* s = USB_CONTAINER_OF(dev, TrainDeviceState, dev);

		switch (bind_index)
		{
			case CID_TC_POWER:
				s->data.power = static_cast<u32>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;
			case CID_TC_BRAKE:
				s->data.brake = static_cast<u32>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_TC_UP:
				s->data.hat_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_TC_DOWN:
				s->data.hat_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_TC_LEFT:
				s->data.hat_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;
			case CID_TC_RIGHT:
				s->data.hat_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateHatSwitch();
				break;

			case CID_TC_A:
			case CID_TC_B:
			case CID_TC_C:
			case CID_TC_D:
			case CID_TC_SELECT:
			case CID_TC_START:
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

	TrainDeviceState::TrainDeviceState(u32 port_, TrainDeviceTypes type_)
		: port(port_)
		, type(type_)
	{
		Reset();
	}

	TrainDeviceState::~TrainDeviceState() = default;

	void TrainDeviceState::Reset()
	{
		data.power = 0x00;
		data.brake = 0x00;
	}

	void TrainDeviceState::UpdateHatSwitch() noexcept
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

	static u8 dct01_power(u8 value)
	{
		// (N) 0x81	0x6D 0x54 0x3F 0x21	0x00 (P5)
		static std::pair<u8, u8> const notches[] = {
			// { control_in, emulated_out },
			{0xF8, 0x00},
			{0xC8, 0x21},
			{0x98, 0x3F},
			{0x58, 0x54},
			{0x28, 0x6D},
			{0x00, 0x81},
		};

		for (const auto& x : notches)
		{
			if (value >= x.first)
				return x.second;
		}
		return notches[std::size(notches) - 1].second;
	}

	static u8 dct01_brake(u8 value)
	{
		// (NB) 0x79 0x8A 0x94 0x9A 0xA2 0xA8 0xAF 0xB2 0xB5 0xB9 (EB)
		static std::pair<u8, u8> const notches[] = {
			// { control_in, emulated_out },
			{0xF8, 0xB9},
			{0xE6, 0xB5},
			{0xCA, 0xB2},
			{0xAE, 0xAF},
			{0x92, 0xA8},
			{0x76, 0xA2},
			{0x5A, 0x9A},
			{0x3E, 0x94},
			{0x22, 0x8A},
			{0x00, 0x79},
		};

		for (const auto& x : notches)
		{
			if (value >= x.first)
				return x.second;
		}
		return notches[std::size(notches) - 1].second;
	}

	static u8 dct02_power(u8 value)
	{
		// (N) 0x12 0x24 0x36 0x48 0x5A 0x6C 0x7E 0x90 0xA2 0xB4 0xC6 0xD7 0xE9 0xFB (P13)
		static std::pair<u8, u8> const notches[] = {
			// { control_in, emulated_out },
			{0xF7, 0xFB},
			{0xE4, 0xE9},
			{0xD1, 0xD7},
			{0xBE, 0xC6},
			{0xAB, 0xB4},
			{0x98, 0xA2},
			{0x85, 0x90},
			{0x72, 0x7E},
			{0x5F, 0x6C},
			{0x4C, 0x5A},
			{0x39, 0x48},
			{0x26, 0x36},
			{0x13, 0x24},
			{0x00, 0x12},
		};

		for (const auto& x : notches)
		{
			if (value >= x.first)
				return x.second;
		}
		return notches[std::size(notches) - 1].second;
	}
	static u8 dct02_brake(u8 value)
	{
		// (NB) 0x1C 0x38 0x54 0x70 0x8B 0xA7 0xC3 0xDF 0xFB (EB)
		static std::pair<u8, u8> const notches[] = {
			// { control_in, emulated_out },
			{0xF8, 0xFB},
			{0xCA, 0xDF},
			{0xAE, 0xC3},
			{0x92, 0xA7},
			{0x76, 0x8B},
			{0x5A, 0x70},
			{0x3E, 0x54},
			{0x22, 0x38},
			{0x00, 0x1C},
		};

		for (const auto& x : notches)
		{
			if (value >= x.first)
				return x.second;
		}
		return notches[std::size(notches) - 1].second;
	}

#define get_ab(buttons) (button_at(buttons, CID_TC_A) | button_at(buttons, CID_TC_B))
#define swap_cd(buttons) ((button_at(buttons, CID_TC_C) << 1) | (button_at(buttons, CID_TC_D) >> 1))
#define get_ss(buttons) (button_at(buttons, CID_TC_START) | button_at(buttons, CID_TC_SELECT))

	// TrainControlID buttons are laid out in Type 2 ordering, no need to remap.
	constexpr u8 dct01_buttons(u8 buttons) { return buttons; }
	constexpr u8 dct02_buttons(u8 buttons)
	{
		return ((get_ab(buttons) << 2) | (swap_cd(buttons) >> 2) | get_ss(buttons));
	}

	static void train_handle_data(USBDevice* dev, USBPacket* p)
	{
		TrainDeviceState* s = USB_CONTAINER_OF(dev, TrainDeviceState, dev);

		if (p->pid != USB_TOKEN_IN || p->ep->nr != 1)
		{
			Console.Error("Unhandled TrainController request pid=%d ep=%u", p->pid, p->ep->nr);
			p->status = USB_RET_STALL;
			return;
		}

		s->UpdateHatSwitch();

		switch (s->type)
		{
			case TRAIN_TYPE2:
			{
				TrainConData_Type2 out = {};
				out.control = 0x1;
				out.brake = dct01_brake(s->data.brake);
				out.power = dct01_power(s->data.power);
				out.horn = 0xFF; // Button C doubles as horn.
				out.hat = s->data.hatswitch;
				out.buttons = dct01_buttons(s->data.buttons);
				usb_packet_copy(p, &out, sizeof(out));
				break;
			}
			case TRAIN_SHINKANSEN:
			{
				TrainConData_Shinkansen out = {};
				out.brake = dct02_brake(s->data.brake);
				out.power = dct02_power(s->data.power);
				out.horn = 0xFF; // Button C doubles as horn, skip.
				out.hat = s->data.hatswitch;
				out.buttons = dct02_buttons(s->data.buttons);
				usb_packet_copy(p, &out, sizeof(out));
				break;
			}
			default:
				Console.Error("Unhandled TrainController USB_TOKEN_IN pid=%d ep=%u type=%u", p->pid, p->ep->nr, s->type);
				p->status = USB_RET_IOERROR;
				return;
		}
	}

	USBDevice* TrainDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		TrainDeviceState* s = new TrainDeviceState(port, static_cast<TrainDeviceTypes>(subtype));

		s->desc.full = &s->desc_dev;

		switch (subtype)
		{
			case TRAIN_TYPE2:
				s->desc.str = dct01_desc_strings;
				if (usb_desc_parse_dev(dct01_dev_descriptor, sizeof(dct01_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				break;
			case TRAIN_SHINKANSEN:
				s->desc.str = dct02_desc_strings;
				if (usb_desc_parse_dev(dct02_dev_descriptor, sizeof(dct02_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				break;

			default:
				goto fail;
		}

		if (usb_desc_parse_config(taito_denshacon_config_descriptor, sizeof(taito_denshacon_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = train_handle_reset;
		s->dev.klass.handle_control = train_handle_control;
		s->dev.klass.handle_data = train_handle_data;
		s->dev.klass.unrealize = train_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		train_handle_reset(&s->dev);

		return &s->dev;

	fail:
		train_handle_destroy(&s->dev);
		return nullptr;
	}
} // namespace usb_pad
