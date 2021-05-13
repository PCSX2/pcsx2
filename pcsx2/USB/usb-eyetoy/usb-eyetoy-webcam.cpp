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

		int frame_step;
		unsigned char* mpeg_frame_data;
		unsigned int mpeg_frame_size;
		unsigned int mpeg_frame_offset;
		uint8_t alts[3];
		uint8_t filter_log;
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

	static void reset_i2c(EYETOYState* s)
	{
		/* OV7648 defaults */
		s->i2c_regs[0x00] = 0x00; //gain
		s->i2c_regs[0x01] = 0x80; //blue
		s->i2c_regs[0x02] = 0x80; //red
		s->i2c_regs[0x03] = 0x84; //saturation
		s->i2c_regs[0x04] = 0x34; //hue
		s->i2c_regs[0x05] = 0x3E; //AWB
		s->i2c_regs[0x06] = 0x80; //ABC brightness
		s->i2c_regs[0x0A] = 0x76; //Product ID r/o
		s->i2c_regs[0x0B] = 0x48; //Product version r/o
		s->i2c_regs[0x10] = 0x41; //exposure
		s->i2c_regs[0x11] = 0x00; //clk
		s->i2c_regs[0x12] = 0x14; //Common A
		s->i2c_regs[0x13] = 0xA3; //Common B
		s->i2c_regs[0x14] = 0x04; //Common C
		s->i2c_regs[0x15] = 0x00; //Common D
		s->i2c_regs[0x17] = 0x1A; //hstart
		s->i2c_regs[0x18] = 0xBA; //hstop
		s->i2c_regs[0x19] = 0x03; //vstart
		s->i2c_regs[0x1A] = 0xF3; //vstop
		s->i2c_regs[0x1B] = 0x00; //pshift
		s->i2c_regs[0x1C] = 0x7F; //Manufacture ID High read-only
		s->i2c_regs[0x1D] = 0xA2; //Manufacture ID Low read-only
		s->i2c_regs[0x1F] = 0x01; //output format
		s->i2c_regs[0x20] = 0xC0; //Common E
		s->i2c_regs[0x24] = 0x10; //AEW
		s->i2c_regs[0x25] = 0x8A; //AEB
		s->i2c_regs[0x26] = 0xA2; //Common F
		s->i2c_regs[0x27] = 0xE2; //Common G
		s->i2c_regs[0x28] = 0x20; //Common H
		s->i2c_regs[0x29] = 0x00; //Common I
		s->i2c_regs[0x2A] = 0x00; //Frame rate adj HI
		s->i2c_regs[0x2B] = 0x00; //Frame rate adj LO
		s->i2c_regs[0x2D] = 0x81; //Common J
		s->i2c_regs[0x60] = 0x06; //Signal process B
		s->i2c_regs[0x6C] = 0x11; //Color matrix R
		s->i2c_regs[0x6D] = 0x01; //Color matrix G
		s->i2c_regs[0x6E] = 0x06; //Color matrix B
		s->i2c_regs[0x71] = 0x00; //Common L
		s->i2c_regs[0x72] = 0x10; //HSYNC rising
		s->i2c_regs[0x73] = 0x50; //HSYNC falling
		s->i2c_regs[0x74] = 0x20; //Common M
		s->i2c_regs[0x75] = 0x02; //Common N
		s->i2c_regs[0x76] = 0x00; //Common O
		s->i2c_regs[0x7E] = 0x00; //AVGY
		s->i2c_regs[0x7F] = 0x00; //AVGR
		s->i2c_regs[0x80] = 0x00; //AVGB
	}

	static void eyetoy_handle_reset(USBDevice* dev)
	{
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
						if (data[0] == 0x42)
						{
							Console.WriteLn("EyeToy : configured for MPEG format");
						}
						else if (data[0] == 0x33)
						{
							Console.WriteLn("EyeToy : configured for JPEG format; Unimplemented");
						}
						else
						{
							Console.WriteLn("EyeToy : configured for unknown format");
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
							if (reg == 0x12)
							{
								const bool mirroring_enabled = val & 0x40;
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

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1)
				{
					memset(data, 0xff, sizeof(data));

					if (s->frame_step == 0)
					{
						s->mpeg_frame_size = s->videodev->GetImage(s->mpeg_frame_data, 320 * 240 * 2);
						if (s->mpeg_frame_size == 0)
						{
							p->status = USB_RET_NAK;
							break;
						}

						uint8_t header[] = {
							0xFF, 0xFF, 0xFF, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x69, 0x70, 0x75, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x40, 0x01, 0xF0, 0x00, 0x01, 0x00, 0x00, 0x00};
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
		s->videodev->Open();
		return 1;
	}

	void eyetoy_close(USBDevice* dev)
	{
		EYETOYState* s = (EYETOYState*)dev;
		s->videodev->Close();
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

		reset_i2c(s);
		s->frame_step = 0;
		s->mpeg_frame_data = (unsigned char*)calloc(1, 320 * 240 * 4); // TODO: 640x480 ?
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

	int EyeToyWebCamDevice::Freeze(int mode, USBDevice* dev, void* data)
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
