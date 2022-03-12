/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

struct ImFont;

namespace ImGuiManager
{
	/// Initializes ImGui, creates fonts, etc.
	bool Initialize();

	/// Frees all ImGui resources.
	void Shutdown();

	/// Updates internal state when the window is size.
	void WindowResized();

	/// Updates scaling of the on-screen elements.
	void UpdateScale();

	/// Call at the beginning of the frame to set up ImGui state.
	void NewFrame();

	/// Renders any on-screen display elements.
	void RenderOSD();

	/// Returns the scale of all on-screen elements.
	float GetGlobalScale();

	/// Returns the standard font for external drawing.
	ImFont* GetStandardFont();

	/// Returns the fixed-width font for external drawing.
	ImFont* GetFixedFont();
} // namespace ImGuiManager

