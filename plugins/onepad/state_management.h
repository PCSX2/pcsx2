/*  OnePAD
 *  Copyright (C) 2016
 *
 *  Based on LilyPad
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "onepad.h"

#define MODE_DIGITAL 0x41
#define MODE_ANALOG 0x73
#define MODE_DS2_NATIVE 0x79

// The state of the PS2 bus
struct QueryInfo
{
    u8 port;
    u8 slot;
    u8 lastByte;
    u8 currentCommand;
    u8 numBytes;
    u8 queryDone;
    u8 response[42];

    void reset();
    u8 start_poll(int port);

    template <size_t S>
    void set_result(const u8 (&rsp)[S])
    {
        memcpy(response + 2, rsp, S);
        numBytes = 2 + S;
    }

    template <size_t S>
    void set_final_result(const u8 (&rsp)[S])
    {
        set_result(rsp);
        queryDone = 1;
    }
};

// Freeze data, for a single pad.  Basically has all pad state that
// a PS2 can set.
struct PadFreezeData
{
    // Digital / Analog / DS2 Native
    u8 mode;

    u8 modeLock;

    // In config mode
    u8 config;

    u8 vibrate[8];
    u8 umask[2];

    // Vibration indices.
    u8 vibrateI[2];

    // Last vibration value sent to controller.
    // Only used so as not to call vibration
    // functions when old and new values are both 0.
    u8 currentVibrate[2];

    // Next vibrate val to send to controller.  If next and current are
    // both 0, nothing is sent to the controller.  Otherwise, it's sent
    // on every update.
    u8 nextVibrate[2];
};

class Pad : public PadFreezeData
{
public:
    // Lilypad store here the state of PC pad

    void rumble(int port);
    void set_vibrate(int motor, u8 val);
    void reset_vibrate();
    void reset();

    void set_mode(int mode);

    static void reset_all();
    static void stop_vibrate_all();
    static void rumble_all();
};

// Full state to manage save state
struct PadPluginFreezeData
{
    char format[8];
    u32 version;
    // active slot for port
    u8 slot[2];
    PadFreezeData padData[2][4];
    QueryInfo query;
};

extern QueryInfo query;
extern Pad pads[2][4];
extern int slots[2];

extern u8 pad_start_poll(u8 pad);
extern u8 pad_poll(u8 value);
