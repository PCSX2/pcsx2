/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "FolderSettingsWidget.h"
#include "SettingWidgetBinder.h"
#include "SettingsDialog.h"

FolderSettingsWidget::FolderSettingsWidget(SettingsDialog* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cache, m_ui.cacheBrowse, m_ui.cacheOpen, m_ui.cacheReset, "Folders", "Cache", Path::Combine(EmuFolders::DataRoot, "cache"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.cheats, m_ui.cheatsBrowse, m_ui.cheatsOpen, m_ui.cheatsReset, "Folders", "Cheats", Path::Combine(EmuFolders::DataRoot, "cheats"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.covers, m_ui.coversBrowse, m_ui.coversOpen, m_ui.coversReset, "Folders", "Covers", Path::Combine(EmuFolders::DataRoot, "covers"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.snapshots, m_ui.snapshotsBrowse, m_ui.snapshotsOpen, m_ui.snapshotsReset, "Folders", "Snapshots", Path::Combine(EmuFolders::DataRoot, "snaps"));
	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.saveStates, m_ui.saveStatesBrowse, m_ui.saveStatesOpen, m_ui.saveStatesReset, "Folders", "SaveStates", Path::Combine(EmuFolders::DataRoot, "sstates"));
}

FolderSettingsWidget::~FolderSettingsWidget() = default;
