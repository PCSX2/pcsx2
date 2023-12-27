// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_DebugSettingsWidget.h"

class SettingsWindow;

class DebugSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	DebugSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~DebugSettingsWidget();

private Q_SLOTS:
	void onDrawDumpingChanged();

private:
	SettingsWindow* m_dialog;

	Ui::DebugSettingsWidget m_ui;
};
