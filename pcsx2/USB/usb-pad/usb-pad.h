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

#ifndef USBPAD_H
#define USBPAD_H

#include "USB/qemu-usb/vl.h"
#include "USB/configuration.h"
#include "USB/deviceproxy.h"

namespace usb_pad
{

#define CHECK(exp)      \
	do                  \
	{                   \
		if (!(exp))     \
			goto Error; \
	} while (0)
#define SAFE_FREE(p)    \
	do                  \
	{                   \
		if (p)          \
		{               \
			free(p);    \
			(p) = NULL; \
		}               \
	} while (0)

#define S_CONFIG_JOY TEXT("Joystick")
#define N_JOYSTICK TEXT("joystick")

	class PadDevice
	{
	public:
		virtual ~PadDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Wheel device");
		}
		static const char* TypeName()
		{
			return "pad";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {"Driving Force", "Driving Force Pro", "Driving Force Pro (rev11.02)", "GT Force"};
		}
	};

	class RBDrumKitDevice
	{
	public:
		virtual ~RBDrumKitDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Rock Band drum kit");
		}
		static const char* TypeName()
		{
			return "rbdrumkit";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class BuzzDevice
	{
	public:
		virtual ~BuzzDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Buzz Device");
		}
		static const char* TypeName()
		{
			return "buzz_device";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class GametrakDevice
	{
	public:
		virtual ~GametrakDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Gametrak Device");
		}
		static const char* TypeName()
		{
			return "gametrak_device";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return std::vector<std::string>();
		}
		static void Initialize();
	};

	class RealPlayDevice
	{
	public:
		virtual ~RealPlayDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("RealPlay Device");
		}
		static const char* TypeName()
		{
			return "realplay_device";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {"RealPlay Racing", "RealPlay Sphere", "RealPlay Golf", "RealPlay Pool"};
		}
		static void Initialize();
	};

	class SeamicDevice
	{
	public:
		virtual ~SeamicDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Sega Seamic");
		}
		static const char* TypeName()
		{
			return "seamic";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	class KeyboardmaniaDevice
	{
	public:
		virtual ~KeyboardmaniaDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Keyboardmania");
		}
		static const char* TypeName()
		{
			return "keyboardmania";
		}
		static std::list<std::string> ListAPIs();
		static const TCHAR* LongAPIName(const std::string& name);
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(FreezeAction mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

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

	enum PS2WheelTypes
	{
		WT_GENERIC,                // DF or any other LT wheel in non-native mode
		WT_DRIVING_FORCE_PRO,      //LPRC-11000? DF GT can be downgraded to Pro (?)
		WT_DRIVING_FORCE_PRO_1102, //hw with buggy hid report?
		WT_GT_FORCE,               //formula gp
		WT_ROCKBAND1_DRUMKIT,
		WT_BUZZ_CONTROLLER,
		WT_GAMETRAK_CONTROLLER,
		WT_REALPLAY_RACING,
		WT_REALPLAY_SPHERE,
		WT_REALPLAY_GOLF,
		WT_REALPLAY_POOL,
		WT_SEGA_SEAMIC,
		WT_KEYBOARDMANIA_CONTROLLER,
	};

	inline int range_max(PS2WheelTypes type)
	{
		if (type == WT_DRIVING_FORCE_PRO || type == WT_DRIVING_FORCE_PRO_1102)
			return 0x3FFF;
		if (type == WT_SEGA_SEAMIC)
			return 255;
		return 0x3FF;
	}

	// hold intermediate wheel data
	struct wheel_data_t
	{
		int32_t steering;
		uint32_t buttons;
		uint32_t hatswitch;
		uint32_t hat_horz;
		uint32_t hat_vert;

		int32_t clutch; //no game uses though
		int32_t throttle;
		int32_t brake;
	};

	struct spring
	{
		uint8_t dead1 : 8; //Lower limit of central dead band
		uint8_t dead2 : 8; //Upper limit of central dead band
		uint8_t k1 : 4;    //Low (left or push) side spring constant selector
		uint8_t k2 : 4;    //High (right or pull) side spring constant selector
		uint8_t s1 : 4;    //Low side slope inversion (1 = inverted)
		uint8_t s2 : 4;    //High side slope inversion (1 = inverted)
		uint8_t clip : 8;  //Clip level (maximum force), on either side
	};

	struct autocenter
	{
		uint8_t k1;
		uint8_t k2;
		uint8_t clip;
	};

	struct variable
	{
		uint8_t l1;     //Initial level for Force 0
		uint8_t l2;     //Initial level for Force 2
		uint8_t s1 : 4; //Force 0 Step size
		uint8_t t1 : 4; //Force 0 Step duration (in main loops)
		uint8_t s2 : 4;
		uint8_t t2 : 4;
		uint8_t d1 : 4; //Force 0 Direction (0 = increasing, 1 = decreasing)
		uint8_t d2 : 4;
	};

	struct ramp
	{
		uint8_t level1; //max force
		uint8_t level2; //min force
		uint8_t dir;
		uint8_t time : 4;
		uint8_t step : 4;
	};

	struct friction
	{
		uint8_t k1;
		uint8_t k2;
		uint8_t clip;
		uint8_t s1 : 4;
		uint8_t s2 : 4;
	};

	struct damper
	{
		uint8_t k1;
		uint8_t s1;
		uint8_t k2;
		uint8_t s2;
		uint8_t clip; //dfp only
	};

	//packet is 8 bytes
	struct ff_data
	{
		uint8_t cmdslot; // 0x0F cmd, 0xF0 slot
		uint8_t type;    // force type or cmd param
		union u
		{
			uint8_t params[5];
			struct spring spring;
			struct autocenter autocenter;
			struct variable variable;
			struct friction friction;
			struct damper damper;
		} u; //if anon union: gcc needs -fms-extensions?
		uint8_t padd0;
	};

	struct ff_state
	{
		uint8_t slot_type[4];
		uint8_t slot_force[4];
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

	enum EffectID
	{
		EFF_CONSTANT = 0,
		EFF_SPRING,
		EFF_DAMPER,
		EFF_FRICTION,
		EFF_RUMBLE,
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
	};

	class Pad
	{
	public:
		Pad(int port, const char* dev_type)
			: mPort(port)
			, mDevType(dev_type)
		{
			memset(&mFFstate, 0, sizeof(mFFstate));
		}
		virtual ~Pad()
		{
			delete mFFdev;
			mFFdev = nullptr;
		}
		virtual int Open() = 0;
		virtual int Close() = 0;
		virtual int TokenIn(uint8_t* buf, int len) = 0;
		virtual int TokenOut(const uint8_t* data, int len) = 0;
		virtual int Reset() = 0;

		virtual PS2WheelTypes Type() { return mType; }
		virtual void Type(PS2WheelTypes type) { mType = type; }
		virtual int Port() { return mPort; }
		virtual void Port(int port) { mPort = port; }
		void ParseFFData(const ff_data* ffdata, bool isDFP);

	protected:
		PS2WheelTypes mType = PS2WheelTypes::WT_GENERIC;
		wheel_data_t mWheelData{};
		ff_state mFFstate;
		FFDevice* mFFdev = nullptr;
		int mPort;
		const char* mDevType;
	};

	//L3/R3 for newer wheels
	//enum PS2Buttons : uint32_t {
	//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE,
	//	PAD_L1, PAD_L2, PAD_R1, PAD_R2,
	//	PAD_SELECT, PAD_START,
	//	PAD_L3, PAD_R3, //order
	//	PAD_BUTTON_COUNT
	//};

	//???
	//enum DFButtons : uint32_t {
	//	PAD_CROSS = 0, PAD_SQUARE, PAD_CIRCLE, PAD_TRIANGLE,
	//	PAD_R2,
	//	PAD_L2,
	//	PAD_R1,
	//	PAD_L1,
	//	PAD_SELECT, PAD_START,
	//	PAD_BUTTON_COUNT
	//};

	//DF Pro buttons (?)
	//Based on Tokyo Xtreme Racer Drift 2
	//GT4 flips R1/L1 with R2/L2 with DF wheel type
	enum PS2Buttons : uint32_t
	{
		PAD_CROSS = 0, //menu up - GT Force
		PAD_SQUARE,    //menu down
		PAD_CIRCLE,    //X
		PAD_TRIANGLE,  //Y
		PAD_R1,        //A? <pause> in GT4
		PAD_L1,        //B
		PAD_R2,
		PAD_L2,
		PAD_SELECT,
		PAD_START,
		PAD_R3,
		PAD_L3, //order, only GT Force/Force EX?
		PAD_SHIFT_UP,
		PAD_SHIFT_DOWN, // DF Pro
		PAD_BUTTON_COUNT
	};

	enum PS2Axis : uint32_t
	{
		PAD_AXIS_X,
		PAD_AXIS_Y,
		PAD_AXIS_Z,
		PAD_AXIS_RZ,
		PAD_AXIS_HAT, //Treat as axis for mapping purposes
		PAD_AXIS_COUNT
	};

	enum PS2HatSwitch
	{
		PAD_HAT_N = 0,
		PAD_HAT_NE,
		PAD_HAT_E,
		PAD_HAT_SE,
		PAD_HAT_S,
		PAD_HAT_SW,
		PAD_HAT_W,
		PAD_HAT_NW,
		PAD_HAT_COUNT
	};

	enum Buzz
	{
		BUZZ_RED,
		BUZZ_YELLOW,
		BUZZ_GREEN,
		BUZZ_ORANGE,
		BUZZ_BLUE,
	};

	static const int HATS_8TO4[] = {PAD_HAT_N, PAD_HAT_E, PAD_HAT_S, PAD_HAT_W};

#define PAD_VID 0x046D
#define PAD_MOMO 0xCA03    //black MOMO
#define GENERIC_PID 0xC294 //actually Driving Force aka PID that most logitech wheels initially report
#define PID_DF 0xC294
#define PID_DFP 0xC298 //SELECT + R3 + RIGHT SHIFT PADDLE (R1) ???
#define PID_DFGT 0xC29A
#define PID_FORMULA 0xC202 //Yellow Wingman Formula
#define PID_FGP 0xC20E     //Formula GP (maybe GT FORCE LPRC-1000)
#define PID_FFGP 0xC293    // Formula Force GP
#define PID_GTF 0xC293     // as is Formula Force GP
#define PID_G25 0xC299     // OutRun 2 (jp) supports it apparently
#define PID_G27 0xC29B
#define MAX_BUTTONS 32
#define MAX_AXES 7 //random 7: axes + hatswitch
#define MAX_JOYS 32
#define PAD_LG_FFB_WHITELIST \
    PAD_MOMO, PID_DF, PID_DFP, PID_DFGT, PID_FORMULA, PID_FGP, PID_FFGP, PID_GTF, PID_G25, PID_G27

	/**
  linux hid-lg4ff.c
  http://www.spinics.net/lists/linux-input/msg16570.html
  Every Logitech wheel reports itself as generic Logitech Driving Force wheel (VID 046d, PID c294). This is done to ensure that the
  wheel will work on every USB HID-aware system even when no Logitech driver is available. It however limits the capabilities of the
  wheel - range is limited to 200 degrees, G25/G27 don't report the clutch pedal and there is only one combined axis for throttle and
  brake. The switch to native mode is done via hardware-specific command which is different for each wheel. When the wheel
  receives such command, it simulates reconnect and reports to the OS with its actual PID.
  Currently not emulating reattachment. Any games that expect to?
**/

	static const uint8_t df_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(PID_DF),
		/* bcdDevice           */ WBVAL(0x0000), //(00.00)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	//XXX different pedal data than 0x1106, buggy hw?
	static const uint8_t dfp_dev_descriptor_1102[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(PID_DFP),
		/* bcdDevice           */ WBVAL(0x1102), //(11.02)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	static const uint8_t dfp_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0100), //(272) //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x08,          //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(PID_DFP),
		/* bcdDevice           */ WBVAL(0x1106), //(11.06)
		/* iManufacturer       */ 0x03,          //(3)
		/* iProduct            */ 0x01,          //(1)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	static const uint8_t gtf_dev_descriptor[] = {
		/* bLength             */ 0x12, //(18)
		/* bDescriptorType     */ 0x01, //(1)
		/* bcdUSB              */ WBVAL(0x0100),
		/* bDeviceClass        */ 0x00, //(0)
		/* bDeviceSubClass     */ 0x00, //(0)
		/* bDeviceProtocol     */ 0x00, //(0)
		/* bMaxPacketSize0     */ 0x08, //(8)
		/* idVendor            */ WBVAL(0x046d),
		/* idProduct           */ WBVAL(PID_GTF),
		/* bcdDevice           */ WBVAL(0x0000),
		/* iManufacturer       */ 0x01, //actual is 0x04
		/* iProduct            */ 0x02, //actual is 0x20
		/* iSerialNumber       */ 0x00, //(0)
		/* bNumConfigurations  */ 0x01, //(1)
	};

	//https://lkml.org/lkml/2011/5/28/140
	//https://github.com/torvalds/linux/blob/master/drivers/hid/hid-lg.c
	// separate axes version
	static const uint8_t pad_driving_force_hid_separate_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x34,             /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0C,       /* Report Count (12), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage (Buttons), */
		0x19, 0x01,       /* Usage Minimum (1), */
		0x29, 0x0c,       /* Usage Maximum (12), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x02,       /* Report Count (2), */
		0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
		0x09, 0x01,       /* Usage (?: 1), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x08,       /* Report Size (8), */
		0x81, 0x02,       /* Input (Variable), */
		0x25, 0x07,       /* Logical Maximum (7), */
		0x46, 0x3B, 0x01, /* Physical Maximum (315), */
		0x75, 0x04,       /* Report Size (4), */
		0x65, 0x14,       /* Unit (Degrees), */
		0x09, 0x39,       /* Usage (Hat Switch), */
		0x81, 0x42,       /* Input (Variable, Null State), */
		0x75, 0x01,       /* Report Size (1), */
		0x95, 0x04,       /* Report Count (4), */
		0x65, 0x00,       /* Unit (none), */
		0x06, 0x00, 0xFF, /* Usage Page (Vendor: 65280), */
		0x09, 0x01,       /* Usage (?: 1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x08,       /* Report Size (8), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (Rz), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x95, 0x07,       /* Report Count (7), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x03,       /* Usage (?: 3), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	static const uint8_t pad_driving_force_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x95, 0x01,       //     Report Count (1)
		0x75, 0x0A,       //     Report Size (10)
		0x15, 0x00,       //     Logical Minimum (0)
		0x26, 0xFF, 0x03, //     Logical Maximum (1023)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0xFF, 0x03, //     Physical Maximum (1023)
		0x09, 0x30,       //     Usage (X)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x0C,       //     Report Count (12)
		0x75, 0x01,       //     Report Size (1)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x05, 0x09,       //     Usage Page (Button)
		0x19, 0x01,       //     Usage Minimum (0x01)
		0x29, 0x0C,       //     Usage Maximum (0x0C)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x02,       //     Report Count (2)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01,       //     Usage (0x01)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,       //     Usage Page (Generic Desktop Ctrls)
		0x09, 0x31,       //     Usage (Y)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x95, 0x01,       //     Report Count (1)
		0x75, 0x08,       //     Report Size (8)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x25, 0x07,       //     Logical Maximum (7)
		0x46, 0x3B, 0x01, //     Physical Maximum (315)
		0x75, 0x04,       //     Report Size (4)
		0x65, 0x14,       //     Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,       //     Usage (Hat switch)
		0x81, 0x42,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x04,       //     Report Count (4)
		0x65, 0x00,       //     Unit (None)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01,       //     Usage (0x01)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x02,       //     Report Count (2)
		0x75, 0x08,       //     Report Size (8)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x02,       //     Usage (0x02)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0xA1, 0x02,       //   Collection (Logical)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x95, 0x07,       //     Report Count (7)
		0x75, 0x08,       //     Report Size (8)
		0x09, 0x03,       //     Usage (0x03)
		0x91, 0x02,       //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             //   End Collection
		0xC0,             // End Collection

		// 130 bytes
	};

	static const uint8_t pad_driving_force_pro_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0E,       /* Report Size (14), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x3F, /* Logical Maximum (16383), */
		0x34,             /* Physical Minimum (0), */
		0x46, 0xFF, 0x3F, /* Physical Maximum (16383), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0E,       /* Report Count (14), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x0E,       /* Usage Maximum (0Eh), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x04,       /* Report Size (4), */
		0x25, 0x07,       /* Logical Maximum (7), */
		0x46, 0x3B, 0x01, /* Physical Maximum (315), */
		0x65, 0x14,       /* Unit (Degrees), */
		0x09, 0x39,       /* Usage (Hat Switch), */
		0x81, 0x42,       /* Input (Variable, Nullstate), */
		0x65, 0x00,       /* Unit, */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x75, 0x08,       /* Report Size (8), */
		0x81, 0x01,       /* Input (Constant), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (Rz), */
		0x81, 0x02,       /* Input (Variable), */
		0x81, 0x01,       /* Input (Constant), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	static const uint8_t pad_momo_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x35, 0x00,       /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x08,       /* Report Count (8), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x08,       /* Usage Maximum (08h), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x75, 0x0E,       /* Report Size (14), */
		0x95, 0x01,       /* Report Count (1), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x00,       /* Usage (00h), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x32,       /* Usage (Z), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x09, 0x01,       /* Usage (01h), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	static const uint8_t pad_generic_hid_report_descriptor[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x35, 0x00,       /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0a,       /* Report Count (10), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x0a,       /* Usage Maximum (0ah), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x75, 0x0C,       /* Report Size (12), */
		0x95, 0x01,       /* Report Count (1), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x00,       /* Usage (00h), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x32,       /* Usage (Z), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (RZ), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x09, 0x01,       /* Usage (01h), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

	//TODO
	static const uint8_t pad_gtforce_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x95, 0x01,       //   Report Count (1)
		0x75, 0x0A,       //   Report Size (10)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x03, //   Logical Maximum (1023)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0xFF, 0x03, //   Physical Maximum (1023)
		0x09, 0x30,       //   Usage (X)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x06,       //   Report Count (6)
		0x75, 0x01,       //   Report Size (1)
		0x25, 0x01,       //   Logical Maximum (1)
		0x45, 0x01,       //   Physical Maximum (1)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x06,       //   Usage Maximum (0x06)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x01,       //   Report Count (1)
		0x75, 0x08,       //   Report Size (8)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		//0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01, //   Usage (0x01)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
		0x09, 0x31, //   Usage (Y)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		//0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x01, //   Usage (0x01)
		0x95, 0x03, //   Report Count (3)
		0x81, 0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,       //   End Collection
		0xA1, 0x02, //   Collection (Logical)
		0x09, 0x02, //   Usage (0x02)
		0x95, 0x07, //   Report Count (7)
		0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,       //   End Collection
		0xC0,       // End Collection
	};

	static const uint8_t pad_gtforce_hid_report_descriptor_prolly_incorrect[] = {
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x09, 0x04,       /* Usage (Joystik), */
		0xA1, 0x01,       /* Collection (Application), */
		0xA1, 0x02,       /* Collection (Logical), */
		0x95, 0x01,       /* Report Count (1), */
		0x75, 0x0A,       /* Report Size (10), */
		0x14,             /* Logical Minimum (0), */
		0x26, 0xFF, 0x03, /* Logical Maximum (1023), */
		0x35, 0x00,       /* Physical Minimum (0), */
		0x46, 0xFF, 0x03, /* Physical Maximum (1023), */
		0x09, 0x30,       /* Usage (X), */
		0x81, 0x02,       /* Input (Variable), */
		0x95, 0x0a,       /* Report Count (6), */
		0x75, 0x01,       /* Report Size (1), */
		0x25, 0x01,       /* Logical Maximum (1), */
		0x45, 0x01,       /* Physical Maximum (1), */
		0x05, 0x09,       /* Usage Page (Button), */
		0x19, 0x01,       /* Usage Minimum (01h), */
		0x29, 0x0a,       /* Usage Maximum (06h), */
		0x81, 0x02,       /* Input (Variable), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x75, 0x0C,       /* Report Size (8), */
		0x95, 0x01,       /* Report Count (1), */
		0x26, 0xFF, 0x00, /* Logical Maximum (255), */
		0x46, 0xFF, 0x00, /* Physical Maximum (255), */
		0x09, 0x00,       /* Usage (00h), */
		0x81, 0x02,       /* Input (Variable), */
		0x05, 0x01,       /* Usage Page (Desktop), */
		0x75, 0x08,       /* Report Size (8), */
		0x09, 0x31,       /* Usage (Y), */
		0x81, 0x01,       /* Input (Constant), */
		0x09, 0x32,       /* Usage (Z), */
		0x81, 0x02,       /* Input (Variable), */
		0x09, 0x35,       /* Usage (RZ), */
		0x81, 0x02,       /* Input (Variable), */
		0x75, 0x0C,       /* Report Size (16), */
		0x95, 0x01,       /* Report Count (1), */
		0x06, 0x00, 0xFF, /* Usage Page (FF00h), */
		0x09, 0x01,       /* Usage (01h), */
		0x81, 0x02,       /* Input (Variable), */
		0xC0,             /* End Collection, */
		0xA1, 0x02,       /* Collection (Logical), */
		0x09, 0x02,       /* Usage (02h), */
		0x95, 0x07,       /* Report Count (7), */
		0x91, 0x02,       /* Output (Variable), */
		0xC0,             /* End Collection, */
		0xC0              /* End Collection */
	};

#define USB_PSIZE 8
#define DESC_CONFIG_WORD(a) (a & 0xFF), ((a >> 8) & 0xFF)

	static const uint8_t df_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE,       // Size of this descriptor in bytes
		USB_INTERFACE_DESCRIPTOR_TYPE, // INTERFACE descriptor type
		0,                             // Interface Number
		0,                             // Alternate Setting Number
		2,                             // Number of endpoints in this intf
		USB_CLASS_HID,                 // Class code
		0,                             // Subclass code
		0,                             // Protocol code
		0,                             // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                                                              // Size of this descriptor in bytes RRoj hack
		USB_DT_HID,                                                        // HID descriptor type
		DESC_CONFIG_WORD(0x0100),                                          // HID Spec Release Number in BCD format (1.11)
		0x21,                                                              // Country Code (0x00 for Not supported, 0x21 for US)
		1,                                                                 // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,                                                     // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_driving_force_hid_report_descriptor)), // Size of the report descriptor

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};

	static const uint8_t dfp_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE,       // Size of this descriptor in bytes
		USB_INTERFACE_DESCRIPTOR_TYPE, // INTERFACE descriptor type
		0,                             // Interface Number
		0,                             // Alternate Setting Number
		2,                             // Number of endpoints in this intf
		USB_CLASS_HID,                 // Class code
		0,                             // Subclass code
		0,                             // Protocol code
		0,                             // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                     // Size of this descriptor in bytes
		USB_DT_HID,               // HID descriptor type
		DESC_CONFIG_WORD(0x0100), // HID Spec Release Number in BCD format (1.11)
		0x21,                     // Country Code (0x00 for Not supported, 0x21 for US)
		1,                        // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,            // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_driving_force_pro_hid_report_descriptor)),

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};

	static const uint8_t gtforce_config_descriptor[] = {
		USB_CONFIGURATION_DESC_SIZE,       /* bLength */
		USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType */
		WBVAL(41),                         /* wTotalLength */
		0x01,                              /* bNumInterfaces */
		0x01,                              /* bConfigurationValue */
		0x00,                              /* iConfiguration */
		0xc0,                              /* bmAttributes */
		USB_CONFIG_POWER_MA(80),           /* bMaxPower */

		/* Interface Descriptor */
		USB_INTERFACE_DESC_SIZE, // Size of this descriptor in bytes
		USB_DT_INTERFACE,        // INTERFACE descriptor type
		0,                       // Interface Number
		0,                       // Alternate Setting Number
		2,                       // Number of endpoints in this intf
		USB_CLASS_HID,           // Class code
		0,                       // Subclass code
		0,                       // Protocol code
		0,                       // Interface string index

		/* HID Class-Specific Descriptor */
		0x09,                                                        // Size of this descriptor in bytes
		USB_DT_HID,                                                  // HID descriptor type
		DESC_CONFIG_WORD(0x0100),                                    // HID Spec Release Number in BCD format (1.11)
		0x21,                                                        // Country Code (0x00 for Not supported, 0x21 for US)
		1,                                                           // Number of class descriptors, see usbcfg.h
		USB_DT_REPORT,                                               // Report descriptor type
		DESC_CONFIG_WORD(sizeof(pad_gtforce_hid_report_descriptor)), // Size of the report descriptor

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_IN(1),           //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval

		/* Endpoint Descriptor */
		USB_ENDPOINT_DESC_SIZE,
		USB_ENDPOINT_DESCRIPTOR_TYPE, //Endpoint Descriptor
		USB_ENDPOINT_OUT(2),          //EndpointAddress
		USB_ENDPOINT_TYPE_INTERRUPT,  //Attributes
		DESC_CONFIG_WORD(USB_PSIZE),  //size
		0x0A,                         //Interval
	};

	// Should be usb 2.0, but seems to make no difference with Rock Band games
	static const uint8_t rb1_dev_descriptor[] = {
		/* bLength             */ 0x12,          //(18)
		/* bDescriptorType     */ 0x01,          //(1)
		/* bcdUSB              */ WBVAL(0x0110), //USB 1.1
		/* bDeviceClass        */ 0x00,          //(0)
		/* bDeviceSubClass     */ 0x00,          //(0)
		/* bDeviceProtocol     */ 0x00,          //(0)
		/* bMaxPacketSize0     */ 0x40,          //(64)
		/* idVendor            */ WBVAL(0x12ba),
		/* idProduct           */ WBVAL(0x0210),
		/* bcdDevice           */ WBVAL(0x1000), //(10.00)
		/* iManufacturer       */ 0x01,          //(1)
		/* iProduct            */ 0x02,          //(2)
		/* iSerialNumber       */ 0x00,          //(0)
		/* bNumConfigurations  */ 0x01,          //(1)
	};

	//Wii Rock Band drum kit
	static const uint8_t rb1_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x29, 0x00, // wTotalLength 41
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x32,       // bMaxPower 100mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x02, // bNumEndpoints 2
		0x03, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x11, 0x01, // bcdHID 1.11
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x89, 0x00, // wDescriptorLength[0] 137

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x02,       // bEndpointAddress (OUT/H2D)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)
					// 41 bytes
	};

	//Wii Rock Band drum kit
	static const uint8_t rb1_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x05,       // Usage (Game Pad)
		0xA1, 0x01,       // Collection (Application)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x35, 0x00,       //   Physical Minimum (0)
		0x45, 0x01,       //   Physical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x0D,       //   Report Count (13)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x0D,       //   Usage Maximum (0x0D)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x95, 0x03,       //   Report Count (3)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x01,       //   Usage Page (Generic Desktop Ctrls)
		0x25, 0x07,       //   Logical Maximum (7)
		0x46, 0x3B, 0x01, //   Physical Maximum (315)
		0x75, 0x04,       //   Report Size (4)
		0x95, 0x01,       //   Report Count (1)
		0x65, 0x14,       //   Unit (System: English Rotation, Length: Centimeter)
		0x09, 0x39,       //   Usage (Hat switch)
		0x81, 0x42,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
		0x65, 0x00,       //   Unit (None)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x09, 0x30,       //   Usage (X)
		0x09, 0x31,       //   Usage (Y)
		0x09, 0x32,       //   Usage (Z)
		0x09, 0x35,       //   Usage (Rz)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x20,       //   Usage (0x20)
		0x09, 0x21,       //   Usage (0x21)
		0x09, 0x22,       //   Usage (0x22)
		0x09, 0x23,       //   Usage (0x23)
		0x09, 0x24,       //   Usage (0x24)
		0x09, 0x25,       //   Usage (0x25)
		0x09, 0x26,       //   Usage (0x26)
		0x09, 0x27,       //   Usage (0x27)
		0x09, 0x28,       //   Usage (0x28)
		0x09, 0x29,       //   Usage (0x29)
		0x09, 0x2A,       //   Usage (0x2A)
		0x09, 0x2B,       //   Usage (0x2B)
		0x95, 0x0C,       //   Report Count (12)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x0A, 0x21, 0x26, //   Usage (0x2621)
		0x95, 0x08,       //   Report Count (8)
		0xB1, 0x02,       //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x0A, 0x21, 0x26, //   Usage (0x2621)
		0x91, 0x02,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0x26, 0xFF, 0x03, //   Logical Maximum (1023)
		0x46, 0xFF, 0x03, //   Physical Maximum (1023)
		0x09, 0x2C,       //   Usage (0x2C)
		0x09, 0x2D,       //   Usage (0x2D)
		0x09, 0x2E,       //   Usage (0x2E)
		0x09, 0x2F,       //   Usage (0x2F)
		0x75, 0x10,       //   Report Size (16)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             // End Collection

		// 137 bytes
	};

	//////////
	// Buzz //
	//////////

	static const uint8_t buzz_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0x4C, 0x05, // idVendor 0x054C
		0x02, 0x00, // idProduct 0x0002
		0xA1, 0x05, // bcdDevice 11.01
		0x03,       // iManufacturer (String Index)
		0x01,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t buzz_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x32,       // bMaxPower 100mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0x03, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x11, 0x01, // bcdHID 1.11
		0x33,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x4E, 0x00, // wDescriptorLength[0] 78

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	static const uint8_t buzz_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0xA1, 0x02,       //   Collection (Logical)
		0x75, 0x08,       //     Report Size (8)
		0x95, 0x02,       //     Report Count (2)
		0x15, 0x00,       //     Logical Minimum (0)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x35, 0x00,       //     Physical Minimum (0)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x30,       //     Usage (X)
		0x09, 0x31,       //     Usage (Y)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x14,       //     Report Count (20)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x05, 0x09,       //     Usage Page (Button)
		0x19, 0x01,       //     Usage Minimum (0x01)
		0x29, 0x14,       //     Usage Maximum (0x14)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //     Usage Page (Vendor Defined 0xFF00)
		0x75, 0x01,       //     Report Size (1)
		0x95, 0x04,       //     Report Count (4)
		0x25, 0x01,       //     Logical Maximum (1)
		0x45, 0x01,       //     Physical Maximum (1)
		0x09, 0x01,       //     Usage (0x01)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0xA1, 0x02,       //   Collection (Logical)
		0x75, 0x08,       //     Report Size (8)
		0x95, 0x07,       //     Report Count (7)
		0x26, 0xFF, 0x00, //     Logical Maximum (255)
		0x46, 0xFF, 0x00, //     Physical Maximum (255)
		0x09, 0x02,       //     Usage (0x02)
		0x91, 0x02,       //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
		0xC0,             //   End Collection
		0xC0,             // End Collection
						  // 78 bytes
	};

	///////////////////
	// Keyboardmania //
	///////////////////
	static const uint8_t kbm_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x10, 0x01, // bcdUSB 1.10
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0x07, 0x05, // idVendor 0x0507
		0x10, 0x00, // idProduct 0x0010
		0x00, 0x01, // bcdDevice 01.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t kbm_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x19,       // bMaxPower 50mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x01, // bNumEndpoints 1
		0x03, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x02, // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x10, 0x01, // bcdHID 1.11
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x96, 0x00, // wDescriptorLength[0] 150

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x08, 0x00, // wMaxPacketSize 8
		0x04,       // bInterval 4 (unit depends on device speed)
	};

	static const uint8_t kbm_hid_report_descriptor[] = {
		0x05, 0x01, // USAGE_PAGE (Generic Desktop)
		0x09, 0x05, // USAGE (Game Pad)
		0xA1, 0x01, // COLLECTION (Application)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x3A, //   USAGE_MINIMUM (Button 58)
		0x29, 0x3F, //   USAGE_MAXIMUM (Button 63)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x06, //   REPORT_COUNT (6)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x02, //   REPORT_COUNT (2)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x01, //   USAGE_MINIMUM (Button 1)
		0x29, 0x07, //   USAGE_MAXIMUM (Button 7)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x08, //   USAGE_MINIMUM (Button 8)
		0x29, 0x0E, //   USAGE_MAXIMUM (Button 14)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x0F, //   USAGE_MINIMUM (Button 15)
		0x29, 0x15, //   USAGE_MAXIMUM (Button 21)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x09, //   USAGE_PAGE (Button)
		0x19, 0x16, //   USAGE_MINIMUM (Button 22)
		0x29, 0x1C, //   USAGE_MAXIMUM (Button 28)
		0x15, 0x00, //   LOGICAL_MINIMUM (0)
		0x25, 0x01, //   LOGICAL_MAXIMUM (1)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x07, //   REPORT_COUNT (7)
		0x81, 0x02, //   INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x75, 0x01, //   REPORT_SIZE (1)
		0x95, 0x01, //   REPORT_COUNT (1)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0x05, 0x01, //   USAGE_PAGE (Generic Desktop)
		0x09, 0x01, //   USAGE (Pointer)
		0xA1, 0x00, //   COLLECTION (Physical)
		0x09, 0x30, //     USAGE (X)
		0x09, 0x31, //     USAGE (Y)
		0x15, 0xFF, //     LOGICAL_MINIMUM (-1)
		0x25, 0x01, //     LOGICAL_MAXIMUM (1)
		0x95, 0x02, //     REPORT_COUNT (2)
		0x75, 0x02, //     REPORT_SIZE (2)
		0x81, 0x02, //     INPUT (Data,Variable,Absolute,NoWrap,Linear,PrefState,NoNull,NonVolatile,Bitmap)
		0x95, 0x04, //     REPORT_COUNT (4)
		0x75, 0x01, //     REPORT_SIZE (1)
		0x81, 0x01, //     INPUT (Constant,Array,Absolute)
		0xC0,       //   END_COLLECTION
		0x75, 0x08, //   REPORT_SIZE (8)
		0x95, 0x02, //   REPORT_COUNT (2)
		0x81, 0x01, //   INPUT (Constant,Array,Absolute)
		0xc0        // END_COLLECTION
	};

	//////////////
	// GameTrak //
	//////////////

	// Product ID :
	// 0x0982 - PlayStation 2
	// 0x0984 - ???

	[[maybe_unused]] static uint8_t gametrak_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x10, 0x01, // bcdUSB 1.10
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x08,       // bMaxPacketSize0 8
		0xB7, 0x14, // idVendor 0x14B7
		0x82, 0x09, // idProduct 0x0982
		0x01, 0x00, // bcdDevice 0.01
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t gametrak_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x22, 0x00, // wTotalLength 34
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x0A,       // bMaxPower 20mA

		0x09,       // bLength
		0x04,       // bDescriptorType (Interface)
		0x00,       // bInterfaceNumber 0
		0x00,       // bAlternateSetting
		0x01,       // bNumEndpoints 1
		0x03,       // bInterfaceClass
		0x00,       // bInterfaceSubClass
		0x00,       // bInterfaceProtocol
		0x00,       // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x01, 0x01, // bcdHID 1.01
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x7A, 0x00, // wDescriptorLength[0] 122

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x10, 0x00, // wMaxPacketSize 16
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	static const uint8_t gametrak_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x04,       // Usage (Joystick)
		0xA1, 0x01,       // Collection (Application)
		0x09, 0x01,       //   Usage (Pointer)
		0xA1, 0x00,       //   Collection (Physical)
		0x09, 0x30,       //     Usage (X)
		0x09, 0x31,       //     Usage (Y)
		0x09, 0x32,       //     Usage (Z)
		0x09, 0x33,       //     Usage (Rx)
		0x09, 0x34,       //     Usage (Ry)
		0x09, 0x35,       //     Usage (Rz)
		0x16, 0x00, 0x00, //     Logical Minimum (0)
		0x26, 0xFF, 0x0F, //     Logical Maximum (4095)
		0x36, 0x00, 0x00, //     Physical Minimum (0)
		0x46, 0xFF, 0x0F, //     Physical Maximum (4095)
		0x66, 0x00, 0x00, //     Unit (None)
		0x75, 0x10,       //     Report Size (16)
		0x95, 0x06,       //     Report Count (6)
		0x81, 0x02,       //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             //   End Collection
		0x09, 0x39,       //   Usage (Hat switch)
		0x15, 0x01,       //   Logical Minimum (1)
		0x25, 0x08,       //   Logical Maximum (8)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0x3B, 0x01, //   Physical Maximum (315)
		0x65, 0x14,       //   Unit (System: English Rotation, Length: Centimeter)
		0x75, 0x04,       //   Report Size (4)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x0C,       //   Usage Maximum (0x0C)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x0C,       //   Report Count (12)
		0x55, 0x00,       //   Unit Exponent (0)
		0x65, 0x00,       //   Unit (None)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x02,       //   Report Count (2)
		0x81, 0x01,       //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x05, 0x08,       //   Usage Page (LEDs)
		0x09, 0x43,       //   Usage (Slow Blink On Time)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x01,       //   Report Count (1)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x44,       //   Usage (Slow Blink Off Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x45,       //   Usage (Fast Blink On Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0x09, 0x46,       //   Usage (Fast Blink Off Time)
		0x91, 0x82,       //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Volatile)
		0xC0,             // End Collection
	};

	//////////////
	// RealPlay //
	//////////////

	// Product ID :
	// Racing - 0x09B2
	// Sphere - 0x09B3
	// Golf - 0x09B5
	// Pool - 0x09B6

	// RealPlay Golf is dumped from a real controller.
	// The others were force-brutted to be accepted by games - they may be inaccurate.

	static const uint8_t realplay_racing_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x40,       // bMaxPacketSize0 64
		0xB7, 0x14, // idVendor 0x14B7
		0xB2, 0x09, // idProduct 0x09B2
		0x00, 0x01, // bcdDevice 2.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t realplay_sphere_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x40,       // bMaxPacketSize0 64
		0xB7, 0x14, // idVendor 0x14B7
		0xB3, 0x09, // idProduct 0x09B3
		0x00, 0x01, // bcdDevice 2.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t realplay_golf_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x40,       // bMaxPacketSize0 64
		0xB7, 0x14, // idVendor 0x14B7
		0xB5, 0x09, // idProduct 0x09B5
		0x00, 0x01, // bcdDevice 2.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t realplay_pool_dev_descriptor[] = {
		0x12,       // bLength
		0x01,       // bDescriptorType (Device)
		0x00, 0x02, // bcdUSB 2.00
		0x00,       // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,       // bDeviceSubClass
		0x00,       // bDeviceProtocol
		0x40,       // bMaxPacketSize0 64
		0xB7, 0x14, // idVendor 0x14B7
		0xB6, 0x09, // idProduct 0x09B6
		0x00, 0x01, // bcdDevice 2.00
		0x01,       // iManufacturer (String Index)
		0x02,       // iProduct (String Index)
		0x00,       // iSerialNumber (String Index)
		0x01,       // bNumConfigurations 1
	};

	static const uint8_t realplay_config_descriptor[] = {
		0x09,       // bLength
		0x02,       // bDescriptorType (Configuration)
		0x29, 0x00, // wTotalLength 41
		0x01,       // bNumInterfaces 1
		0x01,       // bConfigurationValue
		0x00,       // iConfiguration (String Index)
		0x80,       // bmAttributes
		0x32,       // bMaxPower 100mA

		0x09, // bLength
		0x04, // bDescriptorType (Interface)
		0x00, // bInterfaceNumber 0
		0x00, // bAlternateSetting
		0x02, // bNumEndpoints 2
		0x03, // bInterfaceClass
		0x00, // bInterfaceSubClass
		0x00, // bInterfaceProtocol
		0x00, // iInterface (String Index)

		0x09,       // bLength
		0x21,       // bDescriptorType (HID)
		0x11, 0x01, // bcdHID 1.11
		0x00,       // bCountryCode
		0x01,       // bNumDescriptors
		0x22,       // bDescriptorType[0] (HID)
		0x85, 0x00, // wDescriptorLength[0] 133

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x02,       // bEndpointAddress (OUT/H2D)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)

		0x07,       // bLength
		0x05,       // bDescriptorType (Endpoint)
		0x81,       // bEndpointAddress (IN/D2H)
		0x03,       // bmAttributes (Interrupt)
		0x40, 0x00, // wMaxPacketSize 64
		0x0A,       // bInterval 10 (unit depends on device speed)
	};

	static const uint8_t realplay_hid_report_descriptor[] = {
		0x05, 0x01,       // Usage Page (Generic Desktop Ctrls)
		0x09, 0x05,       // Usage (Game Pad)
		0xA1, 0x01,       // Collection (Application)
		0x15, 0x00,       //   Logical Minimum (0)
		0x26, 0xFF, 0x0F, //   Logical Maximum (4095)
		0x35, 0x00,       //   Physical Minimum (0)
		0x46, 0xFF, 0x0F, //   Physical Maximum (4095)
		0x09, 0x30,       //   Usage (X)
		0x75, 0x0C,       //   Report Size (12)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x03,       //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x09, 0x31,       //   Usage (Y)
		0x75, 0x0C,       //   Report Size (12)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x03,       //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x09, 0x32,       //   Usage (Z)
		0x75, 0x0C,       //   Report Size (12)
		0x95, 0x01,       //   Report Count (1)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x03,       //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x20,       //   Usage (0x20)
		0x09, 0x21,       //   Usage (0x21)
		0x09, 0x22,       //   Usage (0x22)
		0x09, 0x23,       //   Usage (0x23)
		0x09, 0x24,       //   Usage (0x24)
		0x09, 0x25,       //   Usage (0x25)
		0x09, 0x26,       //   Usage (0x26)
		0x09, 0x27,       //   Usage (0x27)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x08,       //   Report Count (8)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x15, 0x00,       //   Logical Minimum (0)
		0x25, 0x01,       //   Logical Maximum (1)
		0x35, 0x00,       //   Physical Minimum (0)
		0x45, 0x01,       //   Physical Maximum (1)
		0x75, 0x01,       //   Report Size (1)
		0x95, 0x08,       //   Report Count (8)
		0x05, 0x09,       //   Usage Page (Button)
		0x19, 0x01,       //   Usage Minimum (0x01)
		0x29, 0x08,       //   Usage Maximum (0x08)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
		0x09, 0x28,       //   Usage (0x28)
		0x09, 0x29,       //   Usage (0x29)
		0x09, 0x2A,       //   Usage (0x2A)
		0x09, 0x2B,       //   Usage (0x2B)
		0x26, 0xFF, 0x00, //   Logical Maximum (255)
		0x46, 0xFF, 0x00, //   Physical Maximum (255)
		0x75, 0x08,       //   Report Size (8)
		0x95, 0x04,       //   Report Count (4)
		0x81, 0x02,       //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
		0xC0,             // End Collection
	};

	struct dfp_buttons_t
	{
		uint16_t cross : 1;
		uint16_t square : 1;
		uint16_t circle : 1;
		uint16_t triangle : 1;
		uint16_t rpaddle_R1 : 1;
		uint16_t lpaddle_L1 : 1;
		uint16_t R2 : 1;
		uint16_t L2 : 1;
		uint16_t select : 1;
		uint16_t start : 1;
		uint16_t R3 : 1;
		uint16_t L3 : 1;
		uint16_t shifter_back : 1;
		uint16_t shifter_fwd : 1;
		uint16_t padding : 2;
	};

	struct dfgt_buttons_t
	{
		uint16_t cross : 1;
		uint16_t square : 1;
		uint16_t circle : 1;
		uint16_t triangle : 1;
		uint16_t rpaddle_R1 : 1;
		uint16_t lpaddle_L1 : 1;
		uint16_t R2 : 1;
		uint16_t L2 : 1;
		uint16_t select : 1;
		uint16_t start : 1;
		uint16_t R3 : 1;
		uint16_t L3 : 1;
		uint16_t shifter_back : 1;
		uint16_t shifter_fwd : 1;
		uint16_t dial_center : 1;
		uint16_t dial_cw : 1;

		uint16_t dial_ccw : 1;
		uint16_t rocker_minus : 1;
		uint16_t horn : 1;
		uint16_t ps_button : 1;
		uint16_t padding : 12;
	};

	struct dfp_data_t
	{
		uint32_t axis_x : 14;
		uint32_t buttons : 14;
		uint32_t hatswitch : 4;

		uint32_t pad0 : 8;
		uint32_t magic1 : 2; //8 //constant?
		uint32_t axis_z : 6; //10

		uint32_t magic2 : 1;  //16 //constant?
		uint32_t axis_rz : 6; //17

		uint32_t magic3 : 1; //23

		uint32_t magic4 : 8; //constant
	};

	struct momo2_data_t
	{
		uint32_t pad0 : 8; //report id probably
		uint32_t axis_x : 10;
		uint32_t buttons : 10;
		uint32_t padding0 : 4; //32

		uint8_t padding1;
		uint8_t axis_z;
		uint8_t axis_rz;
		uint8_t padding2; //32
	};

	// DF or any LG wheel in non-native mode
	struct generic_data_t
	{
		uint32_t axis_x : 10;
		uint32_t buttons : 12;
		uint32_t pad0 : 2;   //vendor
		uint32_t axis_y : 8; //constant (0x7f on PC, 0xFF on console?)

		uint32_t hatswitch : 4;
		uint32_t pad1 : 4; //vendor
		uint32_t axis_z : 8;
		uint32_t axis_rz : 8;
		uint32_t pad2 : 8;
	};

	// GT Force?
	struct gtforce_data_t
	{
		uint32_t axis_x : 10;
		uint32_t buttons : 6;
		uint32_t pad0 : 8;
		uint32_t axis_y : 8;

		uint32_t axis_z : 8;
		uint32_t axis_rz : 8;

		uint32_t pad1 : 16;
	};

	struct random_data_t
	{
		uint32_t axis_x : 10;
		uint32_t buttons : 10;
		uint32_t pad1 : 12;

		uint32_t axis_y : 8; //constant
		uint32_t axis_z : 8;
		uint32_t axis_rz : 8;
		uint32_t pad2 : 8;
	};

	struct rb1drumkit_t
	{
		union u
		{
			uint16_t buttons;
			struct s
			{
				uint16_t blue : 1;
				uint16_t green : 1;
				uint16_t red : 1;
				uint16_t yellow : 1;
				uint16_t orange : 1;
				uint16_t something0 : 3;

				uint16_t select : 1;
				uint16_t start : 1;
				uint16_t something1 : 6;
			} s;
		} u;

		uint8_t hatswitch;
	};

	void pad_reset_data(generic_data_t* d);
	void pad_reset_data(dfp_data_t* d);
	void pad_copy_data(PS2WheelTypes type, uint8_t* buf, wheel_data_t& data);
	//Convert DF Pro buttons to selected wheel type
	uint32_t convert_wt_btn(PS2WheelTypes type, uint32_t inBtn);

} // namespace usb_pad
#endif
