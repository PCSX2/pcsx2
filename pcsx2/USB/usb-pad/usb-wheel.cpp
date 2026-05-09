// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "usb-wheel.h"
#include "IconsFontAwesome.h"
#include "IconsPromptFont.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/USB.h"
#include "Host.h"
#include "StateWrapper.h"

#include "common/Console.h"

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
		"Logitech", //actual index @ 0x04
		"Logitech GT Force" //actual index @ 0x20
	};

	static int MapSteeringCurveExponentOptionToExponent(const std::string& option)
	{
		int exponent = 0;
		if (option == "Low")
		{
			exponent = 1;
		}
		else if (option == "Medium")
		{
			exponent = 2;
		}
		else if (option == "High")
		{
			exponent = 3;
		}

		return exponent;
	}

	WheelState::WheelState(u32 port_, WheelType type_)
		: port(port_)
		, type(type_)
	{
		if (type_ == WT_DRIVING_FORCE_PRO)
			steering_range = 0x3FFF >> 1;
		else
			steering_range = 0x3FF >> 1;

		steering_step = std::numeric_limits<u16>::max();

		// steering starts in the center
		data.last_steering = steering_range;
		data.steering = steering_range;

		// throttle/brake start unpressed
		data.throttle = 255;
		data.brake = 255;

		Reset();
	}

	WheelState::~WheelState() = default;

	void WheelState::UpdateSettings(SettingsInterface& si, const char* devname)
	{
		const s32 smoothing_percent = USB::GetConfigInt(si, port, devname, "SteeringSmoothing", 0);
		if (smoothing_percent <= 0)
		{
			// none, allow any amount of change
			steering_step = std::numeric_limits<u16>::max();
		}
		else
		{
			steering_step = static_cast<u16>(std::clamp<s32>((steering_range * smoothing_percent) / 100,
				1, std::numeric_limits<u16>::max()));
		}

		steering_deadzone = (steering_range * USB::GetConfigInt(si, port, devname, "SteeringDeadzone", 0)) / 100;
		steering_curve_exponent = MapSteeringCurveExponentOptionToExponent(USB::GetConfigString(si, port, devname, "SteeringCurveExponent", "Off"));

		const std::string ffdevname(USB::GetConfigString(si, port, devname, "FFDevice"));
		if (ffdevname != mFFdevName)
		{
			mFFdev.reset();
			mFFdevName = std::move(ffdevname);
			OpenFFDevice();
		}
		if (mFFdev != NULL)
		{
			const bool use_ffb_dropout_workaround = USB::GetConfigBool(si, port, devname, "FfbDropoutWorkaround", false);
			mFFdev->use_ffb_dropout_workaround = use_ffb_dropout_workaround;
		}
	}

	void WheelState::Reset()
	{
		data.steering = steering_range;
		mFFstate = {};
	}

	float WheelState::GetBindValue(u32 bind_index) const
	{
		switch (bind_index)
		{
			case CID_STEERING_L:
				return static_cast<float>(data.steering_left) / static_cast<float>(steering_range);

			case CID_STEERING_R:
				return static_cast<float>(data.steering_right) / static_cast<float>(steering_range);

			case CID_THROTTLE:
				return 1.0f - (static_cast<float>(data.throttle) / 255.0f);

			case CID_BRAKE:
				return 1.0f - (static_cast<float>(data.brake) / 255.0f);

			case CID_DPAD_UP:
				return static_cast<float>(data.hat_up);

			case CID_DPAD_DOWN:
				return static_cast<float>(data.hat_down);

			case CID_DPAD_LEFT:
				return static_cast<float>(data.hat_left);

			case CID_DPAD_RIGHT:
				return static_cast<float>(data.hat_right);

			case CID_CROSS:
			case CID_SQUARE:
			case CID_CIRCLE:
			case CID_TRIANGLE:
			case CID_L1:
			case CID_R1:
			case CID_L2:
			case CID_R2:
			case CID_SELECT:
			case CID_START:
			case CID_L3:
			case CID_R3:
			{
				const u32 mask = (1u << (bind_index - CID_CROSS));
				return ((data.buttons & mask) != 0u) ? 1.0f : 0.0f;
			}

			default:
				return 0.0f;
		}
	}

	s16 WheelState::ApplySteeringAxisModifiers(float value)
	{
		const s16 raw_steering = static_cast<s16>(std::lroundf(value * static_cast<float>(steering_range)));
		const s16 deadzone_offset = static_cast<s16>(std::lroundf(value * static_cast<float>(steering_deadzone)));
		const s16 deadzone_modified_steering = std::max((raw_steering - steering_deadzone + deadzone_offset), 0);
		
		if (steering_curve_exponent)
		{
			return std::pow(deadzone_modified_steering, steering_curve_exponent + 1) / std::pow(steering_range, steering_curve_exponent);
		}
		else
		{
			return deadzone_modified_steering;
		}
	}

	void WheelState::SetBindValue(u32 bind_index, float value)
	{
		switch (bind_index)
		{
			case CID_STEERING_L:
				data.steering_left = ApplySteeringAxisModifiers(value);
				UpdateSteering();
				break;

			case CID_STEERING_R:
				data.steering_right = ApplySteeringAxisModifiers(value);
				UpdateSteering();
				break;

			case CID_THROTTLE:
				data.throttle = static_cast<u32>(255 - std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_BRAKE:
				data.brake = static_cast<u32>(255 - std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				break;

			case CID_DPAD_UP:
				data.hat_up = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_DOWN:
				data.hat_down = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_LEFT:
				data.hat_left = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;
			case CID_DPAD_RIGHT:
				data.hat_right = static_cast<u8>(std::clamp<long>(std::lroundf(value * 255.0f), 0, 255));
				UpdateHatSwitch();
				break;

			case CID_CROSS:
			case CID_SQUARE:
			case CID_CIRCLE:
			case CID_TRIANGLE:
			case CID_L1:
			case CID_R1:
			case CID_L2:
			case CID_R2:
			case CID_SELECT:
			case CID_START:
			case CID_L3:
			case CID_R3:
			{
				const u32 mask = (1u << (bind_index - CID_CROSS));
				if (value >= 0.5f)
					data.buttons |= mask;
				else
					data.buttons &= ~mask;
			}
			break;

			default:
				break;
		}
	}

	void WheelState::UpdateSteering()
	{
		u16 value;
		if (data.steering_left > 0)
			value = static_cast<u16>(std::max<int>(steering_range - data.steering_left, 0));
		else
			value = static_cast<u16>(std::min<int>(steering_range + data.steering_right, steering_range * 2));

		// TODO: Smoothing, don't jump too much
		//data.steering = value;
		if (value < data.steering)
			data.steering -= std::min<u16>(data.steering - value, steering_step);
		else if (value > data.steering)
			data.steering += std::min<u16>(value - data.steering, steering_step);
	}

	void WheelState::UpdateHatSwitch()
	{
		if (data.hat_up && data.hat_right)
			data.hatswitch = 1;
		else if (data.hat_right && data.hat_down)
			data.hatswitch = 3;
		else if (data.hat_down && data.hat_left)
			data.hatswitch = 5;
		else if (data.hat_left && data.hat_up)
			data.hatswitch = 7;
		else if (data.hat_up)
			data.hatswitch = 0;
		else if (data.hat_right)
			data.hatswitch = 2;
		else if (data.hat_down)
			data.hatswitch = 4;
		else if (data.hat_left)
			data.hatswitch = 6;
		else
			data.hatswitch = 8;
	}

	void WheelState::OpenFFDevice()
	{
		if (mFFdevName.empty())
			return;

		mFFdev.reset();
		mFFdev = CreateSDLFFDevice(mFFdevName);
	}

	void WheelState::ParseFFData(const ff_data* ffdata, bool isDFP)
	{
		if (mFFdev)
			ProcessLogitechFFPacket(*mFFdev, ffdata, isDFP, mFFstate);
	}

	static void pad_handle_data(USBDevice* dev, USBPacket* p)
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_IN:
			{
				if (p->ep->nr != 1)
					goto fail;
				
				s->UpdateSteering();
				s->UpdateHatSwitch();
				switch (s->type)
				{
					case WT_GT_FORCE:
					{
						s->gtf.steering = s->data.steering;
						s->gtf.l1 = s->GetBindValue(CID_L1);
						s->gtf.r1 = s->GetBindValue(CID_R1);
						s->gtf.x  = s->GetBindValue(CID_SQUARE);
						s->gtf.y  = s->GetBindValue(CID_TRIANGLE);
						s->gtf.a  = s->GetBindValue(CID_CROSS);
						s->gtf.b  = s->GetBindValue(CID_CIRCLE);
						s->gtf.pedals_attached = 1;
						s->gtf.throttle = s->data.throttle;
						s->gtf.brake = s->data.brake;
						usb_packet_copy(p, &s->gtf, sizeof(s->gtf));
						break;
					}

					case WT_DRIVING_FORCE:
					{
						s->df.steering = s->data.steering;
						s->df.cross    = s->GetBindValue(CID_CROSS);
						s->df.square   = s->GetBindValue(CID_SQUARE);
						s->df.circle   = s->GetBindValue(CID_CIRCLE);
						s->df.triangle = s->GetBindValue(CID_TRIANGLE);
						s->df.r1       = s->GetBindValue(CID_R1);
						s->df.l1       = s->GetBindValue(CID_L1);
						s->df.r2       = s->GetBindValue(CID_R2);
						s->df.l2       = s->GetBindValue(CID_L2);
						s->df.select   = s->GetBindValue(CID_SELECT);
						s->df.start    = s->GetBindValue(CID_START);
						s->df.r3       = s->GetBindValue(CID_R3);
						s->df.l3       = s->GetBindValue(CID_L3);
						s->df.dpad = s->data.hatswitch;
						s->df.throttle = s->data.throttle;
						s->df.brake = s->data.brake;
						usb_packet_copy(p, &s->df, sizeof(s->df));
						break;
					}

					case WT_DRIVING_FORCE_PRO:
					{
						s->dfp.steering = s->data.steering;
						s->dfp.cross    = s->GetBindValue(CID_CROSS);
						s->dfp.square   = s->GetBindValue(CID_SQUARE);
						s->dfp.circle   = s->GetBindValue(CID_CIRCLE);
						s->dfp.triangle = s->GetBindValue(CID_TRIANGLE);
						s->dfp.r1       = s->GetBindValue(CID_R1);
						s->dfp.l1       = s->GetBindValue(CID_L1);
						s->dfp.r2       = s->GetBindValue(CID_R2);
						s->dfp.l2       = s->GetBindValue(CID_L2);
						s->dfp.select   = s->GetBindValue(CID_SELECT);
						s->dfp.start    = s->GetBindValue(CID_START);
						s->dfp.r3       = s->GetBindValue(CID_R3);
						s->dfp.l3       = s->GetBindValue(CID_L3);
						s->dfp.r3_2     = s->GetBindValue(CID_R3);
						s->dfp.l3_2     = s->GetBindValue(CID_L3);
						s->dfp.dpad = s->data.hatswitch;
						s->dfp.brake_throttle = 0x7f;
						s->dfp.throttle = s->data.throttle;
						s->dfp.brake = s->data.brake;
						s->dfp.pedals_attached = 1;
						s->dfp.powered = 1;
						s->dfp.self_check_done = 1;
						s->dfp.set1 = 1;
						usb_packet_copy(p, &s->dfp, sizeof(s->dfp));
						break;
					}
				}
				break;
			}
			case USB_TOKEN_OUT: 
			{
				const ff_data* ffdata = reinterpret_cast<const ff_data*>(p->buffer_ptr);
				const bool hires = (s->type == WT_DRIVING_FORCE_PRO);
				s->ParseFFData(ffdata, hires);
				break;
			}
			default:
			fail:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void pad_handle_reset(USBDevice* dev)
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
		s->Reset();
	}

	static void pad_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
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
						if (s->type == WT_GT_FORCE)
						{
							ret = sizeof(pad_gtforce_hid_report_descriptor);
							memcpy(data, pad_gtforce_hid_report_descriptor, ret);
						}
						else if (s->type == WT_DRIVING_FORCE)
						{
							ret = sizeof(pad_driving_force_hid_separate_report_descriptor);
							memcpy(data, pad_driving_force_hid_separate_report_descriptor, ret);
						}
						else if (s->type == WT_DRIVING_FORCE_PRO)
						{
							ret = sizeof(pad_driving_force_pro_hid_report_descriptor);
							memcpy(data, pad_driving_force_pro_hid_report_descriptor, ret);
						}
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

	static void pad_handle_destroy(USBDevice* dev)
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
		delete s;
	}

	static void pad_init(WheelState* s)
	{
		s->dev.speed = USB_SPEED_FULL;
		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = pad_handle_reset;
		s->dev.klass.handle_control = pad_handle_control;
		s->dev.klass.handle_data = pad_handle_data;
		s->dev.klass.unrealize = pad_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = nullptr;

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		pad_handle_reset(&s->dev);
	}

	USBDevice* WheelDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		if (subtype >= WT_COUNT)
			return nullptr;

		WheelState* s = new WheelState(port, static_cast<WheelType>(subtype));
		s->desc.full = &s->desc_dev;

		switch (s->type)
		{
			case WT_GT_FORCE:
				s->desc.str = gtf_desc_strings;
				if (usb_desc_parse_dev(gtf_dev_descriptor, sizeof(gtf_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(gtforce_config_descriptor, sizeof(gtforce_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			case WT_DRIVING_FORCE:
				s->desc.str = df_desc_strings;
				if (usb_desc_parse_dev(df_dev_descriptor, sizeof(df_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(df_config_descriptor, sizeof(df_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			case WT_DRIVING_FORCE_PRO:
				s->desc.str = dfp_desc_strings;
				if (usb_desc_parse_dev(dfp_dev_descriptor, sizeof(dfp_dev_descriptor), s->desc, s->desc_dev) < 0)
					goto fail;
				if (usb_desc_parse_config(dfp_config_descriptor, sizeof(dfp_config_descriptor), s->desc_dev) < 0)
					goto fail;
				break;
			default:
				break;
		}

		s->UpdateSettings(si, TypeName());
		pad_init(s);

		return &s->dev;

	fail:
		pad_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* WheelDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Wheel Device");
	}

	const char* WheelDevice::TypeName() const
	{
		return "Pad";
	}

	const char* WheelDevice::IconName() const
	{
		return ICON_PF_STEERING_WHEEL_ALT;
	}

	bool WheelDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);

		if (!sw.DoMarker("PadDevice"))
			return false;

		sw.Do(&s->data.last_steering);
		sw.DoPOD(&s->mFFstate);
		return true;
	}

	void WheelDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		USB_CONTAINER_OF(dev, WheelState, dev)->UpdateSettings(si, TypeName());
	}

	float WheelDevice::GetBindingValue(const USBDevice* dev, u32 bind_index) const
	{
		const WheelState* s = USB_CONTAINER_OF(dev, const WheelState, dev);
		return s->GetBindValue(bind_index);
	}

	void WheelDevice::SetBindingValue(USBDevice* dev, u32 bind_index, float value) const
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
		s->SetBindValue(bind_index, value);
	}

	std::span<const char*> WheelDevice::SubTypes() const
	{
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Driving Force"),
			TRANSLATE_NOOP("USB", "Driving Force Pro"),
			TRANSLATE_NOOP("USB", "GT Force")
		};
		return subtypes;
	}

	std::span<const InputBindingInfo> WheelDevice::Bindings(u32 subtype) const
	{
		switch (subtype)
		{
			case WT_GT_FORCE:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"L1", TRANSLATE_NOOP("USB", "Left Paddle / L1"), nullptr, InputBindingInfo::Type::Button, CID_L1, GenericInputBinding::L1},
					{"R1", TRANSLATE_NOOP("USB", "Right Paddle / R1"), nullptr, InputBindingInfo::Type::Button, CID_R1, GenericInputBinding::R1},
					{"X", TRANSLATE_NOOP("USB", "X"), nullptr, InputBindingInfo::Type::Button, CID_SQUARE, GenericInputBinding::Square},
					{"Y", TRANSLATE_NOOP("USB", "Y"), nullptr, InputBindingInfo::Type::Button, CID_TRIANGLE, GenericInputBinding::Triangle},
					{"A", TRANSLATE_NOOP("USB", "A"), nullptr, InputBindingInfo::Type::Button, CID_CROSS, GenericInputBinding::Cross},
					{"B", TRANSLATE_NOOP("USB", "B"), nullptr, InputBindingInfo::Type::Button, CID_CIRCLE, GenericInputBinding::Circle},
					{"FFDevice", TRANSLATE_NOOP("USB", "Force Feedback"), nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};
				return bindings;
			}

			case WT_DRIVING_FORCE:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_UP, GenericInputBinding::DPadUp},
					{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_DOWN, GenericInputBinding::DPadDown},
					{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_LEFT, GenericInputBinding::DPadLeft},
					{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, CID_CROSS, GenericInputBinding::Cross},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, CID_SQUARE, GenericInputBinding::Square},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, CID_CIRCLE, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, CID_TRIANGLE, GenericInputBinding::Triangle},
					{"L1", TRANSLATE_NOOP("USB", "L1"), nullptr, InputBindingInfo::Type::Button, CID_L1, GenericInputBinding::L1},
					{"R1", TRANSLATE_NOOP("USB", "R1"), nullptr, InputBindingInfo::Type::Button, CID_R1, GenericInputBinding::R1},
					{"L2", TRANSLATE_NOOP("USB", "L2"), nullptr, InputBindingInfo::Type::Button, CID_L2, GenericInputBinding::Unknown}, // used L2 for brake
					{"R2", TRANSLATE_NOOP("USB", "R2"), nullptr, InputBindingInfo::Type::Button, CID_R2, GenericInputBinding::Unknown}, // used R2 for throttle
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_START, GenericInputBinding::Start},
					{"L3", TRANSLATE_NOOP("USB", "Left Paddle / L3"), nullptr, InputBindingInfo::Type::Button, CID_L3, GenericInputBinding::L3},
					{"R3", TRANSLATE_NOOP("USB", "Right Paddle / R3"), nullptr, InputBindingInfo::Type::Button, CID_R3, GenericInputBinding::R3},
					{"FFDevice", TRANSLATE_NOOP("USB", "Force Feedback"), nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};
				return bindings;
			}

			case WT_DRIVING_FORCE_PRO:
			{
				static constexpr const InputBindingInfo bindings[] = {
					{"SteeringLeft", TRANSLATE_NOOP("USB", "Steering Left"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_L, GenericInputBinding::LeftStickLeft},
					{"SteeringRight", TRANSLATE_NOOP("USB", "Steering Right"), nullptr, InputBindingInfo::Type::HalfAxis, CID_STEERING_R, GenericInputBinding::LeftStickRight},
					{"Throttle", TRANSLATE_NOOP("USB", "Throttle"), nullptr, InputBindingInfo::Type::HalfAxis, CID_THROTTLE, GenericInputBinding::R2},
					{"Brake", TRANSLATE_NOOP("USB", "Brake"), nullptr, InputBindingInfo::Type::HalfAxis, CID_BRAKE, GenericInputBinding::L2},
					{"DPadUp", TRANSLATE_NOOP("USB", "D-Pad Up"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_UP, GenericInputBinding::DPadUp},
					{"DPadDown", TRANSLATE_NOOP("USB", "D-Pad Down"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_DOWN, GenericInputBinding::DPadDown},
					{"DPadLeft", TRANSLATE_NOOP("USB", "D-Pad Left"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_LEFT, GenericInputBinding::DPadLeft},
					{"DPadRight", TRANSLATE_NOOP("USB", "D-Pad Right"), nullptr, InputBindingInfo::Type::Button, CID_DPAD_RIGHT, GenericInputBinding::DPadRight},
					{"Cross", TRANSLATE_NOOP("USB", "Cross"), nullptr, InputBindingInfo::Type::Button, CID_CROSS, GenericInputBinding::Cross},
					{"Square", TRANSLATE_NOOP("USB", "Square"), nullptr, InputBindingInfo::Type::Button, CID_SQUARE, GenericInputBinding::Square},
					{"Circle", TRANSLATE_NOOP("USB", "Circle"), nullptr, InputBindingInfo::Type::Button, CID_CIRCLE, GenericInputBinding::Circle},
					{"Triangle", TRANSLATE_NOOP("USB", "Triangle"), nullptr, InputBindingInfo::Type::Button, CID_TRIANGLE, GenericInputBinding::Triangle},
					{"L1", TRANSLATE_NOOP("USB", "Left Paddle / L1"), nullptr, InputBindingInfo::Type::Button, CID_L1, GenericInputBinding::L1},
					{"R1", TRANSLATE_NOOP("USB", "Right Paddle / R1"), nullptr, InputBindingInfo::Type::Button, CID_R1, GenericInputBinding::R1},
					{"Select", TRANSLATE_NOOP("USB", "Select"), nullptr, InputBindingInfo::Type::Button, CID_SELECT, GenericInputBinding::Select},
					{"Start", TRANSLATE_NOOP("USB", "Start"), nullptr, InputBindingInfo::Type::Button, CID_START, GenericInputBinding::Start},
					{"L2", TRANSLATE_NOOP("USB", "L2"), nullptr, InputBindingInfo::Type::Button, CID_L2, GenericInputBinding::Unknown}, // used L2 for brake
					{"R2", TRANSLATE_NOOP("USB", "R2"), nullptr, InputBindingInfo::Type::Button, CID_R2, GenericInputBinding::Unknown}, // used R2 for throttle
					{"L3", TRANSLATE_NOOP("USB", "L3"), nullptr, InputBindingInfo::Type::Button, CID_L3, GenericInputBinding::L3},
					{"R3", TRANSLATE_NOOP("USB", "R3"), nullptr, InputBindingInfo::Type::Button, CID_R3, GenericInputBinding::R3},
					{"FFDevice", "Force Feedback", nullptr, InputBindingInfo::Type::Device, 0, GenericInputBinding::Unknown},
				};
				return bindings;
			}

			default:
			{
				return {};
			}
		}
	}

	std::span<const SettingInfo> WheelDevice::Settings(u32 subtype) const
	{
		static constexpr const char* SteeringCurveExponentOptions[] = {TRANSLATE_NOOP("USB", "Off"), TRANSLATE_NOOP("USB", "Low"), TRANSLATE_NOOP("USB", "Medium"), TRANSLATE_NOOP("USB", "High"), nullptr};
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::Integer, "SteeringSmoothing", TRANSLATE_NOOP("USB", "Steering Smoothing"),
				TRANSLATE_NOOP("USB", "Smooths out changes in steering to the specified percentage per poll. Needed for using keyboards."),
				"0", "0", "100", "1", TRANSLATE_NOOP("USB", "%d%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::Integer, "SteeringDeadzone", TRANSLATE_NOOP("USB", "Steering Deadzone"),
				TRANSLATE_NOOP("USB", "Steering axis deadzone for pads or non self centering wheels."),
				"0", "0", "100", "1", TRANSLATE_NOOP("USB", "%d%%"), nullptr, nullptr, 1.0f},
			{SettingInfo::Type::StringList, "SteeringCurveExponent", TRANSLATE_NOOP("USB", "Steering Damping"),
				TRANSLATE_NOOP("USB", "Applies power curve filter to steering axis values. Dampens small inputs."),
				"Off", nullptr, nullptr, nullptr, nullptr, SteeringCurveExponentOptions},
			{SettingInfo::Type::Boolean, "FfbDropoutWorkaround", TRANSLATE_NOOP("USB", "Workaround for Intermittent FFB Loss"),
				TRANSLATE_NOOP("USB", "Works around bugs in some wheels' firmware that result in brief interruptions in force. Leave this disabled unless you need it, as it has negative side effects on many wheels."),
				"false"}
		};
		return info;
	}

	void WheelDevice::InputDeviceConnected(USBDevice* dev, const std::string_view identifier) const
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
		if (s->mFFdevName == identifier)
			s->OpenFFDevice();
	}

	void WheelDevice::InputDeviceDisconnected(USBDevice* dev, const std::string_view identifier) const
	{
		WheelState* s = USB_CONTAINER_OF(dev, WheelState, dev);
		if (s->mFFdevName == identifier)
			s->mFFdev.reset();
	}
} // namespace usb_pad
