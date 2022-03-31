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

#include "dialog.h"

#ifdef __APPLE__
#include <Carbon/Carbon.h>

static std::string KeyName(int pad, int key, int keysym)
{
	// Mouse
	if (keysym >> 16)
	{
		switch (keysym & 0xFFFF)
		{
			case 0:
				return "Mouse ???";
			case 1:
				return "Mouse Left";
			case 2:
				return "Mouse Middle";
			case 3:
				return "Mouse Right";
			default: // Use only number for extra button
				return "Mouse " + std::to_string((keysym & 0xFFFF) + 1);
		}
	}


	// clang-format off
	switch (keysym)
	{
		case kVK_ANSI_A:              return "A";
		case kVK_ANSI_B:              return "B";
		case kVK_ANSI_C:              return "C";
		case kVK_ANSI_D:              return "D";
		case kVK_ANSI_E:              return "E";
		case kVK_ANSI_F:              return "F";
		case kVK_ANSI_G:              return "G";
		case kVK_ANSI_H:              return "H";
		case kVK_ANSI_I:              return "I";
		case kVK_ANSI_J:              return "J";
		case kVK_ANSI_K:              return "K";
		case kVK_ANSI_L:              return "L";
		case kVK_ANSI_M:              return "M";
		case kVK_ANSI_N:              return "N";
		case kVK_ANSI_O:              return "O";
		case kVK_ANSI_P:              return "P";
		case kVK_ANSI_Q:              return "Q";
		case kVK_ANSI_R:              return "R";
		case kVK_ANSI_S:              return "S";
		case kVK_ANSI_T:              return "T";
		case kVK_ANSI_U:              return "U";
		case kVK_ANSI_V:              return "V";
		case kVK_ANSI_W:              return "W";
		case kVK_ANSI_X:              return "X";
		case kVK_ANSI_Y:              return "Y";
		case kVK_ANSI_Z:              return "Z";
		case kVK_ANSI_0:              return "0";
		case kVK_ANSI_1:              return "1";
		case kVK_ANSI_2:              return "2";
		case kVK_ANSI_3:              return "3";
		case kVK_ANSI_4:              return "4";
		case kVK_ANSI_5:              return "5";
		case kVK_ANSI_6:              return "6";
		case kVK_ANSI_7:              return "7";
		case kVK_ANSI_8:              return "8";
		case kVK_ANSI_9:              return "9";
		case kVK_ANSI_Grave:          return "`";
		case kVK_ANSI_Minus:          return "-";
		case kVK_ANSI_Equal:          return "=";
		case kVK_ANSI_LeftBracket:    return "[";
		case kVK_ANSI_RightBracket:   return "]";
		case kVK_ANSI_Backslash:      return "\\";
		case kVK_ANSI_Semicolon:      return ";";
		case kVK_ANSI_Quote:          return "'";
		case kVK_ANSI_Comma:          return ",";
		case kVK_ANSI_Period:         return ".";
		case kVK_ANSI_Slash:          return "/";
		case kVK_Escape:              return "⎋";
		case kVK_Tab:                 return "⇥";
		case kVK_Delete:              return "⌫";
		case kVK_ForwardDelete:       return "⌦";
		case kVK_Return:              return "↩";
		case kVK_Space:               return "␣";
		case kVK_ANSI_KeypadDecimal:  return "Keypad .";
		case kVK_ANSI_KeypadMultiply: return "Keypad *";
		case kVK_ANSI_KeypadPlus:     return "Keypad +";
		case kVK_ANSI_KeypadClear:    return "⌧";
		case kVK_ANSI_KeypadDivide:   return "Keypad /";
		case kVK_ANSI_KeypadEnter:    return "⌤";
		case kVK_ANSI_KeypadMinus:    return "Keypad -";
		case kVK_ANSI_KeypadEquals:   return "Keypad =";
		case kVK_ANSI_Keypad0:        return "Keypad 0";
		case kVK_ANSI_Keypad1:        return "Keypad 1";
		case kVK_ANSI_Keypad2:        return "Keypad 2";
		case kVK_ANSI_Keypad3:        return "Keypad 3";
		case kVK_ANSI_Keypad4:        return "Keypad 4";
		case kVK_ANSI_Keypad5:        return "Keypad 5";
		case kVK_ANSI_Keypad6:        return "Keypad 6";
		case kVK_ANSI_Keypad7:        return "Keypad 7";
		case kVK_ANSI_Keypad8:        return "Keypad 8";
		case kVK_ANSI_Keypad9:        return "Keypad 9";
		case kVK_Command:             return "Left ⌘";
		case kVK_Shift:               return "Left ⇧";
		case kVK_CapsLock:            return "⇪";
		case kVK_Option:              return "Left ⌥";
		case kVK_Control:             return "Left ⌃";
		case kVK_RightCommand:        return "Right ⌘";
		case kVK_RightShift:          return "Right ⇧";
		case kVK_RightOption:         return "Right ⌥";
		case kVK_RightControl:        return "Right ⌃";
		case kVK_Function:            return "fn";
		case kVK_VolumeUp:            return "Volume Up";
		case kVK_VolumeDown:          return "Volume Down";
		case kVK_Mute:                return "Mute";
		case kVK_F1:                  return "F1";
		case kVK_F2:                  return "F2";
		case kVK_F3:                  return "F3";
		case kVK_F4:                  return "F4";
		case kVK_F5:                  return "F5";
		case kVK_F6:                  return "F6";
		case kVK_F7:                  return "F7";
		case kVK_F8:                  return "F8";
		case kVK_F9:                  return "F9";
		case kVK_F10:                 return "F10";
		case kVK_F11:                 return "F11";
		case kVK_F12:                 return "F12";
		case kVK_F13:                 return "F13";
		case kVK_F14:                 return "F14";
		case kVK_F15:                 return "F15";
		case kVK_F16:                 return "F16";
		case kVK_F17:                 return "F17";
		case kVK_F18:                 return "F18";
		case kVK_F19:                 return "F19";
		case kVK_F20:                 return "F20";
		case kVK_Help:                return "Help";
		case kVK_Home:                return "↖";
		case kVK_PageUp:              return "⇞";
		case kVK_End:                 return "↘";
		case kVK_PageDown:            return "⇟";
		case kVK_LeftArrow:           return "←";
		case kVK_RightArrow:          return "→";
		case kVK_DownArrow:           return "↓";
		case kVK_UpArrow:             return "↑";
		case kVK_ISO_Section:         return "Section";
		case kVK_JIS_Yen:             return "¥";
		case kVK_JIS_Underscore:      return "_";
		case kVK_JIS_KeypadComma:     return "Keypad ,";
		case kVK_JIS_Eisu:            return "英数";
		case kVK_JIS_Kana:            return "かな";
		default: return "Key " + std::to_string(keysym);
	}
	// clang-format on
}
#else
static std::string KeyName(int pad, int key, int keysym)
{
	// Mouse
	if (keysym >> 16)
	{
		switch (keysym & 0xFFFF)
		{
			case 0:
				return "Mouse ???";
			case 1:
				return "Mouse Left";
			case 2:
				return "Mouse Middle";
			case 3:
				return "Mouse Right";
			default: // Use only number for extra button
				return "Mouse " + std::to_string((keysym & 0xFFFF) + 1);
		}
	}

	return std::string(XKeysymToString(keysym));
}
#endif

// Construtor of Dialog
PADDialog::PADDialog()
	: wxDialog(NULL,                                  // Parent
			   wxID_ANY,                              // ID
			   _T("Gamepad Settings"),                // Title
			   wxDefaultPosition,                     // Position
			   wxSize(DEFAULT_WIDTH, DEFAULT_HEIGHT), // Width + Lenght
			   // Style
			   wxSYSTEM_MENU |
				   wxCAPTION |
				   wxCLOSE_BOX |
				   wxCLIP_CHILDREN)
{

	/*
     * Define the size and the position of each button :
     * padding[ButtonID][0] : Width
     * padding[ButtonID][1] : Height
     * padding[ButtonID][2] : x position
     * padding[ButtonID][3] : y position
    */
	int padding[BUTTONS_LENGHT][4];

	// L1
	padding[PAD_L1][0] = 218; // Width
	padding[PAD_L1][1] = 28;  // Height
	padding[PAD_L1][2] = 50;  // X
	padding[PAD_L1][3] = 175; // Y

	// L2
	padding[PAD_L2][0] = 218; // Width
	padding[PAD_L2][1] = 28;  // Height
	padding[PAD_L2][2] = 50;  // X
	padding[PAD_L2][3] = 104; // Y

	// R1
	padding[PAD_R1][0] = 218; // Width
	padding[PAD_R1][1] = 28;  // Height
	padding[PAD_R1][2] = 726; // X
	padding[PAD_R1][3] = 175; // Y

	// R2
	padding[PAD_R2][0] = 218; // Width
	padding[PAD_R2][1] = 28;  // Height
	padding[PAD_R2][2] = 726; // X
	padding[PAD_R2][3] = 104; // Y

	// Triangle
	padding[PAD_TRIANGLE][0] = 218; // Width
	padding[PAD_TRIANGLE][1] = 28;  // Height
	padding[PAD_TRIANGLE][2] = 726; // X
	padding[PAD_TRIANGLE][3] = 246; // Y

	// Circle
	padding[PAD_CIRCLE][0] = 218; // Width
	padding[PAD_CIRCLE][1] = 28;  // Height
	padding[PAD_CIRCLE][2] = 726; // X
	padding[PAD_CIRCLE][3] = 319; // Y

	// Cross
	padding[PAD_CROSS][0] = 218; // Width
	padding[PAD_CROSS][1] = 28;  // Height
	padding[PAD_CROSS][2] = 726; // X
	padding[PAD_CROSS][3] = 391; // Y

	// Square
	padding[PAD_SQUARE][0] = 218; // Width
	padding[PAD_SQUARE][1] = 28;  // Height
	padding[PAD_SQUARE][2] = 726; // X
	padding[PAD_SQUARE][3] = 463; // Y

	// Directional pad up
	padding[PAD_UP][0] = 100; // Width
	padding[PAD_UP][1] = 25;  // Height
	padding[PAD_UP][2] = 108; // X
	padding[PAD_UP][3] = 290; // Y

	// Directional pad down
	padding[PAD_DOWN][0] = 100; // Width
	padding[PAD_DOWN][1] = 25;  // Height
	padding[PAD_DOWN][2] = 108; // X
	padding[PAD_DOWN][3] = 340; // Y

	// Directional pad right
	padding[PAD_RIGHT][0] = 109; // Width
	padding[PAD_RIGHT][1] = 25;  // Height
	padding[PAD_RIGHT][2] = 159; // X
	padding[PAD_RIGHT][3] = 315; // Y

	// Directional pad left
	padding[PAD_LEFT][0] = 109; // Width
	padding[PAD_LEFT][1] = 25;  // Height
	padding[PAD_LEFT][2] = 50;  // X
	padding[PAD_LEFT][3] = 315; // Y

	// Left Joystick up
	padding[PAD_L_UP][0] = 100; // Width
	padding[PAD_L_UP][1] = 25;  // Height
	padding[PAD_L_UP][2] = 325; // X
	padding[PAD_L_UP][3] = 527; // Y

	// Left Joystick down
	padding[PAD_L_DOWN][0] = 100; // Width
	padding[PAD_L_DOWN][1] = 25;  // Height
	padding[PAD_L_DOWN][2] = 325; // X
	padding[PAD_L_DOWN][3] = 577; // Y

	// Left Joystick right
	padding[PAD_L_RIGHT][0] = 109; // Width
	padding[PAD_L_RIGHT][1] = 25;  // Height
	padding[PAD_L_RIGHT][2] = 377; // X
	padding[PAD_L_RIGHT][3] = 552; // Y

	// Left Joystick left
	padding[PAD_L_LEFT][0] = 109; // Width
	padding[PAD_L_LEFT][1] = 25;  // Height
	padding[PAD_L_LEFT][2] = 268; // X
	padding[PAD_L_LEFT][3] = 552; // Y

	// L3
	padding[PAD_L3][0] = 218; // Width
	padding[PAD_L3][1] = 28;  // Height
	padding[PAD_L3][2] = 268; // X
	padding[PAD_L3][3] = 641; // Y

	// Right Joystick up
	padding[PAD_R_UP][0] = 100; // Width
	padding[PAD_R_UP][1] = 25;  // Height
	padding[PAD_R_UP][2] = 555; // X
	padding[PAD_R_UP][3] = 527; // Y

	// Right Joystick down
	padding[PAD_R_DOWN][0] = 100; // Width
	padding[PAD_R_DOWN][1] = 25;  // Height
	padding[PAD_R_DOWN][2] = 555; // X
	padding[PAD_R_DOWN][3] = 577; // Y

	// Right Joystick right
	padding[PAD_R_RIGHT][0] = 109; // Width
	padding[PAD_R_RIGHT][1] = 25;  // Height
	padding[PAD_R_RIGHT][2] = 607; // X
	padding[PAD_R_RIGHT][3] = 552; // Y

	// Right Joystick left
	padding[PAD_R_LEFT][0] = 109; // Width
	padding[PAD_R_LEFT][1] = 25;  // Height
	padding[PAD_R_LEFT][2] = 498; // X
	padding[PAD_R_LEFT][3] = 552; // Y

	// R3
	padding[PAD_R3][0] = 218; // Width
	padding[PAD_R3][1] = 28;  // Height
	padding[PAD_R3][2] = 498; // X
	padding[PAD_R3][3] = 641; // Y

	// Start
	padding[PAD_START][0] = 218; // Width
	padding[PAD_START][1] = 28;  // Height
	padding[PAD_START][2] = 503; // X
	padding[PAD_START][3] = 34;  // Y

	// Select
	padding[PAD_SELECT][0] = 218; // Width
	padding[PAD_SELECT][1] = 28;  // Height
	padding[PAD_SELECT][2] = 273; // X
	padding[PAD_SELECT][3] = 34;  // Y

	// Analog
	padding[Analog][0] = 218; // Width
	padding[Analog][1] = 28;  // Height
	padding[Analog][2] = 50;  // X
	padding[Analog][3] = 452; // Y

	// Left Joystick Configuration
	padding[JoyL_config][0] = 180; // Width
	padding[JoyL_config][1] = 28;  // Height
	padding[JoyL_config][2] = 50;  // X
	padding[JoyL_config][3] = 550; // Y

	// Right Joystick Configuration
	padding[JoyR_config][0] = 180; // Width
	padding[JoyR_config][1] = 28;  // Height
	padding[JoyR_config][2] = 764; // X
	padding[JoyR_config][3] = 550; // Y

	// Gamepad Configuration
	padding[Gamepad_config][0] = 180; // Width
	padding[Gamepad_config][1] = 28;  // Height
	padding[Gamepad_config][2] = 50;  // X
	padding[Gamepad_config][3] = 585; // Y

	// Set All Buttons
	padding[Set_all][0] = 180; // Width
	padding[Set_all][1] = 28;  // Height
	padding[Set_all][2] = 764; // X
	padding[Set_all][3] = 585; // Y

	// Apply modifications without exit
	padding[Apply][0] = 70;  // Width
	padding[Apply][1] = 28;  // Height
	padding[Apply][2] = 833; // X
	padding[Apply][3] = 642; // Y

	// Ok button
	padding[Ok][0] = 70;  // Width
	padding[Ok][1] = 28;  // Height
	padding[Ok][2] = 913; // X
	padding[Ok][3] = 642; // Y

	// Cancel button
	padding[Cancel][0] = 70;  // Width
	padding[Cancel][1] = 28;  // Height
	padding[Cancel][2] = 753; // X
	padding[Cancel][3] = 642; // Y

	// create a new Notebook
	m_tab_gamepad = new wxNotebook(this, wxID_ANY);
	for (u32 i = 0; i < GAMEPAD_NUMBER; ++i)
	{
		// Tabs panels
		m_pan_tabs[i] = new opPanel(
			m_tab_gamepad,
			wxID_ANY,
			wxDefaultPosition,
			wxSize(DEFAULT_WIDTH, DEFAULT_HEIGHT));
		// Add new page
		// Define label
		std::stringstream sstm;
		std::string label = "Gamepad ";
		sstm << label << i;
		// New page creation
		m_tab_gamepad->AddPage(
			m_pan_tabs[i],                           // Parent
			wxString(sstm.str().c_str(), wxConvUTF8) // Title
		);

		for (int j = 0; j < BUTTONS_LENGHT; ++j)
		{
			// Gamepad buttons
			m_bt_gamepad[i][j] = new wxButton(
				m_pan_tabs[i],                         // Parent
				wxID_HIGHEST + j + 1,                  // ID
				_T("Undefined"),                       // Label
				wxPoint(padding[j][2], padding[j][3]), // Position
				wxSize(padding[j][0], padding[j][1])   // Size
			);
		}
		// Redefine others gui buttons label
		m_bt_gamepad[i][JoyL_config]->SetLabel(_T("&Left Joystick Config"));
		m_bt_gamepad[i][JoyR_config]->SetLabel(_T("&Right Joystick Config"));
		m_bt_gamepad[i][Gamepad_config]->SetLabel(_T("&Gamepad Configuration"));
		m_bt_gamepad[i][Set_all]->SetLabel(_T("&Set All Buttons"));
		m_bt_gamepad[i][Cancel]->SetLabel(_T("&Cancel"));
		m_bt_gamepad[i][Apply]->SetLabel(_T("&Apply"));
		m_bt_gamepad[i][Ok]->SetLabel(_T("&Ok"));

		// Disable analog button (not yet supported)
		m_bt_gamepad[i][Analog]->Disable();
	}

	Bind(wxEVT_BUTTON, &PADDialog::OnButtonClicked, this);

	for (u32 i = 0; i < GAMEPAD_NUMBER; ++i)
	{
		for (int j = 0; j < NB_IMG; ++j)
		{
			m_pressed[i][j] = false;
		}
	}
}

void PADDialog::InitDialog()
{
	EnumerateDevices(); // activate gamepads
	PADLoadConfig();    // Load configuration from the ini file
	repopulate();       // Set label and fit simulated key array
}

/****************************************/
/*********** Events functions ***********/
/****************************************/

void PADDialog::OnButtonClicked(wxCommandEvent& event)
{
	// Display a message each time the button is clicked
	wxButton* bt_tmp = (wxButton*)event.GetEventObject(); // get the button object
	int bt_id = bt_tmp->GetId() - wxID_HIGHEST - 1;       // get the real ID
	int gamepad_id = m_tab_gamepad->GetSelection();       // get the tab ID (equivalent to the gamepad id)
	if (bt_id >= 0 && bt_id <= PAD_R_LEFT)
	{ // if the button ID is a gamepad button
		bt_tmp->Disable(); // switch the button state to "Disable"
		config_key(gamepad_id, bt_id);
		bt_tmp->Enable(); // switch the button state to "Enable"
	}
	else if (bt_id == Gamepad_config)
	{ // If the button ID is equals to the Gamepad_config button ID
		GamepadConfiguration gamepad_config(gamepad_id, this);

		gamepad_config.InitGamepadConfiguration();
		gamepad_config.ShowModal();
	}
	else if (bt_id == JoyL_config)
	{ // If the button ID is equals to the JoyL_config button ID
		JoystickConfiguration joystick_config(gamepad_id, true, this);

		joystick_config.InitJoystickConfiguration();
		joystick_config.ShowModal();
	}
	else if (bt_id == JoyR_config)
	{ // If the button ID is equals to the JoyR_config button ID
		JoystickConfiguration joystick_config(gamepad_id, false, this);

		joystick_config.InitJoystickConfiguration();
		joystick_config.ShowModal();
	}
	else if (bt_id == Set_all)
	{ // If the button ID is equals to the Set_all button ID
		for (u32 i = 0; i < MAX_KEYS; ++i)
		{
			bt_tmp = m_bt_gamepad[gamepad_id][i];
			switch (i)
			{
				case PAD_L_UP: // Left joystick (Up) ↑
					m_pan_tabs[gamepad_id]->ShowImg(img_l_arrow_up);
					break;
				case PAD_L_RIGHT: // Left joystick (Right) →
					m_pan_tabs[gamepad_id]->ShowImg(img_l_arrow_right);
					break;
				case PAD_L_DOWN: // Left joystick (Down) ↓
					m_pan_tabs[gamepad_id]->ShowImg(img_l_arrow_bottom);
					break;
				case PAD_L_LEFT: // Left joystick (Left) ←
					m_pan_tabs[gamepad_id]->ShowImg(img_l_arrow_left);
					break;
				case PAD_R_UP: // Right joystick (Up) ↑
					m_pan_tabs[gamepad_id]->ShowImg(img_r_arrow_up);
					break;
				case PAD_R_RIGHT: // Right joystick (Right) →
					m_pan_tabs[gamepad_id]->ShowImg(img_r_arrow_right);
					break;
				case PAD_R_DOWN: // Right joystick (Down) ↓
					m_pan_tabs[gamepad_id]->ShowImg(img_r_arrow_bottom);
					break;
				case PAD_R_LEFT: // Right joystick (Left) ←
					m_pan_tabs[gamepad_id]->ShowImg(img_r_arrow_left);
					break;
				default:
					m_pan_tabs[gamepad_id]->ShowImg(i);
					break;
			}
			m_pan_tabs[gamepad_id]->Refresh();
			m_pan_tabs[gamepad_id]->Update();
			bool key_captured = config_key(gamepad_id, i);
			switch (i)
			{
				case PAD_L_UP: // Left joystick (Up) ↑
					m_pan_tabs[gamepad_id]->HideImg(img_l_arrow_up);
					break;
				case PAD_L_RIGHT: // Left joystick (Right) →
					m_pan_tabs[gamepad_id]->HideImg(img_l_arrow_right);
					break;
				case PAD_L_DOWN: // Left joystick (Down) ↓
					m_pan_tabs[gamepad_id]->HideImg(img_l_arrow_bottom);
					break;
				case PAD_L_LEFT: // Left joystick (Left) ←
					m_pan_tabs[gamepad_id]->HideImg(img_l_arrow_left);
					break;
				case PAD_R_UP: // Right joystick (Up) ↑
					m_pan_tabs[gamepad_id]->HideImg(img_r_arrow_up);
					break;
				case PAD_R_RIGHT: // Right joystick (Right) →
					m_pan_tabs[gamepad_id]->HideImg(img_r_arrow_right);
					break;
				case PAD_R_DOWN: // Right joystick (Down) ↓
					m_pan_tabs[gamepad_id]->HideImg(img_r_arrow_bottom);
					break;
				case PAD_R_LEFT: // Right joystick (Left) ←
					m_pan_tabs[gamepad_id]->HideImg(img_r_arrow_left);
					break;
				default:
					m_pan_tabs[gamepad_id]->HideImg(i);
					break;
			}
			m_pan_tabs[gamepad_id]->Refresh();
			m_pan_tabs[gamepad_id]->Update();
			if (!key_captured)
			{ // if ESC is hit, abort Set_all and return user control
				break;
			}
			usleep(500000); // give enough time to the user to release the button
		}
	}
	else if (bt_id == Ok)
	{ // If the button ID is equals to the Ok button ID
		PADSaveConfig(); // Save the configuration
		Close();         // Close the window
	}
	else if (bt_id == Apply)
	{ // If the button ID is equals to the Apply button ID
		PADSaveConfig(); // Save the configuration
	}
	else if (bt_id == Cancel)
	{ // If the button ID is equals to the cancel button ID
		Close(); // Close the window
	}
}

/****************************************/
/*********** Methods functions **********/
/****************************************/

bool PADDialog::config_key(int pad, int key)
{
	bool captured = false;
	u32 key_pressed = 0;

	while (!captured)
	{
		if (PollForNewKeyboardKeys(key_pressed))
		{
			// special case for keyboard/mouse to handle multiple keys
			// Note: key_pressed == UINT32_MAX when ESC is hit to abort the capture
			if (key_pressed != UINT32_MAX)
			{
				clear_key(pad, key);
				set_keyboard_key(pad, key_pressed, key);
				m_simulatedKeys[pad][key] = key_pressed;
			}
			else
			{
				return captured;
			}
			captured = true;
		}
	}
	m_bt_gamepad[pad][key]->SetLabel(
		KeyName(pad, key, m_simulatedKeys[pad][key]).c_str());

	return captured;
}

void PADDialog::clear_key(int pad, int key)
{
	// Erase the keyboard binded key
	u32 keysim = m_simulatedKeys[pad][key];
	m_simulatedKeys[pad][key] = 0;

	// erase gamepad entry (keysim map)
	g_conf.keysym_map[pad].erase(keysim);
}


// Set button values
void PADDialog::repopulate()
{
	for (u32 gamepad_id = 0; gamepad_id < GAMEPAD_NUMBER; ++gamepad_id)
	{
		// keyboard/mouse key
		for (const auto& it : g_conf.keysym_map[gamepad_id])
		{
			int keysym = it.first;
			int key = it.second;

			m_bt_gamepad[gamepad_id][key]->SetLabel(
				KeyName(gamepad_id, key, keysym).c_str());

			m_simulatedKeys[gamepad_id][key] = keysym;
		}
	}
}

// Main
void DisplayDialog()
{
	if (g_conf.ftw)
	{
		wxString info("The PAD GUI is provided to map the keyboard/mouse to the virtual PS2 pad.\n\n"
					  "Gamepads/Joysticks are plug and play. Re-mapping of Gamepads/Joysticks is currently not supported in the PAD GUI.\n\n"
					  "The active gamepad can be selected in the 'Gamepad Configuration' panel.\n\n");

		wxMessageDialog ftw(nullptr, info);
		ftw.ShowModal();

		g_conf.ftw = 0;
		PADSaveConfig();
	}

	PADDialog dialog;

	dialog.InitDialog();
	dialog.ShowModal();
}
