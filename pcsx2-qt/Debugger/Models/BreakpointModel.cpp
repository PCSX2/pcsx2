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

#include "BreakpointModel.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/DisassemblyManager.h"

#include "QtHost.h"
#include "QtUtils.h"
#include <QtWidgets/QMessageBox>

#include <algorithm>

BreakpointModel::BreakpointModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
}

int BreakpointModel::rowCount(const QModelIndex&) const
{
	return m_breakpoints.size();
}

int BreakpointModel::columnCount(const QModelIndex&) const
{
	return BreakpointColumns::COLUMN_COUNT;
}

QVariant BreakpointModel::data(const QModelIndex& index, int role) const
{
	if (role == Qt::DisplayRole)
	{
		auto bp_mc = m_breakpoints.at(index.row());

		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::TYPE:
					return tr("Execute");
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(bp->addr, 16);
				case BreakpointColumns::SIZE_LABEL:
					return m_cpu.GetSymbolMap().GetLabelString(bp->addr).c_str();
				case BreakpointColumns::OPCODE:
					// Note: Fix up the disassemblymanager so we can use it here, instead of calling a function through the disassemblyview (yuck)
					return m_cpu.disasm(bp->addr, true).c_str();
				case BreakpointColumns::CONDITION:
					return bp->hasCond ? QString::fromLocal8Bit(bp->cond.expressionString) : tr("No Condition");
				case BreakpointColumns::HITS:
					return tr("--");
				case BreakpointColumns::ENABLED:
					return bp->enabled ? tr("Enabled") : tr("Disabled");
			}
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::TYPE:
				{
					QString type("");
					type += (mc->cond & MEMCHECK_READ) ? tr("Read") : "";
					type += ((mc->cond & MEMCHECK_READWRITE) == MEMCHECK_READWRITE) ? ", " : " ";
					//: (C) = changes, as in "look for changes".
					type += (mc->cond & MEMCHECK_WRITE) ? (mc->cond & MEMCHECK_WRITE_ONCHANGE) ? tr("Write(C)") : tr("Write") : "";
					return type;
				}
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(mc->start, 16);
				case BreakpointColumns::SIZE_LABEL:
					return QString::number(mc->end - mc->start, 16);
				case BreakpointColumns::OPCODE:
					return tr("--"); // Our address is going to point to memory, no purpose in printing the op
				case BreakpointColumns::CONDITION:
					return tr("--"); // No condition on memchecks
				case BreakpointColumns::HITS:
					return QString::number(mc->numHits);
				case BreakpointColumns::ENABLED:
					return (mc->result & MEMCHECK_BREAK) ? tr("Enabled") : tr("Disabled");
			}
		}
	}
	else if (role == Qt::UserRole)
	{
		auto bp_mc = m_breakpoints.at(index.row());

		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::TYPE:
					return 0;
				case BreakpointColumns::OFFSET:
					return bp->addr;
				case BreakpointColumns::SIZE_LABEL:
					return m_cpu.GetSymbolMap().GetLabelString(bp->addr).c_str();
				case BreakpointColumns::OPCODE:
					// Note: Fix up the disassemblymanager so we can use it here, instead of calling a function through the disassemblyview (yuck)
					return m_cpu.disasm(bp->addr, true).c_str();
				case BreakpointColumns::CONDITION:
					return bp->hasCond ? QString::fromLocal8Bit(bp->cond.expressionString) : tr("");
				case BreakpointColumns::HITS:
					return 0;
				case BreakpointColumns::ENABLED:
					return bp->enabled;
			}
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::TYPE:
					return mc->cond;
				case BreakpointColumns::OFFSET:
					return mc->start;
				case BreakpointColumns::SIZE_LABEL:
					return mc->end - mc->start;
				case BreakpointColumns::OPCODE:
					return "";
				case BreakpointColumns::CONDITION:
					return "";
				case BreakpointColumns::HITS:
					return mc->numHits;
				case BreakpointColumns::ENABLED:
					return (mc->result & MEMCHECK_BREAK);
			}
		}
	}
	else if (role == Qt::CheckStateRole)
	{
		if (index.column() == 6)
		{
			auto bp_mc = m_breakpoints.at(index.row());

			if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
			{
				return bp->enabled ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
			}
			else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
			{
				return (mc->result & MEMCHECK_BREAK) ? Qt::CheckState::Checked : Qt::CheckState::Unchecked;
			}
		}
	}
	return QVariant();
}

QVariant BreakpointModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case BreakpointColumns::TYPE:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("TYPE");
			case BreakpointColumns::OFFSET:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("OFFSET");
			case BreakpointColumns::SIZE_LABEL:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("SIZE / LABEL");
			case BreakpointColumns::OPCODE:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("INSTRUCTION");
			case BreakpointColumns::CONDITION:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("CONDITION");
			case BreakpointColumns::HITS:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("HITS");
			case BreakpointColumns::ENABLED:
				//: Warning: limited space available. Abbreviate if needed.
				return tr("ENABLED");
			default:
				return QVariant();
		}
	}
	return QVariant();
}

Qt::ItemFlags BreakpointModel::flags(const QModelIndex& index) const
{
	volatile const int row = index.row();

	bool is_breakpoint = std::holds_alternative<BreakPoint>(m_breakpoints.at(row));

	switch (index.column())
	{
		case BreakpointColumns::CONDITION:
			if (is_breakpoint)
				return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEditable;
			[[fallthrough]];
		case BreakpointColumns::TYPE:
		case BreakpointColumns::OPCODE:
		case BreakpointColumns::HITS:
		case BreakpointColumns::OFFSET:
		case BreakpointColumns::SIZE_LABEL:
			return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;
		case BreakpointColumns::ENABLED:
			return Qt::ItemFlag::ItemIsUserCheckable | Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable;
	}

	return index.flags();
}

bool BreakpointModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (role == Qt::CheckStateRole && index.column() == BreakpointColumns::ENABLED)
	{
		auto bp_mc = m_breakpoints.at(index.row());

		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = this->m_cpu.getCpuType(), bp = *bp, enabled = value.toBool()] {
				CBreakPoints::ChangeBreakPoint(cpu, bp.addr, enabled);
			});
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = this->m_cpu.getCpuType(), mc = *mc] {
				CBreakPoints::ChangeMemCheck(cpu, mc.start, mc.end, mc.cond,
					MemCheckResult(mc.result ^ MEMCHECK_BREAK));
			});
		}
		return true;
	}
	else if (role == Qt::EditRole && index.column() == BreakpointColumns::CONDITION)
	{
		auto bp_mc = m_breakpoints.at(index.row());

		if (std::holds_alternative<MemCheck>(bp_mc))
			return false;

		const auto bp = std::get<BreakPoint>(bp_mc);

		const QString condValue = value.toString();

		if (condValue.isEmpty())
		{
			if (bp.hasCond)
			{
				Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), bp] {
					CBreakPoints::ChangeBreakPointRemoveCond(cpu, bp.addr);
				});
			}
		}
		else
		{
			PostfixExpression expr;

			if (!m_cpu.initExpression(condValue.toLocal8Bit().constData(), expr))
			{
				QMessageBox::warning(nullptr, "Condition Error", QString(getExpressionError()));
				return false;
			}

			BreakPointCond cond;
			cond.debug = &m_cpu;
			cond.expression = expr;
			strcpy(&cond.expressionString[0], condValue.toLocal8Bit().constData());

			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), bp, cond] {
				CBreakPoints::ChangeBreakPointAddCond(cpu, bp.addr, cond);
			});
			return true;
		}
	}

	return false;
}

bool BreakpointModel::removeRows(int row, int count, const QModelIndex& index)
{
	beginRemoveRows(index, row, row + count);

	for (int i = row; i < row + count; i++)
	{
		auto bp_mc = m_breakpoints.at(i);

		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), addr = bp->addr] {
				CBreakPoints::RemoveBreakPoint(cpu, addr);
			});
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), start = mc->start, end = mc->end] {
				CBreakPoints::RemoveMemCheck(cpu, start, end);
			});
		}
	}

	endRemoveRows();
	return true;
}

bool BreakpointModel::insertBreakpointRows(int row, int count, std::vector<BreakpointMemcheck> breakpoints, const QModelIndex& index)
{
	if (breakpoints.size() != static_cast<size_t>(count))
		return false;

	beginInsertRows(index, row, row + (count - 1));

	// After endInsertRows, Qt will try and validate our new rows
	// Because we add the breakpoints off of the UI thread, our new rows may not be visible yet
	// To prevent the (seemingly harmless?) warning emitted by enderInsertRows, add the breakpoints manually here as well
	m_breakpoints.insert(m_breakpoints.begin(), breakpoints.begin(), breakpoints.end());
	for (const auto& bp_mc : breakpoints)
	{
		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), bp = *bp] {
				CBreakPoints::AddBreakPoint(cpu, bp.addr);

				if (bp.hasCond)
				{
					CBreakPoints::ChangeBreakPointAddCond(cpu, bp.addr, bp.cond);
				}
			});
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), mc = *mc] {
				CBreakPoints::AddMemCheck(cpu, mc.start, mc.end, mc.cond, mc.result);
			});
		}
	}

	endInsertRows();
	return true;
}

void BreakpointModel::refreshData()
{
	beginResetModel();

	m_breakpoints.clear();

	auto breakpoints = CBreakPoints::GetBreakpoints(m_cpu.getCpuType(), false);
	for (const auto& bp : breakpoints)
	{
		m_breakpoints.push_back(bp);
	}

	auto memchecks = CBreakPoints::GetMemChecks(m_cpu.getCpuType());
	for (const auto& mc : memchecks)
	{
		m_breakpoints.push_back(mc);
	}

	endResetModel();
}
