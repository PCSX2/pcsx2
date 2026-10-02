// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"
#include "USB/deviceproxy.h"
#include "usb-pad-ff.h"

namespace usb_pad
{
	enum WheelType
	{
		WT_DRIVING_FORCE, // DF or any other LT wheel in compatibility mode
		WT_DRIVING_FORCE_PRO,
		WT_GT_FORCE, // Formula Force GP
		WT_COUNT,
	};

	enum WheelControlID
	{
		CID_STEERING_L,
		CID_STEERING_R,
		CID_THROTTLE,
		CID_BRAKE,
		CID_DPAD_UP,
		CID_DPAD_DOWN,
		CID_DPAD_LEFT,
		CID_DPAD_RIGHT,
		CID_CROSS,
		CID_SQUARE,
		CID_CIRCLE,
		CID_TRIANGLE,
		CID_L1,
		CID_R1,
		CID_L2,
		CID_R2,
		CID_SELECT,
		CID_START,
		CID_R3,
		CID_L3,
	};

	class WheelDevice : public DeviceProxy
	{
	public:
		USBDevice* CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const override;
		const char* Name() const override;
		const char* TypeName() const override;
		const char* IconName() const override;
		bool Freeze(USBDevice* dev, StateWrapper& sw) const override;
		void UpdateSettings(USBDevice* dev, SettingsInterface& si) const override;
		float GetBindingValue(const USBDevice* dev, u32 bind_index) const override;
		void SetBindingValue(USBDevice* dev, u32 bind_index, float value) const override;
		void InputDeviceConnected(USBDevice* dev, const std::string_view identifier) const override;
		void InputDeviceDisconnected(USBDevice* dev, const std::string_view identifier) const override;
		std::span<const char*> SubTypes() const override;
		std::span<const InputBindingInfo> Bindings(u32 subtype) const override;
		std::span<const SettingInfo> Settings(u32 subtype) const override;
	};

	struct WheelState
	{
		WheelState(u32 port_, WheelType type_);
		~WheelState();

		void UpdateSettings(SettingsInterface& si, const char* devname);

		float GetBindValue(u32 bind) const;
		void SetBindValue(u32 bind, float value);

		void Reset();
		void UpdateSteering();
		void UpdateHatSwitch();

		void OpenFFDevice();
		void ParseFFData(const ff_data* ffdata, bool isDFP);

		s16 ApplySteeringAxisModifiers(float value);

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		WheelType type = WT_DRIVING_FORCE;

		s16 steering_range = 0;
		u16 steering_step = 0;
		s32 steering_deadzone = 0;
		s16 steering_curve_exponent = 0;

		#pragma pack(push, 1)
		union
		{
			// GT Force
			struct
			{
				u16 steering : 10; // 000=Left, 3ff=Right
				u16 l1 : 1;
				u16 r1 : 1;
				u16 x : 1;
				u16 y : 1;
				u16 a : 1;
				u16 b : 1;

				u16 pedals_attached : 1;
				u16 : 15;

				u8 throttle; // 00=Pressed, ff=Released
				u8 brake; // 00=Pressed, ff=Released
			} gtf;

			// Driving Force
			struct
			{
				u16 steering : 10; // 000=Left, 3ff=Right
				u16 cross : 1;
				u16 square : 1;
				u16 circle : 1;
				u16 triangle : 1;
				u16 r1 : 1;
				u16 l1 : 1;

				u16 r2 : 1;
				u16 l2 : 1;
				u16 select : 1;
				u16 start : 1;
				u16 r3 : 1;
				u16 l3 : 1;
				u16 : 10;

				u8 dpad;
				u8 throttle; // 00=Pressed, ff=Released
				u8 brake; // 00=Pressed, ff=Released
			} df;

			// Driving Force Pro
			struct
			{
				u16 steering : 14; // 0000=Left, 1fff=Mid, 3fff=Right
				u16 cross: 1;
				u16 square : 1;

				u8 circle : 1;
				u8 triangle : 1;
				u8 r1 : 1; // Right_paddle
				u8 l1 : 1; // Left_paddle
				u8 r2 : 1;
				u8 l2 : 1;
				u8 select : 1;
				u8 start : 1;

				u8 r3 : 1;
				u8 l3 : 1;
				u8 r3_2 : 1;
				u8 l3_2 : 1;
				u8 dpad : 4; // 0=N, 1=NE, 2=E, 3=SW, 4=S, 5=SW, 6=W, 7=NW, 8=IDLE

				u8 brake_throttle; // 00=ThrottlePressed, 7f=BothReleased/BothPressed, ff=BrakePressed
				u8 throttle; // 00=Pressed, ff=Released
				u8 brake; // 00=Pressed, ff=Released

				u8 pedals_attached : 1;
				u8 powered : 1;
				u8 : 1;
				u8 self_check_done : 1;
				u8 set1 : 1; // always set
				u8 : 3;
			} dfp;
		};
		#pragma pack(pop)

		struct
		{
			// intermediate state, resolved at query time
			s16 steering_left;
			s16 steering_right;
			bool hat_left : 1;
			bool hat_right : 1;
			bool hat_up : 1;
			bool hat_down : 1;

			u8 hatswitch; // direction
			u16 steering; // 0..steering_range*2
			u16 last_steering;
			u32 buttons; // active high

			u8 throttle; // inverted, 0 = fully depressed
			u8 brake; // inverted, 0 = fully depressed
		} data = {};

		std::string mFFdevName;
		std::unique_ptr<FFDevice> mFFdev;
		ff_state mFFstate{};
	};

/**
  linux hid-lg4ff.c
  http://www.spinics.net/lists/linux-input/msg16570.html
  Every Logitech wheel reports itself as generic Logitech Driving Force wheel (VID 046d, PID c294). This is done to ensure that the
  wheel will work on every USB HID-aware system even when no Logitech driver is available. It however limits the capabilities of the
  wheel - range is limited to 200 degrees, G25/G27 don't report the clutch pedal and there is only one combined axis for throttle and
  brake. The switch to native mode is done via hardware-specific command which is different for each wheel. When the wheel
  receives such command, it simulates reconnect and reports to the OS with its actual PID.
  Currently not emulating reattachment. Any games that expect to?
**/

	static const uint8_t df_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(0xc294),
		/* bcdDevice           */ WBVAL(0x0000), //(00.00)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	//XXX different pedal data than 0x1106, buggy hw?
	static const uint8_t dfp_dev_descriptor_1102[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(0xc298),
		/* bcdDevice           */ WBVAL(0x1102), //(11.02)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	static const uint8_t dfp_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(0xc298),
		/* bcdDevice           */ WBVAL(0x1106), //(11.06)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};
	
	static const uint8_t gtf_dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0100),
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x08, //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(0xc293),
		/* bcdDevice           */ WBVAL(0x0000),
		/* iManufacturer       */ 0x01, //actual is 0x04
		/* iProduct            */ 0x02, //actual is 0x20
		/* iSerialNumber       */ 0x00, //(0)
		/* bNumConfigurations  */ 0x01, //(1)
	};

	//https://lkml.org/lkml/2011/5/28/140
	//https://github.com/torvalds/linux/blob/master/drivers/hid/hid-lg.c
	// separate axes version
	static const uint8_t pad_driving_force_hid_separate_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x34,             /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0C,       /* Report Count (12), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage (Buttons), */
		0x19, 0x01,       /* Usage Minimum (1), */
		0x29, 0x0c,       /* Usage Maximum (12), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x02,       /* Report Count (2), */
		0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
		0x09, 0x01,       /* Usage (?: 1), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x08,       /* Report Size (8), */
		0x81, 0x02,       /* Input (Variable), */
		0x25, 0x07,       /* Logical Maximum (7), */
		0x46, 0x3B, 0x01, /* Physical Maximum (315), */
		0x75, 0x04,       /* Report Size (4), */
		0x65, 0x14,       /* Unit (Degrees), */
		0x09, 0x39,       /* Usage (Hat Switch), */
		0x81, 0x42,       /* Input (Variable, Null State), */
		0x75, 0x01,       /* Report Size (1), */
		0x95, 0x04,       /* Report Count (4), */
		0x65, 0x00,       /* Unit (none), */
		0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
		0x09, 0x01,       /* Usage (?: 1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x08,       /* Report Size (8), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (Rz), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x95, 0x07,       /* Report Count (7), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x03,       /* Usage (?: 3), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	static const uint8_t pad_driving_force_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x95, 0x01,       //     Report Count (1)
		0x75, 0x0A,       //     Report Size (10)
		0x15, 0x00,       //     Logical Minimum (0)
		0x26, 0xFF, 0x03, //     Logical Maximum (1023)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0xFF, 0x03, //     Physical Maximum (1023)
		0x09, 0x30,       //     Usage (X)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x0C,       //     Report Count (12)
		0x75, 0x01,       //     Report Size (1)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x05, 0x09,       //     Usage Page (Button)
		0x19, 0x01,       //     Usage Minimum (0x01)
		0x29, 0x0C,       //     Usage Maximum (0x0C)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x02,       //     Report Count (2)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01,       //     Usage (0x01)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,       //     Usage Page (Generic Desktop Ctrls)
		0x09, 0x31,       //     Usage (Y)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x95, 0x01,       //     Report Count (1)
		0x75, 0x08,       //     Report Size (8)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x25, 0x07,       //     Logical Maximum (7)
		0x46, 0x3B, 0x01, //     Physical Maximum (315)
		0x75, 0x04,       //     Report Size (4)
		0x65, 0x14,       //     Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,       //     Usage (Hat switch)
		0x81, 0x42,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x04,       //     Report Count (4)
		0x65, 0x00,       //     Unit (None)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01,       //     Usage (0x01)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x02,       //     Report Count (2)
		0x75, 0x08,       //     Report Size (8)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x02,       //     Usage (0x02)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0xA1, 0x02,       //   Collection (Logical)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x95, 0x07,       //     Report Count (7)
		0x75, 0x08,       //     Report Size (8)
		0x09, 0x03,       //     Usage (0x03)
		0x91, 0x02,       //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             //   End Collection
		0xC0,             // End Collection

		// 130 bytes
	};

	static const uint8_t pad_driving_force_pro_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0E,       /* Report Size (14), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x3F, /* Logical Maximum (16383), */
		0x34,             /* Physical Minimum (0), */
		0x46, 0xFF, 0x3F, /* Physical Maximum (16383), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0E,       /* Report Count (14), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x0E,       /* Usage Maximum (0Eh), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x04,       /* Report Size (4), */
		0x25, 0x07,       /* Logical Maximum (7), */
		0x46, 0x3B, 0x01, /* Physical Maximum (315), */
		0x65, 0x14,       /* Unit (Degrees), */
		0x09, 0x39,       /* Usage (Hat Switch), */
		0x81, 0x42,       /* Input (Variable, Nullstate), */
		0x65, 0x00,       /* Unit, */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x75, 0x08,       /* Report Size (8), */
		0x81, 0x01,       /* Input (Constant), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (Rz), */
		0x81, 0x02,       /* Input (Variable), */
		0x81, 0x01,       /* Input (Constant), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	static const uint8_t pad_momo_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x35, 0x00,       /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x08,       /* Report Count (8), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x08,       /* Usage Maximum (08h), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x75, 0x0E,       /* Report Size (14), */
		0x95, 0x01,       /* Report Count (1), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x00,       /* Usage (00h), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x32,       /* Usage (Z), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x09, 0x01,       /* Usage (01h), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	//TODO
	static const uint8_t pad_gtforce_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x95, 0x01,       //   Report Count (1)
		0x75, 0x0A,       //   Report Size (10)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x03, //   Logical Maximum (1023)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0xFF, 0x03, //   Physical Maximum (1023)
		0x09, 0x30,       //   Usage (X)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x06,       //   Report Count (6)
		0x75, 0x01,       //   Report Size (1)
		0x25, 0x01,       //   Logical Maximum (1)
		0x45, 0x01,       //   Physical Maximum (1)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x06,       //   Usage Maximum (0x06)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x01,       //   Report Count (1)
		0x75, 0x08,       //   Report Size (8)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		//0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01, //   Usage (0x01)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
		0x09, 0x31, //   Usage (Y)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		//0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01, //   Usage (0x01)
		0x95, 0x03, //   Report Count (3)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,       //   End Collection
		0xA1, 0x02, //   Collection (Logical)
		0x09, 0x02, //   Usage (0x02)
		0x95, 0x07, //   Report Count (7)
		0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,       //   End Collection
		0xC0,       // End Collection
	};

	static const uint8_t pad_gtforce_hid_report_descriptor_prolly_incorrect[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x35, 0x00,       /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0a,       /* Report Count (6), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x0a,       /* Usage Maximum (06h), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x75, 0x0C,       /* Report Size (8), */
		0x95, 0x01,       /* Report Count (1), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x00,       /* Usage (00h), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x01,       /* Input (Constant), */
		0x09, 0x32,       /* Usage (Z), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (RZ), */
		0x81, 0x02,       /* Input (Variable), */
		0x75, 0x0C,       /* Report Size (16), */
		0x95, 0x01,       /* Report Count (1), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x09, 0x01,       /* Usage (01h), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

#define USB_PSIZE 8
#define DESC_CONFIG_WORD(a) (a & 0xFF), ((a >> 8) & 0xFF)

	static const uint8_t df_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE,       // Size of this descriptor in bytes
		USB_INTERFACE_DESCRIPTOR_TYPE, // INTERFACE descriptor type
		0,                             // Interface Number
		0,                             // Alternate Setting Number
		2,                             // Number of endpoints in this intf
		USB_CLASS_HID,                 // Class code
		0,                             // Subclass code
		0,                             // Protocol code
		0,                             // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                                                              // Size of this descriptor in bytes RRoj hack
		USB_DT_HID,                                                        // HID descriptor type
		DESC_CONFIG_WORD(0x0100),                                          // HID Spec Release Number in BCD format (1.11)
		0x21,                                                              // Country Code (0x00 for Not supported, 0x21 for US)
		1,                                                                 // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,                                                     // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_driving_force_hid_report_descriptor)), // Size of the report descriptor

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};

	static const uint8_t dfp_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE,       // Size of this descriptor in bytes
		USB_INTERFACE_DESCRIPTOR_TYPE, // INTERFACE descriptor type
		0,                             // Interface Number
		0,                             // Alternate Setting Number
		2,                             // Number of endpoints in this intf
		USB_CLASS_HID,                 // Class code
		0,                             // Subclass code
		0,                             // Protocol code
		0,                             // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                     // Size of this descriptor in bytes
		USB_DT_HID,               // HID descriptor type
		DESC_CONFIG_WORD(0x0100), // HID Spec Release Number in BCD format (1.11)
		0x21,                     // Country Code (0x00 for Not supported, 0x21 for US)
		1,                        // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,            // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_driving_force_pro_hid_report_descriptor)),

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};

	static const uint8_t gtforce_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE, // Size of this descriptor in bytes
		USB_DT_INTERFACE,        // INTERFACE descriptor type
		0,                       // Interface Number
		0,                       // Alternate Setting Number
		2,                       // Number of endpoints in this intf
		USB_CLASS_HID,           // Class code
		0,                       // Subclass code
		0,                       // Protocol code
		0,                       // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                                                        // Size of this descriptor in bytes
		USB_DT_HID,                                                  // HID descriptor type
		DESC_CONFIG_WORD(0x0100),                                    // HID Spec Release Number in BCD format (1.11)
		0x21,                                                        // Country Code (0x00 for Not supported, 0x21 for US)
		1,                                                           // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,                                               // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_gtforce_hid_report_descriptor)), // Size of the report descriptor

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};
} // namespace usb_pad
