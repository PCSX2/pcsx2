// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DockUtils.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/KDDockWidgets.h>

class MD5Digest;

class DebuggerWidget;
struct DebuggerWidgetParameters;

namespace DockTables
{
	struct DebuggerWidgetDescription
	{
		DebuggerWidget* (*create_widget)(const DebuggerWidgetParameters& parameters);

		// The untranslated string displayed as the dock widget tab text.
		const char* display_name;

		// This is used to determine which group dock widgets of this type are
		// added to when they're opened from the Windows menu.
		DockUtils::PreferredLocation preferred_location;
	};

	extern const std::map<std::string, DebuggerWidgetDescription> DEBUGGER_WIDGETS;

	enum class DefaultDockGroup
	{
		ROOT = -1,
		TOP_RIGHT = 0,
		BOTTOM = 1,
		TOP_LEFT = 2
	};

	struct DefaultDockGroupDescription
	{
		KDDockWidgets::Location location;
		DefaultDockGroup parent;
	};

	extern const std::vector<DefaultDockGroupDescription> DEFAULT_DOCK_GROUPS;

	struct DefaultDockWidgetDescription
	{
		std::string type;
		DefaultDockGroup group;
	};

	struct DefaultDockLayout
	{
		std::string name;
		BreakPointCpu cpu;
		std::vector<DefaultDockGroupDescription> groups;
		std::vector<DefaultDockWidgetDescription> widgets;
		std::set<std::string> toolbars;
	};

	extern const std::vector<DefaultDockLayout> DEFAULT_DOCK_LAYOUTS;

	const DefaultDockLayout* defaultLayout(const std::string& name);

	// This is used to determine if the user has updated and we need to recreate
	// the default layouts.
	const std::string& hashDefaultLayouts();

	void hashDefaultLayout(const DefaultDockLayout& layout, MD5Digest& md5);
	void hashDefaultGroup(const DefaultDockGroupDescription& group, MD5Digest& md5);
	void hashDefaultDockWidget(const DefaultDockWidgetDescription& widget, MD5Digest& md5);
} // namespace DockTables
