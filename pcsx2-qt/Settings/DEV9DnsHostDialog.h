// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QDialog>

#include "ui_DEV9DnsHostDialog.h"

#include "DEV9UiCommon.h"

class SettingsWindow;

class DEV9DnsHostDialog : public QDialog
{
	Q_OBJECT

private Q_SLOTS:
	void onOK();
	void onCancel();

public:
	DEV9DnsHostDialog(std::vector<HostEntryUi> hosts, QWidget* parent);
	~DEV9DnsHostDialog();

	std::optional<std::vector<HostEntryUi>> PromptList();

protected:
	bool eventFilter(QObject* object, QEvent* event);

private:
	Ui::DEV9DnsHostDialog m_ui;

	std::vector<HostEntryUi> m_hosts;

	QStandardItemModel* m_ethHost_model;
	QSortFilterProxyModel* m_ethHosts_proxy;
};
