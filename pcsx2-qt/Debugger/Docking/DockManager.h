// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockLayout.h"
#include "Debugger/Docking/DockMenuBar.h"

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Draggable_p.h>

#include <QtCore/QPointer>
#include <QtWidgets/QPushButton>
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

	void switchToLayout(DockLayout::Index layout_index, bool blink_tab = false);
	bool switchToLayoutWithCPU(BreakPointCpu cpu, bool blink_tab = false);

	void loadLayouts();
	bool saveLayouts();
	bool saveCurrentLayout();

	QString currentLayoutName();
	bool canResetCurrentLayout();

	void resetCurrentLayout();
	void resetDefaultLayouts();
	void resetAllLayouts();

	void createToolsMenu(QMenu* menu);
	void createWindowsMenu(QMenu* menu);

	QWidget* createMenuBar(QWidget* original_menu_bar);
	void updateLayoutSwitcher();
	void newLayoutClicked();
	void openLayoutSwitcherContextMenu(const QPoint& pos, QTabBar* layout_switcher);
	void editLayoutClicked(DockLayout::Index layout_index);
	void resetLayoutClicked(DockLayout::Index layout_index);
	void deleteLayoutClicked(DockLayout::Index layout_index);
	void layoutSwitcherTabMoved(DockLayout::Index from_index, DockLayout::Index to_index);

	bool hasNameConflict(const QString& name, DockLayout::Index layout_index);

	void updateDockWidgetTitles();

	const std::map<QString, QPointer<DebuggerView>>& debuggerViews();
	size_t countDebuggerViewsOfType(const char* type);
	void recreateDebuggerView(const QString& unique_name);
	void destroyDebuggerView(const QString& unique_name);
	void setPrimaryDebuggerView(DebuggerView* widget, bool is_primary);
	void switchToDebuggerView(DebuggerView* widget);

	void updateTheme();

	bool isLayoutLocked();
	void setLayoutLockedAndSaveSetting(bool locked);
	void setLayoutLocked(bool locked, bool save_setting);
	void updateToolBarLockState();

	std::optional<BreakPointCpu> cpu();

private:
	static KDDockWidgets::Core::DockWidget* dockWidgetFactory(const QString& name);
	static bool dragAboutToStart(KDDockWidgets::Core::Draggable* draggable);

	std::vector<DockLayout> m_layouts;
	DockLayout::Index m_current_layout = DockLayout::INVALID_INDEX;

	DockMenuBar* m_menu_bar = nullptr;

	bool m_layout_locked = true;
};
