// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_DebugSettingsWidget.h"

class SettingsWindow;
class DebugUserInterfaceSettingsWidget;
class DebugAnalysisSettingsWidget;

class DebugSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	DebugSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~DebugSettingsWidget();

private Q_SLOTS:
	void onDrawDumpingChanged();
#ifdef PCSX2_DEVBUILD
	void onLoggingEnableChanged();
#endif

private:
	SettingsWindow* m_dialog;

	DebugUserInterfaceSettingsWidget* m_user_interface_settings;
	DebugAnalysisSettingsWidget* m_analysis_settings;

	Ui::DebugSettingsWidget m_ui;
};
