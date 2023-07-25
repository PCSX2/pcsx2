/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "SIO/Pad/PadMacros.h"

#include "SIO/Pad/PadManager.h"
#include "SIO/Pad/PadBase.h"

PadMacros g_PadMacros;

PadMacros::PadMacros() = default;
PadMacros::~PadMacros() = default;

void PadMacros::ClearMacros()
{
	this->s_macro_buttons = {};
}

PadMacros::MacroButton& PadMacros::GetMacroButton(u32 pad, u32 index)
{
	return this->s_macro_buttons[pad][index];
}

void PadMacros::SetMacroButtonState(u32 pad, u32 index, bool state)
{
	if (pad >= Pad::NUM_CONTROLLER_PORTS || index >= NUM_MACRO_BUTTONS_PER_CONTROLLER)
		return;

	PadMacros::MacroButton& mb = s_macro_buttons[pad][index];
	if (mb.buttons.empty() || mb.trigger_state == state)
		return;

	mb.toggle_counter = mb.toggle_frequency;
	mb.trigger_state = state;
	if (mb.toggle_state != state)
	{
		mb.toggle_state = state;
		ApplyMacroButton(pad, mb);
	}
}

void PadMacros::ApplyMacroButton(u32 controller, const PadMacros::MacroButton& mb)
{
	const float value = mb.toggle_state ? 1.0f : 0.0f;
	PadBase* pad = g_PadManager.GetPad(controller);

	for (const u32 btn : mb.buttons)
		pad->Set(btn, value);
}

void PadMacros::UpdateMacroButtons()
{
	for (u32 pad = 0; pad < Pad::NUM_CONTROLLER_PORTS; pad++)
	{
		for (u32 index = 0; index < NUM_MACRO_BUTTONS_PER_CONTROLLER; index++)
		{
			PadMacros::MacroButton& mb = this->s_macro_buttons[pad][index];

			if (!mb.trigger_state || mb.toggle_frequency == 0)
			{
				continue;
			}

			mb.toggle_counter--;

			if (mb.toggle_counter > 0)
			{
				continue;
			}

			mb.toggle_counter = mb.toggle_frequency;
			mb.toggle_state = !mb.toggle_state;
			ApplyMacroButton(pad, mb);
		}
	}
}