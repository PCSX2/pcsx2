// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "usb-seamic.h"
#include "Host.h"
#include "IconsPromptFont.h"
#include "USB/qemu-usb/desc.h"
#include "USB/usb-mic/usb-mic.h"
#include "USB/USB.h"

#include "common/Console.h"

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
	
	SeamicState::SeamicState(u32 port_)
		: port(port_)
	{
	}

	SeamicState::~SeamicState() = default;
	
	void SeamicState::UpdateStick() noexcept
	{
		if (stick_l > 0)
			data.stick_x = static_cast<u8>(std::max<int>(0x7f - stick_l, 0));
		else if (stick_r > 0)
			data.stick_x = static_cast<u8>(std::min<int>(0x7f + stick_r, 0xff));
		else
			data.stick_x = 0x7f;

		if (stick_u > 0)
			data.stick_y = static_cast<u8>(std::max<int>(0x7f - stick_u, 0));
		else if (stick_d> 0)
			data.stick_y = static_cast<u8>(std::min<int>(0x7f + stick_d, 0xff));
		else
			data.stick_y = 0x7f;
	}

	// TODO: de-duplicate these methods
	u8 SeamicState::UpdateHatSwitch() noexcept
	{
		if (hat_up && hat_right)
			return 1;
		else if (hat_right && hat_down)
			return 3;
		else if (hat_down && hat_left)
			return 5;
		else if (hat_left && hat_up)
			return 7;
		else if (hat_up)
			return 0;
		else if (hat_right)
			return 2;
		else if (hat_down)
			return 4;
		else if (hat_left)
			return 6;
		else
			return 8;
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
						std::memcpy(data, hid_report_descriptor, ret);
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

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		SeamicState* s = USB_CONTAINER_OF(dev, SeamicState, dev);
		uint8_t devep = p->ep->nr;

		switch (p->pid)
		{
			case USB_TOKEN_IN:
				if (devep == 1)
				{
					s->mic->klass.handle_data(s->mic, p);
				}
				else if (devep == 2)
				{
					s->data.dpad = s->UpdateHatSwitch();
					usb_packet_copy(p, &s->data, sizeof(s->data));
				}
				else
				{
					goto fail;
				}
				break;
			case USB_TOKEN_OUT:
				break;
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_reset(USBDevice* dev)
	{
		SeamicState* s = USB_CONTAINER_OF(dev, SeamicState, dev);
		s->mic->klass.handle_reset(s->mic);
	}

	static void pad_unrealize(USBDevice* dev)
	{
		SeamicState* s = USB_CONTAINER_OF(dev, SeamicState, dev);
		s->mic->klass.unrealize(s->mic);
		delete s;
	}

	const char* SeamicDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Sega Seamic");
	}

	const char* SeamicDevice::TypeName() const
	{
		return "seamic";
	}

	const char* SeamicDevice::IconName() const
	{
		return ICON_PF_SEGA_SEAMIC;
	}

	USBDevice* SeamicDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		const usb_mic::MicrophoneDevice* mic_proxy =
			static_cast<usb_mic::MicrophoneDevice*>(RegisterDevice::instance().Device(DEVTYPE_MICROPHONE));
		if (!mic_proxy)
			return nullptr;

		USBDevice* mic = mic_proxy->CreateDevice(si, port, 0, false, 48000, TypeName());
		if (!mic)
			return nullptr;

		SeamicState* s = new SeamicState(port);

		s->mic = mic;
		s->desc.full = &s->desc_dev;
		s->desc.str = desc_strings;

		if (usb_desc_parse_dev(dev_descriptor, sizeof(dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(config_descriptor, sizeof(config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_unrealize;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2]; //not really used

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset(&s->dev);
		return &s->dev;

	fail:
		pad_unrealize(&s->dev);
		return nullptr;
	}

	std::span<const InputBindingInfo> SeamicDevice::Bindings(u32 subtype) const
	{
		static constexpr const InputBindingInfo bindings[] = {
			{"StickLeft", TRANSLATE_NOOP("USB", "Stick Left"), nullptr, InputBindingInfo::Type::HalfAxis, STICK_LEFT, GenericInputBinding::LeftStickLeft},
			{"StickRight", TRANSLATE_NOOP("USB", "Stick Right"), nullptr, InputBindingInfo::Type::HalfAxis, STICK_RIGHT, GenericInputBinding::LeftStickRight},
			{"StickUp", TRANSLATE_NOOP("USB", "Stick Up"), nullptr, InputBindingInfo::Type::HalfAxis, STICK_UP, GenericInputBinding::LeftStickUp},
			{"StickDown", TRANSLATE_NOOP("USB", "Stick Down"), nullptr, InputBindingInfo::Type::HalfAxis, STICK_DOWN, GenericInputBinding::LeftStickDown},
			{"A", TRANSLATE_NOOP("USB", "A"), nullptr, InputBindingInfo::Type::Button, BTN_A, GenericInputBinding::Cross},
			{"B", TRANSLATE_NOOP("USB", "B"), nullptr, InputBindingInfo::Type::Button, BTN_B, GenericInputBinding::Circle},
			{"C", TRANSLATE_NOOP("USB", "C"), nullptr, InputBindingInfo::Type::Button, BTN_C, GenericInputBinding::R2},
			{"X", TRANSLATE_NOOP("USB", "X"), nullptr, InputBindingInfo::Type::Button, BTN_X, GenericInputBinding::Square},
			{"Y", TRANSLATE_NOOP("USB", "Y"), nullptr, InputBindingInfo::Type::Button, BTN_Y, GenericInputBinding::Triangle},
			{"Z", TRANSLATE_NOOP("USB", "Z"), nullptr, InputBindingInfo::Type::Button, BTN_Z, GenericInputBinding::L2},
			{"L", TRANSLATE_NOOP("USB", "L"), nullptr, InputBindingInfo::Type::Button, BTN_L, GenericInputBinding::L1},
			{"R", TRANSLATE_NOOP("USB", "R"), nullptr, InputBindingInfo::Type::Button, BTN_R, GenericInputBinding::R1},
			{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, SELECT, GenericInputBinding::Select},
			{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, START, GenericInputBinding::Start},
			{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, DPAD_UP, GenericInputBinding::DPadUp},
			{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, DPAD_DOWN, GenericInputBinding::DPadDown},
			{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, DPAD_LEFT, GenericInputBinding::DPadLeft},
			{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, DPAD_RIGHT, GenericInputBinding::DPadRight},
		};
		return bindings;
	}

	std::span<const SettingInfo> SeamicDevice::Settings(u32 subtype) const
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
	
	float SeamicDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		SeamicState* s = USB_CONTAINER_OF(dev, SeamicState, dev);
		switch (bind_index)
		{
			case STICK_UP:    return (static_cast<float>(s->stick_u) / 255.0f);
			case STICK_DOWN:  return (static_cast<float>(s->stick_d) / 255.0f);
			case STICK_LEFT:  return (static_cast<float>(s->stick_l) / 255.0f);
			case STICK_RIGHT: return (static_cast<float>(s->stick_r) / 255.0f);
			case BTN_A: return s->data.btn_a;
			case BTN_B: return s->data.btn_b;
			case BTN_C: return s->data.btn_c;
			case BTN_X: return s->data.btn_x;
			case BTN_Y: return s->data.btn_y;
			case BTN_Z: return s->data.btn_z;
			case BTN_L: return s->data.btn_l;
			case BTN_R: return s->data.btn_r;
			case SELECT: return s->data.select;
			case START:  return s->data.start;
			case DPAD_UP:    return s->hat_up;
			case DPAD_DOWN:  return s->hat_down;
			case DPAD_LEFT:  return s->hat_left;
			case DPAD_RIGHT: return s->hat_right;
			default:
				return 0.0f;
		}
	}

	void SeamicDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		SeamicState* s = USB_CONTAINER_OF(dev, SeamicState, dev);
		switch (bind_index)
		{
			case STICK_UP:
				s->stick_u = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case STICK_DOWN:
				s->stick_d = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case STICK_LEFT:
				s->stick_l = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case STICK_RIGHT:
				s->stick_r = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				s->UpdateStick();
				break;
			case BTN_A: s->data.btn_a = (value >= 0.5f); break;
			case BTN_B: s->data.btn_b = (value >= 0.5f); break;
			case BTN_C: s->data.btn_c = (value >= 0.5f); break;
			case BTN_X: s->data.btn_x = (value >= 0.5f); break;
			case BTN_Y: s->data.btn_y = (value >= 0.5f); break;
			case BTN_Z: s->data.btn_z = (value >= 0.5f); break;
			case BTN_L: s->data.btn_l = (value >= 0.5f); break;
			case BTN_R: s->data.btn_r = (value >= 0.5f); break;
			case SELECT: s->data.select = (value >= 0.5f); break;
			case START:  s->data.start = (value >= 0.5f); break;
			case DPAD_UP:    s->hat_up    = (value >= 0.5f); break;
			case DPAD_DOWN:  s->hat_down  = (value >= 0.5f); break;
			case DPAD_LEFT:  s->hat_left  = (value >= 0.5f); break;
			case DPAD_RIGHT: s->hat_right = (value >= 0.5f); break;
		}
	}

	bool SeamicDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		Console.Warning("Not implemented!");
		return true;
	}
} // namespace usb_pad
