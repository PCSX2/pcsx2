// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QHeaderView>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"

using BreakpointMemcheck = std::variant<BreakPoint, MemCheck>;

class BreakpointModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum BreakpointColumns : int
	{
		ENABLED = 0,
		TYPE,
		OFFSET,
		SIZE_LABEL,
		OPCODE,
		CONDITION,
		HITS,
		COLUMN_COUNT
	};

	enum BreakpointRoles : int
	{
		DataRole = Qt::UserRole,
		ExportRole = Qt::UserRole + 1,
	};

	static constexpr QHeaderView::ResizeMode HeaderResizeModes[BreakpointColumns::COLUMN_COUNT] =
	{
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::Stretch,
		QHeaderView::ResizeMode::Stretch,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
	};

	explicit BreakpointModel(DebugInterface& cpu, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	bool removeRows(int row, int count, const QModelIndex& index = QModelIndex()) override;
	bool insertBreakpointRows(int row, int count, std::vector<BreakpointMemcheck> breakpoints, const QModelIndex& index = QModelIndex());

	BreakpointMemcheck at(int row) const { return m_breakpoints.at(row); };

	void refreshData();

private:
	DebugInterface& m_cpu;
	std::vector<BreakpointMemcheck> m_breakpoints;
};
