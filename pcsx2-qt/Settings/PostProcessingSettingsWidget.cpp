// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PostProcessingSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"
#include "QtHost.h"

#include "pcsx2/Config.h"
#include "pcsx2/GS/GS.h"

#include <QtConcurrent/QtConcurrent>

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

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_post.librashaderGroup, "EmuCore/GS", "LibrashaderEnabled", false);
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_post.librashaderPreset, "EmuCore/GS", "LibrashaderPreset", std::string());
#ifndef ENABLE_LIBRASHADER
	m_post.librashaderGroup->setToolTip(tr("librashader support is not compiled into this build."));
	m_post.librashaderGroup->setEnabled(false);
#endif

	connect(m_post.librashaderBrowse, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderBrowse);
	connect(m_post.librashaderClear, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderClear);
	connect(m_post.librashaderRestoreDefaults, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderRestoreDefaults);
	connect(m_post.librashaderUseGlobalParams, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderUseGlobalParams);
	connect(m_post.librashaderPreset, &QLineEdit::textChanged, this, &PostProcessingSettingsWidget::scheduleLibrashaderParamsRebuild);
	connect(m_post.librashaderParamSearch, &QLineEdit::textChanged, this, &PostProcessingSettingsWidget::onLibrashaderParamSearchChanged);
	connect(m_post.librashaderGroup, &QGroupBox::toggled, this, &PostProcessingSettingsWidget::rebuildLibrashaderParamsUi);
#ifdef ENABLE_LIBRASHADER
	connect(m_post.librashaderPrevPage, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderPrevPage);
	connect(m_post.librashaderNextPage, &QPushButton::clicked, this, &PostProcessingSettingsWidget::onLibrashaderNextPage);
#endif
	m_librashader_rebuild_timer = new QTimer(this);
	m_librashader_rebuild_timer->setSingleShot(true);
	m_librashader_rebuild_timer->setInterval(200);
	connect(m_librashader_rebuild_timer, &QTimer::timeout, this, &PostProcessingSettingsWidget::rebuildLibrashaderParamsUi);

	connect(m_post.shadeBoost, &QCheckBox::checkStateChanged, this, &PostProcessingSettingsWidget::onShadeBoostChanged);
	onShadeBoostChanged();

	updateLibrashaderParamsVisibility();
	rebuildLibrashaderParamsUi();

	dialog()->registerWidgetHelp(m_post.librashaderGroup, tr("Shader Preset (librashader)"), tr("Unchecked"),
#ifdef ENABLE_LIBRASHADER
		tr("Enables a shader preset chain using librashader. Some presets may impact performance."));
#else
		tr("librashader support is not compiled into this build."));
#endif

	dialog()->registerWidgetHelp(m_post.librashaderPreset, tr("Preset Path"), tr(""),
		tr("Path to the .slangp shader preset file to load."));

	dialog()->registerWidgetHelp(m_post.librashaderRestoreDefaults, tr("Restore Defaults"), tr(""),
		dialog()->isPerGameSettings() ?
			tr("Resets shader parameters to the preset defaults. Only values that differ from the global saved settings are stored as per-game overrides.") :
			tr("Resets all shader parameters for the current preset to their default values."));

	dialog()->registerWidgetHelp(m_post.librashaderUseGlobalParams, tr("Use Global Parameters"), tr(""),
		tr("Removes per-game shader parameter overrides for the current preset and uses the global saved values instead."));

	dialog()->registerWidgetHelp(m_post.librashaderPassesSectionLabel, tr("Passes"), tr(""),
		tr("Lists shader passes in the order they are declared in the .slangp preset file."));
	dialog()->registerWidgetHelp(m_post.librashaderParamSearch, tr("Parameter Search"), tr(""),
		tr("Filters adjustable parameters by name or description."));

	dialog()->registerWidgetHelp(m_post.librashaderParametersSectionLabel, tr("Parameters"), tr(""),
		tr("Runtime-adjustable parameters exposed by the preset. Global settings save values per preset path; per-game settings save overrides for this game."));

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

void PostProcessingSettingsWidget::onLibrashaderBrowse()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Select Shader Preset"), QString(),
		tr("Shader Presets (*.slangp);;All Files (*.*)"));
	if (!path.isEmpty())
	{
		m_post.librashaderPreset->setText(path);
		rebuildLibrashaderParamsUi();
	}
}

void PostProcessingSettingsWidget::onLibrashaderClear()
{
	m_post.librashaderPreset->clear();
	rebuildLibrashaderParamsUi();
}

void PostProcessingSettingsWidget::scheduleLibrashaderParamsRebuild()
{
	if (m_librashader_rebuild_timer)
		m_librashader_rebuild_timer->start();
}

void PostProcessingSettingsWidget::updateLibrashaderParamsVisibility()
{
	const bool show = m_post.librashaderGroup->isChecked();
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	const bool has_preset = show && !preset_path.isEmpty() && QFile::exists(preset_path);
	m_post.librashaderPassesSectionLabel->setVisible(show);
	m_post.librashaderPassesSectionLabel->setEnabled(has_preset);
	m_post.librashaderPassList->setVisible(show);
	m_post.librashaderPassList->setEnabled(has_preset);
	m_post.librashaderSeparator->setVisible(show);
	m_post.librashaderParametersSectionLabel->setVisible(show);
	m_post.librashaderParametersSectionLabel->setEnabled(has_preset);
	m_post.librashaderParamSearch->setVisible(show);
	m_post.librashaderParamSearch->setEnabled(has_preset);
	m_post.librashaderPrevPage->setVisible(show && has_preset);
	m_post.librashaderPrevPage->setEnabled(has_preset);
	m_post.librashaderPageLabel->setVisible(show && has_preset);
	m_post.librashaderPageLabel->setEnabled(has_preset);
	m_post.librashaderNextPage->setVisible(show && has_preset);
	m_post.librashaderNextPage->setEnabled(has_preset);
	m_post.librashaderParamsScrollArea->setVisible(show);
	m_post.librashaderParamsScrollArea->setEnabled(has_preset);
#ifdef ENABLE_LIBRASHADER
	m_post.librashaderRestoreDefaults->setVisible(show);
	m_post.librashaderRestoreDefaults->setEnabled(false);
	m_post.librashaderUseGlobalParams->setVisible(show && dialog()->isPerGameSettings());
	m_post.librashaderUseGlobalParams->setEnabled(false);
#else
	m_post.librashaderRestoreDefaults->setVisible(false);
	m_post.librashaderUseGlobalParams->setVisible(false);
#endif
}

void PostProcessingSettingsWidget::clearLibrashaderParamsLayout()
{
	qDeleteAll(m_post.librashaderParamsContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));

	QLayout* layout = m_post.librashaderParamsLayout;
	while (QLayoutItem* item = layout->takeAt(0))
	{
		delete item;
	}
}

void PostProcessingSettingsWidget::commitLibrashaderParams()
{
#ifdef ENABLE_LIBRASHADER
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	if (preset_path.isEmpty())
		return;

	if (!SaveLibrashaderParams(preset_path.toStdString(), m_librashader_params, librashaderContext()))
	{
		QMessageBox::warning(this, tr("Shader Parameters"), tr("Failed to save shader parameters."));
		return;
	}

	applyLibrashaderChanges();
	updateUseGlobalParamsButton();
#endif
}

LibrashaderParamsContext PostProcessingSettingsWidget::librashaderContext() const
{
	if (dialog()->isPerGameSettings())
		return LibrashaderParamsContext{dialog()->getSerial(), dialog()->getDiscCRC(), true};

	return {};
}

void PostProcessingSettingsWidget::onLibrashaderUseGlobalParams()
{
#ifdef ENABLE_LIBRASHADER
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	if (preset_path.isEmpty())
		return;

	const LibrashaderParamsContext context = librashaderContext();
	if (!context.IsPerGame())
		return;

	if (!ClearLibrashaderGameOverrides(preset_path.toStdString(), context))
		return;

	startLibrashaderLoad(preset_path);

	applyLibrashaderChanges();
#endif
}

void PostProcessingSettingsWidget::updateUseGlobalParamsButton()
{
#ifdef ENABLE_LIBRASHADER
	if (!dialog()->isPerGameSettings())
		return;

	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	const bool has_preset = m_post.librashaderGroup->isChecked() && !preset_path.isEmpty();
	m_post.librashaderUseGlobalParams->setEnabled(has_preset && HasLibrashaderGameOverrides(preset_path.toStdString(), librashaderContext()));
#else
	(void)0;
#endif
}

void PostProcessingSettingsWidget::onLibrashaderRestoreDefaults()
{
#ifdef ENABLE_LIBRASHADER
	const QString preset_path = m_post.librashaderPreset->text().trimmed();
	if (preset_path.isEmpty())
		return;

	if (!ResetLibrashaderParams(preset_path.toStdString(), m_librashader_params, librashaderContext()))
	{
		QMessageBox::warning(this, tr("Restore Defaults"), tr("Failed to restore shader parameters."));
		return;
	}

	renderLibrashaderParamsPage();

	applyLibrashaderChanges();
	updateUseGlobalParamsButton();
#endif
}

void PostProcessingSettingsWidget::populateLibrashaderPassList(const QString& preset_path)
{
	m_post.librashaderPassList->clear();
	if (preset_path.isEmpty() || !QFile::exists(preset_path))
		return;

	setLibrashaderPassList(GetSlangpPassNames(preset_path.toStdString()));
}

void PostProcessingSettingsWidget::setLibrashaderPassList(const std::vector<std::string>& names)
{
	QSignalBlocker blocker(m_post.librashaderPassList);
	m_post.librashaderPassList->clear();

	for (const std::string& name : names)
		new QListWidgetItem(QString::fromStdString(name), m_post.librashaderPassList);

	if (m_post.librashaderPassList->count() > 0)
		m_post.librashaderPassList->setCurrentRow(0);
}

#ifdef ENABLE_LIBRASHADER
void PostProcessingSettingsWidget::startLibrashaderLoad(const QString& preset_path)
{
	const u64 request_id = ++m_librashader_load_request_id;
	const std::string preset_path_std = preset_path.toStdString();
	const LibrashaderParamsContext context = librashaderContext();
	QFuture<LibrashaderPresetLoad> future = QtConcurrent::run([preset_path_std, context]() {
		return LoadLibrashaderPreset(preset_path_std, context);
	});

	future.then(this, [this, request_id, preset_path](LibrashaderPresetLoad result) {
		applyLibrashaderLoad(request_id, preset_path, std::move(result));
	});
}

void PostProcessingSettingsWidget::applyLibrashaderLoad(u64 request_id, const QString& preset_path,
	LibrashaderPresetLoad&& result)
{
	if (request_id != m_librashader_load_request_id)
		return;
	if (!m_post.librashaderGroup->isChecked())
		return;
	if (m_post.librashaderPreset->text().trimmed() != preset_path)
		return;

	clearLibrashaderParamsLayout();
	QVBoxLayout* const layout = m_post.librashaderParamsLayout;
	const auto showMessage = [layout](const QString& text) {
		QLabel* msg = new QLabel(text);
		msg->setWordWrap(true);
		layout->addWidget(msg);
	};

	setLibrashaderPassList(result.pass_names);
	if (!result.success)
	{
		clearLibrashaderState();

		switch (result.error)
		{
			case LibrashaderLoadError::PresetFailed:
				showMessage(tr("Could not load the shader preset. Check that the path is valid and points to a .slangp file."));
				break;
			case LibrashaderLoadError::ParamsFailed:
				showMessage(tr("Could not read shader parameters from this preset."));
				break;
			default:
				break;
		}
		return;
	}

	m_librashader_params = std::move(result.parameters);
	m_librashader_page_index = 0;
	m_post.librashaderRestoreDefaults->setEnabled(true);
	updateUseGlobalParamsButton();
	renderLibrashaderParamsPage();
}

void PostProcessingSettingsWidget::renderLibrashaderParamsPage()
{
	clearLibrashaderParamsLayout();
	QVBoxLayout* const layout = m_post.librashaderParamsLayout;

	if (m_librashader_params.empty())
	{
		m_post.librashaderPageLabel->setText(tr("Page 1/1"));
		m_post.librashaderPrevPage->setVisible(false);
		m_post.librashaderNextPage->setVisible(false);
		QLabel* msg = new QLabel(tr("This preset does not expose any adjustable parameters."));
		msg->setWordWrap(true);
		layout->addWidget(msg);
		layout->addStretch(1);
		return;
	}

	const LibrashaderVisiblePage visible = GetVisibleLibrashaderPage(
		m_librashader_params, m_post.librashaderParamSearch->text().trimmed().toStdString(), m_librashader_page_index);
	m_librashader_page_index = visible.page.page_index;

	if (visible.indices.empty())
	{
		m_post.librashaderPageLabel->setText(tr("Page 1/1 (0 parameters)"));
		m_post.librashaderPrevPage->setVisible(false);
		m_post.librashaderNextPage->setVisible(false);
		QLabel* msg = new QLabel(tr("No parameters match the current search."));
		msg->setWordWrap(true);
		layout->addWidget(msg);
		layout->addStretch(1);
		return;
	}

	const LibrashaderPage& page = visible.page;

	const QString page_text = tr("Page %1/%2 (%3 parameters)")
	                              .arg(static_cast<int>(page.page_index + 1))
	                              .arg(static_cast<int>(page.total_pages))
	                              .arg(static_cast<int>(page.total_params));
	m_post.librashaderPageLabel->setText(page_text);
	m_post.librashaderPrevPage->setVisible(m_post.librashaderGroup->isChecked() && page.total_pages > 1);
	m_post.librashaderNextPage->setVisible(m_post.librashaderGroup->isChecked() && page.total_pages > 1);
	m_post.librashaderPrevPage->setEnabled(page.page_index > 0);
	m_post.librashaderNextPage->setEnabled((page.page_index + 1) < page.total_pages);

	for (size_t visible_index = page.start; visible_index < page.end; ++visible_index)
	{
		const size_t index = visible.indices[visible_index];
		const LibrashaderParam& p = m_librashader_params[index];
		const QString label = QString::fromStdString(p.GetLabel());
		const QString id_tooltip = label != QString::fromStdString(p.name) ? QString::fromStdString(p.name) : QString();

		switch (p.GetControl())
		{
			case LibrashaderParam::Control::Spacer:
				continue;

			case LibrashaderParam::Control::Label:
			{
				QLabel* const heading = new QLabel(label);
				heading->setWordWrap(true);
				if (!id_tooltip.isEmpty())
					heading->setToolTip(id_tooltip);
				layout->addWidget(heading);
			}
			break;

			case LibrashaderParam::Control::Bool:
			{
				QCheckBox* const cb = new QCheckBox(label);
				cb->setChecked(p.GetBoolValue());
				cb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
				if (!id_tooltip.isEmpty())
					cb->setToolTip(id_tooltip);
				layout->addWidget(cb);
				connect(cb, &QCheckBox::checkStateChanged, this, [this, index](Qt::CheckState state) {
					m_librashader_params[index].SetBoolValue(state == Qt::Checked);
					commitLibrashaderParams();
				});
			}
			break;

			case LibrashaderParam::Control::Float:
			{
				QLabel* const name_label = new QLabel(label);
				name_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
				name_label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
				name_label->setWordWrap(true);
				if (!id_tooltip.isEmpty())
					name_label->setToolTip(id_tooltip);

				QHBoxLayout* const slider_layout = new QHBoxLayout();
				slider_layout->setContentsMargins(0, 0, 0, 0);
				slider_layout->setSpacing(8);

				QSlider* const slider = new QSlider(Qt::Horizontal);
				slider->setRange(0, LIBRASHADER_SLIDER_STEPS);
				slider->setValue(p.ToSlider(p.value));
				slider->setSingleStep(LIBRASHADER_SLIDER_STEPS / 100);
				slider->setPageStep(LIBRASHADER_SLIDER_STEPS / 10);
				slider->setMinimumHeight(22);
				slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
				slider->setFocusPolicy(Qt::StrongFocus);

				QLabel* const value_label = new QLabel(QString::fromStdString(p.FormatValue(p.value)));
				value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
				value_label->setFixedWidth(62);
				value_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

				slider_layout->addWidget(slider, 1);
				slider_layout->addWidget(value_label, 0);

				layout->addWidget(name_label);
				layout->addLayout(slider_layout);

				connect(slider, &QSlider::valueChanged, this, [this, index, slider, value_label](int) {
					LibrashaderParam& param = m_librashader_params[index];
					const float current = param.FromSlider(slider->value());
					param.value = current;
					value_label->setText(QString::fromStdString(param.FormatValue(current)));
				});
				connect(slider, &QSlider::sliderReleased, this, &PostProcessingSettingsWidget::commitLibrashaderParams);
			}
			break;
		}
	}

	layout->addStretch(1);
}

void PostProcessingSettingsWidget::onLibrashaderPrevPage()
{
	if (m_librashader_page_index == 0)
		return;
	m_librashader_page_index--;
	renderLibrashaderParamsPage();
}

void PostProcessingSettingsWidget::onLibrashaderNextPage()
{
	const LibrashaderVisiblePage visible = GetVisibleLibrashaderPage(
		m_librashader_params, m_post.librashaderParamSearch->text().trimmed().toStdString(), m_librashader_page_index);
	if ((visible.page.page_index + 1) >= visible.page.total_pages)
		return;
	m_librashader_page_index++;
	renderLibrashaderParamsPage();
}
#endif

void PostProcessingSettingsWidget::rebuildLibrashaderParamsUi()
{
	if (m_librashader_rebuild_timer)
		m_librashader_rebuild_timer->stop();
	updateLibrashaderParamsVisibility();
	clearLibrashaderParamsLayout();
	QVBoxLayout* const layout = m_post.librashaderParamsLayout;
	const auto showMessage = [layout](const QString& text) {
		QLabel* msg = new QLabel(text);
		msg->setWordWrap(true);
		layout->addWidget(msg);
	};

	if (!m_post.librashaderGroup->isChecked())
	{
		clearLibrashaderState();
		m_post.librashaderPassList->clear();
		return;
	}

	const QString preset_path = m_post.librashaderPreset->text().trimmed();
#ifndef ENABLE_LIBRASHADER
	populateLibrashaderPassList(preset_path);
	showMessage(tr("librashader support is not compiled into this build."));
	return;
#else
	if (preset_path.isEmpty())
	{
		clearLibrashaderState();
		m_post.librashaderPassList->clear();
		showMessage(tr("Select a .slangp preset to view runtime parameters."));
		return;
	}

	if (!QFile::exists(preset_path))
	{
		clearLibrashaderState();
		m_post.librashaderPassList->clear();
		showMessage(tr("The selected preset file does not exist."));
		return;
	}

	populateLibrashaderPassList(preset_path);

	m_post.librashaderRestoreDefaults->setEnabled(false);
	m_post.librashaderPageLabel->setText(tr("Loading..."));
	showMessage(tr("Loading shader parameters..."));
	startLibrashaderLoad(preset_path);
#endif
}

void PostProcessingSettingsWidget::onLibrashaderParamSearchChanged(const QString& text)
{
	Q_UNUSED(text);
#ifdef ENABLE_LIBRASHADER
	m_librashader_page_index = 0;
	renderLibrashaderParamsPage();
#endif
}

#ifdef ENABLE_LIBRASHADER
void PostProcessingSettingsWidget::applyLibrashaderChanges()
{
	if (dialog()->getSettingsInterface())
		g_emu_thread->reloadGameSettings();
	else
		g_emu_thread->applySettings();
}
#endif

void PostProcessingSettingsWidget::clearLibrashaderState()
{
	++m_librashader_load_request_id;
#ifdef ENABLE_LIBRASHADER
	m_librashader_params.clear();
	m_librashader_page_index = 0;
	m_post.librashaderRestoreDefaults->setEnabled(false);
	m_post.librashaderPrevPage->setVisible(false);
	m_post.librashaderNextPage->setVisible(false);
	updateUseGlobalParamsButton();
#endif
	m_post.librashaderPageLabel->setText(tr("Page 1/1"));
}

#include "moc_PostProcessingSettingsWidget.cpp"
