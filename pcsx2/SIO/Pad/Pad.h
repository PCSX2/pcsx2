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

#include "Config.h"
#include "SIO/Pad/PadTypes.h"

#include <memory>

class PadBase;

enum class GenericInputBinding : u8;
class SettingsInterface;
class StateWrapper;

namespace Pad
{
	bool Initialize();
	void Shutdown();

	// Returns the default type for the specified port.
	Pad::ControllerType GetDefaultPadType(u32 pad);

	// Reloads configuration.
	void LoadConfig(const SettingsInterface& si);

	// Restores default configuration.
	void SetDefaultControllerConfig(SettingsInterface& si);
	void SetDefaultHotkeyConfig(SettingsInterface& si);

	// Clears all bindings for a given port.
	void ClearPortBindings(SettingsInterface& si, u32 port);

	// Copies pad configuration from one interface (ini) to another.
	void CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si, bool copy_pad_config = true,
		bool copy_pad_bindings = true, bool copy_hotkey_bindings = true);

	// Returns a list of controller type names. Pair of [name, display name].
	const std::vector<std::pair<const char*, const char*>> GetControllerTypeNames();

	// Returns general information for the specified controller type.
	const ControllerInfo* GetControllerInfo(Pad::ControllerType type);
	const ControllerInfo* GetControllerInfoByName(const std::string_view& name);

	// Returns controller info based on the type in the config.
	// Needed because we can't just read EmuConfig when altering input profiles.
	const ControllerInfo* GetConfigControllerType(const SettingsInterface& si, const char* section, u32 port);

	// Performs automatic controller mapping with the provided list of generic mappings.
	bool MapController(
		SettingsInterface& si, u32 controller, const std::vector<std::pair<GenericInputBinding, std::string>>& mapping);

	// Returns a list of input profiles available.
	std::vector<std::string> GetInputProfileNames();
	std::string GetConfigSection(u32 pad_index);

	bool HasConnectedPad(u8 unifiedSlot);

	PadBase* GetPad(u8 port, u8 slot);
	PadBase* GetPad(const u8 unifiedSlot);

	// Sets the specified bind on a controller to the specified pressure (normalized to 0..1).
	void SetControllerState(u32 controller, u32 bind, float value);

	bool Freeze(StateWrapper& sw);

	// Sets the state of the specified macro button.
	void SetMacroButtonState(u32 pad, u32 index, bool state);
	void UpdateMacroButtons();
}; // namespace Pad
