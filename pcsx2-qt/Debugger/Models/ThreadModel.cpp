// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ThreadModel.h"

#include "QtUtils.h"

ThreadModel::ThreadModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int ThreadModel::rowCount(const QModelIndex&) const
{
	return m_cpu.GetThreadList().size();
}

int ThreadModel::columnCount(const QModelIndex&) const
{
	return ThreadModel::COLUMN_COUNT;
}

QVariant ThreadModel::data(const QModelIndex& index, int role) const
{
	const auto threads = m_cpu.GetThreadList();
	auto* const thread = threads.at(index.row()).get();

	if (role == Qt::DisplayRole)
	{
		switch (index.column())
		{
			case ThreadModel::ID:
				return thread->TID();
			case ThreadModel::PC:
			{
				if (thread->Status() == ThreadStatus::THS_RUN)
					return QtUtils::FilledQStringFromValue(m_cpu.getPC(), 16);

				return QtUtils::FilledQStringFromValue(thread->PC(), 16);
			}
			case ThreadModel::ENTRY:
				return QtUtils::FilledQStringFromValue(thread->EntryPoint(), 16);
			case ThreadModel::PRIORITY:
				return QString::number(thread->Priority());
			case ThreadModel::STATE:
			{
				const auto& state = ThreadStateStrings.find(thread->Status());
				if (state != ThreadStateStrings.end())
					return state->second;

				return tr("INVALID");
			}
			case ThreadModel::WAIT_TYPE:
			{
				const auto& waitType = ThreadWaitStrings.find(thread->Wait());
				if (waitType != ThreadWaitStrings.end())
					return waitType->second;

				return tr("INVALID");
			}
		}
	}
	else if (role == Qt::UserRole)
	{
		switch (index.column())
		{
			case ThreadModel::ID:
				return thread->TID();
			case ThreadModel::PC:
			{
				if (thread->Status() == ThreadStatus::THS_RUN)
					return m_cpu.getPC();

				return thread->PC();
			}
			case ThreadModel::ENTRY:
				return thread->EntryPoint();
			case ThreadModel::PRIORITY:
				return thread->Priority();
			case ThreadModel::STATE:
				return static_cast<u32>(thread->Status());
			case ThreadModel::WAIT_TYPE:
				return static_cast<u32>(thread->Wait());
			default:
				return QVariant();
		}
	}
	return QVariant();
}

QVariant ThreadModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case ThreadColumns::ID:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("ID");
			case ThreadColumns::PC:
				//: Warning: short space limit. Abbreviate if needed. PC = Program Counter (location where the CPU is executing).
				return tr("PC");
			case ThreadColumns::ENTRY:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("ENTRY");
			case ThreadColumns::PRIORITY:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("PRIORITY");
			case ThreadColumns::STATE:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("STATE");
			case ThreadColumns::WAIT_TYPE:
				//: Warning: short space limit. Abbreviate if needed.
				return tr("WAIT TYPE");
			default:
				return QVariant();
		}
	}
	return QVariant();
}

void ThreadModel::refreshData()
{
	beginResetModel();
	endResetModel();
}
