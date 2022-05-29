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

GraphicsSettingsWidget::GraphicsSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Global Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.adapter, "EmuCore/GS", "Adapter");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.vsync, "EmuCore/GS", "VsyncEnable", 0);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableHWFixes, "EmuCore/GS", "UserHacks", false);

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
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.DisableInterlaceOffset, "EmuCore/GS", "disable_interlace_offset", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.internalResolutionScreenshots, "EmuCore/GS", "InternalResolutionScreenshots", false);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.zoom, "EmuCore/GS", "Zoom", 100.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.stretchY, "EmuCore/GS", "StretchY", 100.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.offsetX, "EmuCore/GS", "OffsetX", 0.0f);
	SettingWidgetBinder::BindWidgetToFloatSetting(sif, m_ui.offsetY, "EmuCore/GS", "OffsetY", 0.0f);

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
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", false);

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

	//////////////////////////////////////////////////////////////////////////
	// HW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.upscaleMultiplier, "EmuCore/GS", "upscale_multiplier", 1, 1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.textureFiltering, "EmuCore/GS", "filter", static_cast<int>(BiFiltering::PS2));
	SettingWidgetBinder::BindWidgetToIntSetting(
		sif, m_ui.trilinearFiltering, "EmuCore/GS", "UserHacks_TriFilter", static_cast<int>(TriFiltering::Automatic), -1);
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.anisotropicFiltering, "EmuCore/GS", "MaxAnisotropy", s_anisotropic_filtering_entries, s_anisotropic_filtering_values, "1");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.dithering, "EmuCore/GS", "dithering_ps2", 2);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.mipmapping, "EmuCore/GS", "mipmap_hw", static_cast<int>(HWMipmapLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.crcFixLevel, "EmuCore/GS", "crc_hack_level", static_cast<int>(CRCHackLevel::Automatic), -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.blending, "EmuCore/GS", "accurate_blending_unit", static_cast<int>(AccBlendLevel::Basic));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.accurateDATE, "EmuCore/GS", "accurate_date", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.conservativeBufferAllocation, "EmuCore/GS", "conservative_framebuffer", true);
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

	//////////////////////////////////////////////////////////////////////////
	// Advanced Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useBlitSwapChain, "EmuCore/GS", "UseBlitSwapChain", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.useDebugDevice, "EmuCore/GS", "UseDebugDevice", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.skipPresentingDuplicateFrames, "EmuCore/GS", "SkipDuplicateFrames", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overrideTextureBarriers, "EmuCore/GS", "OverrideTextureBarriers", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.overrideGeometryShader, "EmuCore/GS", "OverrideGeometryShaders", -1, -1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.gsDumpCompression, "EmuCore/GS", "GSDumpCompression", static_cast<int>(GSDumpCompressionMethod::Uncompressed));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableFramebufferFetch, "EmuCore/GS", "DisableFramebufferFetch", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableDualSource, "EmuCore/GS", "DisableDualSourceBlend", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.disableHardwareReadbacks, "EmuCore/GS", "HWDisableReadbacks", false);

	//////////////////////////////////////////////////////////////////////////
	// SW Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.extraSWThreads, "EmuCore/GS", "extrathreads", 2);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.swAutoFlush, "EmuCore/GS", "autoflush_sw", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.swAA1, "EmuCore/GS", "aa1", true);
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
	updateRendererDependentOptions();

	dialog->registerWidgetHelp(m_ui.enableHWFixes, tr("Manual Hardware Renderer Fixes"), tr("Unchecked"),
		tr("Enabling this option gives you the ability to change the renderer and upscaling fixes "
		   "to your games. However IF you have ENABLED this, you WILL DISABLE AUTOMATIC "
		   "SETTINGS and you can re-enable automatic settings by unchecking this option."));

	dialog->registerWidgetHelp(m_ui.useBlitSwapChain, tr("Use Blit Swap Chain"), tr("Unchecked"),
		tr("Uses a blit presentation model instead of flipping when using the Direct3D 11 "
		   "renderer. This usually results in slower performance, but may be required for some "
		   "streaming applications, or to uncap framerates on some systems."));

	dialog->registerWidgetHelp(m_ui.skipPresentingDuplicateFrames, tr("Skip Presenting Duplicate Frames"), tr("Unchecked"),
		tr("Detects when idle frames are being presented in 25/30fps games, and skips presenting those frames. The frame is still rendered, it just means "
		   "the GPU has more time to complete it (this is NOT frame skipping). Can smooth our frame time fluctuations when the CPU/GPU are near maximum "
		   "utilization, but makes frame pacing more inconsistent and can increase input lag."));
}

GraphicsSettingsWidget::~GraphicsSettingsWidget() = default;

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
		(m_dialog->getEffectiveIntValue("EmuCore/GS", "UserHacks_TriFilter", static_cast<int>(TriFiltering::Automatic))
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

	const bool is_hardware = (type == GSRendererType::DX11 || type == GSRendererType::DX12 || type == GSRendererType::OGL || type == GSRendererType::VK);
	const bool is_software = (type == GSRendererType::SW);
	const int current_tab = m_hardware_renderer_visible ? m_ui.hardwareRendererGroup->currentIndex() : m_ui.softwareRendererGroup->currentIndex();

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
			m_ui.hardwareRendererGroup->setCurrentIndex((current_tab < 2) ? current_tab : (current_tab + 2));
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
	onEnableHardwareFixesChanged();
}
