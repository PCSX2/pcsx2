// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockViews.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/DebuggerWindow.h"
#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/core/TabBar.h>
#include <kddockwidgets/qtwidgets/views/DockWidget.h>

#include <QMenu>

KDDockWidgets::Core::View* DockViewFactory::createDockWidget(
	const QString& unique_name,
	KDDockWidgets::DockWidgetOptions options,
	KDDockWidgets::LayoutSaverOptions layout_saver_options,
	Qt::WindowFlags window_flags) const
{
	return new DockWidget(unique_name, options, layout_saver_options, window_flags);
}

KDDockWidgets::Core::View* DockViewFactory::createTitleBar(
	KDDockWidgets::Core::TitleBar* controller,
	KDDockWidgets::Core::View* parent) const
{
	return new DockTitleBar(controller, parent);
}

KDDockWidgets::Core::View* DockViewFactory::createStack(
	KDDockWidgets::Core::Stack* controller,
	KDDockWidgets::Core::View* parent) const
{
	return new DockStack(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent));
}

KDDockWidgets::Core::View* DockViewFactory::createTabBar(
	KDDockWidgets::Core::TabBar* tabBar,
	KDDockWidgets::Core::View* parent) const
{
	return new DockTabBar(tabBar, KDDockWidgets::QtCommon::View_qt::asQWidget(parent));
}

// *****************************************************************************

DockWidget::DockWidget(
	const QString& unique_name,
	KDDockWidgets::DockWidgetOptions options,
	KDDockWidgets::LayoutSaverOptions layout_saver_options,
	Qt::WindowFlags window_flags)
	: KDDockWidgets::QtWidgets::DockWidget(unique_name, options, layout_saver_options, window_flags)
{
	connect(this, &DockWidget::isOpenChanged, this, &DockWidget::openStateChanged);
}

void DockWidget::openStateChanged(bool open)
{
	auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(sender());

	KDDockWidgets::Core::DockWidget* controller = view->asController<KDDockWidgets::Core::DockWidget>();
	if (!controller)
		return;

	if (!open && g_debugger_window)
		g_debugger_window->dockManager().dockWidgetClosed(controller);
}

// *****************************************************************************

DockTitleBar::DockTitleBar(KDDockWidgets::Core::TitleBar* controller, KDDockWidgets::Core::View* parent)
	: KDDockWidgets::QtWidgets::TitleBar(controller, parent)
{
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent* ev)
{
	if (g_debugger_window && !g_debugger_window->dockManager().isLayoutLocked())
		KDDockWidgets::QtWidgets::TitleBar::mouseDoubleClickEvent(ev);
	else
		ev->ignore();
}

// *****************************************************************************

DockStack::DockStack(KDDockWidgets::Core::Stack* controller, QWidget* parent)
	: KDDockWidgets::QtWidgets::Stack(controller, parent)
{
}

void DockStack::init()
{
	KDDockWidgets::QtWidgets::Stack::init();

	if (g_debugger_window)
	{
		bool locked = g_debugger_window->dockManager().isLayoutLocked();
		setTabsClosable(!locked);
	}
}

void DockStack::mouseDoubleClickEvent(QMouseEvent* ev)
{
	if (g_debugger_window && !g_debugger_window->dockManager().isLayoutLocked())
		KDDockWidgets::QtWidgets::Stack::mouseDoubleClickEvent(ev);
	else
		ev->ignore();
}

// *****************************************************************************

DockTabBar::DockTabBar(KDDockWidgets::Core::TabBar* controller, QWidget* parent)
	: KDDockWidgets::QtWidgets::TabBar(controller, parent)
{
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &DockTabBar::customContextMenuRequested, this, &DockTabBar::contextMenu);
}

void DockTabBar::contextMenu(QPoint pos)
{
	auto tab_bar = qobject_cast<KDDockWidgets::QtWidgets::TabBar*>(sender());
	int tab_index = tab_bar->tabAt(pos);

	// Filter out the placeholder widget displayed when there are no layouts.
	if (!hasDebuggerWidget(tab_index))
		return;

	QMenu* menu = new QMenu(tr("Dock Widget Menu"), tab_bar);

	QMenu* set_target_menu = menu->addMenu(tr("Set Target"));

	for (BreakPointCpu cpu : DEBUG_CPUS)
	{
		const char* long_cpu_name = DebugInterface::longCpuName(cpu);
		const char* cpu_name = DebugInterface::cpuName(cpu);
		QString text = QString("%1 (%2)").arg(long_cpu_name).arg(cpu_name);
		QAction* action = new QAction(text, menu);
		connect(action, &QAction::triggered, this, [tab_bar, tab_index, cpu]() {
			KDDockWidgets::Core::TabBar* tab_bar_controller = tab_bar->asController<KDDockWidgets::Core::TabBar>();
			if (!tab_bar_controller)
				return;

			KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
			if (!dock_controller)
				return;

			KDDockWidgets::QtWidgets::DockWidget* dock_view =
				static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());

			DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
			if (!widget)
				return;

			if (!g_debugger_window)
				return;

			if (!widget->setCpuOverride(cpu))
				g_debugger_window->dockManager().recreateDebuggerWidget(dock_view->uniqueName());

			g_debugger_window->dockManager().retranslateDockWidget(dock_controller);
		});
		set_target_menu->addAction(action);
	}

	set_target_menu->addSeparator();

	QAction* inherit_action = new QAction(tr("Inherit From Layout"), menu);
	connect(inherit_action, &QAction::triggered, this, [tab_bar, tab_index]() {
		KDDockWidgets::Core::TabBar* tab_bar_controller = tab_bar->asController<KDDockWidgets::Core::TabBar>();
		if (!tab_bar_controller)
			return;

		KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
		if (!dock_controller)
			return;

		KDDockWidgets::QtWidgets::DockWidget* dock_view =
			static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());

		DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
		if (!widget)
			return;

		if (!g_debugger_window)
			return;

		if (!widget->setCpuOverride(std::nullopt))
			g_debugger_window->dockManager().recreateDebuggerWidget(dock_view->uniqueName());

		g_debugger_window->dockManager().retranslateDockWidget(dock_controller);
	});
	set_target_menu->addAction(inherit_action);

	menu->popup(tab_bar->mapToGlobal(pos));
}

bool DockTabBar::hasDebuggerWidget(int tab_index)
{
	KDDockWidgets::Core::TabBar* tab_bar_controller = asController<KDDockWidgets::Core::TabBar>();
	if (!tab_bar_controller)
		return false;

	KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
	if (!dock_controller)
		return false;

	auto dock_view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());

	DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
	if (!widget)
		return false;

	return true;
}

void DockTabBar::mouseDoubleClickEvent(QMouseEvent* ev)
{
	if (g_debugger_window && !g_debugger_window->dockManager().isLayoutLocked())
		KDDockWidgets::QtWidgets::TabBar::mouseDoubleClickEvent(ev);
	else
		ev->ignore();
}
