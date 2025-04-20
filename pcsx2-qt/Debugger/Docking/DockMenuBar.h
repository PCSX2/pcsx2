// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockLayout.h"

#include <QtWidgets/QMenuBar>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QWidget>

class DockMenuBarStyle;

// The widget that replaces the normal menu bar. This contains the original menu
// bar, the layout switcher and the layout locked/unlocked toggle button.
class DockMenuBar : public QWidget
{
	Q_OBJECT

public:
	DockMenuBar(QWidget* original_menu_bar, QWidget* parent = nullptr);

	void updateLayoutSwitcher(DockLayout::Index current_index, const std::vector<DockLayout>& layouts);

	void updateTheme();

	// Notify the menu bar that a new layout has been selected.
	void onCurrentLayoutChanged(DockLayout::Index current_index);

	// Notify the menu bar that the layout has been locked/unlocked.
	void onLockStateChanged(bool layout_locked);

	void startBlink(DockLayout::Index layout_index);
	void updateBlink();
	void stopBlink();

	int innerHeight() const;

Q_SIGNALS:
	void currentLayoutChanged(DockLayout::Index layout_index);
	void newButtonClicked();
	void layoutMoved(DockLayout::Index from_index, DockLayout::Index to_index);
	void lockButtonToggled(bool locked);

	void layoutSwitcherContextMenuRequested(const QPoint& pos, QTabBar* layout_switcher);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	void tabChanged(int index);

	QWidget* m_original_menu_bar;

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

	DockMenuBarStyle* m_style = nullptr;
};

// Fixes some theming issues relating to the menu bar, the layout switcher and
// the layout locked/unlocked toggle button.
class DockMenuBarStyle : public QProxyStyle
{
	Q_OBJECT

public:
	DockMenuBarStyle(QObject* parent = nullptr);

	void drawControl(
		ControlElement element,
		const QStyleOption* option,
		QPainter* painter,
		const QWidget* widget = nullptr) const override;

	QSize sizeFromContents(
		QStyle::ContentsType type,
		const QStyleOption* option,
		const QSize& contents_size,
		const QWidget* widget = nullptr) const override;
};
