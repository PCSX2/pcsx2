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

#ifndef QEMU_HID_H
#define QEMU_HID_H
#include "vl.h"

/* include/ui/console.h */
/* keyboard/mouse support */

#define MOUSE_EVENT_LBUTTON 0x01
#define MOUSE_EVENT_RBUTTON 0x02
#define MOUSE_EVENT_MBUTTON 0x04
/* identical to the ps/2 keyboard bits */
#define QEMU_SCROLL_LOCK_LED (1 << 0)
#define QEMU_NUM_LOCK_LED (1 << 1)
#define QEMU_CAPS_LOCK_LED (1 << 2)

#define HID_MOUSE 1
#define HID_TABLET 2
#define HID_KEYBOARD 3
// idk
#define HID_SUBKIND_BEATMANIA 1

/* scancode without modifiers */
#define SCANCODE_KEYMASK 0xff
/* scancode without grey or up bit */
#define SCANCODE_KEYCODEMASK 0x7f

/* "grey" keys will usually need a 0xe0 prefix */
#define SCANCODE_GREY 0x80
#define SCANCODE_EMUL0 0xE0
#define SCANCODE_EMUL1 0xE1
/* "up" flag */
#define SCANCODE_UP 0x80

/* Additional modifiers to use if not catched another way. */
#define SCANCODE_SHIFT 0x100
#define SCANCODE_CTRL 0x200
#define SCANCODE_ALT 0x400
#define SCANCODE_ALTGR 0x800

typedef enum QKeyCode
{
	Q_KEY_CODE_UNMAPPED = 0,
	Q_KEY_CODE_SHIFT = 1,
	Q_KEY_CODE_SHIFT_R = 2,
	Q_KEY_CODE_ALT = 3,
	Q_KEY_CODE_ALT_R = 4,
	Q_KEY_CODE_CTRL = 5,
	Q_KEY_CODE_CTRL_R = 6,
	Q_KEY_CODE_MENU = 7,
	Q_KEY_CODE_ESC = 8,
	Q_KEY_CODE_1 = 9,
	Q_KEY_CODE_2 = 10,
	Q_KEY_CODE_3 = 11,
	Q_KEY_CODE_4 = 12,
	Q_KEY_CODE_5 = 13,
	Q_KEY_CODE_6 = 14,
	Q_KEY_CODE_7 = 15,
	Q_KEY_CODE_8 = 16,
	Q_KEY_CODE_9 = 17,
	Q_KEY_CODE_0 = 18,
	Q_KEY_CODE_MINUS = 19,
	Q_KEY_CODE_EQUAL = 20,
	Q_KEY_CODE_BACKSPACE = 21,
	Q_KEY_CODE_TAB = 22,
	Q_KEY_CODE_Q = 23,
	Q_KEY_CODE_W = 24,
	Q_KEY_CODE_E = 25,
	Q_KEY_CODE_R = 26,
	Q_KEY_CODE_T = 27,
	Q_KEY_CODE_Y = 28,
	Q_KEY_CODE_U = 29,
	Q_KEY_CODE_I = 30,
	Q_KEY_CODE_O = 31,
	Q_KEY_CODE_P = 32,
	Q_KEY_CODE_BRACKET_LEFT = 33,
	Q_KEY_CODE_BRACKET_RIGHT = 34,
	Q_KEY_CODE_RET = 35,
	Q_KEY_CODE_A = 36,
	Q_KEY_CODE_S = 37,
	Q_KEY_CODE_D = 38,
	Q_KEY_CODE_F = 39,
	Q_KEY_CODE_G = 40,
	Q_KEY_CODE_H = 41,
	Q_KEY_CODE_J = 42,
	Q_KEY_CODE_K = 43,
	Q_KEY_CODE_L = 44,
	Q_KEY_CODE_SEMICOLON = 45,
	Q_KEY_CODE_APOSTROPHE = 46,
	Q_KEY_CODE_GRAVE_ACCENT = 47,
	Q_KEY_CODE_BACKSLASH = 48,
	Q_KEY_CODE_Z = 49,
	Q_KEY_CODE_X = 50,
	Q_KEY_CODE_C = 51,
	Q_KEY_CODE_V = 52,
	Q_KEY_CODE_B = 53,
	Q_KEY_CODE_N = 54,
	Q_KEY_CODE_M = 55,
	Q_KEY_CODE_COMMA = 56,
	Q_KEY_CODE_DOT = 57,
	Q_KEY_CODE_SLASH = 58,
	Q_KEY_CODE_ASTERISK = 59,
	Q_KEY_CODE_SPC = 60,
	Q_KEY_CODE_CAPS_LOCK = 61,
	Q_KEY_CODE_F1 = 62,
	Q_KEY_CODE_F2 = 63,
	Q_KEY_CODE_F3 = 64,
	Q_KEY_CODE_F4 = 65,
	Q_KEY_CODE_F5 = 66,
	Q_KEY_CODE_F6 = 67,
	Q_KEY_CODE_F7 = 68,
	Q_KEY_CODE_F8 = 69,
	Q_KEY_CODE_F9 = 70,
	Q_KEY_CODE_F10 = 71,
	Q_KEY_CODE_NUM_LOCK = 72,
	Q_KEY_CODE_SCROLL_LOCK = 73,
	Q_KEY_CODE_KP_DIVIDE = 74,
	Q_KEY_CODE_KP_MULTIPLY = 75,
	Q_KEY_CODE_KP_SUBTRACT = 76,
	Q_KEY_CODE_KP_ADD = 77,
	Q_KEY_CODE_KP_ENTER = 78,
	Q_KEY_CODE_KP_DECIMAL = 79,
	Q_KEY_CODE_SYSRQ = 80,
	Q_KEY_CODE_KP_0 = 81,
	Q_KEY_CODE_KP_1 = 82,
	Q_KEY_CODE_KP_2 = 83,
	Q_KEY_CODE_KP_3 = 84,
	Q_KEY_CODE_KP_4 = 85,
	Q_KEY_CODE_KP_5 = 86,
	Q_KEY_CODE_KP_6 = 87,
	Q_KEY_CODE_KP_7 = 88,
	Q_KEY_CODE_KP_8 = 89,
	Q_KEY_CODE_KP_9 = 90,
	Q_KEY_CODE_LESS = 91,
	Q_KEY_CODE_F11 = 92,
	Q_KEY_CODE_F12 = 93,
	Q_KEY_CODE_PRINT = 94,
	Q_KEY_CODE_HOME = 95,
	Q_KEY_CODE_PGUP = 96,
	Q_KEY_CODE_PGDN = 97,
	Q_KEY_CODE_END = 98,
	Q_KEY_CODE_LEFT = 99,
	Q_KEY_CODE_UP = 100,
	Q_KEY_CODE_DOWN = 101,
	Q_KEY_CODE_RIGHT = 102,
	Q_KEY_CODE_INSERT = 103,
	Q_KEY_CODE_DELETE = 104,
	Q_KEY_CODE_STOP = 105,
	Q_KEY_CODE_AGAIN = 106,
	Q_KEY_CODE_PROPS = 107,
	Q_KEY_CODE_UNDO = 108,
	Q_KEY_CODE_FRONT = 109,
	Q_KEY_CODE_COPY = 110,
	Q_KEY_CODE_OPEN = 111,
	Q_KEY_CODE_PASTE = 112,
	Q_KEY_CODE_FIND = 113,
	Q_KEY_CODE_CUT = 114,
	Q_KEY_CODE_LF = 115,
	Q_KEY_CODE_HELP = 116,
	Q_KEY_CODE_META_L = 117,
	Q_KEY_CODE_META_R = 118,
	Q_KEY_CODE_COMPOSE = 119,
	Q_KEY_CODE_PAUSE = 120,
	Q_KEY_CODE_RO = 121,
	Q_KEY_CODE_HIRAGANA = 122,
	Q_KEY_CODE_HENKAN = 123,
	Q_KEY_CODE_YEN = 124,
	Q_KEY_CODE_MUHENKAN = 125,
	Q_KEY_CODE_KATAKANAHIRAGANA = 126,
	Q_KEY_CODE_KP_COMMA = 127,
	Q_KEY_CODE_KP_EQUALS = 128,
	Q_KEY_CODE_POWER = 129,
	Q_KEY_CODE_SLEEP = 130,
	Q_KEY_CODE_WAKE = 131,
	Q_KEY_CODE_AUDIONEXT = 132,
	Q_KEY_CODE_AUDIOPREV = 133,
	Q_KEY_CODE_AUDIOSTOP = 134,
	Q_KEY_CODE_AUDIOPLAY = 135,
	Q_KEY_CODE_AUDIOMUTE = 136,
	Q_KEY_CODE_VOLUMEUP = 137,
	Q_KEY_CODE_VOLUMEDOWN = 138,
	Q_KEY_CODE_MEDIASELECT = 139,
	Q_KEY_CODE_MAIL = 140,
	Q_KEY_CODE_CALCULATOR = 141,
	Q_KEY_CODE_COMPUTER = 142,
	Q_KEY_CODE_AC_HOME = 143,
	Q_KEY_CODE_AC_BACK = 144,
	Q_KEY_CODE_AC_FORWARD = 145,
	Q_KEY_CODE_AC_REFRESH = 146,
	Q_KEY_CODE_AC_BOOKMARKS = 147,
	Q_KEY_CODE__MAX = 148,
} QKeyCode;

typedef enum InputEventKind
{
	INPUT_EVENT_KIND_KEY = 0,
	INPUT_EVENT_KIND_BTN = 1,
	INPUT_EVENT_KIND_REL = 2,
	INPUT_EVENT_KIND_ABS = 3,
	INPUT_EVENT_KIND__MAX = 4,
} InputEventKind;

typedef enum KeyValueKind
{
	KEY_VALUE_KIND_NUMBER = 0,
	KEY_VALUE_KIND_QCODE = 1,
	KEY_VALUE_KIND__MAX = 2,
} KeyValueKind;

typedef enum InputAxis
{
	INPUT_AXIS_X = 0,
	INPUT_AXIS_Y = 1,
	INPUT_AXIS__MAX = 2,
} InputAxis;

typedef enum InputButton
{
	INPUT_BUTTON_LEFT = 0,
	INPUT_BUTTON_MIDDLE = 1,
	INPUT_BUTTON_RIGHT = 2,
	INPUT_BUTTON_WHEEL_UP = 3,
	INPUT_BUTTON_WHEEL_DOWN = 4,
	INPUT_BUTTON_SIDE = 5,
	INPUT_BUTTON_EXTRA = 6,
	INPUT_BUTTON__MAX = 7,
} InputButton;

struct KeyValue
{
	KeyValueKind type;
	union
	{
		int number;
		QKeyCode qcode;
	} u;
};

struct InputKeyEvent
{
	KeyValue key;
	bool down;
};

struct InputBtnEvent
{
	InputButton button;
	bool down;
};

struct InputMoveEvent
{
	InputAxis axis;
	int64_t value;
};

struct InputEvent
{
	InputEventKind type;
	union
	{
		InputKeyEvent key;
		InputBtnEvent btn;
		InputMoveEvent rel;
		InputMoveEvent abs;
	} u;
};

typedef struct HIDState HIDState;
typedef void (*HIDEventFunc)(HIDState* s);

typedef void QEMUPutKBDEvent(HIDState* hs, InputEvent* evt);
typedef void QEMUPutLEDEvent(void* opaque, int ledstate);
typedef void QEMUPutMouseEvent(HIDState* hs, InputEvent* evt);

typedef struct HIDPointerEvent
{
	int32_t xdx, ydy; /* relative if it's a mouse, otherwise absolute */
	int32_t dz, buttons_state;
} HIDPointerEvent;

#define QUEUE_LENGTH 16 /* should be enough for a triple-click */
#define QUEUE_MASK (QUEUE_LENGTH - 1u)
#define QUEUE_INCR(v) ((v)++, (v) &= QUEUE_MASK)

typedef struct HIDMouseState
{
	HIDPointerEvent queue[QUEUE_LENGTH];
	int mouse_grabbed;
	QEMUPutMouseEvent* eh_entry;
	HIDEventFunc eh_sync;
} HIDMouseState;

typedef struct HIDKeyboardState
{
	uint32_t keycodes[QUEUE_LENGTH];
	uint16_t modifiers;
	uint8_t leds;
	uint8_t key[16];
	int32_t keys;
	QEMUPutKBDEvent* eh_entry;
} HIDKeyboardState;

struct HIDState
{
	union
	{
		HIDMouseState ptr;
		HIDKeyboardState kbd;
	};
	uint32_t head; /* index into circular queue */
	uint32_t n;
	int kind;
	int sub_kind;
	int32_t protocol;
	uint8_t idle;
	bool idle_pending;
	//QEMUTimer *idle_timer;
	HIDEventFunc event;
};

void hid_init(HIDState* hs, int kind, HIDEventFunc event);
void hid_reset(HIDState* hs);
void hid_free(HIDState* hs);

bool hid_has_events(HIDState* hs);
void hid_set_next_idle(HIDState* hs);
void hid_pointer_activate(HIDState* hs);
int hid_pointer_poll(HIDState* hs, uint8_t* buf, int len);
int hid_keyboard_poll(HIDState* hs, uint8_t* buf, int len);
int hid_keyboard_write(HIDState* hs, uint8_t* buf, int len);

#endif /* QEMU_HID_H */
