// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "USB/deviceproxy.h"
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"

namespace usb_pad
{
	enum TrainDeviceTypes
	{
		TRAIN_TYPE2, // TCPP-20009 or similar
		TRAIN_SHINKANSEN, // TCPP-20011
		TRAIN_RYOJOUHEN, // TCPP-20014
		TRAIN_MASCON, // COTM-02001
		MASTER_CONTROLLER, // VOK-00105 or VOK-00106 with OGCW-10001 adapter
	};

	class TrainDevice final : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;
		const char* IconName() const override;
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
		u8 horn; // pedal
		u8 hat;
		u8 buttons;
	};
	static_assert(sizeof(TrainConData_Type2) == 6);

	struct TrainConData_Shinkansen
	{
		u8 brake;
		u8 power;
		u8 horn; // pedal
		u8 hat;
		u8 buttons;
		u8 pad;
	};
	static_assert(sizeof(TrainConData_Shinkansen) == 6);

	struct TrainConData_Ryojouhen
	{
		u8 brake;
		u8 power;
		u8 horn; // pedal
		u8 hat;
		u8 buttons;
		u8 pad[3];
	};
	static_assert(sizeof(TrainConData_Ryojouhen) == 8);

	struct TrainConData_TrainMascon
	{
		u8 one;

		u8 handle : 4;
		u8 reverser : 4;

		u8 ats : 1;
		u8 close : 1;
		u8 button_a_soft : 1;
		u8 button_a_hard : 1;
		u8 button_b : 1;
		u8 button_c : 1;
		u8 : 2;

		u8 start : 1;
		u8 select : 1;
		u8 dpad_up : 1;
		u8 dpad_down : 1;
		u8 dpad_left : 1;
		u8 dpad_right : 1;
		u8 : 2;
	};
	static_assert(sizeof(TrainConData_TrainMascon) == 4);
#pragma pack(pop)

	struct TrainDeviceState
	{
		TrainDeviceState(u32 port_, TrainDeviceTypes type_);
		~TrainDeviceState();

		void Reset();
		void UpdateHatSwitch() noexcept;
		void UpdateHandles(u8 max_power, u8 max_brake);

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
			u16 buttons; // active high
		} data = {};

		// Master Controller
		const char* mc_handle[16] = {"TSB20", "TSB30", "TSB40", "TSE99", "TSA05", "TSA15", "TSA25", "TSA35", "TSA45", "TSA50", "TSA55", "TSA65", "TSA75", "TSA85", "TSA95", "TSB60"};
		const char* mc_reverser[3] = {"TSG00", "TSG50", "TSG99"};
		const char* mc_button_pressed[4] = {"TSY99", "TSX99", "TSZ99", "TSK99"};
		const char* mc_button_released[4] = {"TSY00", "TSX00", "TSZ00", "TSK00"};
		u8 power_notches;
		u8 brake_notches;

		u16 prev_buttons;
		s8 last_handle = -1, handle = 0;
		s8 last_reverser = -1, reverser = 1;
	};

	// Taito Densha Controllers as described at:
	// https://traincontrollerdb.marcriera.cat/hardware/#usb
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

	// ---- Train Mascon ----

	static const uint8_t train_mascon_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass
		0x00,        // bDeviceProtocol
		0x08,        // bMaxPacketSize0 8
		0x06, 0x1C,  // idVendor 0x1C06
		0xA7, 0x77,  // idProduct 0x77A7
		0x02, 0x02,  // bcdDevice 2.02
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x03,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};

	static const uint8_t train_mascon_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x19, 0x00,  // wTotalLength 25
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x04,        // iConfiguration (String Index)
		0xA0,        // bmAttributes Remote Wakeup
		0x32,        // bMaxPower 100mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x01,        // bNumEndpoints 1
		0x00,        // bInterfaceClass
		0x00,        // bInterfaceSubClass
		0x00,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x81,        // bEndpointAddress (IN/D2H)
		0x03,        // bmAttributes (Interrupt)
		0x08, 0x00,  // wMaxPacketSize 8
		0x14,        // bInterval 20 (unit depends on device speed)
	};

	// ---- Master Controller ----
	// Implements a generic PL2303 adapter.
	// Replace with official OGCW-10001 descriptors when available.

	static const uint8_t master_controller_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass
		0x00,        // bDeviceProtocol
		0x40,        // bMaxPacketSize0 64
		0x7B, 0x06,  // idVendor 0x067B
		0x03, 0x23,  // idProduct 0x2303
		0x00, 0x03,  // bcdDevice 3.00
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x00,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};

	static const uint8_t master_controller_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x27, 0x00,  // wTotalLength 39
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x00,        // iConfiguration (String Index)
		0x80,        // bmAttributes
		0x32,        // bMaxPower 100mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x03,        // bNumEndpoints 3
		0xFF,        // bInterfaceClass
		0x00,        // bInterfaceSubClass
		0x00,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x81,        // bEndpointAddress (IN/D2H)
		0x03,        // bmAttributes (Interrupt)
		0x0A, 0x00,  // wMaxPacketSize 10
		0x01,        // bInterval 1 (unit depends on device speed)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x02,        // bEndpointAddress (OUT/H2D)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x83,        // bEndpointAddress (IN/D2H)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)
	};

} // namespace usb_pad
