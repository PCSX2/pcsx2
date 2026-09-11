// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "usb-rhythm.h"
#include "IconsFontAwesome.h"
#include "IconsPromptFont.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/USB.h"
#include "Host.h"
#include "StateWrapper.h"

#include "common/Console.h"

namespace usb_pad
{
	static const USBDescStrings rb1_desc_strings = {
		"",
		"Licensed by Sony Computer Entertainment America",
		"Harmonix Drum Kit for PlayStation(R)3"};

	static const USBDescStrings kbm_desc_strings = {
		"",
		"KONAMI",
		"USB Multipurpose Controller"};
	
	static const USBDescStrings para_para_paradise_desc_strings = {
		"",
		"KONAMI",
		"ParaParaParadise Controller"};

	RhythmState::RhythmState(u32 port_, RhythmType type_)
		: port(port_)
		, type(type_)
	{
	}

	RhythmState::~RhythmState() = default;
	
	u8 RhythmState::UpdateHatSwitch() noexcept
	{
		if (hat_up && hat_right)
			return 1;
		else if (hat_right && hat_down)
			return 3;
		else if (hat_down && hat_left)
			return 5;
		else if (hat_left && hat_up)
			return 7;
		else if (hat_up)
			return 0;
		else if (hat_right)
			return 2;
		else if (hat_down)
			return 4;
		else if (hat_left)
			return 6;
		else
			return 8;
	}

	static void pad_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		RhythmState* s = USB_CONTAINER_OF(dev, RhythmState, dev);
		int ret = 0;

		switch (request)
		{
			case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret < 0)
					goto fail;
				break;
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				switch (value >> 8)
				{
					case USB_DT_REPORT:
						if (s->type == KEYBOARDMANIA_CONTROLLER)
						{
							ret = sizeof(kbm_hid_report_descriptor);
							memcpy(data, kbm_hid_report_descriptor, ret);
						}
						else if (s->type == PARA_PARA_PARADISE)
						{
							ret = sizeof(ppp_hid_descriptor);
							memcpy(data, ppp_hid_descriptor, ret);
						}
						p->actual_length = ret;
						break;
					default:
						goto fail;
				}
				break;
			/* hid specific requests */
			case SET_REPORT:
				if (length > 0)
				{
					p->actual_length = 0;
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

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		RhythmState* s = USB_CONTAINER_OF(dev, RhythmState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
			{
				if (p->ep->nr != 1)
					goto fail;

				switch (s->type)
				{
					case ROCKBAND1_DRUMKIT:
						s->rb1.dpad = s->UpdateHatSwitch();
						usb_packet_copy(p, &s->rb1, sizeof(s->rb1));
						break;
					case KEYBOARDMANIA_CONTROLLER:
						s->kbm.head = 0x3F;
						usb_packet_copy(p, &s->kbm, sizeof(s->kbm));
						break;
					case PARA_PARA_PARADISE:
						usb_packet_copy(p, &s->ppp, sizeof(s->ppp));
						break;
					case DANCE_UK_XL:
						s->duk.btn_1_2 = s->btn1 ? 0x00
										: s->btn2 ? 0xff
										: 0x7f;
						s->duk.btn_3_4 = s->btn3 ? 0x00
										: s->btn4 ? 0xff
										: 0x7f;
						usb_packet_copy(p, &s->duk, sizeof(s->duk));
						break;
					case DANCING_WITH_THE_STARS:
						usb_packet_copy(p, &s->dwts, sizeof(s->dwts));
						break;
				}
				break;
			}
			case USB_TOKEN_OUT:
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_unrealize(USBDevice* dev)
	{
		RhythmState* s = USB_CONTAINER_OF(dev, RhythmState, dev);
		delete s;
	}

	const char* RhythmDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Rhythm Game");
	}

	const char* RhythmDevice::TypeName() const
	{
		return "rhythm";
	}

	const char* RhythmDevice::IconName() const
	{
		return ICON_FA_DRUM;
	}

	bool RhythmDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		return true;
	}

	std::span<const char*> RhythmDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Rock Band drum kit"),
			TRANSLATE_NOOP("USB", "KeyboardMania"),
			TRANSLATE_NOOP("USB", "Para Para Paradise"),
			TRANSLATE_NOOP("USB", "Dance:UK XL / Party"),
			TRANSLATE_NOOP("USB", "Dancing With the Stars")
		};
		return subtypes;
	}

	USBDevice* RhythmDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		RhythmState* s = new RhythmState(port, static_cast<RhythmType>(subtype));

		s->desc.full = &s->desc_dev;
		
		switch (subtype)
		{
			case ROCKBAND1_DRUMKIT:
			{
				s->desc.str = rb1_desc_strings;
				if (usb_desc_parse_dev(rb1_dev_descriptor, sizeof(rb1_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(rb1_config_descriptor, sizeof(rb1_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			}
			case KEYBOARDMANIA_CONTROLLER:
			{
				s->desc.str = kbm_desc_strings;
				if (usb_desc_parse_dev(kbm_dev_descriptor, sizeof(kbm_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(kbm_config_descriptor, sizeof(kbm_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			}
			case PARA_PARA_PARADISE:
				s->desc.str = para_para_paradise_desc_strings;
				if (usb_desc_parse_dev(ppp_dev_descriptor, sizeof(ppp_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(ppp_config_descriptor, sizeof(ppp_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			case DANCE_UK_XL:
			case DANCING_WITH_THE_STARS:
				if (usb_desc_parse_dev(dwts_dev_descriptor, sizeof(dwts_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(kbm_config_descriptor, sizeof(kbm_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
		}

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = nullptr;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		return &s->dev;

	fail:
		pad_unrealize(&s->dev);
		return nullptr;
	}

	std::span<const InputBindingInfo> RhythmDevice::Bindings(u32 subtype) const
	{
		switch (subtype)
		{
			case ROCKBAND1_DRUMKIT:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"Blue", TRANSLATE_NOOP("USB", "Blue"), nullptr, InputBindingInfo::Type::Button, RB1_BLUE, GenericInputBinding::R1},
					{"Green", TRANSLATE_NOOP("USB", "Green"), nullptr, InputBindingInfo::Type::Button, RB1_GREEN, GenericInputBinding::Triangle},
					{"Red", TRANSLATE_NOOP("USB", "Red"), nullptr, InputBindingInfo::Type::Button, RB1_RED, GenericInputBinding::Circle},
					{"Yellow", TRANSLATE_NOOP("USB", "Yellow"), nullptr, InputBindingInfo::Type::Button, RB1_YELLOW, GenericInputBinding::Square},
					{"Orange", TRANSLATE_NOOP("USB", "Orange"), nullptr, InputBindingInfo::Type::Button, RB1_ORANGE, GenericInputBinding::Cross},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, RB1_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, RB1_START, GenericInputBinding::Start},
					{"D-Pad Up", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, RB1_UP, GenericInputBinding::DPadUp},
					{"D-Pad Right", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, RB1_RIGHT, GenericInputBinding::DPadRight},
					{"D-Pad Down", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, RB1_DOWN, GenericInputBinding::DPadDown},
					{"D-Pad Left", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, RB1_LEFT, GenericInputBinding::DPadLeft},
				};
				return bindings;
			}
			
			case KEYBOARDMANIA_CONTROLLER:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"C1", TRANSLATE_NOOP("USB", "C 1"), nullptr, InputBindingInfo::Type::Button, KBM_C1, GenericInputBinding::Unknown},
					{"CSharp1", TRANSLATE_NOOP("USB", "C# 1"), nullptr, InputBindingInfo::Type::Button, KBM_CS1, GenericInputBinding::Unknown},
					{"D1", TRANSLATE_NOOP("USB", "D 1"), nullptr, InputBindingInfo::Type::Button, KBM_D1, GenericInputBinding::Unknown},
					{"DSharp1", TRANSLATE_NOOP("USB", "D# 1"), nullptr, InputBindingInfo::Type::Button, KBM_DS1, GenericInputBinding::Unknown},
					{"E1", TRANSLATE_NOOP("USB", "E 1"), nullptr, InputBindingInfo::Type::Button, KBM_E1, GenericInputBinding::Unknown},
					{"F1", TRANSLATE_NOOP("USB", "F 1"), nullptr, InputBindingInfo::Type::Button, KBM_F1, GenericInputBinding::Unknown},
					{"FSharp1", TRANSLATE_NOOP("USB", "F# 1"), nullptr, InputBindingInfo::Type::Button, KBM_FS1, GenericInputBinding::Unknown},
					{"G1", TRANSLATE_NOOP("USB", "G 1"), nullptr, InputBindingInfo::Type::Button, KBM_G1, GenericInputBinding::Unknown},
					{"GSharp1", TRANSLATE_NOOP("USB", "G# 1"), nullptr, InputBindingInfo::Type::Button, KBM_GS1, GenericInputBinding::Unknown},
					{"A1", TRANSLATE_NOOP("USB", "A 1"), nullptr, InputBindingInfo::Type::Button, KBM_A1, GenericInputBinding::Unknown},
					{"ASharp1", TRANSLATE_NOOP("USB", "A# 1"), nullptr, InputBindingInfo::Type::Button, KBM_AS1, GenericInputBinding::Unknown},
					{"B1", TRANSLATE_NOOP("USB", "B 1"), nullptr, InputBindingInfo::Type::Button, KBM_B1, GenericInputBinding::Unknown},
					{"C2", TRANSLATE_NOOP("USB", "C 2"), nullptr, InputBindingInfo::Type::Button, KBM_C2, GenericInputBinding::Unknown},
					{"CSharp2", TRANSLATE_NOOP("USB", "C# 2"), nullptr, InputBindingInfo::Type::Button, KBM_CS2, GenericInputBinding::Unknown},
					{"D2", TRANSLATE_NOOP("USB", "D 2"), nullptr, InputBindingInfo::Type::Button, KBM_D2, GenericInputBinding::Unknown},
					{"DSharp2", TRANSLATE_NOOP("USB", "D# 2"), nullptr, InputBindingInfo::Type::Button, KBM_DS2, GenericInputBinding::Unknown},
					{"E2", TRANSLATE_NOOP("USB", "E 2"), nullptr, InputBindingInfo::Type::Button, KBM_E2, GenericInputBinding::Unknown},
					{"F2", TRANSLATE_NOOP("USB", "F 2"), nullptr, InputBindingInfo::Type::Button, KBM_F2, GenericInputBinding::Unknown},
					{"FSharp2", TRANSLATE_NOOP("USB", "F# 2"), nullptr, InputBindingInfo::Type::Button, KBM_FS2, GenericInputBinding::Unknown},
					{"G2", TRANSLATE_NOOP("USB", "G 2"), nullptr, InputBindingInfo::Type::Button, KBM_G2, GenericInputBinding::Unknown},
					{"GSharp2", TRANSLATE_NOOP("USB", "G# 2"), nullptr, InputBindingInfo::Type::Button, KBM_GS2, GenericInputBinding::Unknown},
					{"A2", TRANSLATE_NOOP("USB", "A 2"), nullptr, InputBindingInfo::Type::Button, KBM_A2, GenericInputBinding::Unknown},
					{"ASharp2", TRANSLATE_NOOP("USB", "A# 2"), nullptr, InputBindingInfo::Type::Button, KBM_AS2, GenericInputBinding::Unknown},
					{"B2", TRANSLATE_NOOP("USB", "B 2"), nullptr, InputBindingInfo::Type::Button, KBM_B2, GenericInputBinding::Unknown},

					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, KBM_START, GenericInputBinding::Unknown},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, KBM_SELECT, GenericInputBinding::Unknown},
					{"WheelUp", TRANSLATE_NOOP("USB", "Wheel Up"), nullptr, InputBindingInfo::Type::Button, KBM_UP, GenericInputBinding::Unknown},
					{"WheelDown", TRANSLATE_NOOP("USB", "Wheel Down"), nullptr, InputBindingInfo::Type::Button, KBM_DOWN, GenericInputBinding::Unknown},
				};
				return bindings;
			}
		
			case PARA_PARA_PARADISE:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"Left", TRANSLATE_NOOP("USB", "Left"), nullptr, InputBindingInfo::Type::Button, PPP_LEFT, GenericInputBinding::Unknown},
					{"UpLeft", TRANSLATE_NOOP("USB", "Up Left"), nullptr, InputBindingInfo::Type::Button, PPP_UP_LEFT, GenericInputBinding::Unknown},
					{"Up", TRANSLATE_NOOP("USB", "Up"), nullptr, InputBindingInfo::Type::Button, PPP_UP, GenericInputBinding::Unknown},
					{"UpRight", TRANSLATE_NOOP("USB", "Up Right"), nullptr, InputBindingInfo::Type::Button, PPP_UP_RIGHT, GenericInputBinding::Unknown},
					{"Right", TRANSLATE_NOOP("USB", "Right"), nullptr, InputBindingInfo::Type::Button, PPP_RIGHT, GenericInputBinding::Unknown},
					{"MenuLeft", TRANSLATE_NOOP("USB", "Menu Left"), nullptr, InputBindingInfo::Type::Button, PPP_MENU_LEFT, GenericInputBinding::DPadLeft},
					{"MenuRight", TRANSLATE_NOOP("USB", "Menu Right"), nullptr, InputBindingInfo::Type::Button, PPP_MENU_RIGHT, GenericInputBinding::DPadRight},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, PPP_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, PPP_START, GenericInputBinding::Start},
				};
				return bindings;
			}

			case DANCE_UK_XL:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"Up", TRANSLATE_NOOP("USB", "Up"), nullptr, InputBindingInfo::Type::Button, DM_UP, GenericInputBinding::DPadUp},
					{"Down", TRANSLATE_NOOP("USB", "Down"), nullptr, InputBindingInfo::Type::Button, DM_DOWN, GenericInputBinding::DPadDown},
					{"Left", TRANSLATE_NOOP("USB", "Left"), nullptr, InputBindingInfo::Type::Button, DM_LEFT, GenericInputBinding::DPadLeft},
					{"Right", TRANSLATE_NOOP("USB", "Right"), nullptr, InputBindingInfo::Type::Button, DM_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, DM_CROSS, GenericInputBinding::Cross},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, DM_CIRCLE, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, DM_TRIANGLE, GenericInputBinding::Triangle},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, DM_SQUARE, GenericInputBinding::Square},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, DM_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, DM_START, GenericInputBinding::Start},
					{"1", TRANSLATE_NOOP("USB", "1"), nullptr, InputBindingInfo::Type::Button, DM_BTN_1, GenericInputBinding::Unknown},
					{"2", TRANSLATE_NOOP("USB", "2"), nullptr, InputBindingInfo::Type::Button, DM_BTN_2, GenericInputBinding::Unknown},
					{"3", TRANSLATE_NOOP("USB", "3"), nullptr, InputBindingInfo::Type::Button, DM_BTN_3, GenericInputBinding::Unknown},
					{"4", TRANSLATE_NOOP("USB", "4"), nullptr, InputBindingInfo::Type::Button, DM_BTN_4, GenericInputBinding::Unknown},
					{"5", TRANSLATE_NOOP("USB", "5"), nullptr, InputBindingInfo::Type::Button, DM_BTN_5, GenericInputBinding::Unknown},
					{"6", TRANSLATE_NOOP("USB", "6"), nullptr, InputBindingInfo::Type::Button, DM_BTN_6, GenericInputBinding::Unknown},
					{"7", TRANSLATE_NOOP("USB", "7"), nullptr, InputBindingInfo::Type::Button, DM_BTN_7, GenericInputBinding::Unknown},
					{"8", TRANSLATE_NOOP("USB", "8"), nullptr, InputBindingInfo::Type::Button, DM_BTN_8, GenericInputBinding::Unknown},
				};
				return bindings;
			}

			case DANCING_WITH_THE_STARS:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"Up", TRANSLATE_NOOP("USB", "Up"), nullptr, InputBindingInfo::Type::Button, DM_UP, GenericInputBinding::DPadUp},
					{"Down", TRANSLATE_NOOP("USB", "Down"), nullptr, InputBindingInfo::Type::Button, DM_DOWN, GenericInputBinding::DPadDown},
					{"Left", TRANSLATE_NOOP("USB", "Left"), nullptr, InputBindingInfo::Type::Button, DM_LEFT, GenericInputBinding::DPadLeft},
					{"Right", TRANSLATE_NOOP("USB", "Right"), nullptr, InputBindingInfo::Type::Button, DM_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, DM_CROSS, GenericInputBinding::Cross},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, DM_CIRCLE, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, DM_TRIANGLE, GenericInputBinding::Triangle},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, DM_SQUARE, GenericInputBinding::Square},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, DM_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, DM_START, GenericInputBinding::Start},
				};
				return bindings;
			}

			default:
			{
				return {};
			}
		}
	}
	
	std::span<const SettingInfo> RhythmDevice::Settings(u32 subtype) const
	{
		return {};
	}

	float RhythmDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		RhythmState* s = USB_CONTAINER_OF(dev, RhythmState, dev);
		switch (s->type)
		{
		case ROCKBAND1_DRUMKIT:
			switch (bind_index)
			{
				case RB1_BLUE:   return s->rb1.blue;
				case RB1_GREEN:  return s->rb1.green;
				case RB1_RED:    return s->rb1.red;
				case RB1_YELLOW: return s->rb1.yellow;
				case RB1_ORANGE: return s->rb1.orange;
				case RB1_SELECT: return s->rb1.select;
				case RB1_START:  return s->rb1.start;
				case RB1_UP:     return s->hat_up;
				case RB1_RIGHT:  return s->hat_right;
				case RB1_DOWN:   return s->hat_down;
				case RB1_LEFT:   return s->hat_left;
				default:
					return 0.0f;
			}
			break;
		case KEYBOARDMANIA_CONTROLLER:
			switch (bind_index)
			{
				case KBM_C1:  return s->kbm.c1;
				case KBM_CS1: return s->kbm.cs1;
				case KBM_D1:  return s->kbm.d1;
				case KBM_DS1: return s->kbm.ds1;
				case KBM_E1:  return s->kbm.e1;
				case KBM_F1:  return s->kbm.f1;
				case KBM_FS1: return s->kbm.fs1;
				case KBM_G1:  return s->kbm.g1;
				case KBM_GS1: return s->kbm.gs1;
				case KBM_A1:  return s->kbm.a1;
				case KBM_AS1: return s->kbm.as1;
				case KBM_B1:  return s->kbm.b1;
				case KBM_C2:  return s->kbm.c2;
				case KBM_CS2: return s->kbm.cs2;
				case KBM_D2:  return s->kbm.d2;
				case KBM_DS2: return s->kbm.ds2;
				case KBM_E2:  return s->kbm.e2;
				case KBM_F2:  return s->kbm.f2;
				case KBM_FS2: return s->kbm.fs2;
				case KBM_G2:  return s->kbm.g2;
				case KBM_GS2: return s->kbm.gs2;
				case KBM_A2:  return s->kbm.a2;
				case KBM_AS2: return s->kbm.as2;
				case KBM_B2:  return s->kbm.b2;
				case KBM_START: return s->kbm.start;
				case KBM_SELECT: return s->kbm.select;
				case KBM_UP:   return s->kbm.up;
				case KBM_DOWN: return s->kbm.down;
				default:
					return 0.0f;
			}
			break;
		case PARA_PARA_PARADISE:
			switch (bind_index)
			{
				case PPP_LEFT:       return s->ppp.btn_left;
				case PPP_UP_LEFT:    return s->ppp.btn_up_left;
				case PPP_UP:         return s->ppp.btn_up;
				case PPP_UP_RIGHT:   return s->ppp.btn_up_right;
				case PPP_RIGHT:      return s->ppp.btn_right;
				case PPP_MENU_LEFT:  return s->ppp.menu_left;
				case PPP_MENU_RIGHT: return s->ppp.menu_right;
				case PPP_SELECT:     return s->ppp.select;
				case PPP_START:      return s->ppp.start;
				default:
					return 0.0f;
			}
			break;
		case DANCE_UK_XL:
			switch (bind_index)
			{
				case DM_UP: return s->duk.up;
				case DM_DOWN: return s->duk.down;
				case DM_LEFT: return s->duk.left;
				case DM_RIGHT: return s->duk.right;
				case DM_CROSS: return s->duk.cross;
				case DM_CIRCLE: return s->duk.circle;
				case DM_TRIANGLE: return s->duk.triangle;
				case DM_SQUARE: return s->duk.square;
				case DM_SELECT: return s->duk.select;
				case DM_START: return s->duk.start;
				case DM_BTN_1: return s->btn1;
				case DM_BTN_2: return s->btn2;
				case DM_BTN_3: return s->btn3;
				case DM_BTN_4: return s->btn4;
				case DM_BTN_5: return s->duk.btn_5;
				case DM_BTN_6: return s->duk.btn_6;
				case DM_BTN_7: return s->duk.btn_7;
				case DM_BTN_8: return s->duk.btn_8;
				default:
					return 0.0f;
			}
			break;
		case DANCING_WITH_THE_STARS:
			switch (bind_index)
			{
				case DM_UP: return !s->dwts.up;
				case DM_DOWN: return !s->dwts.down;
				case DM_LEFT: return !s->dwts.left;
				case DM_RIGHT: return !s->dwts.right;
				case DM_CROSS: return !s->dwts.cross;
				//case DM_CIRCLE: return !s->dwts.circle;
				case DM_TRIANGLE: return !s->dwts.triangle;
				//case DM_SQUARE: return !s->dwts.square;
				//case DM_SELECT: return !s->dwts.select;
				case DM_START: return !s->dwts.start;
				default:
					return 0.0f;
			}
			break;
		}
	}

	void RhythmDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		RhythmState* s = USB_CONTAINER_OF(dev, RhythmState, dev);
		switch (s->type)
		{
		case ROCKBAND1_DRUMKIT:
			switch (bind_index)
			{
				case RB1_BLUE:   s->rb1.blue   = (value >= 0.5f); break;
				case RB1_GREEN:  s->rb1.green  = (value >= 0.5f); break;
				case RB1_RED:    s->rb1.red    = (value >= 0.5f); break;
				case RB1_YELLOW: s->rb1.yellow = (value >= 0.5f); break;
				case RB1_ORANGE: s->rb1.orange = (value >= 0.5f); break;
				case RB1_SELECT: s->rb1.select = (value >= 0.5f); break;
				case RB1_START:  s->rb1.start  = (value >= 0.5f); break;
				case RB1_UP:     s->hat_up     = (value >= 0.5f); break;
				case RB1_RIGHT:  s->hat_right  = (value >= 0.5f); break;
				case RB1_DOWN:   s->hat_down   = (value >= 0.5f); break;
				case RB1_LEFT:   s->hat_left   = (value >= 0.5f); break;
			}
			break;
		case KEYBOARDMANIA_CONTROLLER:
			switch (bind_index)
			{
				case KBM_C1:  s->kbm.c1  = (value >= 0.5f); break;
				case KBM_CS1: s->kbm.cs1 = (value >= 0.5f); break;
				case KBM_D1:  s->kbm.d1  = (value >= 0.5f); break;
				case KBM_DS1: s->kbm.ds1 = (value >= 0.5f); break;
				case KBM_E1:  s->kbm.e1  = (value >= 0.5f); break;
				case KBM_F1:  s->kbm.f1  = (value >= 0.5f); break;
				case KBM_FS1: s->kbm.fs1 = (value >= 0.5f); break;
				case KBM_G1:  s->kbm.g1  = (value >= 0.5f); break;
				case KBM_GS1: s->kbm.gs1 = (value >= 0.5f); break;
				case KBM_A1:  s->kbm.a1  = (value >= 0.5f); break;
				case KBM_AS1: s->kbm.as1 = (value >= 0.5f); break;
				case KBM_B1:  s->kbm.b1  = (value >= 0.5f); break;
				case KBM_C2:  s->kbm.c2  = (value >= 0.5f); break;
				case KBM_CS2: s->kbm.cs2 = (value >= 0.5f); break;
				case KBM_D2:  s->kbm.d2  = (value >= 0.5f); break;
				case KBM_DS2: s->kbm.ds2 = (value >= 0.5f); break;
				case KBM_E2:  s->kbm.e2  = (value >= 0.5f); break;
				case KBM_F2:  s->kbm.f2  = (value >= 0.5f); break;
				case KBM_FS2: s->kbm.fs2 = (value >= 0.5f); break;
				case KBM_G2:  s->kbm.g2  = (value >= 0.5f); break;
				case KBM_GS2: s->kbm.gs2 = (value >= 0.5f); break;
				case KBM_A2:  s->kbm.a2  = (value >= 0.5f); break;
				case KBM_AS2: s->kbm.as2 = (value >= 0.5f); break;
				case KBM_B2:  s->kbm.b2  = (value >= 0.5f); break;
				case KBM_START:  s->kbm.start  = (value >= 0.5f); break;
				case KBM_SELECT: s->kbm.select = (value >= 0.5f); break;
				case KBM_UP:   s->kbm.up   = (value >= 0.5f); break;
				case KBM_DOWN: s->kbm.down = (value >= 0.5f); break;
			}
			break;
		case PARA_PARA_PARADISE:
			switch (bind_index)
			{
				case PPP_LEFT:       s->ppp.btn_left     = (value >= 0.5f); break;
				case PPP_UP_LEFT:    s->ppp.btn_up_left  = (value >= 0.5f); break;
				case PPP_UP:         s->ppp.btn_up       = (value >= 0.5f); break;
				case PPP_UP_RIGHT:   s->ppp.btn_up_right = (value >= 0.5f); break;
				case PPP_RIGHT:      s->ppp.btn_right    = (value >= 0.5f); break;
				case PPP_MENU_LEFT:  s->ppp.menu_left    = (value >= 0.5f); break;
				case PPP_MENU_RIGHT: s->ppp.menu_right   = (value >= 0.5f); break;
				case PPP_SELECT:     s->ppp.select       = (value >= 0.5f); break;
				case PPP_START:      s->ppp.start        = (value >= 0.5f); break;
			}
			break;
		case DANCE_UK_XL:
			switch (bind_index)
			{
				case DM_UP:       s->duk.up       = (value >= 0.5f); break;
				case DM_DOWN:     s->duk.down     = (value >= 0.5f); break;
				case DM_LEFT:     s->duk.left     = (value >= 0.5f); break;
				case DM_RIGHT:    s->duk.right    = (value >= 0.5f); break;
				case DM_CROSS:    s->duk.cross    = (value >= 0.5f); break;
				case DM_CIRCLE:   s->duk.circle   = (value >= 0.5f); break;
				case DM_TRIANGLE: s->duk.triangle = (value >= 0.5f); break;
				case DM_SQUARE:   s->duk.square   = (value >= 0.5f); break;
				case DM_SELECT:   s->duk.select   = (value >= 0.5f); break;
				case DM_START:    s->duk.start    = (value >= 0.5f); break;
				case DM_BTN_1:    s->btn1         = (value >= 0.5f); break;
				case DM_BTN_2:    s->btn2         = (value >= 0.5f); break;
				case DM_BTN_3:    s->btn3         = (value >= 0.5f); break;
				case DM_BTN_4:    s->btn4         = (value >= 0.5f); break;
				case DM_BTN_5:    s->duk.btn_5    = (value >= 0.5f); break;
				case DM_BTN_6:    s->duk.btn_6    = (value >= 0.5f); break;
				case DM_BTN_7:    s->duk.btn_7    = (value >= 0.5f); break;
				case DM_BTN_8:    s->duk.btn_8    = (value >= 0.5f); break;
			}
			break;
		case DANCING_WITH_THE_STARS:
			switch (bind_index)
			{
				case DM_UP:       s->dwts.up       = !(value >= 0.5f); break;
				case DM_DOWN:     s->dwts.down     = !(value >= 0.5f); break;
				case DM_LEFT:     s->dwts.left     = !(value >= 0.5f); break;
				case DM_RIGHT:    s->dwts.right    = !(value >= 0.5f); break;
				case DM_CROSS:    s->dwts.cross    = !(value >= 0.5f); break;
				//case DM_CIRCLE: s->dwts.circle   = !(value >= 0.5f); break;
				case DM_TRIANGLE: s->dwts.triangle = !(value >= 0.5f); break;
				//case DM_SQUARE: s->dwts.square   = !(value >= 0.5f); break;
				//case DM_SELECT: s->dwts.select   = !(value >= 0.5f); break;
				case DM_START:    s->dwts.start    = !(value >= 0.5f); break;
			}
			break;
		}
	}
} // namespace usb_pad
