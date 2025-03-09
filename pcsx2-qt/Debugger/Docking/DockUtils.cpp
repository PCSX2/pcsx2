// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockUtils.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/Group.h>

DockUtils::DockWidgetPair DockUtils::dockWidgetFromName(const QString& unique_name)
{
	KDDockWidgets::Vector<QString> names{unique_name};
	KDDockWidgets::Vector<KDDockWidgets::Core::DockWidget*> dock_widgets =
		KDDockWidgets::DockRegistry::self()->dockWidgets(names);
	if (dock_widgets.size() != 1 || !dock_widgets[0])
		return {};

	return {dock_widgets[0], static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_widgets[0]->view())};
}

void DockUtils::insertDockWidgetAtPreferredLocation(
	KDDockWidgets::Core::DockWidget* dock_widget,
	PreferredLocation location,
	KDDockWidgets::QtWidgets::MainWindow* window)
{
	int width = window->width();
	int height = window->height();
	int half_width = width / 2;
	int half_height = height / 2;

	QPoint preferred_location;
	switch (location)
	{
		case DockUtils::TOP_LEFT:
			preferred_location = {0, 0};
			break;
		case DockUtils::TOP_MIDDLE:
			preferred_location = {half_width, 0};
			break;
		case DockUtils::TOP_RIGHT:
			preferred_location = {width, 0};
			break;
		case DockUtils::MIDDLE_LEFT:
			preferred_location = {0, half_height};
			break;
		case DockUtils::MIDDLE_MIDDLE:
			preferred_location = {half_width, half_height};
			break;
		case DockUtils::MIDDLE_RIGHT:
			preferred_location = {width, half_height};
			break;
		case DockUtils::BOTTOM_LEFT:
			preferred_location = {0, height};
			break;
		case DockUtils::BOTTOM_MIDDLE:
			preferred_location = {half_width, height};
			break;
		case DockUtils::BOTTOM_RIGHT:
			preferred_location = {width, height};
			break;
	}

	// Find the dock group which is closest to the preferred location.
	KDDockWidgets::Core::Group* best_group = nullptr;
	int best_distance_squared = 0;

	for (KDDockWidgets::Core::Group* group_controller : KDDockWidgets::DockRegistry::self()->groups())
	{
		if (group_controller->isFloating())
			continue;

		auto group = static_cast<KDDockWidgets::QtWidgets::Group*>(group_controller->view());

		QPoint local_midpoint = group->pos() + QPoint(group->width() / 2, group->height() / 2);
		QPoint midpoint = group->mapTo(window, local_midpoint);
		QPoint delta = midpoint - preferred_location;
		int distance_squared = delta.x() * delta.x() + delta.y() * delta.y();

		if (!best_group || distance_squared < best_distance_squared)
		{
			best_group = group_controller;
			best_distance_squared = distance_squared;
		}
	}

	if (best_group && best_group->dockWidgetCount() > 0)
	{
		KDDockWidgets::Core::DockWidget* other_dock_widget = best_group->dockWidgetAt(0);
		other_dock_widget->addDockWidgetAsTab(dock_widget);
	}
	else
	{
		auto dock_view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_widget->view());
		window->addDockWidget(dock_view, KDDockWidgets::Location_OnTop);
	}
}
