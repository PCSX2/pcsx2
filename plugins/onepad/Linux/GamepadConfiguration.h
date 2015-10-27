/*  GamepadConfiguration.h
 *  PCSX2 Dev Team
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

#ifndef __GAMEPADCONFIGURATION_H__
#define __GAMEPADCONFIGURATION_H__

#include <wx/wx.h>
#include <wx/frame.h>
#include <wx/window.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/slider.h>
#include "GamePad.h"
#include "keyboard.h"
#include "onepad.h"

class GamepadConfiguration : public wxFrame
{
    wxPanel* pan_gamepad_config;
    wxCheckBox *cb_rumble, *cb_hack_sixaxis;
    wxSlider *sl_rumble_intensity, *sl_joystick_sensibility;
    wxButton *bt_ok, *bt_cancel;
    wxStaticText *lbl_rumble_intensity, *lbl_joystick_sensibility;

    int pad_id;
    u32 init_rumble_intensity, init_joystick_sensibility;
    bool init_rumble, init_hack_sixaxis;

    // methods
    void repopulate();
    void reset();
    // Events
    void OnButtonClicked(wxCommandEvent&);
    void OnSliderReleased(wxCommandEvent&);
    void OnCheckboxChange(wxCommandEvent&);

public:
    GamepadConfiguration(int, wxWindow*);
    void InitGamepadConfiguration();
};

#endif // __GAMEPADCONFIGURATION_H__
