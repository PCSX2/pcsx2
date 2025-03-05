// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/qtwidgets/ViewFactory.h>
#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/Stack.h>
#include <kddockwidgets/qtwidgets/views/TitleBar.h>
#include <kddockwidgets/qtwidgets/views/TabBar.h>

class DebuggerWidget;
class DockManager;

class DockViewFactory : public KDDockWidgets::QtWidgets::ViewFactory
{
	Q_OBJECT

public:
	KDDockWidgets::Core::View* createDockWidget(
		const QString& unique_name,
		KDDockWidgets::DockWidgetOptions options = {},
		KDDockWidgets::LayoutSaverOptions layout_saver_options = {},
		Qt::WindowFlags window_flags = {}) const override;

	KDDockWidgets::Core::View* createTitleBar(
		KDDockWidgets::Core::TitleBar* controller,
		KDDockWidgets::Core::View* parent) const override;

	KDDockWidgets::Core::View* createStack(
		KDDockWidgets::Core::Stack* controller,
		KDDockWidgets::Core::View* parent) const override;

	KDDockWidgets::Core::View* createTabBar(
		KDDockWidgets::Core::TabBar* tabBar,
		KDDockWidgets::Core::View* parent) const override;

	KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* createClassicIndicatorWindow(
		KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators,
		KDDockWidgets::Core::View* parent) const override;

	KDDockWidgets::Core::ClassicIndicatorWindowViewInterface* createFallbackClassicIndicatorWindow(
		KDDockWidgets::Core::ClassicDropIndicatorOverlay* classic_indicators,
		KDDockWidgets::Core::View* parent) const;

	KDDockWidgets::Core::View* createSegmentedDropIndicatorOverlayView(
		KDDockWidgets::Core::SegmentedDropIndicatorOverlay* controller,
		KDDockWidgets::Core::View* parent) const override;
};

class DockWidget : public KDDockWidgets::QtWidgets::DockWidget
{
	Q_OBJECT

public:
	DockWidget(
		const QString& unique_name,
		KDDockWidgets::DockWidgetOptions options,
		KDDockWidgets::LayoutSaverOptions layout_saver_options,
		Qt::WindowFlags window_flags);

protected:
	void openStateChanged(bool open);
};

class DockTitleBar : public KDDockWidgets::QtWidgets::TitleBar
{
	Q_OBJECT

public:
	DockTitleBar(KDDockWidgets::Core::TitleBar* controller, KDDockWidgets::Core::View* parent = nullptr);

protected:
	void mouseDoubleClickEvent(QMouseEvent* event) override;
};

class DockStack : public KDDockWidgets::QtWidgets::Stack
{
	Q_OBJECT

public:
	DockStack(KDDockWidgets::Core::Stack* controller, QWidget* parent = nullptr);

	void init() override;

protected:
	void mouseDoubleClickEvent(QMouseEvent* event) override;
};

class DockTabBar : public KDDockWidgets::QtWidgets::TabBar
{
	Q_OBJECT

public:
	DockTabBar(KDDockWidgets::Core::TabBar* controller, QWidget* parent = nullptr);

protected:
	void openContextMenu(QPoint pos);

	struct WidgetsFromTabIndexResult
	{
		DebuggerWidget* debugger_widget = nullptr;
		KDDockWidgets::Core::DockWidget* controller = nullptr;
		KDDockWidgets::QtWidgets::DockWidget* view = nullptr;
	};

	void setCpuOverrideForTab(int tab_index, std::optional<BreakPointCpu> cpu_override);
	WidgetsFromTabIndexResult widgetsFromTabIndex(int tab_index);

	void mouseDoubleClickEvent(QMouseEvent* event) override;
};
