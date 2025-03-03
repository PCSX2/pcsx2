// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DebugUserInterfaceSettingsWidget.h"

class SettingsWindow;

class DebugUserInterfaceSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	DebugUserInterfaceSettingsWidget(SettingsWindow* dialog, QWidget* parent = nullptr);

private:
	Ui::DebugUserInterfaceSettingsWidget m_ui;
};
