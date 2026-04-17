// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "SettingsWidget.h"
#include "ui_PostProcessingSettingsWidget.h"

class PostProcessingSettingsWidget final : public SettingsWidget
{
	Q_OBJECT

public:
	PostProcessingSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~PostProcessingSettingsWidget();

private Q_SLOTS:
	void onShadeBoostChanged();

private:
	Ui::PostProcessingSettingsWidget m_post;
};
