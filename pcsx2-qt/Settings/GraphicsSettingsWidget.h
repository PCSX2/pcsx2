/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GraphicsSettingsWidget.h"

enum class GSRendererType : s8;

class SettingsDialog;

class GraphicsSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GraphicsSettingsWidget(SettingsDialog* dialog, QWidget* parent);
	~GraphicsSettingsWidget();

Q_SIGNALS:
	void fullscreenModesChanged(const QStringList& modes);

private Q_SLOTS:
	void onTextureFilteringChange();
	void onSWTextureFilteringChange();
	void onRendererChanged(int index);
	void onAdapterChanged(int index);
	void onTrilinearFilteringChanged();
	void onGpuPaletteConversionChanged(int state);
	void onCPUSpriteRenderBWChanged();
	void onFullscreenModeChanged(int index);
	void onShadeBoostChanged();
	void onCaptureContainerChanged();
	void onEnableVideoCaptureChanged();
	void onEnableVideoCaptureArgumentsChanged();
	void onVideoCaptureAutoResolutionChanged();
	void onEnableAudioCaptureChanged();
	void onEnableAudioCaptureArgumentsChanged();

private:
	GSRendererType getEffectiveRenderer() const;
	void updateRendererDependentOptions();
	void resetManualHardwareFixes();

	SettingsDialog* m_dialog;

	Ui::GraphicsSettingsWidget m_ui;

	bool m_hardware_renderer_visible = false;
	bool m_software_renderer_visible = false;
};
