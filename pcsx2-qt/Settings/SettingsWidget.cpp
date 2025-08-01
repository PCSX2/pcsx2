// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SettingsWidget.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QScrollArea>

SettingsWidget::SettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
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

	if (!custom_margins)
		setWidgetMargins(header, 0);
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
			setWidgetMargins(contents, 0);
	}
	else
	{
		if (m_tab_widget->count() == 2)
			setWidgetMargins(m_last_tab_contents, 9);

		if (!custom_margins)
			setWidgetMargins(contents, 9);
	}

	if (!custom_margins)
		m_last_tab_contents = contents;

	return scroll_area;
}

void SettingsWidget::setTabVisible(QWidget* tab, bool is_visible, QWidget* switch_to)
{
	int index = m_tab_widget->indexOf(tab);
	if (index < 0)
		return;

	if (!is_visible)
	{
		int switch_to_index = m_tab_widget->indexOf(tab);
		if (switch_to_index == index)
			m_tab_widget->setCurrentWidget(switch_to);
	}

	m_tab_widget->setTabEnabled(index, is_visible);
	m_tab_widget->setTabVisible(index, is_visible);
}

void SettingsWidget::setWidgetMargins(QWidget* widget, int margin)
{
	if (widget && widget->layout())
		widget->layout()->setContentsMargins(margin, margin, margin, margin);
}
