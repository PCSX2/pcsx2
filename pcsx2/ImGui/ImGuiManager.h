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

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>

struct ImFont;

union InputBindingKey;
enum class GenericInputBinding : u8;

namespace ImGuiManager
{
	/// Sets the path to the font to use. Empty string means to use the default.
	void SetFontPath(std::string path);

	/// Sets the glyph range to use when loading fonts.
	void SetFontRange(const u16* range);

	/// Initializes ImGui, creates fonts, etc.
	bool Initialize();

	/// Initializes fullscreen UI.
	bool InitializeFullscreenUI();

	/// Frees all ImGui resources.
	void Shutdown(bool clear_state);

	/// Updates internal state when the window is size.
	void WindowResized();

	/// Updates scaling of the on-screen elements.
	void UpdateScale();

	/// Call at the beginning of the frame to set up ImGui state.
	void NewFrame();

	/// Call when skipping rendering a frame, to update internal state.
	void SkipFrame();

	/// Renders any on-screen display elements.
	void RenderOSD();

	/// Returns the scale of all on-screen elements.
	float GetGlobalScale();

	/// Returns true if fullscreen fonts are present.
	bool HasFullscreenFonts();

	/// Allocates/adds fullscreen fonts if they're not loaded.
	bool AddFullscreenFontsIfMissing();

	/// Returns the standard font for external drawing.
	ImFont* GetStandardFont();

	/// Returns the fixed-width font for external drawing.
	ImFont* GetFixedFont();

	/// Returns the medium font for external drawing, scaled by ImGuiFullscreen.
	/// This font is allocated on demand.
	ImFont* GetMediumFont();

	/// Returns the large font for external drawing, scaled by ImGuiFullscreen.
	/// This font is allocated on demand.
	ImFont* GetLargeFont();

	/// Returns true if imgui wants to intercept text input.
	bool WantsTextInput();

	/// Called on the UI or CPU thread in response to a key press. String is UTF-8.
	void AddTextInput(std::string str);

	/// Called on the UI or CPU thread in response to mouse movement.
	void UpdateMousePosition(float x, float y);

	/// Called on the CPU thread in response to a mouse button press.
	/// Returns true if ImGui intercepted the event, and regular handlers should not execute.
	bool ProcessPointerButtonEvent(InputBindingKey key, float value);

	/// Called on the CPU thread in response to a mouse wheel movement.
	/// Returns true if ImGui intercepted the event, and regular handlers should not execute.
	bool ProcessPointerAxisEvent(InputBindingKey key, float value);

	/// Called on the CPU thread in response to a key press.
	/// Returns true if ImGui intercepted the event, and regular handlers should not execute.
	bool ProcessHostKeyEvent(InputBindingKey key, float value);

	/// Called on the CPU thread when any input event fires. Allows imgui to take over controller navigation.
	bool ProcessGenericInputEvent(GenericInputBinding key, float value);
} // namespace ImGuiManager

namespace Host
{
	/// Called by ImGuiManager when the cursor enters a text field. The host may choose to open an on-screen
	/// keyboard for devices without a physical keyboard.
	void BeginTextInput();

	/// Called by ImGuiManager when the cursor leaves a text field.
	void EndTextInput();
} // namespace Host