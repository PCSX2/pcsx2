// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GameFixSettingsWidget.h"

class SettingsWindow;

class GameFixSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GameFixSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~GameFixSettingsWidget();

private:
	Ui::GameFixSettingsWidget m_ui;
};
