// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "SettingsWidget.h"
#include "ui_PostProcessingSettingsWidget.h"

#include "pcsx2/GS/LibrashaderParams.h"

class QCheckBox;
class QLabel;
class QSlider;
class QTimer;

class PostProcessingSettingsWidget final : public SettingsWidget
{
	Q_OBJECT

public:
	PostProcessingSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~PostProcessingSettingsWidget();

private Q_SLOTS:
	void onShadeBoostChanged();
	void onLibrashaderBrowse();
	void onLibrashaderClear();
	void onLibrashaderRestoreDefaults();
	void onLibrashaderUseGlobalParams();
	void rebuildLibrashaderParamsUi();
	void onLibrashaderParamSearchChanged(const QString& text);

private:
	void commitLibrashaderParams();
	LibrashaderParamsContext librashaderContext() const;
	void clearLibrashaderParamsLayout();
	void clearLibrashaderState();
	void updateLibrashaderParamsVisibility();
	void updateUseGlobalParamsButton();
	void populateLibrashaderPassList(const QString& preset_path);
	void setLibrashaderPassList(const std::vector<std::string>& names);
	void scheduleLibrashaderParamsRebuild();
	u64 m_librashader_load_request_id = 0;

#ifdef ENABLE_LIBRASHADER
	void startLibrashaderLoad(const QString& preset_path);
	void applyLibrashaderLoad(u64 request_id, const QString& preset_path, LibrashaderPresetLoad&& result);
	void renderLibrashaderParamsPage();
	void onLibrashaderPrevPage();
	void onLibrashaderNextPage();
	void applyLibrashaderChanges();
	std::vector<LibrashaderParam> m_librashader_params;
	size_t m_librashader_page_index = 0;
#endif

	QTimer* m_librashader_rebuild_timer = nullptr;
	Ui::PostProcessingSettingsWidget m_post;
};
