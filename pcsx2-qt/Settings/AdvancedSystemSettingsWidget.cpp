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

#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "AdvancedSystemSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

AdvancedSystemSettingsWidget::AdvancedSystemSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeRecompiler, "EmuCore/CPU/Recompiler", "EnableEE", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeCache, "EmuCore/CPU/Recompiler", "EnableEECache", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeINTCSpinDetection, "EmuCore/Speedhacks", "IntcStat", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeWaitLoopDetection, "EmuCore/Speedhacks", "WaitLoop", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeFastmem, "EmuCore/CPU/Recompiler", "EnableFastmem", true);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vu0Recompiler, "EmuCore/CPU/Recompiler", "EnableVU0", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vu1Recompiler, "EmuCore/CPU/Recompiler", "EnableVU1", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vuFlagHack, "EmuCore/Speedhacks", "vuFlagHack", true);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.iopRecompiler, "EmuCore/CPU/Recompiler", "EnableIOP", true);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.gameFixes, "EmuCore", "EnableGameFixes", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.patches, "EmuCore", "EnablePatches", true);

	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.ntscFrameRate, "EmuCore/GS", "FramerateNTSC", 59.94f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.palFrameRate, "EmuCore/GS", "FrameratePAL", 50.00f);

	dialog->registerWidgetHelp(m_ui.eeRecompiler, tr("Enable Recompiler"), tr("Checked"),
		tr("Performs just - in - time binary translation of 64 - bit MIPS - IV machine code to x86."));

	dialog->registerWidgetHelp(m_ui.eeWaitLoopDetection, tr("Wait Loop Detection"), tr("Checked"),
		tr("Moderate speedup for some games, with no known side effects."));

	dialog->registerWidgetHelp(m_ui.eeCache, tr("Enable Cache (Slow)"), tr("Unchecked"),
		tr("Interpreter only, provided for diagnostic."));

	dialog->registerWidgetHelp(m_ui.eeINTCSpinDetection, tr("INTC Spin Detection"), tr("Checked"),
		tr("Huge speedup for some games, with almost no compatibility side effects."));

	dialog->registerWidgetHelp(m_ui.eeFastmem, tr("Enable Fast Memory Access"), tr("Checked"),
		tr("Uses backpatching to avoid register flushing on every memory access."));

	dialog->registerWidgetHelp(m_ui.vu0Recompiler, tr("Enable VU0 Recompiler"), tr("Checked"),
		tr("Enables VU0 Recompiler."));

	dialog->registerWidgetHelp(m_ui.vu1Recompiler, tr("Enable VU1 Recompiler"), tr("Checked"),
		tr("Enables VU1 Recompiler."));

	dialog->registerWidgetHelp(m_ui.vuFlagHack, tr("mVU Flag Hack"), tr("Checked"),
		tr("Good speedup and high compatibility, may cause graphical errors."));

	dialog->registerWidgetHelp(m_ui.iopRecompiler, tr("Enable Recompiler"), tr("Checked"),
		tr("Performs just-in-time binary translation of 32-bit MIPS-I machine code to x86."));

	dialog->registerWidgetHelp(m_ui.gameFixes, tr("Enable Game Fixes"), tr("Checked"),
		tr("Automatically loads and applies gamefixes to known problematic games on game start."));

	dialog->registerWidgetHelp(m_ui.patches, tr("Enable Compatibility Patches"), tr("Checked"),
		tr("Automatically loads and applies compatibility patches to known problematic games."));
}

AdvancedSystemSettingsWidget::~AdvancedSystemSettingsWidget() = default;
