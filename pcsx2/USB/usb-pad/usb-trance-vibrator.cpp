// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-pad/usb-trance-vibrator.h"
#include "USB/usb-pad/usb-pad.h"
#include <common/Console.h>

namespace usb_pad
{
	static const USBDescStrings desc_strings = {
		"",
		"ASCII CORPORATION",
		"ASCII Vib"};

	static uint8_t dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x01, // bcdUSB 1.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0x49, 0x0B, // idVendor 0x0B49
		0x4F, 0x06, // idProduct 0x064F
		0x00, 0x01, // bcdDevice 2.00
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
		0x31,       // bMaxPower 98mA

		0x09,       // bLength
		0x04,       // bDescriptorType (Interface)
		0x00,       // bInterfaceNumber 0
		0x00,       // bAlternateSetting
		0x01,       // bNumEndpoints 1
		0x00,       // bInterfaceClass
		0x00,       // bInterfaceSubClass
		0x00,       // bInterfaceProtocol
		0x00,       // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	TranceVibratorState::TranceVibratorState(u32 port_)
		: port(port_)
	{
	}

	TranceVibratorState::~TranceVibratorState() = default;

	static void trancevibrator_handle_control(USBDevice* dev, USBPacket* p,
		int request, int value, int index, int length, uint8_t* data)
	{
		TranceVibratorState* s = USB_CONTAINER_OF(dev, TranceVibratorState, dev);
		int ret = 0;

		switch (request)
		{
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				break;
			case SET_IDLE:
				break;
			case VendorDeviceOutRequest:
				// Vibration = wValue
				// LED1 = wIndex&1, LED2 = wIndex&2, LED3 = wIndex&4
				InputManager::SetUSBVibrationIntensity(s->port, value & 0xff, 0);
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

	static void trancevibrator_handle_data(USBDevice* dev, USBPacket* p)
	{
		switch (p->pid)
		{
			case USB_TOKEN_IN:
				break;
			case USB_TOKEN_OUT:
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void trancevibrator_unrealize(USBDevice* dev)
	{
		TranceVibratorState* s = USB_CONTAINER_OF(dev, TranceVibratorState, dev);
		delete s;
	}

	const char* TranceVibratorDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Trance Vibrator (Rez)");
	}

	const char* TranceVibratorDevice::TypeName() const
	{
		return "TranceVibrator";
	}

	bool TranceVibratorDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		return true;
	}

	USBDevice* TranceVibratorDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		TranceVibratorState* s = new TranceVibratorState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_descriptor, sizeof(config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = nullptr;
		s->dev.klass.handle_control = trancevibrator_handle_control;
		s->dev.klass.handle_data = trancevibrator_handle_data;
		s->dev.klass.unrealize = trancevibrator_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);

		return &s->dev;

	fail:
		trancevibrator_unrealize(&s->dev);
		return nullptr;
	}

	float TranceVibratorDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		return 0.0f;
	}

	void TranceVibratorDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
	}

	std::span<const InputBindingInfo> TranceVibratorDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"Motor", TRANSLATE_NOOP("Pad", "Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::LargeMotor},
		};

		return bindings;
	}

	std::span<const SettingInfo> TranceVibratorDevice::Settings(u32 subtype) const
	{
		return {};
	}
} // namespace usb_pad
