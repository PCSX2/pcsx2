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

void SettingsWidget::addPageHeader(QWidget* header)
{
	static_cast<QVBoxLayout*>(layout())->insertWidget(layout()->count() - 1, header);
}

void SettingsWidget::addTab(QString name, QWidget* contents, bool custom_margins)
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
}

void SettingsWidget::setWidgetMargins(QWidget* widget, int margin)
{
	if (widget && widget->layout())
		widget->layout()->setContentsMargins(margin, margin, margin, margin);
}
