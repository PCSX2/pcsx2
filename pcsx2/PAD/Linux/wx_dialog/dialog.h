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
#include <wx/notebook.h>
#include <wx/frame.h>
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/effects.h>
#include <wx/rawbmp.h>
#include <wx/graphics.h>
#include <wx/timer.h>

#include <sstream>

#include "../InputManager.h"
#include "../keyboard.h"
#include "../Global.h"
#include "opPanel.h"

#include "GamepadConfiguration.h"
#include "JoystickConfiguration.h"

// Allow to found quickly button id
// e.g L2 → 0, triangle → 4, ...
// see Global.h for more details about gamepad button id

enum gui_buttons
{
	Analog = PAD_R_LEFT + 1, // Analog button (not yet supported ?)
	JoyL_config,             // Left Joystick Configuration
	JoyR_config,             // Right Joystick Configuration
	Gamepad_config,          // Gamepad Configuration
	Set_all,                 // Set all buttons
	Apply,                   // Apply modifications without exit
	Ok,                      // Apply modifications and exit
	Cancel                   // Exit without apply modificatons
};

#define BUTTONS_LENGHT 32 // numbers of buttons on the gamepad
#define UPDATE_TIME 5
#define DEFAULT_WIDTH 1000
#define DEFAULT_HEIGHT 760

class PADDialog : public wxDialog
{
	// Panels
	opPanel* m_pan_tabs[GAMEPAD_NUMBER]; // Gamepad Tabs box
	// Notebooks
	wxNotebook* m_tab_gamepad; // Joysticks Tabs
	// Buttons
	wxButton* m_bt_gamepad[GAMEPAD_NUMBER][BUTTONS_LENGHT]; // Joystick button use to modify the button mapping
	// Contain all simulated key
	u32 m_simulatedKeys[GAMEPAD_NUMBER][MAX_KEYS];
	// Timer
	wxTimer m_time_update_gui;
	// Check if the gui must display feddback image
	bool m_pressed[GAMEPAD_NUMBER][NB_IMG];

	// methods
	bool config_key(int, int);
	void clear_key(int, int);
	void repopulate();

	// Events
	void OnButtonClicked(wxCommandEvent&);
	void JoystickEvent(wxTimerEvent&);

public:
	PADDialog();
	void InitDialog();
	void show();
};

extern void DisplayDialog(); // Main function
