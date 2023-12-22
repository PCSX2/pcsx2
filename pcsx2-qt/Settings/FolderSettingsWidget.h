// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
