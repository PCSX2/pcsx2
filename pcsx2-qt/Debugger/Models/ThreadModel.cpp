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
	if (m_cpu.getCpuType() == BREAKPOINT_EE)
		return getEEThreads().size();
	else
		return 0;
}

int ThreadModel::columnCount(const QModelIndex&) const
{
	return ThreadModel::COLUMN_COUNT;
}

QVariant ThreadModel::data(const QModelIndex& index, int role) const
{
	if (role == Qt::DisplayRole)
	{
		const auto thread = getEEThreads().at(index.row());

		switch (index.column())
		{
			case ThreadModel::ID:
				return thread.tid;
			case ThreadModel::PC:
			{
				if (thread.data.status == THS_RUN)
					return QtUtils::FilledQStringFromValue(m_cpu.getPC(), 16);
				else
					return QtUtils::FilledQStringFromValue(thread.data.entry, 16);
			}
			case ThreadModel::ENTRY:
				return QtUtils::FilledQStringFromValue(thread.data.entry_init, 16);
			case ThreadModel::PRIORITY:
				return QString::number(thread.data.currentPriority);
			case ThreadModel::STATE:
			{
				const auto& state = ThreadStateStrings.find(thread.data.status);
				if (state != ThreadStateStrings.end())
					return state->second;
				else
					return tr("INVALID");
			}
			case ThreadModel::WAIT_TYPE:
			{
				const auto& waitType = ThreadWaitStrings.find(thread.data.waitType);
				if (waitType != ThreadWaitStrings.end())
					return waitType->second;
				else
					return tr("INVALID");
			}
		}
	}
	else if (role == Qt::UserRole)
	{
		const auto thread = getEEThreads().at(index.row());

		switch (index.column())
		{
			case ThreadModel::ID:
				return thread.tid;
			case ThreadModel::PC:
			{
				if (thread.data.status == THS_RUN)
					return m_cpu.getPC();
				else
					return thread.data.entry;
			}
			case ThreadModel::ENTRY:
				return thread.data.entry_init;
			case ThreadModel::PRIORITY:
				return thread.data.currentPriority;
			case ThreadModel::STATE:
				return thread.data.status;
			case ThreadModel::WAIT_TYPE:
				return thread.data.waitType;
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
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed.
				return tr("ID");
			case ThreadColumns::PC:
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed. PC= TO BE RESEARCHED AND WRITTEN.
				return tr("PC");
			case ThreadColumns::ENTRY:
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed.
				return tr("ENTRY");
			case ThreadColumns::PRIORITY:
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed.
				return tr("PRIORITY");
			case ThreadColumns::STATE:
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed.
				return tr("STATE");
			case ThreadColumns::WAIT_TYPE:
				//:I18N COMMENT: Warning: short space limit. Abbreviate if needed.
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
