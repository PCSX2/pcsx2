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

#pragma once

#include <wx/wx.h>

#include "../Device.h"
#include "../keyboard.h"
#include "../Global.h"

static const s32 rumble_slider_id = wxID_HIGHEST + 200 + 1;
static const s32 joy_slider_id = wxID_HIGHEST + 200 + 2;
static const s32 enable_rumble_id = wxID_HIGHEST + 200 + 3;

class GamepadConfiguration : public wxDialog
{
	wxCheckBox* m_cb_rumble;
	wxSlider *m_sl_rumble_intensity, *m_sl_joystick_sensibility;
	wxChoice* m_joy_map;

	u32 m_pad_id;

	// Methods
	void repopulate();

	// Events
	void OnUpdateEvent(wxCommandEvent&);
	void OnSliderReleased(wxCommandEvent&);
	void OnCheckboxChange(wxCommandEvent&);
	void OnChoiceChange(wxCommandEvent&);

public:
	GamepadConfiguration(int, wxWindow*);
	void InitGamepadConfiguration();
};
