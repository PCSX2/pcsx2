// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "FolderSettingsWidget.h"
#include "pcsx2/GS/GS.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include <QtWidgets/QMessageBox>

FolderSettingsWidget::FolderSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cache, m_ui.cacheBrowse, m_ui.cacheOpen, m_ui.cacheReset, "Folders", "Cache", Path::Combine(EmuFolders::DataRoot, "cache"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cheats, m_ui.cheatsBrowse, m_ui.cheatsOpen, m_ui.cheatsReset, "Folders", "Cheats", Path::Combine(EmuFolders::DataRoot, "cheats"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.covers, m_ui.coversBrowse, m_ui.coversOpen, m_ui.coversReset, "Folders", "Covers", Path::Combine(EmuFolders::DataRoot, "covers"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.snapshots, m_ui.snapshotsBrowse, m_ui.snapshotsOpen, m_ui.snapshotsReset, "Folders", "Snapshots", Path::Combine(EmuFolders::DataRoot, "snaps"));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.organizeScreenshotsByGame, "EmuCore/GS", "OrganizeScreenshotsByGame", false);
	connect(m_ui.organizeScreenshotsByGame, &QCheckBox::checkStateChanged, this, [](int state) {
		GSConfig.OrganizeScreenshotsByGame = (state == Qt::Checked);
	});
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.saveStates, m_ui.saveStatesBrowse, m_ui.saveStatesOpen, m_ui.saveStatesReset, "Folders", "SaveStates", Path::Combine(EmuFolders::DataRoot, "sstates"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.videoDumpingDirectory, m_ui.videoDumpingDirectoryBrowse, m_ui.videoDumpingDirectoryOpen, m_ui.videoDumpingDirectoryReset, "Folders", "Videos", Path::Combine(EmuFolders::DataRoot, "videos"));
	dialog()->registerWidgetHelp(m_ui.organizeScreenshotsByGame, tr("Organize Screenshots by Game"), tr("Unchecked"),
		tr("When enabled, screenshots will be saved in a folder with the game's name, instead of all being saved in the Snapshots folder"));
}

FolderSettingsWidget::~FolderSettingsWidget() = default;
