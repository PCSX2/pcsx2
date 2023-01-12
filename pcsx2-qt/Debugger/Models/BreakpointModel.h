/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include <QtCore/QAbstractTableModel>
#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"

using BreakpointMemcheck = std::variant<BreakPoint, MemCheck>;

class BreakpointModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum BreakpointColumns : int
	{
		TYPE = 0,
		OFFSET,
		SIZE_LABEL,
		OPCODE,
		CONDITION,
		HITS,
		ENABLED,
		COLUMN_COUNT
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
