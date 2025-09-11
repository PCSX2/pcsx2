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

#include "IconsPromptFont.h"
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/desc.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/usb-mic/usb-mic.h"
#include "USB/usb-mic/audiodev.h"
#include "USB/usb-mic/audiodev-noop.h"
#include "USB/usb-mic/audiodev-cubeb.h"
#include "USB/usb-mic/audio.h"
#include "USB/USB.h"
#include "Host.h"
#include "StateWrapper.h"

#include "common/Console.h"

#include "fmt/format.h"

static FILE* file = NULL;

#define BUFFER_FRAMES 200

/*
 * A Basic Audio Device uses these specific values
 */
#define USBAUDIO_PACKET_SIZE 200 //192
#define USBAUDIO_SAMPLE_RATE 48000
#define USBAUDIO_PACKET_INTERVAL 1

namespace usb_mic
{

	/*
	 * A USB audio device supports an arbitrary number of alternate
	 * interface settings for each interface.  Each corresponds to a block
	 * diagram of parameterized blocks.  This can thus refer to things like
	 * number of channels, data rates, or in fact completely different
	 * block diagrams.  Alternative setting 0 is always the null block diagram,
	 * which is used by a disabled device.
	 */
	enum usb_audio_altset : int8_t
	{
		ALTSET_OFF = 0x00, /* No endpoint */
		ALTSET_ON = 0x01, /* Single endpoint */
	};

	struct SINGSTARMICState
	{
		USBDevice dev;

		USBDesc desc;
		USBDescDevice desc_dev;

		std::unique_ptr<AudioDevice> audsrc[2];

		struct freeze
		{
			int intf;
			MicMode mode;

			enum usb_audio_altset altset;
			bool mute;
			uint8_t vol[2];
			uint32_t srate[2]; //TODO can it have different rates?
		} f; //freezable

		/* properties */
		std::vector<int16_t> buffer[2];
		//uint8_t  fifo[2][200]; //on-chip 400byte fifo
	};

	static const USBDescStrings singstar_desc_strings = {
		"",
		"Nam Tai E&E Products Ltd.",
		"USBMIC",
		"310420811",
	};

	static const USBDescStrings logitech_desc_strings = {
		"",
		"Logitech",
		"USBMIC",
	};

	static const USBDescStrings ak5370_desc_strings = {
		"",
		"AKM",
		"AK5370"
	};

	/* descriptor dumped from a real singstar MIC adapter */
	static const uint8_t singstar_dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0110), //(272)
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x08, //(8)
		/* idVendor            */ WBVAL(0x1415), //(5141)
		/* idProduct           */ WBVAL(0x0000), //(0)
		/* bcdDevice           */ WBVAL(0x0001), //(1)
		/* iManufacturer       */ 0x01, //(1)
		/* iProduct            */ 0x02, //(2)
		/* iSerialNumber       */ 0x00, //(0) unused
		/* bNumConfigurations  */ 0x01, //(1)
	};

	static const uint8_t singstar_config_descriptor[] = {
		/* Configuration 1 */
		0x09, /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(0x00b1), /* wTotalLength */
		0x02, /* bNumInterfaces */
		0x01, /* bConfigurationValue */
		0x00, /* iConfiguration */
		USB_CONFIG_BUS_POWERED, /* bmAttributes */
		USB_CONFIG_POWER_MA(90), /* bMaxPower */

		/* Interface 0, Alternate Setting 0, Audio Control */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x00, /* bInterfaceNumber */
		0x00, /* bAlternateSetting */
		0x00, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOCONTROL, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Control Interface */
		AUDIO_CONTROL_INTERFACE_DESC_SZ(1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_HEADER, /* bDescriptorSubtype */
		WBVAL(0x0100), /* 1.00 */ /* bcdADC */
		WBVAL(0x0028), /* wTotalLength */
		0x01, /* bInCollection */
		0x01, /* baInterfaceNr */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL, /* bDescriptorSubtype */
		0x01, /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE), /* wTerminalType */
		0x02, /* bAssocTerminal */
		0x02, /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L | AUDIO_CHANNEL_R), /* wChannelConfig */
		0x00, /* iChannelNames */
		0x00, /* iTerminal */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL, /* bDescriptorSubtype */
		0x02, /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING), /* wTerminalType */
		0x01, /* bAssocTerminal */
		0x03, /* bSourceID */
		0x00, /* iTerminal */

		/* Audio Feature Unit */
		AUDIO_FEATURE_UNIT_DESC_SZ(2, 1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT, /* bDescriptorSubtype */
		0x03, /* bUnitID */
		0x01, /* bSourceID */
		0x01, /* bControlSize */
		0x01, /* bmaControls(0) */
		0x02, /* bmaControls(1) */
		0x02, /* bmaControls(2) */
		0x00, /* iTerminal */

		/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x00, /* bAlternateSetting */
		0x00, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Interface 1, Alternate Setting 1, Audio Streaming - 1 channel */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x01, /* bAlternateSetting */
		0x01, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_GENERAL, /* bDescriptorSubtype */
		0x02, /* bTerminalLink */
		0x01, /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM), /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE, /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I, /* bFormatType */
		0x01, /* bNrChannels */
		0x02, /* bSubFrameSize */
		0x10, /* bBitResolution */
		0x05, /* bSamFreqType */
		B3VAL(8000), /* tSamFreq 1 */
		B3VAL(11025), /* tSamFreq 2 */
		B3VAL(22050), /* tSamFreq 3 */
		B3VAL(44100), /* tSamFreq 4 */
		B3VAL(48000), /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE, /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		USB_ENDPOINT_IN(1), /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x0064), /* wMaxPacketSize */
		0x01, /* bInterval */
		0x00, /* bRefresh */
		0x00, /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL, /* bDescriptor */
		0x01, /* bmAttributes */
		0x00, /* bLockDelayUnits */
		WBVAL(0x0000), /* wLockDelay */

		/* Interface 1, Alternate Setting 2, Audio Streaming - 2 channels */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x02, /* bAlternateSetting */
		0x01, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_GENERAL, /* bDescriptorSubtype */
		0x02, /* bTerminalLink */
		0x01, /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM), /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE, /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I, /* bFormatType */
		0x02, /* bNrChannels */
		0x02, /* bSubFrameSize */
		0x10, /* bBitResolution */
		0x05, /* bSamFreqType */
		B3VAL(8000), /* tSamFreq 1 */
		B3VAL(11025), /* tSamFreq 2 */
		B3VAL(22050), /* tSamFreq 3 */
		B3VAL(44100), /* tSamFreq 4 */
		B3VAL(48000), /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE, /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		USB_ENDPOINT_IN(1), /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x00c8), /* wMaxPacketSize */
		0x01, /* bInterval */
		0x00, /* bRefresh */
		0x00, /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL, /* bDescriptor */
		0x01, /* bmAttributes */
		0x00, /* bLockDelayUnits */
		WBVAL(0x0000), /* wLockDelay */

		/* Terminator */
		0 /* bLength */
	};

	static const uint8_t logitech_dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0110), //(272)
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x08, //(8)
		/* idVendor            */ WBVAL(0x046D),
		/* idProduct           */ WBVAL(0x0000), //(0)
		/* bcdDevice           */ WBVAL(0x0001), //(1)
		/* iManufacturer       */ 0x01, //(1)
		/* iProduct            */ 0x02, //(2)
		/* iSerialNumber       */ 0x00, //(0) unused
		/* bNumConfigurations  */ 0x01, //(1)
	};

	static const uint8_t logitech_config_descriptor[] = {
		/* Configuration 1 */
		0x09, /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(0x00b1), /* wTotalLength */
		0x02, /* bNumInterfaces */
		0x01, /* bConfigurationValue */
		0x00, /* iConfiguration */
		USB_CONFIG_BUS_POWERED, /* bmAttributes */
		USB_CONFIG_POWER_MA(90), /* bMaxPower */

		/* Interface 0, Alternate Setting 0, Audio Control */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x00, /* bInterfaceNumber */
		0x00, /* bAlternateSetting */
		0x00, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOCONTROL, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Control Interface */
		AUDIO_CONTROL_INTERFACE_DESC_SZ(1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_HEADER, /* bDescriptorSubtype */
		WBVAL(0x0100), /* 1.00 */ /* bcdADC */
		WBVAL(0x0028), /* wTotalLength */
		0x01, /* bInCollection */
		0x01, /* baInterfaceNr */

		/* Audio Input Terminal */
		AUDIO_INPUT_TERMINAL_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_INPUT_TERMINAL, /* bDescriptorSubtype */
		0x01, /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_MICROPHONE), /* wTerminalType */
		0x02, /* bAssocTerminal */
		0x02, /* bNrChannels */
		WBVAL(AUDIO_CHANNEL_L | AUDIO_CHANNEL_R), /* wChannelConfig */
		0x00, /* iChannelNames */
		0x00, /* iTerminal */

		/* Audio Output Terminal */
		AUDIO_OUTPUT_TERMINAL_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_OUTPUT_TERMINAL, /* bDescriptorSubtype */
		0x02, /* bTerminalID */
		WBVAL(AUDIO_TERMINAL_USB_STREAMING), /* wTerminalType */
		0x01, /* bAssocTerminal */
		0x03, /* bSourceID */
		0x00, /* iTerminal */

		/* Audio Feature Unit */
		AUDIO_FEATURE_UNIT_DESC_SZ(2, 1), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_CONTROL_FEATURE_UNIT, /* bDescriptorSubtype */
		0x03, /* bUnitID */
		0x01, /* bSourceID */
		0x01, /* bControlSize */
		0x01, /* bmaControls(0) */
		0x02, /* bmaControls(1) */
		0x02, /* bmaControls(2) */
		0x00, /* iTerminal */

		/* Interface 1, Alternate Setting 0, Audio Streaming - Zero Bandwith */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x00, /* bAlternateSetting */
		0x00, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Interface 1, Alternate Setting 1, Audio Streaming - 1 channel */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x01, /* bAlternateSetting */
		0x01, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_GENERAL, /* bDescriptorSubtype */
		0x02, /* bTerminalLink */
		0x01, /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM), /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE, /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I, /* bFormatType */
		0x01, /* bNrChannels */
		0x02, /* bSubFrameSize */
		0x10, /* bBitResolution */
		0x05, /* bSamFreqType */
		B3VAL(8000), /* tSamFreq 1 */
		B3VAL(11025), /* tSamFreq 2 */
		B3VAL(22050), /* tSamFreq 3 */
		B3VAL(44100), /* tSamFreq 4 */
		B3VAL(48000), /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE, /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		USB_ENDPOINT_IN(1), /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x0064), /* wMaxPacketSize */
		0x01, /* bInterval */
		0x00, /* bRefresh */
		0x00, /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL, /* bDescriptor */
		0x01, /* bmAttributes */
		0x00, /* bLockDelayUnits */
		WBVAL(0x0000), /* wLockDelay */

		/* Interface 1, Alternate Setting 2, Audio Streaming - 2 channels */
		USB_INTERFACE_DESC_SIZE, /* bLength */
		USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		0x01, /* bInterfaceNumber */
		0x02, /* bAlternateSetting */
		0x01, /* bNumEndpoints */
		USB_CLASS_AUDIO, /* bInterfaceClass */
		AUDIO_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
		AUDIO_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
		0x00, /* iInterface */

		/* Audio Streaming Interface */
		AUDIO_STREAMING_INTERFACE_DESC_SIZE, /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_GENERAL, /* bDescriptorSubtype */
		0x02, /* bTerminalLink */
		0x01, /* bDelay */
		WBVAL(AUDIO_FORMAT_PCM), /* wFormatTag */

		/* Audio Type I Format */
		AUDIO_FORMAT_TYPE_I_DESC_SZ(5), /* bLength */
		AUDIO_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_STREAMING_FORMAT_TYPE, /* bDescriptorSubtype */
		AUDIO_FORMAT_TYPE_I, /* bFormatType */
		0x02, /* bNrChannels */
		0x02, /* bSubFrameSize */
		0x10, /* bBitResolution */
		0x05, /* bSamFreqType */
		B3VAL(8000), /* tSamFreq 1 */
		B3VAL(11025), /* tSamFreq 2 */
		B3VAL(22050), /* tSamFreq 3 */
		B3VAL(44100), /* tSamFreq 4 */
		B3VAL(48000), /* tSamFreq 5 */

		/* Endpoint - Standard Descriptor */
		AUDIO_STANDARD_ENDPOINT_DESC_SIZE, /* bLength */
		USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		USB_ENDPOINT_IN(1), /* bEndpointAddress */
		USB_ENDPOINT_TYPE_ISOCHRONOUS | USB_ENDPOINT_SYNC_ASYNCHRONOUS, /* bmAttributes */
		WBVAL(0x00c8), /* wMaxPacketSize */
		0x01, /* bInterval */
		0x00, /* bRefresh */
		0x00, /* bSynchAddress */

		/* Endpoint - Audio Streaming */
		AUDIO_STREAMING_ENDPOINT_DESC_SIZE, /* bLength */
		AUDIO_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType */
		AUDIO_ENDPOINT_GENERAL, /* bDescriptor */
		0x01, /* bmAttributes */
		0x00, /* bLockDelayUnits */
		WBVAL(0x0000), /* wLockDelay */

		/* Terminator */
		0 /* bLength */
	};

	static const uint8_t ak5370_dev_descriptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x10, 0x01,  // bcdUSB 1.10
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass
		0x00,        // bDeviceProtocol
		0x08,        // bMaxPacketSize0 8
		0x56, 0x05,  // idVendor 0x0556
		0x01, 0x00,  // idProduct 0x0001
		0x01, 0x00,  // bcdDevice 0.01
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x00,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};

	static const uint8_t ak5370_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x76, 0x00,  // wTotalLength 118
		0x02,        // bNumInterfaces 2
		0x01,        // bConfigurationValue
		0x00,        // iConfiguration (String Index)
		0x80,        // bmAttributes
		0x2D,        // bMaxPower 90mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x00,        // bNumEndpoints 0
		0x01,        // bInterfaceClass (Audio)
		0x01,        // bInterfaceSubClass (Audio Control)
		0x00,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x09,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x01,        // bDescriptorSubtype (CS_INTERFACE -> HEADER)
		0x00, 0x01,  // bcdADC 1.00
		0x26, 0x00,  // wTotalLength 38
		0x01,        // binCollection 0x01
		0x01,        // baInterfaceNr 1

		0x0C,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x02,        // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
		0x01,        // bTerminalID
		0x01, 0x02,  // wTerminalType (Microphone)
		0x02,        // bAssocTerminal
		0x01,        // bNrChannels 1
		0x00, 0x00,  // wChannelConfig
		0x00,        // iChannelNames
		0x00,        // iTerminal

		0x09,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x03,        // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
		0x02,        // bTerminalID
		0x01, 0x01,  // wTerminalType (USB Streaming)
		0x01,        // bAssocTerminal
		0x03,        // bSourceID
		0x00,        // iTerminal

		0x08,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x06,        // bDescriptorSubtype (CS_INTERFACE -> FEATURE_UNIT)
		0x03,        // bUnitID
		0x01,        // bSourceID
		0x01,        // bControlSize 1
		0x43, 0x00,  // bmaControls[0] (Mute,Volume,Automatic)

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x01,        // bInterfaceNumber 1
		0x00,        // bAlternateSetting
		0x00,        // bNumEndpoints 0
		0x01,        // bInterfaceClass (Audio)
		0x02,        // bInterfaceSubClass (Audio Streaming)
		0x00,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x01,        // bInterfaceNumber 1
		0x01,        // bAlternateSetting
		0x01,        // bNumEndpoints 1
		0x01,        // bInterfaceClass (Audio)
		0x02,        // bInterfaceSubClass (Audio Streaming)
		0x00,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x07,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x01,        // bDescriptorSubtype (CS_INTERFACE -> AS_GENERAL)
		0x02,        // bTerminalLink
		0x01,        // bDelay 1
		0x01, 0x00,  // wFormatTag (PCM)

		0x17,        // bLength
		0x24,        // bDescriptorType (See Next Line)
		0x02,        // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
		0x01,        // bFormatType 1
		0x01,        // bNrChannels (Mono)
		0x02,        // bSubFrameSize 2
		0x10,        // bBitResolution 16
		0x05,        // bSamFreqType 5
		0x40, 0x1F, 0x00,  // tSamFreq[1] 8000 Hz
		0x11, 0x2B, 0x00,  // tSamFreq[2] 11025 Hz
		0x22, 0x56, 0x00,  // tSamFreq[3] 22050 Hz
		0x44, 0xAC, 0x00,  // tSamFreq[4] 44100 Hz
		0x80, 0xBB, 0x00,  // tSamFreq[5] 48000 Hz

		0x07,        // bLength
		0x05,        // bDescriptorType (See Next Line)
		0x81,        // bEndpointAddress (IN/D2H)
		0x01,        // bmAttributes (Isochronous, No Sync, Data EP)
		0x64, 0x00,  // wMaxPacketSize 100
		0x01,        // bInterval 1 (unit depends on device speed)

		0x07,        // bLength
		0x25,        // bDescriptorType (See Next Line)
		0x01,        // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
		0x01,        // bmAttributes (Sampling Freq Control)
		0x00,        // bLockDelayUnits
		0x00, 0x00,  // wLockDelay 0
	};

	static void usb_mic_handle_reset(USBDevice* dev)
	{
		/* XXX: do it */
		return;
	}

/*
 * Note: we arbitrarily map the volume control range onto -inf..+8 dB
 */
#define ATTRIB_ID(cs, attrib, idif) (((cs) << 24) | ((attrib) << 16) | (idif))


	//0x0300 - feature bUnitID 0x03
	static int usb_audio_get_control(SINGSTARMICState* s, uint8_t attrib, uint16_t cscn, uint16_t idif, int length, uint8_t* data)
	{
		const uint8_t cs = cscn >> 8;
		const uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		const uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		int ret = USB_RET_STALL;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0300):
				data[0] = s->f.mute;
				ret = 1;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_CUR, 0x0300):
				if (cn < 2 || cn == 0xff)
				{
					const uint16_t vol = (s->f.vol[cn == 1 ? 1 : 0] * 0x8800 + 127) / 255 + 0x8000;
					data[0] = (uint8_t)(vol & 0xFF);
					data[1] = vol >> 8;
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MIN, 0x0300):
				if (cn < 2 || cn == 0xff)
				{
					data[0] = 0x01;
					data[1] = 0x80;
					//data[0] = 0x00;
					//data[1] = 0xE1; //0xE100 -31dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_MAX, 0x0300):
				if (cn < 2 || cn == 0xff)
				{
					data[0] = 0x00;
					data[1] = 0x08;
					//data[0] = 0x00;
					//data[1] = 0x18; //0x1800 +24dB
					ret = 2;
				}
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_GET_RES, 0x0300):
				if (cn < 2 || cn == 0xff)
				{
					data[0] = 0x88;
					data[1] = 0x00;
					//data[0] = 0x00;
					//data[1] = 0x01; //0x0100 1.0 dB
					ret = 2;
				}
				break;
		}

		return ret;
	}

	static int usb_audio_set_control(SINGSTARMICState* s, uint8_t attrib, uint16_t cscn, uint16_t idif, int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, idif);
		int ret = USB_RET_STALL;

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_MUTE_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0300):
				s->f.mute = data[0] & 1;
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_VOLUME_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0300):
				if (cn < 2 || cn == 0xff)
				{
					uint16_t vol = data[0] + (data[1] << 8);

					//qemu usb audiocard formula, singstar has a bit different range
					vol -= 0x8000;
					vol = (vol * 255 + 0x4400) / 0x8800;
					if (vol > 255)
						vol = 255;

					if (cn == 0xff)
					{
						if (s->f.vol[0] != vol)
							s->f.vol[0] = (uint8_t)vol;
						if (s->f.vol[1] != vol)
							s->f.vol[1] = (uint8_t)vol;
					}
					else
					{
						if (s->f.vol[cn] != vol)
							s->f.vol[cn] = (uint8_t)vol;
					}

					ret = 0;
				}
				break;
			case ATTRIB_ID(AUDIO_AUTOMATIC_GAIN_CONTROL, AUDIO_REQUEST_SET_CUR, 0x0300):
				ret = 0;
				break;
		}

		return ret;
	}

	static int usb_audio_ep_control(SINGSTARMICState* s, uint8_t attrib, uint16_t cscn, uint16_t ep, int length, uint8_t* data)
	{
		uint8_t cs = cscn >> 8;
		uint8_t cn = cscn - 1; /* -1 for the non-present master control */
		uint32_t aid = ATTRIB_ID(cs, attrib, ep);
		int ret = USB_RET_STALL;

		Console.Warning("usb_mic: ep control: cs=0x%x, cn=0x%X, attrib=0x%X, ep=0x%X", cs, cn, attrib, ep);
		/*for(int i=0; i<length; i++)
		Console.Warning("%02X ", data[i]);
		Console.Warning("\n");*/

		switch (aid)
		{
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_SET_CUR, 0x81):
				if (cn == 0xFF)
				{
					const uint32_t sr = data[0] | (data[1] << 8) | (data[2] << 16);
					if (s->f.srate[0] != sr)
					{
						s->f.srate[0] = sr;
						if (s->audsrc[0])
							s->audsrc[0]->SetResampling(s->f.srate[0]);

					}
					if (s->f.srate[1] != sr)
					{
						s->f.srate[1] = sr;
						if (s->audsrc[1])
							s->audsrc[1]->SetResampling(s->f.srate[1]);
						
					}
				}
				else if (cn < 2)
				{
					const uint32_t sr = data[0] | (data[1] << 8) | (data[2] << 16);
					if (s->f.srate[cn] != sr)
					{
						s->f.srate[cn] = sr;
						if (s->audsrc[cn])
							s->audsrc[cn]->SetResampling(s->f.srate[cn]);
					}
				}
				ret = 0;
				break;
			case ATTRIB_ID(AUDIO_SAMPLING_FREQ_CONTROL, AUDIO_REQUEST_GET_CUR, 0x81):
				data[0] = s->f.srate[0] & 0xFF;
				data[1] = (s->f.srate[0] >> 8) & 0xFF;
				data[2] = (s->f.srate[0] >> 16) & 0xFF;
				ret = 3;
				break;
		}

		return ret;
	}

	static void usb_mic_set_interface(USBDevice* dev, int intf, int alt_old, int alt_new)
	{
		SINGSTARMICState* s = USB_CONTAINER_OF(dev, SINGSTARMICState, dev);
		s->f.intf = alt_new;
#if defined(_DEBUG)
		/* close previous debug audio output file */
		if (file && intf > 0 && alt_old != alt_new)
		{
			fclose(file);
			file = nullptr;
		}
#endif
	}

	static void usb_mic_handle_control(USBDevice* dev, USBPacket* p, int request, int value, int index, int length, uint8_t* data)
	{
		SINGSTARMICState* s = USB_CONTAINER_OF(dev, SINGSTARMICState, dev);
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
				ret = usb_audio_get_control(s, request & 0xff, value, index, length, data);
				if (ret < 0)
				{
					Console.Warning("usb_mic: fail: get control, req=%02x, val=%02x, idx=%02x, ret=%d", request, value, index, ret);
					goto fail;
				}
				p->actual_length = ret;
				break;

			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_CUR:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MIN:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_MAX:
			case ClassInterfaceOutRequest | AUDIO_REQUEST_SET_RES:
				ret = usb_audio_set_control(s, request & 0xff, value, index, length, data);
				if (ret < 0)
				{
					Console.Warning("usb_mic: fail: set control, req=%02x, val=%02x, idx=%02x", request, value, index);
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
				ret = usb_audio_ep_control(s, request & 0xff, value, index, length, data);
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

	static void usb_mic_handle_data(USBDevice* dev, USBPacket* p)
	{
		SINGSTARMICState* s = USB_CONTAINER_OF(dev, SINGSTARMICState, dev);
		int ret = 0;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				//Console.Warning("token in ep: %d len: %zd\n", devep, p->iov.size);
				{
					//TODO
					int outChns = s->f.intf == 2 ? 2 : 1;
					uint32_t frames, out_frames[2] = {0}, chn;
					int16_t *src1, *src2;
					int16_t* dst = (int16_t*)p->buffer_ptr;
					size_t len = p->buffer_size;

					// send only 1ms (bInterval) of samples
					if (s->f.srate[0] == 48000 || s->f.srate[0] == 8000 || s->f.srate[0] == 16000)
						len = std::min<u32>(p->buffer_size, outChns * sizeof(int16_t) * s->f.srate[0] / 1000);

					//Divide 'len' bytes between 2 channels of 16 bits
					uint32_t max_frames = len / (outChns * sizeof(uint16_t));

					memset(dst, 0, len);

					for (int i = 0; i < 2; i++)
					{
						frames = max_frames;
						if (s->audsrc[i] && s->audsrc[i]->GetFrames(&frames))
						{
							frames = std::min(max_frames, frames); //max 50 frames usually
							out_frames[i] = s->audsrc[i]->GetBuffer(s->buffer[i].data(), frames);
						}
					}

					//if (frames < max_frames)
					if (!frames)
					{
						p->status = USB_RET_NAK;
						return;
					}


					//TODO well, it is 16bit interleaved, right?
					//Merge with MIC_MODE_SHARED case?
					switch (s->f.mode)
					{
						case MIC_MODE_SINGLE:
						{
							int k = s->audsrc[0] ? 0 : 1;
							int off = s->f.intf == 1 ? 0 : k;
							chn = s->audsrc[k]->GetChannels();
							frames = out_frames[k];

							uint32_t i = 0;
							for (; i < frames && i < max_frames; i++)
							{
								dst[i * outChns + off] = SetVolume(s->buffer[k][i * chn], s->f.vol[0]);
								//dst[i * 2 + 1] = 0;
							}

							ret = i;
						}
						break;
						case MIC_MODE_SHARED:
						{
							chn = s->audsrc[0]->GetChannels();
							frames = out_frames[0];
							src1 = s->buffer[0].data();

							uint32_t i = 0;
							for (; i < frames && i < max_frames; i++)
							{
								dst[i * outChns] = SetVolume(src1[i * chn], s->f.vol[0]);
								if (outChns > 1)
								{
									if (chn == 1)
										dst[i * 2 + 1] = dst[i * 2];
									else
										dst[i * 2 + 1] = SetVolume(src1[i * chn + 1], s->f.vol[0]);
								}
							}

							ret = i;
						}
						break;
						case MIC_MODE_SEPARATE:
						{
							uint32_t cn1 = s->audsrc[0]->GetChannels();
							uint32_t cn2 = s->audsrc[1]->GetChannels();
							uint32_t minLen = std::min(out_frames[0], out_frames[1]);

							src1 = s->buffer[0].data();
							src2 = s->buffer[1].data();

							uint32_t i = 0;
							for (; i < minLen && i < max_frames; i++)
							{
								dst[i * outChns] = SetVolume(src1[i * cn1], s->f.vol[0]);
								if (outChns > 1)
									dst[i * 2 + 1] = SetVolume(src2[i * cn2], s->f.vol[1]);
							}

							ret = i;
						}
						break;
						default:
							break;
					}

					ret = ret * outChns * sizeof(int16_t);
					p->actual_length = ret;

#if 0 //defined(_DEBUG) && defined(_MSC_VER)
					if (!file)
					{
						char name[1024] = {0};
						snprintf(name, sizeof(name), "usb_mic_%dch_%uHz.raw", outChns, s->f.srate[0]);
						file = fopen(name, "wb");
					}

					if (file)
						fwrite(dst, 1, ret, file);
#endif
				}
				break;
			case USB_TOKEN_OUT:
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}


	static void usb_mic_handle_destroy(USBDevice* dev)
	{
		SINGSTARMICState* s = USB_CONTAINER_OF(dev, SINGSTARMICState, dev);
		if (file)
			fclose(file);
		file = NULL;

		if (!s)
			return;
		for (int i = 0; i < 2; i++)
		{
			if (s->audsrc[i])
			{
				s->audsrc[i]->Stop();
				s->audsrc[i].reset();
				s->buffer[i].clear();
			}
		}

		delete s;
	}

	USBDevice* MicrophoneDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		if (subtype >= MIC_COUNT)
			return nullptr;

		static const bool dual_mic = subtype == MIC_SINGSTAR;
		return CreateDevice(si, port, subtype, dual_mic, 48000, MicrophoneDevice::TypeName());
	}

	USBDevice* MicrophoneDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype, bool dual_mic, const int samplerate, const char* devtype) const
	{
		if (subtype >= MIC_COUNT)
			return nullptr;

		SINGSTARMICState* s = new SINGSTARMICState();

		if (dual_mic)
		{
			std::string dev0(USB::GetConfigString(si, port, devtype, "player1_device_name"));
			std::string dev1(USB::GetConfigString(si, port, devtype, "player2_device_name"));
			const s32 latency = USB::GetConfigInt(si, port, devtype, "input_latency", AudioDevice::DEFAULT_LATENCY);

			if (!dev0.empty() && dev0 == dev1)
			{
				// Try to open a single device with two channels. This might not work if it's only a mono mic.
				Console.WriteLn("USB-Mic: Trying to open stereo single source dual mic: '%s'", dev0.c_str());
				s->audsrc[0] = AudioDevice::CreateDevice(AUDIODIR_SOURCE, 2, dev0, latency);
				if (!s->audsrc[0])
				{
					Console.Error("USB-Mic: Failed to get stereo source, mic '%s' might only be mono", dev0.c_str());
					s->audsrc[0] = AudioDevice::CreateDevice(AUDIODIR_SOURCE, 1, std::move(dev0), latency);
				}

				s->f.mode = MIC_MODE_SHARED;
			}
			else
			{
				if (!dev0.empty())
					s->audsrc[0] = AudioDevice::CreateDevice(AUDIODIR_SOURCE, 1, std::move(dev0), latency);
				if (!dev1.empty())
					s->audsrc[1] = AudioDevice::CreateDevice(AUDIODIR_SOURCE, 1, std::move(dev1), latency);

				s->f.mode = (s->audsrc[0] && s->audsrc[1]) ? MIC_MODE_SEPARATE : MIC_MODE_SINGLE;
			}
		}
		else
		{
			std::string dev0(USB::GetConfigString(si, port, devtype, "input_device_name"));
			const s32 latency0 = USB::GetConfigInt(si, port, devtype, "input_latency", AudioDevice::DEFAULT_LATENCY);
			if (!dev0.empty())
				s->audsrc[0] = AudioDevice::CreateDevice(AUDIODIR_SOURCE, 1, std::move(dev0), latency0);

			s->f.mode = MIC_MODE_SINGLE;
		}

		if (!s->audsrc[0] && !s->audsrc[1])
		{
			Host::AddOSDMessage(
				TRANSLATE_STR("USB", "USB-Mic: Neither player 1 nor 2 is connected."), Host::OSD_ERROR_DURATION);
			goto fail;
		}

		Console.WriteLn("USB-Mic Mode: %s",
			(s->f.mode == MIC_MODE_SHARED ? "shared" : (s->f.mode == MIC_MODE_SEPARATE ? "separate" : "single")));
		Console.WriteLn("USB-Mic Source 0: %s", s->audsrc[0] ? "opened" : "not opened");
		Console.WriteLn("USB-Mic Source 1: %s", s->audsrc[1] ? "opened" : "not opened");

		for (int i = 0; i < 2; i++)
		{
			if (s->audsrc[i])
			{
				s->buffer[i].resize(BUFFER_FRAMES * s->audsrc[i]->GetChannels());
				if (!s->audsrc[i]->Start())
				{
					Host::AddOSDMessage(
						fmt::format(TRANSLATE_FS("USB", "USB-Mic: Failed to start player {} audio stream."), i + 1),
						Host::OSD_ERROR_DURATION);
					goto fail;
				}
				s->audsrc[i]->SetResampling(samplerate);
			}
		}

		s->desc.full = &s->desc_dev;
		switch (subtype)
		{
			case MIC_SINGSTAR:
				s->desc.str = singstar_desc_strings;
				if (usb_desc_parse_dev(singstar_dev_descriptor, sizeof(singstar_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(singstar_config_descriptor, sizeof(singstar_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			case MIC_LOGITECH:
				s->desc.str = logitech_desc_strings;
				if (usb_desc_parse_dev(logitech_dev_descriptor, sizeof(logitech_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(logitech_config_descriptor, sizeof(logitech_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			case MIC_KONAMI:
				s->desc.str = ak5370_desc_strings;
				if (usb_desc_parse_dev(ak5370_dev_descriptor, sizeof(ak5370_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(ak5370_config_descriptor, sizeof(ak5370_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
		}

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_mic_handle_reset;
		s->dev.klass.handle_control = usb_mic_handle_control;
		s->dev.klass.handle_data = usb_mic_handle_data;
		s->dev.klass.set_interface = usb_mic_set_interface;
		s->dev.klass.unrealize = usb_mic_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = singstar_desc_strings[2];

		// set defaults
		s->f.vol[0] = 240; /* 0 dB */
		s->f.vol[1] = 240; /* 0 dB */
		s->f.srate[0] = samplerate;
		s->f.srate[1] = samplerate;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		usb_mic_handle_reset(&s->dev);

		return &s->dev;

	fail:
		usb_mic_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* MicrophoneDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Microphone");
	}

	const char* MicrophoneDevice::TypeName() const
	{
		return "singstar";
	}

	const char* MicrophoneDevice::IconName() const
	{
		return ICON_PF_SINGSTAR_MIC;
	}

	bool MicrophoneDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		SINGSTARMICState* s = USB_CONTAINER_OF(dev, SINGSTARMICState, dev);
		if (!sw.DoMarker("SINGSTARMICState"))
			return false;

		sw.Do(&s->f.intf);
		sw.Do(&s->f.mode);
		sw.Do(&s->f.altset);
		sw.Do(&s->f.mute);
		sw.DoPODArray(&s->f.vol, std::size(s->f.vol));
		sw.DoPODArray(s->f.srate, std::size(s->f.srate));

		if (sw.IsReading() && !sw.HasError())
		{
			for (u32 i = 0; i < 2; i++)
			{
				if (s->audsrc[i])
					s->audsrc[i]->SetResampling(s->f.srate[i]);
			}
		}

		return !sw.HasError();
	}

	void MicrophoneDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		// TODO: Reload devices.
	}

	std::span<const char*> MicrophoneDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Singstar"),
			TRANSLATE_NOOP("USB", "Logitech"),
			TRANSLATE_NOOP("USB", "Konami"),
		};
		return subtypes;
	}

	std::span<const SettingInfo> MicrophoneDevice::Settings(u32 subtype) const
	{
		switch (subtype)
		{
			case MIC_SINGSTAR:
			{
				static constexpr const SettingInfo info[] = {
					{SettingInfo::Type::StringList, "player1_device_name", TRANSLATE_NOOP("USB", "Player 1 Device"),
						TRANSLATE_NOOP("USB", "Selects the input for the first player."), "", nullptr, nullptr, nullptr,
						nullptr, nullptr, &AudioDevice::GetInputDeviceList},
					{SettingInfo::Type::StringList, "player2_device_name", TRANSLATE_NOOP("USB", "Player 2 Device"),
						TRANSLATE_NOOP("USB", "Selects the input for the second player."), "", nullptr, nullptr, nullptr,
						nullptr, nullptr, &AudioDevice::GetInputDeviceList},
					{SettingInfo::Type::Integer, "input_latency", TRANSLATE_NOOP("USB", "Input Latency"),
						TRANSLATE_NOOP("USB", "Specifies the latency to the host input device."),
						AudioDevice::DEFAULT_LATENCY_STR, "1", "1000", "1", TRANSLATE_NOOP("USB", "%dms"), nullptr, nullptr, 1.0f},
				};
				return info;
			}
			case MIC_LOGITECH:
			case MIC_KONAMI:
			default:
			{
				static constexpr const SettingInfo info[] = {
					{SettingInfo::Type::StringList, "input_device_name", TRANSLATE_NOOP("USB", "Input Device"),
						TRANSLATE_NOOP("USB", "Selects the device to read audio from."), "", nullptr, nullptr, nullptr, nullptr,
						nullptr, &AudioDevice::GetInputDeviceList},
					{SettingInfo::Type::Integer, "input_latency", TRANSLATE_NOOP("USB", "Input Latency"),
						TRANSLATE_NOOP("USB", "Specifies the latency to the host input device."),
						AudioDevice::DEFAULT_LATENCY_STR, "1", "1000", "1", TRANSLATE_NOOP("USB", "%dms"), nullptr, nullptr, 1.0f},
				};
				return info;
			}
		}
	}
} // namespace usb_mic

std::unique_ptr<AudioDevice> AudioDevice::CreateNoopDevice(AudioDir dir, u32 channels)
{
	return std::make_unique<usb_mic::audiodev_noop::NoopAudioDevice>(dir, channels);
}

std::unique_ptr<AudioDevice> AudioDevice::CreateDevice(AudioDir dir, u32 channels, std::string devname, s32 latency)
{
	return std::make_unique<usb_mic::audiodev_cubeb::CubebAudioDevice>(dir, channels, std::move(devname), latency);
}

std::vector<std::pair<std::string, std::string>> AudioDevice::GetInputDeviceList()
{
	return usb_mic::audiodev_cubeb::CubebAudioDevice::GetDeviceList(true);
}

std::vector<std::pair<std::string, std::string>> AudioDevice::GetOutputDeviceList()
{
	return usb_mic::audiodev_cubeb::CubebAudioDevice::GetDeviceList(false);
}
