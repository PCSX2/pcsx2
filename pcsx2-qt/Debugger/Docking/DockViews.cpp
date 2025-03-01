// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockViews.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/DebuggerWindow.h"
#include "Debugger/Docking/DockManager.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/core/TabBar.h>
#include <kddockwidgets/core/indicators/SegmentedDropIndicatorOverlay.h>
#include <kddockwidgets/qtwidgets/views/DockWidget.h>

#include <QtGui/QActionGroup>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>

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

KDDockWidgets::Core::View* DockViewFactory::createSegmentedDropIndicatorOverlayView(
	KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller,
	KDDockWidgets::Core::View* parent) const
{
	return new DockDropIndicator(controller, KDDockWidgets::QtCommon::View_qt::asQWidget(parent));
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
	// The LayoutSaver class will close a bunch of dock widgets. We only want to
	// delete the dock widgets when they're being closed by the user.
	if (KDDockWidgets::LayoutSaver::restoreInProgress())
		return;

	if (!open && g_debugger_window)
		g_debugger_window->dockManager().destroyDebuggerWidget(uniqueName());
}

// *****************************************************************************

DockTitleBar::DockTitleBar(KDDockWidgets::Core::TitleBar* controller, KDDockWidgets::Core::View* parent)
	: KDDockWidgets::QtWidgets::TitleBar(controller, parent)
{
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (g_debugger_window && !g_debugger_window->dockManager().isLayoutLocked())
		KDDockWidgets::QtWidgets::TitleBar::mouseDoubleClickEvent(event);
	else
		event->ignore();
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
	connect(this, &DockTabBar::customContextMenuRequested, this, &DockTabBar::openContextMenu);
}

void DockTabBar::openContextMenu(QPoint pos)
{
	if (!g_debugger_window)
		return;

	int tab_index = tabAt(pos);

	// Filter out the placeholder widget displayed when there are no layouts.
	auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
	if (!widget)
		return;

	size_t dock_widgets_of_type = g_debugger_window->dockManager().countDebuggerWidgetsOfType(
		widget->metaObject()->className());

	QMenu* menu = new QMenu(tr("Dock Widget Context Menu"), this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* rename_action = menu->addAction(tr("Rename"));
	connect(rename_action, &QAction::triggered, this, [this, tab_index]() {
		if (!g_debugger_window)
			return;

		auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
		if (!widget)
			return;

		bool ok;
		QString new_name = QInputDialog::getText(
			this, tr("Rename Window"), tr("New name:"), QLineEdit::Normal, widget->displayNameWithoutSuffix(), &ok);
		if (!ok)
			return;

		widget->setCustomDisplayName(new_name);
		g_debugger_window->dockManager().updateDockWidgetTitles();
	});

	QAction* reset_name_action = menu->addAction(tr("Reset Name"));
	reset_name_action->setEnabled(!widget->customDisplayName().isEmpty());
	connect(reset_name_action, &QAction::triggered, this, [this, tab_index] {
		if (!g_debugger_window)
			return;

		auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
		if (!widget)
			return;

		widget->setCustomDisplayName(QString());
		g_debugger_window->dockManager().updateDockWidgetTitles();
	});

	QAction* primary_action = menu->addAction(tr("Primary"));
	primary_action->setCheckable(true);
	primary_action->setChecked(widget->isPrimary());
	primary_action->setEnabled(dock_widgets_of_type > 1);
	connect(primary_action, &QAction::triggered, this, [this, tab_index](bool checked) {
		if (!g_debugger_window)
			return;

		auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
		if (!widget)
			return;

		g_debugger_window->dockManager().setPrimaryDebuggerWidget(widget, checked);
	});

	QMenu* set_target_menu = menu->addMenu(tr("Set Target"));
	QActionGroup* set_target_group = new QActionGroup(menu);
	set_target_group->setExclusive(true);

	for (BreakPointCpu cpu : DEBUG_CPUS)
	{
		const char* long_cpu_name = DebugInterface::longCpuName(cpu);
		const char* cpu_name = DebugInterface::cpuName(cpu);
		QString text = QString("%1 (%2)").arg(long_cpu_name).arg(cpu_name);

		QAction* cpu_action = set_target_menu->addAction(text);
		cpu_action->setCheckable(true);
		cpu_action->setChecked(widget->cpuOverride().has_value() && *widget->cpuOverride() == cpu);
		connect(cpu_action, &QAction::triggered, this, [this, tab_index, cpu]() {
			setCpuOverrideForTab(tab_index, cpu);
		});
		set_target_group->addAction(cpu_action);
	}

	set_target_menu->addSeparator();

	QAction* inherit_action = set_target_menu->addAction(tr("Inherit From Layout"));
	inherit_action->setCheckable(true);
	inherit_action->setChecked(!widget->cpuOverride().has_value());
	connect(inherit_action, &QAction::triggered, this, [this, tab_index]() {
		setCpuOverrideForTab(tab_index, std::nullopt);
	});
	set_target_group->addAction(inherit_action);

	QAction* close_action = menu->addAction(tr("Close"));
	connect(close_action, &QAction::triggered, this, [this, tab_index]() {
		if (!g_debugger_window)
			return;

		auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
		if (!widget)
			return;

		g_debugger_window->dockManager().destroyDebuggerWidget(widget->uniqueName());
	});

	menu->popup(mapToGlobal(pos));
}

void DockTabBar::setCpuOverrideForTab(int tab_index, std::optional<BreakPointCpu> cpu_override)
{
	if (!g_debugger_window)
		return;

	auto [widget, controller, view] = widgetsFromTabIndex(tab_index);
	if (!widget)
		return;

	if (!widget->setCpuOverride(cpu_override))
		g_debugger_window->dockManager().recreateDebuggerWidget(view->uniqueName());

	g_debugger_window->dockManager().updateDockWidgetTitles();
}

DockTabBar::WidgetsFromTabIndexResult DockTabBar::widgetsFromTabIndex(int tab_index)
{
	KDDockWidgets::Core::TabBar* tab_bar_controller = asController<KDDockWidgets::Core::TabBar>();
	if (!tab_bar_controller)
		return {};

	KDDockWidgets::Core::DockWidget* dock_controller = tab_bar_controller->dockWidgetAt(tab_index);
	if (!dock_controller)
		return {};

	auto dock_view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());

	DebuggerWidget* widget = qobject_cast<DebuggerWidget*>(dock_view->widget());
	if (!widget)
		return {};

	return {widget, dock_controller, dock_view};
}

void DockTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (g_debugger_window && !g_debugger_window->dockManager().isLayoutLocked())
		KDDockWidgets::QtWidgets::TabBar::mouseDoubleClickEvent(event);
	else
		event->ignore();
}

// *****************************************************************************

DockDropIndicator::DockDropIndicator(KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller, QWidget* parent)
	: KDDockWidgets::QtWidgets::SegmentedDropIndicatorOverlay(controller, parent)
{
}

void DockDropIndicator::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller =
		asController<KDDockWidgets::Core::SegmentedDropIndicatorOverlay>();

	const std::unordered_map<KDDockWidgets::DropLocation, QPolygon>& segments = controller->segments();

	for (KDDockWidgets::DropLocation location :
		{KDDockWidgets::DropLocation_Left,
			KDDockWidgets::DropLocation_Top,
			KDDockWidgets::DropLocation_Right,
			KDDockWidgets::DropLocation_Bottom,
			KDDockWidgets::DropLocation_Center,
			KDDockWidgets::DropLocation_OutterLeft,
			KDDockWidgets::DropLocation_OutterTop,
			KDDockWidgets::DropLocation_OutterRight,
			KDDockWidgets::DropLocation_OutterBottom})
	{
		auto segment = segments.find(location);
		if (segment == segments.end() || segment->second.size() < 2)
			continue;

		// Pick some nice colours.
		QColor fill = palette().highlight().color().lighter(150);
		QColor outline = palette().highlight().color().lighter(150);
		fill.setAlpha(150);
		outline.setAlpha(255);

		if (!segment->second.containsPoint(controller->hoveredPt(), Qt::OddEvenFill))
		{
			fill.setAlpha(fill.alpha() / 2);
			outline.setAlpha(outline.alpha() / 2);
		}

		painter.setBrush(fill);

		QPen pen(outline);
		pen.setWidth(1);
		painter.setPen(pen);

		int margin = KDDockWidgets::Core::SegmentedDropIndicatorOverlay::s_segmentGirth * 2;

		// Make sure the rectangles don't intersect with each other.
		QRect rect;
		switch (location)
		{
			case KDDockWidgets::DropLocation_Top:
			case KDDockWidgets::DropLocation_Bottom:
			case KDDockWidgets::DropLocation_OutterTop:
			case KDDockWidgets::DropLocation_OutterBottom:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(margin, 4, margin, 4));
				break;
			}
			case KDDockWidgets::DropLocation_Left:
			case KDDockWidgets::DropLocation_Right:
			case KDDockWidgets::DropLocation_OutterLeft:
			case KDDockWidgets::DropLocation_OutterRight:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(4, margin, 4, margin));
				break;
			}
			default:
			{
				rect = segment->second.boundingRect().marginsRemoved(QMargins(4, 4, 4, 4));
				break;
			}
		}

		painter.drawRoundedRect(rect, 2, 2);
	}
}
