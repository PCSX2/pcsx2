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
#include "videodeviceproxy.h"
#include "usb-eyetoy-webcam.h"
#include "ov519.h"
#include "USB/qemu-usb/desc.h"
#include "USB/shared/inifile_usb.h"

namespace usb_eyetoy
{
	typedef struct EYETOYState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;

		VideoDevice* videodev;
		//	struct freeze {
		uint8_t regs[0xFF];     //OV519
		uint8_t i2c_regs[0xFF]; //OV764x

		int hw_camera_running;
		int frame_step;
		unsigned char* mpeg_frame_data;
		unsigned int mpeg_frame_size;
		unsigned int mpeg_frame_offset;
		//	} f;
	} EYETOYState;

	static EYETOYState* static_state;

	static const USBDescStrings desc_strings = {
		"",
		"Sony corporation",
		"EyeToy USB camera Namtai",
	};

	/*
	Manufacturer:   OmniVision Technologies, Inc.
	Product ID:     0x8519
	Vendor ID:      0x05a9

	Device VendorID/ProductID:   0x054C/0x0155   (Sony Corporation)
	Device Version Number:   0x0100
	Number of Configurations:   1
	Manufacturer String:   1 "Sony corporation"
	Product String:   2 "EyeToy USB camera Namtai"
	*/

	static const uint8_t eyetoy_dev_descriptor[] = {
		0x12,          /* bLength */
		0x01,          /* bDescriptorType */
		WBVAL(0x0110), /* bcdUSB */
		0x00,          /* bDeviceClass */
		0x00,          /* bDeviceSubClass */
		0x00,          /* bDeviceProtocol */
		0x08,          /* bMaxPacketSize0 */
		WBVAL(0x054c), /* idVendor */
		WBVAL(0x0155), /* idProduct */
		WBVAL(0x0100), /* bcdDevice */
		0x01,          /* iManufacturer */
		0x02,          /* iProduct */
		0x00,          /* iSerialNumber */
		0x01,          /* bNumConfigurations */
	};

	static const uint8_t eyetoy_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0xB4, 0x00, // wTotalLength 180
		0x03,       // bNumInterfaces 3
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0xFA,       // bMaxPower 500mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0xFF, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
		0x00, 0x00, // wMaxPacketSize 0
		0x01,       // bInterval 1 (unit depends on device speed)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x01, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0xFF, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
		0x80, 0x01, // wMaxPacketSize 384
		0x01,       // bInterval 1 (unit depends on device speed)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x02, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0xFF, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
		0x00, 0x02, // wMaxPacketSize 512
		0x01,       // bInterval 1 (unit depends on device speed)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x03, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0xFF, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
		0x00, 0x03, // wMaxPacketSize 768
		0x01,       // bInterval 1 (unit depends on device speed)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x04, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0xFF, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x01,       // bmAttributes (Isochronous, No Sync, Data EP)
		0x80, 0x03, // wMaxPacketSize 896
		0x01,       // bInterval 1 (unit depends on device speed)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x01, // bInterfaceNumber 1
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
		0x1E, 0x00, // wTotalLength 30
		0x01,       // binCollection 0x01
		0x02,       // baInterfaceNr 2

		0x0C,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x02,       // bDescriptorSubtype (CS_INTERFACE -> INPUT_TERMINAL)
		0x01,       // bTerminalID
		0x01, 0x02, // wTerminalType (Microphone)
		0x00,       // bAssocTerminal
		0x01,       // bNrChannels 1
		0x00, 0x00, // wChannelConfig
		0x00,       // iChannelNames
		0x00,       // iTerminal

		0x09,       // bLength
		0x24,       // bDescriptorType (See Next Line)
		0x03,       // bDescriptorSubtype (CS_INTERFACE -> OUTPUT_TERMINAL)
		0x02,       // bTerminalID
		0x01, 0x01, // wTerminalType (USB Streaming)
		0x00,       // bAssocTerminal
		0x01,       // bSourceID
		0x00,       // iTerminal

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x02, // bInterfaceNumber 2
		0x00, // bAlternateSetting
		0x00, // bNumEndpoints 0
		0x01, // bInterfaceClass (Audio)
		0x02, // bInterfaceSubClass (Audio Streaming)
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x02, // bInterfaceNumber 2
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

		0x0B,             // bLength
		0x24,             // bDescriptorType (See Next Line)
		0x02,             // bDescriptorSubtype (CS_INTERFACE -> FORMAT_TYPE)
		0x01,             // bFormatType 1
		0x01,             // bNrChannels (Mono)
		0x02,             // bSubFrameSize 2
		0x10,             // bBitResolution 16
		0x01,             // bSamFreqType 1
		0x80, 0x3E, 0x00, // tSamFreq[1] 16000 Hz

		0x09,       // bLength
		0x05,       // bDescriptorType (See Next Line)
		0x82,       // bEndpointAddress (IN/D2H)
		0x05,       // bmAttributes (Isochronous, Async, Data EP)
		0x28, 0x00, // wMaxPacketSize 40
		0x01,       // bInterval 1 (unit depends on device speed)
		0x00,       // bRefresh
		0x00,       // bSyncAddress

		0x07,       // bLength
		0x25,       // bDescriptorType (See Next Line)
		0x01,       // bDescriptorSubtype (CS_ENDPOINT -> EP_GENERAL)
		0x00,       // bmAttributes (None)
		0x00,       // bLockDelayUnits
		0x00, 0x00, // wLockDelay 0
	};

	static void reset_ov519(EYETOYState* s)
	{
		static const uint8_t ov519_defaults[] = {
			0xc0, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
			0x14, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x08, 0x98, 0xff, 0x00, 0x03, 0x00, 0x00, 0x1e, 0x01, 0xf1, 0x00, 0x01, 0x00, 0x00, 0x00,
			0xff, 0xff, 0xff, 0xff, 0x50, 0x51, 0x52, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0xff, 0x00, 0x01, 0x00, 0x21, 0x00, 0x02, 0x6d, 0x0e, 0x00, 0x02, 0x00, 0x11,
			0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xb4, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x03, 0x03, 0xfc, 0x00, 0xff, 0x00, 0x00, 0xff,
			0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x33, 0x04, 0x40, 0x40, 0x0c, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x0a, 0x0f, 0x1e, 0x2d, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x01, 0x02, 0x03, 0x00, 0x05, 0x02, 0x07, 0x00, 0x09, 0x02, 0x0b, 0x00, 0x0d, 0x02, 0x0f,
			0x00, 0x11, 0x02, 0x13, 0x00, 0x15, 0x02, 0x17, 0x00, 0x19, 0x02, 0x1b, 0x00, 0x1d, 0x02, 0x1f,
			0x50, 0x64, 0x82, 0x96, 0x82, 0x81, 0x00, 0x01, 0x02, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		};
		memcpy(s->regs, ov519_defaults, sizeof(s->regs));
	}

	static void reset_i2c(EYETOYState* s)
	{
		static const uint8_t ov7648_defaults[] = {
			0x00, 0x84, 0x84, 0x84, 0x34, 0x3e, 0x80, 0x8c, 0x00, 0x00, 0x76, 0x48, 0x7b, 0x5b, 0x00, 0x98,
			0x57, 0x00, 0x14, 0xa3, 0x04, 0x00, 0x00, 0x1a, 0xba, 0x03, 0xf3, 0x00, 0x7f, 0xa2, 0x00, 0x01,
			0xc0, 0x80, 0x80, 0xde, 0x10, 0x8a, 0xa2, 0xe2, 0x20, 0x00, 0x00, 0x00, 0x88, 0x81, 0x00, 0x94,
			0x40, 0xa0, 0xc0, 0x16, 0x16, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
			0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00,
			0x06, 0xe0, 0x88, 0x11, 0x89, 0x02, 0x55, 0x01, 0x7a, 0x04, 0x00, 0x00, 0x11, 0x01, 0x06, 0x00,
			0x01, 0x00, 0x10, 0x50, 0x20, 0x02, 0x00, 0xf3, 0x80, 0x80, 0x80, 0x00, 0x00, 0x47, 0x27, 0x8a,
			0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
			0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x75, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		};
		memcpy(s->i2c_regs, ov7648_defaults, sizeof(s->regs));
	}

	static void eyetoy_handle_reset(USBDevice* dev)
	{
		reset_ov519((EYETOYState*)dev);
		reset_i2c((EYETOYState*)dev);
	}

	static void eyetoy_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
									  int index, int length, uint8_t* data)
	{
		EYETOYState* s = (EYETOYState*)dev;
		int ret = 0;

		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			case VendorDeviceRequest | 0x1: //Read register
				data[0] = s->regs[index & 0xFF];
				p->actual_length = 1;
				break;

			case VendorDeviceOutRequest | 0x1: //Write register
				switch (index)
				{
					case OV519_RA0_FORMAT:
						if (data[0] == OV519_RA0_FORMAT_MPEG)
						{
							Console.WriteLn("EyeToy : configured for MPEG format");
						}
						else if (data[0] == OV519_RA0_FORMAT_JPEG)
						{
							Console.WriteLn("EyeToy : configured for JPEG format");
						}
						else
						{
							Console.WriteLn("EyeToy : configured for unknown format");
						}

						if (s->hw_camera_running && s->regs[OV519_RA0_FORMAT] != data[0])
						{
							Console.WriteLn("EyeToy : reinitialize the camera");
							s->dev.klass.close(dev);
							s->dev.klass.open(dev);
						}
						break;
					case OV519_R10_H_SIZE:
						Console.WriteLn("EyeToy : Image width : %d", data[0] << 4);
						break;
					case OV519_R11_V_SIZE:
						Console.WriteLn("EyeToy : Image height : %d", data[0] << 3);
						break;
					case OV519_GPIO_DATA_OUT0:
						{
							static char led_state = -1;
							if (led_state != data[0])
							{
								led_state = data[0];
								Console.WriteLn("EyeToy : LED : %d", !!led_state);
							}
						}
						break;
					case R518_I2C_CTL:
						if (data[0] == 1) // Commit I2C write
						{
							//uint8_t reg = s->regs[s->regs[R51x_I2C_W_SID]];
							uint8_t reg = s->regs[R51x_I2C_SADDR_3];
							uint8_t val = s->regs[R51x_I2C_DATA];
							if ((reg == 0x12) && (val & 0x80))
							{
								s->i2c_regs[0x12] = val & ~0x80; //or skip?
								reset_i2c(s);
							}
							else if (reg < sizeof(s->i2c_regs))
							{
								s->i2c_regs[reg] = val;
							}
							if (reg == OV7610_REG_COM_A)
							{
								const bool mirroring_enabled = val & OV7610_REG_COM_A_MASK_MIRROR;
								s->videodev->SetMirroring(mirroring_enabled);
								Console.WriteLn("EyeToy : mirroring %s", mirroring_enabled ? "ON" : "OFF");
							}
						}
						else if (s->regs[R518_I2C_CTL] == 0x03 && data[0] == 0x05)
						{
							//s->regs[s->regs[R51x_I2C_R_SID]] but seems to default to 0x43 (R51x_I2C_SADDR_2)
							uint8_t i2c_reg = s->regs[R51x_I2C_SADDR_2];
							s->regs[R51x_I2C_DATA] = 0;

							if (i2c_reg < sizeof(s->i2c_regs))
							{
								s->regs[R51x_I2C_DATA] = s->i2c_regs[i2c_reg];
							}
						}
						break;
					default:
						break;
				}

				//Max 0xFFFF regs?
				s->regs[index & 0xFF] = data[0];
				p->actual_length = 1;

				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void eyetoy_handle_data(USBDevice* dev, USBPacket* p)
	{
		EYETOYState* s = (EYETOYState*)dev;
		static const int max_ep_size = 896;
		uint8_t data[max_ep_size];
		uint8_t devep = p->ep->nr;

		if (!s->hw_camera_running)
		{
			Console.WriteLn("EyeToy : initialization done; start the camera");
			s->hw_camera_running = 1;
			s->dev.klass.open(dev);
		}

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1)
				{
					memset(data, 0xff, sizeof(data));

					if (s->frame_step == 0)
					{
						s->mpeg_frame_size = s->videodev->GetImage(s->mpeg_frame_data, 640 * 480 * 3);
						if (s->mpeg_frame_size == 0)
						{
							p->status = USB_RET_NAK;
							break;
						}

						uint8_t header[] = {
							0xFF, 0xFF, 0xFF, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
						header[0x0A] = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? 0x03 : 0x01;
						memcpy(data, header, sizeof(header));

						int data_pk = max_ep_size - sizeof(header);
						memcpy(data + sizeof(header), s->mpeg_frame_data, data_pk);
						s->mpeg_frame_offset = data_pk;
						s->frame_step++;
					}
					else if (s->mpeg_frame_offset < s->mpeg_frame_size)
					{
						int data_pk = s->mpeg_frame_size - s->mpeg_frame_offset;
						if (data_pk > max_ep_size)
							data_pk = max_ep_size;
						memcpy(data, s->mpeg_frame_data + s->mpeg_frame_offset, data_pk);
						s->mpeg_frame_offset += data_pk;
						s->frame_step++;
					}
					else
					{
						uint8_t footer[] = {
							0xFF, 0xFF, 0xFF, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
						footer[0x0A] = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? 0x03 : 0x01;
						memcpy(data, footer, sizeof(footer));
						s->frame_step = 0;
					}

					usb_packet_copy(p, data, max_ep_size);
				}
				else if (devep == 2)
				{
					// get audio
					//Console.Warning("get audio %d\n", len);
					memset(data, 0, p->iov.size);
					usb_packet_copy(p, data, p->iov.size);
				}
				break;
			case USB_TOKEN_OUT:
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void eyetoy_handle_destroy(USBDevice* dev)
	{
		EYETOYState* s = (EYETOYState*)dev;

		delete s;
	}

	int eyetoy_open(USBDevice* dev)
	{
		EYETOYState* s = (EYETOYState*)dev;
		if (s->hw_camera_running)
		{
			const int width = s->regs[OV519_R10_H_SIZE] << 4;
			const int height = s->regs[OV519_R11_V_SIZE] << 3;
			const FrameFormat format = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? format_jpeg : format_mpeg;
			const int mirror = !!(s->i2c_regs[OV7610_REG_COM_A] & OV7610_REG_COM_A_MASK_MIRROR);
			Console.Error("EyeToy : eyetoy_open(); hw=%d, w=%d, h=%d, fmt=%d, mirr=%d", s->hw_camera_running,
				width, height, format, mirror);
			s->videodev->Open(width, height, format, mirror);
		}
		return 1;
	}

	void eyetoy_close(USBDevice* dev)
	{
		EYETOYState* s = (EYETOYState*)dev;
		Console.Error("EyeToy : eyetoy_close(); hw=%d", s->hw_camera_running);
		if (s->hw_camera_running)
		{
			s->hw_camera_running = 0;
			s->videodev->Close();
		}
	}

	USBDevice* EyeToyWebCamDevice::CreateDevice(int port)
	{
		VideoDevice* videodev = nullptr;
		std::string varApi;
#ifdef _WIN32
		std::wstring tmp;
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, tmp);
		varApi = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, port, TypeName(), N_DEVICE_API, varApi);
#endif
		VideoDeviceProxyBase* proxy = RegisterVideoDevice::instance().Proxy(varApi);
		if (!proxy)
		{
			Console.WriteLn("Invalid video device API: %" SFMTs "\n", varApi.c_str());
			return NULL;
		}

		videodev = proxy->CreateObject(port);

		if (!videodev)
			return NULL;

		EYETOYState* s;

		s = new EYETOYState();
		if (!s)
			return NULL;

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(eyetoy_dev_descriptor, sizeof(eyetoy_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(eyetoy_config_descriptor, sizeof(eyetoy_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->videodev = videodev;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = eyetoy_handle_reset;
		s->dev.klass.handle_control = eyetoy_handle_control;
		s->dev.klass.handle_data = eyetoy_handle_data;
		s->dev.klass.unrealize = eyetoy_handle_destroy;
		s->dev.klass.open = eyetoy_open;
		s->dev.klass.close = eyetoy_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		eyetoy_handle_reset((USBDevice*)s);

		s->hw_camera_running = 0;
		s->frame_step = 0;
		s->mpeg_frame_data = (unsigned char*)calloc(1, 640 * 480 * 3);
		s->mpeg_frame_offset = 0;

		static_state = s;
		return (USBDevice*)s;
	fail:
		eyetoy_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int EyeToyWebCamDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterVideoDevice::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int EyeToyWebCamDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		/*switch (mode)
	{
		case FREEZE_LOAD:
			if (!s) return -1;
			s->f = *(PADState::freeze *)data;
			s->pad->Type((PS2WheelTypes)s->f.wheel_type);
			return sizeof(PADState::freeze);
		case FREEZE_SAVE:
			if (!s) return -1;
			*(PADState::freeze *)data = s->f;
			return sizeof(PADState::freeze);
		case FREEZE_SIZE:
			return sizeof(PADState::freeze);
		default:
		break;
	}*/
		return 0;
	}

} // namespace usb_eyetoy
