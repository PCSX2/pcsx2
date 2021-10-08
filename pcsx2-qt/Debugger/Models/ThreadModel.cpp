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
				return tr("ID");
			case ThreadColumns::PC:
				return tr("PC");
			case ThreadColumns::ENTRY:
				return tr("ENTRY");
			case ThreadColumns::PRIORITY:
				return tr("PRIORITY");
			case ThreadColumns::STATE:
				return tr("STATE");
			case ThreadColumns::WAIT_TYPE:
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
