// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "USB/deviceproxy.h"
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"

namespace usb_pad
{
	enum TrainDeviceTypes
	{
		TRAIN_TYPE2, // TCPP20009 or similar
		TRAIN_SHINKANSEN, // TCPP20011
		TRAIN_RYOJOUHEN, // TCPP20014
		TRAIN_COUNT,
	};

	class TrainDevice final : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;
		std::span<const char*> SubTypes() const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
	};

#pragma pack(push, 1)
	struct TrainConData_Type2
	{
		u8 control;
		u8 brake;
		u8 power;
		u8 horn;
		u8 hat;
		u8 buttons;
	};
	static_assert(sizeof(TrainConData_Type2) == 6);

	struct TrainConData_Shinkansen
	{
		u8 brake;
		u8 power;
		u8 horn;
		u8 hat;
		u8 buttons;
		u8 pad;
	};
	static_assert(sizeof(TrainConData_Shinkansen) == 6);

	struct TrainConData_Ryojouhen
	{
		u8 brake;
		u8 power;
		u8 horn;
		u8 hat;
		u8 buttons;
		u8 pad[3];
	};
	static_assert(sizeof(TrainConData_Ryojouhen) == 8);
#pragma pack(pop)

	struct TrainDeviceState
	{
		TrainDeviceState(u32 port_, TrainDeviceTypes type_);
		~TrainDeviceState();

		void Reset();
		void UpdateHatSwitch() noexcept;

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		TrainDeviceTypes type = TRAIN_TYPE2;
		bool passthrough = false;

		struct
		{
			// intermediate state, resolved at query time
			bool hat_left : 1;
			bool hat_right : 1;
			bool hat_up : 1;
			bool hat_down : 1;

			u8 power; // 255 is fully applied
			u8 brake; // 255 is fully applied
			u8 hatswitch; // direction
			u8 buttons; // active high
		} data = {};
	};

	// Taito Densha Controllers as described at:
	// https://marcriera.github.io/ddgo-controller-docs/controllers/usb/
#define DEFINE_DCT_DEV_DESCRIPTOR(prefix, subclass, product) \
	static const uint8_t prefix##_dev_descriptor[] = { \
		/* bLength             */ USB_DEVICE_DESC_SIZE, \
		/* bDescriptorType     */ USB_DEVICE_DESCRIPTOR_TYPE, \
		/* bcdUSB              */ WBVAL(0x0110), /* USB 1.1 */ \
		/* bDeviceClass        */ 0xFF, \
		/* bDeviceSubClass     */ subclass, \
		/* bDeviceProtocol     */ 0x00, \
		/* bMaxPacketSize0     */ 0x08, \
		/* idVendor            */ WBVAL(0x0ae4), \
		/* idProduct           */ WBVAL(product), \
		/* bcdDevice           */ WBVAL(0x0102), /* 1.02 */ \
		/* iManufacturer       */ 0x01, \
		/* iProduct            */ 0x02, \
		/* iSerialNumber       */ 0x03, \
		/* bNumConfigurations  */ 0x01, \
	}

	// These settings are common across multiple models.
	static const uint8_t taito_denshacon_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE, // bLength
		USB_CONFIGURATION_DESCRIPTOR_TYPE, // bDescriptorType
		WBVAL(25), // wTotalLength
		0x01, // bNumInterfaces
		0x01, // bConfigurationValue
		0x00, // iConfiguration (String Index)
		0xA0, // bmAttributes
		0xFA, // bMaxPower 500mA

		USB_INTERFACE_DESC_SIZE, // bLength
		USB_INTERFACE_DESCRIPTOR_TYPE, // bDescriptorType
		0x00, // bInterfaceNumber
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints
		USB_CLASS_HID, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		USB_ENDPOINT_DESC_SIZE, // bLength
		USB_ENDPOINT_DESCRIPTOR_TYPE, // bDescriptorType
		USB_ENDPOINT_IN(1), // bEndpointAddress (IN/D2H)
		USB_ENDPOINT_TYPE_INTERRUPT, // bmAttributes (Interrupt)
		WBVAL(8), // wMaxPacketSize
		0x14, // bInterval 20 (unit depends on device speed)
		// 25 bytes (43 total with dev descriptor)
	};

	// ---- Two handle controller "Type 2" ----

	static const USBDescStrings dct01_desc_strings = {
		"",
		"TAITO",
		"TAITO_DENSYA_CON_T01",
		"TCPP20009",
	};

	// dct01_dev_descriptor
	DEFINE_DCT_DEV_DESCRIPTOR(dct01, 0x04, 0x0004);

	// ---- Shinkansen controller ----

	static const USBDescStrings dct02_desc_strings = {
		"",
		"TAITO",
		"TAITO_DENSYA_CON_T02",
		"TCPP20011",
	};

	// dct02_dev_descriptor
	DEFINE_DCT_DEV_DESCRIPTOR(dct02, 0x05, 0x0005);

	// ---- Ryojouhen controller ----

	static const USBDescStrings dct03_desc_strings = {
		"",
		"TAITO",
		"TAITO_DENSYA_CON_T03",
		"TCPP20014",
	};

	// dct03_dev_descriptor
	DEFINE_DCT_DEV_DESCRIPTOR(dct03, 0xFF, 0x0007);

} // namespace usb_pad
