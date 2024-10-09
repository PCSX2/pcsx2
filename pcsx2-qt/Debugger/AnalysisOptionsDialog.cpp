// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "AnalysisOptionsDialog.h"

#include "Host.h"
#include "DebugTools/SymbolImporter.h"

AnalysisOptionsDialog::AnalysisOptionsDialog(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	m_analysis_settings = new DebugAnalysisSettingsWidget();

	m_ui.analysisSettings->setLayout(new QVBoxLayout());
	m_ui.analysisSettings->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.analysisSettings->layout()->addWidget(m_analysis_settings);
}

void AnalysisOptionsDialog::accept()
{
	Pcsx2Config::DebugAnalysisOptions options;
	m_analysis_settings->parseSettingsFromWidgets(options);

	Host::RunOnCPUThread([options]() {
		R5900SymbolImporter.LoadAndAnalyseElf(options);
	});

	QDialog::accept();
}
