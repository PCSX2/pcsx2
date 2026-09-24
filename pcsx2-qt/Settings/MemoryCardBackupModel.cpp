// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryCardBackupModel.h"

#include "Config.h"

MemoryCardBackupModel::MemoryCardBackupModel(QObject* parent)
	: QAbstractTableModel(parent)
{
	refresh();
}

int MemoryCardBackupModel::rowCount(const QModelIndex& parent) const
{
	return static_cast<int>(m_backups.size());
}

int MemoryCardBackupModel::columnCount(const QModelIndex& parent) const
{
	return COLUMN_COUNT;
}

QVariant MemoryCardBackupModel::data(const QModelIndex& index, int role) const
{
	size_t row = static_cast<size_t>(index.row());
	if (row >= m_backups.size())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	const MemoryCardBackup::BackupMetadata& backup = m_backups[row];

	switch (index.column())
	{
		case CARD_NAME:
			return QString::fromStdString(backup.name);
		case CREATION_TIME:
			return QString::number(backup.creation_time);
		case COMPRESSED_SIZE:
			return QString::number(backup.backup_size);
	}

	return QVariant();
}

QVariant MemoryCardBackupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	switch (section)
	{
		case CARD_NAME:
			return tr("Card Name");
		case CREATION_TIME:
			return tr("Creation Time");
		case COMPRESSED_SIZE:
			return tr("Compressed Size");
	}

	return QVariant();
}

void MemoryCardBackupModel::refresh()
{
	beginResetModel();
	m_backups = MemoryCardBackup::EnumerateBackups(EmuFolders::MemoryCardBackups);
	endResetModel();
}

#include "moc_MemoryCardBackupModel.cpp"
