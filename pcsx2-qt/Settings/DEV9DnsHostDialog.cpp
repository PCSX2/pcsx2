// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtGui/QStandardItemModel>
#include <algorithm>

#include "common/StringUtil.h"
#include "DEV9DnsHostDialog.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

//Figure out lists
//On export, we take list from settings (or are given it from the DEV9 panel)
//We display, then export

//On import, we read file
//we display, then pass list back to main DEV9 panel

DEV9DnsHostDialog::DEV9DnsHostDialog(std::vector<HostEntryUi> hosts, QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	m_ethHost_model = new QStandardItemModel(0, 5, m_ui.hostList);

	QStringList headers;
	headers.push_back(tr("Selected"));
	headers.push_back(tr("Name"));
	headers.push_back(tr("Hostname"));
	headers.push_back(tr("Address"));
	headers.push_back(tr("Enabled"));
	m_ethHost_model->setHorizontalHeaderLabels(headers);

	m_ethHosts_proxy = new QSortFilterProxyModel(m_ui.hostList);
	m_ethHosts_proxy->setSourceModel(m_ethHost_model);

	m_ui.hostList->setModel(m_ethHosts_proxy);
	m_ui.hostList->setItemDelegateForColumn(3, new IPItemDelegate(m_ui.hostList));

	for (size_t i = 0; i < hosts.size(); i++)
	{
		HostEntryUi entry = hosts[i];
		const int row = m_ethHost_model->rowCount();
		m_ethHost_model->insertRow(row);

		QSignalBlocker sb(m_ethHost_model);

		QStandardItem* includeItem = new QStandardItem();
		includeItem->setEditable(false);
		includeItem->setCheckable(true);
		includeItem->setCheckState(Qt::CheckState::Checked);
		m_ethHost_model->setItem(row, 0, includeItem);

		QStandardItem* nameItem = new QStandardItem();
		nameItem->setText(QString::fromStdString(entry.Desc));
		nameItem->setEnabled(false);
		m_ethHost_model->setItem(row, 1, nameItem);

		QStandardItem* urlItem = new QStandardItem();
		urlItem->setText(QString::fromStdString(entry.Url));
		urlItem->setEnabled(false);
		m_ethHost_model->setItem(row, 2, urlItem);

		QStandardItem* addressItem = new QStandardItem();
		addressItem->setText(QString::fromStdString(entry.Address));
		addressItem->setEnabled(false);
		m_ethHost_model->setItem(row, 3, addressItem);

		QStandardItem* enabledItem = new QStandardItem();
		enabledItem->setEditable(false);
		enabledItem->setCheckable(true);
		enabledItem->setCheckState(entry.Enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
		enabledItem->setEnabled(false);
		m_ethHost_model->setItem(row, 4, enabledItem);
	}

	m_ui.hostList->sortByColumn(1, Qt::AscendingOrder);

	m_ui.hostList->installEventFilter(this);

	connect(m_ui.btnOK, &QPushButton::clicked, this, &DEV9DnsHostDialog::onOK);
	connect(m_ui.btnCancel, &QPushButton::clicked, this, &DEV9DnsHostDialog::onCancel);

	m_hosts = hosts;
}

std::optional<std::vector<HostEntryUi>> DEV9DnsHostDialog::PromptList()
{
	int ret = exec();

	if (ret != Accepted)
		return std::nullopt;

	std::vector<HostEntryUi> selectedList;

	for (int i = 0; i < m_ethHost_model->rowCount(); i++)
	{
		if (m_ethHost_model->item(i, 0)->checkState() == Qt::CheckState::Checked)
			selectedList.push_back(m_hosts[i]);
	}

	return selectedList;
}

void DEV9DnsHostDialog::onOK()
{
	accept();
}

void DEV9DnsHostDialog::onCancel()
{
	reject();
}

bool DEV9DnsHostDialog::eventFilter(QObject* object, QEvent* event)
{
	if (object == m_ui.hostList)
	{
		//Check isVisible to avoind an unnessecery call to ResizeColumnsForTableView()
		if (event->type() == QEvent::Resize && m_ui.hostList->isVisible())
			QtUtils::ResizeColumnsForTableView(m_ui.hostList, {80, -1, 170, 90, 80});
		else if (event->type() == QEvent::Show)
			QtUtils::ResizeColumnsForTableView(m_ui.hostList, {80, -1, 170, 90, 80});
	}
	return false;
}

DEV9DnsHostDialog::~DEV9DnsHostDialog() = default;
