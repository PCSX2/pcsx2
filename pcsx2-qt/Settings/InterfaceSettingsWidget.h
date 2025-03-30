// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_InterfaceSettingsWidget.h"

class SettingsWindow;

class InterfaceSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	InterfaceSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~InterfaceSettingsWidget();

Q_SIGNALS:
	void themeChanged();
	void languageChanged();
	void globalMenusChanged();

private Q_SLOTS:
	void onRenderToSeparateWindowChanged();

private:
	void populateLanguages();

	Ui::InterfaceSettingsWidget m_ui;

public:
	static const char* THEME_NAMES[];
	static const char* THEME_VALUES[];
};
