// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_InterfaceSettingsWidget.h"

#include "SettingsWidget.h"

class InterfaceSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	InterfaceSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~InterfaceSettingsWidget();

	void updatePromptOnStateLoadSaveFailureCheckbox(Qt::CheckState state);

Q_SIGNALS:
	void themeChanged();
	void languageChanged();
	void backgroundChanged();

private Q_SLOTS:
	void onRenderToSeparateWindowChanged();
	void onSetGameListBackgroundTriggered();
	void onClearGameListBackgroundTriggered();

private:
	void populateLanguages();

	Ui::InterfaceSettingsWidget m_ui;

public:
	static const char* THEME_NAMES[];
	static const char* THEME_VALUES[];
	static const char* BACKGROUND_SCALE_NAMES[];
	static const char* IMAGE_FILE_FILTER;
};
