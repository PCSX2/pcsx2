// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_GameFixSettingsWidget.h"

#include "SettingsWidget.h"

class GameFixSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GameFixSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~GameFixSettingsWidget();

private:
	Ui::GameFixSettingsWidget m_ui;
};
