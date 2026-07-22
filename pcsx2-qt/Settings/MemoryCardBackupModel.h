// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SIO/Memcard/MemoryCardBackup.h"

#include <QtCore/QAbstractTableModel>

class MemoryCardBackupModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	MemoryCardBackupModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	void refresh();

private:
	enum Columnn
	{
		CARD_NAME,
		CREATION_TIME,
		COMPRESSED_SIZE,
		COLUMN_COUNT
	};

	std::vector<MemoryCardBackup::BackupMetadata> m_backups;
};
