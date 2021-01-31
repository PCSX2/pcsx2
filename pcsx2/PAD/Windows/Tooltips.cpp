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

#include "PrecompiledHeader.h"
#include "Global.h"
#include "resource_pad.h"

LPWSTR pad_dialog_message(int ID, bool* updateText)
{
	if (updateText)
		*updateText = true;
	switch (ID)
	{
		// General tab
		case IDC_M_WM:
		case IDC_M_RAW:
			return L"Enables mouse inputs to be used as pad controls.\n\n"
				   L"The mouse needs to be in focus to be used for playing. By default this is not the case as the \"Start without mouse focus\" checkbox is enabled. "
				   L"Either disable this checkbox or enable/disable the mouse while playing by assigning a key to the \"Mouse\" button on the Pad tabs.\n\n"
				   L"Note 1: By default PCSX2 uses a double-click by the left mouse button to toggle fullscreen mode, this makes the left mouse button unusable as an input."
				   L"To disable this option in PCSX2 go to Config > Emulation Settings > GS Window tab, and disable the \"Double-click toggles fullscreen mode\" checkbox.\n\n"
				   L"Note 2: This does not enable the mouse to function as an in-game mouse in PS2 games that support a USB mouse or lightgun."
				   L"This requires a USB device.";
		case IDC_MOUSE_UNFOCUS:
			return L"Enabled: Mouse is unfocused and can be used for emulation and outside it.\n\n"
				   L"Disabled: Mouse is focused and can be used for emulation.";
		case IDC_MULTIPLE_BINDING:
			return L"Allows binding multiple PS2 controls to one PC control, and binding conflicting controls on opposing ports and/or slots.\n\n"
				   L"Also enables swapping different kinds of pad types(for example, between DS2 and Guitar) when right-clicking in the pad list.";
		case IDC_PAD_LIST:
			return L"Left-click on one of the available pads to enable the pad specific options on the right."
				   L"These options include being able to select the pad type(DS2, Guitar, etc.) and enabling automatic analog mode for PS1 emulation.\n\n"
				   L"Right-click to show a pop-up menu that allows for swapping all the settings and bindings, or just the bindings of individual pad types,"
				   L"between the selected pad and one of other active pads, and clearing all the settings and bindings from the selected pad or just the bindings from a selected pad type.\n\n"
				   L"Note: To allow swapping different kinds of pad types(like DS2 to Guitar), the \"Allow binding multiple PS2 Controls...\" option needs to be enabled as well.";
		case IDC_PAD_TYPE:
			return L"\"Unplugged\" disables the controller and removes the corresponding pad tab.\n\n"
				   L"\"Dualshock 2\" emulates the default PS2 controller for use in both PS1 and PS2 games.\n\n"
				   L"\"Guitar\" emulates a PS2 controller used in the Guitar Hero and Rock Band series of games.\n\n"
				   L"\"Pop'n Music controller\" emulates a PS2 controller used exclusively in the Japanese Pop'n Music series of games.\n\n"
				   L"\"PS1 Mouse\" emulates the PlayStation Mouse. This controller can only be used in a number of PS1 games like \"Command & Conquer: Red Alert\" and \"Myst\".\n\n"
				   L"\"neGcon\" emulates a controller that can be used in a number of PS1 games and PS2 games like the \"Ridge Racer\" and \"Ace Combat\" series.";
		case IDC_DIAG_LIST:
			return L"Shows a list of currently available input devices.\n\n"
				   L"Double-click a device in the list or right-click it and select \"Test Device\" to display a continuously updated list of the state of all inputs on the selected device.\n"
				   L"Use this option to check if all the inputs on a controller function properly.\n\n"
				   L"Right-click and select \"Refresh\" to update the list of devices in case a recently connected device has not shown up yet.";
		case IDC_G_DI:
			return L"(Legacy) Enable this if your gamepad doesn't support Xinput.\n\n"
				   L"Disable for DualShock 4 (PS4 controllers) and probably others.";
		case IDC_G_XI:
			return L"For Xbox 360/ Xbox One controllers (or devices supporting Xinput).\n\n"
				   L"If it doesn't support Xinput then running through Steam as a non-Steam game might be required for the controllers to work properly.\n\n"
				   L"https://gamepad-tester.com/ to test your controller and check if it only says 'Xinput' on top.";
		case ID_RESTORE_DEFAULTS:
			return L"Restores the default contents of PAD.ini, undoing all settings changes and bindings that have been set up.";
		// Pad tabs
		case IDC_BINDINGS_LIST:
			return L"Shows a list of currently bound inputs of the selected Pad.\n\n"
				   L"Left-click on one of the bindings in the list to configure it.\n\n"
				   L"Right-click and select \"Delete Selected\" to remove the selected input from the list.\n\n"
				   L"Right-click and select \"Clear All\" to remove all the inputs from the list.\n\n"
				   L"Note: Use Shift/Ctrl + Left-click to select multiple bindings. Changing the displayed configuration will now copy it to all selected bindings.";
		case IDC_DEVICE_SELECT:
			return L"Select a single device to hide the bindings from other devices in the bindings list, and to only be able to add new bindings for the selected device.\n\n"
				   L"This can also avoid input conflict issues when one controller is recognized as several devices through different APIs.";
		case IDC_CONFIGURE_ON_BIND:
			return L"Immediately go to the configuration setup when you create a new binding.";
		case ID_MOUSE:
			return L"Bind a key that releases or captures the mouse.\n\n"
				   L"Pressing the assigned button when the mouse is in focus, it releases the mouse from use in-game and makes the cursor visible so it can move/resize the emulator window.\n\n"
				   L"Alt-tabbing to another application also makes the cursor visible, but focusing the emulation window hides it again.\n\n"
				   L"Pressing the button when the mouse is out of focus and visible, it captures the mouse so that it can be used as a controller again.\n\n"
				   L"Note 1: Though the binding appears on the page of a specific pad, pressing the button affects all mice.\n\n"
				   L"Note 2: By default PCSX2 uses a double-click by the left mouse button to toggle fullscreen mode, this makes the left mouse button unusable as an input."
				   L"To disable this option in PCSX2 go to Config > Emulation Settings > GS Window tab, and disable the \"Double-click toggles fullscreen mode\" checkbox.";
		case ID_ANALOG:
			return L"Bind a keys that switches the pad from digital mode to analog mode and vice versa.\n\n"
				   L"This option is useful when analog mode is enabled in a game that does not support it, as this causes the game to not recognise any input or to not even detect a controller.\n\n"
				   L"This option can also be used to enable analog mode in games that support, but do not automatically enable analog mode.\n\n"
				   L"Note: Analog mode enables the analog sticks to function on a DualShock controller, while in digital mode it behaves as an original PlayStation controller.\n\n";
		case ID_TURBO_KEY:
			return L"Sets a key to send a TAB press to the emulator, which toggles Turbo mode(200% speed) in PCSX2.";
		case ID_EXCLUDE:
			return L"Disables an input so it will be ignored when trying to bind another input.\n\n"
				   L"This is helpful when binding controls for a device with an input that's difficult to center like an accelerator, or just always active like a faulty button or analog stick.";
		case ID_LOCK_ALL_INPUT:
			return L"Locks the current state of the pad. Any further input is handled normally, but the initial pad state is the locked state instead of a state with no buttons pressed. "
				   L"Pressing it again releases the old pad state, if the old pad state had any keys pressed. Otherwise, it's released automatically.";
		case ID_LOCK_DIRECTION:
			return L"Locks the current state of the d-pad and analog sticks. Pressing this when all input is locked unlocks only the pad and sticks."
				   L"Pressing it again will lock them again, keeping the buttons locked.";
		case ID_LOCK_BUTTONS:
			return L"Locks the current state of the buttons. Pressing this when all input is locked unlocks only the buttons. "
				   L"Pressing it again will lock them again, keeping the d-pad and analog sticks locked.";
		case IDC_RAPID_FIRE:
			return L"Automatically presses/releases the input every other time the button is polled.";
		case IDC_FLIP:
			return L"Inverts a button or axis, making down up and up down.";
		case IDC_SLIDER_DEADZONE:
			return L"Decreases or increases the range of an input where no input is recognised.\n\n"
				   L"Increasing the dead zone requires the input to be pressed harder or moved more before it is applied, decreasing it makes it recognise a softer press or a shorter movement.";
		case IDC_SLIDER_SKIP_DEADZONE:
			return L"Skips and avoids the dead zone to detect input earlier.\n\n"
				   L"Note: This is useful when a controller input requires too much movement/pressure before there's a corresponding action in-game.";
		case IDC_SLIDER_SENSITIVITY:
			return L"Sets how hard an axis or button is pressed.\n\n"
				   L"Note 1: What the default sensitivity value of \"1.00\" means depends on the device itself. The default is high enough that relative axes (which are primarily used by mice) are generally either considered fully up or down."
				   L"For absolute axes (and force feedback devices), which are used by most game devices, a value of 1.0 should map the device's extreme values to the extreme values of a stick/pad.\n\n"
				   L"Note 2: Setting the sensitivity of PC button bindings only really has an effect for PS2 analog sticks or when playing a game with full DS2 pressure sensitivity support.";
		default:
			if (updateText)
				*updateText = false;
			return L"";
	}
}
