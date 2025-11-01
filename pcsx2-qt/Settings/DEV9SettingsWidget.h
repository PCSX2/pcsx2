// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_DEV9SettingsWidget.h"

#include "SettingsWidget.h"

#include "DEV9UiCommon.h"
#include "DEV9DnsHostDialog.h"
#include "DEV9/net.h"

#include <QtGui/QStandardItemModel>

class DEV9SettingsWidget : public SettingsWidget
{
	Q_OBJECT

private Q_SLOTS:
	void onEthEnabledChanged(Qt::CheckState state);
	void onEthDeviceTypeChanged(int index);
	void onEthDeviceChanged(int index);
	void onEthDHCPInterceptChanged(Qt::CheckState state);
	void onEthIPChanged(QLineEdit* sender, const char* section, const char* key);
	void onEthAutoChanged(QCheckBox* sender, Qt::CheckState state, QLineEdit* input, const char* section, const char* key);
	void onEthDNSModeChanged(QComboBox* sender, int index, QLineEdit* input, const char* section, const char* key);
	void onEthHostAdd();
	void onEthHostDel();
	void onEthHostExport();
	void onEthHostImport();
	void onEthHostPerGame();
	void onEthHostEdit(QStandardItem* item);
	void onEthPortAdd();
	void onEthPortDel();
	void onEthPortPerGame();
	void onEthPortEdit(QStandardItem* item);

	void onHddEnabledChanged(Qt::CheckState state);
	void onHddBrowseFileClicked();
	void onHddFileTextChange();
	void onHddFileEdit();
	void onHddSizeSlide(int i);
	void onHddSizeAccessorSpin();
	void onHddLBA48Changed(Qt::CheckState state);
	void onHddCreateClicked();

public:
	DEV9SettingsWidget(SettingsWindow* settings_dialog, QWidget* parent);
	~DEV9SettingsWidget();

protected:
	void showEvent(QShowEvent* event);
	bool eventFilter(QObject* object, QEvent* event);

private:
	void AddAdapter(const AdapterEntry& adapter);
	void LoadAdapters();
	void RefreshHostList();
	int CountHostsConfig();
	std::optional<std::vector<HostEntryUi>> ListHostsConfig();
	std::vector<HostEntryUi> ListBaseHostsConfig();
	void AddNewHostConfig(const HostEntryUi& host);
	void DeleteHostConfig(int index);

	void RefreshPortList();
	int CountPortsConfig();
	std::optional<std::vector<PortEntryUi>> ListPortsConfig();
	std::vector<PortEntryUi> ListBasePortsConfig();
	void AddNewPortConfig(const PortEntryUi& port);
	void DeletePortConfig(int index);

	void UpdateHddSizeUIEnabled();
	void UpdateHddSizeUIValues();

	Ui::DEV9SettingsWidget m_ui;

	bool m_firstShow{true};

	QStandardItemModel* m_ethHost_model;
	QSortFilterProxyModel* m_ethHosts_proxy;

	QStandardItemModel* m_ethPort_model;
	QSortFilterProxyModel* m_ethPorts_proxy;

	bool m_adaptersLoaded{false};
	std::vector<Pcsx2Config::DEV9Options::NetApi> m_api_list;
	std::vector<const char*> m_api_namelist;
	std::vector<const char*> m_api_valuelist;
	std::vector<std::vector<AdapterEntry>> m_adapter_list;

	AdapterOptions m_adapter_options{AdapterOptions::None};

	//Use by per-game ui only
	Pcsx2Config::DEV9Options::NetApi m_global_api{Pcsx2Config::DEV9Options::NetApi::Unset};
};
