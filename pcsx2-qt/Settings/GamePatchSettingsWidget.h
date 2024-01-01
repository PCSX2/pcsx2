// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>

#include "ui_GamePatchDetailsWidget.h"
#include "ui_GamePatchSettingsWidget.h"

#include "pcsx2/Patch.h"

namespace GameList
{
	struct Entry;
}

class SettingsWindow;

class GamePatchDetailsWidget : public QWidget
{
	Q_OBJECT

public:
	GamePatchDetailsWidget(std::string name, const std::string& author, const std::string& description, bool enabled,
		SettingsWindow* dialog, QWidget* parent);
	~GamePatchDetailsWidget();

private Q_SLOTS:
	void onEnabledStateChanged(int state);

private:
	Ui::GamePatchDetailsWidget m_ui;
	SettingsWindow* m_dialog;
	std::string m_name;
};

class GamePatchSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	GamePatchSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	void disableAllPatches();
	~GamePatchSettingsWidget();

private Q_SLOTS:
	void onReloadClicked();

private:
	void reloadList();
	void setUnlabeledPatchesWarningVisibility(bool visible);

	Ui::GamePatchSettingsWidget m_ui;
	SettingsWindow* m_dialog;
};
