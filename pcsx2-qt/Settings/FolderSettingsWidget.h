// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_FolderSettingsWidget.h"

#include "SettingsWidget.h"

class FolderSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	FolderSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~FolderSettingsWidget();

private:
	Ui::FolderSettingsWidget m_ui;
};
