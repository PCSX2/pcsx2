// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockLayout.h"

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Draggable_p.h>

#include <QtCore/QPointer>
#include <QtWidgets/QTabBar>

class DockManager : public QObject
{
	Q_OBJECT

public:
	DockManager(QObject* parent = nullptr);

	DockManager(const DockManager& rhs) = delete;
	DockManager& operator=(const DockManager& rhs) = delete;

	DockManager(DockManager&& rhs) = delete;
	DockManager& operator=(DockManager&&) = delete;

	// This needs to be called before any KDDockWidgets objects are created
	// including the debugger window itself.
	static void configureDockingSystem();

	template <typename... Args>
	DockLayout::Index createLayout(Args&&... args)
	{
		DockLayout::Index layout_index = m_layouts.size();

		if (m_layouts.empty())
		{
			// Delete the placeholder created in DockManager::deleteLayout.
			for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
				delete dock;
		}

		m_layouts.emplace_back(std::forward<Args>(args)..., layout_index);

		return layout_index;
	}

	bool deleteLayout(DockLayout::Index layout_index);

	void switchToLayout(DockLayout::Index layout_index);
	bool switchToLayoutWithCPU(BreakPointCpu cpu);

	void loadLayouts();
	bool saveLayouts();
	bool saveCurrentLayout();

	void resetAllLayouts();
	void resetDefaultLayouts();

	void createToolsMenu(QMenu* menu);
	void createWindowsMenu(QMenu* menu);

	QWidget* createLayoutSwitcher(QWidget* menu_bar);
	void updateLayoutSwitcher();
	void layoutSwitcherTabChanged(int index);
	void layoutSwitcherTabMoved(int from, int to);
	void layoutSwitcherContextMenu(QPoint pos);

	bool hasNameConflict(const std::string& name, DockLayout::Index layout_index);

	void retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget);
	void dockWidgetClosed(KDDockWidgets::Core::DockWidget* dock_widget);

	const std::map<QString, QPointer<DebuggerWidget>>& debuggerWidgets();
	void recreateDebuggerWidget(QString unique_name);
	void switchToDebuggerWidget(DebuggerWidget* widget);

	bool isLayoutLocked();
	void setLayoutLocked(bool locked);
	void updateToolBarLockState();

	std::optional<BreakPointCpu> cpu();

private:
	static KDDockWidgets::Core::DockWidget* dockWidgetFactory(const QString& name);
	static bool dragAboutToStart(KDDockWidgets::Core::Draggable* draggable);

	std::vector<DockLayout> m_layouts;
	DockLayout::Index m_current_layout = DockLayout::INVALID_INDEX;

	QTabBar* m_switcher = nullptr;
	int m_plus_tab_index = -1;
	int m_current_tab_index = -1;

	QMetaObject::Connection m_tab_connection;

	bool m_layout_locked = true;
};
