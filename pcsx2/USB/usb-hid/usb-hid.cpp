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

#include "Host.h"
#include "Input/InputManager.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "USB/deviceproxy.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-hid/usb-hid.h"

namespace usb_hid
{
	struct UsbHIDState
	{
		explicit UsbHIDState(u32 port_);

		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		USBEndpoint* intr = nullptr;
		HIDState hid{};

		u32 port = 0;

		std::map<u32, QKeyCode> keycode_mapping;

		void SetKeycodeMapping();

		void QueueKeyboardState(KeyValue keycode, bool pressed);
		void QueueMouseAxisState(InputPointerAxis axis, float delta);
		void QueueMouseButtonState(InputButton button, bool pressed);
	};

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
		0x12, /*  u8 bLength; */
		0x01, /*  u8 bDescriptorType; Device */
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

		STR_MANUFACTURER, /*  u8  iManufacturer; */
		STR_PRODUCT_KEYBOARD, /*  u8  iProduct; */
		STR_SERIALNUMBER, /*  u8  iSerialNumber; */
		0x01 /*  u8  bNumConfigurations; */
	};

	static const uint8_t kbd_config_desc[] = {
		/* one configuration */
		0x09, /*  u8  bLength; */
		0x02, /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01, /*  u8  bNumInterfaces; (1) */
		0x01, /*  u8  bConfigurationValue; */
		0x04, /*  u8  iConfiguration; */
		0xa0, /*  u8  bmAttributes;
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50, /*  u8  MaxPower; */

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
		0x09, /*  u8  bLength; */
		0x21, /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00, /*  u8 country_code */
		0x01, /*  u8 num_descriptors */
		0x22, /*  u8 type; Report */
		63, 0, /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07, /*  u8  ep_bLength; */
		0x05, /*  u8  ep_bDescriptorType; Endpoint */
		0x81, /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03, /*  u8  ep_bmAttributes; Interrupt */
		0x08, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a, /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
	};

	/* mostly the same values as the Bochs USB Mouse device */
	static const uint8_t qemu_mouse_dev_descriptor[] = {
		0x12, /*  u8 bLength; */
		0x01, /*  u8 bDescriptorType; Device */
		0x10, 0x00, /*  u16 bcdUSB; v1.0 */

		0x00, /*  u8  bDeviceClass; */
		0x00, /*  u8  bDeviceSubClass; */
		0x00, /*  u8  bDeviceProtocol; [ low/full speeds only ] */
		0x08, /*  u8  bMaxPacketSize0; 8 Bytes */

		0x27, 0x06, /*  u16 idVendor; */
		0x01, 0x00, /*  u16 idProduct; */
		0x00, 0x00, /*  u16 bcdDevice */

		STR_MANUFACTURER, /*  u8  iManufacturer; */
		STR_PRODUCT_MOUSE, /*  u8  iProduct; */
		STR_SERIALNUMBER, /*  u8  iSerialNumber; */
		0x01 /*  u8  bNumConfigurations; */
	};

	static const uint8_t qemu_mouse_config_descriptor[] = {
		/* one configuration */
		0x09, /*  u8  bLength; */
		0x02, /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01, /*  u8  bNumInterfaces; (1) */
		0x01, /*  u8  bConfigurationValue; */
		0x04, /*  u8  iConfiguration; */
		0xa0, /*  u8  bmAttributes;
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50, /*  u8  MaxPower; */

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
		0x09, /*  u8  bLength; */
		0x21, /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00, /*  u8 country_code */
		0x01, /*  u8 num_descriptors */
		0x22, /*  u8 type; Report */
		52, 0, /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07, /*  u8  ep_bLength; */
		0x05, /*  u8  ep_bDescriptorType; Endpoint */
		0x81, /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03, /*  u8  ep_bmAttributes; Interrupt */
		0x04, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a, /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
	};

	[[maybe_unused]] static const uint8_t qemu_tablet_config_descriptor[] = {
		/* one configuration */
		0x09, /*  u8  bLength; */
		0x02, /*  u8  bDescriptorType; Configuration */
		0x22, 0x00, /*  u16 wTotalLength; */
		0x01, /*  u8  bNumInterfaces; (1) */
		0x01, /*  u8  bConfigurationValue; */
		0x04, /*  u8  iConfiguration; */
		0xa0, /*  u8  bmAttributes;
                 Bit 7: must be set,
                     6: Self-powered,
                     5: Remote wakeup,
                     4..0: resvd */
		50, /*  u8  MaxPower; */

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
		0x09, /*  u8  bLength; */
		0x21, /*  u8 bDescriptorType; */
		0x01, 0x00, /*  u16 HID_class */
		0x00, /*  u8 country_code */
		0x01, /*  u8 num_descriptors */
		0x22, /*  u8 type; Report */
		74, 0, /*  u16 len */

		/* one endpoint (status change endpoint) */
		0x07, /*  u8  ep_bLength; */
		0x05, /*  u8  ep_bDescriptorType; Endpoint */
		0x81, /*  u8  ep_bEndpointAddress; IN Endpoint 1 */
		0x03, /*  u8  ep_bmAttributes; Interrupt */
		0x08, 0x00, /*  u16 ep_wMaxPacketSize; */
		0x0a, /*  u8  ep_bInterval; (255ms -- usb 2.0 spec) */
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
		0xc0, /*   End Collection */
		0xc0, /* End Collection */
	};

	static const uint8_t qemu_tablet_hid_report_descriptor[] = {
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
		0x15, 0x00, /*     Logical Minimum (0) */
		0x26, 0xff, 0x7f, /*     Logical Maximum (0x7fff) */
		0x35, 0x00, /*     Physical Minimum (0) */
		0x46, 0xff, 0x7f, /*     Physical Maximum (0x7fff) */
		0x75, 0x10, /*     Report Size (16) */
		0x95, 0x02, /*     Report Count (2) */
		0x81, 0x02, /*     Input (Data, Variable, Absolute) */
		0x05, 0x01, /*     Usage Page (Generic Desktop) */
		0x09, 0x38, /*     Usage (Wheel) */
		0x15, 0x81, /*     Logical Minimum (-0x7f) */
		0x25, 0x7f, /*     Logical Maximum (0x7f) */
		0x35, 0x00, /*     Physical Minimum (same as logical) */
		0x45, 0x00, /*     Physical Maximum (same as logical) */
		0x75, 0x08, /*     Report Size (8) */
		0x95, 0x01, /*     Report Count (1) */
		0x81, 0x06, /*     Input (Data, Variable, Relative) */
		0xc0, /*   End Collection */
		0xc0, /* End Collection */
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
		0xc0, /* End Collection */
	};

	static const uint8_t beatmania_dev_desc[] = {
		0x12, /*  u8 bLength; */
		0x01, /*  u8 bDescriptorType; Device */
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

		1, /*  u8  iManufacturer; */
		2, /*  u8  iProduct; */
		0, /*  u8  iSerialNumber; */
		0x01 /*  u8  bNumConfigurations; */
	};

	static const uint8_t beatmania_config_desc[] = {
		0x09, // bLength
		0x02, // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01, // bNumInterfaces 1
		0x01, // bConfigurationValue
		0x02, // iConfiguration (String Index)
		0xA0, // bmAttributes Remote Wakeup
		0x14, // bMaxPower 40mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0x03, // bInterfaceClass
		0x01, // bInterfaceSubClass
		0x01, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09, // bLength
		0x21, // bDescriptorType (HID)
		0x10, 0x01, // bcdHID 1.10
		0x0F, // bCountryCode
		0x01, // bNumDescriptors
		0x22, // bDescriptorType[0] (HID)
		0x44, 0x00, // wDescriptorLength[0] 68

		0x07, // bLength
		0x05, // bDescriptorType (Endpoint)
		0x81, // bEndpointAddress (IN/D2H)
		0x03, // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x0A, // bInterval 10 (unit depends on device speed)

		// 34 bytes
	};

	static const uint8_t beatmania_dadada_hid_report_descriptor[] = {
		0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
		0x09, 0x06, // Usage (Keyboard)
		0xA1, 0x01, // Collection (Application)
		0x05, 0x07, //   Usage Page (Kbrd/Keypad)
		0x19, 0xE0, //   Usage Minimum (0xE0)
		0x29, 0xE7, //   Usage Maximum (0xE7)
		0x15, 0x00, //   Logical Minimum (0)
		0x25, 0x01, //   Logical Maximum (1)
		0x75, 0x01, //   Report Size (1)
		0x95, 0x08, //   Report Count (8)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x08, //   Report Size (8)
		0x95, 0x01, //   Report Count (1)
		0x81, 0x01, //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x07, //   Usage Page (Kbrd/Keypad)
		0x19, 0x00, //   Usage Minimum (0x00)
		0x29, 0xFF, //   Usage Maximum (0xFF)
		0x15, 0x00, //   Logical Minimum (0)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x75, 0x08, //   Report Size (8)
		0x95, 0x06, //   Report Count (6)
		0x81, 0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x08, //   Usage Page (LEDs)
		0x19, 0x01, //   Usage Minimum (Num Lock)
		0x29, 0x05, //   Usage Maximum (Kana)
		0x15, 0x00, //   Logical Minimum (0)
		0x25, 0x01, //   Logical Maximum (1)
		0x75, 0x01, //   Report Size (1)
		0x95, 0x05, //   Report Count (5)
		0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x75, 0x03, //   Report Size (3)
		0x95, 0x01, //   Report Count (1)
		0x91, 0x01, //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0, // End Collection

		// 68 bytes
	};

	static constexpr const std::pair<QKeyCode, const char*> s_qkeycode_names[] = {
		{Q_KEY_CODE_0, "0"},
		{Q_KEY_CODE_1, "1"},
		{Q_KEY_CODE_2, "2"},
		{Q_KEY_CODE_3, "3"},
		{Q_KEY_CODE_4, "4"},
		{Q_KEY_CODE_5, "5"},
		{Q_KEY_CODE_6, "6"},
		{Q_KEY_CODE_7, "7"},
		{Q_KEY_CODE_8, "8"},
		{Q_KEY_CODE_9, "9"},
		{Q_KEY_CODE_A, "A"},
		{Q_KEY_CODE_AC_BACK, "ACBack"},
		{Q_KEY_CODE_AC_BOOKMARKS, "ACBookmarks"},
		{Q_KEY_CODE_AC_FORWARD, "ACForward"},
		{Q_KEY_CODE_AC_HOME, "ACHome"},
		{Q_KEY_CODE_AC_REFRESH, "ACRefresh"},
		{Q_KEY_CODE_AGAIN, "Again"},
		{Q_KEY_CODE_ALT, "Alt"},
		{Q_KEY_CODE_ALT_R, "Alt_r"},
		{Q_KEY_CODE_APOSTROPHE, "Apostrophe"},
		{Q_KEY_CODE_ASTERISK, "Asterisk"},
		{Q_KEY_CODE_AUDIOMUTE, "AudioMute"},
		{Q_KEY_CODE_AUDIONEXT, "AudioNext"},
		{Q_KEY_CODE_AUDIOPLAY, "AudioPlay"},
		{Q_KEY_CODE_AUDIOPREV, "AudioPrev"},
		{Q_KEY_CODE_AUDIOSTOP, "AudioStop"},
		{Q_KEY_CODE_B, "B"},
		{Q_KEY_CODE_BACKSLASH, "Backslash"},
		{Q_KEY_CODE_BACKSPACE, "Backspace"},
		{Q_KEY_CODE_BRACKET_LEFT, "BracketLeft"},
		{Q_KEY_CODE_BRACKET_RIGHT, "BracketRight"},
		{Q_KEY_CODE_C, "C"},
		{Q_KEY_CODE_CALCULATOR, "Calculator"},
		{Q_KEY_CODE_CAPS_LOCK, "Caps_lock"},
		{Q_KEY_CODE_COMMA, "Comma"},
		{Q_KEY_CODE_COMPOSE, "Compose"},
		{Q_KEY_CODE_COMPUTER, "Computer"},
		{Q_KEY_CODE_COPY, "Copy"},
		{Q_KEY_CODE_CTRL, "Control"},
		{Q_KEY_CODE_CTRL_R, "Control_r"},
		{Q_KEY_CODE_CUT, "Cut"},
		{Q_KEY_CODE_D, "D"},
		{Q_KEY_CODE_DELETE, "Delete"},
		{Q_KEY_CODE_DOT, "Dot"},
		{Q_KEY_CODE_DOWN, "Down"},
		{Q_KEY_CODE_E, "E"},
		{Q_KEY_CODE_END, "End"},
		{Q_KEY_CODE_EQUAL, "Equal"},
		{Q_KEY_CODE_ESC, "Escape"},
		{Q_KEY_CODE_F, "F"},
		{Q_KEY_CODE_F1, "F1"},
		{Q_KEY_CODE_F10, "F10"},
		{Q_KEY_CODE_F11, "F11"},
		{Q_KEY_CODE_F12, "F12"},
		{Q_KEY_CODE_F2, "F2"},
		{Q_KEY_CODE_F3, "F3"},
		{Q_KEY_CODE_F4, "F4"},
		{Q_KEY_CODE_F5, "F5"},
		{Q_KEY_CODE_F6, "F6"},
		{Q_KEY_CODE_F7, "F7"},
		{Q_KEY_CODE_F8, "F8"},
		{Q_KEY_CODE_F9, "F9"},
		{Q_KEY_CODE_FIND, "Find"},
		{Q_KEY_CODE_FRONT, "Front"},
		{Q_KEY_CODE_G, "G"},
		{Q_KEY_CODE_GRAVE_ACCENT, "Agrave"},
		{Q_KEY_CODE_H, "H"},
		{Q_KEY_CODE_HELP, "Help"},
		{Q_KEY_CODE_HENKAN, "Henkan"},
		{Q_KEY_CODE_HIRAGANA, "Hiragana"},
		{Q_KEY_CODE_HOME, "Home"},
		{Q_KEY_CODE_I, "I"},
		{Q_KEY_CODE_INSERT, "Insert"},
		{Q_KEY_CODE_J, "J"},
		{Q_KEY_CODE_K, "K"},
		{Q_KEY_CODE_KATAKANAHIRAGANA, "KatakanaHiragana"},
		{Q_KEY_CODE_KP_0, "Numpad0"},
		{Q_KEY_CODE_KP_1, "Numpad1"},
		{Q_KEY_CODE_KP_2, "Numpad2"},
		{Q_KEY_CODE_KP_3, "Numpad3"},
		{Q_KEY_CODE_KP_4, "Numpad4"},
		{Q_KEY_CODE_KP_5, "Numpad5"},
		{Q_KEY_CODE_KP_6, "Numpad6"},
		{Q_KEY_CODE_KP_7, "Numpad7"},
		{Q_KEY_CODE_KP_8, "Numpad8"},
		{Q_KEY_CODE_KP_9, "Numpad9"},
		{Q_KEY_CODE_KP_ADD, "NumpadPlus"},
		{Q_KEY_CODE_KP_COMMA, "NumpadComma"},
		{Q_KEY_CODE_KP_DECIMAL, "NumpadPeriod"},
		{Q_KEY_CODE_KP_DIVIDE, "NumpadSlash"},
		{Q_KEY_CODE_KP_ENTER, "NumpadReturn"},
		{Q_KEY_CODE_KP_EQUALS, "NumpadEqual"},
		{Q_KEY_CODE_KP_MULTIPLY, "NumpadAsterisk"},
		{Q_KEY_CODE_KP_SUBTRACT, "NumpadMinus"},
		{Q_KEY_CODE_L, "L"},
		{Q_KEY_CODE_LEFT, "Left"},
		{Q_KEY_CODE_LESS, "Less"},
		{Q_KEY_CODE_LF, "Lf"},
		{Q_KEY_CODE_M, "M"},
		{Q_KEY_CODE_MAIL, "Mail"},
		{Q_KEY_CODE_MEDIASELECT, "MediaSelect"},
		{Q_KEY_CODE_MENU, "Menu"},
		{Q_KEY_CODE_META_L, "Meta"},
		{Q_KEY_CODE_META_R, "Meta"},
		{Q_KEY_CODE_MINUS, "Minus"},
		{Q_KEY_CODE_MUHENKAN, "Muhenkan"},
		{Q_KEY_CODE_N, "N"},
		{Q_KEY_CODE_NUM_LOCK, "Num_lock"},
		{Q_KEY_CODE_O, "O"},
		{Q_KEY_CODE_OPEN, "Open"},
		{Q_KEY_CODE_P, "P"},
		{Q_KEY_CODE_PASTE, "Paste"},
		{Q_KEY_CODE_PAUSE, "Pause"},
		{Q_KEY_CODE_PGDN, "PageDown"},
		{Q_KEY_CODE_PGUP, "PageUp"},
		{Q_KEY_CODE_POWER, "Power"},
		{Q_KEY_CODE_PRINT, "Print"},
		{Q_KEY_CODE_PROPS, "Props"},
		{Q_KEY_CODE_Q, "Q"},
		{Q_KEY_CODE_R, "R"},
		{Q_KEY_CODE_RET, "Return"},
		{Q_KEY_CODE_RIGHT, "Right"},
		{Q_KEY_CODE_RO, "Ro"},
		{Q_KEY_CODE_S, "S"},
		{Q_KEY_CODE_SCROLL_LOCK, "Scroll_lock"},
		{Q_KEY_CODE_SEMICOLON, "Semicolon"},
		{Q_KEY_CODE_SHIFT, "Shift"},
		{Q_KEY_CODE_SHIFT_R, "Shift_r"},
		{Q_KEY_CODE_SLASH, "Slash"},
		{Q_KEY_CODE_SLEEP, "Sleep"},
		{Q_KEY_CODE_SPC, "Space"},
		{Q_KEY_CODE_STOP, "Stop"},
		{Q_KEY_CODE_SYSRQ, "Sysrq"},
		{Q_KEY_CODE_T, "T"},
		{Q_KEY_CODE_TAB, "Tab"},
		{Q_KEY_CODE_U, "U"},
		{Q_KEY_CODE_UNDO, "Undo"},
		{Q_KEY_CODE_UP, "Up"},
		{Q_KEY_CODE_V, "V"},
		{Q_KEY_CODE_VOLUMEDOWN, "VolumeDown"},
		{Q_KEY_CODE_VOLUMEUP, "VolumeUp"},
		{Q_KEY_CODE_W, "W"},
		{Q_KEY_CODE_WAKE, "Wake"},
		{Q_KEY_CODE_X, "X"},
		{Q_KEY_CODE_Y, "Y"},
		{Q_KEY_CODE_YEN, "Yen"},
		{Q_KEY_CODE_Z, "Z"},
	};

	static void usb_hid_changed(HIDState* hs)
	{
		UsbHIDState* us = USB_CONTAINER_OF(hs, UsbHIDState, hid);

		usb_wakeup(us->intr, 0);
	}

	static void usb_hid_handle_reset(USBDevice* dev)
	{
		UsbHIDState* us = USB_CONTAINER_OF(dev, UsbHIDState, dev);

		hid_reset(&us->hid);
	}

	static void usb_hid_handle_control(USBDevice* dev, USBPacket* p,
		int request, int value, int index, int length, uint8_t* data)
	{
		UsbHIDState* us = USB_CONTAINER_OF(dev, UsbHIDState, dev);
		HIDState* hs = &us->hid;
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
		UsbHIDState* us = USB_CONTAINER_OF(dev, UsbHIDState, dev);
		HIDState* hs = &us->hid;
		uint8_t buf[16];
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
						len = hid_pointer_poll(hs, buf, p->buffer_size);
					}
					else if (hs->kind == HID_KEYBOARD)
					{
						len = hid_keyboard_poll(hs, buf, p->buffer_size);
					}
					usb_packet_copy(p, buf, len);
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
		UsbHIDState* us = USB_CONTAINER_OF(dev, UsbHIDState, dev);
		hid_free(&us->hid);
		delete us;
	}

	UsbHIDState::UsbHIDState(u32 port_)
		: port(port_)
	{
	}

	// NOTE: This is really cruddy, reusing qemu's stuff here, when we could just do
	// it ourselves. But this code isn't used often enough to make it worthwhile.

	void UsbHIDState::QueueMouseButtonState(InputButton button, bool pressed)
	{
		InputEvent evt;
		evt.type = INPUT_EVENT_KIND_BTN;
		evt.u.btn.button = button;
		evt.u.btn.down = pressed;
		hid.ptr.eh_entry(&hid, &evt);
		hid.ptr.eh_sync(&hid);
	}

	void UsbHIDState::QueueMouseAxisState(InputPointerAxis axis, float delta)
	{
		if (axis < InputPointerAxis::WheelX)
		{
			// x/y
			InputEvent evt;
			evt.type = INPUT_EVENT_KIND_REL;
			evt.u.rel.axis = static_cast<InputAxis>(axis);
			evt.u.rel.value = static_cast<s64>(delta);
			hid.ptr.eh_entry(&hid, &evt);
			hid.ptr.eh_sync(&hid);
		}
		else if (axis == InputPointerAxis::WheelY)
		{
			InputEvent evt;
			evt.type = INPUT_EVENT_KIND_BTN;
			evt.u.btn.button = (delta > 0.0f) ? INPUT_BUTTON_WHEEL_DOWN : INPUT_BUTTON_WHEEL_UP;
			evt.u.btn.down = true;
			hid.ptr.eh_entry(&hid, &evt);
			hid.ptr.eh_sync(&hid);
		}
	}

	void UsbHIDState::SetKeycodeMapping()
	{
		for (const auto& [keycode, name] : s_qkeycode_names)
		{
			std::optional<u32> hkeycode(InputManager::ConvertHostKeyboardStringToCode(name));
			if (!hkeycode.has_value())
			{
				DevCon.WriteLn("(UsbHIDState): Missing host mapping for QKey '%s'", name);
				continue;
			}

			keycode_mapping.emplace(hkeycode.value(), keycode);
		}
	}

	void UsbHIDState::QueueKeyboardState(KeyValue keycode, bool pressed)
	{
		InputEvent evt;
		evt.type = INPUT_EVENT_KIND_KEY;
		evt.u.key.key = keycode;
		evt.u.key.down = pressed;
		hid.kbd.eh_entry(&hid, &evt);
	}

	USBDevice* HIDKbdDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		UsbHIDState* s = new UsbHIDState(port);
		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(kbd_dev_desc, sizeof(kbd_dev_desc), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(kbd_config_desc, sizeof(kbd_config_desc), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->hid, HID_KEYBOARD, usb_hid_changed);

		usb_hid_handle_reset(&s->dev);

		s->SetKeycodeMapping();

		return &s->dev;
	fail:
		usb_hid_unrealize(&s->dev);
		return nullptr;
	}

	void HIDKbdDevice::SetBindingValue(USBDevice* dev, u32 bind, float value) const
	{
		UsbHIDState* s = USB_CONTAINER_OF(dev, UsbHIDState, dev);

		const auto it = s->keycode_mapping.find(bind);
		if (it == s->keycode_mapping.end())
			return;

		KeyValue kv;
		kv.type = KEY_VALUE_KIND_QCODE;
		kv.u.qcode = it->second;
		s->QueueKeyboardState(kv, (value >= 0.5f));
	}

	const char* HIDKbdDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "HID Keyboard");
	}

	const char* HIDKbdDevice::TypeName() const
	{
		return "hidkbd";
	}

	std::span<const InputBindingInfo> HIDKbdDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo info[] = {
			{"Keyboard", TRANSLATE_NOOP("USB", "Keyboard"), InputBindingInfo::Type::Keyboard, 0, GenericInputBinding::Unknown},
		};
		return info;
	}

	bool HIDKbdDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		UsbHIDState* s = USB_CONTAINER_OF(dev, UsbHIDState, dev);

		if (!sw.DoMarker("HIDKbdDevice"))
			return false;

		sw.DoPODArray(s->hid.kbd.keycodes, std::size(s->hid.kbd.keycodes));
		sw.Do(&s->hid.kbd.modifiers);
		sw.Do(&s->hid.kbd.leds);
		sw.DoPODArray(s->hid.kbd.key, std::size(s->hid.kbd.key));
		sw.Do(&s->hid.kbd.keys);

		sw.Do(&s->hid.head);
		sw.Do(&s->hid.n);
		sw.Do(&s->hid.protocol);
		sw.Do(&s->hid.idle);
		sw.Do(&s->hid.idle_pending);

		return !sw.HasError();
	}

	USBDevice* HIDMouseDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		UsbHIDState* s = new UsbHIDState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(qemu_mouse_dev_descriptor, sizeof(qemu_mouse_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(qemu_mouse_config_descriptor, sizeof(qemu_mouse_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[STR_CONFIG_MOUSE];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->hid, HID_MOUSE, usb_hid_changed);

		usb_hid_handle_reset(&s->dev);

		return &s->dev;
	fail:
		usb_hid_unrealize(&s->dev);
		return nullptr;
	}

	const char* HIDMouseDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "HID Mouse");
	}

	const char* HIDMouseDevice::TypeName() const
	{
		return "hidmouse";
	}

	bool HIDMouseDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		UsbHIDState* s = USB_CONTAINER_OF(dev, UsbHIDState, dev);

		if (!sw.DoMarker("HIDMouseDevice"))
			return false;

		sw.DoPODArray(s->hid.ptr.queue, std::size(s->hid.ptr.queue));
		sw.Do(&s->hid.ptr.mouse_grabbed);

		sw.Do(&s->hid.head);
		sw.Do(&s->hid.n);
		sw.Do(&s->hid.protocol);
		sw.Do(&s->hid.idle);
		sw.Do(&s->hid.idle_pending);

		return !sw.HasError();
	}

	std::span<const InputBindingInfo> HIDMouseDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo info[] = {
			{"Pointer", TRANSLATE_NOOP("USB", "Pointer"), InputBindingInfo::Type::Pointer, INPUT_BUTTON__MAX, GenericInputBinding::Unknown},
			{"LeftButton", TRANSLATE_NOOP("USB", "Left Button"), InputBindingInfo::Type::Button, INPUT_BUTTON_LEFT, GenericInputBinding::Unknown},
			{"RightButton", TRANSLATE_NOOP("USB", "Right Button"), InputBindingInfo::Type::Button, INPUT_BUTTON_RIGHT, GenericInputBinding::Unknown},
			{"MiddleButton", TRANSLATE_NOOP("USB", "Middle Button"), InputBindingInfo::Type::Button, INPUT_BUTTON_MIDDLE, GenericInputBinding::Unknown},
		};
		return info;
	}

	float HIDMouseDevice::GetBindingValue(const USBDevice* dev, u32 bind) const
	{
		const UsbHIDState* s = USB_CONTAINER_OF(dev, const UsbHIDState, dev);

		if (bind >= INPUT_BUTTON__MAX)
		{
			// axis, don't bother returning, we don't have an absolute value here anyway
			return 0.0f;
		}

		const int index = (s->hid.n ? s->hid.head : s->hid.head - 1);
		const HIDPointerEvent* e = &s->hid.ptr.queue[index & QUEUE_MASK];

		static const int bmap[INPUT_BUTTON__MAX] = {
			/*[INPUT_BUTTON_LEFT] =*/0x01,
			/*[INPUT_BUTTON_MIDDLE] =*/0x04,
			/*[INPUT_BUTTON_RIGHT] =*/0x02,
			0, 0, 0, 0};

		return ((e->buttons_state & bmap[bind]) != 0) ? 1.0f : 0.0f;
	}

	void HIDMouseDevice::SetBindingValue(USBDevice* dev, u32 bind, float value) const
	{
		UsbHIDState* s = USB_CONTAINER_OF(dev, UsbHIDState, dev);

		if (bind < INPUT_BUTTON__MAX)
			s->QueueMouseButtonState(static_cast<InputButton>(bind), (value >= 0.5f));
		else
			s->QueueMouseAxisState(static_cast<InputPointerAxis>(bind - INPUT_BUTTON__MAX), value);
	}

	// ---- BeatMania Da Da Da!! ----

	USBDevice* BeatManiaDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		UsbHIDState* s = new UsbHIDState(port);

		s->desc.full = &s->desc_dev;
		s->desc.str = beatmania_dadada_desc_strings;

		if (usb_desc_parse_dev(beatmania_dev_desc, sizeof(beatmania_dev_desc), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(beatmania_config_desc, sizeof(beatmania_config_desc), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_hid_handle_reset;
		s->dev.klass.handle_control = usb_hid_handle_control;
		s->dev.klass.handle_data = usb_hid_handle_data;
		s->dev.klass.unrealize = usb_hid_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		s->intr = usb_ep_get(&s->dev, USB_TOKEN_IN, 1);
		hid_init(&s->hid, HID_KEYBOARD, usb_hid_changed);
		s->hid.sub_kind = HID_SUBKIND_BEATMANIA;

		usb_hid_handle_reset(&s->dev);

		s->SetKeycodeMapping();

		return &s->dev;
	fail:
		usb_hid_unrealize(&s->dev);
		return nullptr;
	}

	const char* BeatManiaDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "BeatMania Da Da Da!! Keyboard");
	}

	const char* BeatManiaDevice::TypeName() const
	{
		return "beatmania";
	}

	bool BeatManiaDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		return HIDKbdDevice::Freeze(dev, sw);
	}

} // namespace usb_hid
