// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PostProcessingSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/GS/GS.h"

static constexpr int DEFAULT_TV_SHADER_MODE = 0;
static constexpr int DEFAULT_CAS_SHARPNESS = 50;

PostProcessingSettingsWidget::PostProcessingSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_post, tr("Post-Processing"));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.fxaa, "EmuCore/GS", "fxaa", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.shadeBoost, "EmuCore/GS", "ShadeBoost", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostBrightness, "EmuCore/GS", "ShadeBoost_Brightness", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_BRIGHTNESS);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostContrast, "EmuCore/GS", "ShadeBoost_Contrast", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_CONTRAST);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostGamma, "EmuCore/GS", "ShadeBoost_Gamma", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_GAMMA);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.shadeBoostSaturation, "EmuCore/GS", "ShadeBoost_Saturation", Pcsx2Config::GSOptions::DEFAULT_SHADEBOOST_SATURATION);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.tvShader, "EmuCore/GS", "TVShader", DEFAULT_TV_SHADER_MODE);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casMode, "EmuCore/GS", "CASMode", static_cast<int>(Pcsx2Config::GSOptions::DEFAULT_CAS_MODE));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_post.casSharpness, "EmuCore/GS", "CASSharpness", DEFAULT_CAS_SHARPNESS);

	connect(m_post.shadeBoost, &QCheckBox::checkStateChanged, this, &PostProcessingSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();

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

PostProcessingSettingsWidget::~PostProcessingSettingsWidget() = default;

void PostProcessingSettingsWidget::onShadeBoostChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "ShadeBoost", false);
	m_post.shadeBoostBrightness->setEnabled(enabled);
	m_post.shadeBoostContrast->setEnabled(enabled);
	m_post.shadeBoostGamma->setEnabled(enabled);
	m_post.shadeBoostSaturation->setEnabled(enabled);
}

#include "moc_PostProcessingSettingsWidget.cpp"
