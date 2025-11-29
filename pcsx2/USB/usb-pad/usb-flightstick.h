// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "USB/deviceproxy.h"
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"

namespace usb_pad
{
	enum FlightStickDeviceTypes
	{
		FLIGHTSTICK_FS1, // HP2-13  (FlightStick)
		FLIGHTSTICK_FS2, // HP2-217 (FlightStick 2)
		FLIGHTSTICK_COUNT,
	};

	class FlightStickDevice final : public DeviceProxy
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
	struct FlightStickConData // interrupt input data
	{					/* FlightStick 1						| FlightStick 2				*/
		u8 stick_x;		/* stick (left=0x00, right=0xff)		| identical					*/
		u8 stick_y;		/* stick (top=0x00, bottom=0xff)		| identical					*/
		u8 rudder;		/* rudder (left=0x00, right=0xff)		| identical					*/
		u8 throttle;	/* throttle (top=0xff, bottom=0x00)		| (top=0x00, bottom=0xff)	*/
		u8 hat_x;		/* hat (left=0x00, right=0xff)			| identical					*/
		u8 hat_y;		/* hat (top=0x00, bottom=0xff)			| identical					*/
		u8 button_a;	/* triangle	(press=0x00, release=0xff)	| button A					*/
		u8 button_b;	/* square (press=0x00, release=0xff)	| button B					*/
	};
	static_assert(sizeof(FlightStickConData) == 8);

	struct FlightStickConData_VR00 // input data for vendor request 00
	{							/* FlightStick 1	| FlightStick 2		*/
		bool fire_c		: 1;	/* button select	| button fire-c		*/
		bool button_d	: 1;	/* 0x1				| button D			*/
		bool hat_btn	: 1;	/* hat press		| hat press			*/
		bool button_st	: 1;	/* button start		| button ST			*/
		bool hat1_u		: 1;	/* d-pad top		| d-pad 1 top		*/
		bool hat1_r		: 1;	/* d-pad right		| d-pad 1 right		*/
		bool hat1_d		: 1;	/* d-pad bottom		| d-pad 1 bottom	*/
		bool hat1_l		: 1;	/* d-pad left		| d-pad 1 left		*/

		u8 reserved1	: 4;	/* 0xf				| 0xf				*/
		bool reserved2	: 1;	/* 0x1				| 0x1				*/
		bool launch		: 1;	/* button launch	| button launch		*/
		bool trigger	: 1;	/* trigger			| trigger			*/
		bool reserved3	: 1;	/* 0x1				| 0x1				*/
	};
	static_assert(sizeof(FlightStickConData_VR00) == 2);

	struct FlightStickConData_VR01 // input data for vendor request 01
	{							/* FlightStick 1	| FlightStick 2						*/
		u8 reserved4	: 4;	/* 0xf				| 0xf								*/
		bool hat3_r		: 1;	/* 0x1				| d-pad 3 right						*/
		bool hat3_m		: 1;	/* 0x1				| d-pad 3 middle					*/
		bool hat3_l		: 1;	/* 0x1				| d-pad 3 left						*/
		bool reserved5	: 1;	/* 0x1				| 0x0								*/

		u8 mode_select	: 2;	/* 0x3				| mode select (M1=2, M2=1, M3=3)	*/
		bool reserved6	: 1;	/* 0x1				| 0x1								*/
		bool button_sw1	: 1;	/* 0x1				| button sw-1						*/
		bool hat2_u		: 1;	/* 0x1				| d-pad 2 top						*/
		bool hat2_r		: 1;	/* 0x1				| d-pad 2 right						*/
		bool hat2_d		: 1;	/* 0x1				| d-pad 2 bottom					*/
		bool hat2_l		: 1;	/* 0x1				| d-pad 2 left						*/
	};
	static_assert(sizeof(FlightStickConData_VR01) == 2);

#pragma pack(pop)

	struct FlightStickDeviceState
	{
		FlightStickDeviceState(u32 port_, FlightStickDeviceTypes type_);
		~FlightStickDeviceState();

		void Reset();
		void UpdateStick() noexcept;
		void UpdateRudder() noexcept;
		void UpdateThrottle() noexcept;
		void UpdateStickHat() noexcept;

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		u32 port = 0;
		FlightStickDeviceTypes type = FLIGHTSTICK_FS1;
		u8 mode = 3;

		const u8 analog_center = 0x80;
		const u8 analog_range = 0xFF >> 1;

		struct
		{
			// intermediate state, resolved at query time
			u8 stick_left;
			u8 stick_right;
			u8 stick_up;
			u8 stick_down;
			u8 rudder_left;
			u8 rudder_right;
			u8 throttle_up;
			u8 throttle_down;
			u8 stick_hat_left;
			u8 stick_hat_right;
			u8 stick_hat_up;
			u8 stick_hat_down;

			u8 stick_x;
			u8 stick_y;
			u8 rudder;
			u8 throttle;
			u8 hatstick_x;
			u8 hatstick_y;
			
			u8 button_a;
			u8 button_b;

			u32 buttons; // dpads and buttons
		} data = {};
	};

#define DEFINE_DCTFS_DEV_DESCRIPTOR(prefix, bcdUSB, bcdDevice) \
	static const uint8_t prefix##_dev_descriptor[] = { \
		/* bLength             */ USB_DEVICE_DESC_SIZE, \
		/* bDescriptorType     */ USB_DEVICE_DESCRIPTOR_TYPE, \
		/* bcdUSB              */ WBVAL(bcdUSB), /* FS1=0x0100, FS2=0x0110 */ \
		/* bDeviceClass        */ 0xFF, \
		/* bDeviceSubClass     */ 0x01, \
		/* bDeviceProtocol     */ 0xFF, \
		/* bMaxPacketSize0     */ 0x08, \
		/* idVendor            */ WBVAL(0x06D3), \
		/* idProduct           */ WBVAL(0x0F10), \
		/* bcdDevice           */ WBVAL(bcdDevice), /* FS1=0x0001, FS2=0x0002 */ \
		/* iManufacturer       */ 0x00, \
		/* iProduct            */ 0x00, \
		/* iSerialNumber       */ 0x00, \
		/* bNumConfigurations  */ 0x01, \
	}

	// common for both models
	static const uint8_t flightstick_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE, // bLength
		USB_CONFIGURATION_DESCRIPTOR_TYPE, // bDescriptorType
		WBVAL(34), // wTotalLength
		0x01, // bNumInterfaces
		0x01, // bConfigurationValue
		0x00, // iConfiguration (String Index)
		0xA0, // bmAttributes
		0x32, // bMaxPower 100mA

		USB_INTERFACE_DESC_SIZE, // bLength
		USB_INTERFACE_DESCRIPTOR_TYPE, // bDescriptorType
		0x00, // bInterfaceNumber
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints
		0xFF, // bInterfaceClass
		0x01, // bInterfaceSubClass
		0x02, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		// Unknown (looks to be HID. descriptor data is missing)
		0x09, // bLength
		0x21, // bDescriptorType (HID)
		0x00, 0x01, // bcdHID 1.00
		0x00, // bCountryCode
		0x01, // bNumDescriptors
		0x22, // bDescriptorType[0] (HID)
		0x40, 0x00, // wDescriptorLength[0] 64

		USB_ENDPOINT_DESC_SIZE, // bLength
		USB_ENDPOINT_DESCRIPTOR_TYPE, // bDescriptorType
		USB_ENDPOINT_IN(1), // bEndpointAddress (IN/D2H)
		USB_ENDPOINT_TYPE_INTERRUPT, // bmAttributes (Interrupt)
		WBVAL(8), // wMaxPacketSize
		0x0A, // bInterval 10 (unit depends on device speed)
	};

	// ---- FlightStick "Type 1" ----

	static const USBDescStrings fst01_desc_strings = { "" };

	// fst01_dev_descriptor
	DEFINE_DCTFS_DEV_DESCRIPTOR(fst01, 0x0100, 0x0001);

	// ---- FlightStick "Type 2" ----

	static const USBDescStrings fst02_desc_strings = { "" };

	// fst02_dev_descriptor
	DEFINE_DCTFS_DEV_DESCRIPTOR(fst02, 0x0110, 0x0002);

} // namespace usb_pad
