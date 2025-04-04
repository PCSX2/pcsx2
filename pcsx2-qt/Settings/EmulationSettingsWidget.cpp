// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <limits>

#include "pcsx2/Host.h"

#include "EmulationSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

static constexpr int MINIMUM_EE_CYCLE_RATE = -3;
static constexpr int MAXIMUM_EE_CYCLE_RATE = 3;
static constexpr int DEFAULT_EE_CYCLE_RATE = 0;
static constexpr int DEFAULT_EE_CYCLE_SKIP = 0;
static constexpr u32 DEFAULT_FRAME_LATENCY = 2;

EmulationSettingsWidget::EmulationSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	initializeSpeedCombo(m_ui.normalSpeed, "Framerate", "NominalScalar", 1.0f);
	initializeSpeedCombo(m_ui.fastForwardSpeed, "Framerate", "TurboScalar", 2.0f);
	initializeSpeedCombo(m_ui.slowMotionSpeed, "Framerate", "SlomoScalar", 0.5f);

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.maxFrameLatency, "EmuCore/GS", "VsyncQueueSize", DEFAULT_FRAME_LATENCY);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.vsync, "EmuCore/GS", "VsyncEnable", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.syncToHostRefreshRate, "EmuCore/GS", "SyncToHostRefreshRate", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useVSyncForTiming, "EmuCore/GS", "UseVSyncForTiming", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.skipPresentingDuplicateFrames, "EmuCore/GS", "SkipDuplicateFrames", false);
	connect(m_ui.optimalFramePacing, &QCheckBox::checkStateChanged, this, &EmulationSettingsWidget::onOptimalFramePacingChanged);
	connect(m_ui.vsync, &QCheckBox::checkStateChanged, this, &EmulationSettingsWidget::updateUseVSyncForTimingEnabled);
	connect(m_ui.syncToHostRefreshRate, &QCheckBox::checkStateChanged, this, &EmulationSettingsWidget::updateUseVSyncForTimingEnabled);
	m_ui.optimalFramePacing->setTristate(dialog->isPerGameSettings());

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.eeCycleSkipping, "EmuCore/Speedhacks", "EECycleSkip", DEFAULT_EE_CYCLE_SKIP);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.MTVU, "EmuCore/Speedhacks", "vuThread", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.threadPinning, "EmuCore", "EnableThreadPinning", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fastCDVD, "EmuCore/Speedhacks", "fastCDVD", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.precacheCDVD, "EmuCore", "CdvdPrecache", false);

	if (m_dialog->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToDateTimeSetting(sif, m_ui.rtcDateTime, "EmuCore");
		m_ui.rtcDateTime->setDateRange(QDate(2000, 1, 1), QDate(2099, 12, 31));
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.manuallySetRealTimeClock, "EmuCore", "ManuallySetRealTimeClock", false);
		connect(m_ui.manuallySetRealTimeClock, &QCheckBox::checkStateChanged, this, &EmulationSettingsWidget::onManuallySetRealTimeClockChanged);
		EmulationSettingsWidget::onManuallySetRealTimeClockChanged();

		m_ui.eeCycleRate->insertItem(
			0, tr("Use Global Setting [%1]")
				   .arg(m_ui.eeCycleRate->itemText(
					   std::clamp(Host::GetBaseIntSettingValue("EmuCore/Speedhacks", "EECycleRate", DEFAULT_EE_CYCLE_RATE) - MINIMUM_EE_CYCLE_RATE,
						   0, MAXIMUM_EE_CYCLE_RATE - MINIMUM_EE_CYCLE_RATE))));

		// Disable cheats, use the cheats panel instead (move fastcvd up in its spot).
		const int count = m_ui.systemSettingsLayout->count();
		for (int i = 0; i < count; i++)
		{
			QLayoutItem* item = m_ui.systemSettingsLayout->itemAt(i);
			if (item && item->widget() == m_ui.cheats)
			{
				int row, col, rowSpan, colSpan;
				m_ui.systemSettingsLayout->getItemPosition(i, &row, &col, &rowSpan, &colSpan);
				delete m_ui.systemSettingsLayout->takeAt(i);
				m_ui.systemSettingsLayout->removeWidget(m_ui.fastCDVD);
				m_ui.systemSettingsLayout->addWidget(m_ui.fastCDVD, row, col);
				delete m_ui.cheats;
				m_ui.cheats = nullptr;
				break;
			}
		}
	}
	else
	{
		m_ui.rtcGroup->hide();

		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.cheats, "EmuCore", "EnableCheats", false);

		// Allow for FastCDVD for per-game settings only
		m_ui.systemSettingsLayout->removeWidget(m_ui.fastCDVD);
		m_ui.fastCDVD->deleteLater();
	}

	const std::optional<int> cycle_rate =
		m_dialog->getIntValue("EmuCore/Speedhacks", "EECycleRate", sif ? std::nullopt : std::optional<int>(DEFAULT_EE_CYCLE_RATE));
	m_ui.eeCycleRate->setCurrentIndex(cycle_rate.has_value() ? (std::clamp(cycle_rate.value(), MINIMUM_EE_CYCLE_RATE, MAXIMUM_EE_CYCLE_RATE) +
																   (0 - MINIMUM_EE_CYCLE_RATE) + static_cast<int>(m_dialog->isPerGameSettings())) :
                                                               0);
	connect(m_ui.eeCycleRate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		std::optional<int> value;
		if (!m_dialog->isPerGameSettings() || index > 0)
			value = MINIMUM_EE_CYCLE_RATE + index - static_cast<int>(m_dialog->isPerGameSettings());
		m_dialog->setIntSettingValue("EmuCore/Speedhacks", "EECycleRate", value);
	});

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hostFilesystem, "EmuCore", "HostFs", false);

	dialog->registerWidgetHelp(m_ui.normalSpeed, tr("Normal Speed"), tr("100%"),
		tr("Sets the target emulation speed. It is not guaranteed that this speed will be reached, "
		   "and if not, the emulator will run as fast as it can manage."));
	//: The "User Preference" string will appear after the text "Recommended Value:"
	dialog->registerWidgetHelp(m_ui.fastForwardSpeed, tr("Fast-Forward Speed"), tr("User Preference"),
		tr("Sets the fast-forward speed. This speed will be used when the fast-forward hotkey is pressed/toggled."));
	//: The "User Preference" string will appear after the text "Recommended Value:"
	dialog->registerWidgetHelp(m_ui.slowMotionSpeed, tr("Slow-Motion Speed"), tr("User Preference"),
		tr("Sets the slow-motion speed. This speed will be used when the slow-motion hotkey is pressed/toggled."));	

	dialog->registerWidgetHelp(m_ui.eeCycleRate, tr("EE Cycle Rate"), tr("100% (Normal Speed)"),
		tr("Higher values may increase internal framerate in games, but will increase CPU requirements substantially. "
		   "Lower values will reduce the CPU load allowing lightweight games to run full speed on weaker CPUs."));
	dialog->registerWidgetHelp(m_ui.eeCycleSkipping, tr("EE Cycle Skip"), tr("Disabled"),
		tr("Makes the emulated Emotion Engine skip cycles. "
		   //: SOTC = Shadow of the Colossus. A game's title, should not be translated unless an official translation exists.
		   "Helps a small subset of games like SOTC. Most of the time it's harmful to performance."));
	dialog->registerWidgetHelp(m_ui.threadPinning, tr("Enable Thread Pinning"), tr("Unchecked"),
		tr("Sets the priority for specific threads in a specific order ignoring the system scheduler. "
		   //: P-Core = Performance Core, E-Core = Efficiency Core. See if Intel has official translations for these terms.
		   "May help CPUs with big (P) and little (E) cores (e.g. Intel 12th or newer generation CPUs from Intel or other vendors such as AMD)."));
	dialog->registerWidgetHelp(m_ui.MTVU, tr("Enable Multithreaded VU1 (MTVU1)"), tr("Checked"),
		tr("Generally a speedup on CPUs with 4 or more cores. "
		   "Safe for most games, but a few are incompatible and may hang."));
	dialog->registerWidgetHelp(m_ui.fastCDVD, tr("Enable Fast CDVD"), tr("Unchecked"),
		tr("Fast disc access, less loading times. Check HDLoader compatibility lists for known games that have issues with this."));
	dialog->registerWidgetHelp(m_ui.precacheCDVD, tr("Enable CDVD Precaching"), tr("Unchecked"),
		tr("Loads the disc image into RAM before starting the virtual machine. Can reduce stutter on systems with hard drives that "
		   "have long wake times, but significantly increases boot times."));
	dialog->registerWidgetHelp(m_ui.cheats, tr("Enable Cheats"), tr("Unchecked"),
		tr("Automatically loads and applies cheats on game start."));
	dialog->registerWidgetHelp(m_ui.hostFilesystem, tr("Enable Host Filesystem"), tr("Unchecked"),
		tr("Allows games and homebrew to access files / folders directly on the host computer."));

	dialog->registerWidgetHelp(m_ui.optimalFramePacing, tr("Optimal Frame Pacing"), tr("Unchecked"),
		tr("Sets the VSync queue size to 0, making every frame be completed and presented by the GS before input is polled and the next frame begins. "
		   "Using this setting can reduce input lag at the cost of measurably higher CPU and GPU requirements."));
	dialog->registerWidgetHelp(m_ui.maxFrameLatency, tr("Maximum Frame Latency"), tr("2 Frames"),
		tr("Sets the maximum number of frames that can be queued up to the GS, before the CPU thread will wait for one of them to complete before continuing. "
		   "Higher values can assist with smoothing out irregular frame times, but add additional input lag."));
	dialog->registerWidgetHelp(m_ui.syncToHostRefreshRate, tr("Sync to Host Refresh Rate"), tr("Unchecked"),
		tr("Speeds up emulation so that the guest refresh rate matches the host. This results in the smoothest animations possible, at the cost of "
		   "potentially increasing the emulation speed by less than 1%. Sync to Host Refresh Rate will not take effect if "
		   "the console's refresh rate is too far from the host's refresh rate. Users with variable refresh rate displays "
		   "should disable this option."));
	dialog->registerWidgetHelp(m_ui.vsync, tr("Vertical Sync (VSync)"), tr("Unchecked"),
		tr("Enable this option to match PCSX2's refresh rate with your current monitor or screen. VSync is automatically disabled when "
		   "it is not possible (eg. running at non-100% speed)."));
	dialog->registerWidgetHelp(m_ui.useVSyncForTiming, tr("Use Host VSync Timing"), tr("Unchecked"),
		tr("When synchronizing with the host refresh rate, this option disable's PCSX2's internal frame timing, and uses the host instead. "
		   "Can result in smoother frame pacing, <strong>but at the cost of increased input latency</strong>."));
	dialog->registerWidgetHelp(m_ui.skipPresentingDuplicateFrames, tr("Skip Presenting Duplicate Frames"), tr("Checked"),
		tr("Detects when idle frames are being presented in 25/30fps games, and skips presenting those frames. The frame is still "
		   "rendered, it just means the GPU has more time to complete it (this is NOT frame skipping). Can smooth out frame time "
		   "fluctuations when the CPU/GPU are near maximum utilization, but makes frame pacing more inconsistent and can increase "
		   "input lag. Helps when using frame generation on 25/30fps games."));
	dialog->registerWidgetHelp(m_ui.manuallySetRealTimeClock, tr("Manually Set Real-Time Clock"), tr("Unchecked"),
		tr("Manually set a real-time clock to use for the virtual PlayStation 2 instead of using your OS' system clock."));
	dialog->registerWidgetHelp(m_ui.rtcDateTime, tr("Real-Time Clock"), tr("Current date and time"),
		tr("Real-time clock (RTC) used by the virtual PlayStation 2. Date format is the same as the one used by your OS. "
			"This time is only applied upon booting the PS2; changing it while in-game will have no effect. "
			"NOTE: This assumes you have your PS2 set to the default timezone of GMT+0 and default DST of Summer Time. "
			"Some games require an RTC date/time set after their release date."));

	updateOptimalFramePacing();
	updateUseVSyncForTimingEnabled();
}

EmulationSettingsWidget::~EmulationSettingsWidget() = default;

void EmulationSettingsWidget::initializeSpeedCombo(QComboBox* cb, const char* section, const char* key, float default_value)
{
	float value = Host::GetBaseFloatSettingValue(section, key, default_value);
	if (m_dialog->isPerGameSettings())
	{
		cb->addItem(tr("Use Global Setting [%1%]").arg(value * 100.0f, 0, 'f', 0));
		if (!m_dialog->getSettingsInterface()->GetFloatValue(section, key, &value))
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

	//: Every case that uses this particular string seems to refer to speeds: Normal Speed/Fast Forward Speed/Slow Motion Speed.
	cb->addItem(tr("Unlimited"), QVariant(0.0f));

	const int custom_index = cb->count();
	//: Every case that uses this particular string seems to refer to speeds: Normal Speed/Fast Forward Speed/Slow Motion Speed.
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

void EmulationSettingsWidget::updateUseVSyncForTimingEnabled()
{
	const bool vsync = m_dialog->getEffectiveBoolValue("EmuCore/GS", "VsyncEnable", false);
	const bool sync_to_host_refresh = m_dialog->getEffectiveBoolValue("EmuCore/GS", "SyncToHostRefreshRate", false);
	m_ui.useVSyncForTiming->setEnabled(vsync && sync_to_host_refresh);
}

void EmulationSettingsWidget::onManuallySetRealTimeClockChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore", "ManuallySetRealTimeClock", false);
	m_ui.rtcDateTime->setEnabled(enabled);
}
