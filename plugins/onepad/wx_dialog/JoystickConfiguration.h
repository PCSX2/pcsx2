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

#include "../GamePad.h"
#include "../keyboard.h"
#include "../onepad.h"

static const s32 Lx_check_id = wxID_HIGHEST + 100 + 1;
static const s32 Ly_check_id = wxID_HIGHEST + 100 + 2;
static const s32 Ljoy_check_id = wxID_HIGHEST + 100 + 3;

static const s32 Rx_check_id = wxID_HIGHEST + 100 + 4;
static const s32 Ry_check_id = wxID_HIGHEST + 100 + 5;
static const s32 Rjoy_check_id = wxID_HIGHEST + 100 + 6;

class JoystickConfiguration : public wxDialog
{
    wxCheckBox *m_cb_reverse_Lx, *m_cb_reverse_Ly, *m_cb_reverse_Rx, *m_cb_reverse_Ry,
        *m_cb_mouse_Ljoy, // Use mouse for left joystick
        *m_cb_mouse_Rjoy; // Use mouse for right joystick
    wxButton *m_bt_ok, *m_bt_cancel;

    u32 m_pad_id;
    // isForLeftJoystick -> true is for Left Joystick, false is for Right Joystick
    bool m_init_reverse_Lx, m_init_reverse_Ly, m_init_reverse_Rx, m_init_reverse_Ry,
        m_init_mouse_Ljoy, m_init_mouse_Rjoy, m_isForLeftJoystick;

    // Methods
    void repopulate();
    void reset();

    // Events
    void OnCheckboxChange(wxCommandEvent &);
    void OnOk(wxCommandEvent &);
    void OnCancel(wxCommandEvent &);

public:
    JoystickConfiguration(int, bool, wxWindow *);
    void InitJoystickConfiguration();
};

#endif // __JOYSTICKCONFIGURATION_H__
