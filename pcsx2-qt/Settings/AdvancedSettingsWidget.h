// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_AdvancedSettingsWidget.h"

#include "SettingsWidget.h"

class AdvancedSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	AdvancedSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~AdvancedSettingsWidget();

private:
	int getGlobalClampingModeIndex(int vunum) const;
	int getClampingModeIndex(int vunum) const;
	void setClampingMode(int vunum, int index);
	void onSavestateCompressionTypeChanged();

	SettingsWindow* m_dialog;
	Ui::AdvancedSystemSettingsWidget m_ui;
};
