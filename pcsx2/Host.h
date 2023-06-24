/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "fmt/format.h"

#include <ctime>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class VsyncMode;

class SettingsInterface;

namespace Host
{
	/// Typical durations for OSD messages.
	static constexpr float OSD_CRITICAL_ERROR_DURATION = 20.0f;
	static constexpr float OSD_ERROR_DURATION = 15.0f;
	static constexpr float OSD_WARNING_DURATION = 10.0f;
	static constexpr float OSD_INFO_DURATION = 5.0f;
	static constexpr float OSD_QUICK_DURATION = 2.5f;

	/// Reads a file from the resources directory of the application.
	/// This may be outside of the "normal" filesystem on platforms such as Mac.
	std::optional<std::vector<u8>> ReadResourceFile(const char* filename);

	/// Reads a resource file file from the resources directory as a string.
	std::optional<std::string> ReadResourceFileToString(const char* filename);

	/// Returns the modified time of a resource.
	std::optional<std::time_t> GetResourceFileTimestamp(const char* filename);

	/// Returns a localized version of the specified string within the specified context.
	/// The pointer is guaranteed to be valid until the next language change.
	const char* TranslateToCString(const std::string_view& context, const std::string_view& msg);

	/// Returns a localized version of the specified string within the specified context.
	/// The view is guaranteed to be valid until the next language change.
	/// NOTE: When passing this to fmt, positional arguments should be used in the base string, as
	/// not all locales follow the same word ordering.
	std::string_view TranslateToStringView(const std::string_view& context, const std::string_view& msg);

	/// Returns a localized version of the specified string within the specified context.
	std::string TranslateToString(const std::string_view& context, const std::string_view& msg);

	/// Clears the translation cache. All previously used strings should be considered invalid.
	void ClearTranslationCache();

	/// Adds OSD messages, duration is in seconds.
	void AddOSDMessage(std::string message, float duration = 2.0f);
	void AddKeyedOSDMessage(std::string key, std::string message, float duration = 2.0f);
	void AddIconOSDMessage(std::string key, const char* icon, const std::string_view& message, float duration = 2.0f);
	void RemoveKeyedOSDMessage(std::string key);
	void ClearOSDMessages();

	/// Displays an asynchronous error on the UI thread, i.e. doesn't block the caller.
	void ReportErrorAsync(const std::string_view& title, const std::string_view& message);
	void ReportFormattedErrorAsync(const std::string_view& title, const char* format, ...);

	/// Displays a synchronous confirmation on the UI thread, i.e. blocks the caller.
	bool ConfirmMessage(const std::string_view& title, const std::string_view& message);
	bool ConfirmFormattedMessage(const std::string_view& title, const char* format, ...);

	/// Opens a URL, using the default application.
	void OpenURL(const std::string_view& url);

	/// Copies the provided text to the host's clipboard, if present.
	bool CopyTextToClipboard(const std::string_view& text);

	/// Requests settings reset. Can be called from any thread, will call back and apply on the CPU thread.
	bool RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui);

	/// Requests a specific display window size.
	void RequestResizeHostDisplay(s32 width, s32 height);

	/// Safely executes a function on the VM thread.
	void RunOnCPUThread(std::function<void()> function, bool block = false);

	/// Asynchronously starts refreshing the game list.
	void RefreshGameListAsync(bool invalidate_cache);

	/// Cancels game list refresh, if there is one in progress.
	void CancelGameListRefresh();

	/// Requests shut down and exit of the hosting application. This may not actually exit,
	/// if the user cancels the shutdown confirmation.
	void RequestExit(bool allow_confirm);

	/// Requests shut down of the current virtual machine.
	void RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state);

	/// Base setting retrieval, bypasses layers.
	std::string GetBaseStringSettingValue(const char* section, const char* key, const char* default_value = "");
	bool GetBaseBoolSettingValue(const char* section, const char* key, bool default_value = false);
	int GetBaseIntSettingValue(const char* section, const char* key, int default_value = 0);
	uint GetBaseUIntSettingValue(const char* section, const char* key, uint default_value = 0);
	float GetBaseFloatSettingValue(const char* section, const char* key, float default_value = 0.0f);
	double GetBaseDoubleSettingValue(const char* section, const char* key, double default_value = 0.0);
	std::vector<std::string> GetBaseStringListSetting(const char* section, const char* key);

	/// Allows the emucore to write settings back to the frontend. Use with care.
	/// You should call CommitBaseSettingChanges() after finishing writing, or it may not be written to disk.
	void SetBaseBoolSettingValue(const char* section, const char* key, bool value);
	void SetBaseIntSettingValue(const char* section, const char* key, int value);
	void SetBaseUIntSettingValue(const char* section, const char* key, uint value);
	void SetBaseFloatSettingValue(const char* section, const char* key, float value);
	void SetBaseStringSettingValue(const char* section, const char* key, const char* value);
	void SetBaseStringListSettingValue(const char* section, const char* key, const std::vector<std::string>& values);
	bool AddBaseValueToStringList(const char* section, const char* key, const char* value);
	bool RemoveBaseValueFromStringList(const char* section, const char* key, const char* value);
	bool ContainsBaseSettingValue(const char* section, const char* key);
	void RemoveBaseSettingValue(const char* section, const char* key);
	void CommitBaseSettingChanges();

	/// Settings access, thread-safe.
	std::string GetStringSettingValue(const char* section, const char* key, const char* default_value = "");
	bool GetBoolSettingValue(const char* section, const char* key, bool default_value = false);
	int GetIntSettingValue(const char* section, const char* key, int default_value = 0);
	uint GetUIntSettingValue(const char* section, const char* key, uint default_value = 0);
	float GetFloatSettingValue(const char* section, const char* key, float default_value = 0.0f);
	double GetDoubleSettingValue(const char* section, const char* key, double default_value = 0.0);
	std::vector<std::string> GetStringListSetting(const char* section, const char* key);

	/// Direct access to settings interface. Must hold the lock when calling GetSettingsInterface() and while using it.
	std::unique_lock<std::mutex> GetSettingsLock();
	SettingsInterface* GetSettingsInterface();

	/// Returns the settings interface that controller bindings should be loaded from.
	/// If an input profile is being used, this will be the input layer, otherwise the layered interface.
	SettingsInterface* GetSettingsInterfaceForBindings();

	/// Sets host-specific default settings.
	void SetDefaultUISettings(SettingsInterface& si);

	namespace Internal
	{
		/// Retrieves the base settings layer. Must call with lock held.
		SettingsInterface* GetBaseSettingsLayer();

		/// Retrieves the game settings layer, if present. Must call with lock held.
		SettingsInterface* GetGameSettingsLayer();

		/// Retrieves the input settings layer, if present. Must call with lock held.
		SettingsInterface* GetInputSettingsLayer();

		/// Sets the base settings layer. Should be called by the host at initialization time.
		void SetBaseSettingsLayer(SettingsInterface* sif);

		/// Sets the game settings layer. Called by VMManager when the game changes.
		void SetGameSettingsLayer(SettingsInterface* sif);

		/// Sets the input profile settings layer. Called by VMManager when the game changes.
		void SetInputSettingsLayer(SettingsInterface* sif);

		/// Implementation to retrieve a translated string.
		s32 GetTranslatedStringImpl(const std::string_view& context, const std::string_view& msg, char* tbuf, size_t tbuf_space);
	} // namespace Internal
} // namespace Host

// Helper macros for retrieving translated strings.
#define TRANSLATE(context, msg) Host::TranslateToCString(context, msg)
#define TRANSLATE_SV(context, msg) Host::TranslateToStringView(context, msg)
#define TRANSLATE_STR(context, msg) Host::TranslateToString(context, msg)

// Does not translate the string at runtime, but allows the UI to in its own way.
#define TRANSLATE_NOOP(context, msg) msg
