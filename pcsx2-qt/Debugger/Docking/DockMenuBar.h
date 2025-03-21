// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockLayout.h"

#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QWidget>

class DockMenuBar : public QWidget
{
	Q_OBJECT

public:
	DockMenuBar(QWidget* original_menu_bar, QWidget* parent = nullptr);

	void updateLayoutSwitcher(DockLayout::Index current_index, const std::vector<DockLayout>& layouts);

	// Notify the menu bar that a new layout has been selected.
	void onCurrentLayoutChanged(DockLayout::Index current_index);

	// Notify the menu bar that the layout has been locked/unlocked.
	void onLockStateChanged(bool layout_locked);

	void startBlink(DockLayout::Index layout_index);
	void updateBlink();
	void stopBlink();

Q_SIGNALS:
	void currentLayoutChanged(DockLayout::Index layout_index);
	void newButtonClicked();
	void layoutMoved(DockLayout::Index from_index, DockLayout::Index to_index);
	void lockButtonToggled(bool locked);

	void layoutSwitcherContextMenuRequested(const QPoint& pos, QTabBar* layout_switcher);

private:
	void tabChanged(int index);


	QTabBar* m_layout_switcher;
	QMetaObject::Connection m_tab_connection;
	int m_plus_tab_index = -1;
	int m_current_tab_index = -1;
	bool m_ignore_current_tab_changed = false;

	QTimer* m_blink_timer = nullptr;
	int m_blink_tab = 0;
	int m_blink_stage = 0;

	QPushButton* m_layout_locked_toggle;
	bool m_ignore_lock_state_changed = false;
};
