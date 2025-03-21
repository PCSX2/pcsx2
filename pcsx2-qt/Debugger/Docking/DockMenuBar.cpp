// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockMenuBar.h"

#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QStyleOption>

DockMenuBar::DockMenuBar(QWidget* original_menu_bar, QWidget* parent)
	: QWidget(parent)
{
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(0, 2, 2, 0);
	setLayout(layout);

	QWidget* menu_wrapper = new QWidget;
	menu_wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	layout->addWidget(menu_wrapper);

	QHBoxLayout* menu_layout = new QHBoxLayout;
	menu_layout->setContentsMargins(0, 4, 0, 4);
	menu_wrapper->setLayout(menu_layout);

	menu_layout->addWidget(original_menu_bar);

	m_layout_switcher = new QTabBar;
	m_layout_switcher->setContentsMargins(0, 0, 0, 0);
	m_layout_switcher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	m_layout_switcher->setContextMenuPolicy(Qt::CustomContextMenu);
	m_layout_switcher->setMovable(true);
	layout->addWidget(m_layout_switcher);

	QWidget* spacer = new QWidget;
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	connect(m_layout_switcher, &QTabBar::tabMoved, this, [this](int from, int to) {
		DockLayout::Index from_index = static_cast<DockLayout::Index>(from);
		DockLayout::Index to_index = static_cast<DockLayout::Index>(to);
		emit layoutMoved(from_index, to_index);
	});

	connect(m_layout_switcher, &QTabBar::customContextMenuRequested, this, [this](const QPoint& pos) {
		emit layoutSwitcherContextMenuRequested(pos, m_layout_switcher);
	});

	m_blink_timer = new QTimer(this);
	connect(m_blink_timer, &QTimer::timeout, this, &DockMenuBar::updateBlink);

	m_layout_locked_toggle = new QPushButton;
	m_layout_locked_toggle->setCheckable(true);
	m_layout_locked_toggle->setFlat(true);
	connect(m_layout_locked_toggle, &QPushButton::clicked, this, [this](bool checked) {
		if (m_ignore_lock_state_changed)
			return;

		emit lockButtonToggled(checked);
	});
	layout->addWidget(m_layout_locked_toggle);
}

void DockMenuBar::updateLayoutSwitcher(DockLayout::Index current_index, const std::vector<DockLayout>& layouts)
{
	disconnect(m_tab_connection);

	for (int i = m_layout_switcher->count(); i > 0; i--)
		m_layout_switcher->removeTab(i - 1);

	for (const DockLayout& layout : layouts)
	{
		const char* cpu_name = DebugInterface::cpuName(layout.cpu());
		QString tab_name = QString("%1 (%2)").arg(layout.name()).arg(cpu_name);
		m_layout_switcher->addTab(tab_name);
	}

	m_plus_tab_index = m_layout_switcher->addTab("+");
	m_current_tab_index = current_index;

	if (current_index != DockLayout::INVALID_INDEX)
		m_layout_switcher->setCurrentIndex(current_index);
	else
		m_layout_switcher->setCurrentIndex(m_plus_tab_index);

	// If we don't have any layouts, the currently selected tab will never be
	// changed, so we respond to all clicks instead.
	if (m_plus_tab_index > 0)
		m_tab_connection = connect(m_layout_switcher, &QTabBar::currentChanged, this, &DockMenuBar::tabChanged);
	else
		m_tab_connection = connect(m_layout_switcher, &QTabBar::tabBarClicked, this, &DockMenuBar::tabChanged);

	stopBlink();
}

void DockMenuBar::onCurrentLayoutChanged(DockLayout::Index current_index)
{
	m_ignore_current_tab_changed = true;

	if (current_index != DockLayout::INVALID_INDEX)
		m_layout_switcher->setCurrentIndex(current_index);
	else
		m_layout_switcher->setCurrentIndex(m_plus_tab_index);

	m_ignore_current_tab_changed = false;
}

void DockMenuBar::onLockStateChanged(bool layout_locked)
{
	m_ignore_lock_state_changed = true;

	m_layout_locked_toggle->setChecked(layout_locked);

	if (layout_locked)
	{
		m_layout_locked_toggle->setText(tr("Layout Locked"));
		m_layout_locked_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-lock")));
	}
	else
	{
		m_layout_locked_toggle->setText(tr("Layout Unlocked"));
		m_layout_locked_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-unlock")));
	}

	m_ignore_lock_state_changed = false;
}

void DockMenuBar::startBlink(DockLayout::Index layout_index)
{
	stopBlink();

	if (layout_index == DockLayout::INVALID_INDEX)
		return;

	m_blink_tab = static_cast<int>(layout_index);
	m_blink_stage = 0;
	m_blink_timer->start(500);

	updateBlink();
}

void DockMenuBar::updateBlink()
{
	if (m_blink_tab < m_layout_switcher->count())
	{
		if (m_blink_stage % 2 == 0)
			m_layout_switcher->setTabTextColor(m_blink_tab, Qt::red);
		else
			m_layout_switcher->setTabTextColor(m_blink_tab, m_layout_switcher->palette().text().color());
	}

	m_blink_stage++;

	if (m_blink_stage > 7)
		m_blink_timer->stop();
}

void DockMenuBar::stopBlink()
{
	if (m_blink_timer->isActive())
	{
		if (m_blink_tab < m_layout_switcher->count())
			m_layout_switcher->setTabTextColor(m_blink_tab, m_layout_switcher->palette().text().color());

		m_blink_timer->stop();
	}
}

void DockMenuBar::tabChanged(int index)
{
	// Prevent recursion.
	if (m_ignore_current_tab_changed)
		return;

	if (index < m_plus_tab_index)
	{
		DockLayout::Index layout_index = static_cast<DockLayout::Index>(index);
		emit currentLayoutChanged(layout_index);
	}
	else if (index == m_plus_tab_index)
	{
		emit newButtonClicked();
	}
}
