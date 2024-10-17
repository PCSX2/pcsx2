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

	connect(m_ui.analyseButton, &QPushButton::clicked, this, &AnalysisOptionsDialog::analyse);
	connect(m_ui.closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void AnalysisOptionsDialog::analyse()
{
	Pcsx2Config::DebugAnalysisOptions options;
	m_analysis_settings->parseSettingsFromWidgets(options);

	Host::RunOnCPUThread([options]() {
		R5900SymbolImporter.LoadAndAnalyseElf(options);
	});

	if (m_ui.closeCheckBox->isChecked())
		accept();
}
