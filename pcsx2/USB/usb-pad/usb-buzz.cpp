// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-pad/usb-buzz.h"
#include "USB/usb-pad/usb-pad.h"
#include <common/Console.h>

namespace usb_pad
{
	static const USBDescStrings desc_strings = {
		"",
		"Logitech Buzz(tm) Controller V1",
		"",
		"Logitech"};

	static uint8_t dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0x4C, 0x05, // idVendor 0x054C
		0x02, 0x00, // idProduct 0x0002
		0xA1, 0x05, // bcdDevice 11.01
		0x03,       // iManufacturer (String Index)
		0x01,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x32,       // bMaxPower 100mA

		0x09,       // bLength
		0x04,       // bDescriptorType (Interface)
		0x00,       // bInterfaceNumber 0
		0x00,       // bAlternateSetting
		0x01,       // bNumEndpoints 1
		0x03,       // bInterfaceClass
		0x00,       // bInterfaceSubClass
		0x00,       // bInterfaceProtocol
		0x00,       // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x11, 0x01, // bcdHID 1.11
		0x33,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x4E, 0x00, // wDescriptorLength[0] 78

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	static const uint8_t hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x75, 0x08,       //     Report Size (8)
		0x95, 0x02,       //     Report Count (2)
		0x15, 0x00,       //     Logical Minimum (0)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x30,       //     Usage (X)
		0x09, 0x31,       //     Usage (Y)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x14,       //     Report Count (20)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x05, 0x09,       //     Usage Page (Button)
		0x19, 0x01,       //     Usage Minimum (0x01)
		0x29, 0x14,       //     Usage Maximum (0x14)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x04,       //     Report Count (4)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x09, 0x01,       //     Usage (0x01)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0xA1, 0x02,       //   Collection (Logical)
		0x75, 0x08,       //     Report Size (8)
		0x95, 0x07,       //     Report Count (7)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x02,       //     Usage (0x02)
		0x91, 0x02,       //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             //   End Collection
		0xC0,             // End Collection
	};

	BuzzState::BuzzState(u32 port_)
		: port(port_)
	{
	}

	BuzzState::~BuzzState() = default;

	static void buzz_handle_control(USBDevice* dev, USBPacket* p,
		int request, int value, int index, int length, uint8_t* data)
	{
		int ret = 0;

		switch (request)
		{
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				switch (value >> 8)
				{
					case USB_DT_REPORT:
						ret = sizeof(hid_report_descriptor);
						std::memcpy(data, hid_report_descriptor, ret);
						p->actual_length = ret;
						break;
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

	static void buzz_handle_data(USBDevice* dev, USBPacket* p)
	{
		BuzzState* s = USB_CONTAINER_OF(dev, BuzzState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (p->ep->nr == 1)
				{
					pxAssert(p->buffer_size >= sizeof(s->data));

					s->data.head1 = s->data.head2 = 0x7f;
					s->data.tail = 0xf;

					std::memcpy(p->buffer_ptr, &s->data, sizeof(s->data));

					p->actual_length += sizeof(s->data);
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

	static void buzz_unrealize(USBDevice* dev)
	{
		BuzzState* s = USB_CONTAINER_OF(dev, BuzzState, dev);
		delete s;
	}

	const char* BuzzDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Buzz Controller");
	}

	const char* BuzzDevice::TypeName() const
	{
		return "BuzzDevice";
	}

	bool BuzzDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		return true;
	}

	USBDevice* BuzzDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		BuzzState* s = new BuzzState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_descriptor, sizeof(config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = nullptr;
		s->dev.klass.handle_control = buzz_handle_control;
		s->dev.klass.handle_data = buzz_handle_data;
		s->dev.klass.unrealize = buzz_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		return &s->dev;

	fail:
		buzz_unrealize(&s->dev);
		return nullptr;
	}

	float BuzzDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		BuzzState* s = USB_CONTAINER_OF(dev, BuzzState, dev);
		switch (bind_index)
		{
			case CID_BUZZ_PLAYER1_RED:    return s->data.player1_red;
			case CID_BUZZ_PLAYER1_BLUE:   return s->data.player1_blue;
			case CID_BUZZ_PLAYER1_ORANGE: return s->data.player1_orange;
			case CID_BUZZ_PLAYER1_GREEN:  return s->data.player1_green;
			case CID_BUZZ_PLAYER1_YELLOW: return s->data.player1_yellow;
			case CID_BUZZ_PLAYER2_RED:    return s->data.player2_red;
			case CID_BUZZ_PLAYER2_BLUE:   return s->data.player2_blue;
			case CID_BUZZ_PLAYER2_ORANGE: return s->data.player2_orange;
			case CID_BUZZ_PLAYER2_GREEN:  return s->data.player2_green;
			case CID_BUZZ_PLAYER2_YELLOW: return s->data.player2_yellow;
			case CID_BUZZ_PLAYER3_RED:    return s->data.player3_red;
			case CID_BUZZ_PLAYER3_BLUE:   return s->data.player3_blue;
			case CID_BUZZ_PLAYER3_ORANGE: return s->data.player3_orange;
			case CID_BUZZ_PLAYER3_GREEN:  return s->data.player3_green;
			case CID_BUZZ_PLAYER3_YELLOW: return s->data.player3_yellow;
			case CID_BUZZ_PLAYER4_RED:    return s->data.player4_red;
			case CID_BUZZ_PLAYER4_BLUE:   return s->data.player4_blue;
			case CID_BUZZ_PLAYER4_ORANGE: return s->data.player4_orange;
			case CID_BUZZ_PLAYER4_GREEN:  return s->data.player4_green;
			case CID_BUZZ_PLAYER4_YELLOW: return s->data.player4_yellow;
			default:
				return 0.0f;
		}
	}

	void BuzzDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		BuzzState* s = USB_CONTAINER_OF(dev, BuzzState, dev);
		switch (bind_index)
		{
			case CID_BUZZ_PLAYER1_RED:    s->data.player1_red    = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER1_BLUE:   s->data.player1_blue   = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER1_ORANGE: s->data.player1_orange = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER1_GREEN:  s->data.player1_green  = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER1_YELLOW: s->data.player1_yellow = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER2_RED:    s->data.player2_red    = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER2_BLUE:   s->data.player2_blue   = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER2_ORANGE: s->data.player2_orange = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER2_GREEN:  s->data.player2_green  = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER2_YELLOW: s->data.player2_yellow = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER3_RED:    s->data.player3_red    = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER3_BLUE:   s->data.player3_blue   = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER3_ORANGE: s->data.player3_orange = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER3_GREEN:  s->data.player3_green  = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER3_YELLOW: s->data.player3_yellow = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER4_RED:    s->data.player4_red    = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER4_BLUE:   s->data.player4_blue   = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER4_ORANGE: s->data.player4_orange = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER4_GREEN:  s->data.player4_green  = (value >= 0.5f); break;
			case CID_BUZZ_PLAYER4_YELLOW: s->data.player4_yellow = (value >= 0.5f); break;
			default:
				break;
		}
	}

	std::span<const InputBindingInfo> BuzzDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"Red1", TRANSLATE_NOOP("USB", "Player 1 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER1_RED, GenericInputBinding::Circle},
			{"Blue1", TRANSLATE_NOOP("USB", "Player 1 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER1_BLUE, GenericInputBinding::R1},
			{"Orange1", TRANSLATE_NOOP("USB", "Player 1 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER1_ORANGE, GenericInputBinding::Cross},
			{"Green1", TRANSLATE_NOOP("USB", "Player 1 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER1_GREEN, GenericInputBinding::Triangle},
			{"Yellow1", TRANSLATE_NOOP("USB", "Player 1 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER1_YELLOW, GenericInputBinding::Square},

			{"Red2", TRANSLATE_NOOP("USB", "Player 2 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER2_RED, GenericInputBinding::Unknown},
			{"Blue2", TRANSLATE_NOOP("USB", "Player 2 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER2_BLUE, GenericInputBinding::Unknown},
			{"Orange2", TRANSLATE_NOOP("USB", "Player 2 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER2_ORANGE, GenericInputBinding::Unknown},
			{"Green2", TRANSLATE_NOOP("USB", "Player 2 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER2_GREEN, GenericInputBinding::Unknown},
			{"Yellow2", TRANSLATE_NOOP("USB", "Player 2 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER2_YELLOW, GenericInputBinding::Unknown},

			{"Red3", TRANSLATE_NOOP("USB", "Player 3 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER3_RED, GenericInputBinding::Unknown},
			{"Blue3", TRANSLATE_NOOP("USB", "Player 3 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER3_BLUE, GenericInputBinding::Unknown},
			{"Orange3", TRANSLATE_NOOP("USB", "Player 3 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER3_ORANGE, GenericInputBinding::Unknown},
			{"Green3", TRANSLATE_NOOP("USB", "Player 3 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER3_GREEN, GenericInputBinding::Unknown},
			{"Yellow3", TRANSLATE_NOOP("USB", "Player 3 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER3_YELLOW, GenericInputBinding::Unknown},

			{"Red4", TRANSLATE_NOOP("USB", "Player 4 Red"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER4_RED, GenericInputBinding::Unknown},
			{"Blue4", TRANSLATE_NOOP("USB", "Player 4 Blue"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER4_BLUE, GenericInputBinding::Unknown},
			{"Orange4", TRANSLATE_NOOP("USB", "Player 4 Orange"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER4_ORANGE, GenericInputBinding::Unknown},
			{"Green4", TRANSLATE_NOOP("USB", "Player 4 Green"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER4_GREEN, GenericInputBinding::Unknown},
			{"Yellow4", TRANSLATE_NOOP("USB", "Player 4 Yellow"), nullptr, InputBindingInfo::Type::Button, CID_BUZZ_PLAYER4_YELLOW, GenericInputBinding::Unknown},
		};

		return bindings;
	}

	std::span<const SettingInfo> BuzzDevice::Settings(u32 subtype) const
	{
		return {};
	}
} // namespace usb_pad
