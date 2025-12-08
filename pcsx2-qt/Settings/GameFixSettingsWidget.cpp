// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "GameFixSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

GameFixSettingsWidget::GameFixSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.FpuMulHack, "EmuCore/Gamefixes", "FpuMulHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.GoemonTlbHack, "EmuCore/Gamefixes", "GoemonTlbHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.SoftwareRendererFMVHack, "EmuCore/Gamefixes", "SoftwareRendererFMVHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.SkipMPEGHack, "EmuCore/Gamefixes", "SkipMPEGHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.OPHFlagHack, "EmuCore/Gamefixes", "OPHFlagHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.EETimingHack, "EmuCore/Gamefixes", "EETimingHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.InstantDMAHack, "EmuCore/Gamefixes", "InstantDMAHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.DMABusyHack, "EmuCore/Gamefixes", "DMABusyHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.GIFFIFOHack, "EmuCore/Gamefixes", "GIFFIFOHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.VIFFIFOHack, "EmuCore/Gamefixes", "VIFFIFOHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.VIF1StallHack, "EmuCore/Gamefixes", "VIF1StallHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.VuAddSubHack, "EmuCore/Gamefixes", "VuAddSubHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.IbitHack, "EmuCore/Gamefixes", "IbitHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.FullVU0SyncHack, "EmuCore/Gamefixes", "FullVU0SyncHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.VUSyncHack, "EmuCore/Gamefixes", "VUSyncHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.VUOverflowHack, "EmuCore/Gamefixes", "VUOverflowHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.XgKickHack, "EmuCore/Gamefixes", "XgKickHack", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.BlitInternalFPSHack, "EmuCore/Gamefixes", "BlitInternalFPSHack", false);

	dialog()->registerWidgetHelp(m_ui.FpuMulHack, tr("FPU Multiply Hack"), tr("Unchecked"), tr("For Tales of Destiny."));
	dialog()->registerWidgetHelp(m_ui.GoemonTlbHack, tr("Preload TLB Hack"), tr("Unchecked"), tr("To avoid TLB miss on Goemon."));
	dialog()->registerWidgetHelp(m_ui.SoftwareRendererFMVHack, tr("Use Software Renderer For FMVs"), tr("Unchecked"), tr("Needed for some games with complex FMV rendering."));
	dialog()->registerWidgetHelp(m_ui.SkipMPEGHack, tr("Skip MPEG Hack"), tr("Unchecked"), tr("Skips videos/FMVs in games to avoid game hanging/freezes."));
	dialog()->registerWidgetHelp(m_ui.OPHFlagHack, tr("OPH Flag Hack"), tr("Unchecked"), tr("Known to affect following games: Bleach Blade Battlers, Growlanser II and III, Wizardry."));
	dialog()->registerWidgetHelp(m_ui.EETimingHack, tr("EE Timing Hack"), tr("Unchecked"), tr("General-purpose timing hack. Known to affect following games: Digital Devil Saga, SSX."));
	dialog()->registerWidgetHelp(m_ui.InstantDMAHack, tr("Instant DMA Hack"), tr("Unchecked"), tr("Good for cache emulation problems. Known to affect following games: Fire Pro Wrestling Z."));
	dialog()->registerWidgetHelp(m_ui.DMABusyHack, tr("DMA Busy Hack"), tr("Unchecked"), tr("Known to affect following games: Mana Khemia 1, Metal Saga, Pilot Down Behind Enemy Lines."));
	dialog()->registerWidgetHelp(m_ui.GIFFIFOHack, tr("Emulate GIF FIFO"), tr("Unchecked"), tr("Correct but slower. Known to affect the following games: FIFA Street 2."));
	dialog()->registerWidgetHelp(m_ui.VIFFIFOHack, tr("Emulate VIF FIFO"), tr("Unchecked"), tr("Simulate VIF1 FIFO read ahead. Known to affect following games: Test Drive Unlimited, Transformers."));
	dialog()->registerWidgetHelp(m_ui.VIF1StallHack, tr("Delay VIF1 Stalls"), tr("Unchecked"), tr("For SOCOM 2 HUD and Spy Hunter loading hang."));
	dialog()->registerWidgetHelp(m_ui.VuAddSubHack, tr("VU Add Hack"), tr("Unchecked"), tr("For Tri-Ace Games: Star Ocean 3, Radiata Stories, Valkyrie Profile 2."));
	dialog()->registerWidgetHelp(m_ui.IbitHack, tr("VU I Bit Hack"), tr("Unchecked"), tr("Avoids constant recompilation in some games. Known to affect the following games: Scarface The World is Yours, Crash Tag Team Racing."));
	dialog()->registerWidgetHelp(m_ui.FullVU0SyncHack, tr("Full VU0 Synchronization"), tr("Unchecked"), tr("Forces tight VU0 sync on every COP2 instruction."));
	dialog()->registerWidgetHelp(m_ui.VUSyncHack, tr("VU Sync"), tr("Unchecked"), tr("Run behind. To avoid sync problems when reading or writing VU registers."));
	dialog()->registerWidgetHelp(m_ui.VUOverflowHack, tr("VU Overflow Hack"), tr("Unchecked"), tr("To check for possible float overflows (Superman Returns)."));
	dialog()->registerWidgetHelp(m_ui.XgKickHack, tr("VU XGKick Sync"), tr("Unchecked"), tr("Use accurate timing for VU XGKicks (slower)."));
	dialog()->registerWidgetHelp(m_ui.BlitInternalFPSHack, tr("Force Blit Internal FPS Detection"), tr("Unchecked"), tr("Use alternative method to calculate internal FPS to avoid false readings in some games."));
}

GameFixSettingsWidget::~GameFixSettingsWidget() = default;
