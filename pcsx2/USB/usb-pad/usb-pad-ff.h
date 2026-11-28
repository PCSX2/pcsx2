// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <cstdint>
#include <memory>
#include <string_view>

#include "common/Pcsx2Types.h"

namespace usb_pad
{
// Most likely as seen on https://github.com/matlo/GIMX
#define CMD_DOWNLOAD 0x00
#define CMD_DOWNLOAD_AND_PLAY 0x01
#define CMD_PLAY 0x02
#define CMD_STOP 0x03
#define CMD_DEFAULT_SPRING_ON 0x04
#define CMD_DEFAULT_SPRING_OFF 0x05
#define CMD_NORMAL_MODE 0x08
#define CMD_EXTENDED_CMD 0xF8
#define CMD_SET_LED 0x09 //??
#define CMD_RAW_MODE 0x0B
#define CMD_SET_DEFAULT_SPRING 0x0E
#define CMD_SET_DEAD_BAND 0x0F

#define EXT_CMD_CHANGE_MODE_DFP 0x01
#define EXT_CMD_WHEEL_RANGE_200_DEGREES 0x02
#define EXT_CMD_WHEEL_RANGE_900_DEGREES 0x03
#define EXT_CMD_CHANGE_MODE 0x09
#define EXT_CMD_REVERT_IDENTITY 0x0a
#define EXT_CMD_CHANGE_MODE_G25 0x10
#define EXT_CMD_CHANGE_MODE_G25_NO_DETACH 0x11
#define EXT_CMD_SET_RPM_LEDS 0x12
#define EXT_CMD_CHANGE_WHEEL_RANGE 0x81

#define FTYPE_CONSTANT 0x00
#define FTYPE_SPRING 0x01
#define FTYPE_DAMPER 0x02
#define FTYPE_AUTO_CENTER_SPRING 0x03
#define FTYPE_SAWTOOTH_UP 0x04
#define FTYPE_SAWTOOTH_DOWN 0x05
#define FTYPE_TRAPEZOID 0x06
#define FTYPE_RECTANGLE 0x07
#define FTYPE_VARIABLE 0x08
#define FTYPE_RAMP 0x09
#define FTYPE_SQUARE_WAVE 0x0A
#define FTYPE_HIGH_RESOLUTION_SPRING 0x0B
#define FTYPE_HIGH_RESOLUTION_DAMPER 0x0C
#define FTYPE_HIGH_RESOLUTION_AUTO_CENTER_SPRING 0x0D
#define FTYPE_FRICTION 0x0E

	enum EffectID
	{
		EFF_CONSTANT = 0,
		EFF_SPRING,
		EFF_DAMPER,
		EFF_FRICTION,
		EFF_RUMBLE,
	};

	struct spring
	{
		u8 dead1 : 8; //Lower limit of central dead band
		u8 dead2 : 8; //Upper limit of central dead band
		u8 k1 : 4;    //Low (left or push) side spring constant selector
		u8 k2 : 4;    //High (right or pull) side spring constant selector
		u8 s1 : 4;    //Low side slope inversion (1 = inverted)
		u8 s2 : 4;    //High side slope inversion (1 = inverted)
		u8 clip : 8;  //Clip level (maximum force), on either side
	};

	struct autocenter
	{
		u8 k1;
		u8 k2;
		u8 clip;
	};

	struct variable
	{
		u8 l1;     //Initial level for Force 0
		u8 l2;     //Initial level for Force 2
		u8 s1 : 4; //Force 0 Step size
		u8 t1 : 4; //Force 0 Step duration (in main loops)
		u8 s2 : 4;
		u8 t2 : 4;
		u8 d1 : 4; //Force 0 Direction (0 = increasing, 1 = decreasing)
		u8 d2 : 4;
	};

	struct ramp
	{
		u8 level1; //max force
		u8 level2; //min force
		u8 dir;
		u8 time : 4;
		u8 step : 4;
	};

	struct friction
	{
		u8 k1;
		u8 k2;
		u8 clip;
		u8 s1 : 4;
		u8 s2 : 4;
	};

	struct damper
	{
		u8 k1;
		u8 s1;
		u8 k2;
		u8 s2;
		u8 clip; //dfp only
	};

	//packet is 8 bytes
	struct ff_data
	{
		u8 cmdslot; // 0x0F cmd, 0xF0 slot
		u8 type;    // force type or cmd param
		union u
		{
			u8 params[5];
			struct spring spring;
			struct autocenter autocenter;
			struct variable variable;
			struct friction friction;
			struct damper damper;
		} u; //if anon union: gcc needs -fms-extensions?
		u8 padd0;
	};

	struct ff_state
	{
		u8 slot_type[4];
		u8 slot_force[4];
		ff_data slot_ffdata[4];
		bool deadband;
	};

	struct parsed_ff_data
	{
		union u
		{
			struct
			{
				int level;
			} constant;

			struct
			{
				int center;
				int deadband;
				int left_coeff;
				int right_coeff;
				int left_saturation;
				int right_saturation;
			} condition;

			struct
			{
				int weak_magnitude;
				int strong_magnitude;
			} rumble;
		} u;
	};

	struct FFDevice
	{
		virtual ~FFDevice() {}
		virtual void SetConstantForce(/*const parsed_ff_data& ff*/ int level) = 0;
		virtual void SetSpringForce(const parsed_ff_data& ff) = 0;
		virtual void SetDamperForce(const parsed_ff_data& ff) = 0;
		virtual void SetFrictionForce(const parsed_ff_data& ff) = 0;
		virtual void SetAutoCenter(int value) = 0;
		//virtual void SetGain(int gain) = 0;
		virtual void DisableForce(EffectID force) = 0;

		bool use_ffb_dropout_workaround = false;
	};

	void ProcessLogitechFFPacket(FFDevice& dev, const ff_data* data, bool isDFP, ff_state& state);

	std::unique_ptr<FFDevice> CreateSDLFFDevice(std::string_view device);
} // namespace usb_pad
