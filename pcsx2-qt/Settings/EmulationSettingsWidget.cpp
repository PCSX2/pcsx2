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

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <limits>

#include "pcsx2/HostSettings.h"

#include "EmulationSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

static constexpr u32 DEFAULT_FRAME_LATENCY = 2;

EmulationSettingsWidget::EmulationSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	initializeSpeedCombo(m_ui.normalSpeed, "Framerate", "NominalScalar", 1.0f);
	initializeSpeedCombo(m_ui.fastForwardSpeed, "Framerate", "TurboScalar", 2.0f);
	initializeSpeedCombo(m_ui.slowMotionSpeed, "Framerate", "SlomoScalar", 0.5f);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.speedLimiter, "EmuCore/GS", "FrameLimitEnable", true);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.maxFrameLatency, "EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.syncToHostRefreshRate, "EmuCore/GS", "SyncToHostRefreshRate", false);
	connect(m_ui.optimalFramePacing, &QCheckBox::stateChanged, this, &EmulationSettingsWidget::onOptimalFramePacingChanged);
	m_ui.optimalFramePacing->setTristate(dialog->isPerGameSettings());

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.cheats, "EmuCore", "EnableCheats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.widescreenPatches, "EmuCore", "EnableWideScreenPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.noInterlacingPatches, "EmuCore", "EnableNoInterlacingPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.perGameSettings, "EmuCore", "EnablePerGameSettings", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hostFilesystem, "EmuCore", "HostFs", false);

	dialog->registerWidgetHelp(m_ui.normalSpeed, tr("Normal Speed"), "100%",
		tr("Sets the target emulation speed. It is not guaranteed that this speed will be reached, "
		   "and if not, the emulator will run as fast as it can manage."));
	dialog->registerWidgetHelp(m_ui.fastForwardSpeed, tr("Fast Forward Speed"), tr("User Preference"),
		tr("Sets the fast forward speed. This speed will be used when the fast forward hotkey is pressed/toggled."));
	dialog->registerWidgetHelp(m_ui.slowMotionSpeed, tr("Slow Motion Speed"), tr("User Preference"),
		tr("Sets the slow motion speed. This speed will be used when the slow motion hotkey is pressed/toggled."));

	dialog->registerWidgetHelp(m_ui.syncToHostRefreshRate, tr("Sync To Host Refresh Rate"), tr("Unchecked"),
		tr("Adjusts the emulation speed so the console's refresh rate matches the host's refresh rate when both VSync and "
		   "Audio Resampling settings are enabled. This results in the smoothest animations possible, at the cost of "
		   "potentially increasing the emulation speed by less than 1%. Sync To Host Refresh Rate will not take effect if "
		   "the console's refresh rate is too far from the host's refresh rate. Users with variable refresh rate displays "
		   "should disable this option."));
	dialog->registerWidgetHelp(m_ui.cheats, tr("Enable Cheats"), tr("Unchecked"), tr("Automatically loads and applies cheats on game start."));
	dialog->registerWidgetHelp(m_ui.perGameSettings, tr("Enable Per-Game Settings"), tr("Checked"),
		tr("When enabled, per-game settings will be applied, and incompatible enhancements will be disabled. You should "
		   "leave this option enabled except when testing enhancements with incompatible games."));
	dialog->registerWidgetHelp(m_ui.optimalFramePacing, tr("Optimal Frame Pacing"), tr("Unchecked"),
		tr("Sets the vsync queue size to 0, making every frame be completed and presented by the GS before input is polled, and the next frame begins. "
		   "Using this setting can reduce input lag, at the cost of measurably higher CPU and GPU requirements."));
	dialog->registerWidgetHelp(m_ui.maxFrameLatency, tr("Maximum Frame Latency"), tr("2 Frames"),
		tr("Sets the maximum number of frames that can be queued up to the GS, before the CPU thread will wait for one of them to complete before continuing. "
		   "Higher values can assist with smoothing out irregular frame times, but add additional input lag."));

	updateOptimalFramePacing();
}

EmulationSettingsWidget::~EmulationSettingsWidget() = default;

void EmulationSettingsWidget::initializeSpeedCombo(QComboBox* cb, const char* section, const char* key, float default_value)
{
	float value = Host::GetBaseFloatSettingValue(section, key, default_value);
	if (m_dialog->isPerGameSettings())
	{
		cb->addItem(tr("Use Global Setting [%1%]").arg(value * 100.0f, 0, 'f', 0));
		if (!m_dialog->getSettingsInterface()->GetFloatValue(key, section, &value))
		{
			// set to something without data
			value = -1.0f;
			cb->setCurrentIndex(0);
		}
	}

	static const int speeds[] = {2, 10, 25, 50, 75, 90, 100, 110, 120, 150, 175, 200, 300, 400, 500, 1000};
	for (const int speed : speeds)
	{
		cb->addItem(tr("%1% [%2 FPS (NTSC) / %3 FPS (PAL)]")
						.arg(speed)
						.arg((60 * speed) / 100)
						.arg((50 * speed) / 100),
			QVariant(static_cast<float>(speed) / 100.0f));
	}

	cb->addItem(tr("Unlimited"), QVariant(0.0f));

	const int custom_index = cb->count();
	cb->addItem(tr("Custom"));

	if (const int index = cb->findData(QVariant(value)); index >= 0)
	{
		cb->setCurrentIndex(index);
	}
	else if (value > 0.0f)
	{
		cb->setItemText(custom_index, tr("Custom [%1% / %2 FPS (NTSC) / %3 FPS (PAL)]")
										  .arg(value * 100)
										  .arg(60 * value)
										  .arg(50 * value));
		cb->setCurrentIndex(custom_index);
	}

	connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this, cb, section, key](int index) { handleSpeedComboChange(cb, section, key); });
}

void EmulationSettingsWidget::handleSpeedComboChange(QComboBox* cb, const char* section, const char* key)
{
	const int custom_index = cb->count() - 1;
	const int current_index = cb->currentIndex();

	std::optional<float> new_value;
	if (current_index == custom_index)
	{
		bool ok = false;
		const double custom_value = QInputDialog::getDouble(
			QtUtils::GetRootWidget(this), tr("Custom Speed"), tr("Enter Custom Speed"), cb->currentData().toFloat(), 0.0f, 5000.0f, 1, &ok);
		if (!ok)
		{
			// we need to set back to the old value
			float value = m_dialog->getEffectiveFloatValue(section, key, 1.0f);

			QSignalBlocker sb(cb);
			if (m_dialog->isPerGameSettings() && !m_dialog->getSettingsInterface()->GetFloatValue(section, key, &value))
				cb->setCurrentIndex(0);
			else if (const int index = cb->findData(QVariant(value)); index >= 0)
				cb->setCurrentIndex(index);

			return;
		}

		cb->setItemText(custom_index, tr("Custom [%1% / %2 FPS (NTSC) / %3 FPS (PAL)]")
										  .arg(custom_value)
										  .arg((60 * custom_value) / 100)
										  .arg((50 * custom_value) / 100));
		new_value = static_cast<float>(custom_value / 100.0);
	}
	else if (current_index > 0 || !m_dialog->isPerGameSettings())
	{
		new_value = cb->currentData().toFloat();
	}

	m_dialog->setFloatSettingValue(section, key, new_value);
}

void EmulationSettingsWidget::onOptimalFramePacingChanged()
{
	const QSignalBlocker sb(m_ui.maxFrameLatency);

	std::optional<int> value;
	bool optimal = false;
	if (m_ui.optimalFramePacing->checkState() != Qt::PartiallyChecked)
	{
		optimal = m_ui.optimalFramePacing->isChecked();
		value = optimal ? 0 : DEFAULT_FRAME_LATENCY;
	}
	else
	{
		value = m_dialog->getEffectiveIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
		optimal = (value == 0);
	}

	m_ui.maxFrameLatency->setMinimum(optimal ? 0 : 1);
	m_ui.maxFrameLatency->setValue(optimal ? 0 : DEFAULT_FRAME_LATENCY);
	m_ui.maxFrameLatency->setEnabled(!m_dialog->isPerGameSettings() && !m_ui.optimalFramePacing->isChecked());

	m_dialog->setIntSettingValue("EmuCore/GS", "VsyncQueueSize", value);
}

void EmulationSettingsWidget::updateOptimalFramePacing()
{
	const QSignalBlocker sb(m_ui.optimalFramePacing);
	const QSignalBlocker sb2(m_ui.maxFrameLatency);

	int value = m_dialog->getEffectiveIntValue("EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
	bool optimal = (value == 0);
	if (m_dialog->isPerGameSettings() && !m_dialog->getSettingsInterface()->GetIntValue("EmuCore/GS", "VsyncQueueSize", &value))
	{
		m_ui.optimalFramePacing->setCheckState(Qt::PartiallyChecked);
		m_ui.maxFrameLatency->setEnabled(false);
	}
	else
	{
		m_ui.optimalFramePacing->setChecked(optimal);
		m_ui.maxFrameLatency->setEnabled(!optimal);
	}

	m_ui.maxFrameLatency->setMinimum(optimal ? 0 : 1);
	m_ui.maxFrameLatency->setValue(optimal ? 0 : value);
}
