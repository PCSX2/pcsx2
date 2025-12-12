// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "AudioSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "ui_AudioExpansionSettingsDialog.h"
#include "ui_AudioStretchSettingsDialog.h"

#include "pcsx2/Host/AudioStream.h"
#include "pcsx2/SPU2/spu2.h"
#include "pcsx2/VMManager.h"

#include <QtWidgets/QMessageBox>
#include <algorithm>
#include <bit>

AudioSettingsWidget::AudioSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	for (u32 i = 0; i < static_cast<u32>(AudioBackend::Count); i++)
		m_ui.audioBackend->addItem(QString::fromUtf8(AudioStream::GetBackendDisplayName(static_cast<AudioBackend>(i))));

	for (u32 i = 0; i < static_cast<u32>(AudioExpansionMode::Count); i++)
	{
		m_ui.expansionMode->addItem(
			QString::fromUtf8(AudioStream::GetExpansionModeDisplayName(static_cast<AudioExpansionMode>(i))));
	}

	for (u32 i = 0; i < static_cast<u32>(Pcsx2Config::SPU2Options::SPU2SyncMode::Count); i++)
	{
		m_ui.syncMode->addItem(
			QString::fromUtf8(Pcsx2Config::SPU2Options::GetSyncModeDisplayName(
				static_cast<Pcsx2Config::SPU2Options::SPU2SyncMode>(i))));
	}

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.audioBackend, "SPU2/Output", "Backend",
		&AudioStream::ParseBackendName, &AudioStream::GetBackendName,
		Pcsx2Config::SPU2Options::DEFAULT_BACKEND);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.expansionMode, "SPU2/Output", "ExpansionMode",
		&AudioStream::ParseExpansionMode, &AudioStream::GetExpansionModeName,
		AudioStreamParameters::DEFAULT_EXPANSION_MODE);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.syncMode, "SPU2/Output", "SyncMode",
		&Pcsx2Config::SPU2Options::ParseSyncMode, &Pcsx2Config::SPU2Options::GetSyncModeName,
		Pcsx2Config::SPU2Options::DEFAULT_SYNC_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.bufferMS, "SPU2/Output", "BufferMS",
		AudioStreamParameters::DEFAULT_BUFFER_MS);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.outputLatencyMS, "SPU2/Output", "OutputLatencyMS",
		AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MS);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.outputLatencyMinimal, "SPU2/Output", "OutputLatencyMinimal", false);
	connect(m_ui.audioBackend, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::updateDriverNames);
	connect(m_ui.expansionMode, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::onExpansionModeChanged);
	connect(m_ui.expansionSettings, &QToolButton::clicked, this, &AudioSettingsWidget::onExpansionSettingsClicked);
	connect(m_ui.syncMode, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::onSyncModeChanged);
	connect(m_ui.stretchSettings, &QToolButton::clicked, this, &AudioSettingsWidget::onStretchSettingsClicked);
	onExpansionModeChanged();
	onSyncModeChanged();
	updateDriverNames();

	connect(m_ui.bufferMS, &QSlider::valueChanged, this, &AudioSettingsWidget::updateLatencyLabel);
	connect(m_ui.outputLatencyMS, &QSlider::valueChanged, this, &AudioSettingsWidget::updateLatencyLabel);
	connect(m_ui.outputLatencyMinimal, &QCheckBox::checkStateChanged, this, &AudioSettingsWidget::onMinimalOutputLatencyChanged);
	onMinimalOutputLatencyChanged();
	updateLatencyLabel();

	// for per-game, just use the normal path, since it needs to re-read/apply
	if (!dialog()->isPerGameSettings())
	{
		m_ui.standardVolume->setValue(dialog()->getEffectiveIntValue("SPU2/Output", "StandardVolume", 100));
		m_ui.fastForwardVolume->setValue(dialog()->getEffectiveIntValue("SPU2/Output", "FastForwardVolume", 100));
		m_ui.muted->setChecked(dialog()->getEffectiveBoolValue("SPU2/Output", "OutputMuted", false));
		connect(m_ui.standardVolume, &QSlider::valueChanged, this, &AudioSettingsWidget::onStandardVolumeChanged);
		connect(m_ui.fastForwardVolume, &QSlider::valueChanged, this, &AudioSettingsWidget::onFastForwardVolumeChanged);
		connect(m_ui.muted, &QCheckBox::checkStateChanged, this, &AudioSettingsWidget::onOutputMutedChanged);
		updateVolumeLabel();
	}
	else
	{
		SettingWidgetBinder::BindWidgetAndLabelToIntSetting(sif, m_ui.standardVolume, m_ui.standardVolumeLabel, tr("%"), "SPU2/Output", "StandardVolume", 100);
		SettingWidgetBinder::BindWidgetAndLabelToIntSetting(sif, m_ui.fastForwardVolume, m_ui.fastForwardVolumeLabel, tr("%"), "SPU2/Output", "FastForwardVolume", 100);
		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.muted, "SPU2/Output", "OutputMuted", false);
	}
	connect(m_ui.resetStandardVolume, &QToolButton::clicked, this, [this]() { resetVolume(false); });
	connect(m_ui.resetFastForwardVolume, &QToolButton::clicked, this, [this]() { resetVolume(true); });

	dialog()->registerWidgetHelp(
		m_ui.audioBackend, tr("Audio Backend"), QStringLiteral("Cubeb"),
		tr("The audio backend determines how frames produced by the emulator are submitted to the host. Cubeb provides the "
		   "lowest latency, if you encounter issues, try the SDL backend. The null backend disables all host audio "
		   "output."));
	dialog()->registerWidgetHelp(
		m_ui.bufferMS, tr("Buffer Size"), tr("%1 ms").arg(AudioStreamParameters::DEFAULT_BUFFER_MS),
		tr("Determines the buffer size which the time stretcher will try to keep filled. It effectively selects the "
		   "average latency, as audio will be stretched/shrunk to keep the buffer size within check."));
	dialog()->registerWidgetHelp(
		m_ui.outputLatencyMS, tr("Output Latency"), tr("%1 ms").arg(AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MS),
		tr("Determines the latency from the buffer to the host audio output. This can be set lower than the target latency "
		   "to reduce audio delay."));
	dialog()->registerWidgetHelp(m_ui.standardVolume, tr("Standard Volume"), "100%",
		tr("Controls the volume of the audio played on the host at normal speed."));
	dialog()->registerWidgetHelp(m_ui.fastForwardVolume, tr("Fast Forward Volume"), "100%",
		tr("Controls the volume of the audio played on the host when fast forwarding."));
	dialog()->registerWidgetHelp(m_ui.muted, tr("Mute All Sound"), tr("Unchecked"),
		tr("Prevents the emulator from producing any audible sound."));
	dialog()->registerWidgetHelp(m_ui.expansionMode, tr("Expansion Mode"), tr("Disabled (Stereo)"),
		tr("Determines how audio is expanded from stereo to surround for supported games. This "
		   "includes games that support Dolby Pro Logic/Pro Logic II."));
	dialog()->registerWidgetHelp(m_ui.expansionSettings, tr("Expansion Settings"), tr("N/A"),
		tr("These settings fine-tune the behavior of the FreeSurround-based channel expander."));
	dialog()->registerWidgetHelp(m_ui.syncMode, tr("Synchronization"), tr("TimeStretch (Recommended)"),
		tr("When the emulation isn't running at 100% speed, adjusts the tempo of the audio which produces much nicer sound during fast-forward/slowdown."));
	dialog()->registerWidgetHelp(m_ui.stretchSettings, tr("Stretch Settings"), tr("N/A"),
		tr("These settings fine-tune the behavior of the SoundTouch audio time stretcher when running outside of 100% speed."));
	dialog()->registerWidgetHelp(m_ui.resetStandardVolume, tr("Reset Standard Volume"), tr("N/A"),
		dialog()->isPerGameSettings() ? tr("Resets standard volume back to the global/inherited setting.") :
										tr("Resets standard volume back to the default."));
	dialog()->registerWidgetHelp(m_ui.resetFastForwardVolume, tr("Reset Fast Forward Volume"), tr("N/A"),
		dialog()->isPerGameSettings() ? tr("Resets fast forward volume back to the global/inherited setting.") :
										tr("Resets fast forward volume back to the default."));
}

AudioSettingsWidget::~AudioSettingsWidget() = default;

AudioExpansionMode AudioSettingsWidget::getEffectiveExpansionMode() const
{
	return AudioStream::ParseExpansionMode(
		dialog()->getEffectiveStringValue("SPU2/Output", "ExpansionMode",
					AudioStream::GetExpansionModeName(AudioStreamParameters::DEFAULT_EXPANSION_MODE))
			.c_str())
	    .value_or(AudioStreamParameters::DEFAULT_EXPANSION_MODE);
}

u32 AudioSettingsWidget::getEffectiveExpansionBlockSize() const
{
	const AudioExpansionMode expansion_mode = getEffectiveExpansionMode();
	if (expansion_mode == AudioExpansionMode::Disabled)
		return 0;

	const u32 config_block_size = dialog()->getEffectiveIntValue("SPU2/Output", "ExpandBlockSize",
		AudioStreamParameters::DEFAULT_EXPAND_BLOCK_SIZE);
	return std::has_single_bit(config_block_size) ? config_block_size : std::bit_ceil(config_block_size);
}

void AudioSettingsWidget::onExpansionModeChanged()
{
	const AudioExpansionMode expansion_mode = getEffectiveExpansionMode();
	m_ui.expansionSettings->setEnabled(expansion_mode != AudioExpansionMode::Disabled);
	updateLatencyLabel();
}

void AudioSettingsWidget::onSyncModeChanged()
{
	const Pcsx2Config::SPU2Options::SPU2SyncMode sync_mode =
		Pcsx2Config::SPU2Options::ParseSyncMode(
			dialog()->getEffectiveStringValue("SPU2/Output", "SyncMode",
						Pcsx2Config::SPU2Options::GetSyncModeName(Pcsx2Config::SPU2Options::DEFAULT_SYNC_MODE))
				.c_str())
			.value_or(Pcsx2Config::SPU2Options::DEFAULT_SYNC_MODE);
	m_ui.stretchSettings->setEnabled(sync_mode == Pcsx2Config::SPU2Options::SPU2SyncMode::TimeStretch);
}

AudioBackend AudioSettingsWidget::getEffectiveBackend() const
{
	return AudioStream::ParseBackendName(
		dialog()->getEffectiveStringValue("SPU2/Output", "Backend",
					AudioStream::GetBackendName(Pcsx2Config::SPU2Options::DEFAULT_BACKEND))
			.c_str())
	    .value_or(Pcsx2Config::SPU2Options::DEFAULT_BACKEND);
}

void AudioSettingsWidget::updateDriverNames()
{
	const AudioBackend backend = getEffectiveBackend();
	const std::vector<std::pair<std::string, std::string>> names = AudioStream::GetDriverNames(backend);

	QObject::disconnect(m_ui.driver, &QComboBox::currentIndexChanged, nullptr, nullptr);
	m_ui.driver->clear();
	if (names.empty())
	{
		m_ui.driver->addItem(tr("Default"), QString());
		m_ui.driver->setEnabled(false);
	}
	else
	{
		m_ui.driver->setEnabled(true);
		for (const std::pair<std::string, std::string>& it : names)
			m_ui.driver->addItem(QString::fromStdString(it.second), QString::fromStdString(it.first));

		SettingWidgetBinder::BindWidgetToStringSetting(dialog()->getSettingsInterface(), m_ui.driver, "SPU2/Output", "DriverName",
			std::move(names.front().first));
		connect(m_ui.driver, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::updateDeviceNames);
	}

	updateDeviceNames();
}

void AudioSettingsWidget::updateDeviceNames()
{
	const AudioBackend backend = getEffectiveBackend();
	const std::string driver_name = dialog()->getEffectiveStringValue("SPU2/Output", "DriverName", "");
	const std::string current_device = dialog()->getEffectiveStringValue("SPU2/Output", "DeviceName", "");
	const std::vector<AudioStream::DeviceInfo> devices = AudioStream::GetOutputDevices(backend, driver_name.c_str());

	QObject::disconnect(m_ui.outputDevice, &QComboBox::currentIndexChanged, nullptr, nullptr);
	m_ui.outputDevice->clear();
	m_output_device_latency = 0;

	if (devices.empty())
	{
		m_ui.outputDevice->addItem(tr("Default"), QString());
		m_ui.outputDevice->setEnabled(false);
	}
	else
	{
		m_ui.outputDevice->setEnabled(true);

		bool is_known_device = false;
		for (const AudioStream::DeviceInfo& di : devices)
		{
			m_ui.outputDevice->addItem(QString::fromStdString(di.display_name), QString::fromStdString(di.name));
			if (di.name == current_device)
			{
				m_output_device_latency = di.minimum_latency_frames;
				is_known_device = true;
			}
		}

		if (!is_known_device)
		{
			m_ui.outputDevice->addItem(tr("Unknown Device \"%1\"").arg(QString::fromStdString(current_device)),
				QString::fromStdString(current_device));
		}

		SettingWidgetBinder::BindWidgetToStringSetting(dialog()->getSettingsInterface(), m_ui.outputDevice, "SPU2/Output",
			"DeviceName", std::move(devices.front().name));
	}

	updateLatencyLabel();
}

void AudioSettingsWidget::updateLatencyLabel()
{
	const u32 expand_buffer_ms = AudioStream::GetMSForBufferSize(SPU2::SAMPLE_RATE, getEffectiveExpansionBlockSize());
	const u32 config_buffer_ms = dialog()->getEffectiveIntValue("SPU2/Output", "BufferMS", AudioStreamParameters::DEFAULT_BUFFER_MS);
	const u32 config_output_latency_ms = dialog()->getEffectiveIntValue("SPU2/Output", "OutputLatencyMS", AudioStreamParameters::DEFAULT_OUTPUT_LATENCY_MS);
	const bool minimal_output = dialog()->getEffectiveBoolValue("SPU2/Output", "OutputLatencyMinimal", false);

	//: Preserve the %1 variable, adapt the latter ms (and/or any possible spaces in between) to your language's ruleset.
	m_ui.outputLatencyLabel->setText(minimal_output ? tr("N/A") : tr("%1 ms").arg(config_output_latency_ms));
	m_ui.bufferMSLabel->setText(tr("%1 ms").arg(config_buffer_ms));

	const u32 output_latency_ms = minimal_output ? AudioStream::GetMSForBufferSize(SPU2::SAMPLE_RATE, m_output_device_latency) : config_output_latency_ms;
	if (output_latency_ms > 0)
	{
		if (expand_buffer_ms > 0)
		{
			m_ui.bufferingLabel->setText(tr("Maximum Latency: %1 ms (%2 ms buffer + %3 ms expand + %4 ms output)")
											 .arg(config_buffer_ms + expand_buffer_ms + output_latency_ms)
											 .arg(config_buffer_ms)
											 .arg(expand_buffer_ms)
											 .arg(output_latency_ms));
		}
		else
		{
			m_ui.bufferingLabel->setText(tr("Maximum Latency: %1 ms (%2 ms buffer + %3 ms output)")
											 .arg(config_buffer_ms + output_latency_ms)
											 .arg(config_buffer_ms)
											 .arg(output_latency_ms));
		}
	}
	else
	{
		if (expand_buffer_ms > 0)
		{
			m_ui.bufferingLabel->setText(tr("Maximum Latency: %1 ms (%2 ms expand, minimum output latency unknown)")
											 .arg(expand_buffer_ms + config_buffer_ms)
											 .arg(expand_buffer_ms));
		}
		else
		{
			m_ui.bufferingLabel->setText(tr("Maximum Latency: %1 ms (minimum output latency unknown)").arg(config_buffer_ms));
		}
	}
}

void AudioSettingsWidget::updateVolumeLabel()
{
	m_ui.standardVolumeLabel->setText(tr("%1%").arg(m_ui.standardVolume->value()));
	m_ui.fastForwardVolumeLabel->setText(tr("%1%").arg(m_ui.fastForwardVolume->value()));
}

void AudioSettingsWidget::onMinimalOutputLatencyChanged()
{
	const bool minimal = dialog()->getEffectiveBoolValue("SPU2/Output", "OutputLatencyMinimal", false);
	m_ui.outputLatencyMS->setEnabled(!minimal);
	updateLatencyLabel();
}

void AudioSettingsWidget::onStandardVolumeChanged(const int new_value)
{
	// only called for base settings
	pxAssert(!dialog()->isPerGameSettings());
	Host::SetBaseIntSettingValue("SPU2/Output", "StandardVolume", new_value);
	Host::CommitBaseSettingChanges();
	g_emu_thread->applySettings();

	updateVolumeLabel();
}

void AudioSettingsWidget::onFastForwardVolumeChanged(const int new_value)
{
	// only called for base settings
	pxAssert(!dialog()->isPerGameSettings());
	Host::SetBaseIntSettingValue("SPU2/Output", "FastForwardVolume", new_value);
	Host::CommitBaseSettingChanges();
	g_emu_thread->applySettings();

	updateVolumeLabel();
}

void AudioSettingsWidget::onOutputMutedChanged(const int new_state)
{
	// only called for base settings
	pxAssert(!dialog()->isPerGameSettings());

	const bool muted = (new_state != 0);
	Host::SetBaseBoolSettingValue("SPU2/Output", "OutputMuted", muted);
	Host::CommitBaseSettingChanges();
	g_emu_thread->applySettings();
}

void AudioSettingsWidget::onExpansionSettingsClicked()
{
	QDialog dlg(QtUtils::GetRootWidget(this));
	Ui::AudioExpansionSettingsDialog dlgui;
	dlgui.setupUi(&dlg);
	QtUtils::SetScalableIcon(dlgui.icon, QIcon::fromTheme(QStringLiteral("volume-up-line")), QSize(32, 32));

	SettingsInterface* sif = dialog()->getSettingsInterface();
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.blockSize, "SPU2/Output", "ExpandBlockSize",
		AudioStreamParameters::DEFAULT_EXPAND_BLOCK_SIZE, 0);
	QtUtils::BindLabelToSlider(dlgui.blockSize, dlgui.blockSizeLabel);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, dlgui.circularWrap, "SPU2/Output", "ExpandCircularWrap",
		AudioStreamParameters::DEFAULT_EXPAND_CIRCULAR_WRAP);
	QtUtils::BindLabelToSlider(dlgui.circularWrap, dlgui.circularWrapLabel);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.shift, "SPU2/Output", "ExpandShift", 100.0f,
		AudioStreamParameters::DEFAULT_EXPAND_SHIFT);
	QtUtils::BindLabelToSlider(dlgui.shift, dlgui.shiftLabel, 100.0f);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.depth, "SPU2/Output", "ExpandDepth", 10.0f,
		AudioStreamParameters::DEFAULT_EXPAND_DEPTH);
	QtUtils::BindLabelToSlider(dlgui.depth, dlgui.depthLabel, 10.0f);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.focus, "SPU2/Output", "ExpandFocus", 100.0f,
		AudioStreamParameters::DEFAULT_EXPAND_FOCUS);
	QtUtils::BindLabelToSlider(dlgui.focus, dlgui.focusLabel, 100.0f);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.centerImage, "SPU2/Output", "ExpandCenterImage", 100.0f,
		AudioStreamParameters::DEFAULT_EXPAND_CENTER_IMAGE);
	QtUtils::BindLabelToSlider(dlgui.centerImage, dlgui.centerImageLabel, 100.0f);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.frontSeparation, "SPU2/Output", "ExpandFrontSeparation",
		10.0f, AudioStreamParameters::DEFAULT_EXPAND_FRONT_SEPARATION);
	QtUtils::BindLabelToSlider(dlgui.frontSeparation, dlgui.frontSeparationLabel, 10.0f);
	SettingWidgetBinder::BindWidgetToNormalizedSetting(sif, dlgui.rearSeparation, "SPU2/Output", "ExpandRearSeparation", 10.0f,
		AudioStreamParameters::DEFAULT_EXPAND_REAR_SEPARATION);
	QtUtils::BindLabelToSlider(dlgui.rearSeparation, dlgui.rearSeparationLabel, 10.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.lowCutoff, "SPU2/Output", "ExpandLowCutoff",
		AudioStreamParameters::DEFAULT_EXPAND_LOW_CUTOFF);
	QtUtils::BindLabelToSlider(dlgui.lowCutoff, dlgui.lowCutoffLabel);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.highCutoff, "SPU2/Output", "ExpandHighCutoff",
		AudioStreamParameters::DEFAULT_EXPAND_HIGH_CUTOFF);
	QtUtils::BindLabelToSlider(dlgui.highCutoff, dlgui.highCutoffLabel);

	connect(dlgui.buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, &dlg, &QDialog::accept);
	connect(dlgui.buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this, &dlg]() {
		dialog()->setIntSettingValue("SPU2/Output", "ExpandBlockSize",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_EXPAND_BLOCK_SIZE));

		dialog()->setFloatSettingValue("SPU2/Output", "ExpandCircularWrap",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_CIRCULAR_WRAP));
		dialog()->setFloatSettingValue(
			"SPU2/Output", "ExpandShift",
			dialog()->isPerGameSettings() ? std::nullopt : std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_SHIFT));
		dialog()->setFloatSettingValue(
			"SPU2/Output", "ExpandDepth",
			dialog()->isPerGameSettings() ? std::nullopt : std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_DEPTH));
		dialog()->setFloatSettingValue(
			"SPU2/Output", "ExpandFocus",
			dialog()->isPerGameSettings() ? std::nullopt : std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_FOCUS));
		dialog()->setFloatSettingValue("SPU2/Output", "ExpandCenterImage",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_CENTER_IMAGE));
		dialog()->setFloatSettingValue("SPU2/Output", "ExpandFrontSeparation",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_FRONT_SEPARATION));
		dialog()->setFloatSettingValue("SPU2/Output", "ExpandRearSeparation",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<float>(AudioStreamParameters::DEFAULT_EXPAND_REAR_SEPARATION));
		dialog()->setIntSettingValue("SPU2/Output", "ExpandLowCutoff",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_EXPAND_LOW_CUTOFF));
		dialog()->setIntSettingValue("SPU2/Output", "ExpandHighCutoff",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_EXPAND_HIGH_CUTOFF));

		dlg.done(0);

		QMetaObject::invokeMethod(this, &AudioSettingsWidget::onExpansionSettingsClicked, Qt::QueuedConnection);
	});

	dlg.exec();
	updateLatencyLabel();
}

void AudioSettingsWidget::onStretchSettingsClicked()
{
	QDialog dlg(QtUtils::GetRootWidget(this));
	Ui::AudioStretchSettingsDialog dlgui;
	dlgui.setupUi(&dlg);
	QtUtils::SetScalableIcon(dlgui.icon, QIcon::fromTheme(QStringLiteral("volume-up-line")), QSize(32, 32));

	SettingsInterface* sif = dialog()->getSettingsInterface();
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.sequenceLength, "SPU2/Output", "StretchSequenceLengthMS",
		AudioStreamParameters::DEFAULT_STRETCH_SEQUENCE_LENGTH, 0);
	QtUtils::BindLabelToSlider(dlgui.sequenceLength, dlgui.sequenceLengthLabel);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.seekWindowSize, "SPU2/Output", "StretchSeekWindowMS",
		AudioStreamParameters::DEFAULT_STRETCH_SEEKWINDOW, 0);
	QtUtils::BindLabelToSlider(dlgui.seekWindowSize, dlgui.seekWindowSizeLabel);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, dlgui.overlap, "SPU2/Output", "StretchOverlapMS",
		AudioStreamParameters::DEFAULT_STRETCH_OVERLAP, 0);
	QtUtils::BindLabelToSlider(dlgui.overlap, dlgui.overlapLabel);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, dlgui.useQuickSeek, "SPU2/Output", "StretchUseQuickSeek",
		AudioStreamParameters::DEFAULT_STRETCH_USE_QUICKSEEK);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, dlgui.useAAFilter, "SPU2/Output", "StretchUseAAFilter",
		AudioStreamParameters::DEFAULT_STRETCH_USE_AA_FILTER);

	connect(dlgui.buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, &dlg, &QDialog::accept);
	connect(dlgui.buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this, &dlg]() {
		dialog()->setIntSettingValue("SPU2/Output", "StretchSequenceLengthMS",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_STRETCH_SEQUENCE_LENGTH));
		dialog()->setIntSettingValue("SPU2/Output", "StretchSeekWindowMS",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_STRETCH_SEEKWINDOW));
		dialog()->setIntSettingValue("SPU2/Output", "StretchOverlapMS",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<int>(AudioStreamParameters::DEFAULT_STRETCH_OVERLAP));
		dialog()->setBoolSettingValue("SPU2/Output", "StretchUseQuickSeek",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<bool>(AudioStreamParameters::DEFAULT_STRETCH_USE_QUICKSEEK));
		dialog()->setBoolSettingValue("SPU2/Output", "StretchUseAAFilter",
			dialog()->isPerGameSettings() ?
				std::nullopt :
				std::optional<bool>(AudioStreamParameters::DEFAULT_STRETCH_USE_AA_FILTER));

		dlg.done(0);

		QMetaObject::invokeMethod(this, &AudioSettingsWidget::onStretchSettingsClicked, Qt::QueuedConnection);
	});

	dlg.exec();
}

void AudioSettingsWidget::resetVolume(const bool fast_forward)
{
	const char* key = fast_forward ? "FastForwardVolume" : "StandardVolume";
	QSlider* const slider = fast_forward ? m_ui.fastForwardVolume : m_ui.standardVolume;
	QLabel* const label = fast_forward ? m_ui.fastForwardVolumeLabel : m_ui.standardVolumeLabel;

	if (dialog()->isPerGameSettings())
	{
		dialog()->removeSettingValue("SPU2/Output", key);

		const int value = dialog()->getEffectiveIntValue("SPU2/Output", key, 100);
		QSignalBlocker sb(slider);
		slider->setValue(value);
		label->setText(QStringLiteral("%1%2").arg(value).arg(tr("%")));

		// remove bold font if it was previously overridden
		QFont font(label->font());
		font.setBold(false);
		label->setFont(font);
	}
	else
	{
		slider->setValue(100);
	}
}
