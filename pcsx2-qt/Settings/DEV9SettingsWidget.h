// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QStandardItemModel>

#include "ui_DEV9SettingsWidget.h"

#include "DEV9UiCommon.h"
#include "DEV9DnsHostDialog.h"
#include "DEV9/net.h"

class SettingsWindow;

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
	void onEthHostPerGame();
	void onEthHostEdit(QStandardItem* item);

	void onHddEnabledChanged(int state);
	void onHddBrowseFileClicked();
	void onHddFileTextChange();
	void onHddFileEdit();
	void onHddSizeSlide(int i);
	void onHddSizeAccessorSpin();
	void onHddLBA48Changed(int state);
	void onHddCreateClicked();

public:
	DEV9SettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~DEV9SettingsWidget();

protected:
	void showEvent(QShowEvent* event);
	bool eventFilter(QObject* object, QEvent* event);

private:
	void AddAdapter(const AdapterEntry& adapter);
	void RefreshHostList();
	int CountHostsConfig();
	std::optional<std::vector<HostEntryUi>> ListHostsConfig();
	std::vector<HostEntryUi> ListBaseHostsConfig();
	void AddNewHostConfig(const HostEntryUi& host);
	void DeleteHostConfig(int index);

	void UpdateHddSizeUIEnabled();
	void UpdateHddSizeUIValues();

	SettingsWindow* m_dialog;

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
