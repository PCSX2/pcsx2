// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "AdvancedSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

AdvancedSettingsWidget::AdvancedSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeRecompiler, "EmuCore/CPU/Recompiler", "EnableEE", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeCache, "EmuCore/CPU/Recompiler", "EnableEECache", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeINTCSpinDetection, "EmuCore/Speedhacks", "IntcStat", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeWaitLoopDetection, "EmuCore/Speedhacks", "WaitLoop", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.eeFastmem, "EmuCore/CPU/Recompiler", "EnableFastmem", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pauseOnTLBMiss, "EmuCore/CPU/Recompiler", "PauseOnTLBMiss", false);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vu0Recompiler, "EmuCore/CPU/Recompiler", "EnableVU0", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vu1Recompiler, "EmuCore/CPU/Recompiler", "EnableVU1", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vuFlagHack, "EmuCore/Speedhacks", "vuFlagHack", true);

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.eeRoundingMode, "EmuCore/CPU", "FPU.Roundmode", 3);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vu0RoundingMode, "EmuCore/CPU", "VU0.Roundmode", 3);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vu1RoundingMode, "EmuCore/CPU", "VU1.Roundmode", 3);
	if (m_dialog->isPerGameSettings())
	{
		m_ui.eeClampMode->insertItem(0, tr("Use Global Setting [%1]").arg(m_ui.eeClampMode->itemText(getGlobalClampingModeIndex(-1))));
		m_ui.vu0ClampMode->insertItem(0, tr("Use Global Setting [%1]").arg(m_ui.vu0ClampMode->itemText(getGlobalClampingModeIndex(0))));
		m_ui.vu1ClampMode->insertItem(0, tr("Use Global Setting [%1]").arg(m_ui.vu1ClampMode->itemText(getGlobalClampingModeIndex(1))));
	}
	m_ui.eeClampMode->setCurrentIndex(getClampingModeIndex(-1));
	m_ui.vu0ClampMode->setCurrentIndex(getClampingModeIndex(0));
	m_ui.vu1ClampMode->setCurrentIndex(getClampingModeIndex(1));
	connect(m_ui.eeClampMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { setClampingMode(-1, index); });
	connect(m_ui.vu0ClampMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { setClampingMode(0, index); });
	connect(m_ui.vu1ClampMode, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) { setClampingMode(1, index); });

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.iopRecompiler, "EmuCore/CPU/Recompiler", "EnableIOP", true);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.gameFixes, "EmuCore", "EnableGameFixes", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.patches, "EmuCore", "EnablePatches", true);

	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.ntscFrameRate, "EmuCore/GS", "FramerateNTSC", 59.94f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.palFrameRate, "EmuCore/GS", "FrameratePAL", 50.00f);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.pineEnable, "EmuCore", "EnablePINE", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.pineSlot, "EmuCore", "PINESlot", 28011);

	dialog->registerWidgetHelp(m_ui.eeRoundingMode, tr("Rounding Mode"), tr("Chop/Zero (Default)"), tr("Changes how PCSX2 handles rounding while emulating the Emotion Engine's Floating Point Unit (EE FPU). "
	"Because the various FPUs in the PS2 are non-compliant with international standards, some games may need different modes to do math correctly. The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem can cause instability.</b>"));

	dialog->registerWidgetHelp(m_ui.eeClampMode, tr("Clamping Mode"), tr("Normal (Default)"), tr("Changes how PCSX2 handles keeping floats in a standard x86 range. "
	"The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem can cause instability.</b>"));

	dialog->registerWidgetHelp(m_ui.eeRecompiler, tr("Enable Recompiler"), tr("Checked"),
		tr("Performs just-in-time binary translation of 64-bit MIPS-IV machine code to x86."));

	//: Wait loop: When the game makes the CPU do nothing (loop/spin) while it waits for something to happen (usually an interrupt).
	dialog->registerWidgetHelp(m_ui.eeWaitLoopDetection, tr("Wait Loop Detection"), tr("Checked"),
		tr("Moderate speedup for some games, with no known side effects."));

	dialog->registerWidgetHelp(m_ui.eeCache, tr("Enable Cache (Slow)"), tr("Unchecked"), tr("Interpreter only, provided for diagnostic."));

	//: INTC = Name of a PS2 register, leave as-is. "spin" = to make a cpu (or gpu) actively do nothing while you wait for something.  Like spinning in a circle, you're moving but not actually going anywhere.
	dialog->registerWidgetHelp(m_ui.eeINTCSpinDetection, tr("INTC Spin Detection"), tr("Checked"),
		tr("Huge speedup for some games, with almost no compatibility side effects."));

	dialog->registerWidgetHelp(m_ui.eeFastmem, tr("Enable Fast Memory Access"), tr("Checked"),
		//: "Backpatching" = To edit previously generated code to change what it does (in this case, we generate direct memory accesses, then backpatch them to jump to a fancier handler function when we realize they need the fancier handler function)
		tr("Uses backpatching to avoid register flushing on every memory access."));

	dialog->registerWidgetHelp(m_ui.pauseOnTLBMiss, tr("Pause On TLB Miss"), tr("Unchecked"),
		tr("Pauses the virtual machine when a TLB miss occurs, instead of ignoring it and continuing. Note that the VM will pause after the "
		   "end of the block, not on the instruction which caused the exception. Refer to the console to see the address where the invalid "
		   "access occurred."));

	dialog->registerWidgetHelp(m_ui.vu0RoundingMode, tr("VU0 Rounding Mode"), tr("Chop/Zero (Default)"), tr("Changes how PCSX2 handles rounding while emulating the Emotion Engine's Vector Unit 0 (EE VU0). "
	"The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem will cause stability issues and/or crashes.</b>"));

	dialog->registerWidgetHelp(m_ui.vu1RoundingMode, tr("VU1 Rounding Mode"), tr("Chop/Zero (Default)"), tr("Changes how PCSX2 handles rounding while emulating the Emotion Engine's Vector Unit 1 (EE VU1). "
	"The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem will cause stability issues and/or crashes.</b>"));

	dialog->registerWidgetHelp(m_ui.vu0ClampMode, tr("VU0 Clamping Mode"), tr("Normal (Default)"), tr("Changes how PCSX2 handles keeping floats in a standard x86 range in the Emotion Engine's Vector Unit 0 (EE VU0). "
	"The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem can cause instability.</b>"));
	dialog->registerWidgetHelp(m_ui.vu1ClampMode, tr("VU1 Clamping Mode"), tr("Normal (Default)"), tr("Changes how PCSX2 handles keeping floats in a standard x86 range in the Emotion Engine's Vector Unit 1 (EE VU1). "
	"The default value handles the vast majority of games; <b>modifying this setting when a game is not having a visible problem can cause instability.</b>"));

	//: VU0 = Vector Unit 0. One of the PS2's processors.
	dialog->registerWidgetHelp(m_ui.vu0Recompiler, tr("Enable VU0 Recompiler (Micro Mode)"), tr("Checked"), tr("Enables VU0 Recompiler."));

	//: VU1 = Vector Unit 1. One of the PS2's processors.
	dialog->registerWidgetHelp(m_ui.vu1Recompiler, tr("Enable VU1 Recompiler"), tr("Checked"), tr("Enables VU1 Recompiler."));

	dialog->registerWidgetHelp(
		//: mVU = PCSX2's recompiler for VU (Vector Unit) code (full name: microVU)
		m_ui.vuFlagHack, tr("mVU Flag Hack"), tr("Checked"), tr("Good speedup and high compatibility, may cause graphical errors."));

	dialog->registerWidgetHelp(m_ui.iopRecompiler, tr("Enable Recompiler"), tr("Checked"),
		tr("Performs just-in-time binary translation of 32-bit MIPS-I machine code to x86."));

	dialog->registerWidgetHelp(m_ui.gameFixes, tr("Enable Game Fixes"), tr("Checked"),
		tr("Automatically loads and applies fixes to known problematic games on game start."));

	dialog->registerWidgetHelp(m_ui.patches, tr("Enable Compatibility Patches"), tr("Checked"),
		tr("Automatically loads and applies compatibility patches to known problematic games."));
}

AdvancedSettingsWidget::~AdvancedSettingsWidget() = default;

int AdvancedSettingsWidget::getGlobalClampingModeIndex(int vunum) const
{
	if (Host::GetBaseBoolSettingValue(
			"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0SignOverflow" : "vu1SignOverflow") : "fpuFullMode"), false))
		return 3;

	if (Host::GetBaseBoolSettingValue(
			"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0ExtraOverflow" : "vu1ExtraOverflow") : "fpuExtraOverflow"), false))
		return 2;

	if (Host::GetBaseBoolSettingValue(
			"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0Overflow" : "vu1Overflow") : "fpuOverflow"), true))
		return 1;

	return 0;
}

int AdvancedSettingsWidget::getClampingModeIndex(int vunum) const
{
	// This is so messy... maybe we should just make the mode an int in the settings too...
	const bool base = m_dialog->isPerGameSettings() ? 1 : 0;
	std::optional<bool> default_false = m_dialog->isPerGameSettings() ? std::nullopt : std::optional<bool>(false);
	std::optional<bool> default_true = m_dialog->isPerGameSettings() ? std::nullopt : std::optional<bool>(true);

	std::optional<bool> third = m_dialog->getBoolValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0SignOverflow" : "vu1SignOverflow") : "fpuFullMode"), default_false);
	std::optional<bool> second = m_dialog->getBoolValue("EmuCore/CPU/Recompiler",
		(vunum >= 0 ? ((vunum == 0) ? "vu0ExtraOverflow" : "vu1ExtraOverflow") : "fpuExtraOverflow"), default_false);
	std::optional<bool> first = m_dialog->getBoolValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0Overflow" : "vu1Overflow") : "fpuOverflow"), default_true);

	if (third.has_value() && third.value())
		return base + 3;
	if (second.has_value() && second.value())
		return base + 2;
	if (first.has_value() && first.value())
		return base + 1;
	else if (first.has_value())
		return base + 0; // none
	else
		return 0; // no per game override
}

void AdvancedSettingsWidget::setClampingMode(int vunum, int index)
{
	std::optional<bool> first, second, third;

	if (!m_dialog->isPerGameSettings() || index > 0)
	{
		const bool base = m_dialog->isPerGameSettings() ? 1 : 0;
		third = (index >= (base + 3));
		second = (index >= (base + 2));
		first = (index >= (base + 1));
	}

	m_dialog->setBoolSettingValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0SignOverflow" : "vu1SignOverflow") : "fpuFullMode"), third);
	m_dialog->setBoolSettingValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0ExtraOverflow" : "vu1ExtraOverflow") : "fpuExtraOverflow"), second);
	m_dialog->setBoolSettingValue(
		"EmuCore/CPU/Recompiler", (vunum >= 0 ? ((vunum == 0) ? "vu0Overflow" : "vu1Overflow") : "fpuOverflow"), first);
}
