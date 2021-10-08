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

#include "PrecompiledHeader.h"

#include "StackModel.h"
#include "DebugTools/MipsStackWalk.h"
#include "DebugTools/BiosDebugData.h"
#include "QtUtils.h"

StackModel::StackModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int StackModel::rowCount(const QModelIndex&) const
{
	return m_stackFrames.size();
}

int StackModel::columnCount(const QModelIndex&) const
{
	return StackModel::COLUMN_COUNT;
}

QVariant StackModel::data(const QModelIndex& index, int role) const
{
	if (role == Qt::DisplayRole)
	{
		const auto& stackFrame = m_stackFrames.at(index.row());

		switch (index.column())
		{
			case StackModel::ENTRY:
				return QtUtils::FilledQStringFromValue(stackFrame.entry, 16);
			case StackModel::ENTRY_LABEL:
				return m_cpu.GetSymbolMap().GetLabelString(stackFrame.entry).c_str();
			case StackModel::PC:
				return QtUtils::FilledQStringFromValue(stackFrame.pc, 16);
			case StackModel::PC_OPCODE:
				return m_cpu.disasm(stackFrame.pc, true).c_str();
			case StackModel::SP:
				return QtUtils::FilledQStringFromValue(stackFrame.sp, 16);
			case StackModel::SIZE:
				return QString::number(stackFrame.stackSize);
		}
	}
	else if (role == Qt::UserRole)
	{
		const auto& stackFrame = m_stackFrames.at(index.row());
		switch (index.column())
		{
			case StackModel::ENTRY:
				return stackFrame.entry;
			case StackModel::ENTRY_LABEL:
				return m_cpu.GetSymbolMap().GetLabelString(stackFrame.entry).c_str();
			case StackModel::PC:
				return stackFrame.pc;
			case StackModel::PC_OPCODE:
				return m_cpu.disasm(stackFrame.pc, true).c_str();
			case StackModel::SP:
				return stackFrame.sp;
			case StackModel::SIZE:
				return stackFrame.stackSize;
		}
	}
	return QVariant();
}

QVariant StackModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case StackColumns::ENTRY:
				return tr("ENTRY");
			case StackColumns::ENTRY_LABEL:
				return tr("LABEL");
			case StackColumns::PC:
				return tr("PC");
			case StackColumns::PC_OPCODE:
				return tr("INSTRUCTION");
			case StackColumns::SP:
				return tr("STACK POINTER");
			case StackColumns::SIZE:
				return tr("SIZE");
			default:
				return QVariant();
		}
	}
	return QVariant();
}

void StackModel::refreshData()
{
	// Hopefully in the near future we can get a stack frame for
	// each thread
	beginResetModel();
	for (const auto& thread : m_cpu.GetThreadList())
	{
		if (thread->Status() == ThreadStatus::THS_RUN)
		{
			m_stackFrames = MipsStackWalk::Walk(&m_cpu, m_cpu.getPC(), m_cpu.getRegister(0, 31), m_cpu.getRegister(0, 29),
				thread->EntryPoint(), thread->StackTop());
			break;
		}
	}
	endResetModel();
}
