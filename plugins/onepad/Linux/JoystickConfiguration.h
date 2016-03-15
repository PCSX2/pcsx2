/*  JoystickConfiguration.h
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
    wxPanel* m_pan_joystick_config;
    wxCheckBox *m_cb_reverse_Lx, *m_cb_reverse_Ly, *m_cb_reverse_Rx, *m_cb_reverse_Ry,
        *m_cb_mouse_Ljoy, // Use mouse for left joystick
        *m_cb_mouse_Rjoy; // Use mouse for right joystick
    wxButton *m_bt_ok, *m_bt_cancel;

    int m_pad_id;
    // isForLeftJoystick -> true is for Left Joystick, false is for Right Joystick
    bool m_init_reverse_Lx, m_init_reverse_Ly, m_init_reverse_Rx, m_init_reverse_Ry,
        m_init_mouse_Ljoy, m_init_mouse_Rjoy, m_isForLeftJoystick;

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
