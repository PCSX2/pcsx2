// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DebugSettingsWidget.h"
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
