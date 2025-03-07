// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"
#include "SavedAddressesModel.h"

#include "common/Console.h"

SavedAddressesModel::SavedAddressesModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

QVariant SavedAddressesModel::data(const QModelIndex& index, int role) const
{
	if (role == Qt::CheckStateRole)
	{
		return QVariant();
	}

	if (role == Qt::DisplayRole || role == Qt::EditRole)
	{
		SavedAddress savedAddress = m_savedAddresses.at(index.row());
		switch (index.column())
		{
			case HeaderColumns::ADDRESS:
				return QString::number(savedAddress.address, 16).toUpper();
			case HeaderColumns::LABEL:
				return savedAddress.label;
			case HeaderColumns::DESCRIPTION:
				return savedAddress.description;
		}
	}
	if (role == Qt::UserRole)
	{
		SavedAddress savedAddress = m_savedAddresses.at(index.row());
		switch (index.column())
		{
			case HeaderColumns::ADDRESS:
				return savedAddress.address;
			case HeaderColumns::LABEL:
				return savedAddress.label;
			case HeaderColumns::DESCRIPTION:
				return savedAddress.description;
		}
	}
	return QVariant();
}

bool SavedAddressesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (role == Qt::CheckStateRole)
	{
		return false;
	}

	if (role == Qt::EditRole)
	{
		SavedAddress addressToEdit = m_savedAddresses.at(index.row());
		if (index.column() == HeaderColumns::ADDRESS)
		{
			bool ok = false;
			const u32 address = value.toString().toUInt(&ok, 16);
			if (ok)
				addressToEdit.address = address;
			else
				return false;
		}
		if (index.column() == HeaderColumns::DESCRIPTION)
			addressToEdit.description = value.toString();
		if (index.column() == HeaderColumns::LABEL)
			addressToEdit.label = value.toString();
		m_savedAddresses.at(index.row()) = addressToEdit;

		emit dataChanged(index, index, QList<int>(role));
		return true;
	}
	else if (role == Qt::UserRole)
	{
		SavedAddress addressToEdit = m_savedAddresses.at(index.row());
		if (index.column() == HeaderColumns::ADDRESS)
		{
			const u32 address = value.toUInt();
			addressToEdit.address = address;
		}
		if (index.column() == HeaderColumns::DESCRIPTION)
			addressToEdit.description = value.toString();
		if (index.column() == HeaderColumns::LABEL)
			addressToEdit.label = value.toString();
		m_savedAddresses.at(index.row()) = addressToEdit;

		emit dataChanged(index, index, QList<int>(role));
		return true;
	}
	return false;
}

QVariant SavedAddressesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
		return QVariant();

	if (role == Qt::DisplayRole)
	{
		switch (section)
		{
			case SavedAddressesModel::ADDRESS:
				return tr("MEMORY ADDRESS");
			case SavedAddressesModel::LABEL:
				return tr("LABEL");
			case SavedAddressesModel::DESCRIPTION:
				return tr("DESCRIPTION");
			default:
				return QVariant();
		}
	}
	if (role == Qt::UserRole)
	{
		switch (section)
		{
			case SavedAddressesModel::ADDRESS:
				return "MEMORY ADDRESS";
			case SavedAddressesModel::LABEL:
				return "LABEL";
			case SavedAddressesModel::DESCRIPTION:
				return "DESCRIPTION";
			default:
				return QVariant();
		}
	}
	return QVariant();
}

Qt::ItemFlags SavedAddressesModel::flags(const QModelIndex& index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void SavedAddressesModel::addRow()
{
	const SavedAddress defaultNewAddress = {0, "Name", "Description"};
	addRow(defaultNewAddress);
}

void SavedAddressesModel::addRow(SavedAddress addresstoSave)
{
	const int newRowIndex = m_savedAddresses.size();
	beginInsertRows(QModelIndex(), newRowIndex, newRowIndex);
	m_savedAddresses.push_back(addresstoSave);
	endInsertRows();
}

bool SavedAddressesModel::removeRows(int row, int count, const QModelIndex& parent)
{
	if (row < 0 || count < 1 || static_cast<size_t>(row + count) > m_savedAddresses.size())
		return false;
	beginRemoveRows(parent, row, row + count - 1);
	m_savedAddresses.erase(m_savedAddresses.begin() + row, m_savedAddresses.begin() + row + count);
	endRemoveRows();
	return true;
}

int SavedAddressesModel::rowCount(const QModelIndex&) const
{
	return m_savedAddresses.size();
}

int SavedAddressesModel::columnCount(const QModelIndex&) const
{
	return HeaderColumns::COLUMN_COUNT;
}

void SavedAddressesModel::loadSavedAddressFromFieldList(QStringList fields)
{
	if (fields.size() != SavedAddressesModel::HeaderColumns::COLUMN_COUNT)
	{
		Console.WriteLn("Debugger Saved Addresses Model: Invalid number of columns, skipping");
		return;
	}

	bool ok;
	const u32 address = fields[SavedAddressesModel::HeaderColumns::ADDRESS].toUInt(&ok, 16);
	if (!ok)
	{
		Console.WriteLn("Debugger Saved Addresses Model: Failed to parse address '%s', skipping", fields[SavedAddressesModel::HeaderColumns::ADDRESS].toUtf8().constData());
		return;
	}

	const QString label = fields[SavedAddressesModel::HeaderColumns::LABEL];
	const QString description = fields[SavedAddressesModel::HeaderColumns::DESCRIPTION];
	const SavedAddressesModel::SavedAddress importedAddress = {address, label, description};
	addRow(importedAddress);
}

void SavedAddressesModel::clear()
{
	beginResetModel();
	m_savedAddresses.clear();
	endResetModel();
}
