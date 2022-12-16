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

#include "pcsx2/SPU2/Global.h"

#include "AudioSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

static constexpr s32 DEFAULT_INTERPOLATION_MODE = 5;
static constexpr s32 DEFAULT_SYNCHRONIZATION_MODE = 0;
static constexpr s32 DEFAULT_EXPANSION_MODE = 0;
static constexpr s32 DEFAULT_DPL_DECODING_LEVEL = 0;
static const char* DEFAULT_OUTPUT_MODULE = "cubeb";
static constexpr s32 DEFAULT_OUTPUT_LATENCY = 100;
static constexpr s32 DEFAULT_VOLUME = 100;
static constexpr s32 DEFAULT_SOUNDTOUCH_SEQUENCE_LENGTH = 30;
static constexpr s32 DEFAULT_SOUNDTOUCH_SEEK_WINDOW = 20;
static constexpr s32 DEFAULT_SOUNDTOUCH_OVERLAP = 10;

static const char* s_output_module_entries[] = {
	QT_TRANSLATE_NOOP("AudioSettingsWidget", "No Sound (Emulate SPU2 only)"),
	QT_TRANSLATE_NOOP("AudioSettingsWidget", "Cubeb (Cross-platform)"),
#ifdef _WIN32
	QT_TRANSLATE_NOOP("AudioSettingsWidget", "XAudio2"),
#endif
	nullptr
};
static const char* s_output_module_values[] = {
	"nullout",
	"cubeb",
#ifdef _WIN32
	"xaudio2",
#endif
	nullptr
};

AudioSettingsWidget::AudioSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.interpolation, "SPU2/Mixing", "Interpolation", DEFAULT_INTERPOLATION_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.syncMode, "SPU2/Output", "SynchMode", DEFAULT_SYNCHRONIZATION_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.expansionMode, "SPU2/Output", "SpeakerConfiguration", DEFAULT_EXPANSION_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.dplLevel, "SPU2/Output", "DplDecodingLevel", DEFAULT_DPL_DECODING_LEVEL);
	connect(m_ui.expansionMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioSettingsWidget::expansionModeChanged);
	expansionModeChanged();

	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.outputModule, "SPU2/Output", "OutputModule", s_output_module_entries, s_output_module_values, DEFAULT_OUTPUT_MODULE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.latency, "SPU2/Output", "Latency", DEFAULT_OUTPUT_LATENCY);
	connect(m_ui.outputModule, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::outputModuleChanged);
	connect(m_ui.backend, &QComboBox::currentIndexChanged, this, &AudioSettingsWidget::outputBackendChanged);
	connect(m_ui.latency, &QSlider::valueChanged, this, &AudioSettingsWidget::updateLatencyLabel);
	outputModuleChanged();

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.volume, "SPU2/Mixing", "FinalVolume", DEFAULT_VOLUME);
	connect(m_ui.volume, &QSlider::valueChanged, this, &AudioSettingsWidget::updateVolumeLabel);

	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.sequenceLength, "Soundtouch", "SequenceLengthMS", DEFAULT_SOUNDTOUCH_SEQUENCE_LENGTH);
	connect(m_ui.sequenceLength, &QSlider::valueChanged, this, &AudioSettingsWidget::updateTimestretchSequenceLengthLabel);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.seekWindowSize, "Soundtouch", "SeekWindowMS", DEFAULT_SOUNDTOUCH_SEEK_WINDOW);
	connect(m_ui.seekWindowSize, &QSlider::valueChanged, this, &AudioSettingsWidget::updateTimestretchSeekwindowLengthLabel);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overlap, "Soundtouch", "OverlapMS", DEFAULT_SOUNDTOUCH_OVERLAP);
	connect(m_ui.overlap, &QSlider::valueChanged, this, &AudioSettingsWidget::updateTimestretchOverlapLabel);
	connect(m_ui.resetTimestretchDefaults, &QPushButton::clicked, this, &AudioSettingsWidget::resetTimestretchDefaults);

	m_ui.label_3b->setVisible(false);
	m_ui.dplLevel->setVisible(false);

	updateVolumeLabel();
	updateLatencyLabel();
	updateTimestretchSequenceLengthLabel();
	updateTimestretchSeekwindowLengthLabel();
	updateTimestretchOverlapLabel();
}

AudioSettingsWidget::~AudioSettingsWidget() = default;

void AudioSettingsWidget::expansionModeChanged()
{
	const bool expansion51 = m_dialog->getEffectiveIntValue("SPU2/Output", "SpeakerConfiguration", 0) == 2;
	m_ui.dplLevel->setDisabled(!expansion51);
}

void AudioSettingsWidget::outputModuleChanged()
{
	const std::string module_name(m_dialog->getEffectiveStringValue("SPU2/Output", "OutputModule", DEFAULT_OUTPUT_MODULE));
	const char* const* backend_names = GetOutputModuleBackends(module_name.c_str());

	const std::string backend_name(m_dialog->getEffectiveStringValue("SPU2/Output", "BackendName", ""));

	QSignalBlocker sb(m_ui.backend);
	m_ui.backend->clear();

	if (m_dialog->isPerGameSettings())
	{
		const QString global_backend(QString::fromStdString(Host::GetStringSettingValue("SPU2/Output", "BackendName", "")));
		m_ui.backend->addItem(tr("Use Global Setting [%1]").arg(global_backend.isEmpty() ? tr("Default") : global_backend));
	}

	m_ui.backend->setEnabled(backend_names != nullptr);
	m_ui.backend->addItem(tr("(Default)"));
	if (!backend_names || backend_name.empty())
		m_ui.backend->setCurrentIndex(0);

	if (backend_names)
	{
		for (u32 i = 0; backend_names[i] != nullptr; i++)
		{
			const int index = m_ui.backend->count();
			m_ui.backend->addItem(QString::fromUtf8(backend_names[i]));
			if (backend_name == backend_names[i])
				m_ui.backend->setCurrentIndex(index);
		}
	}
}

void AudioSettingsWidget::outputBackendChanged()
{
	int index = m_ui.backend->currentIndex();
	if (m_dialog->isPerGameSettings())
	{
		if (index == 0)
		{
			m_dialog->setStringSettingValue("SPU2/Output", "BackendName", std::nullopt);
			return;
		}

		index--;
	}

	if (index == 0)
		m_dialog->setStringSettingValue("SPU2/Output", "BackendName", "");
	else
		m_dialog->setStringSettingValue("SPU2/Output", "BackendName", m_ui.backend->currentText().toUtf8().constData());
}

void AudioSettingsWidget::updateVolumeLabel()
{
	m_ui.volumeLabel->setText(tr("%1%").arg(m_ui.volume->value()));
}

void AudioSettingsWidget::updateLatencyLabel()
{
	m_ui.latencyLabel->setText(tr("%1 ms (avg)").arg(m_ui.latency->value()));
}

void AudioSettingsWidget::updateTimestretchSequenceLengthLabel()
{
	m_ui.sequenceLengthLabel->setText(tr("%1 ms").arg(m_ui.sequenceLength->value()));
}

void AudioSettingsWidget::updateTimestretchSeekwindowLengthLabel()
{
	m_ui.seekWindowSizeLabel->setText(tr("%1 ms").arg(m_ui.seekWindowSize->value()));
}

void AudioSettingsWidget::updateTimestretchOverlapLabel()
{
	m_ui.overlapLabel->setText(tr("%1 ms").arg(m_ui.overlap->value()));
}

void AudioSettingsWidget::resetTimestretchDefaults()
{
	m_ui.sequenceLength->setValue(DEFAULT_SOUNDTOUCH_SEQUENCE_LENGTH);
	m_ui.seekWindowSize->setValue(DEFAULT_SOUNDTOUCH_SEEK_WINDOW);
	m_ui.overlap->setValue(DEFAULT_SOUNDTOUCH_OVERLAP);
}
