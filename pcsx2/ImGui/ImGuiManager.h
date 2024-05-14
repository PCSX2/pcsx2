// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <string>
#include <vector>

struct ImFont;

union InputBindingKey;
enum class GenericInputBinding : u8;

namespace ImGuiManager
{
	/// Sets the path to the font to use. Empty string means to use the default.
	void SetFontPathAndRange(std::string path, std::vector<u16> range);

	/// Initializes ImGui, creates fonts, etc.
	bool Initialize();

	/// Initializes fullscreen UI.
	bool InitializeFullscreenUI();

	/// Frees all ImGui resources.
	void Shutdown(bool clear_state);

	/// Returns the size of the display window. Can be safely called from any thread.
	float GetWindowWidth();
	float GetWindowHeight();

	/// Updates internal state when the window is size.
	void WindowResized();

	/// Updates scaling of the on-screen elements.
	void RequestScaleUpdate();

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

	/// Returns true if imgui wants to intercept mouse input.
	bool WantsMouseInput();

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

	/// Sets an image and scale for a software cursor. Software cursors can be used for things like crosshairs.
	void SetSoftwareCursor(u32 index, std::string image_path, float image_scale, u32 multiply_color = 0xFFFFFF);
	bool HasSoftwareCursor(u32 index);
	void ClearSoftwareCursor(u32 index);

	/// Sets the position of a software cursor, used when we have relative coordinates such as controllers.
	void SetSoftwareCursorPosition(u32 index, float pos_x, float pos_y);

	/// Strips icon characters from a string.
	std::string StripIconCharacters(std::string_view str);
} // namespace ImGuiManager

namespace Host
{
	/// Called by ImGuiManager when the cursor enters a text field. The host may choose to open an on-screen
	/// keyboard for devices without a physical keyboard.
	void BeginTextInput();

	/// Called by ImGuiManager when the cursor leaves a text field.
	void EndTextInput();
} // namespace Host