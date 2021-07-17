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

	static void reset_controller(EYETOYState* s)
	{
		if (s->videodev->Type() == TYPE_EYETOY)
		{
			memcpy(s->regs, ov519_defaults, sizeof(s->regs));
		}
		else if (s->videodev->Type() == TYPE_OV511P)
		{
			memcpy(s->regs, ov511p_defaults, sizeof(s->regs));
		}
	}

	static void reset_sensor(EYETOYState* s)
	{
		if (s->videodev->Type() == TYPE_EYETOY)
		{
			memcpy(s->i2c_regs, ov7648_defaults, sizeof(s->regs));
		}
		else if (s->videodev->Type() == TYPE_OV511P)
		{
			memcpy(s->i2c_regs, ov7620_defaults, sizeof(s->regs));
		}
	}

	static void eyetoy_handle_reset(USBDevice* dev)
	{
		reset_controller((EYETOYState*)dev);
		reset_sensor((EYETOYState*)dev);
	}

	static void webcam_handle_control_eyetoy(USBDevice* dev, USBPacket* p, int request, int value,
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
								reset_sensor(s);
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

	static void webcam_handle_control_ov511p(USBDevice* dev, USBPacket* p, int request, int value,
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
			case VendorDeviceRequest | 0x3: //Read register
				data[0] = s->regs[index & 0xFF];
				p->actual_length = 1;
				break;

			case VendorDeviceOutRequest | 0x2: //Write register
				switch (index)
				{
					case R511_I2C_CTL:
						if (data[0] == 1)
						{
							uint8_t reg = s->regs[R51x_I2C_SADDR_3];
							uint8_t val = s->regs[R51x_I2C_DATA];
							if (reg < sizeof(s->i2c_regs))
							{
								s->i2c_regs[reg] = val;
							}
						}
						else if (s->regs[R511_I2C_CTL] == 0x03 && data[0] == 0x05)
						{
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

				s->regs[index & 0xFF] = data[0];
				p->actual_length = 1;

				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void webcam_handle_data_eyetoy(USBDevice* dev, USBPacket* p)
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

	static void webcam_handle_data_ov511p(USBDevice* dev, USBPacket* p)
	{
		EYETOYState* s = (EYETOYState*)dev;
		static const int max_ep_size = 960; // 961
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
					memset(data, 0x00, sizeof(data));

					if (s->frame_step == 0)
					{
						s->mpeg_frame_size = s->videodev->GetImage(s->mpeg_frame_data, 640 * 480 * 3);
						if (s->mpeg_frame_size == 0)
						{
							p->status = USB_RET_NAK;
							break;
						}

						uint8_t header[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28}; // 28 <> 29
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
						uint8_t footer[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x09, 0x07};
						memcpy(data, footer, sizeof(footer));
						s->frame_step = 0;
					}

					usb_packet_copy(p, data, max_ep_size);
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
		if (s->hw_camera_running && s->videodev->Type() == TYPE_EYETOY)
		{
			const int width = s->regs[OV519_R10_H_SIZE] << 4;
			const int height = s->regs[OV519_R11_V_SIZE] << 3;
			const FrameFormat format = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? format_jpeg : format_mpeg;
			const int mirror = !!(s->i2c_regs[OV7610_REG_COM_A] & OV7610_REG_COM_A_MASK_MIRROR);
			Console.Error("EyeToy : eyetoy_open(); hw=%d, w=%d, h=%d, fmt=%d, mirr=%d", s->hw_camera_running,
				width, height, format, mirror);
			s->videodev->Open(width, height, format, mirror);
		}
		else if (s->hw_camera_running && s->videodev->Type() == TYPE_OV511P)
		{
			const int width = 320;
			const int height = 240;
			const FrameFormat format = format_yuv400;
			const int mirror = 0;
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

		videodev->Type(GetSelectedSubtype(std::make_pair(port, TypeName())));

		EYETOYState* s;
		s = new EYETOYState();
		if (!s)
			return NULL;

		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (videodev->Type() == TYPE_EYETOY)
		{
			if (usb_desc_parse_dev(eyetoy_dev_descriptor, sizeof(eyetoy_dev_descriptor), s->desc, s->desc_dev) < 0)
				goto fail;
			if (usb_desc_parse_config(eyetoy_config_descriptor, sizeof(eyetoy_config_descriptor), s->desc_dev) < 0)
				goto fail;
			s->dev.klass.handle_control = webcam_handle_control_eyetoy;
			s->dev.klass.handle_data = webcam_handle_data_eyetoy;
		}
		else if (videodev->Type() == TYPE_OV511P)
		{
			if (usb_desc_parse_dev(ov511p_dev_descriptor, sizeof(ov511p_dev_descriptor), s->desc, s->desc_dev) < 0)
				goto fail;
			if (usb_desc_parse_config(ov511p_config_descriptor, sizeof(ov511p_config_descriptor), s->desc_dev) < 0)
				goto fail;
			s->dev.klass.handle_control = webcam_handle_control_ov511p;
			s->dev.klass.handle_data = webcam_handle_data_ov511p;
		}

		s->videodev = videodev;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = eyetoy_handle_reset;
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
