// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockViews.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/core/TabBar.h>
#include <kddockwidgets/qtwidgets/views/TabBar.h>
#include <kddockwidgets/qtwidgets/views/DockWidget.h>

#include <QMenu>

DockViewFactory::DockViewFactory(DockManager* dock_manager)
	: m_dock_manager(dock_manager)
{
	setParent(dock_manager);
}

KDDockWidgets::Core::View* DockViewFactory::createTabBar(
	KDDockWidgets::Core::TabBar* tabBar,
	KDDockWidgets::Core::View* parent) const
{
	KDDockWidgets::QtWidgets::TabBar* view =
		new KDDockWidgets::QtWidgets::TabBar(tabBar, KDDockWidgets::QtCommon::View_qt::asQWidget(parent));
	view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(
		view,
		&KDDockWidgets::QtWidgets::TabBar::customContextMenuRequested,
		this,
		&DockViewFactory::tabBarContextMenu);
	return view;
}

__noinline void breakme(const char* s)
{
}

void DockViewFactory::tabBarContextMenu(QPoint pos)
{
	KDDockWidgets::QtWidgets::TabBar* tab_bar = qobject_cast<KDDockWidgets::QtWidgets::TabBar*>(sender());
	int tab_index = tab_bar->tabAt(pos);

	QMenu* menu = new QMenu(tr("Dock Widget Menu"), tab_bar);

	QMenu* set_target_menu = menu->addMenu(tr("Set Target"));

	for (BreakPointCpu cpu : DEBUG_CPUS)
	{
		QAction* action = new QAction(DebugInterface::cpuName(cpu), menu);
		connect(action, &QAction::triggered, this, [this, tab_bar, tab_index, cpu]() {
			KDDockWidgets::Core::TabBar* tab_bar_controller = tab_bar->asController<KDDockWidgets::Core::TabBar>();
			if (!tab_bar_controller)
				return;

			KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
			if (!dock_controller)
				return;

			KDDockWidgets::QtWidgets::DockWidget* dock_view =
				dynamic_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());
			if (!dock_view)
				return;

			DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
			if (!widget)
				return;

			if (!widget->setCpuOverride(cpu))
				m_dock_manager->recreateDebuggerWidget(dock_view->uniqueName());

			m_dock_manager->retranslateDockWidget(dock_controller);
		});
		set_target_menu->addAction(action);
	}

	set_target_menu->addSeparator();

	QAction* inherit_action = new QAction(tr("Inherit From Layout"), menu);
	connect(inherit_action, &QAction::triggered, this, [this, tab_bar, tab_index]() {
		KDDockWidgets::Core::TabBar* tab_bar_controller = tab_bar->asController<KDDockWidgets::Core::TabBar>();
		if (!tab_bar_controller)
			return;

		KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
		if (!dock_controller)
			return;

		KDDockWidgets::QtWidgets::DockWidget* dock_view =
			dynamic_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());
		if (!dock_view)
			return;

		DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
		if (!widget)
			return;

		if (!widget->setCpuOverride(std::nullopt))
			m_dock_manager->recreateDebuggerWidget(dock_view->uniqueName());

		m_dock_manager->retranslateDockWidget(dock_controller);
	});
	set_target_menu->addAction(inherit_action);

	menu->popup(tab_bar->mapToGlobal(pos));
}
