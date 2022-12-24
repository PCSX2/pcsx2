/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "DebugSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"
#include <QtWidgets/QMessageBox>

#include "pcsx2/HostSettings.h"

DebugSettingsWidget::DebugSettingsWidget(SettingsDialog* dialog, QWidget* parent)
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

	connect(m_ui.dumpGSDraws, &QCheckBox::stateChanged, this, &DebugSettingsWidget::onDrawDumpingChanged);
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
