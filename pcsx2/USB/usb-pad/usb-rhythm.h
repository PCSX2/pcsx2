// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"
#include "USB/deviceproxy.h"

namespace usb_pad
{
	enum RhythmType
	{
		ROCKBAND1_DRUMKIT,
		KEYBOARDMANIA_CONTROLLER,
		PARA_PARA_PARADISE,
		DANCE_UK_XL,
		DANCING_WITH_THE_STARS,
	};

	class RhythmDevice final : public DeviceProxy
	{
		enum RockBand_ControlID
		{
			RB1_BLUE,
			RB1_GREEN,
			RB1_RED,
			RB1_YELLOW,
			RB1_ORANGE,
			RB1_SELECT,
			RB1_START,
			RB1_UP,
			RB1_RIGHT,
			RB1_DOWN,
			RB1_LEFT,
		};

		enum KBMania_ControlID
		{
			KBM_C1, KBM_CS1,
			KBM_D1, KBM_DS1,
			KBM_E1, 
			KBM_F1, KBM_FS1,
			KBM_G1, KBM_GS1,
			KBM_A1, KBM_AS1,
			KBM_B1,
			KBM_C2, KBM_CS2,
			KBM_D2, KBM_DS2,
			KBM_E2,
			KBM_F2, KBM_FS2,
			KBM_G2, KBM_GS2,
			KBM_A2, KBM_AS2,
			KBM_B2,
			KBM_START,
			KBM_SELECT,
			KBM_UP,
			KBM_DOWN,
		};
		
		enum Para_Para_Paradise_ControlID
		{
			PPP_LEFT,
			PPP_UP_LEFT,
			PPP_UP,
			PPP_UP_RIGHT,
			PPP_RIGHT,
			PPP_MENU_LEFT,
			PPP_MENU_RIGHT,
			PPP_SELECT,
			PPP_START,
		};

		enum DanceMat_ControlID
		{
			DM_UP,
			DM_DOWN,
			DM_LEFT,
			DM_RIGHT,
			DM_CROSS,
			DM_CIRCLE,
			DM_TRIANGLE,
			DM_SQUARE,
			DM_SELECT,
			DM_START,
			DM_BTN_1,
			DM_BTN_2,
			DM_BTN_3,
			DM_BTN_4,
			DM_BTN_5,
			DM_BTN_6,
			DM_BTN_7,
			DM_BTN_8,
		};

	public:
		const char* Name() const override;
		const char* TypeName() const override;
		const char* IconName() const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		std::span<const char*> SubTypes() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
	};

	struct RhythmState
	{
		RhythmState(u32 port_, RhythmType type_);
		~RhythmState();
		
		u8 UpdateHatSwitch() noexcept;

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		RhythmType type = ROCKBAND1_DRUMKIT;

		bool hat_up, hat_down, hat_left, hat_right;
		bool btn1, btn2, btn3, btn4;

		#pragma pack(push, 1)
		union
		{
			// RockBand
			struct
			{
				u8 blue : 1;
				u8 green : 1;
				u8 red : 1;
				u8 yellow : 1;
				u8 orange : 1;
				u8 : 3;

				u8 select : 1;
				u8 start : 1;
				u8 : 6;

				u8 dpad : 4;
				u8 : 4;
			} rb1;
			// KeyboardMania
			struct
			{
				u8 head;

				u8 c1 : 1;
				u8 cs1 : 1;
				u8 d1 : 1;
				u8 ds1 : 1;
				u8 e1 : 1;
				u8 f1 : 1;
				u8 fs1 : 1;
				u8 : 1;

				u8 g1 : 1;
				u8 gs1 : 1;
				u8 a1 : 1;
				u8 as1 : 1;
				u8 b1 : 1;
				u8 c2 : 1;
				u8 select : 1;
				u8 : 1;

				u8 cs2 : 1;
				u8 d2 : 1;
				u8 ds2 : 1;
				u8 e2 : 1;
				u8 f2 : 1;
				u8 fs2 : 1;
				u8 start : 1;
				u8 : 1;

				u8 g2 : 1;
				u8 gs2 : 1;
				u8 a2 : 1;
				u8 as2 : 1;
				u8 b2 : 1;
				u8 up : 1;
				u8 down : 1;
				u8 : 1;
			} kbm;
			// Para Para Paradise
			struct
			{
				u8 btn_right : 1;
				u8 btn_up_right : 1;
				u8 btn_up : 1;
				u8 btn_up_left : 1;
				u8 btn_left : 1;
				u8 : 3;

				u8 menu_right : 1;
				u8 start : 1;
				u8 select : 1;
				u8 menu_left : 1;
				u8 : 4;

				u16 : 16;
				u32 : 32;
			} ppp;
			// Dance:UK XL
			struct
			{
				u8 btn_3_4;
				u8 btn_1_2;

				u8 cross : 1;
				u8 circle : 1;
				u8 triangle : 1;
				u8 square : 1;
				u8 start : 1;
				u8 select : 1;
				u8 up : 1;
				u8 down : 1;

				u8 left: 1;
				u8 right : 1;
				u8 btn_5 : 1;
				u8 btn_6 : 1;
				u8 btn_7 : 1;
				u8 btn_8 : 1;
				u8 : 2;

				u32 : 32;
			} duk;
			// Dancing With the Stars
			struct
			{
				u8 : 1;
				u8 down : 1;
				u8 triangle : 1;
				u8 left: 1;
				u8 cross : 1;
				u8 up : 1;
				u8 : 2;

				u8 : 1;
				u8 right : 1;
				u8 : 2;
				u8 start : 1;
				u8 : 3;
			} dwts;
		};
		#pragma pack(pop)
	};
	
	// Wii Rock Band drum kit
	// Should be usb 2.0, but seems to make no difference with Rock Band games
	static const uint8_t rb1_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0110), //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x40,          //(64)
		/* idVendor            */ WBVAL(0x12ba),
		/* idProduct           */ WBVAL(0x0210),
		/* bcdDevice           */ WBVAL(0x1000), //(10.00)
		/* iManufacturer       */ 0x01,          //(1)
		/* iProduct            */ 0x02,          //(2)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	static const uint8_t rb1_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x29, 0x00, // wTotalLength 41
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x32,       // bMaxPower 100mA

		0x09,       // bLength
		0x04,       // bDescriptorType (Interface)
		0x00,       // bInterfaceNumber 0
		0x00,       // bAlternateSetting
		0x02,       // bNumEndpoints 2
		0x03,       // bInterfaceClass
		0x00,       // bInterfaceSubClass
		0x00,       // bInterfaceProtocol
		0x00,       // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x11, 0x01, // bcdHID 1.11
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x89, 0x00, // wDescriptorLength[0] 137

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x02,       // bEndpointAddress (OUT/H2D)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)
					// 41 bytes
	};

	static const uint8_t rb1_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x05,       // Usage (Game Pad)
		0xA1, 0x01,       // Collection (Application)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x35, 0x00,       //   Physical Minimum (0)
		0x45, 0x01,       //   Physical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x0D,       //   Report Count (13)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x0D,       //   Usage Maximum (0x0D)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x03,       //   Report Count (3)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,       //   Usage Page (Generic Desktop Ctrls)
		0x25, 0x07,       //   Logical Maximum (7)
		0x46, 0x3B, 0x01, //   Physical Maximum (315)
		0x75, 0x04,       //   Report Size (4)
		0x95, 0x01,       //   Report Count (1)
		0x65, 0x14,       //   Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,       //   Usage (Hat switch)
		0x81, 0x42,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x65, 0x00,       //   Unit (None)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x09, 0x30,       //   Usage (X)
		0x09, 0x31,       //   Usage (Y)
		0x09, 0x32,       //   Usage (Z)
		0x09, 0x35,       //   Usage (Rz)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x20,       //   Usage (0x20)
		0x09, 0x21,       //   Usage (0x21)
		0x09, 0x22,       //   Usage (0x22)
		0x09, 0x23,       //   Usage (0x23)
		0x09, 0x24,       //   Usage (0x24)
		0x09, 0x25,       //   Usage (0x25)
		0x09, 0x26,       //   Usage (0x26)
		0x09, 0x27,       //   Usage (0x27)
		0x09, 0x28,       //   Usage (0x28)
		0x09, 0x29,       //   Usage (0x29)
		0x09, 0x2A,       //   Usage (0x2A)
		0x09, 0x2B,       //   Usage (0x2B)
		0x95, 0x0C,       //   Report Count (12)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x0A, 0x21, 0x26, //   Usage (0x2621)
		0x95, 0x08,       //   Report Count (8)
		0xB1, 0x02,       //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x0A, 0x21, 0x26, //   Usage (0x2621)
		0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x26, 0xFF, 0x03, //   Logical Maximum (1023)
		0x46, 0xFF, 0x03, //   Physical Maximum (1023)
		0x09, 0x2C,       //   Usage (0x2C)
		0x09, 0x2D,       //   Usage (0x2D)
		0x09, 0x2E,       //   Usage (0x2E)
		0x09, 0x2F,       //   Usage (0x2F)
		0x75, 0x10,       //   Report Size (16)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             // End Collection

		// 137 bytes
	};

	// Keyboardmania
	static const uint8_t kbm_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x10, 0x01, // bcdUSB 1.10
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0x07, 0x05, // idVendor 0x0507
		0x10, 0x00, // idProduct 0x0010
		0x00, 0x01, // bcdDevice 01.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t kbm_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x19,       // bMaxPower 50mA

		0x09,       // bLength
		0x04,       // bDescriptorType (Interface)
		0x00,       // bInterfaceNumber 0
		0x00,       // bAlternateSetting
		0x01,       // bNumEndpoints 1
		0x03,       // bInterfaceClass
		0x00,       // bInterfaceSubClass
		0x00,       // bInterfaceProtocol
		0x02,       // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x10, 0x01, // bcdHID 1.11
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x96, 0x00, // wDescriptorLength[0] 150

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x04,       // bInterval 4 (unit depends on device speed)
	};

	static const uint8_t kbm_hid_report_descriptor[] = {
		0x05, 0x01, // USAGE_PAGE (Generic Desktop)
		0x09, 0x05, // USAGE (Game Pad)
		0xA1, 0x01, // COLLECTION (Application)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x3A, //   USAGE_MINIMUM (Button 58)
		0x29, 0x3F, //   USAGE_MAXIMUM (Button 63)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x06, //   REPORT_COUNT (6)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x02, //   REPORT_COUNT (2)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x01, //   USAGE_MINIMUM (Button 1)
		0x29, 0x07, //   USAGE_MAXIMUM (Button 7)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x08, //   USAGE_MINIMUM (Button 8)
		0x29, 0x0E, //   USAGE_MAXIMUM (Button 14)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x0F, //   USAGE_MINIMUM (Button 15)
		0x29, 0x15, //   USAGE_MAXIMUM (Button 21)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x16, //   USAGE_MINIMUM (Button 22)
		0x29, 0x1C, //   USAGE_MAXIMUM (Button 28)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x01, //   USAGE_PAGE (Generic Desktop)
		0x09, 0x01, //   USAGE (Pointer)
		0xA1, 0x00, //   COLLECTION (Physical)
		0x09, 0x30, //     USAGE (X)
		0x09, 0x31, //     USAGE (Y)
		0x15, 0xFF, //     LOGICAL_MINIMUM (-1)
		0x25, 0x01, //     LOGICAL_MAXIMUM (1)
		0x95, 0x02, //     REPORT_COUNT (2)
		0x75, 0x02, //     REPORT_SIZE (2)
		0x81, 0x02, //     INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x95, 0x04, //     REPORT_COUNT (4)
		0x75, 0x01, //     REPORT_SIZE (1)
		0x81, 0x01, //     INPUT (Constant,Array,Absolute)
		0xC0,       //   END_COLLECTION
		0x75, 0x08, //   REPORT_SIZE (8)
		0x95, 0x02, //   REPORT_COUNT (2)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0xc0        // END_COLLECTION
	};

	// Para Para Paradise
	static const uint8_t ppp_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass 
		0x00,        // bDeviceProtocol 
		0x08,        // bMaxPacketSize0 8
		0x07, 0x05,  // idVendor 0x0507
		0x11, 0x00,  // idProduct 0x0011
		0x00, 0x01,  // bcdDevice 1.00
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x00,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};
	
	static const uint8_t ppp_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x22, 0x00,  // wTotalLength 34
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x00,        // iConfiguration (String Index)
		0x80,        // bmAttributes
		0xC8,        // bMaxPower 400mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x01,        // bNumEndpoints 1
		0x03,        // bInterfaceClass
		0x00,        // bInterfaceSubClass
		0x00,        // bInterfaceProtocol
		0x02,        // iInterface (String Index)

		0x09,        // bLength
		0x21,        // bDescriptorType (HID)
		0x10, 0x01,  // bcdHID 1.10
		0x00,        // bCountryCode
		0x01,        // bNumDescriptors
		0x22,        // bDescriptorType[0] (HID)
		0x52, 0x00,  // wDescriptorLength[0] 82

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x81,        // bEndpointAddress (IN/D2H)
		0x03,        // bmAttributes (Interrupt)
		0x08, 0x00,  // wMaxPacketSize 8
		0x04,        // bInterval 4 (unit depends on device speed)
	};

	static const uint8_t ppp_hid_descriptor[] = {
		0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
		0x09, 0x05,        // Usage (Game Pad)
		0xA1, 0x01,        // Collection (Application)
		0x05, 0x09,        //   Usage Page (Button)
		0x19, 0x01,        //   Usage Minimum (0x01)
		0x29, 0x0C,        //   Usage Maximum (0x0C)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0x01,        //   Logical Maximum (1)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x0C,        //   Report Count (12)
		0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x04,        //   Report Count (4)
		0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
		0x09, 0x01,        //   Usage (Pointer)
		0xA1, 0x00,        //   Collection (Physical)
		0x09, 0x30,        //     Usage (X)
		0x09, 0x31,        //     Usage (Y)
		0x15, 0xFF,        //     Logical Minimum (-1)
		0x25, 0x01,        //     Logical Maximum (1)
		0x95, 0x02,        //     Report Count (2)
		0x75, 0x02,        //     Report Size (2)
		0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x04,        //     Report Count (4)
		0x75, 0x01,        //     Report Size (1)
		0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,              //   End Collection
		0x75, 0x08,        //   Report Size (8)
		0x95, 0x05,        //   Report Count (5)
		0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x08,        //   Usage Page (LEDs)
		0x09, 0x2A,        //   Usage (On-Line)
		0x15, 0x00,        //   Logical Minimum (0)
		0x25, 0x01,        //   Logical Maximum (1)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x01,        //   Report Count (1)
		0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x75, 0x01,        //   Report Size (1)
		0x95, 0x07,        //   Report Count (7)
		0xB1, 0x01,        //   Feature (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,              // End Collection
	};

	// Dancing With the Stars
	static const uint8_t dwts_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass 
		0x00,        // bDeviceProtocol 
		0x08,        // bMaxPacketSize0 8
		0x30, 0x14,  // idVendor 0x1430
		0x00, 0x31,  // idProduct 0x3100
		0x00, 0x01,  // bcdDevice 1.00
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x00,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};
} // namespace usb_pad
