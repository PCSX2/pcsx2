// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GraphicsSettingsWidget.h"

#include "common/Pcsx2Defs.h"

enum class GSRendererType : s8;

class SettingsWindow;

class GraphicsSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GraphicsSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~GraphicsSettingsWidget();

Q_SIGNALS:
	void fullscreenModesChanged(const QStringList& modes);

private Q_SLOTS:
	void onTextureFilteringChange();
	void onSWTextureFilteringChange();
	void onRendererChanged(int index);
	void onAdapterChanged(int index);
	void onUpscaleMultiplierChanged();
	void onTrilinearFilteringChanged();
	void onGpuPaletteConversionChanged(int state);
	void onCPUSpriteRenderBWChanged();
	void onFullscreenModeChanged(int index);
	void onTextureDumpChanged();
	void onTextureReplacementChanged();
	void onShadeBoostChanged();
	void onColorCorrectChanged();
	void onHDRChanged();
	void onMessagesPosChanged();
	void onPerformancePosChanged();
	void onCaptureContainerChanged();
	void onCaptureCodecChanged();
	void onEnableVideoCaptureChanged();
	void onEnableVideoCaptureArgumentsChanged();
	void onVideoCaptureAutoResolutionChanged();
	void onEnableAudioCaptureChanged();
	void onEnableAudioCaptureArgumentsChanged();

private:
	GSRendererType getEffectiveRenderer() const;
	void updateRendererDependentOptions();
	void populateUpscaleMultipliers(u32 max_upscale_multiplier);
	void resetManualHardwareFixes();

	SettingsWindow* m_dialog;

	Ui::GraphicsSettingsWidget m_ui;

	bool m_hardware_renderer_visible = false;
	bool m_software_renderer_visible = false;
};
