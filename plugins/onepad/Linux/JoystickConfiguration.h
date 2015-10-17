/*  onepad.h
 *  Copyright (C) 2015
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

#pragma once

#ifndef __JOYSTICKCONFIGURATION_H__
#define __JOYSTICKCONFIGURATION_H__

#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/window.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/slider.h>
#include "GamePad.h"
#include "keyboard.h"
#include "onepad.h"

class JoystickConfiguration : public wxFrame
{
    wxPanel* pan_joystick_config;
    wxCheckBox *cb_reverse_Lx, *cb_reverse_Ly, *cb_reverse_Rx, *cb_reverse_Ry,
        *cb_mouse_Ljoy, // Use mouse for left joystick
        *cb_mouse_Rjoy; // Use mouse for right joystick
    wxButton *bt_ok, *bt_cancel;

    int pad_id;
    // isForLeftJoystick -> true is for Left Joystick, false is for Right Joystick
    bool init_reverse_Lx, init_reverse_Ly, init_reverse_Rx, init_reverse_Ry,
        init_mouse_Ljoy, init_mouse_Rjoy, isForLeftJoystick;

    // methods
    void repopulate();
    void reset();
    // Events
    void OnButtonClicked(wxCommandEvent&);
    void OnCheckboxChange(wxCommandEvent&);

public:
    JoystickConfiguration(int, bool, wxWindow*);
    void InitJoystickConfiguration();
};

#endif // __JOYSTICKCONFIGURATION_H__
