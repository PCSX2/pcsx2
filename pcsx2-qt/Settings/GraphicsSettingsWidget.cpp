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

#include "GraphicsSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"
#include <QtWidgets/QMessageBox>

#include "pcsx2/GS/GS.h"
#include "pcsx2/GS/GSUtil.h"

#include "Frontend/VulkanHostDisplay.h"

#ifdef _WIN32
#include "Frontend/D3D11HostDisplay.h"
#endif

struct RendererInfo
{
	const char* name;
	GSRendererType type;
};

static constexpr RendererInfo s_renderer_info[] = {
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Automatic"),
	GSRendererType::Auto,
#ifdef _WIN32
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Direct3D 11"),
	GSRendererType::DX11,
#endif
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "OpenGL"),
	GSRendererType::OGL,
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Vulkan"),
	GSRendererType::VK,
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Software"),
	GSRendererType::SW,
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Null"),
	GSRendererType::Null,
};

static const char* s_anisotropic_filtering_entries[] = {QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Off (Default)"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2x"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "4x"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "8x"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "16x"),
	nullptr};
static const char* s_anisotropic_filtering_values[] = {"1", "2", "4", "8", "16", nullptr};

static constexpr int DEFAULT_INTERLACE_MODE = 7;

GraphicsSettingsWidget::GraphicsSettingsWidget(QWidget* parent, SettingsDialog* dialog)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Global Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToStringSetting(m_ui.adapter, "EmuCore/GS", "Adapter");
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.vsync, "EmuCore/GS", "VsyncEnable", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.enableHWFixes, "EmuCore/GS", "UserHacks", false);

	//////////////////////////////////////////////////////////////////////////
	// Game Display Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToEnumSetting(m_ui.aspectRatio, "EmuCore/GS", "AspectRatio",
		Pcsx2Config::GSOptions::AspectRatioNames, AspectRatioType::R4_3);
	SettingWidgetBinder::BindWidgetToEnumSetting(m_ui.fmvAspectRatio, "EmuCore/GS", "FMVAspectRatioSwitch",
		Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames,
		FMVAspectRatioSwitchType::Off);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.interlacing, "EmuCore/GS", "interlace", DEFAULT_INTERLACE_MODE);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.bilinearFiltering, "EmuCore/GS", "LinearPresent", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.integerScaling, "EmuCore/GS", "IntegerScaling", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.internalResolutionScreenshots, "EmuCore/GS",
		"InternalResolutionScreenshots", false);
	SettingWidgetBinder::BindWidgetToFloatSetting(m_ui.zoom, "EmuCore/GS", "Zoom", 100.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(m_ui.stretchY, "EmuCore/GS", "StretchY", 100.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(m_ui.offsetX, "EmuCore/GS", "OffsetX", 0.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(m_ui.offsetY, "EmuCore/GS", "OffsetY", 0.0f);

	connect(m_ui.integerScaling, &QCheckBox::stateChanged, this, &GraphicsSettingsWidget::onIntegerScalingChanged);
	onIntegerScalingChanged();

	connect(m_ui.fullscreenModes, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onFullscreenModeChanged);

	//////////////////////////////////////////////////////////////////////////
	// OSD Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToFloatSetting(m_ui.osdScale, "EmuCore/GS", "OsdScale", 100.0f);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowMessages, "EmuCore/GS", "OsdShowMessages", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowSpeed, "EmuCore/GS", "OsdShowSpeed", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowFPS, "EmuCore/GS", "OsdShowFPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowCPU, "EmuCore/GS", "OsdShowCPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowResolution, "EmuCore/GS", "OsdShowResolution", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.osdShowGSStats, "EmuCore/GS", "OsdShowGSStats", false);

	dialog->registerWidgetHelp(m_ui.osdShowMessages, tr("Show OSD Messages"), tr("Checked"),
		tr("Shows on-screen-display messages when events occur such as save states being "
		   "created/loaded, screenshots being taken, etc."));
	dialog->registerWidgetHelp(m_ui.osdShowFPS, tr("Show Game Frame Rate"), tr("Unchecked"),
		tr("Shows the internal frame rate of the game in the top-right corner of the display."));
	dialog->registerWidgetHelp(
		m_ui.osdShowSpeed, tr("Show Emulation Speed"), tr("Unchecked"),
		tr("Shows the current emulation speed of the system in the top-right corner of the display as a percentage."));
	dialog->registerWidgetHelp(m_ui.osdShowResolution, tr("Show Resolution"), tr("Unchecked"),
		tr("Shows the resolution of the game in the top-right corner of the display."));

	//////////////////////////////////////////////////////////////////////////
	// HW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.upscaleMultiplier, "EmuCore/GS", "upscale_multiplier", 1);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.textureFiltering, "EmuCore/GS", "filter",
		static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.trilinearFiltering, "EmuCore/GS", "UserHacks_TriFilter",
		static_cast<int>(TriFiltering::Off));
	SettingWidgetBinder::BindWidgetToEnumSetting(m_ui.anisotropicFiltering, "EmuCore/GS", "MaxAnisotropy",
		s_anisotropic_filtering_entries, s_anisotropic_filtering_values, "1");
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.dithering, "EmuCore/GS", "dithering_ps2", 2);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.mipmapping, "EmuCore/GS", "mipmap_hw",
		static_cast<int>(HWMipmapLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.crcFixLevel, "EmuCore/GS", "crc_hack_level",
		static_cast<int>(CRCHackLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.blending, "EmuCore/GS", "accurate_blending_unit",
		static_cast<int>(AccBlendLevel::Basic));
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.accurateDATE, "EmuCore/GS", "accurate_date", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.conservativeBufferAllocation, "EmuCore/GS",
		"conservative_framebuffer", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.gpuPaletteConversion, "EmuCore/GS", "paltex", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.preloadTexture, "EmuCore/GS", "preload_texture", false);

	//////////////////////////////////////////////////////////////////////////
	// HW Renderer Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.halfScreenFix, "EmuCore/GS", "UserHacks_Half_Bottom_Override", -1,
		-1);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.skipDrawRangeStart, "EmuCore/GS", "UserHacks_SkipDraw", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.skipDrawRangeCount, "EmuCore/GS", "UserHacks_SkipDraw_Offset", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.hwAutoFlush, "EmuCore/GS", "UserHacks_AutoFlush", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.frameBufferConversion, "EmuCore/GS", "UserHacks_CPU_FB_Conversion",
		false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.disableDepthEmulation, "EmuCore/GS",
		"UserHacks_DisableDepthSupport", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.memoryWrapping, "EmuCore/GS", "wrap_gs_mem", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.disableSafeFeatures, "EmuCore/GS",
		"UserHacks_Disable_Safe_Features", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.preloadFrameData, "EmuCore/GS", "preload_frame_with_gs_data",
		false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.fastTextureInvalidation, "EmuCore/GS",
		"UserHacks_DisablePartialInvalidation", false);

	//////////////////////////////////////////////////////////////////////////
	// HW Upscaling Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.halfPixelOffset, "EmuCore/GS", "UserHacks_HalfPixelOffset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.roundSprite, "EmuCore/GS", "UserHacks_RoundSprite", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.textureOffsetX, "EmuCore/GS", "UserHacks_TCOffsetX", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.textureOffsetY, "EmuCore/GS", "UserHacks_TCOffsetY", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.alignSprite, "EmuCore/GS", "UserHacks_align_sprite_X", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.mergeSprite, "EmuCore/GS", "UserHacks_merge_pp_sprite", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.wildHack, "EmuCore/GS", "UserHacks_WildHack", false);

	//////////////////////////////////////////////////////////////////////////
	// Advanced Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.useBlitSwapChain, "EmuCore/GS", "UseBlitSwapChain", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.useDebugDevice, "EmuCore/GS", "UseDebugDevice", false);

	//////////////////////////////////////////////////////////////////////////
	// SW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(m_ui.extraSWThreads, "EmuCore/GS", "extrathreads", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.swAutoFlush, "EmuCore/GS", "autoflush_sw", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.swAA1, "EmuCore/GS", "aa1", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(m_ui.swMipmap, "EmuCore/GS", "mipmap", true);

	//////////////////////////////////////////////////////////////////////////
	// Non-trivial settings
	//////////////////////////////////////////////////////////////////////////
	const GSRendererType current_renderer = static_cast<GSRendererType>(
		QtHost::GetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
	for (const RendererInfo& ri : s_renderer_info)
	{
		m_ui.renderer->addItem(qApp->translate("GraphicsSettingsWidget", ri.name));
		if (ri.type == current_renderer)
			m_ui.renderer->setCurrentIndex(m_ui.renderer->count() - 1);
	}
	connect(m_ui.renderer, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onRendererChanged);
	connect(m_ui.enableHWFixes, &QCheckBox::stateChanged, this, &GraphicsSettingsWidget::onEnableHardwareFixesChanged);
	updateRendererDependentOptions();

	dialog->registerWidgetHelp(m_ui.useBlitSwapChain, tr("Use Blit Swap Chain"), tr("Unchecked"),
		tr("Uses a blit presentation model instead of flipping when using the Direct3D 11 "
		   "renderer. This usually results in slower performance, but may be required for some "
		   "streaming applications, or to uncap framerates on some systems."));
}

GraphicsSettingsWidget::~GraphicsSettingsWidget() = default;

void GraphicsSettingsWidget::onRendererChanged(int index)
{
	QtHost::SetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(s_renderer_info[index].type));
	g_emu_thread->applySettings();
	updateRendererDependentOptions();
}

void GraphicsSettingsWidget::onAdapterChanged(int index)
{
	if (index == 0)
		QtHost::RemoveBaseSettingValue("EmuCore/GS", "Adapter");
	else
		QtHost::SetBaseStringSettingValue("EmuCore/GS", "Adapter", m_ui.adapter->currentText().toUtf8().constData());
	g_emu_thread->applySettings();
}

void GraphicsSettingsWidget::onEnableHardwareFixesChanged()
{
	const bool enabled = m_ui.enableHWFixes->isChecked();
	m_ui.hardwareRendererGroup->setTabEnabled(2, enabled);
	m_ui.hardwareRendererGroup->setTabEnabled(3, enabled);
}

void GraphicsSettingsWidget::updateRendererDependentOptions()
{
	const int index = m_ui.renderer->currentIndex();
	GSRendererType type = s_renderer_info[index].type;
	if (type == GSRendererType::Auto)
		type = GSUtil::GetPreferredRenderer();

#ifdef _WIN32
	const bool is_dx11 = (type == GSRendererType::DX11 || type == GSRendererType::SW);
#else
	const bool is_dx11 = false;
#endif

	const bool is_hardware = (type == GSRendererType::DX11 || type == GSRendererType::OGL || type == GSRendererType::VK);
	const bool is_software = (type == GSRendererType::SW);

	// move advanced tab to the correct parent
	static constexpr std::array<const char*, 3> move_tab_names = {{"Display", "On-Screen Display", "Advanced"}};
	const std::array<QWidget*, 3> move_tab_pointers = {{m_ui.gameDisplayTab, m_ui.osdTab, m_ui.advancedTab}};
	for (size_t i = 0; i < move_tab_pointers.size(); i++)
	{
		QWidget* tab = move_tab_pointers[i];
		const QString tab_label(tr(move_tab_names[i]));
		if (const int index = m_ui.softwareRendererGroup->indexOf(tab); index >= 0 && is_hardware)
			m_ui.softwareRendererGroup->removeTab(index);
		if (const int index = m_ui.hardwareRendererGroup->indexOf(tab); index >= 0 && is_software)
			m_ui.hardwareRendererGroup->removeTab(index);
		if (const int index = m_ui.hardwareRendererGroup->indexOf(tab); index < 0 && is_hardware)
			m_ui.hardwareRendererGroup->insertTab((i == 0) ? 0 : m_ui.hardwareRendererGroup->count(), tab, tab_label);
		if (const int index = m_ui.softwareRendererGroup->indexOf(tab); index < 0 && is_software)
			m_ui.softwareRendererGroup->insertTab((i == 0) ? 0 : m_ui.softwareRendererGroup->count(), tab, tab_label);
	}

	if (is_hardware != is_software)
	{
		if (is_hardware)
			m_ui.hardwareRendererGroup->setCurrentIndex(0);
		else
			m_ui.softwareRendererGroup->setCurrentIndex(0);
	}

	if (m_hardware_renderer_visible != is_hardware)
	{
		m_ui.hardwareRendererGroup->setVisible(is_hardware);
		if (!is_hardware)
			m_ui.verticalLayout->removeWidget(m_ui.hardwareRendererGroup);
		else
			m_ui.verticalLayout->insertWidget(1, m_ui.hardwareRendererGroup);

		m_hardware_renderer_visible = is_hardware;
	}

	if (m_software_renderer_visible != is_software)
	{
		m_ui.softwareRendererGroup->setVisible(is_software);
		if (!is_hardware)
			m_ui.verticalLayout->removeWidget(m_ui.softwareRendererGroup);
		else
			m_ui.verticalLayout->insertWidget(1, m_ui.softwareRendererGroup);

		m_software_renderer_visible = is_software;
	}

	m_ui.useBlitSwapChain->setEnabled(is_dx11);

	// populate adapters
	HostDisplay::AdapterAndModeList modes;
	switch (type)
	{
#ifdef _WIN32
		case GSRendererType::DX11:
			modes = D3D11HostDisplay::StaticGetAdapterAndModeList();
			break;
#endif

		case GSRendererType::VK:
			modes = VulkanHostDisplay::StaticGetAdapterAndModeList(nullptr);
			break;

		case GSRendererType::OGL:
		case GSRendererType::SW:
		case GSRendererType::Null:
		case GSRendererType::Auto:
		default:
			break;
	}

	// fill+select adapters
	{
		const std::string current_adapter = QtHost::GetBaseStringSettingValue("EmuCore/GS", "Adapter", "");
		QSignalBlocker sb(m_ui.adapter);
		m_ui.adapter->clear();
		m_ui.adapter->setEnabled(!modes.adapter_names.empty());
		m_ui.adapter->addItem(tr("(Default)"));
		for (const std::string& adapter : modes.adapter_names)
		{
			m_ui.adapter->addItem(QString::fromStdString(adapter));
			if (current_adapter == adapter)
				m_ui.adapter->setCurrentIndex(m_ui.adapter->count() - 1);
		}
	}

	// fill+select fullscreen modes
	{
		QSignalBlocker sb(m_ui.fullscreenModes);

		const std::string current_mode(QtHost::GetBaseStringSettingValue("EmuCore/GS", "FullscreenMode", ""));
		m_ui.fullscreenModes->clear();
		m_ui.fullscreenModes->addItem(tr("Borderless Fullscreen"));
		if (current_mode.empty())
			m_ui.fullscreenModes->setCurrentIndex(0);

		for (const std::string& fs_mode : modes.fullscreen_modes)
		{
			m_ui.fullscreenModes->addItem(QString::fromStdString(fs_mode));
			if (current_mode == fs_mode)
				m_ui.fullscreenModes->setCurrentIndex(m_ui.fullscreenModes->count() - 1);
		}
	}

	m_ui.enableHWFixes->setEnabled(is_hardware);
	onEnableHardwareFixesChanged();
}

void GraphicsSettingsWidget::onIntegerScalingChanged()
{
	m_ui.bilinearFiltering->setEnabled(!m_ui.integerScaling->isChecked());
}

void GraphicsSettingsWidget::onFullscreenModeChanged(int index)
{
	if (index == 0)
	{
		QtHost::RemoveBaseSettingValue("EmuCore/GS", "FullscreenMode");
	}
	else
	{
		QtHost::SetBaseStringSettingValue("EmuCore/GS", "FullscreenMode",
			m_ui.fullscreenModes->currentText().toUtf8().constData());
	}

	g_emu_thread->applySettings();
}
