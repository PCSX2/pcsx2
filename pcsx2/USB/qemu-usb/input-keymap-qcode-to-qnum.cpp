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
#include "input-keymap.h"

//TODO how much does std::map kill perf if any?
const std::map<const QKeyCode, unsigned short> qemu_input_map_qcode_to_qnum = {
	{Q_KEY_CODE_0, 0xb},                 /* qcode:Q_KEY_CODE_0 (0) -> linux:11 (KEY_0) -> qnum:11 */
	{Q_KEY_CODE_1, 0x2},                 /* qcode:Q_KEY_CODE_1 (1) -> linux:2 (KEY_1) -> qnum:2 */
	{Q_KEY_CODE_2, 0x3},                 /* qcode:Q_KEY_CODE_2 (2) -> linux:3 (KEY_2) -> qnum:3 */
	{Q_KEY_CODE_3, 0x4},                 /* qcode:Q_KEY_CODE_3 (3) -> linux:4 (KEY_3) -> qnum:4 */
	{Q_KEY_CODE_4, 0x5},                 /* qcode:Q_KEY_CODE_4 (4) -> linux:5 (KEY_4) -> qnum:5 */
	{Q_KEY_CODE_5, 0x6},                 /* qcode:Q_KEY_CODE_5 (5) -> linux:6 (KEY_5) -> qnum:6 */
	{Q_KEY_CODE_6, 0x7},                 /* qcode:Q_KEY_CODE_6 (6) -> linux:7 (KEY_6) -> qnum:7 */
	{Q_KEY_CODE_7, 0x8},                 /* qcode:Q_KEY_CODE_7 (7) -> linux:8 (KEY_7) -> qnum:8 */
	{Q_KEY_CODE_8, 0x9},                 /* qcode:Q_KEY_CODE_8 (8) -> linux:9 (KEY_8) -> qnum:9 */
	{Q_KEY_CODE_9, 0xa},                 /* qcode:Q_KEY_CODE_9 (9) -> linux:10 (KEY_9) -> qnum:10 */
	{Q_KEY_CODE_A, 0x1e},                /* qcode:Q_KEY_CODE_A (a) -> linux:30 (KEY_A) -> qnum:30 */
	{Q_KEY_CODE_AC_BACK, 0xea},          /* qcode:Q_KEY_CODE_AC_BACK (ac_back) -> linux:158 (KEY_BACK) -> qnum:234 */
	{Q_KEY_CODE_AC_BOOKMARKS, 0xe6},     /* qcode:Q_KEY_CODE_AC_BOOKMARKS (ac_bookmarks) -> linux:156 (KEY_BOOKMARKS) -> qnum:230 */
	{Q_KEY_CODE_AC_FORWARD, 0xe9},       /* qcode:Q_KEY_CODE_AC_FORWARD (ac_forward) -> linux:159 (KEY_FORWARD) -> qnum:233 */
	{Q_KEY_CODE_AC_HOME, 0xb2},          /* qcode:Q_KEY_CODE_AC_HOME (ac_home) -> linux:172 (KEY_HOMEPAGE) -> qnum:178 */
	{Q_KEY_CODE_AC_REFRESH, 0xe7},       /* qcode:Q_KEY_CODE_AC_REFRESH (ac_refresh) -> linux:173 (KEY_REFRESH) -> qnum:231 */
	{Q_KEY_CODE_AGAIN, 0x85},            /* qcode:Q_KEY_CODE_AGAIN (again) -> linux:129 (KEY_AGAIN) -> qnum:133 */
	{Q_KEY_CODE_ALT, 0x38},              /* qcode:Q_KEY_CODE_ALT (alt) -> linux:56 (KEY_LEFTALT) -> qnum:56 */
	{Q_KEY_CODE_ALT_R, 0xb8},            /* qcode:Q_KEY_CODE_ALT_R (alt_r) -> linux:100 (KEY_RIGHTALT) -> qnum:184 */
	{Q_KEY_CODE_APOSTROPHE, 0x28},       /* qcode:Q_KEY_CODE_APOSTROPHE (apostrophe) -> linux:40 (KEY_APOSTROPHE) -> qnum:40 */
	{Q_KEY_CODE_ASTERISK, 0x37},         /* qcode:Q_KEY_CODE_ASTERISK (kp_multiply) -> linux:55 (KEY_KPASTERISK) -> qnum:55 */
	{Q_KEY_CODE_AUDIOMUTE, 0xa0},        /* qcode:Q_KEY_CODE_AUDIOMUTE (audiomute) -> linux:113 (KEY_MUTE) -> qnum:160 */
	{Q_KEY_CODE_AUDIONEXT, 0x99},        /* qcode:Q_KEY_CODE_AUDIONEXT (audionext) -> linux:163 (KEY_NEXTSONG) -> qnum:153 */
	{Q_KEY_CODE_AUDIOPLAY, 0xa2},        /* qcode:Q_KEY_CODE_AUDIOPLAY (audioplay) -> linux:164 (KEY_PLAYPAUSE) -> qnum:162 */
	{Q_KEY_CODE_AUDIOPREV, 0x90},        /* qcode:Q_KEY_CODE_AUDIOPREV (audioprev) -> linux:165 (KEY_PREVIOUSSONG) -> qnum:144 */
	{Q_KEY_CODE_AUDIOSTOP, 0xa4},        /* qcode:Q_KEY_CODE_AUDIOSTOP (audiostop) -> linux:166 (KEY_STOPCD) -> qnum:164 */
	{Q_KEY_CODE_B, 0x30},                /* qcode:Q_KEY_CODE_B (b) -> linux:48 (KEY_B) -> qnum:48 */
	{Q_KEY_CODE_BACKSLASH, 0x2b},        /* qcode:Q_KEY_CODE_BACKSLASH (backslash) -> linux:43 (KEY_BACKSLASH) -> qnum:43 */
	{Q_KEY_CODE_BACKSPACE, 0xe},         /* qcode:Q_KEY_CODE_BACKSPACE (backspace) -> linux:14 (KEY_BACKSPACE) -> qnum:14 */
	{Q_KEY_CODE_BRACKET_LEFT, 0x1a},     /* qcode:Q_KEY_CODE_BRACKET_LEFT (bracket_left) -> linux:26 (KEY_LEFTBRACE) -> qnum:26 */
	{Q_KEY_CODE_BRACKET_RIGHT, 0x1b},    /* qcode:Q_KEY_CODE_BRACKET_RIGHT (bracket_right) -> linux:27 (KEY_RIGHTBRACE) -> qnum:27 */
	{Q_KEY_CODE_C, 0x2e},                /* qcode:Q_KEY_CODE_C (c) -> linux:46 (KEY_C) -> qnum:46 */
	{Q_KEY_CODE_CALCULATOR, 0xa1},       /* qcode:Q_KEY_CODE_CALCULATOR (calculator) -> linux:140 (KEY_CALC) -> qnum:161 */
	{Q_KEY_CODE_CAPS_LOCK, 0x3a},        /* qcode:Q_KEY_CODE_CAPS_LOCK (caps_lock) -> linux:58 (KEY_CAPSLOCK) -> qnum:58 */
	{Q_KEY_CODE_COMMA, 0x33},            /* qcode:Q_KEY_CODE_COMMA (comma) -> linux:51 (KEY_COMMA) -> qnum:51 */
	{Q_KEY_CODE_COMPOSE, 0xdd},          /* qcode:Q_KEY_CODE_COMPOSE (compose) -> linux:127 (KEY_COMPOSE) -> qnum:221 */
	{Q_KEY_CODE_COMPUTER, 0xeb},         /* qcode:Q_KEY_CODE_COMPUTER (computer) -> linux:157 (KEY_COMPUTER) -> qnum:235 */
	{Q_KEY_CODE_COPY, 0xf8},             /* qcode:Q_KEY_CODE_COPY (copy) -> linux:133 (KEY_COPY) -> qnum:248 */
	{Q_KEY_CODE_CTRL, 0x1d},             /* qcode:Q_KEY_CODE_CTRL (ctrl) -> linux:29 (KEY_LEFTCTRL) -> qnum:29 */
	{Q_KEY_CODE_CTRL_R, 0x9d},           /* qcode:Q_KEY_CODE_CTRL_R (ctrl_r) -> linux:97 (KEY_RIGHTCTRL) -> qnum:157 */
	{Q_KEY_CODE_CUT, 0xbc},              /* qcode:Q_KEY_CODE_CUT (cut) -> linux:137 (KEY_CUT) -> qnum:188 */
	{Q_KEY_CODE_D, 0x20},                /* qcode:Q_KEY_CODE_D (d) -> linux:32 (KEY_D) -> qnum:32 */
	{Q_KEY_CODE_DELETE, 0xd3},           /* qcode:Q_KEY_CODE_DELETE (delete) -> linux:111 (KEY_DELETE) -> qnum:211 */
	{Q_KEY_CODE_DOT, 0x34},              /* qcode:Q_KEY_CODE_DOT (dot) -> linux:52 (KEY_DOT) -> qnum:52 */
	{Q_KEY_CODE_DOWN, 0xd0},             /* qcode:Q_KEY_CODE_DOWN (down) -> linux:108 (KEY_DOWN) -> qnum:208 */
	{Q_KEY_CODE_E, 0x12},                /* qcode:Q_KEY_CODE_E (e) -> linux:18 (KEY_E) -> qnum:18 */
	{Q_KEY_CODE_END, 0xcf},              /* qcode:Q_KEY_CODE_END (end) -> linux:107 (KEY_END) -> qnum:207 */
	{Q_KEY_CODE_EQUAL, 0xd},             /* qcode:Q_KEY_CODE_EQUAL (equal) -> linux:13 (KEY_EQUAL) -> qnum:13 */
	{Q_KEY_CODE_ESC, 0x1},               /* qcode:Q_KEY_CODE_ESC (esc) -> linux:1 (KEY_ESC) -> qnum:1 */
	{Q_KEY_CODE_F, 0x21},                /* qcode:Q_KEY_CODE_F (f) -> linux:33 (KEY_F) -> qnum:33 */
	{Q_KEY_CODE_F1, 0x3b},               /* qcode:Q_KEY_CODE_F1 (f1) -> linux:59 (KEY_F1) -> qnum:59 */
	{Q_KEY_CODE_F10, 0x44},              /* qcode:Q_KEY_CODE_F10 (f10) -> linux:68 (KEY_F10) -> qnum:68 */
	{Q_KEY_CODE_F11, 0x57},              /* qcode:Q_KEY_CODE_F11 (f11) -> linux:87 (KEY_F11) -> qnum:87 */
	{Q_KEY_CODE_F12, 0x58},              /* qcode:Q_KEY_CODE_F12 (f12) -> linux:88 (KEY_F12) -> qnum:88 */
	{Q_KEY_CODE_F2, 0x3c},               /* qcode:Q_KEY_CODE_F2 (f2) -> linux:60 (KEY_F2) -> qnum:60 */
	{Q_KEY_CODE_F3, 0x3d},               /* qcode:Q_KEY_CODE_F3 (f3) -> linux:61 (KEY_F3) -> qnum:61 */
	{Q_KEY_CODE_F4, 0x3e},               /* qcode:Q_KEY_CODE_F4 (f4) -> linux:62 (KEY_F4) -> qnum:62 */
	{Q_KEY_CODE_F5, 0x3f},               /* qcode:Q_KEY_CODE_F5 (f5) -> linux:63 (KEY_F5) -> qnum:63 */
	{Q_KEY_CODE_F6, 0x40},               /* qcode:Q_KEY_CODE_F6 (f6) -> linux:64 (KEY_F6) -> qnum:64 */
	{Q_KEY_CODE_F7, 0x41},               /* qcode:Q_KEY_CODE_F7 (f7) -> linux:65 (KEY_F7) -> qnum:65 */
	{Q_KEY_CODE_F8, 0x42},               /* qcode:Q_KEY_CODE_F8 (f8) -> linux:66 (KEY_F8) -> qnum:66 */
	{Q_KEY_CODE_F9, 0x43},               /* qcode:Q_KEY_CODE_F9 (f9) -> linux:67 (KEY_F9) -> qnum:67 */
	{Q_KEY_CODE_FIND, 0xc1},             /* qcode:Q_KEY_CODE_FIND (find) -> linux:136 (KEY_FIND) -> qnum:193 */
	{Q_KEY_CODE_FRONT, 0x8c},            /* qcode:Q_KEY_CODE_FRONT (front) -> linux:132 (KEY_FRONT) -> qnum:140 */
	{Q_KEY_CODE_G, 0x22},                /* qcode:Q_KEY_CODE_G (g) -> linux:34 (KEY_G) -> qnum:34 */
	{Q_KEY_CODE_GRAVE_ACCENT, 0x29},     /* qcode:Q_KEY_CODE_GRAVE_ACCENT (grave_accent) -> linux:41 (KEY_GRAVE) -> qnum:41 */
	{Q_KEY_CODE_H, 0x23},                /* qcode:Q_KEY_CODE_H (h) -> linux:35 (KEY_H) -> qnum:35 */
	{Q_KEY_CODE_HELP, 0xf5},             /* qcode:Q_KEY_CODE_HELP (help) -> linux:138 (KEY_HELP) -> qnum:245 */
	{Q_KEY_CODE_HENKAN, 0x79},           /* qcode:Q_KEY_CODE_HENKAN (henkan) -> linux:92 (KEY_HENKAN) -> qnum:121 */
	{Q_KEY_CODE_HIRAGANA, 0x77},         /* qcode:Q_KEY_CODE_HIRAGANA (hiragana) -> linux:91 (KEY_HIRAGANA) -> qnum:119 */
	{Q_KEY_CODE_HOME, 0xc7},             /* qcode:Q_KEY_CODE_HOME (home) -> linux:102 (KEY_HOME) -> qnum:199 */
	{Q_KEY_CODE_I, 0x17},                /* qcode:Q_KEY_CODE_I (i) -> linux:23 (KEY_I) -> qnum:23 */
	{Q_KEY_CODE_INSERT, 0xd2},           /* qcode:Q_KEY_CODE_INSERT (insert) -> linux:110 (KEY_INSERT) -> qnum:210 */
	{Q_KEY_CODE_J, 0x24},                /* qcode:Q_KEY_CODE_J (j) -> linux:36 (KEY_J) -> qnum:36 */
	{Q_KEY_CODE_K, 0x25},                /* qcode:Q_KEY_CODE_K (k) -> linux:37 (KEY_K) -> qnum:37 */
	{Q_KEY_CODE_KATAKANAHIRAGANA, 0x70}, /* qcode:Q_KEY_CODE_KATAKANAHIRAGANA (katakanahiragana) -> linux:93 (KEY_KATAKANAHIRAGANA) -> qnum:112 */
	{Q_KEY_CODE_KP_0, 0x52},             /* qcode:Q_KEY_CODE_KP_0 (kp_0) -> linux:82 (KEY_KP0) -> qnum:82 */
	{Q_KEY_CODE_KP_1, 0x4f},             /* qcode:Q_KEY_CODE_KP_1 (kp_1) -> linux:79 (KEY_KP1) -> qnum:79 */
	{Q_KEY_CODE_KP_2, 0x50},             /* qcode:Q_KEY_CODE_KP_2 (kp_2) -> linux:80 (KEY_KP2) -> qnum:80 */
	{Q_KEY_CODE_KP_3, 0x51},             /* qcode:Q_KEY_CODE_KP_3 (kp_3) -> linux:81 (KEY_KP3) -> qnum:81 */
	{Q_KEY_CODE_KP_4, 0x4b},             /* qcode:Q_KEY_CODE_KP_4 (kp_4) -> linux:75 (KEY_KP4) -> qnum:75 */
	{Q_KEY_CODE_KP_5, 0x4c},             /* qcode:Q_KEY_CODE_KP_5 (kp_5) -> linux:76 (KEY_KP5) -> qnum:76 */
	{Q_KEY_CODE_KP_6, 0x4d},             /* qcode:Q_KEY_CODE_KP_6 (kp_6) -> linux:77 (KEY_KP6) -> qnum:77 */
	{Q_KEY_CODE_KP_7, 0x47},             /* qcode:Q_KEY_CODE_KP_7 (kp_7) -> linux:71 (KEY_KP7) -> qnum:71 */
	{Q_KEY_CODE_KP_8, 0x48},             /* qcode:Q_KEY_CODE_KP_8 (kp_8) -> linux:72 (KEY_KP8) -> qnum:72 */
	{Q_KEY_CODE_KP_9, 0x49},             /* qcode:Q_KEY_CODE_KP_9 (kp_9) -> linux:73 (KEY_KP9) -> qnum:73 */
	{Q_KEY_CODE_KP_ADD, 0x4e},           /* qcode:Q_KEY_CODE_KP_ADD (kp_add) -> linux:78 (KEY_KPPLUS) -> qnum:78 */
	{Q_KEY_CODE_KP_COMMA, 0x7e},         /* qcode:Q_KEY_CODE_KP_COMMA (kp_comma) -> linux:121 (KEY_KPCOMMA) -> qnum:126 */
	{Q_KEY_CODE_KP_DECIMAL, 0x53},       /* qcode:Q_KEY_CODE_KP_DECIMAL (kp_decimal) -> linux:83 (KEY_KPDOT) -> qnum:83 */
	{Q_KEY_CODE_KP_DIVIDE, 0xb5},        /* qcode:Q_KEY_CODE_KP_DIVIDE (kp_divide) -> linux:98 (KEY_KPSLASH) -> qnum:181 */
	{Q_KEY_CODE_KP_ENTER, 0x9c},         /* qcode:Q_KEY_CODE_KP_ENTER (kp_enter) -> linux:96 (KEY_KPENTER) -> qnum:156 */
	{Q_KEY_CODE_KP_EQUALS, 0x59},        /* qcode:Q_KEY_CODE_KP_EQUALS (kp_equals) -> linux:117 (KEY_KPEQUAL) -> qnum:89 */
	{Q_KEY_CODE_KP_MULTIPLY, 0x37},      /* qcode:Q_KEY_CODE_KP_MULTIPLY (kp_multiply) -> linux:55 (KEY_KPASTERISK) -> qnum:55 */
	{Q_KEY_CODE_KP_SUBTRACT, 0x4a},      /* qcode:Q_KEY_CODE_KP_SUBTRACT (kp_subtract) -> linux:74 (KEY_KPMINUS) -> qnum:74 */
	{Q_KEY_CODE_L, 0x26},                /* qcode:Q_KEY_CODE_L (l) -> linux:38 (KEY_L) -> qnum:38 */
	{Q_KEY_CODE_LEFT, 0xcb},             /* qcode:Q_KEY_CODE_LEFT (left) -> linux:105 (KEY_LEFT) -> qnum:203 */
	{Q_KEY_CODE_LESS, 0x56},             /* qcode:Q_KEY_CODE_LESS (less) -> linux:86 (KEY_102ND) -> qnum:86 */
	{Q_KEY_CODE_LF, 0x5b},               /* qcode:Q_KEY_CODE_LF (lf) -> linux:101 (KEY_LINEFEED) -> qnum:91 */
	{Q_KEY_CODE_M, 0x32},                /* qcode:Q_KEY_CODE_M (m) -> linux:50 (KEY_M) -> qnum:50 */
	{Q_KEY_CODE_MAIL, 0xec},             /* qcode:Q_KEY_CODE_MAIL (mail) -> linux:155 (KEY_MAIL) -> qnum:236 */
	{Q_KEY_CODE_MEDIASELECT, 0xed},      /* qcode:Q_KEY_CODE_MEDIASELECT (mediaselect) -> linux:226 (KEY_MEDIA) -> qnum:237 */
	{Q_KEY_CODE_MENU, 0x9e},             /* qcode:Q_KEY_CODE_MENU (menu) -> linux:139 (KEY_MENU) -> qnum:158 */
	{Q_KEY_CODE_META_L, 0xdb},           /* qcode:Q_KEY_CODE_META_L (meta_l) -> linux:125 (KEY_LEFTMETA) -> qnum:219 */
	{Q_KEY_CODE_META_R, 0xdc},           /* qcode:Q_KEY_CODE_META_R (meta_r) -> linux:126 (KEY_RIGHTMETA) -> qnum:220 */
	{Q_KEY_CODE_MINUS, 0xc},             /* qcode:Q_KEY_CODE_MINUS (minus) -> linux:12 (KEY_MINUS) -> qnum:12 */
	{Q_KEY_CODE_MUHENKAN, 0x7b},         /* qcode:Q_KEY_CODE_MUHENKAN (muhenkan) -> linux:94 (KEY_MUHENKAN) -> qnum:123 */
	{Q_KEY_CODE_N, 0x31},                /* qcode:Q_KEY_CODE_N (n) -> linux:49 (KEY_N) -> qnum:49 */
	{Q_KEY_CODE_NUM_LOCK, 0x45},         /* qcode:Q_KEY_CODE_NUM_LOCK (num_lock) -> linux:69 (KEY_NUMLOCK) -> qnum:69 */
	{Q_KEY_CODE_O, 0x18},                /* qcode:Q_KEY_CODE_O (o) -> linux:24 (KEY_O) -> qnum:24 */
	{Q_KEY_CODE_OPEN, 0x64},             /* qcode:Q_KEY_CODE_OPEN (open) -> linux:134 (KEY_OPEN) -> qnum:100 */
	{Q_KEY_CODE_P, 0x19},                /* qcode:Q_KEY_CODE_P (p) -> linux:25 (KEY_P) -> qnum:25 */
	{Q_KEY_CODE_PASTE, 0x65},            /* qcode:Q_KEY_CODE_PASTE (paste) -> linux:135 (KEY_PASTE) -> qnum:101 */
	{Q_KEY_CODE_PAUSE, 0xc6},            /* qcode:Q_KEY_CODE_PAUSE (pause) -> linux:119 (KEY_PAUSE) -> qnum:198 */
	{Q_KEY_CODE_PGDN, 0xd1},             /* qcode:Q_KEY_CODE_PGDN (pgdn) -> linux:109 (KEY_PAGEDOWN) -> qnum:209 */
	{Q_KEY_CODE_PGUP, 0xc9},             /* qcode:Q_KEY_CODE_PGUP (pgup) -> linux:104 (KEY_PAGEUP) -> qnum:201 */
	{Q_KEY_CODE_POWER, 0xde},            /* qcode:Q_KEY_CODE_POWER (power) -> linux:116 (KEY_POWER) -> qnum:222 */
	{Q_KEY_CODE_PRINT, 0x54},            /* qcode:Q_KEY_CODE_PRINT (sysrq) -> linux:99 (KEY_SYSRQ) -> qnum:84 */
	{Q_KEY_CODE_PROPS, 0x86},            /* qcode:Q_KEY_CODE_PROPS (props) -> linux:130 (KEY_PROPS) -> qnum:134 */
	{Q_KEY_CODE_Q, 0x10},                /* qcode:Q_KEY_CODE_Q (q) -> linux:16 (KEY_Q) -> qnum:16 */
	{Q_KEY_CODE_R, 0x13},                /* qcode:Q_KEY_CODE_R (r) -> linux:19 (KEY_R) -> qnum:19 */
	{Q_KEY_CODE_RET, 0x1c},              /* qcode:Q_KEY_CODE_RET (ret) -> linux:28 (KEY_ENTER) -> qnum:28 */
	{Q_KEY_CODE_RIGHT, 0xcd},            /* qcode:Q_KEY_CODE_RIGHT (right) -> linux:106 (KEY_RIGHT) -> qnum:205 */
	{Q_KEY_CODE_RO, 0x73},               /* qcode:Q_KEY_CODE_RO (ro) -> linux:89 (KEY_RO) -> qnum:115 */
	{Q_KEY_CODE_S, 0x1f},                /* qcode:Q_KEY_CODE_S (s) -> linux:31 (KEY_S) -> qnum:31 */
	{Q_KEY_CODE_SCROLL_LOCK, 0x46},      /* qcode:Q_KEY_CODE_SCROLL_LOCK (scroll_lock) -> linux:70 (KEY_SCROLLLOCK) -> qnum:70 */
	{Q_KEY_CODE_SEMICOLON, 0x27},        /* qcode:Q_KEY_CODE_SEMICOLON (semicolon) -> linux:39 (KEY_SEMICOLON) -> qnum:39 */
	{Q_KEY_CODE_SHIFT, 0x2a},            /* qcode:Q_KEY_CODE_SHIFT (shift) -> linux:42 (KEY_LEFTSHIFT) -> qnum:42 */
	{Q_KEY_CODE_SHIFT_R, 0x36},          /* qcode:Q_KEY_CODE_SHIFT_R (shift_r) -> linux:54 (KEY_RIGHTSHIFT) -> qnum:54 */
	{Q_KEY_CODE_SLASH, 0x35},            /* qcode:Q_KEY_CODE_SLASH (slash) -> linux:53 (KEY_SLASH) -> qnum:53 */
	{Q_KEY_CODE_SLEEP, 0xdf},            /* qcode:Q_KEY_CODE_SLEEP (sleep) -> linux:142 (KEY_SLEEP) -> qnum:223 */
	{Q_KEY_CODE_SPC, 0x39},              /* qcode:Q_KEY_CODE_SPC (spc) -> linux:57 (KEY_SPACE) -> qnum:57 */
	{Q_KEY_CODE_STOP, 0xe8},             /* qcode:Q_KEY_CODE_STOP (stop) -> linux:128 (KEY_STOP) -> qnum:232 */
	{Q_KEY_CODE_SYSRQ, 0x54},            /* qcode:Q_KEY_CODE_SYSRQ (sysrq) -> linux:99 (KEY_SYSRQ) -> qnum:84 */
	{Q_KEY_CODE_T, 0x14},                /* qcode:Q_KEY_CODE_T (t) -> linux:20 (KEY_T) -> qnum:20 */
	{Q_KEY_CODE_TAB, 0xf},               /* qcode:Q_KEY_CODE_TAB (tab) -> linux:15 (KEY_TAB) -> qnum:15 */
	{Q_KEY_CODE_U, 0x16},                /* qcode:Q_KEY_CODE_U (u) -> linux:22 (KEY_U) -> qnum:22 */
	{Q_KEY_CODE_UNDO, 0x87},             /* qcode:Q_KEY_CODE_UNDO (undo) -> linux:131 (KEY_UNDO) -> qnum:135 */
	{Q_KEY_CODE_UP, 0xc8},               /* qcode:Q_KEY_CODE_UP (up) -> linux:103 (KEY_UP) -> qnum:200 */
	{Q_KEY_CODE_V, 0x2f},                /* qcode:Q_KEY_CODE_V (v) -> linux:47 (KEY_V) -> qnum:47 */
	{Q_KEY_CODE_VOLUMEDOWN, 0xae},       /* qcode:Q_KEY_CODE_VOLUMEDOWN (volumedown) -> linux:114 (KEY_VOLUMEDOWN) -> qnum:174 */
	{Q_KEY_CODE_VOLUMEUP, 0xb0},         /* qcode:Q_KEY_CODE_VOLUMEUP (volumeup) -> linux:115 (KEY_VOLUMEUP) -> qnum:176 */
	{Q_KEY_CODE_W, 0x11},                /* qcode:Q_KEY_CODE_W (w) -> linux:17 (KEY_W) -> qnum:17 */
	{Q_KEY_CODE_WAKE, 0xe3},             /* qcode:Q_KEY_CODE_WAKE (wake) -> linux:143 (KEY_WAKEUP) -> qnum:227 */
	{Q_KEY_CODE_X, 0x2d},                /* qcode:Q_KEY_CODE_X (x) -> linux:45 (KEY_X) -> qnum:45 */
	{Q_KEY_CODE_Y, 0x15},                /* qcode:Q_KEY_CODE_Y (y) -> linux:21 (KEY_Y) -> qnum:21 */
	{Q_KEY_CODE_YEN, 0x7d},              /* qcode:Q_KEY_CODE_YEN (yen) -> linux:124 (KEY_YEN) -> qnum:125 */
	{Q_KEY_CODE_Z, 0x2c},                /* qcode:Q_KEY_CODE_Z (z) -> linux:44 (KEY_Z) -> qnum:44 */
};

int qemu_input_qcode_to_number(const QKeyCode value)
{
	auto it = qemu_input_map_qcode_to_qnum.find(value);
	if (it == qemu_input_map_qcode_to_qnum.end())
		return 0;
	return it->second;
}

int qemu_input_key_value_to_number(const KeyValue* value)
{
	if (value->type == KEY_VALUE_KIND_QCODE)
	{
		return qemu_input_qcode_to_number(value->u.qcode);
	}
	else
	{
		assert(value->type == KEY_VALUE_KIND_NUMBER);
		return value->u.number;
	}
}

int qemu_input_key_value_to_scancode(const KeyValue* value, bool down,
									 int* codes)
{
	int keycode = qemu_input_key_value_to_number(value);
	int count = 0;

	if (value->type == KEY_VALUE_KIND_QCODE &&
		value->u.qcode == Q_KEY_CODE_PAUSE)
	{
		/* specific case */
		int v = down ? 0 : 0x80;
		codes[count++] = 0xe1;
		codes[count++] = 0x1d | v;
		codes[count++] = 0x45 | v;
		return count;
	}
	if (keycode & SCANCODE_GREY)
	{
		codes[count++] = SCANCODE_EMUL0;
		keycode &= ~SCANCODE_GREY;
	}
	if (!down)
	{
		keycode |= SCANCODE_UP;
	}
	codes[count++] = keycode;

	return count;
}
