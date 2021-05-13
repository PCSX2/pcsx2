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
#include "usb-mic-singstar.h"
#include "audio.h"
#include "USB/qemu-usb/desc.h"
#include "USB/shared/inifile_usb.h"

namespace usb_mic
{

	static const uint8_t logitech_mic_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0110), //(272)
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046D),
		/* idProduct           */ WBVAL(0x0000), //(0)
		/* bcdDevice           */ WBVAL(0x0001), //(1)
		/* iManufacturer       */ 0x01,          //(1)
		/* iProduct            */ 0x02,          //(2)
		/* iSerialNumber       */ 0x00,          //(0) unused
		/* bNumConfigurations  */ 0x01,          //(1)

	};

	static const uint8_t logitech_mic_config_descriptor[] = {

		/* Configuration 1 */
		0x09,                              /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(0x00b1),                     /* wTotalLength */
		0x02,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		USB_CONFIG_BUS_POWERED,            /* bmAttributes */
		USB_CONFIG_POWER_MA(90),           /* bMaxPower */

		/* Interface 0, Alternate Setting 0, Audio Control */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x00,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOCONTROL,   /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Control Interface */
		AUDIO_CONTROL_INTERFACE_DESC_SZ(1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,    /* bDescriptorType */
		AUDIO_CONTROL_HEADER,               /* bDescriptorSubtype */
		WBVAL(0x0100), /* 1.00 */           /* bcdADC */
		WBVAL(0x0028),                      /* wTotalLength */
		0x01,                               /* bInCollection */
		0x01,                               /* baInterfaceNr */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE,           /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,          /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL,             /* bDescriptorSubtype */
		0x01,                                     /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE),         /* wTerminalType */
		0x02,                                     /* bAssocTerminal */
		0x02,                                     /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L | AUDIO_CHANNEL_R), /* wChannelConfig */
		0x00,                                     /* iChannelNames */
		0x00,                                     /* iTerminal */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE,     /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL,       /* bDescriptorSubtype */
		0x02,                                /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING), /* wTerminalType */
		0x01,                                /* bAssocTerminal */
		0x03,                                /* bSourceID */
		0x00,                                /* iTerminal */

		/* Audio Feature Unit */
		AUDIO_FEATURE_UNIT_DESC_SZ(2, 1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT,       /* bDescriptorSubtype */
		0x03,                             /* bUnitID */
		0x01,                             /* bSourceID */
		0x01,                             /* bControlSize */
		0x01,                             /* bmaControls(0) */
		0x02,                             /* bmaControls(1) */
		0x02,                             /* bmaControls(2) */
		0x00,                             /* iTerminal */

		/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Interface 1, Alternate Setting 1, Audio Streaming - Operational */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x01,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x02,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE,     /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I,             /* bFormatType */
		0x01,                            /* bNrChannels */
		0x02,                            /* bSubFrameSize */
		0x10,                            /* bBitResolution */
		0x05,                            /* bSamFreqType */
		B3VAL(8000),                     /* tSamFreq 1 */
		B3VAL(11025),                    /* tSamFreq 2 */
		B3VAL(22050),                    /* tSamFreq 3 */
		B3VAL(44100),                    /* tSamFreq 4 */
		B3VAL(48000),                    /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                              /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                                   /* bDescriptorType */
		USB_ENDPOINT_IN(1),                                             /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x0064),                                                  /* wMaxPacketSize */
		0x01,                                                           /* bInterval */
		0x00,                                                           /* bRefresh */
		0x00,                                                           /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Interface 1, Alternate Setting 2, Audio Streaming - ? */
		USB_INTERFACE_DESC_SIZE,       /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x02,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x02,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE,     /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I,             /* bFormatType */
		0x02,                            /* bNrChannels */
		0x02,                            /* bSubFrameSize */
		0x10,                            /* bBitResolution */
		0x05,                            /* bSamFreqType */
		B3VAL(8000),                     /* tSamFreq 1 */
		B3VAL(11025),                    /* tSamFreq 2 */
		B3VAL(22050),                    /* tSamFreq 3 */
		B3VAL(44100),                    /* tSamFreq 4 */
		B3VAL(48000),                    /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                              /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                                   /* bDescriptorType */
		USB_ENDPOINT_IN(1),                                             /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x00c8),                                                  /* wMaxPacketSize */
		0x01,                                                           /* bInterval */
		0x00,                                                           /* bRefresh */
		0x00,                                                           /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Terminator */
		0 /* bLength */
	};

	static const USBDescStrings lt_desc_strings = {
		"",
		"Logitech",
		"USBMIC",
	};

	//Minified state
	typedef struct SINGSTARMICMINIState
	{
		USBDevice dev;

		USBDesc desc;
		USBDescDevice desc_dev;
	} SINGSTARMICMINIState;

	USBDevice* LogitechMicDevice::CreateDevice(int port)
	{
		std::string api;
#ifdef _WIN32
		std::wstring tmp;
		if (!LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp))
			return nullptr;
		api = wstr_to_str(tmp);
#else
		if (!LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, api))
			return nullptr;
#endif

		USBDevice* dev = SingstarDevice::CreateDevice(port, api);
		if (!dev)
			return nullptr;

		SINGSTARMICMINIState* s = (SINGSTARMICMINIState*)dev;
		s->desc = {};
		s->desc_dev = {};

		s->desc.str = lt_desc_strings;
		s->desc.full = &s->desc_dev;

		if (usb_desc_parse_dev(logitech_mic_dev_descriptor, sizeof(logitech_mic_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(logitech_mic_config_descriptor, sizeof(logitech_mic_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = lt_desc_strings[2];
		usb_desc_init(&s->dev);
		return dev;
	fail:
		s->dev.klass.unrealize(dev);
		return nullptr;
	}

} // namespace usb_mic
