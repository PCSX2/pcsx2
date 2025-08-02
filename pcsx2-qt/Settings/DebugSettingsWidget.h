// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DebugAnalysisSettingsTab.h"
#include "ui_DebugGSSettingsTab.h"
#include "ui_DebugLoggingSettingsTab.h"
#include "ui_DebugUserInterfaceSettingsTab.h"

#include "SettingsWidget.h"

class DebugUserInterfaceSettingsWidget;
class DebugAnalysisSettingsWidget;

class DebugSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	DebugSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~DebugSettingsWidget();

private Q_SLOTS:
	void onDrawDumpingChanged();
#ifdef PCSX2_DEVBUILD
	void onLoggingEnableChanged();
#endif

private:
	DebugUserInterfaceSettingsWidget* m_user_interface_settings;
	DebugAnalysisSettingsWidget* m_analysis_settings;

	Ui::DebugUserInterfaceSettingsTab m_user_interface;
	Ui::DebugAnalysisSettingsTab m_analysis;
	Ui::DebugGSSettingsTab m_gs;
	Ui::DebugLoggingSettingsTab m_logging;

	QWidget* m_user_interface_tab = nullptr;
	QWidget* m_logging_tab = nullptr;
};
