// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_FolderSettingsWidget.h"

class SettingsWindow;

class FolderSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	FolderSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~FolderSettingsWidget();

private:
	Ui::FolderSettingsWidget m_ui;
};
