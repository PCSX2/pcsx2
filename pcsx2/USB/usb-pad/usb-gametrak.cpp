// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-pad/usb-gametrak.h"
#include "USB/usb-pad/usb-pad.h"
#include <common/Console.h>

namespace usb_pad
{
	static const USBDescStrings desc_strings = {
		"",
		"In2Games Ltd.",
		"Game-Trak V1.3"};

	static uint8_t dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x10, 0x01, // bcdUSB 1.10
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0xB7, 0x14, // idVendor 0x14B7
		0x82, 0x09, // idProduct 0x0982
		0x01, 0x00, // bcdDevice 0.01
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
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
		0x0A,       // bMaxPower 20mA

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
		0x01, 0x01, // bcdHID 1.01
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x7A, 0x00, // wDescriptorLength[0] 122

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x10, 0x00, // wMaxPacketSize 16
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	static const uint8_t hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0x09, 0x01,       //   Usage (Pointer)
		0xA1, 0x00,       //   Collection (Physical)
		0x09, 0x30,       //     Usage (X)
		0x09, 0x31,       //     Usage (Y)
		0x09, 0x32,       //     Usage (Z)
		0x09, 0x33,       //     Usage (Rx)
		0x09, 0x34,       //     Usage (Ry)
		0x09, 0x35,       //     Usage (Rz)
		0x16, 0x00, 0x00, //     Logical Minimum (0)
		0x26, 0xFF, 0x0F, //     Logical Maximum (4095)
		0x36, 0x00, 0x00, //     Physical Minimum (0)
		0x46, 0xFF, 0x0F, //     Physical Maximum (4095)
		0x66, 0x00, 0x00, //     Unit (None)
		0x75, 0x10,       //     Report Size (16)
		0x95, 0x06,       //     Report Count (6)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0x09, 0x39,       //   Usage (Hat switch)
		0x15, 0x01,       //   Logical Minimum (1)
		0x25, 0x08,       //   Logical Maximum (8)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0x3B, 0x01, //   Physical Maximum (315)
		0x65, 0x14,       //   Unit (System: English Rotation, Length: Centimeter)
		0x75, 0x04,       //   Report Size (4)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x0C,       //   Usage Maximum (0x0C)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x0C,       //   Report Count (12)
		0x55, 0x00,       //   Unit Exponent (0)
		0x65, 0x00,       //   Unit (None)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x02,       //   Report Count (2)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x08,       //   Usage Page (LEDs)
		0x09, 0x43,       //   Usage (Slow Blink On Time)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x01,       //   Report Count (1)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x44,       //   Usage (Slow Blink Off Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x45,       //   Usage (Fast Blink On Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x46,       //   Usage (Fast Blink Off Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0xC0,             // End Collection
	};

	GametrakState::GametrakState(u32 port_)
		: port(port_)
	{
	}

	GametrakState::~GametrakState() = default;

	static u32 gametrak_compute_key(u32* key)
	{
		u32 ret = 0;
		ret  = *key <<  2 & 0xFC0000;
		ret |= *key << 17 & 0x020000;
		ret ^= *key << 16 & 0xFE0000;
		ret |= *key       & 0x010000;
		ret |= *key >>  9 & 0x007F7F;
		ret |= *key <<  7 & 0x008080;
		*key = ret;
		return ret >> 16;
	};

	static void gametrak_handle_control(USBDevice* dev, USBPacket* p,
		int request, int value, int index, int length, uint8_t* data)
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);
		int ret = 0;

		switch (request)
		{
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				switch (value >> 8)
				{
					case USB_DT_REPORT:
						ret = sizeof(hid_report_descriptor);
						memcpy(data, hid_report_descriptor, ret);
						break;
				}
				break;
			case SET_REPORT:
			{
				constexpr u8 secret[] = "Gametrak";
				if (length == 8 && std::memcmp(data, secret, sizeof(secret)) == 0)
				{
					s->state = 0;
					s->key = 0;
				}
				else if (length == 2)
				{
					if (data[0] == 0x45)
					{
						s->key = data[1] << 16;
					}

					if ((s->key >> 16) == data[1])
					{
						gametrak_compute_key(&s->key);
					}
					else
					{
						ERROR_LOG("gametrak error : own key = {}, recv key = {}", s->key >> 16, data[1]);
					}
				}
				break;
			}
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

	static void gametrak_handle_data(USBDevice* dev, USBPacket* p)
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (p->ep->nr == 1)
				{
					pxAssert(p->buffer_size >= sizeof(s->data));

					if (s->state == 0)
					{
						s->state = 1;
						constexpr u8 secret[] = "Gametrak\0\0\0\0\0\0\0\0";
						std::memcpy(p->buffer_ptr, secret, sizeof(secret));
					}
					else
					{
						s->data.k1 = s->key >> 16 & 1;
						s->data.k2 = s->key >> 17 & 1;
						s->data.k3 = s->key >> 18 & 1;
						s->data.k4 = s->key >> 19 & 1;
						s->data.k5 = s->key >> 20 & 1;
						s->data.k6 = s->key >> 21 & 1;

						auto time = std::chrono::steady_clock::now();
						if (std::chrono::duration_cast<std::chrono::milliseconds>(time - s->last_log).count() > 500)
						{
							Log::Write(LOGLEVEL_INFO, Color_Green, "{} LX={} LY={} LZ={} RX={} RY={} RZ={} Btn={}",
								__FUNCTION__,
								(u16)s->data.left_x, (u16)s->data.left_y, (u16)s->data.left_z,
								(u16)s->data.right_x, (u16)s->data.right_y, (u16)s->data.right_z,
								(u8)s->data.button);
							s->last_log = time;
						}

						std::memcpy(p->buffer_ptr, &s->data, sizeof(s->data));
					}

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

	static void gametrak_unrealize(USBDevice* dev)
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);
		delete s;
	}

	const char* GametrakDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Gametrak Device");
	}

	const char* GametrakDevice::TypeName() const
	{
		return "Gametrak";
	}

	USBDevice* GametrakDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		GametrakState* s = new GametrakState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_descriptor, sizeof(config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = nullptr;
		s->dev.klass.handle_control = gametrak_handle_control;
		s->dev.klass.handle_data = gametrak_handle_data;
		s->dev.klass.unrealize = gametrak_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		return &s->dev;

	fail:
		gametrak_unrealize(&s->dev);
		return nullptr;
	}

	bool GametrakDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);

		if (!sw.DoMarker("GametrakDevice"))
			return false;

		sw.Do(&s->state);
		sw.Do(&s->key);
		return !sw.HasError();
	}

	float GametrakDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);
		if (bind_index == CID_GT_BUTTON)
			return s->data.button;
		return 0.0f;
	}

	void GametrakDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);
		switch (bind_index)
		{
			case CID_GT_BUTTON:
				s->data.button = (value >= 0.5f);
				break;
			case CID_GT_LEFT_X:
				s->data.left_x = static_cast<u16>(std::clamp<long>(std::lroundf(value * 2047.f), 0, 2047));
				s->data.left_x = s->invert_x_axis ? 2047 - s->data.left_x : s->data.left_x;
				break;
			case CID_GT_LEFT_Y:
				s->data.left_y = static_cast<u16>(std::clamp<long>(std::lroundf(value * 2047.f), 0, 2047));
				s->data.left_y = s->invert_y_axis ? 2047 - s->data.left_y : s->data.left_y;
				break;
			case CID_GT_LEFT_Z:
				s->data.left_z = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->limit_z_axis), 0, s->limit_z_axis));
				s->data.left_z = s->invert_z_axis ? s->limit_z_axis - s->data.left_z : s->data.left_z;
				break;
			case CID_GT_RIGHT_X:
				s->data.right_x = static_cast<u16>(std::clamp<long>(std::lroundf(value * 2047.f), 0, 2047));
				s->data.right_x = s->invert_x_axis ? 2047 - s->data.right_x : s->data.right_x;
				break;
			case CID_GT_RIGHT_Y:
				s->data.right_y = static_cast<u16>(std::clamp<long>(std::lroundf(value * 2047.f), 0, 2047));
				s->data.right_y = s->invert_y_axis ? 2047 - s->data.right_y : s->data.right_y;
				break;
			case CID_GT_RIGHT_Z:
				s->data.right_z = static_cast<u32>(std::clamp<long>(std::lroundf(value * s->limit_z_axis), 0, s->limit_z_axis));
				s->data.right_z = s->invert_z_axis ? s->limit_z_axis - s->data.right_z : s->data.right_z;
				break;
			default:
				break;
		}
	}

	void GametrakDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		GametrakState* s = USB_CONTAINER_OF(dev, GametrakState, dev);
		s->invert_x_axis = USB::GetConfigBool(si, s->port, TypeName(), "invert_x_axis", false);
		s->invert_y_axis = USB::GetConfigBool(si, s->port, TypeName(), "invert_y_axis", false);
		s->invert_z_axis = USB::GetConfigBool(si, s->port, TypeName(), "invert_z_axis", false);
		s->limit_z_axis = USB::GetConfigInt(si, s->port, TypeName(), "limit_z_axis", 4095);
	}

	std::span<const InputBindingInfo> GametrakDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"FootPedal", TRANSLATE_NOOP("USB", "Foot Pedal"), nullptr, InputBindingInfo::Type::Button, CID_GT_BUTTON, GenericInputBinding::Cross},
			{"LeftX", TRANSLATE_NOOP("USB", "Left X"), nullptr, InputBindingInfo::Type::Axis, CID_GT_LEFT_X, GenericInputBinding::Unknown},
			{"LeftY", TRANSLATE_NOOP("USB", "Left Y"), nullptr, InputBindingInfo::Type::Axis, CID_GT_LEFT_Y, GenericInputBinding::Unknown},
			{"LeftZ", TRANSLATE_NOOP("USB", "Left Z"), nullptr, InputBindingInfo::Type::Axis, CID_GT_LEFT_Z, GenericInputBinding::Unknown},
			{"RightX", TRANSLATE_NOOP("USB", "Right X"), nullptr, InputBindingInfo::Type::Axis, CID_GT_RIGHT_X, GenericInputBinding::Unknown},
			{"RightY", TRANSLATE_NOOP("USB", "Right Y"), nullptr, InputBindingInfo::Type::Axis, CID_GT_RIGHT_Y, GenericInputBinding::Unknown},
			{"RightZ", TRANSLATE_NOOP("USB", "Right Z"), nullptr, InputBindingInfo::Type::Axis, CID_GT_RIGHT_Z, GenericInputBinding::Unknown},
		};

		return bindings;
	}

	std::span<const SettingInfo> GametrakDevice::Settings(u32 subtype) const
	{
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::Boolean, "invert_x_axis", TRANSLATE_NOOP("USB", "Invert X axis"), TRANSLATE_NOOP("USB", "Invert X axis"), "false"},
			{SettingInfo::Type::Boolean, "invert_y_axis", TRANSLATE_NOOP("USB", "Invert Y axis"), TRANSLATE_NOOP("USB", "Invert Y axis"), "false"},
			{SettingInfo::Type::Boolean, "invert_z_axis", TRANSLATE_NOOP("USB", "Invert Z axis"), TRANSLATE_NOOP("USB", "Invert Z axis"), "false"},
			{SettingInfo::Type::Integer, "limit_z_axis", TRANSLATE_NOOP("USB", "Limit Z axis [100-4095]"),
				TRANSLATE_NOOP("USB", "- 4095 for original Gametrak controllers\n- 1790 for standard gamepads"),
				"4095", "100", "4095", "1", TRANSLATE_NOOP("USB", "%d"), nullptr, nullptr, 1.0f},
		};
		return info;
	}
} // namespace usb_pad
