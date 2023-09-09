// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifdef __APPLE__

#include <string>
#include <optional>

struct WindowInfo;

/// Helper functions for things that need Objective-C
namespace CocoaTools
{
	bool CreateMetalLayer(WindowInfo* wi);
	void DestroyMetalLayer(WindowInfo* wi);
	/// Add a handler to be run when macOS changes between dark and light themes
	void AddThemeChangeHandler(void* ctx, void(handler)(void* ctx));
	/// Remove a handler previously added using AddThemeChangeHandler with the given context
	void RemoveThemeChangeHandler(void* ctx);
	/// Get the bundle path to the actual application without any translocation fun
	std::optional<std::string> GetNonTranslocatedBundlePath();
	/// Move the given file to the trash, and return the path to its new location
	std::optional<std::string> MoveToTrash(std::string_view file);
	/// Launch the given application once this one quits
	bool DelayedLaunch(std::string_view file);
	/// Open a Finder window to the given URL
	bool ShowInFinder(std::string_view file);
}

#endif // __APPLE__
