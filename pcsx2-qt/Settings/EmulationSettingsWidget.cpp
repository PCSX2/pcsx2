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
#include <limits>

#include "EmulationSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

static constexpr u32 DEFAULT_FRAME_LATENCY = 2;

static void FillComboBoxWithEmulationSpeeds(QComboBox* cb)
{
//	cb->addItem(qApp->translate("GeneralSettingsWidget", "Custom"),;// TODO: Make use of getInteger to get manual overrides from users 
																	// for speed choice along with dropdown presets.
	cb->addItem(qApp->translate("GeneralSettingsWidget", "Unlimited"), QVariant(0.0f));

	static const int speeds[] = {1, 10, 25, 50, 75, 90, 100, 110,
		120, 150, 175, 200, 300, 400, 500, 1000};
	for (const int speed : speeds)
	{
		cb->addItem(qApp->translate("EmulationSettingsWidget", "%1% [%2 FPS (NTSC) / %3 FPS (PAL)]")
						.arg(speed)
						.arg((60 * speed) / 100)
						.arg((50 * speed) / 100),
			QVariant(static_cast<float>(speed) / 100.0f));
	}
}

EmulationSettingsWidget::EmulationSettingsWidget(QWidget* parent, SettingsDialog* dialog)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	FillComboBoxWithEmulationSpeeds(m_ui.normalSpeed);
	if (const int index =
			m_ui.normalSpeed->findData(QVariant(QtHost::GetBaseFloatSettingValue("Framerate", "NominalScalar", 1.0f)));
		index >= 0)
	{
		m_ui.normalSpeed->setCurrentIndex(index);
	}
	connect(m_ui.normalSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&EmulationSettingsWidget::onNormalSpeedIndexChanged);
	FillComboBoxWithEmulationSpeeds(m_ui.fastForwardSpeed);

	if (const int index =
			m_ui.fastForwardSpeed->findData(QVariant(QtHost::GetBaseFloatSettingValue("Framerate", "TurboScalar", 2.0f)));
		index >= 0)
	{
		m_ui.fastForwardSpeed->setCurrentIndex(index);
	}
	connect(m_ui.fastForwardSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&EmulationSettingsWidget::onFastForwardSpeedIndexChanged);

	FillComboBoxWithEmulationSpeeds(m_ui.slowMotionSpeed);
	if (const int index =
			m_ui.slowMotionSpeed->findData(QVariant(QtHost::GetBaseFloatSettingValue("Framerate", "SlomoScalar", 0.5f)));
		index >= 0)
	{
		m_ui.slowMotionSpeed->setCurrentIndex(index);
	}
	connect(m_ui.slowMotionSpeed, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&EmulationSettingsWidget::onSlowMotionSpeedIndexChanged);

	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.speedLimiter, "EmuCore/GS", "FrameLimitEnable", true);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.maxFrameLatency, "EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.syncToHostRefreshRate, "EmuCore/GS", "SyncToHostRefreshRate",
		false);
	connect(m_ui.optimalFramePacing, &QCheckBox::toggled, this, &EmulationSettingsWidget::onOptimalFramePacingChanged);

	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.cheats, "EmuCore", "EnableCheats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.widescreenPatches, "EmuCore", "EnableWideScreenPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.perGameSettings, "EmuCore", "EnablePerGameSettings", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.hostFilesystem, "EmuCore", "HostFs", false);

	dialog->registerWidgetHelp(
		m_ui.normalSpeed, tr("Normal Speed"), "100%",
		tr("Sets the target emulation speed. It is not guaranteed that this speed will be reached, "
		   "and if not, the emulator will run as fast as it can manage."));
	dialog->registerWidgetHelp(
		m_ui.fastForwardSpeed, tr("Fast Forward Speed"), tr("User Preference"),
		tr("Sets the fast forward speed. This speed will be used when the fast forward hotkey is pressed/toggled."));
	dialog->registerWidgetHelp(
		m_ui.slowMotionSpeed, tr("Slow Motion Speed"), tr("User Preference"),
		tr("Sets the slow motion speed. This speed will be used when the slow motion hotkey is pressed/toggled."));

	dialog->registerWidgetHelp(
		m_ui.syncToHostRefreshRate, tr("Sync To Host Refresh Rate"), tr("Unchecked"),
		tr("Adjusts the emulation speed so the console's refresh rate matches the host's refresh rate when both VSync and "
		   "Audio Resampling settings are enabled. This results in the smoothest animations possible, at the cost of "
		   "potentially increasing the emulation speed by less than 1%. Sync To Host Refresh Rate will not take effect if "
		   "the console's refresh rate is too far from the host's refresh rate. Users with variable refresh rate displays "
		   "should disable this option."));
	dialog->registerWidgetHelp(m_ui.cheats, tr("Enable Cheats"), tr("Unchecked"),
		tr("Automatically loads and applies cheats on game start."));
	dialog->registerWidgetHelp(
		m_ui.perGameSettings, tr("Enable Per-Game Settings"), tr("Checked"),
		tr("When enabled, per-game settings will be applied, and incompatible enhancements will be disabled. You should "
		   "leave this option enabled except when testing enhancements with incompatible games."));

	updateOptimalFramePacing();
}

EmulationSettingsWidget::~EmulationSettingsWidget() = default;

void EmulationSettingsWidget::onNormalSpeedIndexChanged(int index)
{
	bool okay;
	const float value = m_ui.normalSpeed->currentData().toFloat(&okay);
	QtHost::SetBaseFloatSettingValue("Framerate", "NominalScalar", okay ? value : 1.0f);
	g_emu_thread->applySettings();
}

void EmulationSettingsWidget::onFastForwardSpeedIndexChanged(int index)
{
	bool okay;
	const float value = m_ui.fastForwardSpeed->currentData().toFloat(&okay);
	QtHost::SetBaseFloatSettingValue("Framerate", "TurboScalar", okay ? value : 1.0f);
	g_emu_thread->applySettings();
}

void EmulationSettingsWidget::onSlowMotionSpeedIndexChanged(int index)
{
	bool okay;
	const float value = m_ui.slowMotionSpeed->currentData().toFloat(&okay);
	QtHost::SetBaseFloatSettingValue("Framerate", "SlomoScalar", okay ? value : 1.0f);
	g_emu_thread->applySettings();
}

void EmulationSettingsWidget::onOptimalFramePacingChanged(bool checked)
{
	const QSignalBlocker sb(m_ui.maxFrameLatency);
	m_ui.maxFrameLatency->setValue(DEFAULT_FRAME_LATENCY);
	m_ui.maxFrameLatency->setEnabled(!checked);

	QtHost::SetBaseIntSettingValue("EmuCore/GS", "VsyncQueueSize", checked ? 0 : DEFAULT_FRAME_LATENCY);
	g_emu_thread->applySettings();
}

void EmulationSettingsWidget::updateOptimalFramePacing()
{
	const int value = QtHost::GetBaseIntSettingValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
	const bool optimal = (value == 0);

	const QSignalBlocker sb(m_ui.optimalFramePacing);
	m_ui.optimalFramePacing->setChecked(optimal);

	const QSignalBlocker sb2(m_ui.maxFrameLatency);
	m_ui.maxFrameLatency->setEnabled(!optimal);
	m_ui.maxFrameLatency->setValue(optimal ? DEFAULT_FRAME_LATENCY : value);
}
