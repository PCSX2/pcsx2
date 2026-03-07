// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_OSDSettingsWidget.h"
#include "SettingsWidget.h"

class OSDSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	OSDSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~OSDSettingsWidget();

private Q_SLOTS:
	void onMessagesPosChanged();
	void onPerformancePosChanged();
	void onOsdShowSettingsToggled();

private:
	Ui::OSDSettingsWidget m_ui;
};
