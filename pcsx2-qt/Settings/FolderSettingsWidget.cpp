// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtWidgets/QMessageBox>

#include "FolderSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

FolderSettingsWidget::FolderSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cache, m_ui.cacheBrowse, m_ui.cacheOpen, m_ui.cacheReset, "Folders", "Cache", Path::Combine(EmuFolders::DataRoot, "cache"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cheats, m_ui.cheatsBrowse, m_ui.cheatsOpen, m_ui.cheatsReset, "Folders", "Cheats", Path::Combine(EmuFolders::DataRoot, "cheats"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.covers, m_ui.coversBrowse, m_ui.coversOpen, m_ui.coversReset, "Folders", "Covers", Path::Combine(EmuFolders::DataRoot, "covers"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.snapshots, m_ui.snapshotsBrowse, m_ui.snapshotsOpen, m_ui.snapshotsReset, "Folders", "Snapshots", Path::Combine(EmuFolders::DataRoot, "snaps"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.saveStates, m_ui.saveStatesBrowse, m_ui.saveStatesOpen, m_ui.saveStatesReset, "Folders", "SaveStates", Path::Combine(EmuFolders::DataRoot, "sstates"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.videoDumpingDirectory, m_ui.videoDumpingDirectoryBrowse, m_ui.videoDumpingDirectoryOpen, m_ui.videoDumpingDirectoryReset, "Folders", "Videos", Path::Combine(EmuFolders::DataRoot, "videos"));
}

FolderSettingsWidget::~FolderSettingsWidget() = default;
