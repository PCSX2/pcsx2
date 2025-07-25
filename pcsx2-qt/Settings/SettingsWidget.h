// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SettingsWindow.h"

#include <QtWidgets/QTabWidget>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>

class SettingsWidget : public QWidget
{
	Q_OBJECT

protected:
	SettingsWidget(SettingsWindow* dialog, QWidget* parent = nullptr);

	__fi SettingsWindow* dialog() { return m_dialog; }

	// Create a settings tab with a scroll area.
	template <typename ContentsUi>
	void setupTab(QString name, ContentsUi* contents_ui)
	{
		QWidget* contents = new QWidget(this);
		contents_ui->setupUi(contents);

		addTab(name, contents);
	}

	void addPageHeader(QWidget* header);
	void addTab(QString name, QWidget* contents, bool custom_margins = false);

private:
	void setWidgetMargins(QWidget* widget, int margin);

	SettingsWindow* m_dialog;
	QWidget* m_page_header;
	QTabWidget* m_tab_widget;

	QWidget* m_last_tab_contents = nullptr;
};
