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

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItemModel>

#include "ui_DEV9DnsHostDialog.h"

#include "DEV9UiCommon.h"

class SettingsDialog;

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
