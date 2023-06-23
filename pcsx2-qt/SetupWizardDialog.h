/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#pragma once

#include "Settings/BIOSSettingsWidget.h"

#include "ui_SetupWizardDialog.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtWidgets/QDialog>

#include "common/Pcsx2Defs.h"

class SetupWizardDialog final : public QDialog
{
	Q_OBJECT

public:
	SetupWizardDialog();
	~SetupWizardDialog();

private Q_SLOTS:
	bool canShowNextPage();
	void previousPage();
	void nextPage();
	void confirmCancel();

	void themeChanged();
	void languageChanged();

	void refreshBiosList();
	void biosListItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous);
	void listRefreshed(const QVector<BIOSInfo>& items);

	void onDirectoryListContextMenuRequested(const QPoint& point);
	void onAddSearchDirectoryButtonClicked();
	void onRemoveSearchDirectoryButtonClicked();
	void refreshDirectoryList();
	void resizeDirectoryListColumns();

	void onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices);
	void onInputDeviceConnected(const QString& identifier, const QString& device_name);
	void onInputDeviceDisconnected(const QString& identifier);

protected:
	void resizeEvent(QResizeEvent* event);

private:
	enum Page : u32
	{
		Page_Language,
		Page_BIOS,
		Page_GameList,
		Page_Controller,
		Page_Complete,
		Page_Count,
	};

	void setupUi();
	void setupLanguagePage();
	void setupBIOSPage();
	void setupGameListPage();
	void setupControllerPage();

	void pageChangedTo(int page);
	void updatePageLabels(int prev_page);
	void updatePageButtons();

	void addPathToTable(const std::string& path, bool recursive);

	void openAutomaticMappingMenu(u32 port, QLabel* update_label);
	void doDeviceAutomaticBinding(u32 port, QLabel* update_label, const QString& device);

	Ui::SetupWizardDialog m_ui;

	std::array<QLabel*, Page_Count> m_page_labels;

	BIOSSettingsWidget::RefreshThread* m_bios_refresh_thread = nullptr;

	QList<QPair<QString, QString>> m_device_list;
};
