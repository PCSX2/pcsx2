/*
 * QEMU USB HID devices
 *
 * Copyright (c) 2005 Fabrice Bellard
 * Copyright (c) 2007 OpenMoko, Inc.  (andrew@openedhand.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "PrecompiledHeader.h"
#include "USB/deviceproxy.h"
#include "hidproxy.h"
#include "USB/qemu-usb/desc.h"
#include "usb-hid.h"
#include "USB/shared/inifile_usb.h"

#define CONTAINER_OF(p, type, field) ((type*)((char*)p - ((ptrdiff_t) & ((type*)0)->field)))

namespace usb_hid
{

	typedef struct UsbHIDState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;

		UsbHID* usbhid;

		USBEndpoint* intr;
		uint8_t port;
		struct freeze
		{
			HIDState hid;
			int ep;
		} f;

	} UsbHIDState;

	std::list<std::string> HIDKbdDevice::ListAPIs()
	{
		return RegisterUsbHID::instance().Names();
	}

	const TCHAR* HIDKbdDevice::LongAPIName(const std::string& name)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}

	std::list<std::string> BeatManiaDevice::ListAPIs()
	{
		return RegisterUsbHID::instance().Names();
	}

	const TCHAR* BeatManiaDevice::LongAPIName(const std::string& name)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}
	std::list<std::string> HIDMouseDevice::ListAPIs()
	{
		return RegisterUsbHID::instance().Names();
	}

	const TCHAR* HIDMouseDevice::LongAPIName(const std::string& name)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}

	enum
	{
		STR_MANUFACTURER = 1,
		STR_PRODUCT_MOUSE,
		STR_PRODUCT_TABLET,
		STR_PRODUCT_KEYBOARD,
		STR_SERIALNUMBER,
		STR_CONFIG_MOUSE,
		STR_CONFIG_TABLET,
		STR_CONFIG_KEYBOARD,
	};

	static const USBDescStrings desc_strings = {
		"QEMU",
		"QEMU USB Mouse",
		"QEMU USB Tablet",
		"QEMU USB Keyboard",
		"42", /* == remote wakeup works */
		"HID Mouse",
		"HID Tablet",
		"HID Keyboard",
	};


	static const USBDescStrings beatmania_dadada_desc_strings = {
		"",
		"KONAMI CPJ1",
		"USB JIS Mini Keyboard",
	};


	/* mostly the same values as the Bochs USB Keyboard device */
	static const uint8_t kbd_dev_desc[] = {
		0x12,       /*  u8 bLength; */
		0x01,       /*  u8 bDescriptorType; Device */
		0x10, 0x00, /*  u16 bcdUSB; v1.0 */

		0x00, /*  u8  bDeviceClass; */
		0x00, /*  u8  bDeviceSubClass; */
		0x00, /*  u8  bDeviceProtocol; [ low/full speeds only ] */
		0x08, /*  u8  bMaxPacketSize0; 8 Bytes */

		//  0x27, 0x06, /*  u16 idVendor; */
		0x4C, 0x05,
		// 0x01, 0x00, /*  u16 idProduct; */
		0x00, 0x10,
		0x00, 0x00, /*  u16 bcdDevice */

		STR_MANUFACTURER,     /*  u8  iManufacturer; */
		STR_PRODUCT_KEYBOARD, /*  u8  iProduct; */
		STR_SERIALNUMBER,     /*  u8  iSerialNumber; */
		0x01                  /*  u8  bNumConfigurations; */
	};

	static const uint8_t kbd_config_desc[] = {
		/* one configuration */
		0x09,       /*  u8  bLength; */
		0x02,       /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01,       /*  u8  bNumInterfaces; (1) */
		0x01,       /*  u8  bConfigurationValue; */
		0x04,       /*  u8  iConfiguration; */
		0xa0,       /*  u8  bmAttributes; 
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50,         /*  u8  MaxPower; */

		/* USB 1.1:
     * USB 2.0, single TT organization (mandatory):
     *  one interface, protocol 0
     *
     * USB 2.0, multiple TT organization (optional):
     *  two interfaces, protocols 1 (like single TT)
     *  and 2 (multiple TT mode) ... config is
     *  sometimes settable
     *  NOT IMPLEMENTED
     */

		/* one interface */
		0x09, /*  u8  if_bLength; */
		0x04, /*  u8  if_bDescriptorType; Interface */
		0x00, /*  u8  if_bInterfaceNumber; */
		0x00, /*  u8  if_bAlternateSetting; */
		0x01, /*  u8  if_bNumEndpoints; */
		0x03, /*  u8  if_bInterfaceClass; */
		0x01, /*  u8  if_bInterfaceSubClass; */
		0x01, /*  u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
		0x05, /*  u8  if_iInterface; */

		/* HID descriptor */
		0x09,       /*  u8  bLength; */
		0x21,       /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00,       /*  u8 country_code */
		0x01,       /*  u8 num_descriptors */
		0x22,       /*  u8 type; Report */
		63, 0,      /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07,       /*  u8  ep_bLength; */
		0x05,       /*  u8  ep_bDescriptorType; Endpoint */
		0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03,       /*  u8  ep_bmAttributes; Interrupt */
		0x08, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a,       /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
	};

	/* mostly the same values as the Bochs USB Mouse device */
	static const uint8_t qemu_mouse_dev_descriptor[] = {
		0x12,       /*  u8 bLength; */
		0x01,       /*  u8 bDescriptorType; Device */
		0x10, 0x00, /*  u16 bcdUSB; v1.0 */

		0x00, /*  u8  bDeviceClass; */
		0x00, /*  u8  bDeviceSubClass; */
		0x00, /*  u8  bDeviceProtocol; [ low/full speeds only ] */
		0x08, /*  u8  bMaxPacketSize0; 8 Bytes */

		0x27, 0x06, /*  u16 idVendor; */
		0x01, 0x00, /*  u16 idProduct; */
		0x00, 0x00, /*  u16 bcdDevice */

		STR_MANUFACTURER,  /*  u8  iManufacturer; */
		STR_PRODUCT_MOUSE, /*  u8  iProduct; */
		STR_SERIALNUMBER,  /*  u8  iSerialNumber; */
		0x01               /*  u8  bNumConfigurations; */
	};

	static const uint8_t qemu_mouse_config_descriptor[] = {
		/* one configuration */
		0x09,       /*  u8  bLength; */
		0x02,       /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01,       /*  u8  bNumInterfaces; (1) */
		0x01,       /*  u8  bConfigurationValue; */
		0x04,       /*  u8  iConfiguration; */
		0xa0,       /*  u8  bmAttributes; 
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50,         /*  u8  MaxPower; */

		/* USB 1.1:
     * USB 2.0, single TT organization (mandatory):
     *  one interface, protocol 0
     *
     * USB 2.0, multiple TT organization (optional):
     *  two interfaces, protocols 1 (like single TT)
     *  and 2 (multiple TT mode) ... config is
     *  sometimes settable
     *  NOT IMPLEMENTED
     */

		/* one interface */
		0x09, /*  u8  if_bLength; */
		0x04, /*  u8  if_bDescriptorType; Interface */
		0x00, /*  u8  if_bInterfaceNumber; */
		0x00, /*  u8  if_bAlternateSetting; */
		0x01, /*  u8  if_bNumEndpoints; */
		0x03, /*  u8  if_bInterfaceClass; */
		0x01, /*  u8  if_bInterfaceSubClass; */
		0x02, /*  u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
		0x05, /*  u8  if_iInterface; */

		/* HID descriptor */
		0x09,       /*  u8  bLength; */
		0x21,       /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00,       /*  u8 country_code */
		0x01,       /*  u8 num_descriptors */
		0x22,       /*  u8 type; Report */
		52, 0,      /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07,       /*  u8  ep_bLength; */
		0x05,       /*  u8  ep_bDescriptorType; Endpoint */
		0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03,       /*  u8  ep_bmAttributes; Interrupt */
		0x04, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a,       /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
	};

	[[maybe_unused]]static const uint8_t qemu_tablet_config_descriptor[] = {
		/* one configuration */
		0x09,       /*  u8  bLength; */
		0x02,       /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01,       /*  u8  bNumInterfaces; (1) */
		0x01,       /*  u8  bConfigurationValue; */
		0x04,       /*  u8  iConfiguration; */
		0xa0,       /*  u8  bmAttributes; 
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50,         /*  u8  MaxPower; */

		/* USB 1.1:
     * USB 2.0, single TT organization (mandatory):
     *  one interface, protocol 0
     *
     * USB 2.0, multiple TT organization (optional):
     *  two interfaces, protocols 1 (like single TT)
     *  and 2 (multiple TT mode) ... config is
     *  sometimes settable
     *  NOT IMPLEMENTED
     */

		/* one interface */
		0x09, /*  u8  if_bLength; */
		0x04, /*  u8  if_bDescriptorType; Interface */
		0x00, /*  u8  if_bInterfaceNumber; */
		0x00, /*  u8  if_bAlternateSetting; */
		0x01, /*  u8  if_bNumEndpoints; */
		0x03, /*  u8  if_bInterfaceClass; */
		0x01, /*  u8  if_bInterfaceSubClass; */
		0x02, /*  u8  if_bInterfaceProtocol; [usb1.1 or single tt] */
		0x05, /*  u8  if_iInterface; */

		/* HID descriptor */
		0x09,       /*  u8  bLength; */
		0x21,       /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00,       /*  u8 country_code */
		0x01,       /*  u8 num_descriptors */
		0x22,       /*  u8 type; Report */
		74, 0,      /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07,       /*  u8  ep_bLength; */
		0x05,       /*  u8  ep_bDescriptorType; Endpoint */
		0x81,       /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03,       /*  u8  ep_bmAttributes; Interrupt */
		0x08, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a,       /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
	};

	static const uint8_t qemu_mouse_hid_report_descriptor[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x02, /* Usage (Mouse) */
		0xa1, 0x01, /* Collection (Application) */
		0x09, 0x01, /*   Usage (Pointer) */
		0xa1, 0x00, /*   Collection (Physical) */
		0x05, 0x09, /*     Usage Page (Button) */
		0x19, 0x01, /*     Usage Minimum (1) */
		0x29, 0x03, /*     Usage Maximum (3) */
		0x15, 0x00, /*     Logical Minimum (0) */
		0x25, 0x01, /*     Logical Maximum (1) */
		0x95, 0x03, /*     Report Count (3) */
		0x75, 0x01, /*     Report Size (1) */
		0x81, 0x02, /*     Input (Data, Variable, Absolute) */
		0x95, 0x01, /*     Report Count (1) */
		0x75, 0x05, /*     Report Size (5) */
		0x81, 0x01, /*     Input (Constant) */
		0x05, 0x01, /*     Usage Page (Generic Desktop) */
		0x09, 0x30, /*     Usage (X) */
		0x09, 0x31, /*     Usage (Y) */
		0x09, 0x38, /*     Usage (Wheel) */
		0x15, 0x81, /*     Logical Minimum (-0x7f) */
		0x25, 0x7f, /*     Logical Maximum (0x7f) */
		0x75, 0x08, /*     Report Size (8) */
		0x95, 0x03, /*     Report Count (3) */
		0x81, 0x06, /*     Input (Data, Variable, Relative) */
		0xc0,       /*   End Collection */
		0xc0,       /* End Collection */
	};

	static const uint8_t qemu_tablet_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x02,       /* Usage (Mouse) */
		0xa1, 0x01,       /* Collection (Application) */
		0x09, 0x01,       /*   Usage (Pointer) */
		0xa1, 0x00,       /*   Collection (Physical) */
		0x05, 0x09,       /*     Usage Page (Button) */
		0x19, 0x01,       /*     Usage Minimum (1) */
		0x29, 0x03,       /*     Usage Maximum (3) */
		0x15, 0x00,       /*     Logical Minimum (0) */
		0x25, 0x01,       /*     Logical Maximum (1) */
		0x95, 0x03,       /*     Report Count (3) */
		0x75, 0x01,       /*     Report Size (1) */
		0x81, 0x02,       /*     Input (Data, Variable, Absolute) */
		0x95, 0x01,       /*     Report Count (1) */
		0x75, 0x05,       /*     Report Size (5) */
		0x81, 0x01,       /*     Input (Constant) */
		0x05, 0x01,       /*     Usage Page (Generic Desktop) */
		0x09, 0x30,       /*     Usage (X) */
		0x09, 0x31,       /*     Usage (Y) */
		0x15, 0x00,       /*     Logical Minimum (0) */
		0x26, 0xff, 0x7f, /*     Logical Maximum (0x7fff) */
		0x35, 0x00,       /*     Physical Minimum (0) */
		0x46, 0xff, 0x7f, /*     Physical Maximum (0x7fff) */
		0x75, 0x10,       /*     Report Size (16) */
		0x95, 0x02,       /*     Report Count (2) */
		0x81, 0x02,       /*     Input (Data, Variable, Absolute) */
		0x05, 0x01,       /*     Usage Page (Generic Desktop) */
		0x09, 0x38,       /*     Usage (Wheel) */
		0x15, 0x81,       /*     Logical Minimum (-0x7f) */
		0x25, 0x7f,       /*     Logical Maximum (0x7f) */
		0x35, 0x00,       /*     Physical Minimum (same as logical) */
		0x45, 0x00,       /*     Physical Maximum (same as logical) */
		0x75, 0x08,       /*     Report Size (8) */
		0x95, 0x01,       /*     Report Count (1) */
		0x81, 0x06,       /*     Input (Data, Variable, Relative) */
		0xc0,             /*   End Collection */
		0xc0,             /* End Collection */
	};

	static const uint8_t qemu_keyboard_hid_report_descriptor[] = {
		0x05, 0x01, /* Usage Page (Generic Desktop) */
		0x09, 0x06, /* Usage (Keyboard) */
		0xa1, 0x01, /* Collection (Application) */
		0x75, 0x01, /*   Report Size (1) */
		0x95, 0x08, /*   Report Count (8) */
		0x05, 0x07, /*   Usage Page (Key Codes) */
		0x19, 0xe0, /*   Usage Minimum (224) */
		0x29, 0xe7, /*   Usage Maximum (231) */
		0x15, 0x00, /*   Logical Minimum (0) */
		0x25, 0x01, /*   Logical Maximum (1) */
		0x81, 0x02, /*   Input (Data, Variable, Absolute) */
		0x95, 0x01, /*   Report Count (1) */
		0x75, 0x08, /*   Report Size (8) */
		0x81, 0x01, /*   Input (Constant) */
		0x95, 0x05, /*   Report Count (5) */
		0x75, 0x01, /*   Report Size (1) */
		0x05, 0x08, /*   Usage Page (LEDs) */
		0x19, 0x01, /*   Usage Minimum (1) */
		0x29, 0x05, /*   Usage Maximum (5) */
		0x91, 0x02, /*   Output (Data, Variable, Absolute) */
		0x95, 0x01, /*   Report Count (1) */
		0x75, 0x03, /*   Report Size (3) */
		0x91, 0x01, /*   Output (Constant) */
		0x95, 0x06, /*   Report Count (6) */
		0x75, 0x08, /*   Report Size (8) */
		0x15, 0x00, /*   Logical Minimum (0) */
		0x25, 0xff, /*   Logical Maximum (255) */
		0x05, 0x07, /*   Usage Page (Key Codes) */
		0x19, 0x00, /*   Usage Minimum (0) */
		0x29, 0xff, /*   Usage Maximum (255) */
		0x81, 0x00, /*   Input (Data, Array) */
		0xc0,       /* End Collection */
	};

	static const uint8_t beatmania_dev_desc[] = {
		0x12,         /*  u8 bLength; */
		0x01,         /*  u8 bDescriptorType; Device */
		WBVAL(0x110), /*  u16 bcdUSB; v1.10 */

		0x00, /*  u8  bDeviceClass; */
		0x00, /*  u8  bDeviceSubClass; */
		0x00, /*  u8  bDeviceProtocol; [ low/full speeds only ] */
		0x08, /*  u8  bMaxPacketSize0; 8 Bytes */

		//  0x27, 0x06, /*  u16 idVendor; */
		WBVAL(0x0510),
		// 0x01, 0x00, /*  u16 idProduct; */
		WBVAL(0x0002),
		WBVAL(0x0020), /*  u16 bcdDevice */

		1,   /*  u8  iManufacturer; */
		2,   /*  u8  iProduct; */
		0,   /*  u8  iSerialNumber; */
		0x01 /*  u8  bNumConfigurations; */
	};

	static const uint8_t beatmania_config_desc[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x02,       // iConfiguration (String Index)
		0xA0,       // bmAttributes Remote Wakeup
		0x14,       // bMaxPower 40mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0x03, // bInterfaceClass
		0x01, // bInterfaceSubClass
		0x01, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x10, 0x01, // bcdHID 1.10
		0x0F,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x44, 0x00, // wDescriptorLength[0] 68

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x0A,       // bInterval 10 (unit depends on device speed)

		// 34 bytes
	};

	static const uint8_t beatmania_dadada_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x06,       // Usage (Keyboard)
		0xA1, 0x01,       // Collection (Application)
		0x05, 0x07,       //   Usage Page (Kbrd/Keypad)
		0x19, 0xE0,       //   Usage Minimum (0xE0)
		0x29, 0xE7,       //   Usage Maximum (0xE7)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x08,       //   Report Count (8)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x07,       //   Usage Page (Kbrd/Keypad)
		0x19, 0x00,       //   Usage Minimum (0x00)
		0x29, 0xFF,       //   Usage Maximum (0xFF)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x06,       //   Report Count (6)
		0x81, 0x00,       //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x08,       //   Usage Page (LEDs)
		0x19, 0x01,       //   Usage Minimum (Num Lock)
		0x29, 0x05,       //   Usage Maximum (Kana)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x05,       //   Report Count (5)
		0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x75, 0x03,       //   Report Size (3)
		0x95, 0x01,       //   Report Count (1)
		0x91, 0x01,       //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             // End Collection

		// 68 bytes
	};

	static void usb_hid_changed(HIDState* hs)
	{
		UsbHIDState* us = CONTAINER_OF(hs, UsbHIDState, f.hid);

		usb_wakeup(us->intr, 0);
	}

	static void usb_hid_handle_reset(USBDevice* dev)
	{
		UsbHIDState* us = reinterpret_cast<UsbHIDState*>(dev);

		hid_reset(&us->f.hid);
	}

	static void usb_hid_handle_control(USBDevice* dev, USBPacket* p,
									   int request, int value, int index, int length, uint8_t* data)
	{
		UsbHIDState* us = reinterpret_cast<UsbHIDState*>(dev);
		HIDState* hs = &us->f.hid;
		int ret;

		DevCon.WriteLn("usb-hid: req %04X val: %04X idx: %04X len: %d\n", request, value, index, length);
		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
				/* hid specific requests */
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				switch (value >> 8)
				{
					case 0x22:
						if (hs->kind == HID_MOUSE)
						{
							memcpy(data, qemu_mouse_hid_report_descriptor,
								   sizeof(qemu_mouse_hid_report_descriptor));
							p->actual_length = sizeof(qemu_mouse_hid_report_descriptor);
						}
						else if (hs->kind == HID_TABLET)
						{
							memcpy(data, qemu_tablet_hid_report_descriptor,
								   sizeof(qemu_tablet_hid_report_descriptor));
							p->actual_length = sizeof(qemu_tablet_hid_report_descriptor);
						}
						else if (hs->kind == HID_KEYBOARD)
						{
							if (hs->sub_kind == HID_SUBKIND_BEATMANIA)
							{
								p->actual_length = sizeof(beatmania_dadada_hid_report_descriptor);
								memcpy(data, beatmania_dadada_hid_report_descriptor, p->actual_length);
							}
							else
							{
								p->actual_length = sizeof(qemu_keyboard_hid_report_descriptor);
								memcpy(data, qemu_keyboard_hid_report_descriptor, p->actual_length);
							}
						}
						break;
					default:
						goto fail;
				}
				break;
			case GET_REPORT:
				if (hs->kind == HID_MOUSE || hs->kind == HID_TABLET)
				{
					p->actual_length = hid_pointer_poll(hs, data, length);
				}
				else if (hs->kind == HID_KEYBOARD)
				{
					p->actual_length = hid_keyboard_poll(hs, data, length);
				}
				break;
			case SET_REPORT:
				if (hs->kind == HID_KEYBOARD)
				{
					p->actual_length = hid_keyboard_write(hs, data, length);
				}
				else
				{
					goto fail;
				}
				break;
			case GET_PROTOCOL:
				if (hs->kind != HID_KEYBOARD && hs->kind != HID_MOUSE)
				{
					goto fail;
				}
				data[0] = hs->protocol;
				p->actual_length = 1;
				break;
			case SET_PROTOCOL:
				if (hs->kind != HID_KEYBOARD && hs->kind != HID_MOUSE)
				{
					goto fail;
				}
				hs->protocol = value;
				break;
			case GET_IDLE:
				data[0] = hs->idle;
				p->actual_length = 1;
				break;
			case SET_IDLE:
				hs->idle = (uint8_t)(value >> 8);
				DevCon.WriteLn("IDLE %d\n", hs->idle);
				hid_set_next_idle(hs);
				if (hs->kind == HID_MOUSE || hs->kind == HID_TABLET)
				{
					hid_pointer_activate(hs);
				}
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_hid_handle_data(USBDevice* dev, USBPacket* p)
	{
		UsbHIDState* us = reinterpret_cast<UsbHIDState*>(dev);
		HIDState* hs = &us->f.hid;
		std::vector<uint8_t> buf(p->iov.size);
		size_t len = 0;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (p->ep->nr == 1)
				{
					if (hs->kind == HID_MOUSE || hs->kind == HID_TABLET)
					{
						hid_pointer_activate(hs);
					}
					if (!hid_has_events(hs))
					{
						p->status = USB_RET_NAK;
						return;
					}
					hid_set_next_idle(hs);
					if (hs->kind == HID_MOUSE || hs->kind == HID_TABLET)
					{
						len = hid_pointer_poll(hs, buf.data(), p->iov.size);
					}
					else if (hs->kind == HID_KEYBOARD)
					{
						len = hid_keyboard_poll(hs, buf.data(), p->iov.size);
					}
					usb_packet_copy(p, buf.data(), len);
				}
				else
				{
					goto fail;
				}
				break;
			case USB_TOKEN_OUT:
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_hid_unrealize(USBDevice* dev)
	{
		UsbHIDState* us = reinterpret_cast<UsbHIDState*>(dev);

		hid_free(&us->f.hid);

		delete us;
	}

	int usb_hid_open(USBDevice* dev)
	{
		UsbHIDState* s = (UsbHIDState*)dev;
		if (s)
			return s->usbhid->Open();
		return 0;
	}

	void usb_hid_close(USBDevice* dev)
	{
		UsbHIDState* s = (UsbHIDState*)dev;
		if (s)
			s->usbhid->Close();
	}

	USBDevice* HIDKbdDevice::CreateDevice(int port)
	{
		UsbHIDState* s;

		std::string varApi;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp);
		varApi = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, varApi);
#endif
		UsbHIDProxyBase* proxy = RegisterUsbHID::instance().Proxy(varApi);
		if (!proxy)
		{
			Console.WriteLn("Invalid HID API: %s \n", varApi.c_str());
			return nullptr;
		}

		UsbHID* usbhid = proxy->CreateObject(port, TypeName());

		if (!usbhid)
			return nullptr;

		s = new UsbHIDState();

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(kbd_dev_desc, sizeof(kbd_dev_desc), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(kbd_config_desc, sizeof(kbd_config_desc), s->desc_dev) < 0)
			goto fail;

		s->usbhid = usbhid;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.open = usb_hid_open;
		s->dev.klass.close = usb_hid_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];
		s->port = port;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->f.hid, HID_KEYBOARD, usb_hid_changed);
		s->usbhid->SetHIDState(&s->f.hid);
		s->usbhid->SetHIDType(HIDTYPE_KBD);

		usb_hid_handle_reset((USBDevice*)s);

		return (USBDevice*)s;
	fail:
		usb_hid_unrealize((USBDevice*)s);
		return nullptr;
	}

	int HIDKbdDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), HIDTYPE_KBD, data);
		return RESULT_CANCELED;
	}

	int HIDKbdDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		auto s = reinterpret_cast<UsbHIDState*>(dev);
		auto freezed = reinterpret_cast<UsbHIDState::freeze*>(data);

		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				if (!s)
					return -1;
				s->f = *freezed;
				hid_init(&s->f.hid, HID_KEYBOARD, usb_hid_changed);

				return sizeof(UsbHIDState::freeze);
			case FreezeAction::Save:
				if (!s)
					return -1;
				*freezed = s->f;
				return sizeof(UsbHIDState::freeze);
			case FreezeAction::Size:
				return sizeof(UsbHIDState::freeze);
			default:
				break;
		}
		return 0;
	}

	USBDevice* HIDMouseDevice::CreateDevice(int port)
	{
		UsbHIDState* s;

		std::string varApi;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp);
		varApi = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, varApi);
#endif
		UsbHIDProxyBase* proxy = RegisterUsbHID::instance().Proxy(varApi);
		if (!proxy)
		{
			Console.WriteLn("Invalid HID API: %s\n", varApi.c_str());
			return nullptr;
		}

		UsbHID* usbhid = proxy->CreateObject(port, TypeName());

		if (!usbhid)
			return nullptr;

		s = new UsbHIDState();

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(qemu_mouse_dev_descriptor, sizeof(qemu_mouse_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(qemu_mouse_config_descriptor, sizeof(qemu_mouse_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->usbhid = usbhid;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.open = usb_hid_open;
		s->dev.klass.close = usb_hid_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[STR_CONFIG_MOUSE];
		s->port = port;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->f.hid, HID_MOUSE, usb_hid_changed);
		s->usbhid->SetHIDState(&s->f.hid);
		s->usbhid->SetHIDType(HIDTYPE_MOUSE);

		usb_hid_handle_reset((USBDevice*)s);

		return (USBDevice*)s;
	fail:
		usb_hid_unrealize((USBDevice*)s);
		return nullptr;
	}

	int HIDMouseDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), HIDTYPE_MOUSE, data);
		return RESULT_CANCELED;
	}

	int HIDMouseDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		auto s = reinterpret_cast<UsbHIDState*>(dev);
		auto freezed = reinterpret_cast<UsbHIDState::freeze*>(data);

		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				if (!s)
					return -1;
				s->f = *freezed;
				hid_init(&s->f.hid, HID_MOUSE, usb_hid_changed);

				return sizeof(UsbHIDState::freeze);
			case FreezeAction::Save:
				if (!s)
					return -1;
				*freezed = s->f;
				return sizeof(UsbHIDState::freeze);
			case FreezeAction::Size:
				return sizeof(UsbHIDState::freeze);
			default:
				break;
		}
		return 0;
	}

	// ---- BeatMania Da Da Da!! ----

	USBDevice* BeatManiaDevice::CreateDevice(int port)
	{
		DevCon.WriteLn("%s\n", __func__);
		UsbHIDState* s;

		std::string varApi;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp);
		varApi = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, varApi);
#endif
		UsbHIDProxyBase* proxy = RegisterUsbHID::instance().Proxy(varApi);
		if (!proxy)
		{
			Console.WriteLn("Invalid HID API: %s\n", varApi.c_str());
			return nullptr;
		}

		UsbHID* usbhid = proxy->CreateObject(port, TypeName());

		if (!usbhid)
			return nullptr;

		s = new UsbHIDState();

		s->desc.full = &s->desc_dev;
		s->desc.str = beatmania_dadada_desc_strings;

		if (usb_desc_parse_dev(beatmania_dev_desc, sizeof(beatmania_dev_desc), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(beatmania_config_desc, sizeof(beatmania_config_desc), s->desc_dev) < 0)
			goto fail;

		s->usbhid = usbhid;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.open = usb_hid_open;
		s->dev.klass.close = usb_hid_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];
		s->port = port;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->f.hid, HID_KEYBOARD, usb_hid_changed);
		s->f.hid.sub_kind = HID_SUBKIND_BEATMANIA;
		s->usbhid->SetHIDState(&s->f.hid);
		s->usbhid->SetHIDType(HIDTYPE_KBD);

		usb_hid_handle_reset((USBDevice*)s);

		return (USBDevice*)s;
	fail:
		usb_hid_unrealize((USBDevice*)s);
		return nullptr;
	}

	int BeatManiaDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterUsbHID::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), HIDTYPE_KBD, data);
		return RESULT_CANCELED;
	}

	int BeatManiaDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return HIDKbdDevice::Freeze(mode, dev, data);
	}

} // namespace usb_hid
