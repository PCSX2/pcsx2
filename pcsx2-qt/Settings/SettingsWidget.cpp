// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SettingsWidget.h"

#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

SettingsWidget::SettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(settings_dialog)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_tab_widget = new QTabWidget(this);
	m_tab_widget->setDocumentMode(true);
	layout->addWidget(m_tab_widget);
}

void SettingsWidget::addPageHeader(QWidget* header, bool custom_margins)
{
	QVBoxLayout* box_layout = static_cast<QVBoxLayout*>(layout());
	box_layout->insertWidget(layout()->count() - 1, header);

	if (!custom_margins && header->layout())
		header->layout()->setContentsMargins(0, 0, 0, 0);
}

QWidget* SettingsWidget::addTab(QString name, QWidget* contents, bool custom_margins)
{
	QScrollArea* scroll_area = new QScrollArea(this);
	scroll_area->setWidget(contents);
	scroll_area->setWidgetResizable(true);
	scroll_area->setFrameStyle(QFrame::NoFrame);

	m_tab_widget->addTab(scroll_area, name);

	// Only show the tab bar if there's more than one tab to choose from.
	if (m_tab_widget->count() == 1)
		m_tab_widget->tabBar()->setVisible(false);
	else if ((m_tab_widget->count() == 2))
		m_tab_widget->tabBar()->setVisible(true);

	// Automatically setup the margins on the tab contents and headers.
	if (m_tab_widget->count() == 1)
	{
		if (!custom_margins)
			updateTabMargins(scroll_area);
	}
	else
	{
		if (m_tab_widget->count() == 2 && m_last_scroll_area)
			updateTabMargins(m_last_scroll_area);

		if (!custom_margins)
			updateTabMargins(scroll_area);
	}

	if (!custom_margins)
	{
		m_last_scroll_area = scroll_area;

		connect(scroll_area->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this, scroll_area]() {
			updateTabMargins(scroll_area);
		});

		connect(scroll_area->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this, scroll_area]() {
			updateTabMargins(scroll_area);
		});
	}

	return scroll_area;
}

void SettingsWidget::setTabVisible(QWidget* tab, bool is_visible, QWidget* switch_to)
{
	int index = m_tab_widget->indexOf(tab);
	if (index < 0)
		return;

	if (!is_visible && index == m_tab_widget->currentIndex() && switch_to)
		m_tab_widget->setCurrentWidget(switch_to);

	m_tab_widget->setTabEnabled(index, is_visible);
	m_tab_widget->setTabVisible(index, is_visible);
}

void SettingsWidget::updateTabMargins(QScrollArea* scroll_area)
{
	if (!scroll_area->widget()->layout())
		return;

	if (m_tab_widget->count() == 1)
	{
		int horizontal_margin = 0;
		if (scroll_area->verticalScrollBar()->minimum() < scroll_area->verticalScrollBar()->maximum())
			horizontal_margin = -1;

		int vertical_margin = 0;
		if (scroll_area->horizontalScrollBar()->minimum() < scroll_area->horizontalScrollBar()->maximum())
			vertical_margin = -1;

		if (layoutDirection() == Qt::RightToLeft)
			scroll_area->widget()->layout()->setContentsMargins(horizontal_margin, 0, 0, vertical_margin);
		else
			scroll_area->widget()->layout()->setContentsMargins(0, 0, horizontal_margin, vertical_margin);
	}
	else
	{
		scroll_area->widget()->layout()->setContentsMargins(-1, -1, -1, -1);
	}
}
