// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

namespace DockUtils
{
	inline const constexpr int MAX_LAYOUT_NAME_SIZE = 40;
	inline const constexpr int MAX_DOCK_WIDGET_NAME_SIZE = 40;

	struct DockWidgetPair
	{
		KDDockWidgets::Core::DockWidget* controller = nullptr;
		KDDockWidgets::QtWidgets::DockWidget* view = nullptr;
	};

	DockWidgetPair dockWidgetFromName(const QString& unique_name);

	enum PreferredLocation
	{
		TOP_LEFT,
		TOP_MIDDLE,
		TOP_RIGHT,
		MIDDLE_LEFT,
		MIDDLE_MIDDLE,
		MIDDLE_RIGHT,
		BOTTOM_LEFT,
		BOTTOM_MIDDLE,
		BOTTOM_RIGHT
	};

	void insertDockWidgetAtPreferredLocation(
		KDDockWidgets::Core::DockWidget* dock_widget,
		PreferredLocation location,
		KDDockWidgets::QtWidgets::MainWindow* window);
} // namespace DockUtils
