// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_AdvancedSettingsWidget.h"

class SettingsWindow;

class AdvancedSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	AdvancedSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~AdvancedSettingsWidget();

private:
	int getGlobalClampingModeIndex(int vunum) const;
	int getClampingModeIndex(int vunum) const;
	void setClampingMode(int vunum, int index);

	SettingsWindow* m_dialog;

	Ui::AdvancedSystemSettingsWidget m_ui;
};
