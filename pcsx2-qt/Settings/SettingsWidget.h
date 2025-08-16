// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <QtWidgets/QWidget>

class QScrollArea;
class QTabWidget;

class SettingsWindow;

class SettingsWidget : public QWidget
{
	Q_OBJECT

protected:
	SettingsWidget(SettingsWindow* settings_dialog, QWidget* parent = nullptr);

	__fi SettingsWindow* dialog() { return m_dialog; }
	__fi const SettingsWindow* dialog() const { return m_dialog; }

	template <typename HeaderUi>
	void setupHeader(HeaderUi& header_ui)
	{
		QWidget* header = new QWidget(this);
		header_ui.setupUi(header);

		addPageHeader(header);
	}

	// Create a settings tab with a scroll area.
	template <typename ContentsUi>
	QWidget* setupTab(ContentsUi& contents_ui, QString name = QString())
	{
		QWidget* contents = new QWidget(this);
		contents_ui.setupUi(contents);

		return addTab(name, contents);
	}

	void addPageHeader(QWidget* header, bool custom_margins = false);
	QWidget* addTab(QString name, QWidget* contents, bool custom_margins = false);
	void setTabVisible(QWidget* tab, bool is_visible, QWidget* switch_to = nullptr);

private:
	void updateTabMargins(QScrollArea* scroll_area);

	SettingsWindow* m_dialog;
	QWidget* m_page_header;
	QTabWidget* m_tab_widget;

	QScrollArea* m_last_scroll_area = nullptr;
};
