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

#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItemModel>

#include "ui_DEV9SettingsWidget.h"

#include "DEV9UiCommon.h"
#include "DEV9DnsHostDialog.h"
#include "DEV9/net.h"

class SettingsDialog;

class DEV9SettingsWidget : public QWidget
{
	Q_OBJECT

private Q_SLOTS:
	void onEthEnabledChanged(int state);
	void onEthDeviceTypeChanged(int index);
	void onEthDeviceChanged(int index);
	void onEthDHCPInterceptChanged(int state);
	void onEthIPChanged(QLineEdit* sender, const char* section, const char* key);
	void onEthAutoChanged(QCheckBox* sender, int state, QLineEdit* input, const char* section, const char* key);
	void onEthDNSModeChanged(QComboBox* sender, int index, QLineEdit* input, const char* section, const char* key);
	void onEthHostAdd();
	void onEthHostDel();
	void onEthHostExport();
	void onEthHostImport();
	void onEthHostEdit(QStandardItem* item);

	void onHddEnabledChanged(int state);
	void onHddBrowseFileClicked();
	void onHddFileEdit();
	void onHddSizeSlide(int i);
	void onHddSizeSpin(int i);
	void onHddCreateClicked();

public:
	DEV9SettingsWidget(SettingsDialog* dialog, QWidget* parent);
	~DEV9SettingsWidget();

protected:
	void showEvent(QShowEvent* event);
	bool eventFilter(QObject* object, QEvent* event);

private:
	void AddAdapter(const AdapterEntry& adapter);
	void RefreshHostList();
	int CountHostsConfig();
	std::vector<HostEntryUi> ListHostsConfig();
	void AddNewHostConfig(const HostEntryUi& host);
	void DeleteHostConfig(int index);

	SettingsDialog* m_dialog;

	Ui::DEV9SettingsWidget m_ui;

	QStandardItemModel* m_ethHost_model;
	QSortFilterProxyModel* m_ethHosts_proxy;

	std::vector<Pcsx2Config::DEV9Options::NetApi> m_api_list;
	std::vector<const char*> m_api_namelist;
	std::vector<const char*> m_api_valuelist;
	std::vector<std::vector<AdapterEntry>> m_adapter_list;

	AdapterOptions m_adapter_options{AdapterOptions::None};

	//Use by per-game ui only
	Pcsx2Config::DEV9Options::NetApi m_global_api{Pcsx2Config::DEV9Options::NetApi::Unset};
};
