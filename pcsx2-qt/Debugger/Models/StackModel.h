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
#include "DebugTools/MipsStackWalk.h"

class StackModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum StackColumns : int
	{
		ENTRY = 0,
		ENTRY_LABEL,
		PC,
		PC_OPCODE,
		SP,
		SIZE,
		COLUMN_COUNT
	};

	explicit StackModel(DebugInterface& cpu, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	void refreshData();

private:
	DebugInterface& m_cpu;
	std::vector<MipsStackWalk::StackFrame> m_stackFrames;
};
