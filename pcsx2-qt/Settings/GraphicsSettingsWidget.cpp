// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GraphicsSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"
#include <QtWidgets/QMessageBox>

#include "pcsx2/Host.h"
#include "pcsx2/GS/GS.h"
#include "pcsx2/GS/GSCapture.h"
#include "pcsx2/GS/GSUtil.h"

struct RendererInfo
{
	const char* name;
	GSRendererType type;
};

static constexpr RendererInfo s_renderer_info[] = {
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Automatic (Default)"), GSRendererType::Auto},
#ifdef _WIN32
	//: Graphics backend/engine type. Leave as-is.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Direct3D 11"), GSRendererType::DX11},
	//: Graphics backend/engine type. Leave as-is.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Direct3D 12"), GSRendererType::DX12},
#endif
#ifdef ENABLE_OPENGL
	//: Graphics backend/engine type. Leave as-is.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "OpenGL"), GSRendererType::OGL},
#endif
#ifdef ENABLE_VULKAN
	//: Graphics backend/engine type. Leave as-is.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Vulkan"), GSRendererType::VK},
#endif
#ifdef __APPLE__
	//: Graphics backend/engine type. Leave as-is.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Metal"), GSRendererType::Metal},
#endif
	//: Graphics backend/engine type (refers to emulating the GS in software, on the CPU). Translate accordingly.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Software"), GSRendererType::SW},
	//: Null here means that this is a graphics backend that will show nothing.
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Null"), GSRendererType::Null},
};

static const char* s_anisotropic_filtering_entries[] = {QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Off (Default)"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2x"), QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "4x"),
	QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "8x"), QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "16x"), nullptr};
static const char* s_anisotropic_filtering_values[] = {"0", "2", "4", "8", "16", nullptr};

static constexpr int DEFAULT_INTERLACE_MODE = 0;
static constexpr int DEFAULT_TV_SHADER_MODE = 0;
static constexpr int DEFAULT_CAS_SHARPNESS = 50;

GraphicsSettingsWidget::GraphicsSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

#ifndef PCSX2_DEVBUILD
	if (!m_dialog->isPerGameSettings())
	{
		// We removed hardware fixes from global settings, but people in the past did set this stuff globally.
		// So, just reset it all. We can remove this code at some point in the future.
		resetManualHardwareFixes();
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Global Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.adapterDropdown, "EmuCore/GS", "Adapter");
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
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.interlacing, "EmuCore/GS", "deinterlace_mode", DEFAULT_INTERLACE_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.bilinearFiltering, "EmuCore/GS", "linear_present_mode", static_cast<int>(GSPostBilinearMode::BilinearSmooth));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.widescreenPatches, "EmuCore", "EnableWideScreenPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.noInterlacingPatches, "EmuCore", "EnableNoInterlacingPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.integerScaling, "EmuCore/GS", "IntegerScaling", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCOffsets, "EmuCore/GS", "pcrtc_offsets", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCOverscan, "EmuCore/GS", "pcrtc_overscan", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.PCRTCAntiBlur, "EmuCore/GS", "pcrtc_antiblur", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.DisableInterlaceOffset, "EmuCore/GS", "disable_interlace_offset", false);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.screenshotSize, "EmuCore/GS", "ScreenshotSize", static_cast<int>(GSScreenshotSize::WindowResolution));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.screenshotFormat, "EmuCore/GS", "ScreenshotFormat", static_cast<int>(GSScreenshotFormat::PNG));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.screenshotQuality, "EmuCore/GS", "ScreenshotQuality", 50);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.stretchY, "EmuCore/GS", "StretchY", 100.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropLeft, "EmuCore/GS", "CropLeft", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropTop, "EmuCore/GS", "CropTop", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropRight, "EmuCore/GS", "CropRight", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cropBottom, "EmuCore/GS", "CropBottom", 0);

	connect(
		m_ui.fullscreenModes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onFullscreenModeChanged);

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
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowFrameTimes, "EmuCore/GS", "OsdShowFrameTimes", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.osdShowVersion, "EmuCore/GS", "OsdShowVersion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.warnAboutUnsafeSettings, "EmuCore", "WarnAboutUnsafeSettings", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.tvShader, "EmuCore/GS", "TVShader", DEFAULT_TV_SHADER_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.casMode, "EmuCore/GS", "CASMode", static_cast<int>(GSCASMode::Disabled));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.casSharpness, "EmuCore/GS", "CASSharpness", DEFAULT_CAS_SHARPNESS);

	connect(m_ui.shadeBoost, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();

	//////////////////////////////////////////////////////////////////////////
	// HW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.trilinearFiltering, "EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic), -1);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.anisotropicFiltering, "EmuCore/GS", "MaxAnisotropy",
		s_anisotropic_filtering_entries, s_anisotropic_filtering_values, "0");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.dithering, "EmuCore/GS", "dithering_ps2", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.mipmapping, "EmuCore/GS", "hw_mipmap", true);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.blending, "EmuCore/GS", "accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.texturePreloading, "EmuCore/GS", "texture_preloading", static_cast<int>(TexturePreloadingLevel::Off));
	connect(m_ui.upscaleMultiplier, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onUpscaleMultiplierChanged);
	connect(m_ui.trilinearFiltering, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onTrilinearFilteringChanged);
	onTrilinearFilteringChanged();

	//////////////////////////////////////////////////////////////////////////
	// HW Renderer Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cpuSpriteRenderBW, "EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cpuSpriteRenderLevel, "EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.cpuCLUTRender, "EmuCore/GS", "UserHacks_CPUCLUTRender", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.gpuTargetCLUTMode, "EmuCore/GS", "UserHacks_GPUTargetCLUTMode", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.skipDrawStart, "EmuCore/GS", "UserHacks_SkipDraw_Start", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.skipDrawEnd, "EmuCore/GS", "UserHacks_SkipDraw_End", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.hwAutoFlush, "EmuCore/GS", "UserHacks_AutoFlushLevel", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.frameBufferConversion, "EmuCore/GS", "UserHacks_CPU_FB_Conversion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableDepthEmulation, "EmuCore/GS", "UserHacks_DisableDepthSupport", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableSafeFeatures, "EmuCore/GS", "UserHacks_Disable_Safe_Features", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableRenderFixes, "EmuCore/GS", "UserHacks_DisableRenderFixes", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.preloadFrameData, "EmuCore/GS", "preload_frame_with_gs_data", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.disablePartialInvalidation, "EmuCore/GS", "UserHacks_DisablePartialInvalidation", false);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.textureInsideRt, "EmuCore/GS", "UserHacks_TextureInsideRt", static_cast<int>(GSTextureInRtMode::Disabled));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.readTCOnClose, "EmuCore/GS", "UserHacks_ReadTCOnClose", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.estimateTextureRegion, "EmuCore/GS", "UserHacks_EstimateTextureRegion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.gpuPaletteConversion, "EmuCore/GS", "paltex", false);
	connect(m_ui.cpuSpriteRenderBW, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onCPUSpriteRenderBWChanged);
	connect(m_ui.gpuPaletteConversion, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onGpuPaletteConversionChanged);
	onCPUSpriteRenderBWChanged();
	onGpuPaletteConversionChanged(m_ui.gpuPaletteConversion->checkState());

	//////////////////////////////////////////////////////////////////////////
	// HW Upscaling Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.halfPixelOffset, "EmuCore/GS", "UserHacks_HalfPixelOffset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.nativeScaling, "EmuCore/GS", "UserHacks_native_scaling", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.roundSprite, "EmuCore/GS", "UserHacks_round_sprite_offset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.bilinearHack, "EmuCore/GS", "UserHacks_BilinearHack", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureOffsetX, "EmuCore/GS", "UserHacks_TCOffsetX", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureOffsetY, "EmuCore/GS", "UserHacks_TCOffsetY", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.alignSprite, "EmuCore/GS", "UserHacks_align_sprite_X", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.mergeSprite, "EmuCore/GS", "UserHacks_merge_pp_sprite", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.forceEvenSpritePosition, "EmuCore/GS", "UserHacks_forceEvenSpritePosition", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.nativePaletteDraw, "EmuCore/GS", "UserHacks_NativePaletteDraw", false);

	//////////////////////////////////////////////////////////////////////////
	// Texture Replacements
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpReplaceableTextures, "EmuCore/GS", "DumpReplaceableTextures", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpReplaceableMipmaps, "EmuCore/GS", "DumpReplaceableMipmaps", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpTexturesWithFMVActive, "EmuCore/GS", "DumpTexturesWithFMVActive", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.loadTextureReplacements, "EmuCore/GS", "LoadTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.loadTextureReplacementsAsync, "EmuCore/GS", "LoadTextureReplacementsAsync", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.precacheTextureReplacements, "EmuCore/GS", "PrecacheTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.texturesDirectory, m_ui.texturesBrowse, m_ui.texturesOpen, m_ui.texturesReset,
		"Folders", "Textures", Path::Combine(EmuFolders::DataRoot, "textures"));
	connect(m_ui.dumpReplaceableTextures, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onTextureDumpChanged);
	connect(m_ui.loadTextureReplacements, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onTextureReplacementChanged);
	onTextureDumpChanged();
	onTextureReplacementChanged();

	if (m_dialog->isPerGameSettings())
	{
		m_ui.textureReplacementLayout->removeWidget(m_ui.texturesDirectoryBox);
		m_ui.texturesDirectoryBox->deleteLater();
		m_ui.texturesDirectoryBox = nullptr;
		m_ui.texturesDirectory = nullptr;
		m_ui.texturesBrowse = nullptr;
		m_ui.texturesOpen = nullptr;
		m_ui.texturesReset = nullptr;
		m_ui.textureDescriptionText = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	// Advanced Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useBlitSwapChain, "EmuCore/GS", "UseBlitSwapChain", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useDebugDevice, "EmuCore/GS", "UseDebugDevice", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.skipPresentingDuplicateFrames, "EmuCore/GS", "SkipDuplicateFrames", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableMailboxPresentation, "EmuCore/GS", "DisableMailboxPresentation", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.extendedUpscales, "EmuCore/GS", "ExtendedUpscalingMultipliers", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.exclusiveFullscreenControl, "EmuCore/GS", "ExclusiveFullscreenControl", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overrideTextureBarriers, "EmuCore/GS", "OverrideTextureBarriers", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.gsDumpCompression, "EmuCore/GS", "GSDumpCompression", static_cast<int>(GSDumpCompressionMethod::Zstandard));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableFramebufferFetch, "EmuCore/GS", "DisableFramebufferFetch", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableShaderCache, "EmuCore/GS", "DisableShaderCache", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableVertexShaderExpand, "EmuCore/GS", "DisableVertexShaderExpand", false);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.gsDownloadMode, "EmuCore/GS", "HWDownloadMode", static_cast<int>(GSHardwareDownloadMode::Enabled));

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
		m_ui.rendererDropdown->addItem(qApp->translate("GraphicsSettingsWidget", ri.name));
		if (renderer == static_cast<int>(ri.type))
			m_ui.rendererDropdown->setCurrentIndex(m_ui.rendererDropdown->count() - 1);
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
		m_ui.rendererDropdown->insertItem(0, tr("Use Global Setting [%1]").arg(global_renderer_name));

		// Effective Index already selected, set to global if setting is not per-game
		int override_renderer;
		if (!sif->GetIntValue("EmuCore/GS", "Renderer", &override_renderer))
			m_ui.rendererDropdown->setCurrentIndex(0);
	}

	connect(m_ui.rendererDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onRendererChanged);
	connect(m_ui.enableHWFixes, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::updateRendererDependentOptions);
	connect(m_ui.extendedUpscales, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::updateRendererDependentOptions);
	connect(m_ui.textureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onTextureFilteringChange);
	connect(m_ui.swTextureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onSWTextureFilteringChange);
	updateRendererDependentOptions();

#ifndef _WIN32
	// Exclusive fullscreen control is Windows-only.
	m_ui.advancedOptionsFormLayout->removeRow(2);
	m_ui.exclusiveFullscreenControl = nullptr;
#endif

#ifndef PCSX2_DEVBUILD
	if (!m_dialog->isPerGameSettings())
	{
		// Only allow disabling readbacks for per-game settings, it's too dangerous.
		m_ui.advancedOptionsFormLayout->removeRow(0);
		m_ui.gsDownloadMode = nullptr;

		// Don't allow setting hardware fixes globally.
		// Too many stupid YouTube "best settings" guides that break other games.
		m_ui.hardwareRenderingOptionsLayout->removeWidget(m_ui.enableHWFixes);
		delete m_ui.enableHWFixes;
		m_ui.enableHWFixes = nullptr;
	}
#endif

	// Get rid of widescreen/no-interlace checkboxes from per-game settings, unless the user previously had them set.
	if (m_dialog->isPerGameSettings())
	{
		if ((m_dialog->containsSettingValue("EmuCore", "EnableWideScreenPatches") || m_dialog->containsSettingValue("EmuCore", "EnableNoInterlacingPatches")) &&
			QMessageBox::question(QtUtils::GetRootWidget(this), tr("Remove Unsupported Settings"),
				tr("You currently have the <strong>Enable Widescreen Patches</strong> or <strong>Enable No-Interlacing Patches</strong> options enabled for this game.<br><br>"
				   "We no longer support these options, instead <strong>you should select the \"Patches\" section, and explicitly enable the patches you want.</strong><br><br>"
				   "Do you want to remove these options from your game configuration now?"),
				QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
		{
			m_dialog->removeSettingValue("EmuCore", "EnableWideScreenPatches");
			m_dialog->removeSettingValue("EmuCore", "EnableNoInterlacingPatches");
		}

		m_ui.displayGridLayout->removeWidget(m_ui.widescreenPatches);
		m_ui.displayGridLayout->removeWidget(m_ui.noInterlacingPatches);
		safe_delete(m_ui.widescreenPatches);
		safe_delete(m_ui.noInterlacingPatches);		
	}

	// Hide advanced options by default.
	if (!QtHost::ShouldShowAdvancedSettings())
	{
		// Advanced is always the last tab. Index is different for HW vs SW.
		m_ui.tabs->removeTab(m_ui.tabs->count() - 1);
		m_ui.advancedTab->deleteLater();
		m_ui.advancedTab = nullptr;
		m_ui.gsDownloadMode = nullptr;
		m_ui.gsDumpCompression = nullptr;
		m_ui.exclusiveFullscreenControl = nullptr;
		m_ui.useBlitSwapChain = nullptr;
		m_ui.disableMailboxPresentation = nullptr;
		m_ui.extendedUpscales = nullptr;
		m_ui.skipPresentingDuplicateFrames = nullptr;
		m_ui.overrideTextureBarriers = nullptr;
		m_ui.disableFramebufferFetch = nullptr;
		m_ui.disableShaderCache = nullptr;
		m_ui.disableVertexShaderExpand = nullptr;
		m_ui.useDebugDevice = nullptr;
	}

	// Capture settings
	{
		for (const char** container = Pcsx2Config::GSOptions::CaptureContainers; *container; container++)
		{
			const QString name(QString::fromUtf8(*container));
			m_ui.captureContainer->addItem(name.toUpper(), name);
		}

		SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.videoDumpingDirectory, m_ui.videoDumpingDirectoryBrowse,
			m_ui.videoDumpingDirectoryOpen, m_ui.videoDumpingDirectoryReset, "Folders", "Videos",
			Path::Combine(EmuFolders::DataRoot, "videos"));

		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.captureContainer, "EmuCore/GS", "CaptureContainer");
		connect(m_ui.captureContainer, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onCaptureContainerChanged);

		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableVideoCapture, "EmuCore/GS", "EnableVideoCapture", true);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_ui.videoCaptureBitrate, "EmuCore/GS", "VideoCaptureBitrate", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_BITRATE);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_ui.videoCaptureWidth, "EmuCore/GS", "VideoCaptureWidth", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_WIDTH);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_ui.videoCaptureHeight, "EmuCore/GS", "VideoCaptureHeight", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_HEIGHT);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_ui.videoCaptureResolutionAuto, "EmuCore/GS", "VideoCaptureAutoResolution", true);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_ui.enableVideoCaptureArguments, "EmuCore/GS", "EnableVideoCaptureParameters", false);
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.videoCaptureArguments, "EmuCore/GS", "VideoCaptureParameters");
		connect(m_ui.enableVideoCapture, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onEnableVideoCaptureChanged);
		connect(
			m_ui.videoCaptureResolutionAuto, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onVideoCaptureAutoResolutionChanged);
		connect(m_ui.enableVideoCaptureArguments, &QCheckBox::checkStateChanged, this,
			&GraphicsSettingsWidget::onEnableVideoCaptureArgumentsChanged);

		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableAudioCapture, "EmuCore/GS", "EnableAudioCapture", true);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_ui.audioCaptureBitrate, "EmuCore/GS", "AudioCaptureBitrate", Pcsx2Config::GSOptions::DEFAULT_AUDIO_CAPTURE_BITRATE);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_ui.enableAudioCaptureArguments, "EmuCore/GS", "EnableAudioCaptureParameters", false);
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.audioCaptureArguments, "EmuCore/GS", "AudioCaptureParameters");
		connect(m_ui.enableAudioCapture, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onEnableAudioCaptureChanged);
		connect(m_ui.enableAudioCaptureArguments, &QCheckBox::checkStateChanged, this,
			&GraphicsSettingsWidget::onEnableAudioCaptureArgumentsChanged);

		onCaptureContainerChanged();
		onCaptureCodecChanged();
		onEnableVideoCaptureChanged();
		onEnableVideoCaptureArgumentsChanged();
		onVideoCaptureAutoResolutionChanged();
		onEnableAudioCaptureChanged();
		onEnableAudioCaptureArgumentsChanged();

		if (m_dialog->isPerGameSettings())
		{
			m_ui.recordingTabLayout->removeWidget(m_ui.videoDumpDirectory);
			m_ui.videoDumpDirectory->deleteLater();
			m_ui.videoDumpDirectory = nullptr;
			m_ui.videoDumpLayout = nullptr;
			m_ui.videoDumpingDirectory = nullptr;
			m_ui.videoDumpingDirectoryBrowse = nullptr;
			m_ui.videoDumpingDirectoryOpen = nullptr;
			m_ui.videoDumpingDirectoryReset = nullptr;
		}
	}

	// Display tab
	{
		dialog->registerWidgetHelp(m_ui.widescreenPatches, tr("Enable Widescreen Patches"), tr("Unchecked"),
			tr("Automatically loads and applies widescreen patches on game start. Can cause issues."));

		dialog->registerWidgetHelp(m_ui.noInterlacingPatches, tr("Enable No-Interlacing Patches"), tr("Unchecked"),
			tr("Automatically loads and applies no-interlacing patches on game start. Can cause issues."));

		dialog->registerWidgetHelp(m_ui.DisableInterlaceOffset, tr("Disable Interlace Offset"), tr("Unchecked"),
			tr("Disables interlacing offset which may reduce blurring in some situations."));

		dialog->registerWidgetHelp(m_ui.bilinearFiltering, tr("Bilinear Filtering"), tr("Bilinear (Smooth)"),
			tr("Enables bilinear post processing filter. Smooths the overall picture as it is displayed on the screen. Corrects "
			   "positioning between pixels."));

		dialog->registerWidgetHelp(m_ui.PCRTCOffsets, tr("Screen Offsets"), tr("Unchecked"),
			//: PCRTC: Programmable CRT (Cathode Ray Tube) Controller.
			tr("Enables PCRTC Offsets which position the screen as the game requests. Useful for some games such as WipEout Fusion for its "
			   "screen shake effect, but can make the picture blurry."));

		dialog->registerWidgetHelp(m_ui.PCRTCOverscan, tr("Show Overscan"), tr("Unchecked"),
			tr("Enables the option to show the overscan area on games which draw more than the safe area of the screen."));

		dialog->registerWidgetHelp(
			m_ui.fmvAspectRatio, tr("FMV Aspect Ratio"), tr("Off (Default)"), tr("Overrides the full-motion video (FMV) aspect ratio."));

		dialog->registerWidgetHelp(m_ui.PCRTCAntiBlur, tr("Anti-Blur"), tr("Checked"),
			tr("Enables internal Anti-Blur hacks. Less accurate to PS2 rendering but will make a lot of games look less blurry."));

		dialog->registerWidgetHelp(m_ui.integerScaling, tr("Integer Scaling"), tr("Unchecked"),
			tr("Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an "
			   "integer number. May result in a sharper image in some 2D games."));

		dialog->registerWidgetHelp(m_ui.aspectRatio, tr("Aspect Ratio"), tr("Auto Standard (4:3/3:2 Progressive)"),
			tr("Changes the aspect ratio used to display the console's output to the screen. The default is Auto Standard (4:3/3:2 "
			   "Progressive) which automatically adjusts the aspect ratio to match how a game would be shown on a typical TV of the era."));

		dialog->registerWidgetHelp(m_ui.interlacing, tr("Deinterlacing"), tr("Automatic (Default)"), tr("Determines the deinterlacing method to be used on the interlaced screen of the emulated console. Automatic should be able to correctly deinterlace most games, but if you see visibly shaky graphics, try one of the available options."));

		dialog->registerWidgetHelp(m_ui.screenshotSize, tr("Screenshot Size"), tr("Screen Resolution"),
			tr("Determines the resolution at which screenshots will be saved. Internal resolutions preserve more detail at the cost of "
			   "file size."));

		dialog->registerWidgetHelp(m_ui.screenshotFormat, tr("Screenshot Format"), tr("PNG"),
			tr("Selects the format which will be used to save screenshots. JPEG produces smaller files, but loses detail."));

		dialog->registerWidgetHelp(m_ui.screenshotQuality, tr("Screenshot Quality"), tr("50%"),
			tr("Selects the quality at which screenshots will be compressed. Higher values preserve more detail for JPEG, and reduce file "
			   "size for PNG."));

		dialog->registerWidgetHelp(m_ui.stretchY, tr("Vertical Stretch"), tr("100%"),
			// Characters </> need to be converted into entities in order to be shown correctly.
			tr("Stretches (&lt; 100%) or squashes (&gt; 100%) the vertical component of the display."));

		dialog->registerWidgetHelp(m_ui.fullscreenModes, tr("Fullscreen Mode"), tr("Borderless Fullscreen"),
			tr("Chooses the fullscreen resolution and frequency."));

		dialog->registerWidgetHelp(
			m_ui.cropLeft, tr("Left"), tr("0px"), tr("Changes the number of pixels cropped from the left side of the display."));

		dialog->registerWidgetHelp(
			m_ui.cropTop, tr("Top"), tr("0px"), tr("Changes the number of pixels cropped from the top of the display."));

		dialog->registerWidgetHelp(
			m_ui.cropRight, tr("Right"), tr("0px"), tr("Changes the number of pixels cropped from the right side of the display."));

		dialog->registerWidgetHelp(
			m_ui.cropBottom, tr("Bottom"), tr("0px"), tr("Changes the number of pixels cropped from the bottom of the display."));
	}

	// Rendering tab
	{
		// Hardware
		dialog->registerWidgetHelp(m_ui.upscaleMultiplier, tr("Internal Resolution"), tr("Native (PS2) (Default)"),
			tr("Control the resolution at which games are rendered. High resolutions can impact performance on "
			   "older or lower-end GPUs.<br>Non-native resolution may cause minor graphical issues in some games.<br>"
			   "FMV resolution will remain unchanged, as the video files are pre-rendered."));

		dialog->registerWidgetHelp(
			m_ui.mipmapping, tr("Mipmapping"), tr("Checked"), tr("Enables mipmapping, which some games require to render correctly."));

		dialog->registerWidgetHelp(
			m_ui.textureFiltering, tr("Texture Filtering"), tr("Bilinear (PS2)"), tr("Control the texture filtering of the emulation."));

		dialog->registerWidgetHelp(m_ui.trilinearFiltering, tr("Trilinear Filtering"), tr("Automatic (Default)"),
			tr("Control the texture's trilinear filtering of the emulation."));

		dialog->registerWidgetHelp(m_ui.anisotropicFiltering, tr("Anisotropic Filtering"), tr("Off (Default)"),
			tr("Reduces texture aliasing at extreme viewing angles."));

		dialog->registerWidgetHelp(m_ui.dithering, tr("Dithering"), tr("Unscaled (Default)"),
			tr("Reduces banding between colors and improves the perceived color depth.<br> "
			   "Off: Disables any dithering.<br> "
			   "Scaled: Upscaling-aware / Highest dithering effect.<br> "
			   "Unscaled: Native Dithering / Lowest dithering effect does not increase size of squares when upscaling.<br> "
			   "Force 32bit: Treat all draws as if they were 32bit to avoid banding and dithering."));

		dialog->registerWidgetHelp(m_ui.blending, tr("Blending Accuracy"), tr("Basic (Recommended)"),
			tr("Control the accuracy level of the GS blending unit emulation.<br> "
			   "The higher the setting, the more blending is emulated in the shader accurately, and the higher the speed penalty will "
			   "be.<br> "
			   "Do note that Direct3D's blending is reduced in capability compared to OpenGL/Vulkan."));

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
			   "May improve performance during readbacks but with a significant increase in power usage."));

		dialog->registerWidgetHelp(m_ui.spinGPUDuringReadbacks, tr("Spin GPU During Readbacks"), tr("Unchecked"),
			tr("Submits useless work to the GPU during readbacks to prevent it from going into powersave modes. "
			   "May improve performance during readbacks but with a significant increase in power usage."));

		// Software
		dialog->registerWidgetHelp(m_ui.extraSWThreads, tr("Software Rendering Threads"), tr("2 threads"),
			tr("Number of rendering threads: 0 for single thread, 2 or more for multithread (1 is for debugging). "
			   "2 to 4 threads is recommended, any more than that is likely to be slower instead of faster."));

		dialog->registerWidgetHelp(m_ui.swAutoFlush, tr("Auto Flush"), tr("Checked"),
			tr("Force a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog->registerWidgetHelp(
			m_ui.swMipmap, tr("Mipmapping"), tr("Checked"), tr("Enables mipmapping, which some games require to render correctly."));
	}

	// Hardware Fixes tab
	{
		dialog->registerWidgetHelp(m_ui.cpuSpriteRenderBW, tr("CPU Sprite Render Size"), tr("0 (Disabled)"), 
			tr("The maximum target memory width that will allow the CPU Sprite Renderer to activate on."));

		dialog->registerWidgetHelp(m_ui.cpuCLUTRender, tr("Software CLUT Render"), tr("0 (Disabled)"), 
			tr("Tries to detect when a game is drawing its own color palette and then renders it in software, instead of on the GPU."));

		dialog->registerWidgetHelp(m_ui.gpuTargetCLUTMode, tr("GPU Target CLUT"), tr("Disabled"), 
			tr("Try to detect when a game is drawing its own color palette and then renders it on the GPU with special handling."));

		dialog->registerWidgetHelp(m_ui.skipDrawStart, tr("Skipdraw Range Start"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog->registerWidgetHelp(m_ui.skipDrawEnd, tr("Skipdraw Range End"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog->registerWidgetHelp(m_ui.hwAutoFlush, tr("Auto Flush"), tr("Unchecked"),
			tr("Force a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog->registerWidgetHelp(m_ui.disableDepthEmulation, tr("Disable Depth Conversion"), tr("Unchecked"),
			tr("Disable the support of depth buffers in the texture cache. "
			   "Will likely create various glitches and is only useful for debugging."));

		dialog->registerWidgetHelp(m_ui.disableSafeFeatures, tr("Disable Safe Features"), tr("Unchecked"),
			tr("This option disables multiple safe features. "
			   "Disables accurate Unscale Point and Line rendering which can help Xenosaga games. "
			   "Disables accurate GS Memory Clearing to be done on the CPU, and let the GPU handle it, which can help Kingdom Hearts "
			   "games."));

		dialog->registerWidgetHelp(
			m_ui.disableRenderFixes, tr("Disable Render Fixes"), tr("Unchecked"), tr("This option disables game-specific render fixes."));

		dialog->registerWidgetHelp(m_ui.disablePartialInvalidation, tr("Disable Partial Source Invalidation"), tr("Unchecked"),
			tr("By default, the texture cache handles partial invalidations. Unfortunately it is very costly to compute CPU wise. "
			   "This hack replaces the partial invalidation with a complete deletion of the texture to reduce the CPU load. "
			   "It helps with the Snowblind engine games."));
		dialog->registerWidgetHelp(m_ui.frameBufferConversion, tr("Framebuffer Conversion"), tr("Unchecked"),
			tr("Convert 4-bit and 8-bit framebuffer on the CPU instead of the GPU. "
			   "Helps Harry Potter and Stuntman games. It has a big impact on performance."));

		dialog->registerWidgetHelp(m_ui.preloadFrameData, tr("Preload Frame Data"), tr("Unchecked"),
			tr("Uploads GS data when rendering a new frame to reproduce some effects accurately."));

		dialog->registerWidgetHelp(m_ui.textureInsideRt, tr("Texture Inside RT"), tr("Disabled"),
			tr("Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer."));

		dialog->registerWidgetHelp(m_ui.readTCOnClose, tr("Read Targets When Closing"), tr("Unchecked"),
			tr("Flushes all targets in the texture cache back to local memory when shutting down. Can prevent lost visuals when saving "
			   "state or switching renderers, but can also cause graphical corruption."));

		dialog->registerWidgetHelp(m_ui.estimateTextureRegion, tr("Estimate Texture Region"), tr("Unchecked"),
			tr("Attempts to reduce the texture size when games do not set it themselves (e.g. Snowblind games)."));
	}

	// Upscaling Fixes tab
	{
		dialog->registerWidgetHelp(m_ui.halfPixelOffset, tr("Half Pixel Offset"), tr("Off (Default)"),
			tr("Might fix some misaligned fog, bloom, or blend effect."));

		dialog->registerWidgetHelp(m_ui.roundSprite, tr("Round Sprite"), tr("Off (Default)"),
			tr("Corrects the sampling of 2D sprite textures when upscaling. "
			   "Fixes lines in sprites of games like Ar tonelico when upscaling. Half option is for flat sprites, Full is for all "
			   "sprites."));

		dialog->registerWidgetHelp(m_ui.textureOffsetX, tr("Texture Offsets X"), tr("0"),
			//: ST and UV are different types of texture coordinates, like XY would be spatial coordinates.
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment "
			   "too."));

		dialog->registerWidgetHelp(m_ui.textureOffsetY, tr("Texture Offsets Y"), tr("0"),
			//: ST and UV are different types of texture coordinates, like XY would be spatial coordinates.
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment "
			   "too."));

		dialog->registerWidgetHelp(m_ui.alignSprite, tr("Align Sprite"), tr("Unchecked"),
			//: Namco: a game publisher and development company. Leave the name as-is. Ace Combat, Tekken, Soul Calibur: game names. Leave as-is or use official translations.
			tr("Fixes issues with upscaling (vertical lines) in Namco games like Ace Combat, Tekken, Soul Calibur, etc."));

		dialog->registerWidgetHelp(m_ui.forceEvenSpritePosition, tr("Force Even Sprite Position"), tr("Unchecked"),
			//: Wild Arms: name of a game series. Leave as-is or use an official translation.
			tr("Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games."));

		dialog->registerWidgetHelp(m_ui.bilinearHack, tr("Bilinear Upscale"), tr("Unchecked"),
			tr("Can smooth out textures due to be bilinear filtered when upscaling. E.g. Brave sun glare."));

		dialog->registerWidgetHelp(m_ui.mergeSprite, tr("Merge Sprite"), tr("Unchecked"),
			tr("Replaces post-processing multiple paving sprites by a single fat sprite. It reduces various upscaling lines."));

		dialog->registerWidgetHelp(m_ui.nativePaletteDraw, tr("Unscaled Palette Texture Draws"), tr("Unchecked"),
			tr("Force palette texture draws to render at native resolution."));
	}

	// Texture Replacement tab
	{
		dialog->registerWidgetHelp(m_ui.dumpReplaceableTextures, tr("Dump Textures"), tr("Unchecked"), tr("Dumps replaceable textures to disk. Will reduce performance."));

		dialog->registerWidgetHelp(m_ui.dumpReplaceableMipmaps, tr("Dump Mipmaps"), tr("Unchecked"), tr("Includes mipmaps when dumping textures."));

		dialog->registerWidgetHelp(m_ui.dumpTexturesWithFMVActive, tr("Dump FMV Textures"), tr("Unchecked"), tr("Allows texture dumping when FMVs are active. You should not enable this."));

		dialog->registerWidgetHelp(m_ui.loadTextureReplacementsAsync, tr("Asynchronous Texture Loading"), tr("Checked"), tr("Loads replacement textures on a worker thread, reducing microstutter when replacements are enabled."));

		dialog->registerWidgetHelp(m_ui.loadTextureReplacements, tr("Load Textures"), tr("Unchecked"), tr("Loads replacement textures where available and user-provided."));

		dialog->registerWidgetHelp(m_ui.precacheTextureReplacements, tr("Precache Textures"), tr("Unchecked"), tr("Preloads all replacement textures to memory. Not necessary with asynchronous loading."));
	}

	// Post Processing tab
	{
		//: You might find an official translation for this on AMD's website (Spanish version linked): https://www.amd.com/es/technologies/radeon-software-fidelityfx
		dialog->registerWidgetHelp(m_ui.casMode, tr("Contrast Adaptive Sharpening"), tr("None (Default)"), tr("Enables FidelityFX Contrast Adaptive Sharpening."));

		dialog->registerWidgetHelp(m_ui.casSharpness, tr("Sharpness"), tr("50%"), tr("Determines the intensity the sharpening effect in CAS post-processing."));

		dialog->registerWidgetHelp(m_ui.shadeBoost, tr("Shade Boost"), tr("Unchecked"),
			tr("Enables saturation, contrast, and brightness to be adjusted. Values of brightness, saturation, and contrast are at default "
			   "50."));

		dialog->registerWidgetHelp(
			m_ui.fxaa, tr("FXAA"), tr("Unchecked"), tr("Applies the FXAA anti-aliasing algorithm to improve the visual quality of games."));

		dialog->registerWidgetHelp(m_ui.shadeBoostBrightness, tr("Brightness"), tr("50"), tr("Adjusts brightness. 50 is normal."));

		dialog->registerWidgetHelp(m_ui.shadeBoostContrast, tr("Contrast"), tr("50"), tr("Adjusts contrast. 50 is normal."));

		dialog->registerWidgetHelp(m_ui.shadeBoostSaturation, tr("Saturation"), tr("50"), tr("Adjusts saturation. 50 is normal."));

		dialog->registerWidgetHelp(m_ui.tvShader, tr("TV Shader"), tr("None (Default)"),
			tr("Applies a shader which replicates the visual effects of different styles of television set."));
	}

	// OSD tab
	{
		dialog->registerWidgetHelp(m_ui.osdScale, tr("OSD Scale"), tr("100%"), tr("Scales the size of the onscreen OSD from 50% to 500%."));

		dialog->registerWidgetHelp(m_ui.osdShowMessages, tr("Show OSD Messages"), tr("Checked"),
			tr("Shows on-screen-display messages when events occur such as save states being "
			   "created/loaded, screenshots being taken, etc."));

		dialog->registerWidgetHelp(m_ui.osdShowFPS, tr("Show FPS"), tr("Unchecked"),
			tr("Shows the internal frame rate of the game in the top-right corner of the display."));

		dialog->registerWidgetHelp(m_ui.osdShowSpeed, tr("Show Speed Percentages"), tr("Unchecked"),
			tr("Shows the current emulation speed of the system in the top-right corner of the display as a percentage."));

		dialog->registerWidgetHelp(m_ui.osdShowResolution, tr("Show Resolution"), tr("Unchecked"),
			tr("Shows the resolution of the game in the top-right corner of the display."));

		dialog->registerWidgetHelp(m_ui.osdShowCPU, tr("Show CPU Usage"), tr("Unchecked"), tr("Shows host's CPU utilization."));

		dialog->registerWidgetHelp(m_ui.osdShowGPU, tr("Show GPU Usage"), tr("Unchecked"), tr("Shows host's GPU utilization."));

		dialog->registerWidgetHelp(m_ui.osdShowGSStats, tr("Show Statistics"), tr("Unchecked"),
			tr("Shows counters for internal graphical utilization, useful for debugging."));

		dialog->registerWidgetHelp(m_ui.osdShowIndicators, tr("Show Indicators"), tr("Checked"),
			tr("Shows OSD icon indicators for emulation states such as Pausing, Turbo, Fast-Forward, and Slow-Motion."));

		dialog->registerWidgetHelp(m_ui.osdShowSettings, tr("Show Settings"), tr("Unchecked"),
			tr("Displays various settings and the current values of those settings, useful for debugging."));

		dialog->registerWidgetHelp(m_ui.osdShowInputs, tr("Show Inputs"), tr("Unchecked"),
			tr("Shows the current controller state of the system in the bottom-left corner of the display."));

		dialog->registerWidgetHelp(m_ui.osdShowFrameTimes, tr("Show Frame Times"), tr("Unchecked"), 
			tr("Displays a graph showing the average frametimes."));
		
		dialog->registerWidgetHelp(m_ui.osdShowVersion, tr("Show PCSX2 Version"), tr("Unchecked"),
			tr("Shows the current PCSX2 version on the top-right corner of the display"));

		dialog->registerWidgetHelp(m_ui.warnAboutUnsafeSettings, tr("Warn About Unsafe Settings"), tr("Checked"),
			tr("Displays warnings when settings are enabled which may break games."));
	}

	// Recording tab
	{
		dialog->registerWidgetHelp(m_ui.videoCaptureCodec, tr("Video Codec"), tr("Default"), tr("Selects which Video Codec to be used for Video Capture. "
		
		"<b>If unsure, leave it on default.<b>"));

		dialog->registerWidgetHelp(m_ui.videoCaptureFormat, tr("Video Format"), tr("Default"), tr("Selects which Video Format to be used for Video Capture. If by chance the codec does not support the format, the first format available will be used. "
		
		"<b>If unsure, leave it on default.<b>"));

		dialog->registerWidgetHelp(m_ui.videoCaptureBitrate, tr("Video Bitrate"), tr("6000 kbps"), tr("Sets the video bitrate to be used. "
		
		"Larger bitrate generally yields better video quality at the cost of larger resulting file size."));

		dialog->registerWidgetHelp(m_ui.videoCaptureResolutionAuto, tr("Automatic Resolution"), tr("Unchecked"), tr("When checked, the video capture resolution will follows the internal resolution of the running game.<br><br>"
		
		"<b>Be careful when using this setting especially when you are upscaling, as higher internal resolution (above 4x) can results in very large video capture and can cause system overload.</b>"));


		dialog->registerWidgetHelp(m_ui.enableVideoCaptureArguments, tr("Enable Extra Video Arguments"), tr("Unchecked"), tr("Allows you to pass arguments to the selected video codec."));

		dialog->registerWidgetHelp(m_ui.videoCaptureArguments, tr("Extra Video Arguments"), tr("Leave It Blank"),
			tr("Parameters passed to the selected video codec.<br>"
			   "<b>You must use '=' to separate key from value and ':' to separate two pairs from each other.</b><br>"
			   "For example: \"crf = 21 : preset = veryfast\""));

		dialog->registerWidgetHelp(m_ui.audioCaptureCodec, tr("Audio Codec"), tr("Default"), tr("Selects which Audio Codec to be used for Video Capture. "
		
		"<b>If unsure, leave it on default.<b>"));

		dialog->registerWidgetHelp(m_ui.audioCaptureBitrate, tr("Audio Bitrate"), tr("160 kbps"), tr("Sets the audio bitrate to be used."));

		dialog->registerWidgetHelp(m_ui.enableAudioCaptureArguments, tr("Enable Extra Audio Arguments"), tr("Unchecked"), tr("Allows you to pass arguments to the selected audio codec."));

		dialog->registerWidgetHelp(m_ui.audioCaptureArguments, tr("Extra Audio Arguments"), tr("Leave It Blank"),
			tr("Parameters passed to the selected audio codec.<br>"
			   "<b>You must use '=' to separate key from value and ':' to separate two pairs from each other.</b><br>"
			   "For example: \"compression_level = 4 : joint_stereo = 1\""));
	}

	// Advanced tab
	{
		dialog->registerWidgetHelp(m_ui.gsDumpCompression, tr("GS Dump Compression"), tr("Zstandard (zst)"),
			tr("Change the compression algorithm used when creating a GS dump."));

		//: Blit = a data operation. You might want to write it as-is, but fully uppercased. More information: https://en.wikipedia.org/wiki/Bit_blit \nSwap chain: see Microsoft's Terminology Portal.
		dialog->registerWidgetHelp(m_ui.useBlitSwapChain, tr("Use Blit Swap Chain"), tr("Unchecked"),
			//: Blit = a data operation. You might want to write it as-is, but fully uppercased. More information: https://en.wikipedia.org/wiki/Bit_blit
			tr("Uses a blit presentation model instead of flipping when using the Direct3D 11 "
			   "renderer. This usually results in slower performance, but may be required for some "
			   "streaming applications, or to uncap framerates on some systems."));

		dialog->registerWidgetHelp(m_ui.exclusiveFullscreenControl, tr("Allow Exclusive Fullscreen"), tr("Automatic (Default)"),
			tr("Overrides the driver's heuristics for enabling exclusive fullscreen, or direct flip/scanout.<br>"
			   "Disallowing exclusive fullscreen may enable smoother task switching and overlays, but increase input latency."));

		dialog->registerWidgetHelp(m_ui.skipPresentingDuplicateFrames, tr("Skip Presenting Duplicate Frames"), tr("Unchecked"),
			tr("Detects when idle frames are being presented in 25/30fps games, and skips presenting those frames. The frame is still "
			   "rendered, it just means the GPU has more time to complete it (this is NOT frame skipping). Can smooth out frame time "
			   "fluctuations when the CPU/GPU are near maximum utilization, but makes frame pacing more inconsistent and can increase "
			   "input lag."));

		dialog->registerWidgetHelp(m_ui.disableMailboxPresentation, tr("Disable Mailbox Presentation"), tr("Unchecked"),
			tr("Forces the use of FIFO over Mailbox presentation, i.e. double buffering instead of triple buffering. "
			   "Usually results in worse frame pacing."));

		dialog->registerWidgetHelp(m_ui.extendedUpscales, tr("Extended Upscaling Multipliers"), tr("Unchecked"),
			tr("Displays additional, very high upscaling multipliers dependent on GPU capability."));

		dialog->registerWidgetHelp(m_ui.useDebugDevice, tr("Enable Debug Device"), tr("Unchecked"),
			tr("Enables API-level validation of graphics commands."));

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
		m_dialog->setStringSettingValue("EmuCore/GS", "Adapter", m_ui.adapterDropdown->currentText().toUtf8().constData());
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

void GraphicsSettingsWidget::onTrilinearFilteringChanged()
{
	const bool forced_bilinear = (m_dialog->getEffectiveIntValue("EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic)) >=
								  static_cast<int>(TriFiltering::Forced));
	m_ui.textureFiltering->setDisabled(forced_bilinear);
}

void GraphicsSettingsWidget::onShadeBoostChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "ShadeBoost", false);
	m_ui.shadeBoostBrightness->setEnabled(enabled);
	m_ui.shadeBoostContrast->setEnabled(enabled);
	m_ui.shadeBoostSaturation->setEnabled(enabled);
}

void GraphicsSettingsWidget::onTextureDumpChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "DumpReplaceableTextures", false);
	m_ui.dumpReplaceableMipmaps->setEnabled(enabled);
	m_ui.dumpTexturesWithFMVActive->setEnabled(enabled);
}

void GraphicsSettingsWidget::onTextureReplacementChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "LoadTextureReplacements", false);
	m_ui.loadTextureReplacementsAsync->setEnabled(enabled);
	m_ui.precacheTextureReplacements->setEnabled(enabled);
}


void GraphicsSettingsWidget::onCaptureContainerChanged()
{
	const std::string container(
		m_dialog->getEffectiveStringValue("EmuCore/GS", "CaptureContainer", Pcsx2Config::GSOptions::DEFAULT_CAPTURE_CONTAINER));

	m_ui.videoCaptureCodec->disconnect();
	m_ui.videoCaptureCodec->clear();
	//: This string refers to a default codec, whether it's an audio codec or a video codec.
	m_ui.videoCaptureCodec->addItem(tr("Default"), QString());
	for (const auto& [format, name] : GSCapture::GetVideoCodecList(container.c_str()))
	{
		const QString qformat(QString::fromStdString(format));
		const QString qname(QString::fromStdString(name));
		m_ui.videoCaptureCodec->addItem(QStringLiteral("%1 [%2]").arg(qformat).arg(qname), qformat);
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		m_dialog->getSettingsInterface(), m_ui.videoCaptureCodec, "EmuCore/GS", "VideoCaptureCodec");
	connect(m_ui.videoCaptureCodec, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onCaptureCodecChanged);

	m_ui.audioCaptureCodec->disconnect();
	m_ui.audioCaptureCodec->clear();
	m_ui.audioCaptureCodec->addItem(tr("Default"), QString());
	for (const auto& [format, name] : GSCapture::GetAudioCodecList(container.c_str()))
	{
		const QString qformat(QString::fromStdString(format));
		const QString qname(QString::fromStdString(name));
		m_ui.audioCaptureCodec->addItem(QStringLiteral("%1 [%2]").arg(qformat).arg(qname), qformat);
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		m_dialog->getSettingsInterface(), m_ui.audioCaptureCodec, "EmuCore/GS", "AudioCaptureCodec");
}

void GraphicsSettingsWidget::GraphicsSettingsWidget::onCaptureCodecChanged()
{
	m_ui.videoCaptureFormat->disconnect();
	m_ui.videoCaptureFormat->clear();
	//: This string refers to a default pixel format
	m_ui.videoCaptureFormat->addItem(tr("Default"), "");

	const std::string codec(
		m_dialog->getEffectiveStringValue("EmuCore/GS", "VideoCaptureCodec", ""));

	if (!codec.empty())
	{
		for (const auto& [id, name] : GSCapture::GetVideoFormatList(codec.c_str()))
		{
			const QString qid(QString::number(id));
			const QString qname(QString::fromStdString(name));
			m_ui.videoCaptureFormat->addItem(qname, qid);
		}
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		m_dialog->getSettingsInterface(), m_ui.videoCaptureFormat, "EmuCore/GS", "VideoCaptureFormat");
}

void GraphicsSettingsWidget::onEnableVideoCaptureChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "EnableVideoCapture", true);
	m_ui.videoCaptureOptions->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableVideoCaptureArgumentsChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "EnableVideoCaptureParameters", false);
	m_ui.videoCaptureArguments->setEnabled(enabled);
}

void GraphicsSettingsWidget::onVideoCaptureAutoResolutionChanged()
{
	const bool enabled = !m_dialog->getEffectiveBoolValue("EmuCore/GS", "VideoCaptureAutoResolution", true);
	m_ui.videoCaptureWidth->setEnabled(enabled);
	m_ui.videoCaptureHeight->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableAudioCaptureChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "EnableAudioCapture", true);
	m_ui.audioCaptureOptions->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableAudioCaptureArgumentsChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "EnableAudioCaptureParameters", false);
	m_ui.audioCaptureArguments->setEnabled(enabled);
}

void GraphicsSettingsWidget::onGpuPaletteConversionChanged(int state)
{
	const bool disabled =
		state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("EmuCore/GS", "paltex", false) : (state != 0);

	m_ui.anisotropicFiltering->setDisabled(disabled);
}

void GraphicsSettingsWidget::onCPUSpriteRenderBWChanged()
{
	const int value = m_dialog->getEffectiveIntValue("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
	m_ui.cpuSpriteRenderLevel->setEnabled(value != 0);
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

	const bool is_hardware = (type == GSRendererType::DX11 || type == GSRendererType::DX12 || type == GSRendererType::OGL ||
							  type == GSRendererType::VK || type == GSRendererType::Metal);
	const bool is_software = (type == GSRendererType::SW);
	const bool is_auto = (type == GSRendererType::Auto);
	const bool is_vk = (type == GSRendererType::VK);
	const bool is_disable_barriers = (type == GSRendererType::DX11 || type == GSRendererType::DX12 || type == GSRendererType::Metal || type == GSRendererType::SW);
	const bool hw_fixes = (is_hardware && m_ui.enableHWFixes && m_ui.enableHWFixes->checkState() == Qt::Checked);
	const int prev_tab = m_ui.tabs->currentIndex();

	// hw rendering
	m_ui.tabs->setTabEnabled(1, is_hardware);
	m_ui.tabs->setTabVisible(1, is_hardware);

	// sw rendering
	m_ui.tabs->setTabEnabled(2, is_software);
	m_ui.tabs->setTabVisible(2, is_software);

	// hardware fixes
	m_ui.tabs->setTabEnabled(3, hw_fixes);
	m_ui.tabs->setTabVisible(3, hw_fixes);

	// upscaling fixes
	m_ui.tabs->setTabEnabled(4, hw_fixes);
	m_ui.tabs->setTabVisible(4, hw_fixes);

	// texture replacement
	m_ui.tabs->setTabEnabled(5, is_hardware);
	m_ui.tabs->setTabVisible(5, is_hardware);

	// move back to the renderer if we're on one of the now-hidden tabs
	if (is_software && (prev_tab == 1 || (prev_tab >= 2 && prev_tab <= 5)))
		m_ui.tabs->setCurrentIndex(2);
	else if (is_hardware && prev_tab == 2)
		m_ui.tabs->setCurrentIndex(1);

	if (m_ui.useBlitSwapChain)
		m_ui.useBlitSwapChain->setEnabled(is_dx11);

	if (m_ui.overrideTextureBarriers)
		m_ui.overrideTextureBarriers->setDisabled(is_disable_barriers);

	if (m_ui.disableFramebufferFetch)
		m_ui.disableFramebufferFetch->setDisabled(is_sw_dx);

	if (m_ui.exclusiveFullscreenControl)
		m_ui.exclusiveFullscreenControl->setEnabled(is_auto || is_vk);

	// populate adapters
	std::vector<GSAdapterInfo> adapters = GSGetAdapterInfo(type);
	const GSAdapterInfo* current_adapter_info = nullptr;
	
	// fill+select adapters
	{
		QSignalBlocker sb(m_ui.adapterDropdown);

		std::string current_adapter = Host::GetBaseStringSettingValue("EmuCore/GS", "Adapter", "");
		m_ui.adapterDropdown->clear();
		m_ui.adapterDropdown->setEnabled(!adapters.empty());
		m_ui.adapterDropdown->addItem(tr("(Default)"));
		m_ui.adapterDropdown->setCurrentIndex(0);

		if (m_dialog->isPerGameSettings())
		{
			m_ui.adapterDropdown->insertItem(
				0, tr("Use Global Setting [%1]").arg(current_adapter.empty() ? tr("(Default)") : QString::fromStdString(current_adapter)));
			if (!m_dialog->getSettingsInterface()->GetStringValue("EmuCore/GS", "Adapter", &current_adapter))
			{
				// clear the adapter so we don't set it to the global value
				current_adapter.clear();
				m_ui.adapterDropdown->setCurrentIndex(0);
			}
		}

		for (const GSAdapterInfo& adapter : adapters)
		{
			m_ui.adapterDropdown->addItem(QString::fromStdString(adapter.name));
			if (current_adapter == adapter.name)
			{
				m_ui.adapterDropdown->setCurrentIndex(m_ui.adapterDropdown->count() - 1);
				current_adapter_info = &adapter;
			}
		}

		current_adapter_info = (current_adapter_info || adapters.empty()) ? current_adapter_info : &adapters.front();
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
				0, tr("Use Global Setting [%1]").arg(current_mode.empty() ? tr("Borderless Fullscreen") : QString::fromStdString(current_mode)));
			if (!m_dialog->getSettingsInterface()->GetStringValue("EmuCore/GS", "FullscreenMode", &current_mode))
			{
				current_mode.clear();
				m_ui.fullscreenModes->setCurrentIndex(0);
			}
		}

		if (current_adapter_info)
		{
			for (const std::string& fs_mode : current_adapter_info->fullscreen_modes)
			{
				m_ui.fullscreenModes->addItem(QString::fromStdString(fs_mode));
				if (current_mode == fs_mode)
					m_ui.fullscreenModes->setCurrentIndex(m_ui.fullscreenModes->count() - 1);
			}
		}
	}

	// assume the GPU can do 10K textures.
	const u32 max_upscale_multiplier = std::max(current_adapter_info ? current_adapter_info->max_upscale_multiplier : 0u, 10u);
	populateUpscaleMultipliers(max_upscale_multiplier);
}

void GraphicsSettingsWidget::populateUpscaleMultipliers(u32 max_upscale_multiplier)
{
	static constexpr std::pair<const char*, float> templates[] = {
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Native (PS2) (Default)"), 1.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "1.25x Native (~450px)"), 1.25f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "1.5x Native (~540px)"), 1.5f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "1.75x Native (~630px)"), 1.75f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2x Native (~720px/HD)"), 2.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2.5x Native (~900px/HD+)"), 2.5f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "3x Native (~1080px/FHD)"), 3.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "3.5x Native (~1260px)"), 3.5f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "4x Native (~1440px/QHD)"), 4.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "5x Native (~1800px/QHD+)"), 5.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "6x Native (~2160px/4K UHD)"), 6.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "7x Native (~2520px)"), 7.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "8x Native (~2880px/5K UHD)"), 8.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "9x Native (~3240px)"), 9.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "10x Native (~3600px/6K UHD)"), 10.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "11x Native (~3960px)"), 11.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "12x Native (~4320px/8K UHD)"), 12.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "13x Native (~4680px)"), 13.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "14x Native (~5040px)"), 14.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "15x Native (~5400px)"), 15.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "16x Native (~5760px)"), 16.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "17x Native (~6120px)"), 17.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "18x Native (~6480px/12K UHD)"), 18.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "19x Native (~6840px)"), 19.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "20x Native (~7200px)"), 20.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "21x Native (~7560px)"), 21.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "22x Native (~7920px)"), 22.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "23x Native (~8280px)"), 23.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "24x Native (~8640px/16K UHD)"), 24.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "25x Native (~9000px)"), 25.0f},
	};
	static constexpr u32 max_template_multiplier = 25;

	// Limit the dropdown to 12x if we're not showing advanced settings. Save the noobs.
	static constexpr u32 max_non_advanced_multiplier = 12;

	QSignalBlocker sb(m_ui.upscaleMultiplier);
	m_ui.upscaleMultiplier->clear();

	const u32 max_shown_multiplier = m_ui.extendedUpscales && m_ui.extendedUpscales->checkState() == Qt::Checked ?
										 max_upscale_multiplier :
										 std::min(max_upscale_multiplier, max_non_advanced_multiplier);
	for (const auto& [name, value] : templates)
	{
		if (value > max_shown_multiplier)
			break;

		m_ui.upscaleMultiplier->addItem(tr(name), QVariant(value));
	}
	for (u32 i = max_template_multiplier + 1; i <= max_shown_multiplier; i++)
		m_ui.upscaleMultiplier->addItem(tr("%1x Native").arg(i), QVariant(static_cast<float>(i)));

	const float global_value = Host::GetBaseFloatSettingValue("EmuCore/GS", "upscale_multiplier", 1.0f);
	if (m_dialog->isPerGameSettings())
	{
		const int name_idx = m_ui.upscaleMultiplier->findData(QVariant(global_value));
		const QString global_name = (name_idx >= 0) ? m_ui.upscaleMultiplier->itemText(name_idx) : tr("%1x Native");
		m_ui.upscaleMultiplier->insertItem(0, tr("Use Global Setting [%1]").arg(global_name));

		const std::optional<float> config_value = m_dialog->getFloatValue("EmuCore/GS", "upscale_multiplier", std::nullopt);
		if (config_value.has_value())
		{
			if (int index = m_ui.upscaleMultiplier->findData(QVariant(config_value.value())); index > 0)
				m_ui.upscaleMultiplier->setCurrentIndex(index);
		}
		else
		{
			m_ui.upscaleMultiplier->setCurrentIndex(0);
		}		
	}
	else
	{
		if (int index = m_ui.upscaleMultiplier->findData(QVariant(global_value)); index > 0)
			m_ui.upscaleMultiplier->setCurrentIndex(index);
	}
}

void GraphicsSettingsWidget::onUpscaleMultiplierChanged()
{
	const QVariant data = m_ui.upscaleMultiplier->currentData();
	m_dialog->setFloatSettingValue("EmuCore/GS", "upscale_multiplier",
		data.isValid() ? std::optional<float>(data.toFloat()) : std::optional<float>());
}

void GraphicsSettingsWidget::resetManualHardwareFixes()
{
	bool changed = false;
	{
		auto lock = Host::GetSettingsLock();
		SettingsInterface* const si = Host::Internal::GetBaseSettingsLayer();

		auto check_bool = [&](const char* section, const char* key, bool expected) {
			if (si->GetBoolValue(section, key, expected) != expected)
			{
				si->SetBoolValue(section, key, expected);
				changed = true;
			}
		};
		auto check_int = [&](const char* section, const char* key, s32 expected) {
			if (si->GetIntValue(section, key, expected) != expected)
			{
				si->SetIntValue(section, key, expected);
				changed = true;
			}
		};

		check_bool("EmuCore/GS", "UserHacks", false);

		check_int("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
		check_int("EmuCore/GS", "UserHacks_CPUCLUTRender", 0);
		check_int("EmuCore/GS", "UserHacks_GPUTargetCLUTMode", 0);
		check_int("EmuCore/GS", "UserHacks_SkipDraw_Start", 0);
		check_int("EmuCore/GS", "UserHacks_SkipDraw_End", 0);
		check_bool("EmuCore/GS", "UserHacks_AutoFlush", false);
		check_bool("EmuCore/GS", "UserHacks_CPU_FB_Conversion", false);
		check_bool("EmuCore/GS", "UserHacks_DisableDepthSupport", false);
		check_bool("EmuCore/GS", "UserHacks_Disable_Safe_Features", false);
		check_bool("EmuCore/GS", "UserHacks_DisableRenderFixes", false);
		check_bool("EmuCore/GS", "preload_frame_with_gs_data", false);
		check_bool("EmuCore/GS", "UserHacks_DisablePartialInvalidation", false);
		check_int("EmuCore/GS", "UserHacks_TextureInsideRt", static_cast<int>(GSTextureInRtMode::Disabled));
		check_bool("EmuCore/GS", "UserHacks_ReadTCOnClose", false);
		check_bool("EmuCore/GS", "UserHacks_EstimateTextureRegion", false);
		check_bool("EmuCore/GS", "paltex", false);
		check_int("EmuCore/GS", "UserHacks_HalfPixelOffset", 0);
		check_int("EmuCore/GS", "UserHacks_native_scaling", static_cast<int>(GSNativeScaling::Off));
		check_int("EmuCore/GS", "UserHacks_round_sprite_offset", 0);
		check_int("EmuCore/GS", "UserHacks_TCOffsetX", 0);
		check_int("EmuCore/GS", "UserHacks_TCOffsetY", 0);
		check_bool("EmuCore/GS", "UserHacks_align_sprite_X", false);
		check_bool("EmuCore/GS", "UserHacks_merge_pp_sprite", false);
		check_bool("EmuCore/GS", "UserHacks_ForceEvenSpritePosition", false);
		check_bool("EmuCore/GS", "UserHacks_BilinearHack", false);
	}

	if (changed)
		Host::CommitBaseSettingChanges();
}
