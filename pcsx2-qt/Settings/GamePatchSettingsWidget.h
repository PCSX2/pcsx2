// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_GamePatchDetailsWidget.h"
#include "ui_GamePatchSettingsWidget.h"

#include "SettingsWidget.h"

#include "pcsx2/Patch.h"

namespace GameList
{
	struct Entry;
}

class GamePatchDetailsWidget : public QWidget
{
	Q_OBJECT

public:
	GamePatchDetailsWidget(std::string name, const std::string& author, const std::string& description, bool tristate, Qt::CheckState checkState,
		SettingsWindow* dialog, QWidget* parent);
	~GamePatchDetailsWidget();

private Q_SLOTS:
	void onEnabledStateChanged(int state);

private:
	Ui::GamePatchDetailsWidget m_ui;
	SettingsWindow* m_dialog;
	std::string m_name;
};

class GamePatchSettingsWidget : public SettingsWidget
{
	Q_OBJECT

public:
	GamePatchSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	void disableAllPatches();
	~GamePatchSettingsWidget();

private Q_SLOTS:
	void onReloadClicked();

private:
	void reloadList();
	void setUnlabeledPatchesWarningVisibility(bool visible);
	void setGlobalWsPatchNoteVisibility(bool visible);
	void setGlobalNiPatchNoteVisibility(bool visible);

	Ui::GamePatchSettingsWidget m_ui;
};
