// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QHeaderView>

#include "DebugTools/DebugInterface.h"

class SavedAddressesModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	struct SavedAddress
	{
		u32 address;
		QString label;
		QString description;
	};

	enum HeaderColumns: int
	{
		ADDRESS = 0,
		LABEL,
		DESCRIPTION,
		COLUMN_COUNT
	};

	static constexpr QHeaderView::ResizeMode HeaderResizeModes[HeaderColumns::COLUMN_COUNT] =
	{
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::Stretch,
	};

	explicit SavedAddressesModel(DebugInterface& cpu, QObject* parent = nullptr);
	QVariant data(const QModelIndex& index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	void addRow();
	void addRow(SavedAddress addresstoSave);
	bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	void loadSavedAddressFromFieldList(QStringList fields);
	void clear();

private:
	DebugInterface& m_cpu;
	std::vector<SavedAddress> m_savedAddresses;
};
