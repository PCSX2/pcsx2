// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
				return m_cpu.GetSymbolMap().GetLabelName(stackFrame.entry).c_str();
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
				return m_cpu.GetSymbolMap().GetLabelName(stackFrame.entry).c_str();
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
				//: Warning: short space limit. Abbreviate if needed.
				return tr("ENTRY");
			case StackColumns::ENTRY_LABEL:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("LABEL");
			case StackColumns::PC:
				//: Warning: short space limit. Abbreviate if needed. PC = Program Counter (location where the CPU is executing).
				return tr("PC");
			case StackColumns::PC_OPCODE:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("INSTRUCTION");
			case StackColumns::SP:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("STACK POINTER");
			case StackColumns::SIZE:
				//: Warning: short space limit. Abbreviate if needed.
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
