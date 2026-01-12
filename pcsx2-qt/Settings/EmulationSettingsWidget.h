// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_EmulationSettingsWidget.h"

#include "SettingsWidget.h"

class EmulationSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	EmulationSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~EmulationSettingsWidget();

private Q_SLOTS:
	void onOptimalFramePacingChanged();

private:
	void initializeSpeedCombo(QComboBox* cb, const char* section, const char* key, float default_value);
	void handleSpeedComboChange(QComboBox* cb, const char* section, const char* key);
	void updateOptimalFramePacing();
	void updateUseVSyncForTimingEnabled();
	void onManuallySetRealTimeClockChanged();
	void onUseSystemLocaleFormatChanged();

	Ui::EmulationSettingsWidget m_ui;
};
