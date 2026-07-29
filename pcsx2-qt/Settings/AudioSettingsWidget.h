// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_AudioSettingsWidget.h"

#include "SettingsWidget.h"

enum class AudioBackend : u8;
enum class AudioExpansionMode : u8;

class AudioSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	AudioSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~AudioSettingsWidget();

private Q_SLOTS:
	void onExpansionModeChanged();
	void onSyncModeChanged();

	void updateDriverNames();
	void updateDeviceNames();
	void updateLatencyLabel();
	void updateVolumeLabel();
	void onMinimalOutputLatencyChanged();
	void onStandardVolumeChanged(const int new_value);
	void onFastForwardVolumeChanged();
	void onOutputMutedChanged(const int new_state);
	void resetVolumeAction(bool per_game, const char* key, QSlider* slider, QLabel* label, bool fast_forward);

	void onExpansionSettingsClicked();
	void onStretchSettingsClicked();

private:
	AudioBackend getEffectiveBackend() const;
	AudioExpansionMode getEffectiveExpansionMode() const;
	u32 getEffectiveExpansionBlockSize() const;
	int getNearestFastForwardMultiplierIndex(int standard_volume, int fast_forward_volume) const;
	int getComputedFastForwardVolume(int standard_volume, int multiplier_index) const;
	QString getFastForwardVolumeLabel(int multiplier_index) const;
	void updateFastForwardVolumeSetting();
	void resetVolume(const bool fast_forward);

	Ui::AudioSettingsWidget m_ui;
	u32 m_output_device_latency = 0;
};
