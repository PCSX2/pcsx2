// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "USB/deviceproxy.h"
#include "USB/qemu-usb/desc.h"

#define GET_DEVICE_ID   0
#define GET_PORT_STATUS 1
#define SOFT_RESET      2

#define GET_PORT_STATUS_PAPER_EMPTY      1<<5
#define GET_PORT_STATUS_PAPER_NOT_EMPTY  0<<5
#define GET_PORT_STATUS_SELECTED         1<<4
#define GET_PORT_STATUS_NOT_SELECTED     0<<4
#define GET_PORT_STATUS_NO_ERROR         1<<3
#define GET_PORT_STATUS_ERROR            0<<3

namespace usb_printer
{
	static const uint8_t dpp_mp1_dev_desciptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass
		0x00,        // bDeviceProtocol
		0x08,        // bMaxPacketSize0 8
		0x4C, 0x05,  // idVendor 0x054C
		0x65, 0x00,  // idProduct 0x0065
		0x04, 0x02,  // bcdDevice 2.04
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x00,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};
	static int dpp_mp1_dev_desciptor_size = sizeof(dpp_mp1_dev_desciptor);

	static const uint8_t dpp_mp1_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x20, 0x00,  // wTotalLength 32
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x00,        // iConfiguration (String Index)
		0xC0,        // bmAttributes Self Powered
		0x00,        // bMaxPower 0mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x02,        // bNumEndpoints 2
		0x07,        // bInterfaceClass
		0x01,        // bInterfaceSubClass
		0x02,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x01,        // bEndpointAddress (OUT/H2D)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x82,        // bEndpointAddress (IN/D2H)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)
	};
	static int dpp_mp1_config_descriptor_size = sizeof(dpp_mp1_config_descriptor);

	enum PrinterModel
	{
		Sony_DPP_MP1,
	};

	enum PrinterProtocol
	{
		ProtocolSonyUPD,
	};

	struct PrinterData
	{
		const PrinterModel model;
		const char* commercial_name;
		const uint8_t* device_descriptor;
		const int device_descriptor_size;
		const uint8_t* config_descriptor;
		const int config_descriptor_size;
		const USBDescStrings usb_strings;
		const char* device_id;
		const PrinterProtocol protocol;
	};

	static const PrinterData sPrinters[] = {
		{
			Sony_DPP_MP1,
			"Sony DPP-MP1",
			dpp_mp1_dev_desciptor, dpp_mp1_dev_desciptor_size,
			dpp_mp1_config_descriptor, dpp_mp1_config_descriptor_size,
			{"", "SONY", "USB printer"},
			"MFG:SONY;MDL:DPP-MP1;DES:SONYDPP-MP1;CMD:SONY-Original;CLS:PRINTER",
			ProtocolSonyUPD,
		},
	};

	class PrinterDevice final : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;

		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		std::span<const char*> SubTypes() const override;
	};

#pragma pack(push, 1)
	struct BMPHeader
	{
		uint16_t magic;
		uint32_t filesize;
		uint32_t reserved;
		uint32_t data_offset;
		uint32_t core_header_size;
		uint16_t width;
		uint16_t height;
		uint16_t planes;
		uint16_t bpp;
	};
#pragma pack(pop)

} // namespace usb_printer
