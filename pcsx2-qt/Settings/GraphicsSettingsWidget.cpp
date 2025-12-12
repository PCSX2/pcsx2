// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GraphicsSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"
#include <QtWidgets/QMessageBox>

#include "pcsx2/Host.h"
#include "pcsx2/Patch.h"
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
	{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "Software Renderer"), GSRendererType::SW},
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

GraphicsSettingsWidget::GraphicsSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupHeader(m_header);
	m_display_tab = setupTab(m_display, tr("Display"));
	m_hardware_rendering_tab = setupTab(m_hw, tr("Rendering"));
	m_software_rendering_tab = setupTab(m_sw, tr("Rendering"));
	m_hardware_fixes_tab = setupTab(m_fixes, tr("Hardware Fixes"));
	m_upscaling_fixes_tab = setupTab(m_upscaling, tr("Upscaling Fixes"));
	m_texture_replacement_tab = setupTab(m_texture, tr("Texture Replacement"));
	setupTab(m_post, tr("Post-Processing"));
	setupTab(m_osd, tr("OSD"));
	setupTab(m_capture, tr("Media Capture"));
	m_advanced_tab = setupTab(m_advanced, tr("Advanced"));

	//////////////////////////////////////////////////////////////////////////
	// Display Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_display.aspectRatio, "EmuCore/GS", "AspectRatio", Pcsx2Config::GSOptions::AspectRatioNames, AspectRatioType::RAuto4_3_3_2);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_display.fmvAspectRatio, "EmuCore/GS", "FMVAspectRatioSwitch",
		Pcsx2Config::GSOptions::FMVAspectRatioSwitchNames, FMVAspectRatioSwitchType::Off);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_display.interlacing, "EmuCore/GS", "deinterlace_mode", DEFAULT_INTERLACE_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_display.bilinearFiltering, "EmuCore/GS", "linear_present_mode", static_cast<int>(GSPostBilinearMode::BilinearSmooth));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.widescreenPatches, "EmuCore", "EnableWideScreenPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.noInterlacingPatches, "EmuCore", "EnableNoInterlacingPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.integerScaling, "EmuCore/GS", "IntegerScaling", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.PCRTCOffsets, "EmuCore/GS", "pcrtc_offsets", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.PCRTCOverscan, "EmuCore/GS", "pcrtc_overscan", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.PCRTCAntiBlur, "EmuCore/GS", "pcrtc_antiblur", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_display.disableInterlaceOffset, "EmuCore/GS", "disable_interlace_offset", false);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_capture.screenshotSize, "EmuCore/GS", "ScreenshotSize", static_cast<int>(GSScreenshotSize::WindowResolution));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_capture.screenshotFormat, "EmuCore/GS", "ScreenshotFormat", static_cast<int>(GSScreenshotFormat::PNG));
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_display.stretchY, "EmuCore/GS", "StretchY", 100.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_display.cropLeft, "EmuCore/GS", "CropLeft", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_display.cropTop, "EmuCore/GS", "CropTop", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_display.cropRight, "EmuCore/GS", "CropRight", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_display.cropBottom, "EmuCore/GS", "CropBottom", 0);

	connect(
		m_display.fullscreenModes, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onFullscreenModeChanged);

	//////////////////////////////////////////////////////////////////////////
	// HW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_hw.textureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_hw.trilinearFiltering, "EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic), -1);
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_hw.anisotropicFiltering, "EmuCore/GS", "MaxAnisotropy",
		s_anisotropic_filtering_entries, s_anisotropic_filtering_values, "0");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_hw.dithering, "EmuCore/GS", "dithering_ps2", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_hw.mipmapping, "EmuCore/GS", "hw_mipmap", true);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_hw.blending, "EmuCore/GS", "accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_hw.enableHWFixes, "EmuCore/GS", "UserHacks", false);
	connect(m_hw.upscaleMultiplier, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onUpscaleMultiplierChanged);
	connect(m_hw.trilinearFiltering, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onTrilinearFilteringChanged);
	onTrilinearFilteringChanged();

	//////////////////////////////////////////////////////////////////////////
	// SW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_sw.swTextureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_sw.extraSWThreads, "EmuCore/GS", "extrathreads", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_sw.swAutoFlush, "EmuCore/GS", "autoflush_sw", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_sw.swMipmap, "EmuCore/GS", "mipmap", true);

	//////////////////////////////////////////////////////////////////////////
	// HW Renderer Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.cpuSpriteRenderBW, "EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.cpuSpriteRenderLevel, "EmuCore/GS", "UserHacks_CPUSpriteRenderLevel", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.cpuCLUTRender, "EmuCore/GS", "UserHacks_CPUCLUTRender", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.gpuTargetCLUTMode, "EmuCore/GS", "UserHacks_GPUTargetCLUTMode", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.skipDrawStart, "EmuCore/GS", "UserHacks_SkipDraw_Start", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.skipDrawEnd, "EmuCore/GS", "UserHacks_SkipDraw_End", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_fixes.hwAutoFlush, "EmuCore/GS", "UserHacks_AutoFlushLevel", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.frameBufferConversion, "EmuCore/GS", "UserHacks_CPU_FB_Conversion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.disableDepthEmulation, "EmuCore/GS", "UserHacks_DisableDepthSupport", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.disableSafeFeatures, "EmuCore/GS", "UserHacks_Disable_Safe_Features", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.disableRenderFixes, "EmuCore/GS", "UserHacks_DisableRenderFixes", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.preloadFrameData, "EmuCore/GS", "preload_frame_with_gs_data", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_fixes.disablePartialInvalidation, "EmuCore/GS", "UserHacks_DisablePartialInvalidation", false);
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_fixes.textureInsideRt, "EmuCore/GS", "UserHacks_TextureInsideRt", static_cast<int>(GSTextureInRtMode::Disabled));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.readTCOnClose, "EmuCore/GS", "UserHacks_ReadTCOnClose", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.estimateTextureRegion, "EmuCore/GS", "UserHacks_EstimateTextureRegion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_fixes.gpuPaletteConversion, "EmuCore/GS", "paltex", false);
	connect(m_fixes.cpuSpriteRenderBW, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&GraphicsSettingsWidget::onCPUSpriteRenderBWChanged);
	connect(m_fixes.gpuPaletteConversion, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onGpuPaletteConversionChanged);
	onCPUSpriteRenderBWChanged();
	onGpuPaletteConversionChanged(m_fixes.gpuPaletteConversion->checkState());

	//////////////////////////////////////////////////////////////////////////
	// HW Upscaling Fixes
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.halfPixelOffset, "EmuCore/GS", "UserHacks_HalfPixelOffset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.nativeScaling, "EmuCore/GS", "UserHacks_native_scaling", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.roundSprite, "EmuCore/GS", "UserHacks_round_sprite_offset", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.bilinearHack, "EmuCore/GS", "UserHacks_BilinearHack", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.textureOffsetX, "EmuCore/GS", "UserHacks_TCOffsetX", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_upscaling.textureOffsetY, "EmuCore/GS", "UserHacks_TCOffsetY", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_upscaling.alignSprite, "EmuCore/GS", "UserHacks_align_sprite_X", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_upscaling.mergeSprite, "EmuCore/GS", "UserHacks_merge_pp_sprite", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_upscaling.forceEvenSpritePosition, "EmuCore/GS", "UserHacks_forceEvenSpritePosition", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_upscaling.nativePaletteDraw, "EmuCore/GS", "UserHacks_NativePaletteDraw", false);

	//////////////////////////////////////////////////////////////////////////
	// Texture Replacements
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_texture.dumpReplaceableTextures, "EmuCore/GS", "DumpReplaceableTextures", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_texture.dumpReplaceableMipmaps, "EmuCore/GS", "DumpReplaceableMipmaps", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_texture.dumpTexturesWithFMVActive, "EmuCore/GS", "DumpTexturesWithFMVActive", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_texture.loadTextureReplacements, "EmuCore/GS", "LoadTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_texture.loadTextureReplacementsAsync, "EmuCore/GS", "LoadTextureReplacementsAsync", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_texture.precacheTextureReplacements, "EmuCore/GS", "PrecacheTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_texture.texturesDirectory, m_texture.texturesBrowse, m_texture.texturesOpen, m_texture.texturesReset,
		"Folders", "Textures", Path::Combine(EmuFolders::DataRoot, "textures"));
	connect(m_texture.dumpReplaceableTextures, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onTextureDumpChanged);
	connect(m_texture.loadTextureReplacements, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onTextureReplacementChanged);
	onTextureDumpChanged();
	onTextureReplacementChanged();

	if (dialog()->isPerGameSettings())
	{
		m_texture.verticalLayout->removeWidget(m_texture.texturesDirectoryBox);
		m_texture.texturesDirectoryBox->deleteLater();
		m_texture.texturesDirectoryBox = nullptr;
		m_texture.texturesDirectory = nullptr;
		m_texture.texturesBrowse = nullptr;
		m_texture.texturesOpen = nullptr;
		m_texture.texturesReset = nullptr;
		m_texture.textureDescriptionText = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	// Post-Processing Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_BRIGHTNESS);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_CONTRAST);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostGamma, "EmuCore/GS", "ShadeBoost_Gamma", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_GAMMA);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_SATURATION);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.tvShader, "EmuCore/GS", "TVShader", DEFAULT_TV_SHADER_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casMode, "EmuCore/GS", "CASMode", static_cast<int>(GSCASMode::Disabled));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casSharpness, "EmuCore/GS", "CASSharpness", DEFAULT_CAS_SHARPNESS);

	connect(m_post.shadeBoost, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();
	connect(m_osd.messagesPos, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onMessagesPosChanged);
	connect(m_osd.performancePos, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onPerformancePosChanged);
	onMessagesPosChanged();
	onPerformancePosChanged();

	//////////////////////////////////////////////////////////////////////////
	// OSD Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_osd.scale, "EmuCore/GS", "OsdScale", 100.0f);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_osd.messagesPos, "EmuCore/GS", "OsdMessagesPos", static_cast<int>(OsdOverlayPos::TopLeft));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_osd.performancePos, "EmuCore/GS", "OsdPerformancePos", static_cast<int>(OsdOverlayPos::TopRight));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showSpeedPercentages, "EmuCore/GS", "OsdShowSpeed", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showFPS, "EmuCore/GS", "OsdShowFPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showVPS, "EmuCore/GS", "OsdShowVPS", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showResolution, "EmuCore/GS", "OsdShowResolution", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showGSStats, "EmuCore/GS", "OsdShowGSStats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showUsageCPU, "EmuCore/GS", "OsdShowCPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showUsageGPU, "EmuCore/GS", "OsdShowGPU", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showStatusIndicators, "EmuCore/GS", "OsdShowIndicators", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showFrameTimes, "EmuCore/GS", "OsdShowFrameTimes", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showHardwareInfo, "EmuCore/GS", "OsdShowHardwareInfo", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showVersion, "EmuCore/GS", "OsdShowVersion", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showSettings, "EmuCore/GS", "OsdShowSettings", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showPatches, "EmuCore/GS", "OsdshowPatches", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showInputs, "EmuCore/GS", "OsdShowInputs", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showVideoCapture, "EmuCore/GS", "OsdShowVideoCapture", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showInputRec, "EmuCore/GS", "OsdShowInputRec", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.showTextureReplacements, "EmuCore/GS", "OsdShowTextureReplacements", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_osd.warnAboutUnsafeSettings, "EmuCore", "OsdWarnAboutUnsafeSettings", true);

	//////////////////////////////////////////////////////////////////////////
	// Advanced Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.useBlitSwapChain, "EmuCore/GS", "UseBlitSwapChain", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.useDebugDevice, "EmuCore/GS", "UseDebugDevice", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.disableMailboxPresentation, "EmuCore/GS", "DisableMailboxPresentation", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.extendedUpscales, "EmuCore/GS", "ExtendedUpscalingMultipliers", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_advanced.exclusiveFullscreenControl, "EmuCore/GS", "ExclusiveFullscreenControl", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_advanced.overrideTextureBarriers, "EmuCore/GS", "OverrideTextureBarriers", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_advanced.gsDumpCompression, "EmuCore/GS", "GSDumpCompression", static_cast<int>(GSDumpCompressionMethod::Zstandard));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.disableFramebufferFetch, "EmuCore/GS", "DisableFramebufferFetch", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.disableShaderCache, "EmuCore/GS", "DisableShaderCache", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.disableVertexShaderExpand, "EmuCore/GS", "DisableVertexShaderExpand", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_advanced.gsDownloadMode, "EmuCore/GS", "HWDownloadMode", static_cast<int>(GSHardwareDownloadMode::Enabled));
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_advanced.ntscFrameRate, "EmuCore/GS", "FrameRateNTSC", 59.94f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_advanced.palFrameRate, "EmuCore/GS", "FrameRatePAL", 50.00f);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.spinCPUDuringReadbacks, "EmuCore/GS", "HWSpinCPUForReadbacks", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_advanced.spinGPUDuringReadbacks, "EmuCore/GS", "HWSpinGPUForReadbacks", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_advanced.texturePreloading, "EmuCore/GS", "texture_preloading", static_cast<int>(TexturePreloadingLevel::Off));

	setTabVisible(m_advanced_tab, QtHost::ShouldShowAdvancedSettings());

	//////////////////////////////////////////////////////////////////////////
	// Non-trivial settings
	//////////////////////////////////////////////////////////////////////////
	const int renderer = dialog()->getEffectiveIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto));
	for (const RendererInfo& ri : s_renderer_info)
	{
		m_header.rendererDropdown->addItem(qApp->translate("GraphicsSettingsWidget", ri.name));
		if (renderer == static_cast<int>(ri.type))
			m_header.rendererDropdown->setCurrentIndex(m_header.rendererDropdown->count() - 1);
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
		m_header.rendererDropdown->insertItem(0, tr("Use Global Setting [%1]").arg(global_renderer_name));

		// Effective Index already selected, set to global if setting is not per-game
		int override_renderer;
		if (!sif->GetIntValue("EmuCore/GS", "Renderer", &override_renderer))
			m_header.rendererDropdown->setCurrentIndex(0);
	}

	connect(m_header.rendererDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onRendererChanged);
	connect(m_header.adapterDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GraphicsSettingsWidget::onAdapterChanged);
	connect(m_hw.enableHWFixes, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::updateRendererDependentOptions);
	connect(m_advanced.extendedUpscales, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::updateRendererDependentOptions);
	connect(m_hw.textureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onTextureFilteringChange);
	connect(m_sw.swTextureFiltering, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onSWTextureFilteringChange);
	updateRendererDependentOptions();

#ifndef _WIN32
	// Exclusive fullscreen control is Windows-only.
	m_advanced.advancedOptionsFormLayout->removeRow(2);
	m_advanced.exclusiveFullscreenControl = nullptr;
#endif

#ifndef PCSX2_DEVBUILD
	if (!dialog()->isPerGameSettings())
	{
		// Only allow disabling readbacks for per-game settings, it's too dangerous.
		m_advanced.advancedOptionsFormLayout->removeRow(0);
		m_advanced.gsDownloadMode = nullptr;

		// Don't allow setting hardware fixes globally.
		// Too many stupid YouTube "best settings" guides that break other games.
		m_hw.hardwareRenderingOptionsLayout->removeWidget(m_hw.enableHWFixes);
		delete m_hw.enableHWFixes;
		m_hw.enableHWFixes = nullptr;
	}
#endif

	// Get rid of widescreen/no-interlace checkboxes from per-game settings, and migrate them to Patches if necessary.
	if (dialog()->isPerGameSettings())
	{
		SettingsInterface* si = dialog()->getSettingsInterface();
		bool needs_save = false;

		if (si->ContainsValue("EmuCore", "EnableWideScreenPatches"))
		{
			const bool ws_enabled = si->GetBoolValue("EmuCore", "EnableWideScreenPatches");
			si->DeleteValue("EmuCore", "EnableWideScreenPatches");

			const char* WS_PATCH_NAME = "Widescreen 16:9";
			if (ws_enabled)
			{
				si->AddToStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, WS_PATCH_NAME);
				si->RemoveFromStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_DISABLE_CONFIG_KEY, WS_PATCH_NAME);
			}
			else
			{
				si->AddToStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_DISABLE_CONFIG_KEY, WS_PATCH_NAME);
				si->RemoveFromStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, WS_PATCH_NAME);
			}
			needs_save = true;
		}

		if (si->ContainsValue("EmuCore", "EnableNoInterlacingPatches"))
		{
			const bool ni_enabled = si->GetBoolValue("EmuCore", "EnableNoInterlacingPatches");
			si->DeleteValue("EmuCore", "EnableNoInterlacingPatches");

			const char* NI_PATCH_NAME = "No-Interlacing";
			if (ni_enabled)
			{
				si->AddToStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, NI_PATCH_NAME);
				si->RemoveFromStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_DISABLE_CONFIG_KEY, NI_PATCH_NAME);
			}
			else
			{
				si->AddToStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_DISABLE_CONFIG_KEY, NI_PATCH_NAME);
				si->RemoveFromStringList(Patch::PATCHES_CONFIG_SECTION, Patch::PATCH_ENABLE_CONFIG_KEY, NI_PATCH_NAME);
			}
			needs_save = true;
		}

		if (needs_save)
		{
			dialog()->saveAndReloadGameSettings();
		}

		m_display.displayGridLayout->removeWidget(m_display.widescreenPatches);
		m_display.displayGridLayout->removeWidget(m_display.noInterlacingPatches);
		m_display.widescreenPatches->deleteLater();
		m_display.noInterlacingPatches->deleteLater();
		m_display.widescreenPatches = nullptr;
		m_display.noInterlacingPatches = nullptr;
	}

	// Capture settings
	{
		for (const char** container = Pcsx2Config::GSOptions::CaptureContainers; *container; container++)
		{
			const QString name(QString::fromUtf8(*container));
			m_capture.captureContainer->addItem(name.toUpper(), name);
		}

		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_capture.captureContainer, "EmuCore/GS", "CaptureContainer");
		connect(m_capture.captureContainer, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onCaptureContainerChanged);

		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_capture.enableVideoCapture, "EmuCore/GS", "EnableVideoCapture", true);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_capture.videoCaptureBitrate, "EmuCore/GS", "VideoCaptureBitrate", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_BITRATE);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_capture.videoCaptureWidth, "EmuCore/GS", "VideoCaptureWidth", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_WIDTH);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_capture.videoCaptureHeight, "EmuCore/GS", "VideoCaptureHeight", Pcsx2Config::GSOptions::DEFAULT_VIDEO_CAPTURE_HEIGHT);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_capture.videoCaptureResolutionAuto, "EmuCore/GS", "VideoCaptureAutoResolution", true);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_capture.enableVideoCaptureArguments, "EmuCore/GS", "EnableVideoCaptureParameters", false);
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_capture.videoCaptureArguments, "EmuCore/GS", "VideoCaptureParameters");
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_capture.screenshotQuality, "EmuCore/GS", "ScreenshotQuality", 90);
		connect(m_capture.enableVideoCapture, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onEnableVideoCaptureChanged);
		connect(
			m_capture.videoCaptureResolutionAuto, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onVideoCaptureAutoResolutionChanged);
		connect(m_capture.enableVideoCaptureArguments, &QCheckBox::checkStateChanged, this,
			&GraphicsSettingsWidget::onEnableVideoCaptureArgumentsChanged);

		SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_capture.enableAudioCapture, "EmuCore/GS", "EnableAudioCapture", true);
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_capture.audioCaptureBitrate, "EmuCore/GS", "AudioCaptureBitrate", Pcsx2Config::GSOptions::DEFAULT_AUDIO_CAPTURE_BITRATE);
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_capture.enableAudioCaptureArguments, "EmuCore/GS", "EnableAudioCaptureParameters", false);
		SettingWidgetBinder::BindWidgetToStringSetting(sif, m_capture.audioCaptureArguments, "EmuCore/GS", "AudioCaptureParameters");
		connect(m_capture.enableAudioCapture, &QCheckBox::checkStateChanged, this, &GraphicsSettingsWidget::onEnableAudioCaptureChanged);
		connect(m_capture.enableAudioCaptureArguments, &QCheckBox::checkStateChanged, this,
			&GraphicsSettingsWidget::onEnableAudioCaptureArgumentsChanged);

		onCaptureContainerChanged();
		onCaptureCodecChanged();
		onEnableVideoCaptureChanged();
		onEnableVideoCaptureArgumentsChanged();
		onVideoCaptureAutoResolutionChanged();
		onEnableAudioCaptureChanged();
		onEnableAudioCaptureArgumentsChanged();
	}

	// Display tab
	{
		dialog()->registerWidgetHelp(m_display.widescreenPatches, tr("Enable Widescreen Patches"), tr("Unchecked"),
			tr("Automatically loads and applies widescreen patches on game start. Can cause issues."));

		dialog()->registerWidgetHelp(m_display.noInterlacingPatches, tr("Enable No-Interlacing Patches"), tr("Unchecked"),
			tr("Automatically loads and applies no-interlacing patches on game start. Can cause issues."));

		dialog()->registerWidgetHelp(m_display.disableInterlaceOffset, tr("Disable Interlace Offset"), tr("Unchecked"),
			tr("Disables interlacing offset which may reduce blurring in some situations."));

		dialog()->registerWidgetHelp(m_display.bilinearFiltering, tr("Bilinear Filtering"), tr("Bilinear (Smooth)"),
			tr("Enables bilinear post processing filter. Smooths the overall picture as it is displayed on the screen. Corrects "
			   "positioning between pixels."));

		dialog()->registerWidgetHelp(m_display.PCRTCOffsets, tr("Screen Offsets"), tr("Unchecked"),
			//: PCRTC: Programmable CRT (Cathode Ray Tube) Controller.
			tr("Enables PCRTC Offsets which position the screen as the game requests. Useful for some games such as WipEout Fusion for its "
			   "screen shake effect, but can make the picture blurry."));

		dialog()->registerWidgetHelp(m_display.PCRTCOverscan, tr("Show Overscan"), tr("Unchecked"),
			tr("Enables the option to show the overscan area on games which draw more than the safe area of the screen."));

		dialog()->registerWidgetHelp(
			m_display.fmvAspectRatio, tr("FMV Aspect Ratio Override"), tr("Off (Default)"),
			tr("Overrides the full-motion video (FMV) aspect ratio. "
			   "If disabled, the FMV Aspect Ratio will match the same value as the general Aspect Ratio setting."));

		dialog()->registerWidgetHelp(m_display.PCRTCAntiBlur, tr("Anti-Blur"), tr("Checked"),
			tr("Enables internal Anti-Blur hacks. Less accurate than PS2 rendering but will make a lot of games look less blurry."));

		dialog()->registerWidgetHelp(m_display.integerScaling, tr("Integer Scaling"), tr("Unchecked"),
			tr("Adds padding to the display area to ensure that the ratio between pixels on the host to pixels in the console is an "
			   "integer number. May result in a sharper image in some 2D games."));

		dialog()->registerWidgetHelp(m_display.aspectRatio, tr("Aspect Ratio"), tr("Auto Standard (4:3/3:2 Progressive)"),
			tr("Changes the aspect ratio used to display the console's output to the screen. The default is Auto Standard (4:3/3:2 "
			   "Progressive) which automatically adjusts the aspect ratio to match how a game would be shown on a typical TV of the era, and adapts to widescreen/ultrawide game patches."));

		dialog()->registerWidgetHelp(m_display.interlacing, tr("Deinterlacing"), tr("Automatic (Default)"), tr("Determines the deinterlacing method to be used on the interlaced screen of the emulated console. Automatic should be able to correctly deinterlace most games, but if you see visibly shaky graphics, try one of the other options."));

		dialog()->registerWidgetHelp(m_capture.screenshotSize, tr("Screenshot Resolution"), tr("Display Resolution"),
			tr("Determines the resolution at which screenshots will be saved. Internal resolutions preserve more detail at the cost of "
			   "file size."));

		dialog()->registerWidgetHelp(m_capture.screenshotFormat, tr("Screenshot Format"), tr("PNG"),
			tr("Selects the format which will be used to save screenshots. JPEG produces smaller files, but loses detail."));

		dialog()->registerWidgetHelp(m_capture.screenshotQuality, tr("Screenshot Quality"), tr("90%"),
			tr("Selects the quality at which screenshots will be compressed. Higher values preserve more detail for JPEG and WebP, and reduce file "
			   "size for PNG."));

		dialog()->registerWidgetHelp(m_display.stretchY, tr("Vertical Stretch"), tr("100%"),
			// Characters </> need to be converted into entities in order to be shown correctly.
			tr("Stretches (&lt; 100%) or squashes (&gt; 100%) the vertical component of the display."));

		dialog()->registerWidgetHelp(m_display.fullscreenModes, tr("Fullscreen Mode"), tr("Borderless Fullscreen"),
			tr("Chooses the fullscreen resolution and frequency."));

		dialog()->registerWidgetHelp(
			m_display.cropLeft, tr("Left"), tr("0px"), tr("Changes the number of pixels cropped from the left side of the display."));

		dialog()->registerWidgetHelp(
			m_display.cropTop, tr("Top"), tr("0px"), tr("Changes the number of pixels cropped from the top of the display."));

		dialog()->registerWidgetHelp(
			m_display.cropRight, tr("Right"), tr("0px"), tr("Changes the number of pixels cropped from the right side of the display."));

		dialog()->registerWidgetHelp(
			m_display.cropBottom, tr("Bottom"), tr("0px"), tr("Changes the number of pixels cropped from the bottom of the display."));
	}

	// Rendering tab
	{
		// Hardware
		dialog()->registerWidgetHelp(m_hw.upscaleMultiplier, tr("Internal Resolution"), tr("Native (PS2) (Default)"),
			tr("Control the resolution at which games are rendered. High resolutions can impact performance on "
			   "older or lower-end GPUs.<br>Non-native resolution may cause minor graphical issues in some games.<br>"
			   "FMV resolution will remain unchanged, as the video files are pre-rendered."));

		dialog()->registerWidgetHelp(
			m_hw.mipmapping, tr("Mipmapping"), tr("Checked"), tr("Enables mipmapping, which some games require to render correctly. Mipmapping uses progressively lower resolution variants of textures at progressively further distances to reduce processing load and avoid visual artifacts."));

		dialog()->registerWidgetHelp(
			m_hw.textureFiltering, tr("Texture Filtering"), tr("Bilinear (PS2)"),
			tr("Changes what filtering algorithm is used to map textures to surfaces.<br> "
			   "Nearest: Makes no attempt to blend colors.<br> "
			   "Bilinear (Forced): Will blend colors together to remove harsh edges between different colored pixels even if the game told the PS2 not to.<br> "
			   "Bilinear (PS2): Will apply filtering to all surfaces that a game instructs the PS2 to filter.<br> "
			   "Bilinear (Forced Excluding Sprites): Will apply filtering to all surfaces, even if the game told the PS2 not to, except sprites."));

		dialog()->registerWidgetHelp(m_hw.trilinearFiltering, tr("Trilinear Filtering"), tr("Automatic (Default)"),
			tr("Reduces blurriness of large textures applied to small, steeply angled surfaces by sampling colors from the two nearest Mipmaps. Requires Mipmapping to be 'on'.<br> "
			   "Off: Disables the feature.<br> "
			   "Trilinear (PS2): Applies Trilinear filtering to all surfaces that a game instructs the PS2 to.<br> "
			   "Trilinear (Forced): Applies Trilinear filtering to all surfaces, even if the game told the PS2 not to."));

		dialog()->registerWidgetHelp(m_hw.anisotropicFiltering, tr("Anisotropic Filtering"), tr("Off (Default)"),
			tr("Reduces texture aliasing at extreme viewing angles."));

		dialog()->registerWidgetHelp(m_hw.dithering, tr("Dithering"), tr("Unscaled (Default)"),
			tr("Reduces banding between colors and improves the perceived color depth.<br> "
			   "Off: Disables any dithering.<br> "
			   "Scaled: Upscaling-aware / Highest dithering effect.<br> "
			   "Unscaled: Native dithering / Lowest dithering effect, does not increase size of squares when upscaling.<br> "
			   "Force 32bit: Treats all draws as if they were 32bit to avoid banding and dithering."));

		dialog()->registerWidgetHelp(m_hw.blending, tr("Blending Accuracy"), tr("Basic (Recommended)"),
			tr("Control the accuracy level of the GS blending unit emulation.<br> "
			   "The higher the setting, the more blending is emulated in the shader accurately, and the higher the speed penalty will be."));

		dialog()->registerWidgetHelp(m_advanced.texturePreloading, tr("Texture Preloading"), tr("Full (Hash Cache)"),
			tr("Uploads entire textures at once instead of in small pieces, avoiding redundant uploads when possible. "
			   "Improves performance in most games, but can make a small selection slower."));

		dialog()->registerWidgetHelp(m_fixes.gpuPaletteConversion, tr("GPU Palette Conversion"), tr("Unchecked"),
			tr("When enabled the GPU will convert colormap textures, otherwise the CPU will. "
			   "It is a trade-off between GPU and CPU."));

		dialog()->registerWidgetHelp(m_hw.enableHWFixes, tr("Manual Hardware Renderer Fixes"), tr("Unchecked"),
			tr("Enabling this option gives you the ability to change the renderer and upscaling fixes "
			   "to your games. However IF you have ENABLED this, you WILL DISABLE AUTOMATIC "
			   "SETTINGS and you can re-enable automatic settings by unchecking this option."));

		dialog()->registerWidgetHelp(m_advanced.spinCPUDuringReadbacks, tr("Spin CPU During Readbacks"), tr("Unchecked"),
			tr("Does useless work on the CPU during readbacks to prevent it from going to into powersave modes. "
			   "May improve performance during readbacks but with a significant increase in power usage."));

		dialog()->registerWidgetHelp(m_advanced.spinGPUDuringReadbacks, tr("Spin GPU During Readbacks"), tr("Unchecked"),
			tr("Submits useless work to the GPU during readbacks to prevent it from going into powersave modes. "
			   "May improve performance during readbacks but with a significant increase in power usage."));

		// Software
		dialog()->registerWidgetHelp(m_sw.extraSWThreads, tr("Software Rendering Threads"), tr("2 threads"),
			tr("Number of rendering threads: 0 for single thread, 2 or more for multithread (1 is for debugging). "
			   "2 to 4 threads is recommended, any more than that is likely to be slower instead of faster."));

		dialog()->registerWidgetHelp(m_sw.swAutoFlush, tr("Auto Flush"), tr("Checked"),
			tr("Forces a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog()->registerWidgetHelp(
			m_sw.swMipmap, tr("Mipmapping"), tr("Checked"), tr("Enables mipmapping, which some games require to render correctly."));
	}

	// Hardware Fixes tab
	{
		dialog()->registerWidgetHelp(m_fixes.cpuSpriteRenderBW, tr("CPU Sprite Render Size"), tr("0 (Disabled)"),
			tr("The maximum target memory width that will allow the CPU Sprite Renderer to activate on."));

		dialog()->registerWidgetHelp(m_fixes.cpuCLUTRender, tr("Software CLUT Render"), tr("0 (Disabled)"),
			tr("Tries to detect when a game is drawing its own color palette and then renders it in software, instead of on the GPU."));

		dialog()->registerWidgetHelp(m_fixes.gpuTargetCLUTMode, tr("GPU Target CLUT"), tr("Disabled"),
			tr("Tries to detect when a game is drawing its own color palette and then renders it on the GPU with special handling."));

		dialog()->registerWidgetHelp(m_fixes.skipDrawStart, tr("Skip Draw Range Start"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog()->registerWidgetHelp(m_fixes.skipDrawEnd, tr("Skip Draw Range End"), tr("0"),
			tr("Completely skips drawing surfaces from the surface in the left box up to the surface specified in the box on the right."));

		dialog()->registerWidgetHelp(m_fixes.hwAutoFlush, tr("Auto Flush"), tr("Unchecked"),
			tr("Forces a primitive flush when a framebuffer is also an input texture. "
			   "Fixes some processing effects such as the shadows in the Jak series and radiosity in GTA:SA."));

		dialog()->registerWidgetHelp(m_fixes.disableDepthEmulation, tr("Disable Depth Conversion"), tr("Unchecked"),
			tr("Disables the support of depth buffers in the texture cache. "
			   "Will likely create various glitches and is only useful for debugging."));

		dialog()->registerWidgetHelp(m_fixes.disableSafeFeatures, tr("Disable Safe Features"), tr("Unchecked"),
			tr("This option disables multiple safe features. "
			   "Disables accurate Unscale Point and Line rendering which can help Xenosaga games. "
			   "Disables accurate GS Memory Clearing to be done on the CPU, and lets the GPU handle it, which can help Kingdom Hearts "
			   "games."));

		dialog()->registerWidgetHelp(
			m_fixes.disableRenderFixes, tr("Disable Render Fixes"), tr("Unchecked"), tr("This option disables game-specific render fixes."));

		dialog()->registerWidgetHelp(m_fixes.disablePartialInvalidation, tr("Disable Partial Source Invalidation"), tr("Unchecked"),
			tr("By default, the texture cache handles partial invalidations. Unfortunately it is very costly to compute CPU wise. "
			   "This hack replaces the partial invalidation with a complete deletion of the texture to reduce the CPU load. "
			   "It helps with the Snowblind engine games."));
		dialog()->registerWidgetHelp(m_fixes.frameBufferConversion, tr("Framebuffer Conversion"), tr("Unchecked"),
			tr("Convert 4-bit and 8-bit framebuffer on the CPU instead of the GPU. "
			   "Helps Harry Potter and Stuntman games. It has a big impact on performance."));

		dialog()->registerWidgetHelp(m_fixes.preloadFrameData, tr("Preload Frame Data"), tr("Unchecked"),
			tr("Uploads GS data when rendering a new frame to reproduce some effects accurately."));

		dialog()->registerWidgetHelp(m_fixes.textureInsideRt, tr("Texture Inside RT"), tr("Disabled"),
			tr("Allows the texture cache to reuse as an input texture the inner portion of a previous framebuffer."));

		dialog()->registerWidgetHelp(m_fixes.readTCOnClose, tr("Read Targets When Closing"), tr("Unchecked"),
			tr("Flushes all targets in the texture cache back to local memory when shutting down. Can prevent lost visuals when saving "
			   "state or switching graphics APIs, but can also cause graphical corruption."));

		dialog()->registerWidgetHelp(m_fixes.estimateTextureRegion, tr("Estimate Texture Region"), tr("Unchecked"),
			tr("Attempts to reduce the texture size when games do not set it themselves (e.g. Snowblind games)."));
	}

	// Upscaling Fixes tab
	{
		dialog()->registerWidgetHelp(m_upscaling.halfPixelOffset, tr("Half Pixel Offset"), tr("Off (Default)"),
			tr("Might fix some misaligned fog, bloom, or blend effect."));

		dialog()->registerWidgetHelp(m_upscaling.roundSprite, tr("Round Sprite"), tr("Off (Default)"),
			tr("Corrects the sampling of 2D sprite textures when upscaling. "
			   "Fixes lines in sprites of games like Ar tonelico when upscaling. Half option is for flat sprites, Full is for all "
			   "sprites."));

		dialog()->registerWidgetHelp(m_upscaling.textureOffsetX, tr("Texture Offsets X"), tr("0"),
			//: ST and UV are different types of texture coordinates, like XY would be spatial coordinates.
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment "
			   "too."));

		dialog()->registerWidgetHelp(m_upscaling.textureOffsetY, tr("Texture Offsets Y"), tr("0"),
			//: ST and UV are different types of texture coordinates, like XY would be spatial coordinates.
			tr("Offset for the ST/UV texture coordinates. Fixes some odd texture issues and might fix some post processing alignment "
			   "too."));

		dialog()->registerWidgetHelp(m_upscaling.alignSprite, tr("Align Sprite"), tr("Unchecked"),
			//: Namco: a game publisher and development company. Leave the name as-is. Ace Combat, Tekken, Soul Calibur: game names. Leave as-is or use official translations.
			tr("Fixes issues with upscaling (vertical lines) in Namco games like Ace Combat, Tekken, Soul Calibur, etc."));

		dialog()->registerWidgetHelp(m_upscaling.forceEvenSpritePosition, tr("Force Even Sprite Position"), tr("Unchecked"),
			//: Wild Arms: name of a game series. Leave as-is or use an official translation.
			tr("Lowers the GS precision to avoid gaps between pixels when upscaling. Fixes the text on Wild Arms games."));

		dialog()->registerWidgetHelp(m_upscaling.bilinearHack, tr("Bilinear Dirty Upscale"), tr("Unchecked"),
			tr("Can smooth out textures due to be bilinear filtered when upscaling. E.g. Brave sun glare."));

		dialog()->registerWidgetHelp(m_upscaling.mergeSprite, tr("Merge Sprite"), tr("Unchecked"),
			tr("Replaces post-processing multiple paving sprites by a single fat sprite. It reduces various upscaling lines."));

		dialog()->registerWidgetHelp(m_upscaling.nativePaletteDraw, tr("Unscaled Palette Texture Draws"), tr("Unchecked"),
			tr("Forces palette texture draws to render at native resolution."));
	}

	// Texture Replacement tab
	{
		dialog()->registerWidgetHelp(m_texture.dumpReplaceableTextures, tr("Dump Textures"), tr("Unchecked"), tr("Dumps replaceable textures to disk. Will reduce performance."));

		dialog()->registerWidgetHelp(m_texture.dumpReplaceableMipmaps, tr("Dump Mipmaps"), tr("Unchecked"), tr("Includes mipmaps when dumping textures."));

		dialog()->registerWidgetHelp(m_texture.dumpTexturesWithFMVActive, tr("Dump FMV Textures"), tr("Unchecked"), tr("Allows texture dumping when FMVs are active. You should not enable this."));

		dialog()->registerWidgetHelp(m_texture.loadTextureReplacementsAsync, tr("Asynchronous Texture Loading"), tr("Checked"), tr("Loads replacement textures on a worker thread, reducing microstutter when replacements are enabled."));

		dialog()->registerWidgetHelp(m_texture.loadTextureReplacements, tr("Load Textures"), tr("Unchecked"), tr("Loads replacement textures where available and user-provided."));

		dialog()->registerWidgetHelp(m_texture.precacheTextureReplacements, tr("Precache Textures"), tr("Unchecked"), tr("Preloads all replacement textures to memory. Not necessary with asynchronous loading."));
	}

	// Post Processing tab
	{
		//: You might find an official translation for this on AMD's website (Spanish version linked): https://www.amd.com/es/technologies/radeon-software-fidelityfx
		dialog()->registerWidgetHelp(m_post.casMode, tr("Contrast Adaptive Sharpening"), tr("None (Default)"), tr("Enables FidelityFX Contrast Adaptive Sharpening."));

		dialog()->registerWidgetHelp(m_post.casSharpness, tr("Sharpness"), tr("50%"), tr("Determines the intensity the sharpening effect in CAS post-processing."));

		dialog()->registerWidgetHelp(m_post.shadeBoost, tr("Shade Boost"), tr("Unchecked"),
			tr("Enables saturation, contrast, and brightness to be adjusted. Values of brightness, saturation, and contrast are at default "
			   "50."));

		dialog()->registerWidgetHelp(
			m_post.fxaa, tr("FXAA"), tr("Unchecked"), tr("Applies the FXAA anti-aliasing algorithm to improve the visual quality of games."));

		dialog()->registerWidgetHelp(m_post.shadeBoostBrightness, tr("Brightness"), tr("50"), tr("Adjusts brightness. 50 is normal."));

		dialog()->registerWidgetHelp(m_post.shadeBoostContrast, tr("Contrast"), tr("50"), tr("Adjusts contrast. 50 is normal."));

		dialog()->registerWidgetHelp(m_post.shadeBoostGamma, tr("Gamma"), tr("50"), tr("Adjusts gamma. 50 is normal."));

		dialog()->registerWidgetHelp(m_post.shadeBoostSaturation, tr("Saturation"), tr("50"), tr("Adjusts saturation. 50 is normal."));

		dialog()->registerWidgetHelp(m_post.tvShader, tr("TV Shader"), tr("None (Default)"),
			tr("Applies a shader which replicates the visual effects of different styles of television sets."));
	}

	// OSD tab
	{
		dialog()->registerWidgetHelp(m_osd.scale, tr("OSD Scale"), tr("100%"), tr("Scales the size of the onscreen OSD from 50% to 500%."));

		dialog()->registerWidgetHelp(m_osd.messagesPos, tr("OSD Messages Position"), tr("Left (Default)"),
			tr("Position of on-screen-display messages when events occur such as save states being "
			   "created/loaded, screenshots being taken, etc."));

		dialog()->registerWidgetHelp(m_osd.performancePos, tr("OSD Performance Position"), tr("Right (Default)"),
			tr("Position of a variety of on-screen performance data points as selected by the user."));

		dialog()->registerWidgetHelp(m_osd.showSpeedPercentages, tr("Show Speed Percentages"), tr("Unchecked"),
			tr("Shows the current emulation speed of the system as a percentage."));

		dialog()->registerWidgetHelp(m_osd.showFPS, tr("Show FPS"), tr("Unchecked"),
			tr("Shows the number of internal video frames displayed per second by the system."));

		dialog()->registerWidgetHelp(m_osd.showVPS, tr("Show VPS"), tr("Unchecked"),
			tr("Shows the number of Vsyncs performed per second by the system."));

		dialog()->registerWidgetHelp(m_osd.showResolution, tr("Show Resolution"), tr("Unchecked"),
			tr("Shows the internal resolution of the game."));

		dialog()->registerWidgetHelp(m_osd.showGSStats, tr("Show GS Statistics"), tr("Unchecked"),
			tr("Shows statistics about the emulated GS such as primitives and draw calls."));

		dialog()->registerWidgetHelp(m_osd.showUsageCPU, tr("Show CPU Usage"),
			tr("Unchecked"), tr("Shows the host's CPU utilization based on threads."));

		dialog()->registerWidgetHelp(m_osd.showUsageGPU, tr("Show GPU Usage"),
			tr("Unchecked"), tr("Shows the host's GPU utilization."));

		dialog()->registerWidgetHelp(m_osd.showStatusIndicators, tr("Show Status Indicators"), tr("Checked"),
			tr("Shows icon indicators for emulation states such as Pausing, Turbo, Fast-Forward, and Slow-Motion."));

		dialog()->registerWidgetHelp(m_osd.showFrameTimes, tr("Show Frame Times"), tr("Unchecked"),
			tr("Displays a graph showing the average frametimes."));

		dialog()->registerWidgetHelp(m_osd.showHardwareInfo, tr("Show Hardware Info"), tr("Unchecked"),
			tr("Shows the current system CPU and GPU information."));

		dialog()->registerWidgetHelp(m_osd.showVersion, tr("Show PCSX2 Version"), tr("Unchecked"),
			tr("Shows the current PCSX2 version."));

		dialog()->registerWidgetHelp(m_osd.showSettings, tr("Show Settings"), tr("Unchecked"),
			tr("Displays various settings and the current values of those settings in the bottom-right corner of the display."));

		dialog()->registerWidgetHelp(m_osd.showPatches, tr("Show Patches"), tr("Unchecked"),
			tr("Shows the amount of currently active patches/cheats in the bottom-right corner of the display."));

		dialog()->registerWidgetHelp(m_osd.showInputs, tr("Show Inputs"), tr("Unchecked"),
			tr("Shows the current controller state of the system in the bottom-left corner of the display."));

		dialog()->registerWidgetHelp(m_osd.showVideoCapture, tr("Show Video Capture Status"), tr("Checked"),
			tr("Shows the status of the currently active video capture in the top-right corner of the display."));

		dialog()->registerWidgetHelp(m_osd.showInputRec, tr("Show Input Recording Status"), tr("Checked"),
			tr("Shows the status of the currently active input recording in the top-right corner of the display."));

		dialog()->registerWidgetHelp(m_osd.showTextureReplacements, tr("Show Texture Replacement Status"), tr("Unchecked"),
			tr("Shows the status of the number of dumped and loaded texture replacements in the top-right corner of the display."));

		dialog()->registerWidgetHelp(m_osd.warnAboutUnsafeSettings, tr("Warn About Unsafe Settings"), tr("Checked"),
			tr("Displays warnings when settings are enabled which may break games."));

		connect(m_osd.showSettings, &QCheckBox::checkStateChanged, this,
			&GraphicsSettingsWidget::onOsdShowSettingsToggled);
	}

	// Recording tab
	{
		dialog()->registerWidgetHelp(m_capture.videoCaptureCodec, tr("Video Codec"), tr("Default"),
			tr("Selects the Video Codec to be used for Video Capture. "
			   "<b>If unsure, leave it on default.<b>"));

		dialog()->registerWidgetHelp(m_capture.videoCaptureFormat, tr("Video Format"), tr("Default"),
			tr("Selects the Video Format to be used for Video Capture. If by chance the codec does not support the format, the first format available will be used. "
			   "<b>If unsure, leave it on default.<b>"));

		dialog()->registerWidgetHelp(m_capture.videoCaptureBitrate, tr("Video Bitrate"), tr("6000 kbps"),
			tr("Sets the video bitrate to be used. "
			   "Higher bitrates generally yield better video quality at the cost of larger resulting file sizes."));

		dialog()->registerWidgetHelp(m_capture.videoCaptureResolutionAuto, tr("Automatic Resolution"), tr("Unchecked"),
			tr("When checked, the video capture resolution will follow the internal resolution of the running game.<br><br>"

			   "<b>Be careful when using this setting especially when you are upscaling, as higher internal resolutions (above 4x) can result in very large video capture and can cause system overload.</b>"));


		dialog()->registerWidgetHelp(m_capture.enableVideoCaptureArguments, tr("Enable Extra Video Arguments"), tr("Unchecked"), tr("Allows you to pass arguments to the selected video codec."));

		dialog()->registerWidgetHelp(m_capture.videoCaptureArguments, tr("Extra Video Arguments"), tr("Leave It Blank"),
			tr("Parameters passed to the selected video codec.<br>"
			   "<b>You must use '=' to separate key from value and ':' to separate two pairs from each other.</b><br>"
			   "For example: \"crf = 21 : preset = veryfast\""));

		dialog()->registerWidgetHelp(m_capture.audioCaptureCodec, tr("Audio Codec"), tr("Default"),
			tr("Selects the Audio Codec to be used for Video Capture. "
			   "<b>If unsure, leave it on default.<b>"));

		dialog()->registerWidgetHelp(m_capture.audioCaptureBitrate, tr("Audio Bitrate"), tr("192 kbps"), tr("Sets the audio bitrate to be used."));

		dialog()->registerWidgetHelp(m_capture.enableAudioCaptureArguments, tr("Enable Extra Audio Arguments"), tr("Unchecked"), tr("Allows you to pass arguments to the selected audio codec."));

		dialog()->registerWidgetHelp(m_capture.audioCaptureArguments, tr("Extra Audio Arguments"), tr("Leave It Blank"),
			tr("Parameters passed to the selected audio codec.<br>"
			   "<b>You must use '=' to separate key from value and ':' to separate two pairs from each other.</b><br>"
			   "For example: \"compression_level = 4 : joint_stereo = 1\""));
	}

	// Advanced tab
	{
		dialog()->registerWidgetHelp(m_advanced.gsDumpCompression, tr("GS Dump Compression"), tr("Zstandard (zst)"),
			tr("Change the compression algorithm used when creating a GS dump."));

		//: Blit = a data operation. You might want to write it as-is, but fully uppercased. More information: https://en.wikipedia.org/wiki/Bit_blit \nSwap chain: see Microsoft's Terminology Portal.
		dialog()->registerWidgetHelp(m_advanced.useBlitSwapChain, tr("Use Blit Swap Chain"), tr("Unchecked"),
			//: Blit = a data operation. You might want to write it as-is, but fully uppercased. More information: https://en.wikipedia.org/wiki/Bit_blit
			tr("Uses a blit presentation model instead of flipping when using the Direct3D 11 "
			   "graphics API. This usually results in slower performance, but may be required for some "
			   "streaming applications, or to uncap framerates on some systems."));

		dialog()->registerWidgetHelp(m_advanced.exclusiveFullscreenControl, tr("Allow Exclusive Fullscreen"), tr("Automatic (Default)"),
			tr("Overrides the driver's heuristics for enabling exclusive fullscreen, or direct flip/scanout.<br>"
			   "Disallowing exclusive fullscreen may enable smoother task switching and overlays, but increase input latency."));

		dialog()->registerWidgetHelp(m_advanced.disableMailboxPresentation, tr("Disable Mailbox Presentation"), tr("Unchecked"),
			tr("Forces the use of FIFO over Mailbox presentation, i.e. double buffering instead of triple buffering. "
			   "Usually results in worse frame pacing."));

		dialog()->registerWidgetHelp(m_advanced.extendedUpscales, tr("Extended Upscaling Multipliers"), tr("Unchecked"),
			tr("Displays additional, very high upscaling multipliers dependent on GPU capability."));

		dialog()->registerWidgetHelp(m_advanced.useDebugDevice, tr("Enable Debug Device"), tr("Unchecked"),
			tr("Enables API-level validation of graphics commands."));

		dialog()->registerWidgetHelp(m_advanced.gsDownloadMode, tr("GS Download Mode"), tr("Accurate"),
			tr("Skips synchronizing with the GS thread and host GPU for GS downloads. "
			   "Can result in a large speed boost on slower systems, at the cost of many broken graphical effects. "
			   "If games are broken and you have this option enabled, please disable it first."));

		dialog()->registerWidgetHelp(m_advanced.ntscFrameRate, tr("NTSC Frame Rate"), tr("59.94 Hz"),
			tr("Determines what frame rate NTSC games run at."));

		dialog()->registerWidgetHelp(m_advanced.palFrameRate, tr("PAL Frame Rate"), tr("50.00 Hz"),
			tr("Determines what frame rate PAL games run at."));
	}
}

GraphicsSettingsWidget::~GraphicsSettingsWidget() = default;

void GraphicsSettingsWidget::onTextureFilteringChange()
{
	const QSignalBlocker block(m_sw.swTextureFiltering);

	m_sw.swTextureFiltering->setCurrentIndex(m_hw.textureFiltering->currentIndex());
}

void GraphicsSettingsWidget::onSWTextureFilteringChange()
{
	const QSignalBlocker block(m_hw.textureFiltering);

	m_hw.textureFiltering->setCurrentIndex(m_sw.swTextureFiltering->currentIndex());
}

void GraphicsSettingsWidget::onRendererChanged(int index)
{
	if (dialog()->isPerGameSettings())
	{
		if (index > 0)
			dialog()->setIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(s_renderer_info[index - 1].type));
		else
			dialog()->setIntSettingValue("EmuCore/GS", "Renderer", std::nullopt);
	}
	else
	{
		dialog()->setIntSettingValue("EmuCore/GS", "Renderer", static_cast<int>(s_renderer_info[index].type));
	}

	g_emu_thread->applySettings();
	updateRendererDependentOptions();
}

void GraphicsSettingsWidget::onAdapterChanged(int index)
{
	const int first_adapter = dialog()->isPerGameSettings() ? 2 : 1;

	if (index >= first_adapter)
		dialog()->setStringSettingValue("EmuCore/GS", "Adapter", m_header.adapterDropdown->currentText().toUtf8().constData());
	else if (index > 0 && dialog()->isPerGameSettings())
		dialog()->setStringSettingValue("EmuCore/GS", "Adapter", "");
	else
		dialog()->setStringSettingValue("EmuCore/GS", "Adapter", std::nullopt);

	g_emu_thread->applySettings();
}

void GraphicsSettingsWidget::onFullscreenModeChanged(int index)
{
	const int first_mode = dialog()->isPerGameSettings() ? 2 : 1;

	if (index >= first_mode)
		dialog()->setStringSettingValue("EmuCore/GS", "FullscreenMode", m_display.fullscreenModes->currentText().toUtf8().constData());
	else if (index > 0 && dialog()->isPerGameSettings())
		dialog()->setStringSettingValue("EmuCore/GS", "FullscreenMode", "");
	else
		dialog()->setStringSettingValue("EmuCore/GS", "FullscreenMode", std::nullopt);

	g_emu_thread->applySettings();
}

void GraphicsSettingsWidget::onTrilinearFilteringChanged()
{
	const bool forced_bilinear = (dialog()->getEffectiveIntValue("EmuCore/GS", "TriFilter", static_cast<int>(TriFiltering::Automatic)) >=
								  static_cast<int>(TriFiltering::Forced));
	m_hw.textureFiltering->setDisabled(forced_bilinear);
}

void GraphicsSettingsWidget::onShadeBoostChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "ShadeBoost", false);
	m_post.shadeBoostBrightness->setEnabled(enabled);
	m_post.shadeBoostContrast->setEnabled(enabled);
	m_post.shadeBoostGamma->setEnabled(enabled);
	m_post.shadeBoostSaturation->setEnabled(enabled);
}

void GraphicsSettingsWidget::onMessagesPosChanged()
{
	const bool enabled = m_osd.messagesPos->currentIndex() != (dialog()->isPerGameSettings() ? 1 : 0);

	m_osd.warnAboutUnsafeSettings->setEnabled(enabled);
}

void GraphicsSettingsWidget::onPerformancePosChanged()
{
	const bool enabled = m_osd.performancePos->currentIndex() != (dialog()->isPerGameSettings() ? 1 : 0);

	m_osd.showSpeedPercentages->setEnabled(enabled);
	m_osd.showFPS->setEnabled(enabled);
	m_osd.showVPS->setEnabled(enabled);
	m_osd.showResolution->setEnabled(enabled);
	m_osd.showGSStats->setEnabled(enabled);
	m_osd.showUsageCPU->setEnabled(enabled);
	m_osd.showUsageGPU->setEnabled(enabled);
	m_osd.showStatusIndicators->setEnabled(enabled);
	m_osd.showFrameTimes->setEnabled(enabled);
	m_osd.showHardwareInfo->setEnabled(enabled);
	m_osd.showVersion->setEnabled(enabled);
}

void GraphicsSettingsWidget::onTextureDumpChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "DumpReplaceableTextures", false);
	m_texture.dumpReplaceableMipmaps->setEnabled(enabled);
	m_texture.dumpTexturesWithFMVActive->setEnabled(enabled);
}

void GraphicsSettingsWidget::onTextureReplacementChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "LoadTextureReplacements", false);
	m_texture.loadTextureReplacementsAsync->setEnabled(enabled);
	m_texture.precacheTextureReplacements->setEnabled(enabled);
}

void GraphicsSettingsWidget::onCaptureContainerChanged()
{
	const std::string container(
		dialog()->getEffectiveStringValue("EmuCore/GS", "CaptureContainer", Pcsx2Config::GSOptions::DEFAULT_CAPTURE_CONTAINER));

	QObject::disconnect(m_capture.videoCaptureCodec, &QComboBox::currentIndexChanged, nullptr, nullptr);
	m_capture.videoCaptureCodec->clear();
	//: This string refers to a default codec, whether it's an audio codec or a video codec.
	m_capture.videoCaptureCodec->addItem(tr("Default"), QString());
	for (const auto& [format, name] : GSCapture::GetVideoCodecList(container.c_str()))
	{
		const QString qformat(QString::fromStdString(format));
		const QString qname(QString::fromStdString(name));
		m_capture.videoCaptureCodec->addItem(QStringLiteral("%1 [%2]").arg(qformat).arg(qname), qformat);
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		dialog()->getSettingsInterface(), m_capture.videoCaptureCodec, "EmuCore/GS", "VideoCaptureCodec");
	connect(m_capture.videoCaptureCodec, &QComboBox::currentIndexChanged, this, &GraphicsSettingsWidget::onCaptureCodecChanged);

	QObject::disconnect(m_capture.audioCaptureCodec, &QComboBox::currentIndexChanged, nullptr, nullptr);
	m_capture.audioCaptureCodec->clear();
	m_capture.audioCaptureCodec->addItem(tr("Default"), QString());
	for (const auto& [format, name] : GSCapture::GetAudioCodecList(container.c_str()))
	{
		const QString qformat(QString::fromStdString(format));
		const QString qname(QString::fromStdString(name));
		m_capture.audioCaptureCodec->addItem(QStringLiteral("%1 [%2]").arg(qformat).arg(qname), qformat);
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		dialog()->getSettingsInterface(), m_capture.audioCaptureCodec, "EmuCore/GS", "AudioCaptureCodec");
}

void GraphicsSettingsWidget::GraphicsSettingsWidget::onCaptureCodecChanged()
{
	QObject::disconnect(m_capture.videoCaptureFormat, &QComboBox::currentIndexChanged, nullptr, nullptr);
	m_capture.videoCaptureFormat->clear();
	//: This string refers to a default pixel format
	m_capture.videoCaptureFormat->addItem(tr("Default"), "");

	const std::string codec(
		dialog()->getEffectiveStringValue("EmuCore/GS", "VideoCaptureCodec", ""));

	if (!codec.empty())
	{
		for (const auto& [id, name] : GSCapture::GetVideoFormatList(codec.c_str()))
		{
			const QString qid(QString::number(id));
			const QString qname(QString::fromStdString(name));
			m_capture.videoCaptureFormat->addItem(qname, qid);
		}
	}

	SettingWidgetBinder::BindWidgetToStringSetting(
		dialog()->getSettingsInterface(), m_capture.videoCaptureFormat, "EmuCore/GS", "VideoCaptureFormat");
}

void GraphicsSettingsWidget::onEnableVideoCaptureChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "EnableVideoCapture", true);
	m_capture.videoCaptureOptions->setEnabled(enabled);
}

void GraphicsSettingsWidget::onOsdShowSettingsToggled()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "OsdShowSettings", false);
	m_osd.showPatches->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableVideoCaptureArgumentsChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "EnableVideoCaptureParameters", false);
	m_capture.videoCaptureArguments->setEnabled(enabled);
}

void GraphicsSettingsWidget::onVideoCaptureAutoResolutionChanged()
{
	const bool enabled = !dialog()->getEffectiveBoolValue("EmuCore/GS", "VideoCaptureAutoResolution", true);
	m_capture.videoCaptureWidth->setEnabled(enabled);
	m_capture.videoCaptureHeight->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableAudioCaptureChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "EnableAudioCapture", true);
	m_capture.audioCaptureOptions->setEnabled(enabled);
}

void GraphicsSettingsWidget::onEnableAudioCaptureArgumentsChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "EnableAudioCaptureParameters", false);
	m_capture.audioCaptureArguments->setEnabled(enabled);
}

void GraphicsSettingsWidget::onGpuPaletteConversionChanged(int state)
{
	const bool disabled =
		state == Qt::CheckState::PartiallyChecked ? Host::GetBaseBoolSettingValue("EmuCore/GS", "paltex", false) : (state != 0);

	m_hw.anisotropicFiltering->setDisabled(disabled);
}

void GraphicsSettingsWidget::onCPUSpriteRenderBWChanged()
{
	const int value = dialog()->getEffectiveIntValue("EmuCore/GS", "UserHacks_CPUSpriteRenderBW", 0);
	m_fixes.cpuSpriteRenderLevel->setEnabled(value != 0);
}

GSRendererType GraphicsSettingsWidget::getEffectiveRenderer() const
{
	const GSRendererType type =
		static_cast<GSRendererType>(dialog()->getEffectiveIntValue("EmuCore/GS", "Renderer", static_cast<int>(GSRendererType::Auto)));
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
	const bool is_disable_barriers = (type == GSRendererType::Metal || type == GSRendererType::SW);
	const bool hw_fixes = (is_hardware && m_hw.enableHWFixes && m_hw.enableHWFixes->checkState() == Qt::Checked);

	QWidget* prev_tab;
	if (is_hardware)
	{
		setTabVisible(m_hardware_rendering_tab, true);
		setTabVisible(m_software_rendering_tab, false, m_hardware_rendering_tab);

		prev_tab = m_hardware_rendering_tab;
	}
	else if (is_software)
	{
		setTabVisible(m_software_rendering_tab, true);
		setTabVisible(m_hardware_rendering_tab, false, m_software_rendering_tab);

		prev_tab = m_software_rendering_tab;
	}
	else
	{
		setTabVisible(m_hardware_rendering_tab, false, m_display_tab);
		setTabVisible(m_software_rendering_tab, false, m_display_tab);

		prev_tab = m_display_tab;
	}

	setTabVisible(m_hardware_fixes_tab, hw_fixes, prev_tab);
	setTabVisible(m_upscaling_fixes_tab, hw_fixes, prev_tab);
	setTabVisible(m_texture_replacement_tab, is_hardware, prev_tab);

	if (m_advanced.useBlitSwapChain)
		m_advanced.useBlitSwapChain->setEnabled(is_dx11);

	if (m_advanced.overrideTextureBarriers)
		m_advanced.overrideTextureBarriers->setDisabled(is_disable_barriers);

	if (m_advanced.disableFramebufferFetch)
		m_advanced.disableFramebufferFetch->setDisabled(is_sw_dx);

	if (m_advanced.exclusiveFullscreenControl)
		m_advanced.exclusiveFullscreenControl->setEnabled(is_auto || is_vk);

	// populate adapters
	std::vector<GSAdapterInfo> adapters = GSGetAdapterInfo(type);
	const GSAdapterInfo* current_adapter_info = nullptr;

	// fill+select adapters
	{
		QSignalBlocker sb(m_header.adapterDropdown);

		std::string current_adapter = Host::GetBaseStringSettingValue("EmuCore/GS", "Adapter", "");
		m_header.adapterDropdown->clear();
		m_header.adapterDropdown->setEnabled(!adapters.empty());
		m_header.adapterDropdown->addItem(tr("(Default)"));
		m_header.adapterDropdown->setCurrentIndex(0);

		// Treat default adapter as empty
		if (current_adapter == GetDefaultAdapter())
			current_adapter.clear();

		if (dialog()->isPerGameSettings())
		{
			m_header.adapterDropdown->insertItem(
				0, tr("Use Global Setting [%1]").arg((current_adapter.empty()) ? tr("(Default)") : QString::fromStdString(current_adapter)));
			if (!dialog()->getSettingsInterface()->GetStringValue("EmuCore/GS", "Adapter", &current_adapter))
			{
				// clear the adapter so we don't set it to the global value
				current_adapter.clear();
				m_header.adapterDropdown->setCurrentIndex(0);
			}
		}

		for (const GSAdapterInfo& adapter : adapters)
		{
			m_header.adapterDropdown->addItem(QString::fromStdString(adapter.name));
			if (current_adapter == adapter.name)
			{
				m_header.adapterDropdown->setCurrentIndex(m_header.adapterDropdown->count() - 1);
				current_adapter_info = &adapter;
			}
		}

		current_adapter_info = (current_adapter_info || adapters.empty()) ? current_adapter_info : &adapters.front();
	}

	// fill+select fullscreen modes
	{
		QSignalBlocker sb(m_display.fullscreenModes);

		std::string current_mode(Host::GetBaseStringSettingValue("EmuCore/GS", "FullscreenMode", ""));
		m_display.fullscreenModes->clear();
		m_display.fullscreenModes->addItem(tr("Borderless Fullscreen"));
		m_display.fullscreenModes->setCurrentIndex(0);

		if (dialog()->isPerGameSettings())
		{
			m_display.fullscreenModes->insertItem(
				0, tr("Use Global Setting [%1]").arg(current_mode.empty() ? tr("Borderless Fullscreen") : QString::fromStdString(current_mode)));
			if (!dialog()->getSettingsInterface()->GetStringValue("EmuCore/GS", "FullscreenMode", &current_mode))
			{
				current_mode.clear();
				m_display.fullscreenModes->setCurrentIndex(0);
			}
		}

		if (current_adapter_info)
		{
			for (const std::string& fs_mode : current_adapter_info->fullscreen_modes)
			{
				m_display.fullscreenModes->addItem(QString::fromStdString(fs_mode));
				if (current_mode == fs_mode)
					m_display.fullscreenModes->setCurrentIndex(m_display.fullscreenModes->count() - 1);
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
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "2x Native (~720px/HD)"), 2.0f},
		{QT_TRANSLATE_NOOP("GraphicsSettingsWidget", "3x Native (~1080px/FHD)"), 3.0f},
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

	QSignalBlocker sb(m_hw.upscaleMultiplier);
	m_hw.upscaleMultiplier->clear();

	const u32 max_shown_multiplier = m_advanced.extendedUpscales && m_advanced.extendedUpscales->checkState() == Qt::Checked ?
	                                     max_upscale_multiplier :
	                                     std::min(max_upscale_multiplier, max_non_advanced_multiplier);
	for (const auto& [name, value] : templates)
	{
		if (value > max_shown_multiplier)
			break;

		m_hw.upscaleMultiplier->addItem(tr(name), QVariant(value));
	}
	for (u32 i = max_template_multiplier + 1; i <= max_shown_multiplier; i++)
		m_hw.upscaleMultiplier->addItem(tr("%1x Native").arg(i), QVariant(static_cast<float>(i)));

	const float global_value = Host::GetBaseFloatSettingValue("EmuCore/GS", "upscale_multiplier", 1.0f);
	if (dialog()->isPerGameSettings())
	{
		const int name_idx = m_hw.upscaleMultiplier->findData(QVariant(global_value));
		const QString global_name = (name_idx >= 0) ? m_hw.upscaleMultiplier->itemText(name_idx) : tr("%1x Native");
		m_hw.upscaleMultiplier->insertItem(0, tr("Use Global Setting [%1]").arg(global_name));

		const std::optional<float> config_value = dialog()->getFloatValue("EmuCore/GS", "upscale_multiplier", std::nullopt);
		if (config_value.has_value())
		{
			if (int index = m_hw.upscaleMultiplier->findData(QVariant(config_value.value())); index > 0)
				m_hw.upscaleMultiplier->setCurrentIndex(index);
		}
		else
		{
			m_hw.upscaleMultiplier->setCurrentIndex(0);
		}
	}
	else
	{
		if (int index = m_hw.upscaleMultiplier->findData(QVariant(global_value)); index > 0)
			m_hw.upscaleMultiplier->setCurrentIndex(index);
	}
}

void GraphicsSettingsWidget::onUpscaleMultiplierChanged()
{
	const QVariant data = m_hw.upscaleMultiplier->currentData();
	dialog()->setFloatSettingValue("EmuCore/GS", "upscale_multiplier",
		data.isValid() ? std::optional<float>(data.toFloat()) : std::optional<float>());
}
