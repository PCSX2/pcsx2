// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Input/InputSource.h"

#include <array>
#include <mutex>
#include <vector>

class SettingsInterface;

/// Controllers plugged into the MiSTer, streamed back to us over UDP 32101.
///
/// A first-class InputSource rather than a bespoke pad backend, so MiSTer pads appear in
/// the normal controller-binding UI and rebind like any other device.
///
/// This does not own the connection. The Groovy client is a single shared object owned by
/// the video path (GroovyMiSTer::Output), which subscribes for inputs during its CMD_INIT
/// handshake; this polls the cached state, and only while that connection is live - hence
/// the gmw_is_connected() guard in PollEvents(). The input socket is separate from the
/// video socket, so polling never races the sender thread.
class GroovyMiSTerInputSource final : public InputSource
{
public:
	enum : u32
	{
		// The Groovy protocol carries two joysticks.
		NUM_CONTROLLERS = 2,
		// GMW_JOY_RIGHT/LEFT/DOWN/UP then B1..B12. Under inputs v2 the core's button space is
		// PlayStation-semantic and fixed: B1..B12 = Cross, Circle, Square, Triangle, L1, R1,
		// Select, Start, L2, R2, L3, R3. Bits 14/15 (L3/R3) arrive even from a v1 16-bit
		// packet, so all 16 buttons work whichever protocol was negotiated.
		NUM_BUTTONS = 16,
		// Motors for MiSTer-side rumble (inputs v2): 0 = large/strong, 1 = small/weak.
		NUM_MOTORS = 2,
	};

	enum : u32
	{
		AXIS_LEFTX,
		AXIS_LEFTY,
		AXIS_RIGHTX,
		AXIS_RIGHTY,
		// Analog triggers (0..255 on the wire), only streamed on a v2 analog session;
		// pads without analog triggers just report the digital L2/R2 bits instead.
		AXIS_LEFTTRIGGER,
		AXIS_RIGHTTRIGGER,
		NUM_AXES,
	};

	GroovyMiSTerInputSource();
	~GroovyMiSTerInputSource() override;

	bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	bool ReloadDevices() override;
	void Shutdown() override;
	bool IsInitialized() override;

	void PollEvents() override;

	std::vector<std::pair<std::string, std::string>> EnumerateDevices() override;
	std::vector<InputBindingKey> EnumerateMotors() override;
	bool GetGenericBindingMapping(const std::string_view device, InputManager::GenericInputBindingMapping* mapping) override;
	InputLayout GetControllerLayout(u32 index) override;
	void UpdateMotorState(InputBindingKey key, float intensity) override;
	void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity,
		float small_intensity) override;

	std::optional<InputBindingKey> ParseKeyString(const std::string_view device, const std::string_view binding) override;
	TinyString ConvertKeyToString(InputBindingKey key, bool display = false, bool migration = false) override;
	TinyString ConvertKeyToIcon(InputBindingKey key) override;

private:
	struct ControllerData
	{
		u32 last_buttons = 0;
		// Sticks are signed (-128..127), triggers unsigned (0..255); s16 holds both raw
		// encodings exactly, so change detection is exact. NormalizeAxis() maps them.
		std::array<s16, NUM_AXES> last_axes{};
		// Last rumble values sent to the MiSTer (large, small). The Groovy core repeats the
		// last value until replaced, so we must only send on change.
		std::array<u8, NUM_MOTORS> last_rumble{};
	};

	static float NormalizeAxis(u32 axis, s16 raw);

	void CheckForStateChanges(u32 index, u32 buttons, const std::array<s16, NUM_AXES>& axes);
	void ReleaseHeldInputs();
	void SendRumble(u32 index);

	std::array<ControllerData, NUM_CONTROLLERS> m_controllers;
	bool m_initialized = false;
	// The MiSTer streams pad state only while the video connection is live; this tracks that
	// so held buttons can be released when the stream drops. Not device presence: both pads
	// stay advertised while the source is enabled (see EnumerateDevices).
	bool m_streaming = false;
};
