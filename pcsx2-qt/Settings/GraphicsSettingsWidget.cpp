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

#include "pcsx2/HostSettings.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/GS/GSUtil.h"

#ifdef ENABLE_VULKAN
#include "Frontend/VulkanHostDisplay.h"
#endif

#ifdef _WIN32
#include "Frontend/D3D11HostDisplay.h"
#include "Frontend/D3D12HostDisplay.h"
#endif
#ifdef __APPLE__
#include "GS/Renderers/Metal/GSMetalCPPAccessible.h"
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
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Direct3D 12"),
	GSRendererType::DX12,
#endif
#ifdef ENABLE_OPENGL
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "OpenGL"),
	GSRendererType::OGL,
#endif
#ifdef ENABLE_VULKAN
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Vulkan"),
	GSRendererType::VK,
#endif
#ifdef __APPLE__
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Metal"),
	GSRendererType::Metal,
#endif
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Software"),
	GSRendererType::SW,
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Null"),
	GSRendererType::Null,
};

static const char* s_anisotropic_filtering_entries[] = {QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Off (Default)"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2x"), QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "4x"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "8x"), QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "16x"), nullptr};
static const char* s_anisotropic_filtering_values[] = {"0", "2", "4", "8", "16", nullptr};

static constexpr int DEFAULT_INTERLACE_MODE = 7;
static constexpr int DEFAULT_TV_SHADER_MODE = 0;

GraphicsSettingsWidget::GraphicsSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	// start hidden, fixup in updateRendererDependentOptions()
	m_ui.hardwareRendererGroup->setVisible(false);
	m_ui.verticalLayout->removeWidget(m_ui.hardwareRendererGroup);
	m_ui.softwareRendererGroup->setVisible(false);
	m_ui.verticalLayout->removeWidget(m_ui.softwareRendererGroup);

	//////////////////////////////////////////////////////////////////////////
	// Global Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.adapter, "EmuCore/GS", "Adapter");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vsync, "EmuCore/GS", "VsyncEnable", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableHWFixes, "EmuCore/GS", "UserHacks", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.spinGPUDuringReadbacks, "EmuCore/GS", "HWSpinGPUForReadbacks", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.spinCPUDuringReadbacks, "EmuCore/GS", "HWSpinCPUForReadbacks", false);

	//////////////////////////////////////////////////////////////////////////
	// Game Display Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.aspectRatio, "EmuCore/GS", "AspectRatio", Pcsx2Config::GSOptions::AspectRatioNames, AspectRatioType::RAuto4_3_3_2);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.fmvAspectRatio, "EmuCore/GS", "FMVAspectRatioSwitch",
		Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames, FMVAspectRatioSwitchType::Off);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.interlacing, "EmuCore/GS", "deinterlace", DEFAULT_INTERLACE_MODE);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.bilinearFiltering, "EmuCore/GS", "linear_present", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.integerScaling, "EmuCore/GS", "IntegerScaling", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCOffsets, "EmuCore/GS", "pcrtc_offsets", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCOverscan, "EmuCore/GS", "pcrtc_overscan", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCAntiBlur, "EmuCore/GS", "pcrtc_antiblur", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.DisableInterlaceOffset, "EmuCore/GS", "disable_interlace_offset", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.internalResolutionScreenshots, "EmuCore/GS", "InternalResolutionScreenshots", false);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.zoom, "EmuCore/GS", "Zoom", 100.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.stretchY, "EmuCore/GS", "StretchY", 100.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropLeft, "EmuCore/GS", "CropLeft", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropTop, "EmuCore/GS", "CropTop", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropRight, "EmuCore/GS", "CropRight", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropBottom, "EmuCore/GS", "CropBottom", 0);

	connect(m_ui.integerScaling, &QCheckBox::stateChanged, this, &GraphicsSettingsWidget::onIntegerScalingChanged);
	onIntegerScalingChanged();

	connect(m_ui.fullscreenModes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onFullscreenModeChanged);

	//////////////////////////////////////////////////////////////////////////
	// OSD Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.osdScale, "EmuCore/GS", "OsdScale", 100.0f);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowMessages, "EmuCore/GS", "OsdShowMessages", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowSpeed, "EmuCore/GS", "OsdShowSpeed", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowFPS, "EmuCore/GS", "OsdShowFPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowCPU, "EmuCore/GS", "OsdShowCPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowGPU, "EmuCore/GS", "OsdShowGPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowResolution, "EmuCore/GS", "OsdShowResolution", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowGSStats, "EmuCore/GS", "OsdShowGSStats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowIndicators, "EmuCore/GS", "OsdShowIndicators", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowSettings, "EmuCore/GS", "OsdShowSettings", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowInputs, "EmuCore/GS", "OsdShowInputs", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.tvShader, "EmuCore/GS", "TVShader", DEFAULT_TV_SHADER_MODE);

	connect(m_ui.shadeBoost, QOverload<int>::of(&QCheckBox::stateChanged), this, &GraphicsSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();

	dialog->registerWidgetHelp(m_ui.osdShowMessages, tr("Show OSD Messages"), tr("Checked"),
		tr("Shows on-screen-display messages when events occur such as save states being "
		   "created/loaded, screenshots being taken, etc."));
	dialog->registerWidgetHelp(m_ui.osdShowFPS, tr("Show Game Frame Rate"), tr("Unchecked"),
		tr("Shows the internal frame rate of the game in the top-right corner of the display."));
	dialog->registerWidgetHelp(m_ui.osdShowSpeed, tr("Show Emulation Speed"), tr("Unchecked"),
		tr("Shows the current emulation speed of the system in the top-right corner of the display as a percentage."));
	dialog->registerWidgetHelp(m_ui.osdShowResolution, tr("Show Resolution"), tr("Unchecked"),
		tr("Shows the resolution of the game in the top-right corner of the display."));
	dialog->registerWidgetHelp(m_ui.osdShowCPU, tr("Show CPU Usage"), tr("Unchecked"),
		tr("Shows host's CPU utilization."));
	dialog->registerWidgetHelp(m_ui.osdShowGPU, tr("Show GPU Usage"), tr("Unchecked"),
		tr("Shows host's GPU utilization."));
	dialog->registerWidgetHelp(m_ui.osdShowGSStats, tr("Show Statistics"), tr("Unchecked"),
		tr("Shows counters for internal graphical utilization, useful for debugging."));
	dialog->registerWidgetHelp(m_ui.osdShowIndicators, tr("Show Indicators"), tr("Unchecked"),
		tr("Shows OSD icon indicators for emulation states such as Pausing, Turbo, Fast Forward, and Slow Motion."));

    dialog->registerWidgetHelp(m_ui.shadeBoost, tr("Shade Boost"), tr("Unchecked"),
		tr("Enables saturation, contrast, and brightness to be adjusted. Values of brightness, saturation, and contrast are at default 50."));
	dialog->registerWidgetHelp(m_ui.fxaa, tr("FXAA"), tr("Unchecked"),
		tr("Applies the FXAA anti-aliasing algorithm to improve the visual quality of games."));


	//////////////////////////////////////////////////////////////////////////
	// HW Settings
	//////////////////////////////////////////////////////////////////////////
	static const char* upscale_entries[] = {
		"Native (PS2)",
		"1.25x Native",
		"1.5x Native",
		"1.75x Native",
		"2x Native (~720p)",
		"2.25x Native",
		"2.5x Native",
		"2.75x Native",
		"3x Native (~1080p)",
		"3.5x Native",
		"4x Native (~1440p/2K)",
		"5x Native (~1620p)",
		"6x Native (~2160p/4K)",
		"7x Native (~2520p)",
		"8x Native (~2880p)",
	nullptr};
	static const char* upscale_values[] = {
		"1",
		"1.25",
		"1.5",
		"1.75",
		"2",
		"2.25",
		"2.5",
		"2.75",
		"3",
		"3.5",
		"4",
		"5",
		"6",
		"7",
		"8",
	nullptr };
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.upscaleMultiplier, "EmuCore/GS", "upscale_multiplier", upscale_entries, upscale_values, "1.0");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.trilinearFiltering, "EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic), -1);
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.anisotropicFiltering, "EmuCore/GS", "MaxAnisotropy", s_anisotropic_filtering_entries, s_anisotropic_filtering_values, "0");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.dithering, "EmuCore/GS", "dithering_ps2", 2);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.mipmapping, "EmuCore/GS", "mipmap_hw", static_cast<int>(HWMipmapLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.crcFixLevel, "EmuCore/GS", "crc_hack_level", static_cast<int>(CRCHackLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.blending, "EmuCore/GS", "accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.gpuPaletteConversion, "EmuCore/GS", "paltex", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.texturePreloading, "EmuCore/GS", "texture_preloading",
		static_cast<int>(TexturePreloadingLevel::Off));

	connect(m_ui.trilinearFiltering, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onTrilinearFilteringChanged);
	connect(m_ui.gpuPaletteConversion, QOverload<int>::of(&QCheckBox::stateChanged), this, &GraphicsSettingsWidget::onGpuPaletteConversionChanged);
	onTrilinearFilteringChanged();
	onGpuPaletteConversionChanged(m_ui.gpuPaletteConversion->checkState());

	//////////////////////////////////////////////////////////////////////////
	// HW Renderer Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.halfScreenFix, "EmuCore/GS", "UserHacks_Half_Bottom_Override", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cpuSpriteRenderBW, "EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cpuCLUTRender, "EmuCore/GS", "UserHacks_CPUCLUTRender", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.skipDrawStart, "EmuCore/GS", "UserHacks_SkipDraw_Start", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.skipDrawEnd, "EmuCore/GS", "UserHacks_SkipDraw_End", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.hwAutoFlush, "EmuCore/GS", "UserHacks_AutoFlush", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.frameBufferConversion, "EmuCore/GS", "UserHacks_CPU_FB_Conversion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableDepthEmulation, "EmuCore/GS", "UserHacks_DisableDepthSupport", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.memoryWrapping, "EmuCore/GS", "wrap_gs_mem", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableSafeFeatures, "EmuCore/GS", "UserHacks_Disable_Safe_Features", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.preloadFrameData, "EmuCore/GS", "preload_frame_with_gs_data", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disablePartialInvalidation, "EmuCore/GS", "UserHacks_DisablePartialInvalidation", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.textureInsideRt, "EmuCore/GS", "UserHacks_TextureInsideRt", false);

	//////////////////////////////////////////////////////////////////////////
	// HW Upscaling Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.halfPixelOffset, "EmuCore/GS", "UserHacks_HalfPixelOffset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.roundSprite, "EmuCore/GS", "UserHacks_round_sprite_offset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureOffsetX, "EmuCore/GS", "UserHacks_TCOffsetX", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureOffsetY, "EmuCore/GS", "UserHacks_TCOffsetY", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.alignSprite, "EmuCore/GS", "UserHacks_align_sprite_X", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.mergeSprite, "EmuCore/GS", "UserHacks_merge_pp_sprite", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.wildHack, "EmuCore/GS", "UserHacks_WildHack", false);

	//////////////////////////////////////////////////////////////////////////
	// Texture Replacements
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpReplaceableTextures, "EmuCore/GS", "DumpReplaceableTextures", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpReplaceableMipmaps, "EmuCore/GS", "DumpReplaceableMipmaps", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpTexturesWithFMVActive, "EmuCore/GS", "DumpTexturesWithFMVActive", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.loadTextureReplacements, "EmuCore/GS", "LoadTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.loadTextureReplacementsAsync, "EmuCore/GS", "LoadTextureReplacementsAsync", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.precacheTextureReplacements, "EmuCore/GS", "PrecacheTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.texturesDirectory, m_ui.texturesBrowse, m_ui.texturesOpen, m_ui.texturesReset,
		"Folders", "Textures", Path::Combine(EmuFolders::DataRoot, "textures"));

	//////////////////////////////////////////////////////////////////////////
	// Advanced Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useBlitSwapChain, "EmuCore/GS", "UseBlitSwapChain", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useDebugDevice, "EmuCore/GS", "UseDebugDevice", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.skipPresentingDuplicateFrames, "EmuCore/GS", "SkipDuplicateFrames", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overrideTextureBarriers, "EmuCore/GS", "OverrideTextureBarriers", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overrideGeometryShader, "EmuCore/GS", "OverrideGeometryShaders", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.gsDumpCompression, "EmuCore/GS", "GSDumpCompression", static_cast<int>(GSDumpCompressionMethod::LZMA));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableFramebufferFetch, "EmuCore/GS", "DisableFramebufferFetch", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableDualSource, "EmuCore/GS", "DisableDualSourceBlend", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.gsDownloadMode, "EmuCore/GS", "HWDownloadMode", static_cast<int>(GSHardwareDownloadMode::Enabled));

	//////////////////////////////////////////////////////////////////////////
	// SW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.swTextureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.extraSWThreads, "EmuCore/GS", "extrathreads", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.swAutoFlush, "EmuCore/GS", "autoflush_sw", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.swMipmap, "EmuCore/GS", "mipmap", true);

	//////////////////////////////////////////////////////////////////////////
	// Non-trivial settings
	//////////////////////////////////////////////////////////////////////////
	const int renderer = m_dialog->getEffectiveIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto));
	for (const RendererInfo& ri : s_renderer_info)
	{
		m_ui.renderer->addItem(qApp->translate("GraphicsSettingsWidget", ri.name));
		if (renderer == static_cast<int>(ri.type))
			m_ui.renderer->setCurrentIndex(m_ui.renderer->count() - 1);
	}

	// per-game override for renderer is slightly annoying, since we need to populate the global setting field
	if (sif)
	{
		const int global_renderer = Host::GetBaseIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto));
		QString global_renderer_name;
		for (const RendererInfo& ri : s_renderer_info)
		{
			if (global_renderer == static_cast<int>(ri.type))
				global_renderer_name = qApp->translate("GraphicsSettingsWidget", ri.name);
		}
		m_ui.renderer->insertItem(0, tr("Use Global Setting [%1]").arg(global_renderer_name));

		// Effective Index already selected, set to global if setting is not per-game
		int override_renderer;
		if (!sif->GetIntValue("EmuCore/GS", "Renderer", &override_renderer))
			m_ui.renderer->setCurrentIndex(0);
	}

	connect(m_ui.renderer, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onRendererChanged);
	connect(m_ui.enableHWFixes, &QCheckBox::stateChanged, this, &GraphicsSettingsWidget::onEnableHardwareFixesChanged);
	connect(m_ui.textureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onTextureFilteringChange);
	connect(m_ui.swTextureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onSWTextureFilteringChange);
	updateRendererDependentOptions();

#ifndef PCSX2_DEVBUILD
	// only allow disabling readbacks for per-game settings, it's too dangerous
	m_ui.gsDownloadMode->setEnabled(m_dialog->isPerGameSettings());

	// Remove texture offset and skipdraw range for global settings.
	if (!m_dialog->isPerGameSettings())
	{
		m_ui.upscalingFixesLayout->removeRow(2);
		m_ui.hardwareFixesLayout->removeRow(2);
		m_ui.hardwareFixesLayout->removeRow(1);
		m_ui.skipDrawStart = nullptr;
		m_ui.skipDrawEnd = nullptr;
		m_ui.textureOffsetX = nullptr;
		m_ui.textureOffsetY = nullptr;
	}
#endif

	// Display tab
	{

		dialog->registerWidgetHelp(m_ui.DisableInterlaceOffset, tr("Disable Interlace Offset"), tr("Unchecked"),
			tr("Disables interlacing offset which may reduce blurring in some situations."));

		dialog->registerWidgetHelp(m_ui.bilinearFiltering, tr("Bilinear Filtering"), tr("Checked"),
			tr("Enables bilinear post processing filter. Smooths the overall picture as it is displayed on the screen. Corrects positioning between pixels."));

		dialog->registerWidgetHelp(m_ui.PCRTCOffsets, tr("Screen Offsets"), tr("Unchecked"),
			tr("Enables PCRTC Offsets which position the screen as the game requests. Useful for some games such as WipEout Fusion for its screen shake effect, but can make the picture blurry."));

		dialog->registerWidgetHelp(m_ui.PCRTCOverscan, tr("Show Overscan"), tr("Unchecked"),
			tr("Enables the option to show the overscan area on games which draw more than the safe area of the screen."));

		dialog->registerWidgetHelp(m_ui.fmvAspectRatio, tr("FMV Aspect Ratio"), tr("Off (Default)"),
			tr("Overrides the FMV aspect ratio."));
	
		dialog->registerWidgetHelp(m_ui.PCRTCAntiBlur, tr("Anti-Blur"), tr("Checked"),
			tr("Enables internal Anti-Blur hacks. Less accurate to PS2 rendering but will make a lot of games look less blurry."));
		
		dialog->registerWidgetHelp(m_ui.vsync, tr("VSync"), tr("Unchecked"),
			tr("Enable this option to match PCSX2's refresh rate with your current monitor or screen. VSync is automatically disabled when it is not possible (eg. running at non-100% speed)."));

		dialog->registerWidgetHelp(m_ui.internalResolutionScreenshots, tr("Internal Resolution Screenshots"), tr("Unchecked"),
			tr("Saves screenshots at internal render resolution and without postprocessing. If this option is disabled, the screenshots will be taken at the window's resolution. Internal resolution screenshots can be very large at high rendering scales."));

		dialog->registerWidgetHelp(m_ui.integerScaling, tr("Integer Scaling"), tr("Unchecked"),
			tr("Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an integer number. May result in a sharper image in some 2D games."));
	}

	// Rendering tab
	{
		// Hardware
		dialog->registerWidgetHelp(m_ui.mipmapping, tr("Mipmapping"), tr("Automatic (Default)"),
			tr("Control the accuracy level of the mipmapping emulation."));

		dialog->registerWidgetHelp(m_ui.textureFiltering, tr("Texture Filtering"), tr("Bilinear (PS2)"),
			tr("Control the texture filtering of the emulation."));

		dialog->registerWidgetHelp(m_ui.trilinearFiltering, tr("Trilinear Filtering"), tr("Automatic (Default)"),
			tr("Control the texture tri-filtering of the emulation."));

		dialog->registerWidgetHelp(m_ui.anisotropicFiltering, tr("Anisotropic Filtering"), tr("Off (Default)"),
			tr("Reduces texture aliasing at extreme viewing angles."));

		dialog->registerWidgetHelp(m_ui.dithering, tr("Dithering"), tr("Unscaled (Default)"),
			tr("Reduces banding between colors and improves the perceived color depth."));

		dialog->registerWidgetHelp(m_ui.crcFixLevel, tr("CRC Fix Level"), tr("Automatic (Default)"),
			tr("Control the number of Auto-CRC fixes and hacks applied to games."));

		dialog->registerWidgetHelp(m_ui.blending, tr("Blending Accuracy"), tr("Basic (Recommended)"),
			tr("Control the accuracy level of the GS blending unit emulation. "
			   "The higher the setting, the more blending is emulated in the shader accurately, and the higher the speed penalty will be. "
			   "Do note that Direct3D's blending is reduced in capability compared to OpenGL/Vulkan"));

		dialog->registerWidgetHelp(m_ui.texturePreloading, tr("Texture Preloading"), tr("Full (Hash Cache)"),
			tr("Uploads entire textures at once instead of small pieces, avoiding redundant uploads when possible. "
			   "Improves performance in most games, but can make a small selection slower."));

		dialog->registerWidgetHelp(m_ui.gpuPaletteConversion, tr("GPU Palette Conversion"), tr("Unchecked"),
			tr("When enabled GPU converts colormap-textures, otherwise the CPU will. "
			   "It is a trade-off between GPU and CPU."));

		dialog->registerWidgetHelp(m_ui.enableHWFixes, tr("Manual Hardware Renderer Fixes"), tr("Unchecked"),
			tr("Enabling this option gives you the ability to change the renderer and upscaling fixes "
			   "to your games. However IF you have ENABLED this, you WILL DISABLE AUTOMATIC "
			   "SETTINGS and you can re-enable automatic settings by unchecking this option."));

		dialog->registerWidgetHelp(m_ui.spinCPUDuringReadbacks, tr("Spin CPU During Readbacks"), tr("Unchecked"),
			tr("Does useless work on the CPU during readbacks to prevent it from going to into powersave modes. "
			   "May improve performance but with a significant increase in power usage."));

		dialog->registerWidgetHelp(m_ui.spinGPUDuringReadbacks, tr("Spin GPU During Readbacks"), tr("Unchecked"),
			tr("Submits useless work to the GPU during readbacks to prevent it from going into powersave modes. "
			   "May improve performance but with a significant increase in power usage."));

		// Software
		dialog->registerWidgetHelp(m_ui.extraSWThreads, tr("Extra Rendering Threads"), tr("2 threads"),
			tr("Number of rendering threads: 0 for single thread, 2 or more for multithread (1 is for debugging). "
			   "If you have 4 threads on your CPU pick 2 or 3. You can calculate how to get the best performance (amount of CPU threads - 2). "
			   "7+ threads will not give much more performance and could perhaps even lower it."));

		dialog->registerWidgetHelp(m_ui.swAutoFlush, tr("Auto Flush"), tr("Checked"),
			tr("Force a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog->registerWidgetHelp(m_ui.swMipmap, tr("Mipmapping"), tr("Checked"),
			tr("Enables mipmapping, which some games require to render correctly."));
	}

	// Hardware Fixes tab
	{
		dialog->registerWidgetHelp(m_ui.halfScreenFix, tr("Half Screen Fix"), tr("Automatic (Default)"),
			tr("Control the half-screen fix detection on texture shuffling."));

		dialog->registerWidgetHelp(m_ui.skipDrawStart, tr("Skipdraw Range Start"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog->registerWidgetHelp(m_ui.skipDrawEnd, tr("Skipdraw Range End"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog->registerWidgetHelp(m_ui.hwAutoFlush, tr("Auto Flush"), tr("Unchecked"),
			tr("Force a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog->registerWidgetHelp(m_ui.disableDepthEmulation, tr("Disable Depth Emulation"), tr("Unchecked"),
			tr("Disable the support of Depth buffer in the texture cache. "
			   "It can help to increase speed but it will likely create various glitches."));

		dialog->registerWidgetHelp(m_ui.disableSafeFeatures, tr("Disable Safe Features"), tr("Unchecked"),
			tr("This option disables multiple safe features. "
			   "Disables accurate Unscale Point and Line rendering which can help Xenosaga games. "
			   "Disables accurate GS Memory Clearing to be done on the CPU, and let the GPU handle it, which can help Kingdom Hearts games."));

		dialog->registerWidgetHelp(m_ui.disablePartialInvalidation, tr("Disable Partial Invalidation"), tr("Unchecked"),
			tr("By default, the texture cache handles partial invalidations. Unfortunately it is very costly to compute CPU wise. "
			   "This hack replaces the partial invalidation with a complete deletion of the texture to reduce the CPU load. "
			   "It helps snowblind engine games."));
		dialog->registerWidgetHelp(m_ui.frameBufferConversion, tr("Frame Buffer Conversion"), tr("Unchecked"),
			tr("Convert 4-bit and 8-bit frame buffer on the CPU instead of the GPU. "
			   "Helps Harry Potter and Stuntman games. It has a big impact on performance."));

		dialog->registerWidgetHelp(m_ui.memoryWrapping, tr("Memory Wrapping"), tr("Unchecked"),
			tr("Emulates GS memory wrapping accurately. "
			   "This fixes issues where part of the image is cut-off by block shaped sections such as the FMVs in "
			   "Wallace & Gromit: The Curse of the Were-Rabbit and Thrillville."));

		dialog->registerWidgetHelp(m_ui.preloadFrameData, tr("Preload Frame Data"), tr("Unchecked"),
			tr("Uploads GS data when rendering a new frame to reproduce some effects accurately. "
			   "Fixes black screen issues in games like Armored Core: Last Raven."));

		dialog->registerWidgetHelp(m_ui.textureInsideRt, tr("Texture Inside RT"), tr("Unchecked"),
			tr("Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer. "
			   "In some selected games this is enabled by default regardless of this setting."));
	}

	// Upscaling Fixes tab
	{
		dialog->registerWidgetHelp(m_ui.halfPixelOffset, tr("Half Pixel Offset"), tr("Off (Default)"),
			tr("Might fix some misaligned fog, bloom, or blend effect."));

		dialog->registerWidgetHelp(m_ui.roundSprite, tr("Round Sprite"), tr("Off (Default)"),
			tr("Corrects the sampling of 2D sprite textures when upscaling. "
			   "Fixes lines in sprites of games like Ar tonelico when upscaling. Half option is for flat sprites, Full is for all sprites."));

		dialog->registerWidgetHelp(m_ui.textureOffsetX, tr("Texture Offsets X"), tr("0"),
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too."));

		dialog->registerWidgetHelp(m_ui.textureOffsetY, tr("Texture Offsets Y"), tr("0"),
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment too."));

		dialog->registerWidgetHelp(m_ui.alignSprite, tr("Align Sprite"), tr("Unchecked"),
			tr("Fixes issues with upscaling(vertical lines) in Namco games like Ace Combat, Tekken, Soul Calibur, etc."));

		dialog->registerWidgetHelp(m_ui.wildHack, tr("Wild Arms Hack"), tr("Unchecked"),
			tr("Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games."));

		dialog->registerWidgetHelp(m_ui.mergeSprite, tr("Merge Sprite"), tr("Unchecked"),
			tr("Replaces post-processing multiple paving sprites by a single fat sprite. It reduces various upscaling lines."));
	}

	// Advanced tab
	{
		dialog->registerWidgetHelp(m_ui.overrideGeometryShader, tr("Override Geometry Shader"), tr("Automatic (Default)"),
			tr("Allows the GPU instead of just the CPU to transform lines into sprites. "
			   "This reduces CPU load and bandwidth requirement, but it is heavier on the GPU."));

		dialog->registerWidgetHelp(m_ui.useBlitSwapChain, tr("Use Blit Swap Chain"), tr("Unchecked"),
			tr("Uses a blit presentation model instead of flipping when using the Direct3D 11 "
			   "renderer. This usually results in slower performance, but may be required for some "
			   "streaming applications, or to uncap framerates on some systems."));

		dialog->registerWidgetHelp(m_ui.skipPresentingDuplicateFrames, tr("Skip Presenting Duplicate Frames"), tr("Unchecked"),
			tr("Detects when idle frames are being presented in 25/30fps games, and skips presenting those frames. The frame is still rendered, it just means "
			   "the GPU has more time to complete it (this is NOT frame skipping). Can smooth our frame time fluctuations when the CPU/GPU are near maximum "
			   "utilization, but makes frame pacing more inconsistent and can increase input lag."));

		dialog->registerWidgetHelp(m_ui.gsDownloadMode, tr("GS Download Mode"), tr("Accurate"),
			tr("Skips synchronizing with the GS thread and host GPU for GS downloads. "
			   "Can result in a large speed boost on slower systems, at the cost of many broken graphical effects. "
			   "If games are broken and you have this option enabled, please disable it first."));
	}
}

GraphicsSettingsWidget::~GraphicsSettingsWidget() = default;

void GraphicsSettingsWidget::onTextureFilteringChange()
{
	const QSignalBlocker block(m_ui.swTextureFiltering);

	m_ui.swTextureFiltering->setCurrentIndex(m_ui.textureFiltering->currentIndex());
}

void GraphicsSettingsWidget::onSWTextureFilteringChange()
{
	const QSignalBlocker block(m_ui.textureFiltering);

	m_ui.textureFiltering->setCurrentIndex(m_ui.swTextureFiltering->currentIndex());
}

void GraphicsSettingsWidget::onRendererChanged(int index)
{
	if (m_dialog->isPerGameSettings())
	{
		if (index > 0)
			m_dialog->setIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(s_renderer_info[index - 1].type));
		else
			m_dialog->setIntSettingValue("EmuCore/GS", "Renderer", std::nullopt);
	}
	else
	{
		m_dialog->setIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(s_renderer_info[index].type));
	}

	g_emu_thread->applySettings();
	updateRendererDependentOptions();
}

void GraphicsSettingsWidget::onAdapterChanged(int index)
{
	const int first_adapter = m_dialog->isPerGameSettings() ? 2 : 1;

	if (index >= first_adapter)
		m_dialog->setStringSettingValue("EmuCore/GS", "Adapter", m_ui.adapter->currentText().toUtf8().constData());
	else if (index > 0 && m_dialog->isPerGameSettings())
		m_dialog->setStringSettingValue("EmuCore/GS", "Adapter", "");
	else
		m_dialog->setStringSettingValue("EmuCore/GS", "Adapter", std::nullopt);

	g_emu_thread->applySettings();
}

void GraphicsSettingsWidget::onFullscreenModeChanged(int index)
{
	const int first_mode = m_dialog->isPerGameSettings() ? 2 : 1;

	if (index >= first_mode)
		m_dialog->setStringSettingValue("EmuCore/GS", "FullscreenMode", m_ui.fullscreenModes->currentText().toUtf8().constData());
	else if (index > 0 && m_dialog->isPerGameSettings())
		m_dialog->setStringSettingValue("EmuCore/GS", "FullscreenMode", "");
	else
		m_dialog->setStringSettingValue("EmuCore/GS", "FullscreenMode", std::nullopt);

	g_emu_thread->applySettings();
}

void GraphicsSettingsWidget::onIntegerScalingChanged()
{
	m_ui.bilinearFiltering->setEnabled(!m_ui.integerScaling->isChecked());
}

void GraphicsSettingsWidget::onTrilinearFilteringChanged()
{
	const bool forced_bilinear =
		(m_dialog->getEffectiveIntValue("EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic))
			>= static_cast<int>(TriFiltering::Forced));
	m_ui.textureFiltering->setDisabled(forced_bilinear);
}

void GraphicsSettingsWidget::onShadeBoostChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "ShadeBoost", false);
	m_ui.shadeBoostBrightness->setEnabled(enabled);
	m_ui.shadeBoostContrast->setEnabled(enabled);
	m_ui.shadeBoostSaturation->setEnabled(enabled);
}

void GraphicsSettingsWidget::onGpuPaletteConversionChanged(int state)
{
	const bool enabled = state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("EmuCore/GS", "paltex", false) : state;

	m_ui.anisotropicFiltering->setEnabled(!enabled);
}

void GraphicsSettingsWidget::onEnableHardwareFixesChanged()
{
	const bool enabled = (m_ui.enableHWFixes->checkState() == Qt::Checked);
	m_ui.hardwareRendererGroup->setTabEnabled(2, enabled);
	m_ui.hardwareRendererGroup->setTabEnabled(3, enabled);
}

GSRendererType GraphicsSettingsWidget::getEffectiveRenderer() const
{
	const GSRendererType type =
		static_cast<GSRendererType>(m_dialog->getEffectiveIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
	return (type == GSRendererType::Auto) ? GSUtil::GetPreferredRenderer() : type;
}

void GraphicsSettingsWidget::updateRendererDependentOptions()
{
	const GSRendererType type = getEffectiveRenderer();

#ifdef _WIN32
	const bool is_dx11 = (type == GSRendererType::DX11 || type == GSRendererType::SW);
	const bool is_sw_dx = (type == GSRendererType::DX11 || type == GSRendererType::DX12 || type == GSRendererType::SW);
#else
	const bool is_dx11 = false;
	const bool is_sw_dx = false;
#endif

	const bool is_hardware = (type == GSRendererType::DX11 || type == GSRendererType::DX12 || type == GSRendererType::OGL || type == GSRendererType::VK || type == GSRendererType::Metal);
	const bool is_software = (type == GSRendererType::SW);
	const int current_tab = m_hardware_renderer_visible ? m_ui.hardwareRendererGroup->currentIndex() : m_ui.softwareRendererGroup->currentIndex();

	// move advanced tab to the correct parent
	static constexpr std::array<const char*, 3> move_tab_names = {{"Display", "OSD", "Advanced"}};
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

	if (m_hardware_renderer_visible != is_hardware)
	{
		m_ui.hardwareRendererGroup->setVisible(is_hardware);
		if (!is_hardware)
		{
			m_ui.verticalLayout->removeWidget(m_ui.hardwareRendererGroup);
		}
		else
		{
			// map first two tabs over, skip hacks
			m_ui.verticalLayout->insertWidget(1, m_ui.hardwareRendererGroup);
			m_ui.hardwareRendererGroup->setCurrentIndex((current_tab < 2) ? current_tab : (current_tab + 3));
		}

		m_hardware_renderer_visible = is_hardware;
	}

	if (m_software_renderer_visible != is_software)
	{
		m_ui.softwareRendererGroup->setVisible(is_software);
		if (is_hardware)
		{
			m_ui.verticalLayout->removeWidget(m_ui.softwareRendererGroup);
		}
		else
		{
			// software has no hacks tabs
			m_ui.verticalLayout->insertWidget(1, m_ui.softwareRendererGroup);
			m_ui.softwareRendererGroup->setCurrentIndex((current_tab >= 5) ? (current_tab - 3) : (current_tab >= 2 ? 1 : current_tab));
		}

		m_software_renderer_visible = is_software;
	}

	m_ui.overrideTextureBarriers->setDisabled(is_sw_dx);
	m_ui.overrideGeometryShader->setDisabled(is_sw_dx);

	m_ui.useBlitSwapChain->setEnabled(is_dx11);

	// populate adapters
	HostDisplay::AdapterAndModeList modes;
	switch (type)
	{
#ifdef _WIN32
		case GSRendererType::DX11:
			modes = D3D11HostDisplay::StaticGetAdapterAndModeList();
			break;
		case GSRendererType::DX12:
			modes = D3D12HostDisplay::StaticGetAdapterAndModeList();
			break;
#endif

#ifdef ENABLE_VULKAN
		case GSRendererType::VK:
			modes = VulkanHostDisplay::StaticGetAdapterAndModeList(nullptr);
			break;
#endif

#ifdef __APPLE__
		case GSRendererType::Metal:
			modes = GetMetalAdapterAndModeList();
			break;
#endif

		case GSRendererType::OGL:
		case GSRendererType::SW:
		case GSRendererType::Null:
		case GSRendererType::Auto:
		default:
			break;
	}

	// fill+select adapters
	{
		QSignalBlocker sb(m_ui.adapter);

		std::string current_adapter = Host::GetBaseStringSettingValue("EmuCore/GS", "Adapter", "");
		m_ui.adapter->clear();
		m_ui.adapter->setEnabled(!modes.adapter_names.empty());
		m_ui.adapter->addItem(tr("(Default)"));
		m_ui.adapter->setCurrentIndex(0);

		if (m_dialog->isPerGameSettings())
		{
			m_ui.adapter->insertItem(
				0, tr("Use Global Setting [%1]").arg(current_adapter.empty() ? tr("(Default)") : QString::fromStdString(current_adapter)));
			if (!m_dialog->getSettingsInterface()->GetStringValue("EmuCore/GS", "Adapter", &current_adapter))
			{
				// clear the adapter so we don't set it to the global value
				current_adapter.clear();
				m_ui.adapter->setCurrentIndex(0);
			}
		}

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

		std::string current_mode(Host::GetBaseStringSettingValue("EmuCore/GS", "FullscreenMode", ""));
		m_ui.fullscreenModes->clear();
		m_ui.fullscreenModes->addItem(tr("Borderless Fullscreen"));
		m_ui.fullscreenModes->setCurrentIndex(0);

		if (m_dialog->isPerGameSettings())
		{
			m_ui.fullscreenModes->insertItem(
				0, tr("Use Global Setting [%1]").arg(current_mode.empty() ? tr("(Default)") : QString::fromStdString(current_mode)));
			if (!m_dialog->getSettingsInterface()->GetStringValue("EmuCore/GS", "FullscreenMode", &current_mode))
			{
				current_mode.clear();
				m_ui.fullscreenModes->setCurrentIndex(0);
			}
		}

		for (const std::string& fs_mode : modes.fullscreen_modes)
		{
			m_ui.fullscreenModes->addItem(QString::fromStdString(fs_mode));
			if (current_mode == fs_mode)
				m_ui.fullscreenModes->setCurrentIndex(m_ui.fullscreenModes->count() - 1);
		}
	}

	m_ui.enableHWFixes->setEnabled(is_hardware);
	if (is_hardware)
		onEnableHardwareFixesChanged();
}
