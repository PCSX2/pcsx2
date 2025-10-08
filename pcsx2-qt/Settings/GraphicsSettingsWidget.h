// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GraphicsAdvancedSettingsTab.h"
#include "ui_GraphicsDisplaySettingsTab.h"
#include "ui_GraphicsHardwareFixesSettingsTab.h"
#include "ui_GraphicsHardwareRenderingSettingsTab.h"
#include "ui_GraphicsMediaCaptureSettingsTab.h"
#include "ui_GraphicsOnScreenDisplaySettingsTab.h"
#include "ui_GraphicsPostProcessingSettingsTab.h"
#include "ui_GraphicsSettingsHeader.h"
#include "ui_GraphicsSoftwareRenderingSettingsTab.h"
#include "ui_GraphicsTextureReplacementSettingsTab.h"
#include "ui_GraphicsUpscalingFixesSettingsTab.h"

#include "SettingsWidget.h"

enum class GSRendererType : s8;

class GraphicsSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GraphicsSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
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
	void onMessagesPosChanged();
	void onPerformancePosChanged();
	void onCaptureContainerChanged();
	void onCaptureCodecChanged();
	void onEnableVideoCaptureChanged();
	void onEnableVideoCaptureArgumentsChanged();
	void onVideoCaptureAutoResolutionChanged();
	void onEnableAudioCaptureChanged();
	void onEnableAudioCaptureArgumentsChanged();
	void onOsdShowSettingsToggled();

private:
	GSRendererType getEffectiveRenderer() const;
	void updateRendererDependentOptions();
	void populateUpscaleMultipliers(u32 max_upscale_multiplier);

	Ui::GraphicsSettingsHeader m_header;
	Ui::GraphicsDisplaySettingsTab m_display;
	Ui::GraphicsHardwareRenderingSettingsTab m_hw;
	Ui::GraphicsSoftwareRenderingSettingsTab m_sw;
	Ui::GraphicsHardwareFixesSettingsTab m_fixes;
	Ui::GraphicsUpscalingFixesSettingsTab m_upscaling;
	Ui::GraphicsTextureReplacementSettingsTab m_texture;
	Ui::GraphicsPostProcessingSettingsTab m_post;
	Ui::GraphicsOnScreenDisplaySettingsTab m_osd;
	Ui::GraphicsMediaCaptureSettingsTab m_capture;
	Ui::GraphicsAdvancedSettingsTab m_advanced;

	QWidget* m_display_tab = nullptr;
	QWidget* m_hardware_rendering_tab = nullptr;
	QWidget* m_software_rendering_tab = nullptr;
	QWidget* m_hardware_fixes_tab = nullptr;
	QWidget* m_upscaling_fixes_tab = nullptr;
	QWidget* m_texture_replacement_tab = nullptr;
	QWidget* m_advanced_tab = nullptr;
};
