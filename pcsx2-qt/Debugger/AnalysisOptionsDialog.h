// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Settings/DebugAnalysisSettingsWidget.h"

#include <QtWidgets/QDialog>

#include "ui_AnalysisOptionsDialog.h"

class AnalysisOptionsDialog : public QDialog
{
	Q_OBJECT

public:
	AnalysisOptionsDialog(QWidget* parent = nullptr);

protected:
	void analyse();

	DebugAnalysisSettingsWidget* m_analysis_settings;

	Ui::AnalysisOptionsDialog m_ui;
};
