// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QHeaderView>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/BiosDebugData.h"

class ModuleModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum ModuleColumns : int
	{
		NAME = 0,
		VERSION,
		ENTRY,
		GP,
		TEXT_SECTION,
		DATA_SECTION,
		BSS_SECTION,
		COLUMN_COUNT
	};

	static constexpr QHeaderView::ResizeMode HeaderResizeModes[ModuleColumns::COLUMN_COUNT] =
		{
			QHeaderView::ResizeMode::ResizeToContents,
			QHeaderView::ResizeMode::Stretch,
			QHeaderView::ResizeMode::Stretch,
			QHeaderView::ResizeMode::Stretch,
			QHeaderView::ResizeMode::ResizeToContents,
			QHeaderView::ResizeMode::ResizeToContents,
			QHeaderView::ResizeMode::ResizeToContents,
		};

	explicit ModuleModel(DebugInterface& cpu, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	void refreshData();

private:
	DebugInterface& m_cpu;
	std::vector<IopMod> m_modules;
};
