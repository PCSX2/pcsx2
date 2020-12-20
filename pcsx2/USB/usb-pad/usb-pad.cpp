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
#include "USB/shared/inifile_usb.h"

namespace usb_pad
{

	static const USBDescStrings df_desc_strings = {
		"",
		"Logitech Driving Force",
		"",
		"Logitech",
	};

	static const USBDescStrings dfp_desc_strings = {
		"",
		"Logitech Driving Force Pro",
		"",
		"Logitech",
	};

	static const USBDescStrings gtf_desc_strings = {
		"",
		"Logitech",         //actual index @ 0x04
		"Logitech GT Force" //actual index @ 0x20
	};

	static const USBDescStrings rb1_desc_strings = {
		"1234567890AB",
		"Licensed by Sony Computer Entertainment America",
		"Harmonix Drum Kit for PlayStation(R)3"};

	static const USBDescStrings buzz_desc_strings = {
		"",
		"Logitech Buzz(tm) Controller V1",
		"",
		"Logitech"};

	static const USBDescStrings kbm_desc_strings = {
		"",
		"USB Multipurpose Controller",
		"",
		"KONAMI"};

	static const USBDescStrings gametrak_desc_strings = {
		"",
		"In2Games Ltd.",
		"Game-Trak V1.3"};

	static const USBDescStrings realplay_desc_strings = {
		"",
		"In2Games",
		"Real Play"};

	std::list<std::string> PadDevice::ListAPIs()
	{
		return RegisterPad::instance().Names();
	}

	const TCHAR* PadDevice::LongAPIName(const std::string& name)
	{
		auto proxy = RegisterPad::instance().Proxy(name);
		if (proxy)
			return proxy->Name();
		return nullptr;
	}

	std::list<std::string> RBDrumKitDevice::ListAPIs()
	{
		return PadDevice::ListAPIs();
	}

	const TCHAR* RBDrumKitDevice::LongAPIName(const std::string& name)
	{
		return PadDevice::LongAPIName(name);
	}

	std::list<std::string> BuzzDevice::ListAPIs()
	{
		return PadDevice::ListAPIs();
	}

	const TCHAR* BuzzDevice::LongAPIName(const std::string& name)
	{
		return PadDevice::LongAPIName(name);
	}

	std::list<std::string> KeyboardmaniaDevice::ListAPIs()
	{
		return PadDevice::ListAPIs();
	}

	const TCHAR* KeyboardmaniaDevice::LongAPIName(const std::string& name)
	{
		return PadDevice::LongAPIName(name);
	}

	std::list<std::string> GametrakDevice::ListAPIs()
	{
		return PadDevice::ListAPIs();
	}

	const TCHAR* GametrakDevice::LongAPIName(const std::string& name)
	{
		return PadDevice::LongAPIName(name);
	}

	std::list<std::string> RealPlayDevice::ListAPIs()
	{
		return PadDevice::ListAPIs();
	}

	const TCHAR* RealPlayDevice::LongAPIName(const std::string& name)
	{
		return PadDevice::LongAPIName(name);
	}

#ifdef _DEBUG
	void PrintBits(void* data, int size)
	{
		std::vector<unsigned char> buf(size * 8 + 1 + size);
		unsigned char* bits = buf.data();
		unsigned char* ptrD = (unsigned char*)data;
		unsigned char* ptrB = bits;
		for (int i = 0; i < size * 8; i++)
		{
			*(ptrB++) = '0' + (*(ptrD + i / 8) & (1 << (i % 8)) ? 1 : 0);
			if (i % 8 == 7)
				*(ptrB++) = ' ';
		}
		*ptrB = '\0';
	}

#else
#define PrintBits(...)
#define DbgPrint(...)
#endif //_DEBUG

	uint32_t gametrak_compute_key(uint32_t* key)
	{
		uint32_t ret = 0;
		ret = *key << 2 & 0xFC0000;
		ret |= *key << 17 & 0x020000;
		ret ^= *key << 16 & 0xFE0000;
		ret |= *key & 0x010000;
		ret |= *key >> 9 & 0x007F7F;
		ret |= *key << 7 & 0x008080;
		*key = ret;
		return ret >> 16;
	};

	typedef struct PADState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;
		Pad* pad;
		uint8_t port;
		struct freeze
		{
			int dev_subtype;
		} f;
		uint8_t gametrak_state;
		uint32_t gametrak_key;
		uint8_t realplay_state;
	} PADState;

	typedef struct u_wheel_data_t
	{
		union
		{
			generic_data_t generic_data;
			dfp_data_t dfp_data;
			gtforce_data_t gtf_data;
			rb1drumkit_t rb1dk_data;
		} u;
	} u_wheel_data_t;

	//Convert DF Pro buttons to selected wheel type
	uint32_t convert_wt_btn(PS2WheelTypes type, uint32_t inBtn)
	{
		if (type == WT_GT_FORCE)
		{
			/***
		R1 > SQUARE == menu down	L1 > CROSS == menu up
		SQUARE > CIRCLE == X		TRIANG > TRIANG == Y
		CROSS > R1 == A				CIRCLE > L1 == B
		***/
			switch (inBtn)
			{
				case PAD_L1:
					return PAD_CROSS;
				case PAD_R1:
					return PAD_SQUARE;
				case PAD_SQUARE:
					return PAD_CIRCLE;
				case PAD_TRIANGLE:
					return PAD_TRIANGLE;
				case PAD_CIRCLE:
					return PAD_L1;
				case PAD_CROSS:
					return PAD_R1;
				default:
					return PAD_BUTTON_COUNT; //Aka invalid
			}
		}

		return inBtn;
	}

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		PADState* s = (PADState*)dev;
		uint8_t data[64];

		int ret = 0;
		uint8_t devep = p->ep->nr;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1 && s->pad)
				{
					if (s->pad->Type() == WT_GAMETRAK_CONTROLLER)
					{
						if (s->gametrak_state == 0)
						{
							s->gametrak_state = 1;
							const unsigned char secret[] = "Gametrak\0\0\0\0\0\0\0\0";
							memcpy(data, secret, sizeof(secret));
							usb_packet_copy(p, data, 16);
							break;
						}
						else if (s->gametrak_state == 1)
						{
							s->pad->TokenIn(data, p->iov.size);
							data[0x00] |= s->gametrak_key >> 16 & 1;
							data[0x02] |= s->gametrak_key >> 17 & 1;
							data[0x04] |= s->gametrak_key >> 18 & 1;
							data[0x06] |= s->gametrak_key >> 19 & 1;
							data[0x08] |= s->gametrak_key >> 20 & 1;
							data[0x0A] |= s->gametrak_key >> 21 & 1;
							usb_packet_copy(p, data, 16);
							break;
						}
					}
					else if (s->pad->Type() >= WT_REALPLAY_RACING && s->pad->Type() <= WT_REALPLAY_POOL)
					{
						s->pad->TokenIn(data, p->iov.size);
						// simulate a slight move to avoid a game "protection" : controller disconnected
						s->realplay_state = !s->realplay_state;
						data[0] |= s->realplay_state;
						usb_packet_copy(p, data, 19);
						break;
					}
					ret = s->pad->TokenIn(data, p->iov.size);
					if (ret > 0)
						usb_packet_copy(p, data, MIN(ret, (int)sizeof(data)));
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
				/*Console.Warning("usb-pad: data token out len=0x%X %X,%X,%X,%X,%X,%X,%X,%X\n",len,
			data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);*/
				//Console.Warning("usb-pad: data token out len=0x%X\n",len);
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
		PADState* s = (PADState*)dev;
		s->pad->Reset();
		return;
	}

	static void pad_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
								   int index, int length, uint8_t* data)
	{
		PADState* s = (PADState*)dev;
		int ret = 0;

		int t = s->pad->Type();

		switch (request)
		{
			case DeviceRequest | USB_REQ_GET_DESCRIPTOR:
				ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
				if (ret < 0)
					goto fail;

				break;
			case InterfaceRequest | USB_REQ_GET_DESCRIPTOR: //GT3
				switch (value >> 8)
				{
					case USB_DT_REPORT:
						if (t == WT_DRIVING_FORCE_PRO || t == WT_DRIVING_FORCE_PRO_1102)
						{
							ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
							memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
						}
						else if (t == WT_GT_FORCE)
						{
							ret = sizeof(pad_gtforce_hid_report_descriptor);
							memcpy(data, pad_gtforce_hid_report_descriptor, ret);
						}
						else if (t == WT_KEYBOARDMANIA_CONTROLLER)
						{
							ret = sizeof(kbm_hid_report_descriptor);
							memcpy(data, kbm_hid_report_descriptor, ret);
						}
						else if (t == WT_GENERIC)
						{
							ret = sizeof(pad_driving_force_hid_separate_report_descriptor);
							memcpy(data, pad_driving_force_hid_separate_report_descriptor, ret);
						}
						else if (t == WT_BUZZ_CONTROLLER)
						{
							ret = sizeof(buzz_hid_report_descriptor);
							memcpy(data, buzz_hid_report_descriptor, ret);
						}
						else if (t == WT_GAMETRAK_CONTROLLER)
						{
							ret = sizeof(gametrak_hid_report_descriptor);
							memcpy(data, gametrak_hid_report_descriptor, ret);
						}
						else if (t >= WT_REALPLAY_RACING && t <= WT_REALPLAY_POOL)
						{
							ret = sizeof(realplay_hid_report_descriptor);
							memcpy(data, realplay_hid_report_descriptor, ret);
						}
						p->actual_length = ret;
						break;
					default:
						goto fail;
				}
				break;
			/* hid specific requests */
			case SET_REPORT:
				if (t == WT_GAMETRAK_CONTROLLER)
				{
					const char secret[] = "Gametrak";
					if (length == 8 && memcmp(data, secret, 8) == 0)
					{
						s->gametrak_state = 0;
						s->gametrak_key = 0;
					}
					else if (length == 2)
					{
						if (data[0] == 0x45)
						{
							s->gametrak_key = data[1] << 16;
						}
						if ((s->gametrak_key >> 16) == data[1])
						{
							gametrak_compute_key(&s->gametrak_key);
						}
						else
						{
							fprintf(stderr, "gametrak error : own key = %02x, recv key = %02x\n", s->gametrak_key >> 16, data[1]);
						}
					}
				}
				// no idea, Rock Band 2 keeps spamming this
				if (length > 0)
				{
					/* 0x01: Num Lock LED
			 * 0x02: Caps Lock LED
			 * 0x04: Scroll Lock LED
			 * 0x08: Compose LED
			 * 0x10: Kana LED */
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
		PADState* s = (PADState*)dev;
		delete s;
	}

	int pad_open(USBDevice* dev)
	{
		PADState* s = (PADState*)dev;
		if (s)
			return s->pad->Open();
		return 1;
	}

	void pad_close(USBDevice* dev)
	{
		PADState* s = (PADState*)dev;
		if (s)
			s->pad->Close();
	}

	void pad_reset_data(generic_data_t* d)
	{
		memset(d, 0, sizeof(generic_data_t));
		d->axis_x = 0x3FF >> 1;
		d->axis_y = 0xFF;
		d->axis_z = 0xFF;
		d->axis_rz = 0xFF;
	}

	void pad_reset_data(dfp_data_t* d)
	{
		memset(d, 0, sizeof(dfp_data_t));
		d->axis_x = 0x3FFF >> 1;
		//d->axis_y = 0xFF;
		d->axis_z = 0x3F;
		d->axis_rz = 0x3F;
	}

	void pad_copy_data(PS2WheelTypes type, uint8_t* buf, wheel_data_t& data)
	{
#if 1
		struct wheel_data_t
		{
			uint32_t lo;
			uint32_t hi;
		};

		wheel_data_t* w = (wheel_data_t*)buf;
		memset(w, 0, 8);

		switch (type)
		{
			case WT_GENERIC:
				w->lo = data.steering & 0x3FF;
				w->lo |= (data.buttons & 0xFFF) << 10;
				w->lo |= 0xFF << 24;

				w->hi = (data.hatswitch & 0xF);
				w->hi |= (data.throttle & 0xFF) << 8;
				w->hi |= (data.brake & 0xFF) << 16;

				break;

			case WT_GT_FORCE:

				w->lo = data.steering & 0x3FF;
				w->lo |= (data.buttons & 0xFFF) << 10;
				w->lo |= 0xFF << 24;

				w->hi = (data.throttle & 0xFF);
				w->hi |= (data.brake & 0xFF) << 8;

				break;
			case WT_DRIVING_FORCE_PRO:

				w->lo = data.steering & 0x3FFF;
				w->lo |= (data.buttons & 0x3FFF) << 14;
				w->lo |= (data.hatswitch & 0xF) << 28;

				w->hi = 0x00;
				w->hi |= data.throttle << 8;
				w->hi |= data.brake << 16; //axis_rz
				w->hi |= 0x11 << 24;       //enables wheel and pedals?

				//PrintBits(w, sizeof(*w));

				break;
			case WT_DRIVING_FORCE_PRO_1102:

				// what's up with the bitmap?
				// xxxxxxxx xxxxxxbb bbbbbbbb bbbbhhhh ???????? ?01zzzzz 1rrrrrr1 10001000
				w->lo = data.steering & 0x3FFF;
				w->lo |= (data.buttons & 0x3FFF) << 14;
				w->lo |= (data.hatswitch & 0xF) << 28;

				w->hi = 0x00;
				//w->hi |= 0 << 9; //bit 9 must be 0
				w->hi |= (1 | (data.throttle * 0x3F) / 0xFF) << 10;          //axis_z
				w->hi |= 1 << 16;                                            //bit 16 must be 1
				w->hi |= ((0x3F - (data.brake * 0x3F) / 0xFF) & 0x3F) << 17; //axis_rz
				w->hi |= 1 << 23;                                            //bit 23 must be 1
				w->hi |= 0x11 << 24;                                         //enables wheel and pedals?

				//PrintBits(w, sizeof(*w));

				break;
			case WT_ROCKBAND1_DRUMKIT:
				w->lo = (data.buttons & 0xFFF);
				w->lo |= (data.hatswitch & 0xF) << 16;
				break;

			case WT_BUZZ_CONTROLLER:
				// https://gist.github.com/Lewiscowles1986/eef220dac6f0549e4702393a7b9351f6
				buf[0] = 0x7f;
				buf[1] = 0x7f;
				buf[2] = data.buttons & 0xff;
				buf[3] = (data.buttons >> 8) & 0xff;
				buf[4] = 0xf0 | ((data.buttons >> 16) & 0xf);
				break;

			case WT_GAMETRAK_CONTROLLER:
				memset(buf, 0, 16);
				buf[0] = data.clutch & 0xfe;
				buf[1] = data.clutch >> 8;
				buf[2] = data.throttle & 0xfe;
				buf[3] = data.throttle >> 8;
				buf[4] = data.brake & 0xfe;
				buf[5] = data.brake >> 8;
				buf[6] = data.hatswitch & 0xfe;
				buf[7] = data.hatswitch >> 8;
				buf[8] = data.hat_horz & 0xfe;
				buf[9] = data.hat_horz >> 8;
				buf[10] = data.hat_vert & 0xfe;
				buf[11] = data.hat_vert >> 8;
				buf[12] = data.buttons;
				break;

			case WT_REALPLAY_RACING:
			case WT_REALPLAY_SPHERE:
			case WT_REALPLAY_GOLF:
			case WT_REALPLAY_POOL:
				memset(buf, 0, 19);
				buf[0] = data.clutch & 0xfe;
				buf[1] = data.clutch >> 8;
				buf[2] = data.throttle & 0xff;
				buf[3] = data.throttle >> 8;
				buf[4] = data.brake & 0xff;
				buf[5] = data.brake >> 8;
				buf[14] = data.buttons;
				break;

			case WT_SEGA_SEAMIC:
				buf[0] = data.steering & 0xFF;
				buf[1] = data.throttle & 0xFF;
				buf[2] = data.brake & 0xFF;
				buf[3] = data.hatswitch & 0x0F;       // 4bits?
				buf[3] |= (data.buttons & 0x0F) << 4; // 4 bits // TODO Or does it start at buf[4]?
				buf[4] = (data.buttons >> 4) & 0x3F;  // 10 - 4 = 6 bits
				break;

			case WT_KEYBOARDMANIA_CONTROLLER:
				buf[0] = 0x3F;
				buf[1] = data.buttons & 0xFF;
				buf[2] = (data.buttons >> 8) & 0xFF;
				buf[3] = (data.buttons >> 16) & 0xFF;
				buf[4] = (data.buttons >> 24) & 0xFF;
				break;

			default:
				break;
		}

#else

		u_wheel_data_t* w = (u_wheel_data_t*)buf;

		//Console.Warning("usb-pad: axis x %d\n", data.axis_x);
		switch (type)
		{
			case WT_GENERIC:
				memset(&w->u.generic_data, 0xff, sizeof(generic_data_t));
				//pad_reset_data(&w->u.generic_data);

				w->u.generic_data.buttons = data.buttons;
				w->u.generic_data.hatswitch = data.hatswitch;
				w->u.generic_data.axis_x = data.steering;
				w->u.generic_data.axis_y = 0xFF; //data.clutch;
				w->u.generic_data.axis_z = data.throttle;
				w->u.generic_data.axis_rz = data.brake;

				break;

			case WT_DRIVING_FORCE_PRO:
				//memset(&w->u.dfp_data, 0, sizeof(dfp_data_t));
				//pad_reset_data(&w->u.dfp_data);

				w->u.dfp_data.buttons = data.buttons;
				w->u.dfp_data.hatswitch = data.hatswitch;
				w->u.dfp_data.axis_x = data.steering;
				w->u.dfp_data.axis_z = data.throttle;
				w->u.dfp_data.axis_rz = data.brake;

				w->u.dfp_data.magic1 = 1;
				w->u.dfp_data.magic2 = 1;
				w->u.dfp_data.magic3 = 1;
				w->u.dfp_data.magic4 =
					1 << 0 | //enable pedals?
					0 << 1 |
					0 << 2 |
					0 << 3 |
					1 << 4 | //enable wheel?
					0 << 5 |
					0 << 6 |
					0 << 7;

				PrintBits(&w->u.dfp_data, sizeof(dfp_data_t));

				break;

			case WT_DRIVING_FORCE_PRO_1102:
				//memset(&w->u.dfp_data, 0, sizeof(dfp_data_t));
				//pad_reset_data(&w->u.dfp_data);

				w->u.dfp_data.buttons = data.buttons;
				w->u.dfp_data.hatswitch = data.hatswitch;
				w->u.dfp_data.axis_x = data.steering;
				w->u.dfp_data.axis_z = 1 | (data.throttle * 0x3F) / 0xFF; //TODO Always > 0 or everything stops working, wut.
				w->u.dfp_data.axis_rz = 0x3F - (data.brake * 0x3F) / 0xFF;

				w->u.dfp_data.magic1 = 1;
				w->u.dfp_data.magic2 = 1;
				w->u.dfp_data.magic3 = 1;
				w->u.dfp_data.magic4 =
					1 << 0 | //enable pedals?
					0 << 1 |
					0 << 2 |
					0 << 3 |
					1 << 4 | //enable wheel?
					0 << 5 |
					0 << 6 |
					0 << 7;

				PrintBits(&w->u.dfp_data, sizeof(dfp_data_t));

				break;

			case WT_GT_FORCE:
				memset(&w->u.gtf_data, 0xff, sizeof(gtforce_data_t));

				w->u.gtf_data.buttons = data.buttons;
				w->u.gtf_data.axis_x = data.steering;
				w->u.gtf_data.axis_y = 0xFF; //data.clutch;
				w->u.gtf_data.axis_z = data.throttle;
				w->u.gtf_data.axis_rz = data.brake;

				break;

			case WT_ROCKBAND1_DRUMKIT:
				memset(&w->u.rb1dk_data, 0x0, sizeof(rb1drumkit_t));

				w->u.rb1dk_data.u.buttons = data.buttons;
				w->u.rb1dk_data.hatswitch = data.hatswitch;

				break;

			default:
				break;
		}

#endif
	}

	static void pad_init(PADState* s, int port, Pad* pad)
	{
		s->f.dev_subtype = pad->Type();
		s->pad = pad;
		s->port = port;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.open = pad_open;
		s->dev.klass.close = pad_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset((USBDevice*)s);
	}

	USBDevice* PadDevice::CreateDevice(int port)
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
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type((PS2WheelTypes)GetSelectedSubtype(std::make_pair(port, TypeName())));
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = df_desc_strings;

		const uint8_t* dev_desc = df_dev_descriptor;
		int dev_desc_len = sizeof(df_dev_descriptor);
		const uint8_t* config_desc = df_config_descriptor;
		int config_desc_len = sizeof(df_config_descriptor);

		switch (pad->Type())
		{
			case WT_DRIVING_FORCE_PRO:
			{
				dev_desc = dfp_dev_descriptor;
				dev_desc_len = sizeof(dfp_dev_descriptor);
				config_desc = dfp_config_descriptor;
				config_desc_len = sizeof(dfp_config_descriptor);
				s->desc.str = dfp_desc_strings;
			}
			break;
			case WT_DRIVING_FORCE_PRO_1102:
			{
				dev_desc = dfp_dev_descriptor_1102;
				dev_desc_len = sizeof(dfp_dev_descriptor_1102);
				config_desc = dfp_config_descriptor;
				config_desc_len = sizeof(dfp_config_descriptor);
				s->desc.str = dfp_desc_strings;
			}
			break;
			case WT_GT_FORCE:
			{
				dev_desc = gtf_dev_descriptor;
				dev_desc_len = sizeof(gtf_dev_descriptor);
				config_desc = gtforce_config_descriptor; //TODO
				config_desc_len = sizeof(gtforce_config_descriptor);
				s->desc.str = gtf_desc_strings;
			}
			default:
				break;
		}

		if (usb_desc_parse_dev(dev_desc, dev_desc_len, s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_desc, config_desc_len, s->desc_dev) < 0)
			goto fail;

		pad_init(s, port, pad);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int PadDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int PadDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		PADState* s = (PADState*)dev;

		if (!s)
			return 0;
		switch (mode)
		{
			case FreezeAction::Load:
				s->f = *(PADState::freeze*)data;
				s->pad->Type((PS2WheelTypes)s->f.dev_subtype);
				return sizeof(PADState::freeze);
			case FreezeAction::Save:
				*(PADState::freeze*)data = s->f;
				return sizeof(PADState::freeze);
			case FreezeAction::Size:
				return sizeof(PADState::freeze);
			default:
				break;
		}
		return 0;
	}

	// ---- Rock Band drum kit ----

	USBDevice* RBDrumKitDevice::CreateDevice(int port)
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
			Console.WriteLn("RBDK: Invalid input API.\n");
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type(WT_ROCKBAND1_DRUMKIT);
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = rb1_desc_strings;

		if (usb_desc_parse_dev(rb1_dev_descriptor, sizeof(rb1_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(rb1_config_descriptor, sizeof(rb1_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s, port, pad);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int RBDrumKitDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int RBDrumKitDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return PadDevice::Freeze(mode, dev, data);
	}

	// ---- Buzz ----

	USBDevice* BuzzDevice::CreateDevice(int port)
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
			Console.WriteLn("Buzz: Invalid input API.\n");
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type(WT_BUZZ_CONTROLLER);
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = buzz_desc_strings;

		if (usb_desc_parse_dev(buzz_dev_descriptor, sizeof(buzz_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(buzz_config_descriptor, sizeof(buzz_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s, port, pad);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int BuzzDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int BuzzDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return PadDevice::Freeze(mode, dev, data);
	}

	// ---- Keyboardmania ----

	USBDevice* KeyboardmaniaDevice::CreateDevice(int port)
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
			Console.WriteLn("usb-pad: %s: Invalid input API.", TypeName());
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type(WT_KEYBOARDMANIA_CONTROLLER);
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = kbm_desc_strings;

		if (usb_desc_parse_dev(kbm_dev_descriptor, sizeof(kbm_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(kbm_config_descriptor, sizeof(kbm_config_descriptor), s->desc_dev) < 0)
			goto fail;

		pad_init(s, port, pad);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int KeyboardmaniaDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int KeyboardmaniaDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return PadDevice::Freeze(mode, dev, data);
	}

	// ---- Gametrak ----

	USBDevice* GametrakDevice::CreateDevice(int port)
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
			Console.WriteLn("Gametrak: Invalid input API.");
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type(WT_GAMETRAK_CONTROLLER);
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = gametrak_desc_strings;

		if (usb_desc_parse_dev(gametrak_dev_descriptor, sizeof(gametrak_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(gametrak_config_descriptor, sizeof(gametrak_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->f.dev_subtype = pad->Type();
		s->pad = pad;
		s->port = port;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.open = pad_open;
		s->dev.klass.close = pad_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset((USBDevice*)s);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int GametrakDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int GametrakDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return PadDevice::Freeze(mode, dev, data);
	}

	// ---- RealPlay ----

	USBDevice* RealPlayDevice::CreateDevice(int port)
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
			Console.WriteLn("RealPlay: Invalid input API");
			return nullptr;
		}

		Pad* pad = proxy->CreateObject(port, TypeName());

		if (!pad)
			return nullptr;

		pad->Type((PS2WheelTypes)(WT_REALPLAY_RACING + GetSelectedSubtype(std::make_pair(port, TypeName()))));
		PADState* s = new PADState();

		s->desc.full = &s->desc_dev;
		s->desc.str = realplay_desc_strings;

		if (pad->Type() == WT_REALPLAY_RACING && usb_desc_parse_dev(realplay_racing_dev_descriptor, sizeof(realplay_racing_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (pad->Type() == WT_REALPLAY_SPHERE && usb_desc_parse_dev(realplay_sphere_dev_descriptor, sizeof(realplay_sphere_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (pad->Type() == WT_REALPLAY_GOLF && usb_desc_parse_dev(realplay_golf_dev_descriptor, sizeof(realplay_golf_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (pad->Type() == WT_REALPLAY_POOL && usb_desc_parse_dev(realplay_pool_dev_descriptor, sizeof(realplay_pool_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(realplay_config_descriptor, sizeof(realplay_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->f.dev_subtype = pad->Type();
		s->pad = pad;
		s->port = port;
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.open = pad_open;
		s->dev.klass.close = pad_close;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[1];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset((USBDevice*)s);

		return (USBDevice*)s;

	fail:
		pad_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int RealPlayDevice::Configure(int port, const std::string& api, void* data)
	{
		auto proxy = RegisterPad::instance().Proxy(api);
		if (proxy)
			return proxy->Configure(port, TypeName(), data);
		return RESULT_CANCELED;
	}

	int RealPlayDevice::Freeze(FreezeAction mode, USBDevice* dev, void* data)
	{
		return PadDevice::Freeze(mode, dev, data);
	}

} // namespace usb_pad
