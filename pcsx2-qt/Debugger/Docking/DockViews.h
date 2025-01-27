// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <kddockwidgets/qtwidgets/ViewFactory.h>

class DockManager;

class DockViewFactory : public KDDockWidgets::QtWidgets::ViewFactory
{
	Q_OBJECT

public:
	DockViewFactory(DockManager* dock_manager);

	KDDockWidgets::Core::View* createTabBar(
		KDDockWidgets::Core::TabBar* tabBar,
		KDDockWidgets::Core::View* parent) const override;

	void tabBarContextMenu(QPoint pos);

private:
	DockManager* m_dock_manager;
};
