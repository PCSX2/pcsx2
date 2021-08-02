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

#include "evdev.h"

#include <chrono>
#include <thread>
#include <stdio.h>
#include <sstream>
#include "USB/gtk.h"

namespace usb_pad
{
	namespace evdev
	{

		using sys_clock = std::chrono::system_clock;
		using ms = std::chrono::milliseconds;

#define EVDEV_DIR "/dev/input/by-id/"

		const std::array<const char*, 525> key_to_str = {
			"RESERVED",         /* linux:0 (KEY_RESERVED) */
			"ESC",              /* linux:1 (KEY_ESC) */
			"1",                /* linux:2 (KEY_1) */
			"2",                /* linux:3 (KEY_2) */
			"3",                /* linux:4 (KEY_3) */
			"4",                /* linux:5 (KEY_4) */
			"5",                /* linux:6 (KEY_5) */
			"6",                /* linux:7 (KEY_6) */
			"7",                /* linux:8 (KEY_7) */
			"8",                /* linux:9 (KEY_8) */
			"9",                /* linux:10 (KEY_9) */
			"0",                /* linux:11 (KEY_0) */
			"MINUS",            /* linux:12 (KEY_MINUS) */
			"EQUAL",            /* linux:13 (KEY_EQUAL) */
			"BACKSPACE",        /* linux:14 (KEY_BACKSPACE) */
			"TAB",              /* linux:15 (KEY_TAB) */
			"Q",                /* linux:16 (KEY_Q) */
			"W",                /* linux:17 (KEY_W) */
			"E",                /* linux:18 (KEY_E) */
			"R",                /* linux:19 (KEY_R) */
			"T",                /* linux:20 (KEY_T) */
			"Y",                /* linux:21 (KEY_Y) */
			"U",                /* linux:22 (KEY_U) */
			"I",                /* linux:23 (KEY_I) */
			"O",                /* linux:24 (KEY_O) */
			"P",                /* linux:25 (KEY_P) */
			"{",                /* linux:26 (KEY_LEFTBRACE) */
			"}",                /* linux:27 (KEY_RIGHTBRACE) */
			"ENTER",            /* linux:28 (KEY_ENTER) */
			"L-CTRL",           /* linux:29 (KEY_LEFTCTRL) */
			"A",                /* linux:30 (KEY_A) */
			"S",                /* linux:31 (KEY_S) */
			"D",                /* linux:32 (KEY_D) */
			"F",                /* linux:33 (KEY_F) */
			"G",                /* linux:34 (KEY_G) */
			"H",                /* linux:35 (KEY_H) */
			"J",                /* linux:36 (KEY_J) */
			"K",                /* linux:37 (KEY_K) */
			"L",                /* linux:38 (KEY_L) */
			";",                /* linux:39 (KEY_SEMICOLON) */
			"'",                /* linux:40 (KEY_APOSTROPHE) */
			"~",                /* linux:41 (KEY_GRAVE) */
			"L-SHIFT",          /* linux:42 (KEY_LEFTSHIFT) */
			"\\",               /* linux:43 (KEY_BACKSLASH) */
			"Z",                /* linux:44 (KEY_Z) */
			"X",                /* linux:45 (KEY_X) */
			"C",                /* linux:46 (KEY_C) */
			"V",                /* linux:47 (KEY_V) */
			"B",                /* linux:48 (KEY_B) */
			"N",                /* linux:49 (KEY_N) */
			"M",                /* linux:50 (KEY_M) */
			",",                /* linux:51 (KEY_COMMA) */
			".",                /* linux:52 (KEY_DOT) */
			"/",                /* linux:53 (KEY_SLASH) */
			"R-SHIFT",          /* linux:54 (KEY_RIGHTSHIFT) */
			"KPASTERISK",       /* linux:55 (KEY_KPASTERISK) */
			"LEFTALT",          /* linux:56 (KEY_LEFTALT) */
			"SPACE",            /* linux:57 (KEY_SPACE) */
			"CAPSLOCK",         /* linux:58 (KEY_CAPSLOCK) */
			"F1",               /* linux:59 (KEY_F1) */
			"F2",               /* linux:60 (KEY_F2) */
			"F3",               /* linux:61 (KEY_F3) */
			"F4",               /* linux:62 (KEY_F4) */
			"F5",               /* linux:63 (KEY_F5) */
			"F6",               /* linux:64 (KEY_F6) */
			"F7",               /* linux:65 (KEY_F7) */
			"F8",               /* linux:66 (KEY_F8) */
			"F9",               /* linux:67 (KEY_F9) */
			"F10",              /* linux:68 (KEY_F10) */
			"NUMLOCK",          /* linux:69 (KEY_NUMLOCK) */
			"SCROLLLOCK",       /* linux:70 (KEY_SCROLLLOCK) */
			"KP7",              /* linux:71 (KEY_KP7) */
			"KP8",              /* linux:72 (KEY_KP8) */
			"KP9",              /* linux:73 (KEY_KP9) */
			"KPMINUS",          /* linux:74 (KEY_KPMINUS) */
			"KP4",              /* linux:75 (KEY_KP4) */
			"KP5",              /* linux:76 (KEY_KP5) */
			"KP6",              /* linux:77 (KEY_KP6) */
			"KPPLUS",           /* linux:78 (KEY_KPPLUS) */
			"KP1",              /* linux:79 (KEY_KP1) */
			"KP2",              /* linux:80 (KEY_KP2) */
			"KP3",              /* linux:81 (KEY_KP3) */
			"KP0",              /* linux:82 (KEY_KP0) */
			"KPDOT",            /* linux:83 (KEY_KPDOT) */
			"84",               /* linux:84 (unnamed) */
			"ZENKAKUHANKAKU",   /* linux:85 (KEY_ZENKAKUHANKAKU) */
			"102ND",            /* linux:86 (KEY_102ND) */
			"F11",              /* linux:87 (KEY_F11) */
			"F12",              /* linux:88 (KEY_F12) */
			"RO",               /* linux:89 (KEY_RO) */
			"KATAKANA",         /* linux:90 (KEY_KATAKANA) */
			"HIRAGANA",         /* linux:91 (KEY_HIRAGANA) */
			"HENKAN",           /* linux:92 (KEY_HENKAN) */
			"KATAKANAHIRAGANA", /* linux:93 (KEY_KATAKANAHIRAGANA) */
			"MUHENKAN",         /* linux:94 (KEY_MUHENKAN) */
			"KPJPCOMMA",        /* linux:95 (KEY_KPJPCOMMA) */
			"KPENTER",          /* linux:96 (KEY_KPENTER) */
			"RIGHTCTRL",        /* linux:97 (KEY_RIGHTCTRL) */
			"KPSLASH",          /* linux:98 (KEY_KPSLASH) */
			"SYSRQ",            /* linux:99 (KEY_SYSRQ) */
			"RIGHTALT",         /* linux:100 (KEY_RIGHTALT) */
			"LINEFEED",         /* linux:101 (KEY_LINEFEED) */
			"HOME",             /* linux:102 (KEY_HOME) */
			"UP",               /* linux:103 (KEY_UP) */
			"PAGEUP",           /* linux:104 (KEY_PAGEUP) */
			"LEFT",             /* linux:105 (KEY_LEFT) */
			"RIGHT",            /* linux:106 (KEY_RIGHT) */
			"END",              /* linux:107 (KEY_END) */
			"DOWN",             /* linux:108 (KEY_DOWN) */
			"PAGEDOWN",         /* linux:109 (KEY_PAGEDOWN) */
			"INSERT",           /* linux:110 (KEY_INSERT) */
			"DELETE",           /* linux:111 (KEY_DELETE) */
			"MACRO",            /* linux:112 (KEY_MACRO) */
			"MUTE",             /* linux:113 (KEY_MUTE) */
			"VOLUMEDOWN",       /* linux:114 (KEY_VOLUMEDOWN) */
			"VOLUMEUP",         /* linux:115 (KEY_VOLUMEUP) */
			"POWER",            /* linux:116 (KEY_POWER) */
			"KPEQUAL",          /* linux:117 (KEY_KPEQUAL) */
			"KPPLUSMINUS",      /* linux:118 (KEY_KPPLUSMINUS) */
			"PAUSE",            /* linux:119 (KEY_PAUSE) */
			"SCALE",            /* linux:120 (KEY_SCALE) */
			"KPCOMMA",          /* linux:121 (KEY_KPCOMMA) */
			"HANGEUL",          /* linux:122 (KEY_HANGEUL) */
			"HANJA",            /* linux:123 (KEY_HANJA) */
			"YEN",              /* linux:124 (KEY_YEN) */
			"LEFTMETA",         /* linux:125 (KEY_LEFTMETA) */
			"RIGHTMETA",        /* linux:126 (KEY_RIGHTMETA) */
			"COMPOSE",          /* linux:127 (KEY_COMPOSE) */
			"STOP",             /* linux:128 (KEY_STOP) */
			"AGAIN",            /* linux:129 (KEY_AGAIN) */
			"PROPS",            /* linux:130 (KEY_PROPS) */
			"UNDO",             /* linux:131 (KEY_UNDO) */
			"FRONT",            /* linux:132 (KEY_FRONT) */
			"COPY",             /* linux:133 (KEY_COPY) */
			"OPEN",             /* linux:134 (KEY_OPEN) */
			"PASTE",            /* linux:135 (KEY_PASTE) */
			"FIND",             /* linux:136 (KEY_FIND) */
			"CUT",              /* linux:137 (KEY_CUT) */
			"HELP",             /* linux:138 (KEY_HELP) */
			"MENU",             /* linux:139 (KEY_MENU) */
			"CALC",             /* linux:140 (KEY_CALC) */
			"SETUP",            /* linux:141 (KEY_SETUP) */
			"SLEEP",            /* linux:142 (KEY_SLEEP) */
			"WAKEUP",           /* linux:143 (KEY_WAKEUP) */
			"FILE",             /* linux:144 (KEY_FILE) */
			"SENDFILE",         /* linux:145 (KEY_SENDFILE) */
			"DELETEFILE",       /* linux:146 (KEY_DELETEFILE) */
			"XFER",             /* linux:147 (KEY_XFER) */
			"PROG1",            /* linux:148 (KEY_PROG1) */
			"PROG2",            /* linux:149 (KEY_PROG2) */
			"WWW",              /* linux:150 (KEY_WWW) */
			"MSDOS",            /* linux:151 (KEY_MSDOS) */
			"SCREENLOCK",       /* linux:152 (KEY_SCREENLOCK) */
			"DIRECTION",        /* linux:153 (KEY_DIRECTION) */
			"CYCLEWINDOWS",     /* linux:154 (KEY_CYCLEWINDOWS) */
			"MAIL",             /* linux:155 (KEY_MAIL) */
			"BOOKMARKS",        /* linux:156 (KEY_BOOKMARKS) */
			"COMPUTER",         /* linux:157 (KEY_COMPUTER) */
			"BACK",             /* linux:158 (KEY_BACK) */
			"FORWARD",          /* linux:159 (KEY_FORWARD) */
			"CLOSECD",          /* linux:160 (KEY_CLOSECD) */
			"EJECTCD",          /* linux:161 (KEY_EJECTCD) */
			"EJECTCLOSECD",     /* linux:162 (KEY_EJECTCLOSECD) */
			"NEXTSONG",         /* linux:163 (KEY_NEXTSONG) */
			"PLAYPAUSE",        /* linux:164 (KEY_PLAYPAUSE) */
			"PREVIOUSSONG",     /* linux:165 (KEY_PREVIOUSSONG) */
			"STOPCD",           /* linux:166 (KEY_STOPCD) */
			"RECORD",           /* linux:167 (KEY_RECORD) */
			"REWIND",           /* linux:168 (KEY_REWIND) */
			"PHONE",            /* linux:169 (KEY_PHONE) */
			"ISO",              /* linux:170 (KEY_ISO) */
			"CONFIG",           /* linux:171 (KEY_CONFIG) */
			"HOMEPAGE",         /* linux:172 (KEY_HOMEPAGE) */
			"REFRESH",          /* linux:173 (KEY_REFRESH) */
			"EXIT",             /* linux:174 (KEY_EXIT) */
			"MOVE",             /* linux:175 (KEY_MOVE) */
			"EDIT",             /* linux:176 (KEY_EDIT) */
			"SCROLLUP",         /* linux:177 (KEY_SCROLLUP) */
			"SCROLLDOWN",       /* linux:178 (KEY_SCROLLDOWN) */
			"KPLEFTPAREN",      /* linux:179 (KEY_KPLEFTPAREN) */
			"KPRIGHTPAREN",     /* linux:180 (KEY_KPRIGHTPAREN) */
			"NEW",              /* linux:181 (KEY_NEW) */
			"REDO",             /* linux:182 (KEY_REDO) */
			"F13",              /* linux:183 (KEY_F13) */
			"F14",              /* linux:184 (KEY_F14) */
			"F15",              /* linux:185 (KEY_F15) */
			"F16",              /* linux:186 (KEY_F16) */
			"F17",              /* linux:187 (KEY_F17) */
			"F18",              /* linux:188 (KEY_F18) */
			"F19",              /* linux:189 (KEY_F19) */
			"F20",              /* linux:190 (KEY_F20) */
			"F21",              /* linux:191 (KEY_F21) */
			"F22",              /* linux:192 (KEY_F22) */
			"F23",              /* linux:193 (KEY_F23) */
			"F24",              /* linux:194 (KEY_F24) */
			"195",              /* linux:195 (unnamed) */
			"196",              /* linux:196 (unnamed) */
			"197",              /* linux:197 (unnamed) */
			"198",              /* linux:198 (unnamed) */
			"199",              /* linux:199 (unnamed) */
			"PLAYCD",           /* linux:200 (KEY_PLAYCD) */
			"PAUSECD",          /* linux:201 (KEY_PAUSECD) */
			"PROG3",            /* linux:202 (KEY_PROG3) */
			"PROG4",            /* linux:203 (KEY_PROG4) */
			"DASHBOARD",        /* linux:204 (KEY_DASHBOARD) */
			"SUSPEND",          /* linux:205 (KEY_SUSPEND) */
			"CLOSE",            /* linux:206 (KEY_CLOSE) */
			"PLAY",             /* linux:207 (KEY_PLAY) */
			"FASTFORWARD",      /* linux:208 (KEY_FASTFORWARD) */
			"BASSBOOST",        /* linux:209 (KEY_BASSBOOST) */
			"PRINT",            /* linux:210 (KEY_PRINT) */
			"HP",               /* linux:211 (KEY_HP) */
			"CAMERA",           /* linux:212 (KEY_CAMERA) */
			"SOUND",            /* linux:213 (KEY_SOUND) */
			"QUESTION",         /* linux:214 (KEY_QUESTION) */
			"EMAIL",            /* linux:215 (KEY_EMAIL) */
			"CHAT",             /* linux:216 (KEY_CHAT) */
			"SEARCH",           /* linux:217 (KEY_SEARCH) */
			"CONNECT",          /* linux:218 (KEY_CONNECT) */
			"FINANCE",          /* linux:219 (KEY_FINANCE) */
			"SPORT",            /* linux:220 (KEY_SPORT) */
			"SHOP",             /* linux:221 (KEY_SHOP) */
			"ALTERASE",         /* linux:222 (KEY_ALTERASE) */
			"CANCEL",           /* linux:223 (KEY_CANCEL) */
			"BRIGHTNESSDOWN",   /* linux:224 (KEY_BRIGHTNESSDOWN) */
			"BRIGHTNESSUP",     /* linux:225 (KEY_BRIGHTNESSUP) */
			"MEDIA",            /* linux:226 (KEY_MEDIA) */
			"SWITCHVIDEOMODE",  /* linux:227 (KEY_SWITCHVIDEOMODE) */
			"KBDILLUMTOGGLE",   /* linux:228 (KEY_KBDILLUMTOGGLE) */
			"KBDILLUMDOWN",     /* linux:229 (KEY_KBDILLUMDOWN) */
			"KBDILLUMUP",       /* linux:230 (KEY_KBDILLUMUP) */
			"SEND",             /* linux:231 (KEY_SEND) */
			"REPLY",            /* linux:232 (KEY_REPLY) */
			"FORWARDMAIL",      /* linux:233 (KEY_FORWARDMAIL) */
			"SAVE",             /* linux:234 (KEY_SAVE) */
			"DOCUMENTS",        /* linux:235 (KEY_DOCUMENTS) */
			"BATTERY",          /* linux:236 (KEY_BATTERY) */
			"BLUETOOTH",        /* linux:237 (KEY_BLUETOOTH) */
			"WLAN",             /* linux:238 (KEY_WLAN) */
			"UWB",              /* linux:239 (KEY_UWB) */
			"UNKNOWN",          /* linux:240 (KEY_UNKNOWN) */
			"VIDEO_NEXT",       /* linux:241 (KEY_VIDEO_NEXT) */
			"VIDEO_PREV",       /* linux:242 (KEY_VIDEO_PREV) */
			"BRIGHTNESS_CYCLE", /* linux:243 (KEY_BRIGHTNESS_CYCLE) */
			"BRIGHTNESS_ZERO",  /* linux:244 (KEY_BRIGHTNESS_ZERO) */
			"DISPLAY_OFF",      /* linux:245 (KEY_DISPLAY_OFF) */
			"WIMAX",            /* linux:246 (KEY_WIMAX) */
			"RFKILL",           /* linux:247 (KEY_RFKILL) */
			"MICMUTE",          /* linux:248 (KEY_MICMUTE) */
			"249",              /* linux:249 (unnamed) */
			"250",              /* linux:250 (unnamed) */
			"251",              /* linux:251 (unnamed) */
			"252",              /* linux:252 (unnamed) */
			"253",              /* linux:253 (unnamed) */
			"254",              /* linux:254 (unnamed) */
			"255",              /* linux:255 (unnamed) */
			"BTN_0",            /* linux:256 (BTN_0) */
			"BTN_1",            /* linux:257 (BTN_1) */
			"BTN_2",            /* linux:258 (BTN_2) */
			"BTN_3",            /* linux:259 (BTN_3) */
			"BTN_4",            /* linux:260 (BTN_4) */
			"BTN_5",            /* linux:261 (BTN_5) */
			"BTN_6",            /* linux:262 (BTN_6) */
			"BTN_7",            /* linux:263 (BTN_7) */
			"BTN_8",            /* linux:264 (BTN_8) */
			"BTN_9",            /* linux:265 (BTN_9) */
			"266",              /* linux:266 (unnamed) */
			"267",              /* linux:267 (unnamed) */
			"268",              /* linux:268 (unnamed) */
			"269",              /* linux:269 (unnamed) */
			"270",              /* linux:270 (unnamed) */
			"271",              /* linux:271 (unnamed) */
			"BTN_LEFT",         /* linux:272 (BTN_LEFT) */
			"BTN_RIGHT",        /* linux:273 (BTN_RIGHT) */
			"BTN_MIDDLE",       /* linux:274 (BTN_MIDDLE) */
			"BTN_SIDE",         /* linux:275 (BTN_SIDE) */
			"BTN_EXTRA",        /* linux:276 (BTN_EXTRA) */
			"BTN_FORWARD",      /* linux:277 (BTN_FORWARD) */
			"BTN_BACK",         /* linux:278 (BTN_BACK) */
			"BTN_TASK",         /* linux:279 (BTN_TASK) */
			"280",              /* linux:280 (unnamed) */
			"281",              /* linux:281 (unnamed) */
			"282",              /* linux:282 (unnamed) */
			"283",              /* linux:283 (unnamed) */
			"284",              /* linux:284 (unnamed) */
			"285",              /* linux:285 (unnamed) */
			"286",              /* linux:286 (unnamed) */
			"287",              /* linux:287 (unnamed) */
			"TRIGGER",          /* linux:288 (BTN_TRIGGER) */
			"THUMB",            /* linux:289 (BTN_THUMB) */
			"THUMB2",           /* linux:290 (BTN_THUMB2) */
			"TOP",              /* linux:291 (BTN_TOP) */
			"TOP2",             /* linux:292 (BTN_TOP2) */
			"PINKIE",           /* linux:293 (BTN_PINKIE) */
			"BASE",             /* linux:294 (BTN_BASE) */
			"BASE2",            /* linux:295 (BTN_BASE2) */
			"BASE3",            /* linux:296 (BTN_BASE3) */
			"BASE4",            /* linux:297 (BTN_BASE4) */
			"BASE5",            /* linux:298 (BTN_BASE5) */
			"BASE6",            /* linux:299 (BTN_BASE6) */
			"300",              /* linux:300 (unnamed) */
			"301",              /* linux:301 (unnamed) */
			"302",              /* linux:302 (unnamed) */
			"DEAD",             /* linux:303 (BTN_DEAD) */
			"BTN_A",            /* linux:304 (BTN_A) */
			"BTN_B",            /* linux:305 (BTN_B) */
			"BTN_C",            /* linux:306 (BTN_C) */
			"BTN_X",            /* linux:307 (BTN_X) */
			"BTN_Y",            /* linux:308 (BTN_Y) */
			"BTN_Z",            /* linux:309 (BTN_Z) */
			"TL",               /* linux:310 (BTN_TL) */
			"TR",               /* linux:311 (BTN_TR) */
			"TL2",              /* linux:312 (BTN_TL2) */
			"TR2",              /* linux:313 (BTN_TR2) */
			"SELECT",           /* linux:314 (BTN_SELECT) */
			"START",            /* linux:315 (BTN_START) */
			"MODE",             /* linux:316 (BTN_MODE) */
			"THUMBL",           /* linux:317 (BTN_THUMBL) */
			"THUMBR",           /* linux:318 (BTN_THUMBR) */
			"319",              /* linux:319 (unnamed) */
			"TOOL_PEN",         /* linux:320 (BTN_TOOL_PEN) */
			"TOOL_RUBBER",      /* linux:321 (BTN_TOOL_RUBBER) */
			"TOOL_BRUSH",       /* linux:322 (BTN_TOOL_BRUSH) */
			"TOOL_PENCIL",      /* linux:323 (BTN_TOOL_PENCIL) */
			"TOOL_AIRBRUSH",    /* linux:324 (BTN_TOOL_AIRBRUSH) */
			"TOOL_FINGER",      /* linux:325 (BTN_TOOL_FINGER) */
			"TOOL_MOUSE",       /* linux:326 (BTN_TOOL_MOUSE) */
			"TOOL_LENS",        /* linux:327 (BTN_TOOL_LENS) */
			"328",              /* linux:328 (unnamed) */
			"329",              /* linux:329 (unnamed) */
			"TOUCH",            /* linux:330 (BTN_TOUCH) */
			"STYLUS",           /* linux:331 (BTN_STYLUS) */
			"STYLUS2",          /* linux:332 (BTN_STYLUS2) */
			"TOOL_DOUBLETAP",   /* linux:333 (BTN_TOOL_DOUBLETAP) */
			"TOOL_TRIPLETAP",   /* linux:334 (BTN_TOOL_TRIPLETAP) */
			"TOOL_QUADTAP",     /* linux:335 (BTN_TOOL_QUADTAP) */
			"GEAR_DOWN",        /* linux:336 (BTN_GEAR_DOWN) */
			"GEAR_UP",          /* linux:337 (BTN_GEAR_UP) */
			"338",              /* linux:338 (unnamed) */
			"339",              /* linux:339 (unnamed) */
			"340",              /* linux:340 (unnamed) */
			"341",              /* linux:341 (unnamed) */
			"342",              /* linux:342 (unnamed) */
			"343",              /* linux:343 (unnamed) */
			"344",              /* linux:344 (unnamed) */
			"345",              /* linux:345 (unnamed) */
			"346",              /* linux:346 (unnamed) */
			"347",              /* linux:347 (unnamed) */
			"348",              /* linux:348 (unnamed) */
			"349",              /* linux:349 (unnamed) */
			"350",              /* linux:350 (unnamed) */
			"351",              /* linux:351 (unnamed) */
			"OK",               /* linux:352 (KEY_OK) */
			"SELECT",           /* linux:353 (KEY_SELECT) */
			"GOTO",             /* linux:354 (KEY_GOTO) */
			"CLEAR",            /* linux:355 (KEY_CLEAR) */
			"POWER2",           /* linux:356 (KEY_POWER2) */
			"OPTION",           /* linux:357 (KEY_OPTION) */
			"INFO",             /* linux:358 (KEY_INFO) */
			"TIME",             /* linux:359 (KEY_TIME) */
			"VENDOR",           /* linux:360 (KEY_VENDOR) */
			"ARCHIVE",          /* linux:361 (KEY_ARCHIVE) */
			"PROGRAM",          /* linux:362 (KEY_PROGRAM) */
			"CHANNEL",          /* linux:363 (KEY_CHANNEL) */
			"FAVORITES",        /* linux:364 (KEY_FAVORITES) */
			"EPG",              /* linux:365 (KEY_EPG) */
			"PVR",              /* linux:366 (KEY_PVR) */
			"MHP",              /* linux:367 (KEY_MHP) */
			"LANGUAGE",         /* linux:368 (KEY_LANGUAGE) */
			"TITLE",            /* linux:369 (KEY_TITLE) */
			"SUBTITLE",         /* linux:370 (KEY_SUBTITLE) */
			"ANGLE",            /* linux:371 (KEY_ANGLE) */
			"ZOOM",             /* linux:372 (KEY_ZOOM) */
			"MODE",             /* linux:373 (KEY_MODE) */
			"KEYBOARD",         /* linux:374 (KEY_KEYBOARD) */
			"SCREEN",           /* linux:375 (KEY_SCREEN) */
			"PC",               /* linux:376 (KEY_PC) */
			"TV",               /* linux:377 (KEY_TV) */
			"TV2",              /* linux:378 (KEY_TV2) */
			"VCR",              /* linux:379 (KEY_VCR) */
			"VCR2",             /* linux:380 (KEY_VCR2) */
			"SAT",              /* linux:381 (KEY_SAT) */
			"SAT2",             /* linux:382 (KEY_SAT2) */
			"CD",               /* linux:383 (KEY_CD) */
			"TAPE",             /* linux:384 (KEY_TAPE) */
			"RADIO",            /* linux:385 (KEY_RADIO) */
			"TUNER",            /* linux:386 (KEY_TUNER) */
			"PLAYER",           /* linux:387 (KEY_PLAYER) */
			"TEXT",             /* linux:388 (KEY_TEXT) */
			"DVD",              /* linux:389 (KEY_DVD) */
			"AUX",              /* linux:390 (KEY_AUX) */
			"MP3",              /* linux:391 (KEY_MP3) */
			"AUDIO",            /* linux:392 (KEY_AUDIO) */
			"VIDEO",            /* linux:393 (KEY_VIDEO) */
			"DIRECTORY",        /* linux:394 (KEY_DIRECTORY) */
			"LIST",             /* linux:395 (KEY_LIST) */
			"MEMO",             /* linux:396 (KEY_MEMO) */
			"CALENDAR",         /* linux:397 (KEY_CALENDAR) */
			"RED",              /* linux:398 (KEY_RED) */
			"GREEN",            /* linux:399 (KEY_GREEN) */
			"YELLOW",           /* linux:400 (KEY_YELLOW) */
			"BLUE",             /* linux:401 (KEY_BLUE) */
			"CHANNELUP",        /* linux:402 (KEY_CHANNELUP) */
			"CHANNELDOWN",      /* linux:403 (KEY_CHANNELDOWN) */
			"FIRST",            /* linux:404 (KEY_FIRST) */
			"LAST",             /* linux:405 (KEY_LAST) */
			"AB",               /* linux:406 (KEY_AB) */
			"NEXT",             /* linux:407 (KEY_NEXT) */
			"RESTART",          /* linux:408 (KEY_RESTART) */
			"SLOW",             /* linux:409 (KEY_SLOW) */
			"SHUFFLE",          /* linux:410 (KEY_SHUFFLE) */
			"BREAK",            /* linux:411 (KEY_BREAK) */
			"PREVIOUS",         /* linux:412 (KEY_PREVIOUS) */
			"DIGITS",           /* linux:413 (KEY_DIGITS) */
			"TEEN",             /* linux:414 (KEY_TEEN) */
			"TWEN",             /* linux:415 (KEY_TWEN) */
			"VIDEOPHONE",       /* linux:416 (KEY_VIDEOPHONE) */
			"GAMES",            /* linux:417 (KEY_GAMES) */
			"ZOOMIN",           /* linux:418 (KEY_ZOOMIN) */
			"ZOOMOUT",          /* linux:419 (KEY_ZOOMOUT) */
			"ZOOMRESET",        /* linux:420 (KEY_ZOOMRESET) */
			"WORDPROCESSOR",    /* linux:421 (KEY_WORDPROCESSOR) */
			"EDITOR",           /* linux:422 (KEY_EDITOR) */
			"SPREADSHEET",      /* linux:423 (KEY_SPREADSHEET) */
			"GRAPHICSEDITOR",   /* linux:424 (KEY_GRAPHICSEDITOR) */
			"PRESENTATION",     /* linux:425 (KEY_PRESENTATION) */
			"DATABASE",         /* linux:426 (KEY_DATABASE) */
			"NEWS",             /* linux:427 (KEY_NEWS) */
			"VOICEMAIL",        /* linux:428 (KEY_VOICEMAIL) */
			"ADDRESSBOOK",      /* linux:429 (KEY_ADDRESSBOOK) */
			"MESSENGER",        /* linux:430 (KEY_MESSENGER) */
			"DISPLAYTOGGLE",    /* linux:431 (KEY_DISPLAYTOGGLE) */
			"SPELLCHECK",       /* linux:432 (KEY_SPELLCHECK) */
			"LOGOFF",           /* linux:433 (KEY_LOGOFF) */
			"DOLLAR",           /* linux:434 (KEY_DOLLAR) */
			"EURO",             /* linux:435 (KEY_EURO) */
			"FRAMEBACK",        /* linux:436 (KEY_FRAMEBACK) */
			"FRAMEFORWARD",     /* linux:437 (KEY_FRAMEFORWARD) */
			"CONTEXT_MENU",     /* linux:438 (KEY_CONTEXT_MENU) */
			"MEDIA_REPEAT",     /* linux:439 (KEY_MEDIA_REPEAT) */
			"440",              /* linux:440 (unnamed) */
			"441",              /* linux:441 (unnamed) */
			"442",              /* linux:442 (unnamed) */
			"443",              /* linux:443 (unnamed) */
			"444",              /* linux:444 (unnamed) */
			"445",              /* linux:445 (unnamed) */
			"446",              /* linux:446 (unnamed) */
			"447",              /* linux:447 (unnamed) */
			"DEL_EOL",          /* linux:448 (KEY_DEL_EOL) */
			"DEL_EOS",          /* linux:449 (KEY_DEL_EOS) */
			"INS_LINE",         /* linux:450 (KEY_INS_LINE) */
			"DEL_LINE",         /* linux:451 (KEY_DEL_LINE) */
			"452",              /* linux:452 (unnamed) */
			"453",              /* linux:453 (unnamed) */
			"454",              /* linux:454 (unnamed) */
			"455",              /* linux:455 (unnamed) */
			"456",              /* linux:456 (unnamed) */
			"457",              /* linux:457 (unnamed) */
			"458",              /* linux:458 (unnamed) */
			"459",              /* linux:459 (unnamed) */
			"460",              /* linux:460 (unnamed) */
			"461",              /* linux:461 (unnamed) */
			"462",              /* linux:462 (unnamed) */
			"463",              /* linux:463 (unnamed) */
			"FN",               /* linux:464 (KEY_FN) */
			"FN_ESC",           /* linux:465 (KEY_FN_ESC) */
			"FN_F1",            /* linux:466 (KEY_FN_F1) */
			"FN_F2",            /* linux:467 (KEY_FN_F2) */
			"FN_F3",            /* linux:468 (KEY_FN_F3) */
			"FN_F4",            /* linux:469 (KEY_FN_F4) */
			"FN_F5",            /* linux:470 (KEY_FN_F5) */
			"FN_F6",            /* linux:471 (KEY_FN_F6) */
			"FN_F7",            /* linux:472 (KEY_FN_F7) */
			"FN_F8",            /* linux:473 (KEY_FN_F8) */
			"FN_F9",            /* linux:474 (KEY_FN_F9) */
			"FN_F10",           /* linux:475 (KEY_FN_F10) */
			"FN_F11",           /* linux:476 (KEY_FN_F11) */
			"FN_F12",           /* linux:477 (KEY_FN_F12) */
			"FN_1",             /* linux:478 (KEY_FN_1) */
			"FN_2",             /* linux:479 (KEY_FN_2) */
			"FN_D",             /* linux:480 (KEY_FN_D) */
			"FN_E",             /* linux:481 (KEY_FN_E) */
			"FN_F",             /* linux:482 (KEY_FN_F) */
			"FN_S",             /* linux:483 (KEY_FN_S) */
			"FN_B",             /* linux:484 (KEY_FN_B) */
			"485",              /* linux:485 (unnamed) */
			"486",              /* linux:486 (unnamed) */
			"487",              /* linux:487 (unnamed) */
			"488",              /* linux:488 (unnamed) */
			"489",              /* linux:489 (unnamed) */
			"490",              /* linux:490 (unnamed) */
			"491",              /* linux:491 (unnamed) */
			"492",              /* linux:492 (unnamed) */
			"493",              /* linux:493 (unnamed) */
			"494",              /* linux:494 (unnamed) */
			"495",              /* linux:495 (unnamed) */
			"496",              /* linux:496 (unnamed) */
			"BRL_DOT1",         /* linux:497 (KEY_BRL_DOT1) */
			"BRL_DOT2",         /* linux:498 (KEY_BRL_DOT2) */
			"BRL_DOT3",         /* linux:499 (KEY_BRL_DOT3) */
			"BRL_DOT4",         /* linux:500 (KEY_BRL_DOT4) */
			"BRL_DOT5",         /* linux:501 (KEY_BRL_DOT5) */
			"BRL_DOT6",         /* linux:502 (KEY_BRL_DOT6) */
			"BRL_DOT7",         /* linux:503 (KEY_BRL_DOT7) */
			"BRL_DOT8",         /* linux:504 (KEY_BRL_DOT8) */
			"BRL_DOT9",         /* linux:505 (KEY_BRL_DOT9) */
			"BRL_DOT10",        /* linux:506 (KEY_BRL_DOT10) */
			"507",              /* linux:507 (unnamed) */
			"508",              /* linux:508 (unnamed) */
			"509",              /* linux:509 (unnamed) */
			"510",              /* linux:510 (unnamed) */
			"511",              /* linux:511 (unnamed) */
			"NUMERIC_0",        /* linux:512 (KEY_NUMERIC_0) */
			"NUMERIC_1",        /* linux:513 (KEY_NUMERIC_1) */
			"NUMERIC_2",        /* linux:514 (KEY_NUMERIC_2) */
			"NUMERIC_3",        /* linux:515 (KEY_NUMERIC_3) */
			"NUMERIC_4",        /* linux:516 (KEY_NUMERIC_4) */
			"NUMERIC_5",        /* linux:517 (KEY_NUMERIC_5) */
			"NUMERIC_6",        /* linux:518 (KEY_NUMERIC_6) */
			"NUMERIC_7",        /* linux:519 (KEY_NUMERIC_7) */
			"NUMERIC_8",        /* linux:520 (KEY_NUMERIC_8) */
			"NUMERIC_9",        /* linux:521 (KEY_NUMERIC_9) */
			"NUMERIC_STAR",     /* linux:522 (KEY_NUMERIC_STAR) */
			"NUMERIC_POUND",    /* linux:523 (KEY_NUMERIC_POUND) */
			"KEY_NUMERIC_A",    /* linux:524 (KEY_NUMERIC_A) */
		};

		static bool GetEventName(const char* dev_type, int map, int event, bool is_button, const char** name)
		{
			if (!name)
				return false;

			if (is_button)
			{
				if (event < (int)key_to_str.size())
				{
					*name = key_to_str[event];
					return true;
				}
				return false;
			}

			// assuming that PS2 axes are always mapped to PC axes
			static char axis[256] = {0};
			snprintf(axis, sizeof(axis), "Axis %d", event);
			*name = axis;
			return true;
		}

		static bool PollInput(const std::vector<std::pair<std::string, ConfigMapping>>& fds, std::string& dev_name, bool isaxis, int& value, bool& inverted, int& initial)
		{
			int event_fd = -1;
			ssize_t len;
			input_event event;
			struct AxisValue
			{
				int16_t value;
				bool initial;
			};
			AxisValue axisVal[ABS_MAX + 1]{};
			unsigned long absbit[NBITS(ABS_MAX)]{};
			axis_correct abs_correct[ABS_MAX]{};

			inverted = false;

			fd_set fdset;
			int maxfd = -1;

			FD_ZERO(&fdset);
			for (const auto& js : fds)
			{
				FD_SET(js.second.fd, &fdset);
				if (maxfd < js.second.fd)
					maxfd = js.second.fd;
			}

			// wait to avoid some false positives like mouse movement
			std::this_thread::sleep_for(ms(250));

			// empty event queues
			for (const auto& js : fds)
				while ((len = read(js.second.fd, &event, sizeof(event))) > 0)
					;

			timeval timeout{};
			timeout.tv_sec = 5;
			int result = select(maxfd + 1, &fdset, NULL, NULL, &timeout);

			if (!result)
				return false;

			if (result == -1)
			{
				return false;
			}

			for (const auto& js : fds)
			{
				if (FD_ISSET(js.second.fd, &fdset))
				{
					event_fd = js.second.fd;
					dev_name = js.first;
					break;
				}
			}

			if (event_fd == -1)
				return false;

			if (isaxis && ioctl(event_fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0)
			{
				for (int i = 0; i < ABS_MAX; ++i)
				{
					if (test_bit(i, absbit))
					{
						struct input_absinfo absinfo;

						if (ioctl(event_fd, EVIOCGABS(i), &absinfo) < 0)
						{
							continue;
						}

						//TODO from SDL2, usable here?
						CalcAxisCorr(abs_correct[i], absinfo);
					}
				}
			}

			auto last = sys_clock::now();
			//Non-blocking read sets len to -1 and errno to EAGAIN if no new data
			while (true)
			{
				auto dur = std::chrono::duration_cast<ms>(sys_clock::now() - last).count();
				if (dur > 5000)
					goto error;

				if (!isaxis)
				{
					event_fd = -1;
					for (const auto& js : fds)
					{
						if (FD_ISSET(js.second.fd, &fdset))
						{
							event_fd = js.second.fd;
							dev_name = js.first;

							break;
						}
					}
				}

				if (event_fd > -1 && (len = read(event_fd, &event, sizeof(event))) > -1 && (len == sizeof(event)))
				{
					if (isaxis && event.type == EV_ABS)
					{
						auto& val = axisVal[event.code];

						if (!val.initial)
						{
							//val.value = event.value;
							val.value = AxisCorrect(abs_correct[event.code], event.value);
							val.initial = true;
						}
						//else if (std::abs(event.value - val.value) > 1000)
						else
						{
							int ac_val = AxisCorrect(abs_correct[event.code], event.value);
							int diff = ac_val - val.value;
							if (std::abs(diff) > 2047)
							{
								value = event.code;
								inverted = (diff < 0);
								initial = val.value;
								break;
							}
						}
					}
					else if (!isaxis && event.type == EV_KEY)
					{
						if (event.value)
						{
							value = event.code;
							break;
						}
					}
				}
				else if (errno != EAGAIN)
				{
					goto error;
				}
				else
				{
					while (gtk_events_pending())
						gtk_main_iteration_do(FALSE);
					std::this_thread::sleep_for(ms(1));
				}
			}

			return true;

		error:
			return false;
		}

		int EvDevPad::Configure(int port, const char* dev_type, void* data)
		{
			ApiCallbacks apicbs{GetEventName, EnumerateDevices, PollInput};
			int ret = 0;
			if (!strcmp(dev_type, BuzzDevice::TypeName()))
				ret = GtkBuzzConfigure(port, dev_type, "Evdev Settings", evdev::APINAME, GTK_WINDOW(data), apicbs);
			else if (!strcmp(dev_type, KeyboardmaniaDevice::TypeName()))
				ret = GtkKeyboardmaniaConfigure(port, dev_type, "Evdev Settings", evdev::APINAME, GTK_WINDOW(data), apicbs);
			else
				ret = GtkPadConfigure(port, dev_type, "Evdev Settings", evdev::APINAME, GTK_WINDOW(data), apicbs);
			return ret;
		}

#undef EVDEV_DIR
	} // namespace evdev
} // namespace usb_pad
