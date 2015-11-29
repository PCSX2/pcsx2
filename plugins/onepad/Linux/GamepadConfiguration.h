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
    wxPanel* m_pan_gamepad_config;
    wxCheckBox *m_cb_rumble, *m_cb_hack_sixaxis_usb, *m_cb_hack_sixaxis_pressure;
    wxSlider *m_sl_rumble_intensity, *m_sl_joystick_sensibility;
    wxButton *m_bt_ok, *m_bt_cancel;
    wxStaticText *m_lbl_rumble_intensity, *m_lbl_joystick_sensibility;

    int m_pad_id;
    u32 m_init_rumble_intensity, m_init_joystick_sensibility;
    bool m_init_rumble, m_init_hack_sixaxis, m_init_hack_sixaxis_pressure;

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
