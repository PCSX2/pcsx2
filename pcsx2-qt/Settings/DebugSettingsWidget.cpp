// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugSettingsWidget.h"

#include "DebugAnalysisSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/Host.h"

#include <QtWidgets/QMessageBox>

DebugSettingsWidget::DebugSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Analysis Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.analysisCondition, "Debugger/Analysis", "RunCondition",
		Pcsx2Config::DebugAnalysisOptions::RunConditionNames, DebugAnalysisCondition::IF_DEBUGGER_IS_OPEN);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.generateSymbolsForIRXExportTables, "Debugger/Analysis", "GenerateSymbolsForIRXExports", true);

	dialog->registerWidgetHelp(m_ui.analysisCondition, tr("Analyze Program"), tr("If Debugger Is Open"),
		tr("Choose when the analysis passes should be run: Always (to save time when opening the debugger), If "
		   "Debugger Is Open (to save memory if you never open the debugger), or Never."));
	dialog->registerWidgetHelp(m_ui.generateSymbolsForIRXExportTables, tr("Generate Symbols for IRX Export Tables"), tr("Checked"),
		tr("Hook IRX module loading/unloading and generate symbols for exported functions on the fly."));

	m_analysis_settings = new DebugAnalysisSettingsWidget(dialog);

	m_ui.analysisSettings->setLayout(new QVBoxLayout());
	m_ui.analysisSettings->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.analysisSettings->layout()->addWidget(m_analysis_settings);

	//////////////////////////////////////////////////////////////////////////
	// GS Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpGSDraws, "EmuCore/GS", "dump", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveRT, "EmuCore/GS", "save", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveFrame, "EmuCore/GS", "savef", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveTexture, "EmuCore/GS", "savet", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveDepth, "EmuCore/GS", "savez", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.startDraw, "EmuCore/GS", "saven", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.dumpCount, "EmuCore/GS", "savel", 5000);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_ui.hwDumpDirectory, m_ui.hwDumpBrowse, m_ui.hwDumpOpen, nullptr, "EmuCore/GS", "HWDumpDirectory", std::string(), false);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_ui.swDumpDirectory, m_ui.swDumpBrowse, m_ui.swDumpOpen, nullptr, "EmuCore/GS", "SWDumpDirectory", std::string(), false);

	connect(m_ui.dumpGSDraws, &QCheckBox::checkStateChanged, this, &DebugSettingsWidget::onDrawDumpingChanged);
	onDrawDumpingChanged();
}

DebugSettingsWidget::~DebugSettingsWidget() = default;

void DebugSettingsWidget::onDrawDumpingChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "dump", false);
	m_ui.saveRT->setEnabled(enabled);
	m_ui.saveFrame->setEnabled(enabled);
	m_ui.saveTexture->setEnabled(enabled);
	m_ui.saveDepth->setEnabled(enabled);
}
