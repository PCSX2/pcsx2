/*
* QEMU HID devices
*
* Copyright (c) 2005 Fabrice Bellard
* Copyright (c) 2007 OpenMoko, Inc.  (andrew@openedhand.com)
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "PrecompiledHeader.h"
#include "hid.h"
#include "input-keymap.h"

#define HID_USAGE_ERROR_ROLLOVER 0x01
#define HID_USAGE_POSTFAIL 0x02
#define HID_USAGE_ERROR_UNDEFINED 0x03

/* Indices are QEMU keycodes, values are from HID Usage Table.  Indices
* above 0x80 are for keys that come after 0xe0 or 0xe1+0x1d or 0xe1+0x9d.  */
static const uint8_t hid_usage_keys[0x100] = {
	0x00,
	0x29,
	0x1e,
	0x1f,
	0x20,
	0x21,
	0x22,
	0x23,
	0x24,
	0x25,
	0x26,
	0x27,
	0x2d,
	0x2e,
	0x2a,
	0x2b,
	0x14,
	0x1a,
	0x08,
	0x15,
	0x17,
	0x1c,
	0x18,
	0x0c,
	0x12,
	0x13,
	0x2f,
	0x30,
	0x28,
	0xe0,
	0x04,
	0x16,
	0x07,
	0x09,
	0x0a,
	0x0b,
	0x0d,
	0x0e,
	0x0f,
	0x33,
	0x34,
	0x35,
	0xe1,
	0x31,
	0x1d,
	0x1b,
	0x06,
	0x19,
	0x05,
	0x11,
	0x10,
	0x36,
	0x37,
	0x38,
	0xe5,
	0x55,
	0xe2,
	0x2c,
	0x39,
	0x3a,
	0x3b,
	0x3c,
	0x3d,
	0x3e,
	0x3f,
	0x40,
	0x41,
	0x42,
	0x43,
	0x53,
	0x47,
	0x5f,
	0x60,
	0x61,
	0x56,
	0x5c,
	0x5d,
	0x5e,
	0x57,
	0x59,
	0x5a,
	0x5b,
	0x62,
	0x63,
	0x46,
	0x00,
	0x64,
	0x44,
	0x45,
	0x68,
	0x69,
	0x6a,
	0x6b,
	0x6c,
	0x6d,
	0x6e,
	0xe8,
	0xe9,
	0x71,
	0x72,
	0x73,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x85,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0xe3,
	0xe7,
	0x65,

	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x58,
	0xe4,
	0x00,
	0x00,
	0x7f,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x81,
	0x00,
	0x80,
	0x00,
	0x00,
	0x00,
	0x00,
	0x54,
	0x00,
	0x46,
	0xe6,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x48,
	0x48,
	0x4a,
	0x52,
	0x4b,
	0x00,
	0x50,
	0x00,
	0x4f,
	0x00,
	0x4d,
	0x51,
	0x4e,
	0x49,
	0x4c,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0xe3,
	0xe7,
	0x65,
	0x66,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};

bool hid_has_events(HIDState* hs)
{
	return hs->n > 0 || hs->idle_pending;
}

static void hid_idle_timer(void* opaque)
{
	HIDState* hs = (HIDState*)opaque;

	hs->idle_pending = true;
	hs->event(hs);
}

static void hid_del_idle_timer(HIDState* hs)
{
	/*if (hs->idle_timer) {
        timer_del(hs->idle_timer);
        timer_free(hs->idle_timer);
        hs->idle_timer = NULL;
    }*/
}

void hid_set_next_idle(HIDState* hs)
{
	/*if (hs->idle) {
        uint64_t expire_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
            NANOSECONDS_PER_SECOND * hs->idle * 4 / 1000;
        if (!hs->idle_timer) {
            hs->idle_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, hid_idle_timer, hs);
        }
        timer_mod_ns(hs->idle_timer, expire_time);
    }
    else {
        hid_del_idle_timer(hs);
    }*/
}

static void hid_pointer_event(HIDState* hs, InputEvent* evt)
{
	static const int bmap[INPUT_BUTTON__MAX] = {
		/*[INPUT_BUTTON_LEFT] =*/0x01,
		/*[INPUT_BUTTON_MIDDLE] =*/0x04,
		/*[INPUT_BUTTON_RIGHT] =*/0x02,
		0, 0, 0, 0};
	HIDPointerEvent* e;
	InputMoveEvent* move;
	InputBtnEvent* btn;

	assert(hs->n < QUEUE_LENGTH);
	e = &hs->ptr.queue[(hs->head + hs->n) & QUEUE_MASK];

	switch (evt->type)
	{
		case INPUT_EVENT_KIND_REL:
			move = &evt->u.rel;
			if (move->axis == INPUT_AXIS_X)
			{
				e->xdx += move->value;
			}
			else if (move->axis == INPUT_AXIS_Y)
			{
				e->ydy += move->value;
			}
			break;

		case INPUT_EVENT_KIND_ABS:
			move = &evt->u.abs;
			if (move->axis == INPUT_AXIS_X)
			{
				e->xdx = move->value;
			}
			else if (move->axis == INPUT_AXIS_Y)
			{
				e->ydy = move->value;
			}
			break;

		case INPUT_EVENT_KIND_BTN:
			btn = &evt->u.btn;
			if (btn->down)
			{
				e->buttons_state |= bmap[btn->button];
				if (btn->button == INPUT_BUTTON_WHEEL_UP)
				{
					e->dz--;
				}
				else if (btn->button == INPUT_BUTTON_WHEEL_DOWN)
				{
					e->dz++;
				}
			}
			else
			{
				e->buttons_state &= ~bmap[btn->button];
			}
			break;

		default:
			/* keep gcc happy */
			break;
	}
}

static void hid_pointer_sync(HIDState* hs)
{
	HIDPointerEvent *prev, *curr, *next;
	bool event_compression = false;

	if (hs->n == QUEUE_LENGTH - 1)
	{
		/*
        * Queue full.  We are losing information, but we at least
        * keep track of most recent button state.
        */
		return;
	}

	prev = &hs->ptr.queue[(hs->head + hs->n - 1) & QUEUE_MASK];
	curr = &hs->ptr.queue[(hs->head + hs->n) & QUEUE_MASK];
	next = &hs->ptr.queue[(hs->head + hs->n + 1) & QUEUE_MASK];

	if (hs->n > 0)
	{
		/*
        * No button state change between previous and current event
        * (and previous wasn't seen by the guest yet), so there is
        * motion information only and we can combine the two event
        * into one.
        */
		if (curr->buttons_state == prev->buttons_state)
		{
			event_compression = true;
		}
	}

	if (event_compression)
	{
		/* add current motion to previous, clear current */
		if (hs->kind == HID_MOUSE)
		{
			prev->xdx += curr->xdx;
			curr->xdx = 0;
			prev->ydy += curr->ydy;
			curr->ydy = 0;
		}
		else
		{
			prev->xdx = curr->xdx;
			prev->ydy = curr->ydy;
		}
		prev->dz += curr->dz;
		curr->dz = 0;
	}
	else
	{
		/* prepate next (clear rel, copy abs + btns) */
		if (hs->kind == HID_MOUSE)
		{
			next->xdx = 0;
			next->ydy = 0;
		}
		else
		{
			next->xdx = curr->xdx;
			next->ydy = curr->ydy;
		}
		next->dz = 0;
		next->buttons_state = curr->buttons_state;
		/* make current guest visible, notify guest */
		hs->n++;
		hs->event(hs);
	}
}

static void hid_keyboard_event(HIDState* hs, InputEvent* evt)
{
	int scancodes[3], i, count;
	int slot;
	InputKeyEvent* key = &evt->u.key;

	count = qemu_input_key_value_to_scancode(&key->key,
											 key->down,
											 scancodes);
	if (hs->n + count > QUEUE_LENGTH)
	{
		//trace_hid_kbd_queue_full();
		return;
	}
	for (i = 0; i < count; i++)
	{
		slot = (hs->head + hs->n) & QUEUE_MASK;
		hs->n++;
		hs->kbd.keycodes[slot] = scancodes[i];
	}
	hs->event(hs);
}

static void hid_keyboard_process_keycode(HIDState* hs)
{
	uint8_t hid_code, index, key;
	int i, keycode, slot;

	if (hs->n == 0)
	{
		return;
	}
	slot = hs->head & QUEUE_MASK;
	QUEUE_INCR(hs->head);
	hs->n--;
	keycode = hs->kbd.keycodes[slot];

	if (!hs->n)
	{
		//trace_hid_kbd_queue_empty();
	}

	key = keycode & 0x7f;
	index = key | ((hs->kbd.modifiers & (1 << 8)) >> 1);
	hid_code = hid_usage_keys[index];
	hs->kbd.modifiers &= ~(1 << 8);

	switch (hid_code)
	{
		case 0x00:
			return;

		case 0xe0:
			assert(key == 0x1d);
			if (hs->kbd.modifiers & (1 << 9))
			{
				/* The hid_codes for the 0xe1/0x1d scancode sequence are 0xe9/0xe0.
            * Here we're processing the second hid_code.  By dropping bit 9
            * and setting bit 8, the scancode after 0x1d will access the
            * second half of the table.
            */
				hs->kbd.modifiers ^= (1 << 8) | (1 << 9);
				return;
			}
			/* fall through to process Ctrl_L */
			//case 0xe1 ... 0xe7:
			[[fallthrough]];
		case 0xe1:
		case 0xe2:
		case 0xe3:
		case 0xe4:
		case 0xe5:
		case 0xe6:
		case 0xe7:
			/* Ctrl_L/Ctrl_R, Shift_L/Shift_R, Alt_L/Alt_R, Win_L/Win_R.
        * Handle releases here, or fall through to process presses.
        */
			if (keycode & (1 << 7))
			{
				hs->kbd.modifiers &= ~(1 << (hid_code & 0x0f));
				return;
			}
			/* fall through */
		case 0xe8:
		case 0xe9:
			/* USB modifiers are just 1 byte long.  Bits 8 and 9 of
        * hs->kbd.modifiers implement a state machine that detects the
        * 0xe0 and 0xe1/0x1d sequences.  These bits do not follow the
        * usual rules where bit 7 marks released keys; they are cleared
        * elsewhere in the function as the state machine dictates.
        */
			hs->kbd.modifiers |= 1 << (hid_code & 0x0f);
			return;

		case 0xea:
		case 0xeb:
		case 0xec:
		case 0xed:
		case 0xee:
		case 0xef:
#ifdef _DEBUG
			abort();
#endif
		default:
			break;
	}

	if (keycode & (1 << 7))
	{
		for (i = hs->kbd.keys - 1; i >= 0; i--)
		{
			if (hs->kbd.key[i] == hid_code)
			{
				hs->kbd.key[i] = hs->kbd.key[--hs->kbd.keys];
				hs->kbd.key[hs->kbd.keys] = 0x00;
				break;
			}
		}
		if (i < 0)
		{
			return;
		}
	}
	else
	{
		for (i = hs->kbd.keys - 1; i >= 0; i--)
		{
			if (hs->kbd.key[i] == hid_code)
			{
				break;
			}
		}
		if (i < 0)
		{
			if (hs->kbd.keys < (int32_t)sizeof(hs->kbd.key))
			{
				hs->kbd.key[hs->kbd.keys++] = hid_code;
			}
		}
		else
		{
			return;
		}
	}
}

static inline int int_clamp(int val, int vmin, int vmax)
{
	if (val < vmin)
	{
		return vmin;
	}
	else if (val > vmax)
	{
		return vmax;
	}
	else
	{
		return val;
	}
}

void hid_pointer_activate(HIDState* hs)
{
	if (!hs->ptr.mouse_grabbed)
	{
		//qemu_input_handler_activate(hs->s);
		hs->ptr.mouse_grabbed = 1;
	}
}

int hid_pointer_poll(HIDState* hs, uint8_t* buf, int len)
{
	int dx, dy, dz, l;
	int index;
	HIDPointerEvent* e;

	hs->idle_pending = false;

	hid_pointer_activate(hs);

	/* When the buffer is empty, return the last event.  Relative
    movements will all be zero.  */
	index = (hs->n ? hs->head : hs->head - 1);
	e = &hs->ptr.queue[index & QUEUE_MASK];

	if (hs->kind == HID_MOUSE)
	{
		dx = int_clamp(e->xdx, -127, 127);
		dy = int_clamp(e->ydy, -127, 127);
		e->xdx -= dx;
		e->ydy -= dy;
	}
	else
	{
		dx = e->xdx;
		dy = e->ydy;
	}
	dz = int_clamp(e->dz, -127, 127);
	e->dz -= dz;

	if (hs->n &&
		!e->dz &&
		(hs->kind == HID_TABLET || (!e->xdx && !e->ydy)))
	{
		/* that deals with this event */
		QUEUE_INCR(hs->head);
		hs->n--;
	}

	/* Appears we have to invert the wheel direction */
	dz = 0 - dz;
	l = 0;
	switch (hs->kind)
	{
		case HID_MOUSE:
			if (len > l)
			{
				buf[l++] = e->buttons_state;
			}
			if (len > l)
			{
				buf[l++] = dx;
			}
			if (len > l)
			{
				buf[l++] = dy;
			}
			if (len > l)
			{
				buf[l++] = dz;
			}
			break;

		case HID_TABLET:
			if (len > l)
			{
				buf[l++] = e->buttons_state;
			}
			if (len > l)
			{
				buf[l++] = dx & 0xff;
			}
			if (len > l)
			{
				buf[l++] = dx >> 8;
			}
			if (len > l)
			{
				buf[l++] = dy & 0xff;
			}
			if (len > l)
			{
				buf[l++] = dy >> 8;
			}
			if (len > l)
			{
				buf[l++] = dz;
			}
			break;

		default:
			abort();
	}

	return l;
}

int hid_keyboard_poll(HIDState* hs, uint8_t* buf, int len)
{
	hs->idle_pending = false;

	if (len < 2)
	{
		return 0;
	}

	hid_keyboard_process_keycode(hs);

	buf[0] = hs->kbd.modifiers & 0xff;
	buf[1] = 0;
	if (hs->kbd.keys > 6)
	{
		memset(buf + 2, HID_USAGE_ERROR_ROLLOVER, MIN(8, len) - 2);
	}
	else
	{
		memcpy(buf + 2, hs->kbd.key, MIN(8, len) - 2);
	}

	return MIN(8, len);
}

int hid_keyboard_write(HIDState* hs, uint8_t* buf, int len)
{
	if (len > 0)
	{
		int ledstate = 0;
		/* 0x01: Num Lock LED
        * 0x02: Caps Lock LED
        * 0x04: Scroll Lock LED
        * 0x08: Compose LED
        * 0x10: Kana LED */
		hs->kbd.leds = buf[0];
		if (hs->kbd.leds & 0x04)
		{
			ledstate |= QEMU_SCROLL_LOCK_LED;
		}
		if (hs->kbd.leds & 0x01)
		{
			ledstate |= QEMU_NUM_LOCK_LED;
		}
		if (hs->kbd.leds & 0x02)
		{
			ledstate |= QEMU_CAPS_LOCK_LED;
		}
		//kbd_put_ledstate(ledstate);
	}
	return 0;
}

void hid_reset(HIDState* hs)
{
	switch (hs->kind)
	{
		case HID_KEYBOARD:
			memset(hs->kbd.keycodes, 0, sizeof(hs->kbd.keycodes));
			memset(hs->kbd.key, 0, sizeof(hs->kbd.key));
			hs->kbd.keys = 0;
			hs->kbd.modifiers = 0;
			break;
		case HID_MOUSE:
		case HID_TABLET:
			memset(hs->ptr.queue, 0, sizeof(hs->ptr.queue));
			break;
	}
	hs->head = 0;
	hs->n = 0;
	hs->protocol = 1;
	hs->idle = 0;
	hs->idle_pending = false;
	hid_del_idle_timer(hs);
}

void hid_free(HIDState* hs)
{
	//qemu_input_handler_unregister(hs->s);
	hid_del_idle_timer(hs);
}

void hid_init(HIDState* hs, int kind, HIDEventFunc event)
{
	hs->kind = kind;
	hs->event = event;

	if (hs->kind == HID_KEYBOARD)
	{
		hs->kbd.eh_entry = hid_keyboard_event;
	}
	else if (hs->kind == HID_MOUSE || hs->kind == HID_TABLET)
	{
		hs->ptr.eh_entry = hid_pointer_event;
		hs->ptr.eh_sync = hid_pointer_sync;
	}
}
