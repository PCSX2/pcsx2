/*  OnePAD - author: arcum42(@gmail.com)
 *  Copyright (C) 2009
 *
 *  Based on ZeroPAD, author zerofrog@gmail.com
 *  Copyright (C) 2006-2007
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

#include "GamePad.h"
#include "onepad.h"
#include "keyboard.h"
#include "state_management.h"

#include <string.h>
#include "linux.h"
#include <wx/wx.h>

Display *GSdsp;
Window GSwin;

static void SysMessage(const char *fmt, ...)
{
    va_list list;
    char msg[512];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    if (msg[strlen(msg) - 1] == '\n')
        msg[strlen(msg) - 1] = 0;

    wxMessageDialog dialog(nullptr, msg, "Info", wxOK);
    dialog.ShowModal();
}


EXPORT_C_(void) PADabout()
{
    SysMessage("OnePad is a rewrite of Zerofrog's ZeroPad, done by arcum42.");
}

EXPORT_C_(s32) PADtest()
{
    return 0;
}

s32 _PADopen(void *pDsp)
{
    GSdsp = *(Display **)pDsp;
    GSwin = (Window) * (((u32 *)pDsp) + 1);

    SetAutoRepeat(false);
    return 0;
}

void _PADclose()
{
    SetAutoRepeat(true);

    std::vector<GamePad *>::iterator it = s_vgamePad.begin();

    // Delete everything in the vector vjoysticks.
    while (it != s_vgamePad.end())
    {
        delete *it;
        ++it;
    }

    s_vgamePad.clear();
}

void PollForJoystickInput(int cpad)
{
    int joyid = conf->get_joyid(cpad);
    if (!GamePadIdWithinBounds(joyid))
        return;

    GamePad::UpdateGamePadState();
    for (int i = 0; i < MAX_KEYS; i++)
    {
        GamePad *gamePad = s_vgamePad[joyid];

        switch (type_of_joykey(cpad, i))
        {
            case PAD_JOYBUTTONS:
            {

                int value = gamePad->GetButton(key_to_button(cpad, i));
                if (value)
                    key_status->press(cpad, i);
                else
                    key_status->release(cpad, i);

                break;
            }
            case PAD_HAT:
            {
                int value = gamePad->GetHat(key_to_axis(cpad, i));

                // key_to_hat_dir and SDL_JoystickGetHat are a 4 bits bitmap, one for each directions. Only 1 bit can be high for
                // key_to_hat_dir. SDL_JoystickGetHat handles diagonal too (2 bits) so you must check the intersection
                // '&' not only equality '=='. -- Gregory
                if (key_to_hat_dir(cpad, i) & value)
                    key_status->press(cpad, i);
                else
                    key_status->release(cpad, i);

                break;
            }
            case PAD_AXIS:
            {
                int value = gamePad->GetAxisFromKey(cpad, i);
                bool sign = key_to_axis_sign(cpad, i);
                bool full_axis = key_to_axis_type(cpad, i);
                float k=conf->get_sensibility()/100.0; // convert sensibility to float
                if (IsAnalogKey(i))
                {
                    if (abs(value) > gamePad->GetDeadzone())
                        key_status->press(cpad, i, (int)(value*k));
                    else
                        key_status->release(cpad, i);
                }
                else
                {
                    if (full_axis)
                    {
                        value += 0x8000;
                        if (value > gamePad->GetDeadzone())
                            key_status->press(cpad, i, std::min(value / 256, 0xFF));
                        else
                            key_status->release(cpad, i);
                    }
                    else
                    {
                        if (sign && (-value > gamePad->GetDeadzone()))
                            key_status->press(cpad, i, std::min((int)(-value*k) / 128, 0xFF));
                        else if (!sign && (value > gamePad->GetDeadzone()))
                            key_status->press(cpad, i, std::min((int)(value*k) / 128, 0xFF));
                        else
                            key_status->release(cpad, i);
                    }
                }
            }
            default:
                break;
        }
    }
}

EXPORT_C_(void) PADupdate(int pad)
{
    // Gamepad inputs don't count as an activity. Therefore screensaver will
    // be fired after a couple of minute.
    // Emulate an user activity
    static int count = 0;
    count++;
    if ((count & 0xFFF) == 0)
    {
        // 1 call every 4096 Vsync is enough
        XResetScreenSaver(GSdsp);
    }

    // Actually PADupdate is always call with pad == 0. So you need to update both
    // pads -- Gregory

    // Poll keyboard/mouse event. There is currently no way to separate pad0 from pad1 event.
    // So we will populate both pad in the same time
    for (int cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
    {
        key_status->keyboard_state_acces(cpad);
    }
    PollForX11KeyboardInput();

    // Get joystick state + Commit
    for (int cpad = 0; cpad < GAMEPAD_NUMBER; cpad++)
    {
        key_status->joystick_state_acces(cpad);

        PollForJoystickInput(cpad);

        key_status->commit_status(cpad);
    }

    Pad::rumble_all();
}

EXPORT_C_(void) PADconfigure()
{
    LoadConfig();

    DisplayDialog();
    return;
}
