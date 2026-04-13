// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "USB/deviceproxy.h"
#include "USB/qemu-usb/desc.h"

namespace usb_modem
{
	// Omron ME56PS2 USB Modem descriptors
	// Vendor ID: 0x0590 (Omron), Product ID: 0x001a
	// Internal chip: FTDI FT232-series USB-to-serial converter
	// Interface class: 0xFF/0xFF/0xFF (Vendor Specific)

	static const uint8_t me56ps2_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass
		0x00,        // bDeviceSubClass
		0x00,        // bDeviceProtocol
		0x40,        // bMaxPacketSize0 64 (matches me56ps2-emulator; original HW uses 8)
		0x90, 0x05,  // idVendor 0x0590 (Omron)
		0x1A, 0x00,  // idProduct 0x001A
		0x01, 0x01,  // bcdDevice 1.01 (matches me56ps2-emulator)
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x03,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};

	static const uint8_t me56ps2_config_descriptor[] = {
		// Configuration Descriptor
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x20, 0x00,  // wTotalLength 32
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x02,        // iConfiguration (String Index) — matches me56ps2-emulator
		0xA0,        // bmAttributes (Bus Powered + Remote Wakeup) — me56ps2-emulator uses 0x20
		0x1E,        // bMaxPower 60mA

		// Interface Descriptor
		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x02,        // bNumEndpoints 2
		0xFF,        // bInterfaceClass (Vendor Specific)
		0xFF,        // bInterfaceSubClass
		0xFF,        // bInterfaceProtocol
		0x02,        // iInterface (String Index)

		// Endpoint Descriptor - Bulk IN (Device to Host)
		// me56ps2-emulator lists IN endpoint FIRST, then OUT.
		// IOP FTDI driver may expect this order.
		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x82,        // bEndpointAddress (IN/D2H, EP2)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0

		// Endpoint Descriptor - Bulk OUT (Host to Device)
		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x02,        // bEndpointAddress (OUT/H2D, EP2)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0
	};

	class ModemDevice final : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;
		const char* IconName() const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
	};

} // namespace usb_modem
