// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
