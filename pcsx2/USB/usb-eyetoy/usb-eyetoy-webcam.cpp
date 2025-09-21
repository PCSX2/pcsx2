// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host.h"
#include "IconsPromptFont.h"
#include "videodev.h"
#include "usb-eyetoy-webcam.h"
#include "ov519.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-mic/audio.h"
#include "USB/usb-mic/usb-mic.h"
#include "USB/USB.h"
#include "StateWrapper.h"

#include "common/Console.h"

namespace usb_eyetoy
{
	typedef struct EYETOYState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;

		u32 port;
		u32 subtype;

		std::unique_ptr<VideoDevice> videodev;
		USBDevice* mic;
		u8 regs[0xFF]; //OV519
		u8 i2c_regs[0xFF]; //OV764x

		int hw_camera_running;
		int frame_step;
		std::unique_ptr<unsigned char[]> mpeg_frame_data;
		unsigned int mpeg_frame_size;
		unsigned int mpeg_frame_offset;
	} EYETOYState;

	static const USBDescStrings desc_strings = {
		"",
		"Sony corporation",
		"EyeToy USB camera Namtai",
	};

	static void reset_controller(EYETOYState* s)
	{
		if (s->subtype == TYPE_EYETOY)
		{
			memcpy(s->regs, ov519_defaults, sizeof(s->regs));
		}
		else if (s->subtype == TYPE_OV511P)
		{
			memcpy(s->regs, ov511p_defaults, sizeof(s->regs));
		}
	}

	static void reset_sensor(EYETOYState* s)
	{
		if (s->subtype == TYPE_EYETOY)
		{
			memcpy(s->i2c_regs, ov7648_defaults, sizeof(s->regs));
		}
		else if (s->subtype == TYPE_OV511P)
		{
			memcpy(s->i2c_regs, ov7620_defaults, sizeof(s->regs));
		}
	}

	static void open_camera(EYETOYState* s)
	{
		if (s->hw_camera_running && s->subtype == TYPE_EYETOY)
		{
			const int width = s->regs[OV519_R10_H_SIZE] << 4;
			const int height = s->regs[OV519_R11_V_SIZE] << 3;
			const FrameFormat format = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? format_jpeg : format_mpeg;
			const int mirror = !!(s->i2c_regs[OV7610_REG_COM_A] & OV7610_REG_COM_A_MASK_MIRROR);
			Console.WriteLn(
				"EyeToy : eyetoy_open(); hw=%d, w=%d, h=%d, fmt=%d, mirr=%d", s->hw_camera_running, width, height, format, mirror);
			if (s->videodev->Open(width, height, format, mirror) != 0)
				Console.Error("(Eyetoy) Failed to open video device");
		}
		else if (s->hw_camera_running && s->subtype == TYPE_OV511P)
		{
			constexpr int width = 320;
			constexpr int height = 240;
			const FrameFormat format = format_yuv400;
			constexpr int mirror = 0;
			Console.WriteLn(
				"EyeToy : eyetoy_open(); hw=%d, w=%d, h=%d, fmt=%d, mirr=%d", s->hw_camera_running, width, height, format, mirror);
			if (s->videodev->Open(width, height, format, mirror) != 0)
				Console.Error("(Eyetoy) Failed to open video device");
		}
	}

	static void close_camera(EYETOYState* s)
	{
		Console.WriteLn("EyeToy : eyetoy_close(); hw=%d", s->hw_camera_running);
		if (s->hw_camera_running)
		{
			s->hw_camera_running = 0;
			s->videodev->Close();
		}
	}

	static void eyetoy_handle_reset(USBDevice* dev)
	{
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
		reset_controller(s);
		reset_sensor(s);
		if (s->mic)
			s->mic->klass.handle_reset(s->mic);
	}

	static void webcam_handle_control_eyetoy(USBDevice* dev, USBPacket* p, int request, int value, int index, int length, u8* data)
	{
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
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
							close_camera(s);
							open_camera(s);
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
							//u8 reg = s->regs[s->regs[R51x_I2C_W_SID]];
							const u8 reg = s->regs[R51x_I2C_SADDR_3];
							const u8 val = s->regs[R51x_I2C_DATA];
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
							const u8 i2c_reg = s->regs[R51x_I2C_SADDR_2];
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

	static void webcam_handle_control_ov511p(USBDevice* dev, USBPacket* p, int request, int value, int index, int length, u8* data)
	{
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
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
							u8 reg = s->regs[R51x_I2C_SADDR_3];
							const u8 val = s->regs[R51x_I2C_DATA];
							if (reg < sizeof(s->i2c_regs))
							{
								s->i2c_regs[reg] = val;
							}
						}
						else if (s->regs[R511_I2C_CTL] == 0x03 && data[0] == 0x05)
						{
							const u8 i2c_reg = s->regs[R51x_I2C_SADDR_2];
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
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
		static constexpr unsigned int max_ep_size = 896;
		const u8 devep = p->ep->nr;

		if (!s->hw_camera_running)
		{
			Console.WriteLn("EyeToy : initialization done; start the camera");
			s->hw_camera_running = 1;
			open_camera(s);
		}

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				u8 data[max_ep_size];
				pxAssert(p->buffer_size <= max_ep_size);
				if (devep == 1)
				{
					if (s->frame_step == 0)
					{
						s->mpeg_frame_size = s->videodev->GetImage(s->mpeg_frame_data.get(), 640 * 480 * 3);
						if (s->mpeg_frame_size == 0)
						{
							p->status = USB_RET_NAK;
							break;
						}

						u8 header[] = {0xFF, 0xFF, 0xFF, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
						header[0x0A] = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? 0x03 : 0x01;
						std::memcpy(data, header, sizeof(header));

						const u32 data_pk = std::min(p->buffer_size - static_cast<u32>(sizeof(header)), s->mpeg_frame_size);
						std::memcpy(data + sizeof(header), s->mpeg_frame_data.get(), data_pk);
						usb_packet_copy(p, data, sizeof(header) + data_pk);

						s->mpeg_frame_offset = data_pk;
						s->frame_step++;
					}
					else if (s->mpeg_frame_offset < s->mpeg_frame_size)
					{
						const u32 data_pk = std::min(s->mpeg_frame_size - s->mpeg_frame_offset, p->buffer_size);
						usb_packet_copy(p, s->mpeg_frame_data.get() + s->mpeg_frame_offset, data_pk);

						s->mpeg_frame_offset += data_pk;
						s->frame_step++;
					}
					else
					{
						u8 footer[] = {0xFF, 0xFF, 0xFF, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
						footer[0x0A] = s->regs[OV519_RA0_FORMAT] == OV519_RA0_FORMAT_JPEG ? 0x03 : 0x01;
						usb_packet_copy(p, footer, sizeof(footer));
						s->frame_step = 0;
					}
				}
				else if (devep == 2)
				{
					if (s->mic)
						s->mic->klass.handle_data(s->mic, p);
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
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
		const u8 devep = p->ep->nr;

		if (!s->hw_camera_running)
		{
			Console.WriteLn("EyeToy : initialization done; start the camera");
			s->hw_camera_running = 1;
			open_camera(s);
		}

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1)
				{
					if (s->frame_step == 0)
					{
						s->mpeg_frame_size = s->videodev->GetImage(s->mpeg_frame_data.get(), 640 * 480 * 3);
						if (s->mpeg_frame_size == 0)
						{
							p->status = USB_RET_NAK;
							break;
						}

						static const unsigned int max_ep_size = 961;
						u8 data[max_ep_size];
						pxAssert(p->buffer_size <= max_ep_size);

						static constexpr const u8 header[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28}; // 28 <> 29
						std::memcpy(data, header, sizeof(header));

						const u32 data_pk = std::min(p->buffer_size - 1 - static_cast<u32>(sizeof(header)), s->mpeg_frame_size);
						std::memcpy(data + sizeof(header), s->mpeg_frame_data.get(), data_pk);
						usb_packet_copy(p, data, sizeof(header) + data_pk);

						s->mpeg_frame_offset = data_pk;
						s->frame_step++;
					}
					else if (s->mpeg_frame_offset < s->mpeg_frame_size)
					{
						const u32 data_pk = std::min(s->mpeg_frame_size - s->mpeg_frame_offset, p->buffer_size - 1);
						usb_packet_copy(p, s->mpeg_frame_data.get() + s->mpeg_frame_offset, data_pk);

						s->mpeg_frame_offset += data_pk;
						s->frame_step++;
					}
					else
					{
						static constexpr const u8 footer[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x09, 0x07};
						usb_packet_copy(p, const_cast<u8*>(footer), sizeof(footer));
						s->frame_step = 0;
					}
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
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
		close_camera(s);
		if (s->mic)
			s->mic->klass.unrealize(s->mic);
		delete s;
	}

	USBDevice* EyeToyWebCamDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		const usb_mic::MicrophoneDevice* mic_proxy =
			static_cast<usb_mic::MicrophoneDevice*>(RegisterDevice::instance().Device(DEVTYPE_MICROPHONE));
		if (!mic_proxy)
			return nullptr;

		std::unique_ptr<VideoDevice> videodev(VideoDevice::CreateInstance());
		if (!videodev)
		{
			Console.Error("Failed to create video device.");
			return nullptr;
		}

		videodev->HostDevice(USB::GetConfigString(si, port, TypeName(), "device_name"));

		EYETOYState* s = new EYETOYState();
		s->subtype = subtype;
		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (subtype == TYPE_EYETOY)
		{
			if (usb_desc_parse_dev(eyetoy_dev_descriptor, sizeof(eyetoy_dev_descriptor), s->desc, s->desc_dev) < 0)
				goto fail;
			if (usb_desc_parse_config(eyetoy_config_descriptor, sizeof(eyetoy_config_descriptor), s->desc_dev) < 0)
				goto fail;
			s->dev.klass.handle_control = webcam_handle_control_eyetoy;
			s->dev.klass.handle_data = webcam_handle_data_eyetoy;

			s->mic = mic_proxy->CreateDevice(si, port, 0, false, 16000, TypeName());
		}
		else if (subtype == TYPE_OV511P)
		{
			if (usb_desc_parse_dev(ov511p_dev_descriptor, sizeof(ov511p_dev_descriptor), s->desc, s->desc_dev) < 0)
				goto fail;
			if (usb_desc_parse_config(ov511p_config_descriptor, sizeof(ov511p_config_descriptor), s->desc_dev) < 0)
				goto fail;
			s->dev.klass.handle_control = webcam_handle_control_ov511p;
			s->dev.klass.handle_data = webcam_handle_data_ov511p;
		}

		s->videodev = std::move(videodev);
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = eyetoy_handle_reset;
		s->dev.klass.unrealize = eyetoy_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		eyetoy_handle_reset(&s->dev);

		s->hw_camera_running = 0;
		s->frame_step = 0;
		s->mpeg_frame_data = std::make_unique<unsigned char[]>(640 * 480 * 3);
		std::memset(s->mpeg_frame_data.get(), 0, 640 * 480 * 3);
		s->mpeg_frame_offset = 0;

		return &s->dev;
	fail:
		eyetoy_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* EyeToyWebCamDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Webcam (EyeToy)");
	}

	const char* EyeToyWebCamDevice::TypeName() const
	{
		return "webcam";
	}

	const char* EyeToyWebCamDevice::IconName() const
	{
		return ICON_PF_EYETOY_WEBCAM;
	}

	bool EyeToyWebCamDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		EYETOYState* s = USB_CONTAINER_OF(dev, EYETOYState, dev);
		if (!sw.DoMarker("EYETOYState"))
			return false;

		sw.DoBytes(s->regs, sizeof(s->regs));
		sw.DoBytes(s->i2c_regs, sizeof(s->i2c_regs));
		sw.Do(&s->frame_step);
		sw.DoBytes(s->mpeg_frame_data.get(), 640 * 480 * 3);
		sw.Do(&s->mpeg_frame_size);
		sw.Do(&s->mpeg_frame_offset);
		return !sw.HasError();
	}

	void EyeToyWebCamDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		// TODO: Update device name
	}

	std::span<const char*> EyeToyWebCamDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Sony EyeToy"),
			TRANSLATE_NOOP("USB", "Konami Capture Eye")
		};
		return subtypes;
	}

	std::span<const SettingInfo> EyeToyWebCamDevice::Settings(u32 subtype) const
	{
		switch (subtype)
		{
			case TYPE_EYETOY:
			{
				static constexpr const SettingInfo info[] = {
					{SettingInfo::Type::StringList, "device_name", TRANSLATE_NOOP("USB", "Video Device"),
						TRANSLATE_NOOP("USB", "Selects the device to capture images from."), "", nullptr, nullptr, nullptr,
						nullptr, nullptr, &VideoDevice::GetDeviceList},
					{SettingInfo::Type::StringList, "input_device_name", TRANSLATE_NOOP("USB", "Audio Device"),
						TRANSLATE_NOOP("USB", "Selects the device to read audio from."), "", nullptr, nullptr, nullptr, nullptr,
						nullptr, &AudioDevice::GetInputDeviceList},
					{SettingInfo::Type::Integer, "input_latency", TRANSLATE_NOOP("USB", "Audio Latency"),
						TRANSLATE_NOOP("USB", "Specifies the latency to the host input device."),
						AudioDevice::DEFAULT_LATENCY_STR, "1", "1000", "1", TRANSLATE_NOOP("USB", "%dms"), nullptr, nullptr, 1.0f},
				};
				return info;
			}
			case TYPE_OV511P:
			{
				static constexpr const SettingInfo info[] = {
					{SettingInfo::Type::StringList, "device_name", TRANSLATE_NOOP("USB", "Video Device"),
						TRANSLATE_NOOP("USB", "Selects the device to capture images from."), "", nullptr, nullptr, nullptr,
						nullptr, nullptr, &VideoDevice::GetDeviceList},
				};
				return info;
			}
			default:
				return {};
		}
	}
} // namespace usb_eyetoy
