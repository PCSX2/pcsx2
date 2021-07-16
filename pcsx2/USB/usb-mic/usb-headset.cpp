/*
 * QEMU USB HID devices
 *
 * Copyright (c) 2005 Fabrice Bellard
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

// Most stuff is based on Qemu 1.7 USB soundcard passthrough code.

#include "PrecompiledHeader.h"
#include "USB/qemu-usb/vl.h"
#include "USB/qemu-usb/desc.h"
#include "USB/shared/inifile_usb.h"
#include <assert.h>

#include "audio.h"
#include "usb-headset.h"

#define BUFFER_FRAMES 200

/*
 * A Basic Audio Device uses these specific values
 */
#define USBAUDIO_PACKET_SIZE 200 //192
#define USBAUDIO_SAMPLE_RATE 48000
#define USBAUDIO_PACKET_INTERVAL 1

namespace usb_mic
{

	static FILE* file = NULL;

	typedef struct HeadsetState
	{
		USBDevice dev;
		AudioDevice* audsrc;
		AudioDevice* audsink;
		AudioDeviceProxyBase* audsrcproxy;

		struct freeze
		{
			int intf;
			MicMode mode;

			/* state */
			struct
			{
				bool mute;
				uint8_t vol[2];
				uint32_t srate;
			} out;

			struct
			{
				bool mute;
				uint8_t vol;
				uint32_t srate;
			} in;

			struct
			{
				bool mute;
				uint8_t vol[2];
			} mixer; //TODO
		} f;         //freezable

		std::vector<int16_t> in_buffer;
		std::vector<int16_t> out_buffer;

		USBDesc desc;
		USBDescDevice desc_dev;
	} HeadsetState;

	std::list<std::string> HeadsetDevice::ListAPIs()
	{
		return RegisterAudioDevice::instance().Names();
	}
	const TCHAR* HeadsetDevice::LongAPIName(const std::string& name)
	{
		return RegisterAudioDevice::instance().Proxy(name)->Name();
	}

	static const uint8_t headset_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0110), //(272)
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x40,          //(64)
		/* idVendor            */ WBVAL(0x046d), //Logitech
		/* idProduct           */ WBVAL(0x0a01), //"USB headset" from usb.ids
		/* bcdDevice           */ WBVAL(0x1012), //(10.12)
		/* iManufacturer       */ 0x01,          //(1)
		/* iProduct            */ 0x02,          //(2)
		/* iSerialNumber       */ 0x00,          //(0) unused
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	static const uint8_t headset_config_descriptor[] = {

		/* Configuration 1 */
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(318),                        /* wTotalLength */
		0x03,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		USB_CONFIG_BUS_POWERED,            /* bmAttributes */
		USB_CONFIG_POWER_MA(100),          /* bMaxPower */

		/* Interface 0, Alternate Setting 0, Audio Control */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x00,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOCONTROL,   /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Control Interface */
		AUDIO_CONTROL_INTERFACE_DESC_SZ(2), /* bLength : 8+n */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,    /* bDescriptorType */
		AUDIO_CONTROL_HEADER,               /* bDescriptorSubtype */
		WBVAL(0x0100), /* 1.00 */           /* bcdADC */
		WBVAL(0x0075),                      /* wTotalLength : this + following unit/terminal sizes */
		0x02,                               /* bInCollection */
		0x01,                               /* baInterfaceNr(0) */
		0x02,                               /* baInterfaceNr(1) */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE,   /* bLength : 12 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL,     /* bDescriptorSubtype */
		0x0d,                             /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE), /* wTerminalType */
		0x00,                             /* bAssocTerminal */
		0x01,                             /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L),           /* wChannelConfig */
		0x00,                             /* iChannelNames */
		0x00,                             /* iTerminal */

		/* Audio Feature Unit 6 */
		AUDIO_FEATURE_UNIT_DESC_SZ(1, 1), /* bLength : f(ch,n) = 7 + (ch+1)*n */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT,       /* bDescriptorSubtype */
		0x06,                             /* bUnitID */
		0x0d,                             /* bSourceID */
		0x01,                             /* bControlSize */
		0x03, /* bmaControls(0) */        /* mute / volume */
		0x00,                             /* bmaControls(1) */
		0x00,                             /* iFeature */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE,             /* bLength : 12 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,            /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL,               /* bDescriptorSubtype */
		0x0c,                                       /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING),        /* wTerminalType */
		0x00,                                       /* bAssocTerminal */
		0x02,                                       /* bNrChannels */
		WBVAL((AUDIO_CHANNEL_L | AUDIO_CHANNEL_R)), /* wChannelConfig */
		0x00,                                       /* iChannelNames */
		0x00,                                       /* iTerminal */

		AUDIO_MIXER_UNIT_DESC_SZ(2, 1), //0x0A+p+n //0x0d
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,
		AUDIO_CONTROL_MIXER_UNIT,
		0x09,                                       /* bUnitID */
		0x02,                                       /* bNrInPins */
		0x0c,                                       /* baSourceID( 0) */
		0x06,                                       /* baSourceID( 1) */
		0x02,                                       /* bNrChannels */
		WBVAL((AUDIO_CHANNEL_L | AUDIO_CHANNEL_R)), /* wChannelConfig */
		0,                                          /* iChannelNames */
		0x00,                                       /* bmControls */
		0,                                          /* iMixer */

		/* Audio Feature Unit 1 */
		AUDIO_FEATURE_UNIT_DESC_SZ(2, 1), /* bLength : f(ch,n) = 7 + (ch+1)*n */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT,       /* bDescriptorSubtype */
		0x01,                             /* bUnitID */
		0x09,                             /* bSourceID */
		0x01,                             /* bControlSize */
		0x01, /* bmaControls(0) */        /* mute */
		0x02, /* bmaControls(1) */        /* volume */
		0x02, /* bmaControls(2) */        /* volume */
		0x00,                             /* iFeature */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE, /* bLength : 9 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL,   /* bDescriptorSubtype */
		0x0e,                            /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_SPEAKER),   /* wTerminalType */
		0x00,                            /* bAssocTerminal */
		0x01,                            /* bSourceID */
		0x00,                            /* iTerminal */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE,   /* bLength : 12 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL,     /* bDescriptorSubtype */
		0x0b,                             /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE), /* wTerminalType */
		0x00,                             /* bAssocTerminal */
		0x01,                             /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L),           /* wChannelConfig */
		0x00,                             /* iChannelNames */
		0x00,                             /* iTerminal */

		/* Audio Feature Unit 2 */
		AUDIO_FEATURE_UNIT_DESC_SZ(1, 1), /* bLength : f(ch,n) = 7 + (ch+1)*n */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,  /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT,       /* bDescriptorSubtype */
		0x02,                             /* bUnitID */
		0x0b,                             /* bSourceID */
		0x01,                             /* bControlSize */
		0x03, /* bmaControls(0) */        /* mute, volume */
		0x00,                             /* bmaControls(1) */
		0x00,                             /* iFeature */

		AUDIO_MIXER_UNIT_DESC_SZ(1, 1),
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,
		AUDIO_CONTROL_MIXER_UNIT,
		0x07,                   /* bUnitID */
		0x01,                   /* bNrInPins */
		0x02,                   /* baSourceID( 0) */
		0x01,                   /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L), /* wChannelConfig */
		0,                      /* iChannelNames */
		0x00,                   /* bmControls */
		0,                      /* iMixer */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE,     /* bLength : 9 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL,       /* bDescriptorSubtype */
		0x0a,                                /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING), /* wTerminalType */
		0x00,                                /* bAssocTerminal */
		0x07,                                /* bSourceID */
		0x00,                                /* iTerminal */

		/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Interface 1, Alternate Setting 1, Audio Streaming - Operational */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x01,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength : 7 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x0c,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength : 8 + (n*3) */
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
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                          /* bLength : 9 */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                               /* bDescriptorType */
		USB_ENDPOINT_OUT(1),                                        /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ADAPTIVE, /* bmAttributes */
		WBVAL(0x00c0),                                              /* wMaxPacketSize */
		0x01,                                                       /* bInterval */
		0x00,                                                       /* bRefresh */
		0x00,                                                       /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength : 7 */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Interface 1, Alternate Setting 2, Audio Streaming - Operational */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01,                          /* bInterfaceNumber */
		0x02,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength : 7 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x0c,                                /* bTerminalLink */
		0x01,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength : 8 + (n*3) */
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
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                          /* bLength : 9 */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                               /* bDescriptorType */
		USB_ENDPOINT_OUT(1),                                        /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ADAPTIVE, /* bmAttributes */
		WBVAL(0x0060),                                              /* wMaxPacketSize */
		0x01,                                                       /* bInterval */
		0x00,                                                       /* bRefresh */
		0x00,                                                       /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength : 7 */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x00,                               /* bLockDelayUnits */
		WBVAL(0x0000),                      /* wLockDelay */

		/* Interface 2, Alternate Setting 0 */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x02,                          /* bInterfaceNumber */
		0x00,                          /* bAlternateSetting */
		0x00,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Interface 2, Alternate Setting 1 */
		USB_INTERFACE_DESC_SIZE,       /* bLength : 9 */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x02,                          /* bInterfaceNumber */
		0x01,                          /* bAlternateSetting */
		0x01,                          /* bNumEndpoints */
		USB_CLASS_AUDIO,               /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED,      /* bInterfaceProtocol */
		0x00,                          /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength : 7 */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_STREAMING_GENERAL,             /* bDescriptorSubtype */
		0x0a,                                /* bTerminalLink */
		0x00,                                /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM),             /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5),  /* bLength : 8+(n*3) */
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
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE,                          /* bLength : 9 */
		USB_ENDPOINT_DESCRIPTOR_TYPE,                               /* bDescriptorType */
		USB_ENDPOINT_IN(4),                                         /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ADAPTIVE, /* bmAttributes */
		WBVAL(0x0060),                                              /* wMaxPacketSize */
		0x01,                                                       /* bInterval */
		0x00,                                                       /* bRefresh */
		0x00,                                                       /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength : 7 */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE,     /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL,             /* bDescriptor */
		0x01,                               /* bmAttributes */
		0x02,                               /* bLockDelayUnits (PCM samples) */
		WBVAL(0x0001),                      /* wLockDelay */

		/* Terminator */
		0 /* bLength */
	};

	static const USBDescStrings desc_strings = {
		"",
		"Logitech", // Atleast SOCOM II checks this (liblgaud?)
		"Logitech USB Headset",
		"00000000"};

	static void headset_handle_reset(USBDevice* dev)
	{
		/* XXX: do it */
		return;
	}

/*
 * Note: we arbitrarily map the volume control range onto -inf..+8 dB
 */
#define ATTRIB_ID(cs, attrib, idif) \
	(((cs) << 24) | ((attrib) << 16) | (idif))


	// With current descriptor, if I'm not mistaken,
	// feature unit 2 (0x0100): headphones
	// feature unit 5 (0x0600): microphone

	static int usb_audio_get_control(HeadsetState* s, uint8_t attrib,
									 uint16_t cscn, uint16_t idif,
									 int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		const uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		int ret = USB_RET_STALL;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0600):
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0200):
				data[0] = s->f.in.mute;
				ret = 1;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0600):
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0200):
				//if (cn < 2) //TODO
				{
					uint16_t vol = (s->f.in.vol * 0x8800 + 127) / 255 + 0x8000;
					data[0] = (uint8_t)(vol & 0xFF);
					data[1] = vol >> 8;
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MIN, 0x0600):
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MIN, 0x0200):
				//if (cn < 2)
				{
					data[0] = 0x01;
					data[1] = 0x80;
					//data[0] = 0x00;
					//data[1] = 0xE1; //0xE100 -31dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MAX, 0x0600):
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MAX, 0x0200):
				//if (cn < 2)
				{
					data[0] = 0x00;
					data[1] = 0x08;
					//data[0] = 0x00;
					//data[1] = 0x18; //0x1800 +24dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_RES, 0x0600):
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_RES, 0x0200):
				//if (cn < 2)
				{
					data[0] = 0x88;
					data[1] = 0x00;
					//data[0] = 0x00;
					//data[1] = 0x01; //0x0100 1.0 dB
					ret = 2;
				}
				break;

			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0100):
				data[0] = s->f.out.mute;
				ret = 1;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0100):
				if (cn < 2) //TODO
				{
					uint16_t vol = (s->f.out.vol[cn] * 0x8800 + 127) / 255 + 0x8000;
					data[0] = (uint8_t)(vol & 0xFF);
					data[1] = vol >> 8;
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MIN, 0x0100):
				//if (cn < 2)
				{
					data[0] = 0x01;
					data[1] = 0x80;
					//data[0] = 0x00;
					//data[1] = 0xE1; //0xE100 -31dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MAX, 0x0100):
				//if (cn < 2)
				{
					data[0] = 0x00;
					data[1] = 0x08;
					//data[0] = 0x00;
					//data[1] = 0x18; //0x1800 +24dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_RES, 0x0100):
				//if (cn < 2)
				{
					data[0] = 0x88;
					data[1] = 0x00;
					//data[0] = 0x00;
					//data[1] = 0x01; //0x0100 1.0 dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_BASS_BOOST_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0100): //??? SOCOM II when in stereo, but there is no bass control defined in descriptor...
																					 //if (cn < 2) { //asks with cn == 2, meaning both channels? -1 is 'master'
				data[0] = 0;                                                         //bool
				ret = 1;
				//}
				break;
		}

		return ret;
	}

	static int usb_audio_set_control(HeadsetState* s, uint8_t attrib,
									 uint16_t cscn, uint16_t idif,
									 int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		const uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		uint16_t vol;
		int ret = USB_RET_STALL;
		bool set_vol = false;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0600):
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0200):
				s->f.in.mute = data[0] & 1;
				set_vol = true;
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0600):
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0200):
				vol = data[0] + (data[1] << 8);
				//qemu usb audiocard formula
				vol -= 0x8000;
				vol = (vol * 255 + 0x4400) / 0x8800;
				if (vol > 255)
				{
					vol = 255;
				}

				if (s->f.in.vol != vol)
				{
					s->f.in.vol = (uint8_t)vol;
					set_vol = true;
				}
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0100):
				s->f.out.mute = data[0] & 1;
				set_vol = true;
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0100):
				vol = data[0] + (data[1] << 8);
				if (cn < 2)
				{

					//qemu usb audiocard formula
					vol -= 0x8000;
					vol = (vol * 255 + 0x4400) / 0x8800;
					if (vol > 255)
					{
						vol = 255;
					}

					if (s->f.out.vol[cn] != vol)
					{
						s->f.out.vol[cn] = (uint8_t)vol;
						set_vol = true;
					}
					ret = 0;
				}
				break;
		}

		if (set_vol)
		{
			//if (s->f.debug) {
			//}
		}

		return ret;
	}

	static int usb_audio_ep_control(HeadsetState* s, uint8_t attrib,
									uint16_t cscn, uint16_t ep,
									int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		[[maybe_unused]] const uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, ep);
		int ret = USB_RET_STALL;

		//cs 1 cn 0xFF, ep 0x81 attrib 1
		/*for(int i=0; i<length; i++)
        Console.Warning("%02X ", data[i]);
    Console.Warning("\n");*/

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_SET_CUR, 0x84):
				s->f.in.srate = data[0] | (data[1] << 8) | (data[2] << 16);
				if (s->audsrc)
					s->audsrc->SetResampling(s->f.in.srate);
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_GET_CUR, 0x84):
				data[0] = s->f.in.srate & 0xFF;
				data[1] = (s->f.in.srate >> 8) & 0xFF;
				data[2] = (s->f.in.srate >> 16) & 0xFF;
				ret = 3;
				break;

			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_SET_CUR, 0x01):
				s->f.out.srate = data[0] | (data[1] << 8) | (data[2] << 16);
				if (s->audsink)
					s->audsink->SetResampling(s->f.out.srate);
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_GET_CUR, 0x01):
				data[0] = s->f.out.srate & 0xFF;
				data[1] = (s->f.out.srate >> 8) & 0xFF;
				data[2] = (s->f.out.srate >> 16) & 0xFF;
				ret = 3;
				break;
		}

		return ret;
	}

	static void headset_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
									   int index, int length, uint8_t* data)
	{
		HeadsetState* s = (HeadsetState*)dev;
		int ret = 0;


		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			/*
    * Audio device specific request
    */
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_CUR:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_MIN:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_MAX:
			case ClassInterfaceRequest | AUDIO_REQUEST_GET_RES:
				ret = usb_audio_get_control(s, request & 0xff, value, index,
											length, data);
				if (ret < 0)
				{
					//if (s->debug) {
					Console.Warning("headset: fail: get control\n");
					//}
					goto fail;
				}
				p->actual_length = ret;
				break;

			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_CUR:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MIN:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MAX:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_RES:
				ret = usb_audio_set_control(s, request & 0xff, value, index,
											length, data);
				if (ret < 0)
				{
					//if (s->debug) {
					Console.Warning("headset: fail: set control\n data:");
					//}
					goto fail;
				}
				break;

			case ClassEndpointRequest | AUDIO_REQUEST_GET_CUR:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_MIN:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_MAX:
			case ClassEndpointRequest | AUDIO_REQUEST_GET_RES:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_CUR:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_MIN:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_MAX:
			case ClassEndpointOutRequest | AUDIO_REQUEST_SET_RES:
				ret = usb_audio_ep_control(s, request & 0xff, value, index,
										   length, data);
				if (ret < 0)
					goto fail;
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	//naive, needs interpolation and stuff
	inline static int16_t SetVolume(int16_t sample, int vol)
	{
		//return (int16_t)(((uint16_t)(0x7FFF + sample) * vol / 0xFF) - 0x7FFF );
		return (int16_t)((int32_t)sample * vol / 0xFF);
	}

	static void headset_handle_data(USBDevice* dev, USBPacket* p)
	{
		HeadsetState* s = (HeadsetState*)dev;
		int ret = USB_RET_STALL;
		uint8_t devep = p->ep->nr;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				//Console.Warning("token in ep: %d len: %zd\n", devep, p->iov.size);
				if (devep == 4 && s->dev.altsetting[2] && s->audsrc)
				{

					uint32_t outChns = 1; //s->dev.altsetting[2] == 1 ? 2 : 1;
					uint32_t inChns = s->audsrc->GetChannels();
					int16_t* dst = nullptr;
					std::vector<int16_t> dst_alloc(0); //TODO
					size_t len = p->iov.size;
					//Divide 'len' bytes between n channels of 16 bits
					uint32_t maxFrames = len / (outChns * sizeof(int16_t)), frames = 0;

					if (p->iov.niov == 1)
						dst = (int16_t*)p->iov.iov[0].iov_base;
					else
					{
						dst_alloc.resize(len / sizeof(int16_t));
						dst = dst_alloc.data();
					}

					if (s->audsrc->GetFrames(&frames))
					{
						frames = MIN(frames, maxFrames);
						s->in_buffer.resize(frames * inChns);
						frames = s->audsrc->GetBuffer(s->in_buffer.data(), frames);
					}

					uint32_t i = 0;
					for (; i < frames; i++)
					{
						dst[i * outChns] = SetVolume(s->in_buffer[i * inChns], s->f.in.vol);
						//if (outChns > 1 && inChns > 1)
						//    dst[i * outChns + 1] = SetVolume(s->in_buffer[i * inChns + 1], s->f.in.vol);
						//else if (outChns > 1)
						//    dst[i * outChns + 1] = 0;
					}

					ret = i;

#if 0 //defined(_DEBUG) && _MSC_VER > 1800
            if (!file)
            {
                char name[1024] = { 0 };
                snprintf(name, sizeof(name), "headset_s16le_%dch_%dHz.raw", outChns, s->f.in.srate);
                file = fopen(name, "wb");
            }

            if (file)
                fwrite(data, sizeof(short), ret * outChns, file);
#endif
					ret = ret * outChns * sizeof(int16_t);
					if (p->iov.niov > 1)
					{
						usb_packet_copy(p, dst_alloc.data(), ret);
					}
					else
						p->actual_length = ret;
				}
				break;
			case USB_TOKEN_OUT:

				if (!s->audsink)
					return;

				if (devep == 1 && s->dev.altsetting[1])
				{
					uint32_t inChns = s->dev.altsetting[1] == 1 ? 2 : 1;
					uint32_t outChns = s->audsink->GetChannels();
					size_t len = p->iov.size;
					//Divide 'len' bytes between n channels of 16 bits
					uint32_t frames = len / (inChns * sizeof(int16_t));
					int16_t* src = nullptr;
					std::vector<int16_t> src_alloc(0);

					if (p->iov.niov == 1)
						src = (int16_t*)p->iov.iov[0].iov_base;
					else
					{
						//Copy iov data into continuous space
						src_alloc.resize(len / sizeof(int16_t));
						src = src_alloc.data();
						usb_packet_copy(p, src, len);
					}

					s->out_buffer.resize(frames * outChns); //TODO move to AudioDevice for less data copying

					uint32_t i = 0;
					for (; i < frames; i++)
					{
						if (inChns == outChns)
						{
							for (uint32_t cn = 0; cn < outChns; cn++)
								s->out_buffer[i * outChns + cn] = SetVolume(src[i * inChns + cn], s->f.out.vol[cn]);
						}
						else if (inChns < outChns)
						{
							for (uint32_t cn = 0; cn < outChns; cn++)
								s->out_buffer[i * outChns + cn] = SetVolume(src[i * inChns], s->f.out.vol[cn]);
						}
					}

#if 0
            if (!file)
            {
                char name[1024] = { 0 };
                snprintf(name, sizeof(name), "headset_s16le_%dch_%dHz.raw", inChns, s->f.out.srate);
                file = fopen(name, "wb");
            }

            if (file)
                fwrite(data, sizeof(short), frames * inChns, file);
#endif

					frames = s->audsink->SetBuffer(s->out_buffer.data(), frames);

					p->actual_length = frames * inChns * sizeof(int16_t);
				}
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}


	static void headset_handle_destroy(USBDevice* dev)
	{
		HeadsetState* s = (HeadsetState*)dev;
		if (file)
			fclose(file);
		file = NULL;

		if (!s)
			return;
		if (s->audsrc)
		{
			s->audsrc->Stop();
			delete s->audsrc;
			s->audsrc = NULL;
			s->in_buffer.clear();
		}

		if (s->audsink)
		{
			s->audsink->Stop();
			delete s->audsink;
			s->audsink = NULL;
			s->out_buffer.clear();
		}

		s->audsrcproxy->AudioDeinit();
		delete s;
	}

	static int headset_handle_open(USBDevice* dev)
	{
		HeadsetState* s = (HeadsetState*)dev;
		if (s)
		{
			if (s->audsrc)
				s->audsrc->Start();

			if (s->audsink)
				s->audsink->Start();
		}
		return 0;
	}

	static void headset_handle_close(USBDevice* dev)
	{
		HeadsetState* s = (HeadsetState*)dev;
		if (s)
		{
			if (s->audsrc)
				s->audsrc->Stop();

			if (s->audsink)
				s->audsink->Stop();
		}
	}

	USBDevice* HeadsetDevice::CreateDevice(int port)
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
		return HeadsetDevice::CreateDevice(port, api);
	}

	USBDevice* HeadsetDevice::CreateDevice(int port, const std::string& api)
	{
		HeadsetState* s;
		AudioDeviceInfo info;

		s = new HeadsetState();

		s->audsrcproxy = RegisterAudioDevice::instance().Proxy(api);
		if (!s->audsrcproxy)
		{
			Console.WriteLn("headset: Invalid audio API: '%s'\n", api.c_str());
			delete s;
			return NULL;
		}

		s->audsrcproxy->AudioInit();

		s->audsrc = s->audsrcproxy->CreateObject(port, TypeName(), 0, AUDIODIR_SOURCE);
		s->audsink = s->audsrcproxy->CreateObject(port, TypeName(), 0, AUDIODIR_SINK);
		s->f.mode = MIC_MODE_SINGLE;

		if (!s->audsrc || !s->audsink)
			goto fail;

		s->in_buffer.reserve(BUFFER_FRAMES * s->audsrc->GetChannels());
		s->out_buffer.reserve(BUFFER_FRAMES * s->audsink->GetChannels());

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;
		if (usb_desc_parse_dev(headset_dev_descriptor, sizeof(headset_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(headset_config_descriptor, sizeof(headset_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = headset_handle_reset;
		s->dev.klass.handle_control = headset_handle_control;
		s->dev.klass.handle_data = headset_handle_data;
		s->dev.klass.unrealize = headset_handle_destroy;
		s->dev.klass.open = headset_handle_open;
		s->dev.klass.close = headset_handle_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = desc_strings[2];

		// set defaults
		s->f.out.vol[0] = 240; /* 0 dB */
		s->f.out.vol[1] = 240; /* 0 dB */
		s->f.in.vol = 240;     /* 0 dB */
		s->f.out.srate = 48000;
		s->f.in.srate = 48000;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		headset_handle_reset((USBDevice*)s);

		return (USBDevice*)s;

	fail:
		headset_handle_destroy((USBDevice*)s);
		return NULL;
	}

	int HeadsetDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterAudioDevice::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int HeadsetDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		HeadsetState* s = (HeadsetState*)dev;
		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				s->f = *(HeadsetState::freeze*)data;
				if (s->audsrc)
					s->audsrc->SetResampling(s->f.in.srate);
				if (s->audsink)
					s->audsink->SetResampling(s->f.out.srate);
				return sizeof(HeadsetState::freeze);
			case FreezeAction::Save:
				*(HeadsetState::freeze*)data = s->f;
				return sizeof(HeadsetState::freeze);
			case FreezeAction::Size:
				return sizeof(HeadsetState::freeze);
			default:
				break;
		}
		return 0;
	}

} // namespace usb_mic
