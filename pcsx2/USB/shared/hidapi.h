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

#ifndef HIDAPI_H
#define HIDAPI_H

#include <pshpack4.h>

#define NTSTATUS int

typedef USHORT USAGE, *PUSAGE;

#define HID_USAGE_PAGE_GENERIC ((USAGE)0x01)
#define HID_USAGE_PAGE_SIMULATION ((USAGE)0x02)
#define HID_USAGE_PAGE_VR ((USAGE)0x03)
#define HID_USAGE_PAGE_SPORT ((USAGE)0x04)
#define HID_USAGE_PAGE_GAME ((USAGE)0x05)
#define HID_USAGE_PAGE_KEYBOARD ((USAGE)0x07)
#define HID_USAGE_PAGE_LED ((USAGE)0x08)
#define HID_USAGE_PAGE_BUTTON ((USAGE)0x09)
#define HID_USAGE_PAGE_ORDINAL ((USAGE)0x0A)
#define HID_USAGE_PAGE_TELEPHONY ((USAGE)0x0B)
#define HID_USAGE_PAGE_CONSUMER ((USAGE)0x0C)
#define HID_USAGE_PAGE_DIGITIZER ((USAGE)0x0D)
#define HID_USAGE_PAGE_UNICODE ((USAGE)0x10)
#define HID_USAGE_PAGE_ALPHANUMERIC ((USAGE)0x14)


//
// Usages from Generic Desktop Page (0x01)
//

#define HID_USAGE_GENERIC_POINTER ((USAGE)0x01)
#define HID_USAGE_GENERIC_MOUSE ((USAGE)0x02)
#define HID_USAGE_GENERIC_JOYSTICK ((USAGE)0x04)
#define HID_USAGE_GENERIC_GAMEPAD ((USAGE)0x05)
#define HID_USAGE_GENERIC_KEYBOARD ((USAGE)0x06)
#define HID_USAGE_GENERIC_KEYPAD ((USAGE)0x07)
#define HID_USAGE_GENERIC_SYSTEM_CTL ((USAGE)0x80)

#define HID_USAGE_GENERIC_X ((USAGE)0x30)
#define HID_USAGE_GENERIC_Y ((USAGE)0x31)
#define HID_USAGE_GENERIC_Z ((USAGE)0x32)
#define HID_USAGE_GENERIC_RX ((USAGE)0x33)
#define HID_USAGE_GENERIC_RY ((USAGE)0x34)
#define HID_USAGE_GENERIC_RZ ((USAGE)0x35)
#define HID_USAGE_GENERIC_SLIDER ((USAGE)0x36)
#define HID_USAGE_GENERIC_DIAL ((USAGE)0x37)
#define HID_USAGE_GENERIC_WHEEL ((USAGE)0x38)
#define HID_USAGE_GENERIC_HATSWITCH ((USAGE)0x39)
#define HID_USAGE_GENERIC_COUNTED_BUFFER ((USAGE)0x3A)
#define HID_USAGE_GENERIC_BYTE_COUNT ((USAGE)0x3B)
#define HID_USAGE_GENERIC_MOTION_WAKEUP ((USAGE)0x3C)
#define HID_USAGE_GENERIC_VX ((USAGE)0x40)
#define HID_USAGE_GENERIC_VY ((USAGE)0x41)
#define HID_USAGE_GENERIC_VZ ((USAGE)0x42)
#define HID_USAGE_GENERIC_VBRX ((USAGE)0x43)
#define HID_USAGE_GENERIC_VBRY ((USAGE)0x44)
#define HID_USAGE_GENERIC_VBRZ ((USAGE)0x45)
#define HID_USAGE_GENERIC_VNO ((USAGE)0x46)
#define HID_USAGE_GENERIC_SYSCTL_POWER ((USAGE)0x81)
#define HID_USAGE_GENERIC_SYSCTL_SLEEP ((USAGE)0x82)
#define HID_USAGE_GENERIC_SYSCTL_WAKE ((USAGE)0x83)
#define HID_USAGE_GENERIC_SYSCTL_CONTEXT_MENU ((USAGE)0x84)
#define HID_USAGE_GENERIC_SYSCTL_MAIN_MENU ((USAGE)0x85)
#define HID_USAGE_GENERIC_SYSCTL_APP_MENU ((USAGE)0x86)
#define HID_USAGE_GENERIC_SYSCTL_HELP_MENU ((USAGE)0x87)
#define HID_USAGE_GENERIC_SYSCTL_MENU_EXIT ((USAGE)0x88)
#define HID_USAGE_GENERIC_SYSCTL_MENU_SELECT ((USAGE)0x89)
#define HID_USAGE_GENERIC_SYSCTL_MENU_RIGHT ((USAGE)0x8A)
#define HID_USAGE_GENERIC_SYSCTL_MENU_LEFT ((USAGE)0x8B)
#define HID_USAGE_GENERIC_SYSCTL_MENU_UP ((USAGE)0x8C)
#define HID_USAGE_GENERIC_SYSCTL_MENU_DOWN ((USAGE)0x8D)

//
// Usages from Simulation Controls Page (0x02)
//

#define HID_USAGE_SIMULATION_RUDDER ((USAGE)0xBA)
#define HID_USAGE_SIMULATION_THROTTLE ((USAGE)0xBB)

//
// Virtual Reality Controls Page (0x03)
//


//
// Sport Controls Page (0x04)
//


//
// Game Controls Page (0x05)
//


//
// Keyboard/Keypad Page (0x07)
//

// Error "keys"
#define HID_USAGE_KEYBOARD_NOEVENT ((USAGE)0x00)
#define HID_USAGE_KEYBOARD_ROLLOVER ((USAGE)0x01)
#define HID_USAGE_KEYBOARD_POSTFAIL ((USAGE)0x02)
#define HID_USAGE_KEYBOARD_UNDEFINED ((USAGE)0x03)

// Letters
#define HID_USAGE_KEYBOARD_aA ((USAGE)0x04)
#define HID_USAGE_KEYBOARD_zZ ((USAGE)0x1D)
// Numbers
#define HID_USAGE_KEYBOARD_ONE ((USAGE)0x1E)
#define HID_USAGE_KEYBOARD_ZERO ((USAGE)0x27)
// Modifier Keys
#define HID_USAGE_KEYBOARD_LCTRL ((USAGE)0xE0)
#define HID_USAGE_KEYBOARD_LSHFT ((USAGE)0xE1)
#define HID_USAGE_KEYBOARD_LALT ((USAGE)0xE2)
#define HID_USAGE_KEYBOARD_LGUI ((USAGE)0xE3)
#define HID_USAGE_KEYBOARD_RCTRL ((USAGE)0xE4)
#define HID_USAGE_KEYBOARD_RSHFT ((USAGE)0xE5)
#define HID_USAGE_KEYBOARD_RALT ((USAGE)0xE6)
#define HID_USAGE_KEYBOARD_RGUI ((USAGE)0xE7)
#define HID_USAGE_KEYBOARD_SCROLL_LOCK ((USAGE)0x47)
#define HID_USAGE_KEYBOARD_NUM_LOCK ((USAGE)0x53)
#define HID_USAGE_KEYBOARD_CAPS_LOCK ((USAGE)0x39)
// Funtion keys
#define HID_USAGE_KEYBOARD_F1 ((USAGE)0x3A)
#define HID_USAGE_KEYBOARD_F12 ((USAGE)0x45)

#define HID_USAGE_KEYBOARD_RETURN ((USAGE)0x28)
#define HID_USAGE_KEYBOARD_ESCAPE ((USAGE)0x29)
#define HID_USAGE_KEYBOARD_DELETE ((USAGE)0x2A)

#define HID_USAGE_KEYBOARD_PRINT_SCREEN ((USAGE)0x46)

// and hundreds more...

//
// LED Page (0x08)
//

#define HID_USAGE_LED_NUM_LOCK ((USAGE)0x01)
#define HID_USAGE_LED_CAPS_LOCK ((USAGE)0x02)
#define HID_USAGE_LED_SCROLL_LOCK ((USAGE)0x03)
#define HID_USAGE_LED_COMPOSE ((USAGE)0x04)
#define HID_USAGE_LED_KANA ((USAGE)0x05)
#define HID_USAGE_LED_POWER ((USAGE)0x06)
#define HID_USAGE_LED_SHIFT ((USAGE)0x07)
#define HID_USAGE_LED_DO_NOT_DISTURB ((USAGE)0x08)
#define HID_USAGE_LED_MUTE ((USAGE)0x09)
#define HID_USAGE_LED_TONE_ENABLE ((USAGE)0x0A)
#define HID_USAGE_LED_HIGH_CUT_FILTER ((USAGE)0x0B)
#define HID_USAGE_LED_LOW_CUT_FILTER ((USAGE)0x0C)
#define HID_USAGE_LED_EQUALIZER_ENABLE ((USAGE)0x0D)
#define HID_USAGE_LED_SOUND_FIELD_ON ((USAGE)0x0E)
#define HID_USAGE_LED_SURROUND_FIELD_ON ((USAGE)0x0F)
#define HID_USAGE_LED_REPEAT ((USAGE)0x10)
#define HID_USAGE_LED_STEREO ((USAGE)0x11)
#define HID_USAGE_LED_SAMPLING_RATE_DETECT ((USAGE)0x12)
#define HID_USAGE_LED_SPINNING ((USAGE)0x13)
#define HID_USAGE_LED_CAV ((USAGE)0x14)
#define HID_USAGE_LED_CLV ((USAGE)0x15)
#define HID_USAGE_LED_RECORDING_FORMAT_DET ((USAGE)0x16)
#define HID_USAGE_LED_OFF_HOOK ((USAGE)0x17)
#define HID_USAGE_LED_RING ((USAGE)0x18)
#define HID_USAGE_LED_MESSAGE_WAITING ((USAGE)0x19)
#define HID_USAGE_LED_DATA_MODE ((USAGE)0x1A)
#define HID_USAGE_LED_BATTERY_OPERATION ((USAGE)0x1B)
#define HID_USAGE_LED_BATTERY_OK ((USAGE)0x1C)
#define HID_USAGE_LED_BATTERY_LOW ((USAGE)0x1D)
#define HID_USAGE_LED_SPEAKER ((USAGE)0x1E)
#define HID_USAGE_LED_HEAD_SET ((USAGE)0x1F)
#define HID_USAGE_LED_HOLD ((USAGE)0x20)
#define HID_USAGE_LED_MICROPHONE ((USAGE)0x21)
#define HID_USAGE_LED_COVERAGE ((USAGE)0x22)
#define HID_USAGE_LED_NIGHT_MODE ((USAGE)0x23)
#define HID_USAGE_LED_SEND_CALLS ((USAGE)0x24)
#define HID_USAGE_LED_CALL_PICKUP ((USAGE)0x25)
#define HID_USAGE_LED_CONFERENCE ((USAGE)0x26)
#define HID_USAGE_LED_STAND_BY ((USAGE)0x27)
#define HID_USAGE_LED_CAMERA_ON ((USAGE)0x28)
#define HID_USAGE_LED_CAMERA_OFF ((USAGE)0x29)
#define HID_USAGE_LED_ON_LINE ((USAGE)0x2A)
#define HID_USAGE_LED_OFF_LINE ((USAGE)0x2B)
#define HID_USAGE_LED_BUSY ((USAGE)0x2C)
#define HID_USAGE_LED_READY ((USAGE)0x2D)
#define HID_USAGE_LED_PAPER_OUT ((USAGE)0x2E)
#define HID_USAGE_LED_PAPER_JAM ((USAGE)0x2F)
#define HID_USAGE_LED_REMOTE ((USAGE)0x30)
#define HID_USAGE_LED_FORWARD ((USAGE)0x31)
#define HID_USAGE_LED_REVERSE ((USAGE)0x32)
#define HID_USAGE_LED_STOP ((USAGE)0x33)
#define HID_USAGE_LED_REWIND ((USAGE)0x34)
#define HID_USAGE_LED_FAST_FORWARD ((USAGE)0x35)
#define HID_USAGE_LED_PLAY ((USAGE)0x36)
#define HID_USAGE_LED_PAUSE ((USAGE)0x37)
#define HID_USAGE_LED_RECORD ((USAGE)0x38)
#define HID_USAGE_LED_ERROR ((USAGE)0x39)
#define HID_USAGE_LED_SELECTED_INDICATOR ((USAGE)0x3A)
#define HID_USAGE_LED_IN_USE_INDICATOR ((USAGE)0x3B)
#define HID_USAGE_LED_MULTI_MODE_INDICATOR ((USAGE)0x3C)
#define HID_USAGE_LED_INDICATOR_ON ((USAGE)0x3D)
#define HID_USAGE_LED_INDICATOR_FLASH ((USAGE)0x3E)
#define HID_USAGE_LED_INDICATOR_SLOW_BLINK ((USAGE)0x3F)
#define HID_USAGE_LED_INDICATOR_FAST_BLINK ((USAGE)0x40)
#define HID_USAGE_LED_INDICATOR_OFF ((USAGE)0x41)
#define HID_USAGE_LED_FLASH_ON_TIME ((USAGE)0x42)
#define HID_USAGE_LED_SLOW_BLINK_ON_TIME ((USAGE)0x43)
#define HID_USAGE_LED_SLOW_BLINK_OFF_TIME ((USAGE)0x44)
#define HID_USAGE_LED_FAST_BLINK_ON_TIME ((USAGE)0x45)
#define HID_USAGE_LED_FAST_BLINK_OFF_TIME ((USAGE)0x46)
#define HID_USAGE_LED_INDICATOR_COLOR ((USAGE)0x47)
#define HID_USAGE_LED_RED ((USAGE)0x48)
#define HID_USAGE_LED_GREEN ((USAGE)0x49)
#define HID_USAGE_LED_AMBER ((USAGE)0x4A)
#define HID_USAGE_LED_GENERIC_INDICATOR ((USAGE)0x3B)

//
//  Button Page (0x09)
//
//  There is no need to label these usages.
//


//
//  Ordinal Page (0x0A)
//
//  There is no need to label these usages.
//


//
//  Telephony Device Page (0x0B)
//

#define HID_USAGE_TELEPHONY_PHONE ((USAGE)0x01)
#define HID_USAGE_TELEPHONY_ANSWERING_MACHINE ((USAGE)0x02)
#define HID_USAGE_TELEPHONY_MESSAGE_CONTROLS ((USAGE)0x03)
#define HID_USAGE_TELEPHONY_HANDSET ((USAGE)0x04)
#define HID_USAGE_TELEPHONY_HEADSET ((USAGE)0x05)
#define HID_USAGE_TELEPHONY_KEYPAD ((USAGE)0x06)
#define HID_USAGE_TELEPHONY_PROGRAMMABLE_BUTTON ((USAGE)0x07)

// BUGBUG defined in ntstatus.h
#ifndef FACILITY_HID_ERROR_CODE
#define FACILITY_HID_ERROR_CODE 0x11
#endif

#define HIDP_ERROR_CODES(SEV, CODE) \
	((NTSTATUS)(((SEV) << 28) | (FACILITY_HID_ERROR_CODE << 16) | (CODE)))

#define HIDP_STATUS_SUCCESS (HIDP_ERROR_CODES(0x0, 0))
#define HIDP_STATUS_NULL (HIDP_ERROR_CODES(0x8, 1))
#define HIDP_STATUS_INVALID_PREPARSED_DATA (HIDP_ERROR_CODES(0xC, 1))
#define HIDP_STATUS_INVALID_REPORT_TYPE (HIDP_ERROR_CODES(0xC, 2))
#define HIDP_STATUS_INVALID_REPORT_LENGTH (HIDP_ERROR_CODES(0xC, 3))
#define HIDP_STATUS_USAGE_NOT_FOUND (HIDP_ERROR_CODES(0xC, 4))
#define HIDP_STATUS_VALUE_OUT_OF_RANGE (HIDP_ERROR_CODES(0xC, 5))
#define HIDP_STATUS_BAD_LOG_PHY_VALUES (HIDP_ERROR_CODES(0xC, 6))
#define HIDP_STATUS_BUFFER_TOO_SMALL (HIDP_ERROR_CODES(0xC, 7))
#define HIDP_STATUS_INTERNAL_ERROR (HIDP_ERROR_CODES(0xC, 8))
#define HIDP_STATUS_I8242_TRANS_UNKNOWN (HIDP_ERROR_CODES(0xC, 9))
#define HIDP_STATUS_INCOMPATIBLE_REPORT_ID (HIDP_ERROR_CODES(0xC, 0xA))
#define HIDP_STATUS_NOT_VALUE_ARRAY (HIDP_ERROR_CODES(0xC, 0xB))
#define HIDP_STATUS_IS_VALUE_ARRAY (HIDP_ERROR_CODES(0xC, 0xC))
#define HIDP_STATUS_DATA_INDEX_NOT_FOUND (HIDP_ERROR_CODES(0xC, 0xD))
#define HIDP_STATUS_DATA_INDEX_OUT_OF_RANGE (HIDP_ERROR_CODES(0xC, 0xE))
#define HIDP_STATUS_BUTTON_NOT_PRESSED (HIDP_ERROR_CODES(0xC, 0xF))
#define HIDP_STATUS_REPORT_DOES_NOT_EXIST (HIDP_ERROR_CODES(0xC, 0x10))
#define HIDP_STATUS_NOT_IMPLEMENTED (HIDP_ERROR_CODES(0xC, 0x20))

typedef enum _HIDP_REPORT_TYPE
{
	HidP_Input,
	HidP_Output,
	HidP_Feature
} HIDP_REPORT_TYPE;

typedef struct _USAGE_AND_PAGE
{
	USAGE Usage;
	USAGE UsagePage;
} USAGE_AND_PAGE, *PUSAGE_AND_PAGE;

typedef struct _HIDP_BUTTON_CAPS
{
	USAGE UsagePage;
	UCHAR ReportID;
	BOOLEAN IsAlias;

	USHORT BitField;
	USHORT LinkCollection; // A unique internal index pointer

	USAGE LinkUsage;
	USAGE LinkUsagePage;

	BOOLEAN IsRange;
	BOOLEAN IsStringRange;
	BOOLEAN IsDesignatorRange;
	BOOLEAN IsAbsolute;

	ULONG Reserved[10];
	union
	{
		struct
		{
			USAGE UsageMin, UsageMax;
			USHORT StringMin, StringMax;
			USHORT DesignatorMin, DesignatorMax;
			USHORT DataIndexMin, DataIndexMax;
		} Range;
		struct
		{
			USAGE Usage, Reserved1;
			USHORT StringIndex, Reserved2;
			USHORT DesignatorIndex, Reserved3;
			USHORT DataIndex, Reserved4;
		} NotRange;
	};

} HIDP_BUTTON_CAPS, *PHIDP_BUTTON_CAPS;


typedef struct _HIDP_VALUE_CAPS
{
	USAGE UsagePage;
	UCHAR ReportID;
	BOOLEAN IsAlias;

	USHORT BitField;
	USHORT LinkCollection; // A unique internal index pointer

	USAGE LinkUsage;
	USAGE LinkUsagePage;

	BOOLEAN IsRange;
	BOOLEAN IsStringRange;
	BOOLEAN IsDesignatorRange;
	BOOLEAN IsAbsolute;

	BOOLEAN HasNull; // Does this channel have a null report   union
	UCHAR Reserved;
	USHORT BitSize; // How many bits are devoted to this value?

	USHORT ReportCount; // See Note below.  Usually set to 1.
	USHORT Reserved2[5];

	ULONG UnitsExp;
	ULONG Units;

	LONG LogicalMin, LogicalMax;
	LONG PhysicalMin, PhysicalMax;

	union
	{
		struct
		{
			USAGE UsageMin, UsageMax;
			USHORT StringMin, StringMax;
			USHORT DesignatorMin, DesignatorMax;
			USHORT DataIndexMin, DataIndexMax;
		} Range;

		struct
		{
			USAGE Usage, Reserved1;
			USHORT StringIndex, Reserved2;
			USHORT DesignatorIndex, Reserved3;
			USHORT DataIndex, Reserved4;
		} NotRange;
	};
} HIDP_VALUE_CAPS, *PHIDP_VALUE_CAPS;

typedef struct _HIDD_ATTRIBUTES
{
	ULONG Size; // = sizeof (struct _HIDD_ATTRIBUTES)

	//
	// Vendor ids of this hid device
	//
	USHORT VendorID;
	USHORT ProductID;
	USHORT VersionNumber;

	//
	// Additional fields will be added to the end of this structure.
	//
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

typedef PUCHAR PHIDP_REPORT_DESCRIPTOR;
typedef struct _HIDP_PREPARSED_DATA* PHIDP_PREPARSED_DATA;


typedef struct _HIDP_CAPS
{
	USAGE Usage;
	USAGE UsagePage;
	USHORT InputReportByteLength;
	USHORT OutputReportByteLength;
	USHORT FeatureReportByteLength;
	USHORT Reserved[17];

	USHORT NumberLinkCollectionNodes;

	USHORT NumberInputButtonCaps;
	USHORT NumberInputValueCaps;
	USHORT NumberInputDataIndices;

	USHORT NumberOutputButtonCaps;
	USHORT NumberOutputValueCaps;
	USHORT NumberOutputDataIndices;

	USHORT NumberFeatureButtonCaps;
	USHORT NumberFeatureValueCaps;
	USHORT NumberFeatureDataIndices;
} HIDP_CAPS, *PHIDP_CAPS;

typedef struct _HIDP_DATA
{
	USHORT DataIndex;
	USHORT Reserved;
	union
	{
		ULONG RawValue; // for values
		BOOLEAN On;     // for buttons MUST BE TRUE for buttons.
	};
} HIDP_DATA, *PHIDP_DATA;

typedef BOOLEAN(__stdcall* _HidD_GetAttributes)(HANDLE HidDeviceObject, HIDD_ATTRIBUTES* Attributes);
typedef void(__stdcall* _HidD_GetHidGuid)(GUID* HidGuid);
typedef BOOLEAN(__stdcall* _HidD_GetPreparsedData)(HANDLE HidDeviceObject, PHIDP_PREPARSED_DATA* PreparsedData);
typedef NTSTATUS(__stdcall* _HidP_GetCaps)(PHIDP_PREPARSED_DATA PreparsedData, HIDP_CAPS* caps);
typedef BOOLEAN(__stdcall* _HidD_FreePreparsedData)(PHIDP_PREPARSED_DATA PreparsedData);
typedef BOOLEAN(__stdcall* _HidD_GetFeature)(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);
typedef BOOLEAN(__stdcall* _HidD_SetFeature)(HANDLE HidDeviceObject, PVOID ReportBuffer, ULONG ReportBufferLength);
typedef NTSTATUS(__stdcall* _HidP_GetSpecificButtonCaps)(HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection, USAGE Usage, PHIDP_BUTTON_CAPS ButtonCaps, PUSHORT ButtonCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
typedef NTSTATUS(__stdcall* _HidP_GetButtonCaps)(HIDP_REPORT_TYPE ReportType, PHIDP_BUTTON_CAPS ButtonCaps, PUSHORT ButtonCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
typedef NTSTATUS(__stdcall* _HidP_GetUsages)(HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection, USAGE* UsageList, ULONG* UsageLength, PHIDP_PREPARSED_DATA PreparsedData, PCHAR Report, ULONG ReportLength);
typedef NTSTATUS(__stdcall* _HidP_GetValueCaps)(HIDP_REPORT_TYPE ReportType, PHIDP_VALUE_CAPS ValueCaps, PUSHORT ValueCapsLength, PHIDP_PREPARSED_DATA PreparsedData);
typedef NTSTATUS(__stdcall* _HidP_GetUsageValue)(HIDP_REPORT_TYPE ReportType, USAGE UsagePage, USHORT LinkCollection, USAGE Usage, PULONG UsageValue, PHIDP_PREPARSED_DATA PreparsedData, PCHAR Report, ULONG ReportLength);
typedef BOOLEAN(__stdcall* _HidD_GetProductString)(HANDLE HidDeviceObject, PVOID Buffer, ULONG BufferLength);

//#define HidP_GetButtonCaps(_Type_, _Caps_, _Len_, _Data_) \
//    HidP_GetSpecificButtonCaps(_Type_, 0, 0, 0, _Caps_, _Len_, _Data_)

extern _HidD_GetHidGuid HidD_GetHidGuid;
extern _HidD_GetAttributes HidD_GetAttributes;
extern _HidD_GetPreparsedData HidD_GetPreparsedData;
extern _HidP_GetCaps HidP_GetCaps;
extern _HidD_FreePreparsedData HidD_FreePreparsedData;
extern _HidD_GetFeature HidD_GetFeature;
extern _HidD_SetFeature HidD_SetFeature;
extern _HidP_GetSpecificButtonCaps HidP_GetSpecificButtonCaps;
extern _HidP_GetButtonCaps HidP_GetButtonCaps;
extern _HidP_GetUsages HidP_GetUsages;
extern _HidP_GetValueCaps HidP_GetValueCaps;
extern _HidP_GetUsageValue HidP_GetUsageValue;
extern _HidD_GetProductString HidD_GetProductString;

void UninitHid();
int InitHid();

#include <poppack.h>

#endif
