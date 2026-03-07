// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "OSDSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/Config.h"

OSDSettingsWidget::OSDSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui, tr("On-Screen Display"));

	connect(m_ui.messagesPos, &QComboBox::currentIndexChanged, this, &OSDSettingsWidget::onMessagesPosChanged);
	connect(m_ui.performancePos, &QComboBox::currentIndexChanged, this, &OSDSettingsWidget::onPerformancePosChanged);
	onMessagesPosChanged();
	onPerformancePosChanged();

	//////////////////////////////////////////////////////////////////////////
	// OSD Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.scale, "EmuCore/GS", "OsdScale", 100.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.messagesPos, "EmuCore/GS", "OsdMessagesPos", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_OSD_MESSAGE_POS));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.performancePos, "EmuCore/GS", "OsdPerformancePos", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_OSD_PERFORMANCE_POS));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showSpeedPercentages, "EmuCore/GS", "OsdShowSpeed", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showFPS, "EmuCore/GS", "OsdShowFPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showVPS, "EmuCore/GS", "OsdShowVPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showResolution, "EmuCore/GS", "OsdShowResolution", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showGSStats, "EmuCore/GS", "OsdShowGSStats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showUsageCPU, "EmuCore/GS", "OsdShowCPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showUsageGPU, "EmuCore/GS", "OsdShowGPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showStatusIndicators, "EmuCore/GS", "OsdShowIndicators", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showFrameTimes, "EmuCore/GS", "OsdShowFrameTimes", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showHardwareInfo, "EmuCore/GS", "OsdShowHardwareInfo", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showVersion, "EmuCore/GS", "OsdShowVersion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showSettings, "EmuCore/GS", "OsdShowSettings", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showPatches, "EmuCore/GS", "OsdshowPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showInputs, "EmuCore/GS", "OsdShowInputs", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showVideoCapture, "EmuCore/GS", "OsdShowVideoCapture", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showInputRec, "EmuCore/GS", "OsdShowInputRec", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.showTextureReplacements, "EmuCore/GS", "OsdShowTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.warnAboutUnsafeSettings, "EmuCore", "OsdWarnAboutUnsafeSettings", true);

	connect(m_ui.showSettings, &QCheckBox::checkStateChanged, this, &OSDSettingsWidget::onOsdShowSettingsToggled);

	//////////////////////////////////////////////////////////////////////////
	// OSD Help
	//////////////////////////////////////////////////////////////////////////
	dialog()->registerWidgetHelp(m_ui.scale, tr("OSD Scale"), tr("100%"), tr("Scales the size of the onscreen OSD from 50% to 500%."));

	dialog()->registerWidgetHelp(m_ui.messagesPos, tr("OSD Messages Position"), tr("Left (Default)"),
		tr("Position of on-screen-display messages when events occur such as save states being "
		   "created/loaded, screenshots being taken, etc."));

	dialog()->registerWidgetHelp(m_ui.performancePos, tr("OSD Performance Position"), tr("Right (Default)"),
		tr("Position of a variety of on-screen performance data points as selected by the user."));

	dialog()->registerWidgetHelp(m_ui.showSpeedPercentages, tr("Show Speed Percentages"), tr("Unchecked"),
		tr("Shows the current emulation speed of the system as a percentage."));

	dialog()->registerWidgetHelp(m_ui.showFPS, tr("Show FPS"), tr("Unchecked"),
		tr("Shows the number of internal video frames displayed per second by the system."));

	dialog()->registerWidgetHelp(m_ui.showVPS, tr("Show VPS"), tr("Unchecked"),
		tr("Shows the number of Vsyncs performed per second by the system."));

	dialog()->registerWidgetHelp(m_ui.showResolution, tr("Show Resolution"), tr("Unchecked"),
		tr("Shows the internal resolution of the game."));

	dialog()->registerWidgetHelp(m_ui.showGSStats, tr("Show GS Statistics"), tr("Unchecked"),
		tr("Shows statistics about the emulated GS such as primitives and draw calls."));

	dialog()->registerWidgetHelp(m_ui.showUsageCPU, tr("Show CPU Usage"),
		tr("Unchecked"), tr("Shows the host's CPU utilization based on threads."));

	dialog()->registerWidgetHelp(m_ui.showUsageGPU, tr("Show GPU Usage"),
		tr("Unchecked"), tr("Shows the host's GPU utilization."));

	dialog()->registerWidgetHelp(m_ui.showStatusIndicators, tr("Show Status Indicators"), tr("Checked"),
		tr("Shows icon indicators for emulation states such as Pausing, Turbo, Fast-Forward, and Slow-Motion."));

	dialog()->registerWidgetHelp(m_ui.showFrameTimes, tr("Show Frame Times"), tr("Unchecked"),
		tr("Displays a graph showing the average frametimes."));

	dialog()->registerWidgetHelp(m_ui.showHardwareInfo, tr("Show Hardware Info"), tr("Unchecked"),
		tr("Shows the current system CPU and GPU information."));

	dialog()->registerWidgetHelp(m_ui.showVersion, tr("Show PCSX2 Version"), tr("Unchecked"),
		tr("Shows the current PCSX2 version."));

	dialog()->registerWidgetHelp(m_ui.showSettings, tr("Show Settings"), tr("Unchecked"),
		tr("Displays various settings and the current values of those settings in the bottom-right corner of the display."));

	dialog()->registerWidgetHelp(m_ui.showPatches, tr("Show Patches"), tr("Unchecked"),
		tr("Shows the amount of currently active patches/cheats in the bottom-right corner of the display."));

	dialog()->registerWidgetHelp(m_ui.showInputs, tr("Show Inputs"), tr("Unchecked"),
		tr("Shows the current controller state of the system in the bottom-left corner of the display."));

	dialog()->registerWidgetHelp(m_ui.showVideoCapture, tr("Show Video Capture Status"), tr("Checked"),
		tr("Shows the status of the currently active video capture in the top-right corner of the display."));

	dialog()->registerWidgetHelp(m_ui.showInputRec, tr("Show Input Recording Status"), tr("Checked"),
		tr("Shows the status of the currently active input recording in the top-right corner of the display."));

	dialog()->registerWidgetHelp(m_ui.showTextureReplacements, tr("Show Texture Replacement Status"), tr("Unchecked"),
		tr("Shows the status of the number of dumped and loaded texture replacements in the top-right corner of the display."));

	dialog()->registerWidgetHelp(m_ui.warnAboutUnsafeSettings, tr("Warn About Unsafe Settings"), tr("Checked"),
		tr("Displays warnings when settings are enabled which may break games."));
}

OSDSettingsWidget::~OSDSettingsWidget() = default;

void OSDSettingsWidget::onMessagesPosChanged()
{
	const bool enabled = m_ui.messagesPos->currentIndex() != (dialog()->isPerGameSettings() ? 1 : 0);
	m_ui.warnAboutUnsafeSettings->setEnabled(enabled);
}

void OSDSettingsWidget::onPerformancePosChanged()
{
	const bool enabled = m_ui.performancePos->currentIndex() != (dialog()->isPerGameSettings() ? 1 : 0);

	m_ui.showSpeedPercentages->setEnabled(enabled);
	m_ui.showFPS->setEnabled(enabled);
	m_ui.showVPS->setEnabled(enabled);
	m_ui.showResolution->setEnabled(enabled);
	m_ui.showGSStats->setEnabled(enabled);
	m_ui.showUsageCPU->setEnabled(enabled);
	m_ui.showUsageGPU->setEnabled(enabled);
	m_ui.showStatusIndicators->setEnabled(enabled);
	m_ui.showFrameTimes->setEnabled(enabled);
	m_ui.showHardwareInfo->setEnabled(enabled);
	m_ui.showVersion->setEnabled(enabled);
}

void OSDSettingsWidget::onOsdShowSettingsToggled()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "OsdShowSettings", false);
	m_ui.showPatches->setEnabled(enabled);
}

#include "moc_OSDSettingsWidget.cpp"