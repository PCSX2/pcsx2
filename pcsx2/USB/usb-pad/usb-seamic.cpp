/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "padproxy.h"
#include "usb-pad.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-mic/usb-mic-singstar.h"
#include "USB/shared/inifile_usb.h"

namespace usb_pad
{

	static const USBDescStrings desc_strings = {
		"",
		"ASCII CORPORATION",
		"ASCII Mic/Joy-stick",
	};

	static const uint8_t dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0110),
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x08, //(8)
		/* idVendor            */ WBVAL(0x0B49),
		/* idProduct           */ WBVAL(0x0644),
		/* bcdDevice           */ WBVAL(0x0100),
		/* iManufacturer       */ 0x01,
		/* iProduct            */ 0x02,
		/* iSerialNumber       */ 0x00,
		/* bNumConfigurations  */ 0x01,
	};

	static const uint8_t hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0x09, 0x01,       //   Usage (Pointer)
		0xA1, 0x00,       //   Collection (Physical)
		0x95, 0x03,       //     Report Count (3)
		0x75, 0x08,       //     Report Size (8)
		0x15, 0x00,       //     Logical Minimum (0)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x66, 0x00, 0x00, //     Unit (None)
		0x05, 0x01,       //     Usage Page (Generic Desktop Ctrls)
		0x09, 0x30,       //     Usage (X)
		0x09, 0x31,       //     Usage (Y)
		0x09, 0x32,       //     Usage (Z)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x01,       //     Report Count (1)
		0x75, 0x04,       //     Report Size (4)
		0x15, 0x00,       //     Logical Minimum (0)
		0x25, 0x07,       //     Logical Maximum (7)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0x3B, 0x01, //     Physical Maximum (315)
		0x66, 0x14, 0x00, //     Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,       //     Usage (Hat switch)
		0x81, 0x42,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x95, 0x0A,       //     Report Count (10)
		0x75, 0x01,       //     Report Size (1)
		0x15, 0x00,       //     Logical Minimum (0)
		0x25, 0x01,       //     Logical Maximum (1)
		0x35, 0x00,       //     Physical Minimum (0)
		0x45, 0x01,       //     Physical Maximum (1)
		0x66, 0x00, 0x00, //     Unit (None)
		0x05, 0x09,       //     Usage Page (Button)
		0x19, 0x01,       //     Usage Minimum (0x01)
		0x29, 0x0A,       //     Usage Maximum (0x0A)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x02,       //     Report Count (2)
		0x81, 0x01,       //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x08,       //     Report Count (8)
		0x75, 0x01,       //     Report Size (1)
		0x05, 0x08,       //     Usage Page (LEDs)
		0x19, 0x01,       //     Usage Minimum (Num Lock)
		0x29, 0x08,       //     Usage Maximum (Do Not Disturb)
		0x91, 0x02,       //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             //   End Collection
		0xC0,             // End Collection

		// 98 bytes
	};

	static const uint8_t config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x86, 0x00, // wTotalLength 134
		0x03,       // bNumInterfaces 3
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x31,       // bMaxPower 98mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x00, // bNumEndpoints 0
		0x01, // bInterfaceClass (Audio)
		0x01, // bInterfaceSubClass (Audio Control)
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x01,       // bDescriptorSubtype (CS_INTERFACE -> HEADER)
		0x00, 0x01, // bcdADC 1.00
		WBVAL(38),  // wTotalLength 38
		0x01,       // binCollection 0x01
		0x01,       // baInterfaceNr 1

		0x0C,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x02,       // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
		0x01,       // bTerminalID
		0x01, 0x02, // wTerminalType (Microphone)
		0x02,       // bAssocTerminal
		0x01,       // bNrChannels 1
		0x00, 0x00, // wChannelConfig
		0x00,       // iChannelNames
		0x00,       // iTerminal

		0x09,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x03,       // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
		0x02,       // bTerminalID
		0x01, 0x01, // wTerminalType (USB Streaming)
		0x01,       // bAssocTerminal
		0x03,       // bSourceID
		0x00,       // iTerminal

		0x08,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x06,       // bDescriptorSubtype (CS_INTERFACE -> FEATURE_UNIT)
		0x03,       // bUnitID
		0x01,       // bSourceID
		0x01,       // bControlSize 1
		0x03, 0x00, // bmaControls[0] (Mute,Volume)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x01, // bInterfaceNumber 1
		0x00, // bAlternateSetting
		0x00, // bNumEndpoints 0
		0x01, // bInterfaceClass (Audio)
		0x02, // bInterfaceSubClass (Audio Streaming)
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x01, // bInterfaceNumber 1
		0x01, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0x01, // bInterfaceClass (Audio)
		0x02, // bInterfaceSubClass (Audio Streaming)
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x01,       // bDescriptorSubtype (CS_INTERFACE -> AS_GENERAL)
		0x02,       // bTerminalLink
		0x01,       // bDelay 1
		0x01, 0x00, // wFormatTag (PCM)

		0x0E,         // bLength
		0x24,         // bDescriptorType (See Next Line)
		0x02,         // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
		0x01,         // bFormatType 1
		0x01,         // bNrChannels (Mono)
		0x02,         // bSubFrameSize 2
		0x10,         // bBitResolution 16
		0x02,         // bSamFreqType 2
		B3VAL(8000),  // tSamFreq[1] 8000 Hz
		B3VAL(11025), // tSamFreq[2] 11025 Hz

		0x07,                         // bLength
		USB_ENDPOINT_DESCRIPTOR_TYPE, // bDescriptorType (See Next Line)
		USB_ENDPOINT_IN(1),           // bEndpointAddress (IN/D2H)
		0x01,                         // bmAttributes (Isochronous, No Sync, Data EP)
		WBVAL(100),                   // wMaxPacketSize 100
		0x01,                         // bInterval 1 (unit depends on device speed)

		0x07,       // bLength
		0x25,       // bDescriptorType (See Next Line)
		0x01,       // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
		0x01,       // bmAttributes (Sampling Freq Control)
		0x00,       // bLockDelayUnits
		0x00, 0x00, // wLockDelay 0

		USB_INTERFACE_DESC_SIZE,       // bLength
		USB_INTERFACE_DESCRIPTOR_TYPE, // bDescriptorType (Interface)
		0x02,                          // bInterfaceNumber 2
		0x00,                          // bAlternateSetting
		0x01,                          // bNumEndpoints 1
		USB_CLASS_HID,                 // bInterfaceClass
		0x00,                          // bInterfaceSubClass
		0x00,                          // bInterfaceProtocol
		0x00,                          // iInterface (String Index)

		0x09,          // bLength
		USB_DT_HID,    // bDescriptorType (HID)
		WBVAL(0x0100), // bcdHID 1.00
		0x00,          // bCountryCode
		0x01,          // bNumDescriptors
		USB_DT_REPORT, // bDescriptorType[0] (HID)
		WBVAL(98),     // wDescriptorLength[0] 98

		0x07,                         // bLength
		USB_ENDPOINT_DESCRIPTOR_TYPE, // bDescriptorType (Endpoint)
		USB_ENDPOINT_IN(2),           // bEndpointAddress (IN/D2H)
		USB_ENDPOINT_TYPE_INTERRUPT,  // bmAttributes (Interrupt)
		WBVAL(8),                     // wMaxPacketSize 8
		0x0A,                         // bInterval 10 (unit depends on device speed)

		// 134 bytes
	};

	std::list<std::string> SeamicDevice::ListAPIs()
	{
		return RegisterPad::instance().Names();
	}

	const TCHAR* SeamicDevice::LongAPIName(const std::string& name)
	{
		auto proxy = RegisterPad::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}

	typedef struct SeamicState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;
		USBDevice* mic;
		Pad* pad;
		uint8_t port;
		struct freeze
		{
			int nothing;
		} f;
	} SeamicState;

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		SeamicState* s = (SeamicState*)dev;
		uint8_t data[64];

		int ret = 0;
		uint8_t devep = p->ep->nr;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1 && s->mic)
				{
					s->mic->klass.handle_data(s->mic, p);
				}
				else if (devep == 2 && s->pad)
				{
					ret = s->pad->TokenIn(data, p->iov.size);
					if (ret > 0)
						usb_packet_copy(p, data, MIN((unsigned long)ret, sizeof(data)));
					else
						p->status = ret;
				}
				else
				{
					goto fail;
				}
				break;
			case USB_TOKEN_OUT:
				usb_packet_copy(p, data, MIN(p->iov.size, sizeof(data)));
				ret = s->pad->TokenOut(data, p->iov.size);
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_reset(USBDevice* dev)
	{
		/* XXX: do it */
		SeamicState* s = (SeamicState*)dev;
		s->pad->Reset();
		s->mic->klass.handle_reset(s->mic);
		return;
	}

	static void pad_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
								   int index, int length, uint8_t* data)
	{
		int ret = 0;

		switch (request)
		{
			case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret < 0)
					goto fail;

				break;
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR:
				switch (value >> 8)
				{
					case USB_DT_REPORT:
						ret = sizeof(hid_report_descriptor);
						memcpy(data, hid_report_descriptor, ret);
						p->actual_length = ret;
						break;
					default:
						goto fail;
				}
				break;
			/* hid specific requests */
			case SET_REPORT:
				if (length > 0)
				{
					p->actual_length = 0;
					//p->status = USB_RET_SUCCESS;
				}
				break;
			case SET_IDLE:
				break;
			default:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret >= 0)
				{
					return;
				}
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_destroy(USBDevice* dev)
	{
		SeamicState* s = (SeamicState*)dev;
		s->mic->klass.unrealize(s->mic);
		s->mic = nullptr;
		delete s;
	}

	static int pad_open(USBDevice* dev)
	{
		SeamicState* s = (SeamicState*)dev;
		if (s)
		{
			s->mic->klass.open(s->mic);
			return s->pad->Open();
		}
		return 1;
	}

	static void pad_close(USBDevice* dev)
	{
		SeamicState* s = (SeamicState*)dev;
		if (s)
		{
			s->mic->klass.close(s->mic);
			s->pad->Close();
		}
	}

	USBDevice* SeamicDevice::CreateDevice(int port)
	{
		std::string varApi;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp);
		varApi = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, varApi);
#endif
		PadProxyBase* proxy = RegisterPad::instance().Proxy(varApi);
		if (!proxy)
		{
			Console.WriteLn("USB: PAD: Invalid input API.\n");
			return NULL;
		}

		std::string api;


#ifdef _WIN32
		if (!LoadSetting(nullptr, port, usb_mic::SingstarDevice::TypeName(), N_DEVICE_API, tmp))
			return nullptr;
		api = wstr_to_str(tmp);	
#else
		if (!LoadSetting(nullptr, port, usb_mic::SingstarDevice::TypeName(), N_DEVICE_API, api))
			return nullptr;
#endif

		USBDevice* mic = usb_mic::SingstarDevice::CreateDevice(port, api);
		if (!mic)
			return nullptr;

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return NULL;

		pad->Type(WT_SEGA_SEAMIC);
		SeamicState* s = new SeamicState();

		s->mic = mic;
		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_descriptor, sizeof(config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->pad = pad;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.open = pad_open;
		s->dev.klass.close = pad_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2]; //not really used
		s->port = port;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset((USBDevice*)s);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int SeamicDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int SeamicDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return 0;
		//  SeamicState *s = (SeamicState *)dev;
		// 	switch (mode)
		// 	{
		// 		case FREEZE_LOAD:
		// 			if (!s) return -1;
		// 			s->f = *(SeamicState::freeze *)data;
		// 			return sizeof(SeamicState::freeze);
		// 		case FREEZE_SAVE:
		// 			if (!s) return -1;
		// 			return sizeof(SeamicState::freeze);
		// 		case FREEZE_SIZE:
		// 			return sizeof(SeamicState::freeze);
		// 		default:
		// 		break;
		// 	}
		// 	return -1;
	}

} // namespace usb_pad
