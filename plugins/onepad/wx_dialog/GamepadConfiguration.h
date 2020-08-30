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

#include "../GamePad.h"
#include "../keyboard.h"
#include "../onepad.h"

static const s32 rumble_slider_id = wxID_HIGHEST + 200 + 1;
static const s32 joy_slider_id = wxID_HIGHEST + 200 + 2;
static const s32 enable_rumble_id = wxID_HIGHEST + 200 + 3;

class GamepadConfiguration : public wxDialog
{
    wxCheckBox *m_cb_rumble;
    wxSlider *m_sl_rumble_intensity, *m_sl_joystick_sensibility;
    wxChoice *m_joy_map;

    u32 m_pad_id;

    // Methods
    void repopulate();

    // Events
    void OnOk(wxCommandEvent &);
    void OnSliderReleased(wxCommandEvent &);
    void OnCheckboxChange(wxCommandEvent &);
    void OnChoiceChange(wxCommandEvent &);

public:
    GamepadConfiguration(int, wxWindow *);
    void InitGamepadConfiguration();
};

#endif // __GAMEPADCONFIGURATION_H__
