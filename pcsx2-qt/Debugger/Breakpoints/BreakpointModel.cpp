// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BreakpointModel.h"

#include "QtHost.h"
#include "QtUtils.h"
#include "Debugger/DebuggerSettingsManager.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/DisassemblyManager.h"
#include "common/Console.h"

#include <QtWidgets/QMessageBox>

#include <algorithm>

std::map<BreakPointCpu, BreakpointModel*> BreakpointModel::s_instances;

BreakpointModel::BreakpointModel(DebugInterface& cpu, QObject* parent)
	: QAbstractTableModel(parent)
	, m_cpu(cpu)
{
	if (m_cpu.getCpuType() == BREAKPOINT_EE)
	{
		connect(g_emu_thread, &EmuThread::onGameChanged, this, [this](const QString& title) {
			if (title.isEmpty())
				return;

			if (rowCount() == 0)
				DebuggerSettingsManager::loadGameSettings(this);
		});

		DebuggerSettingsManager::loadGameSettings(this);
	}

	connect(this, &BreakpointModel::dataChanged, this, &BreakpointModel::refreshData);
}

BreakpointModel* BreakpointModel::getInstance(DebugInterface& cpu)
{
	auto iterator = s_instances.find(cpu.getCpuType());
	if (iterator == s_instances.end())
		iterator = s_instances.emplace(cpu.getCpuType(), new BreakpointModel(cpu)).first;

	return iterator->second;
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
	size_t row = static_cast<size_t>(index.row());
	if (!index.isValid() || row >= m_breakpoints.size())
		return QVariant();

	const BreakpointMemcheck& bp_mc = m_breakpoints[row];

	if (role == Qt::DisplayRole)
	{
		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::ENABLED:
					return "";
				case BreakpointColumns::TYPE:
					return tr("Execute");
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(bp->addr, 16);
				case BreakpointColumns::SIZE_LABEL:
					return QString::fromStdString(m_cpu.GetSymbolGuardian().FunctionStartingAtAddress(bp->addr).name);
				case BreakpointColumns::OPCODE:
					// Note: Fix up the disassemblymanager so we can use it here, instead of calling a function through the disassemblyview (yuck)
					return m_cpu.disasm(bp->addr, true).c_str();
				case BreakpointColumns::CONDITION:
					return bp->hasCond ? QString::fromStdString(bp->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return tr("--");
			}
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::ENABLED:
					return (mc->result & MEMCHECK_BREAK) ? tr("Enabled") : tr("Disabled");
				case BreakpointColumns::TYPE:
				{
					QString type("");
					type += (mc->memCond & MEMCHECK_READ) ? tr("Read") : "";
					type += ((mc->memCond & MEMCHECK_READWRITE) == MEMCHECK_READWRITE) ? ", " : " ";
					//: (C) = changes, as in "look for changes".
					type += (mc->memCond & MEMCHECK_WRITE) ? (mc->memCond & MEMCHECK_WRITE_ONCHANGE) ? tr("Write(C)") : tr("Write") : "";
					return type;
				}
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(mc->start, 16);
				case BreakpointColumns::SIZE_LABEL:
					return QString::number(mc->end - mc->start, 16);
				case BreakpointColumns::OPCODE:
					return tr("--"); // Our address is going to point to memory, no purpose in printing the op
				case BreakpointColumns::CONDITION:
					return mc->hasCond ? QString::fromStdString(mc->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return QString::number(mc->numHits);
			}
		}
	}
	else if (role == BreakpointModel::DataRole)
	{
		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::ENABLED:
					return static_cast<int>(bp->enabled);
				case BreakpointColumns::TYPE:
					return MEMCHECK_INVALID;
				case BreakpointColumns::OFFSET:
					return bp->addr;
				case BreakpointColumns::SIZE_LABEL:
					return QString::fromStdString(m_cpu.GetSymbolGuardian().FunctionStartingAtAddress(bp->addr).name);
				case BreakpointColumns::OPCODE:
					// Note: Fix up the disassemblymanager so we can use it here, instead of calling a function through the disassemblyview (yuck)
					return m_cpu.disasm(bp->addr, false).c_str();
				case BreakpointColumns::CONDITION:
					return bp->hasCond ? QString::fromStdString(bp->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return 0;
			}
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::ENABLED:
					return (mc->result & MEMCHECK_BREAK);
				case BreakpointColumns::TYPE:
					return mc->memCond;
				case BreakpointColumns::OFFSET:
					return mc->start;
				case BreakpointColumns::SIZE_LABEL:
					return mc->end - mc->start;
				case BreakpointColumns::OPCODE:
					return "";
				case BreakpointColumns::CONDITION:
					return mc->hasCond ? QString::fromStdString(mc->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return mc->numHits;
			}
		}
	}
	else if (role == BreakpointModel::ExportRole)
	{
		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::ENABLED:
					return static_cast<int>(bp->enabled);
				case BreakpointColumns::TYPE:
					return MEMCHECK_INVALID;
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(bp->addr, 16);
				case BreakpointColumns::SIZE_LABEL:
					return QString::fromStdString(m_cpu.GetSymbolGuardian().FunctionStartingAtAddress(bp->addr).name);
				case BreakpointColumns::OPCODE:
					// Note: Fix up the disassemblymanager so we can use it here, instead of calling a function through the disassemblyview (yuck)
					return m_cpu.disasm(bp->addr, false).c_str();
				case BreakpointColumns::CONDITION:
					return bp->hasCond ? QString::fromStdString(bp->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return 0;
			}
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			switch (index.column())
			{
				case BreakpointColumns::TYPE:
					return mc->memCond;
				case BreakpointColumns::OFFSET:
					return QtUtils::FilledQStringFromValue(mc->start, 16);
				case BreakpointColumns::SIZE_LABEL:
					return mc->end - mc->start;
				case BreakpointColumns::OPCODE:
					return "";
				case BreakpointColumns::CONDITION:
					return mc->hasCond ? QString::fromStdString(mc->cond.expressionString) : "";
				case BreakpointColumns::HITS:
					return mc->numHits;
				case BreakpointColumns::ENABLED:
					return (mc->result & MEMCHECK_BREAK);
			}
		}
	}
	else if (role == Qt::CheckStateRole)
	{
		if (index.column() == 0)
		{
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
				return tr("X");
			default:
				return QVariant();
		}
	}
	if (role == Qt::UserRole && orientation == Qt::Horizontal)
	{
		switch (section)
		{
			case BreakpointColumns::TYPE:
				return "TYPE";
			case BreakpointColumns::OFFSET:
				return "OFFSET";
			case BreakpointColumns::SIZE_LABEL:
				return "SIZE / LABEL";
			case BreakpointColumns::OPCODE:
				return "INSTRUCTION";
			case BreakpointColumns::CONDITION:
				return "CONDITION";
			case BreakpointColumns::HITS:
				return "HITS";
			case BreakpointColumns::ENABLED:
				return "X";
			default:
				return QVariant();
		}
	}
	return QVariant();
}

Qt::ItemFlags BreakpointModel::flags(const QModelIndex& index) const
{
	switch (index.column())
	{
		case BreakpointColumns::CONDITION:
			return Qt::ItemFlag::ItemIsEnabled | Qt::ItemFlag::ItemIsSelectable | Qt::ItemFlag::ItemIsEditable;
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
	size_t row = static_cast<size_t>(index.row());
	if (!index.isValid() || row >= m_breakpoints.size())
		return false;

	const BreakpointMemcheck& bp_mc = m_breakpoints[row];

	if (role == Qt::CheckStateRole && index.column() == BreakpointColumns::ENABLED)
	{
		if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = this->m_cpu.getCpuType(), bp = *bp, enabled = value.toBool()] {
				CBreakPoints::ChangeBreakPoint(cpu, bp.addr, enabled);
			});
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = this->m_cpu.getCpuType(), mc = *mc] {
				CBreakPoints::ChangeMemCheck(cpu, mc.start, mc.end, mc.memCond,
					MemCheckResult(mc.result ^ MEMCHECK_BREAK));
			});
		}
		emit dataChanged(index, index);
		return true;
	}
	else if (role == Qt::EditRole && index.column() == BreakpointColumns::CONDITION)
	{
		if (auto* bp = std::get_if<BreakPoint>(&bp_mc))
		{
			const QString condValue = value.toString();

			if (condValue.isEmpty())
			{
				if (bp->hasCond)
				{
					Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), bp] {
						CBreakPoints::ChangeBreakPointRemoveCond(cpu, bp->addr);
					});
				}
			}
			else
			{
				PostfixExpression expr;

				std::string error;
				if (!m_cpu.initExpression(condValue.toLocal8Bit().constData(), expr, error))
				{
					QMessageBox::warning(nullptr, "Condition Error", QString::fromStdString(error));
					return false;
				}

				BreakPointCond cond;
				cond.debug = &m_cpu;
				cond.expression = expr;
				cond.expressionString = condValue.toStdString();

				Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), bp, cond] {
					CBreakPoints::ChangeBreakPointAddCond(cpu, bp->addr, cond);
				});
			}
		}
		else if (auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			const QString condValue = value.toString();

			if (condValue.isEmpty())
			{
				if (mc->hasCond)
				{
					Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), mc] {
						CBreakPoints::ChangeMemCheckRemoveCond(cpu, mc->start, mc->end);
					});
				}
			}
			else
			{
				PostfixExpression expr;

				std::string error;
				if (!m_cpu.initExpression(condValue.toLocal8Bit().constData(), expr, error))
				{
					QMessageBox::warning(nullptr, "Condition Error", QString::fromStdString(error));
					return false;
				}

				BreakPointCond cond;
				cond.debug = &m_cpu;
				cond.expression = expr;
				cond.expressionString = condValue.toStdString();

				Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), mc, cond] {
					CBreakPoints::ChangeMemCheckAddCond(cpu, mc->start, mc->end, cond);
				});
			}
		}
		emit dataChanged(index, index);
		return true;
	}

	return false;
}

bool BreakpointModel::removeRows(int row, int count, const QModelIndex& index)
{
	beginRemoveRows(index, row, row + count - 1);

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
	const auto begin = m_breakpoints.begin() + row;
	const auto end = begin + count;
	m_breakpoints.erase(begin, end);

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
				CBreakPoints::AddBreakPoint(cpu, bp.addr, false, bp.enabled);

				if (bp.hasCond)
				{
					CBreakPoints::ChangeBreakPointAddCond(cpu, bp.addr, bp.cond);
				}
			});
		}
		else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
		{
			Host::RunOnCPUThread([cpu = m_cpu.getCpuType(), mc = *mc] {
				CBreakPoints::AddMemCheck(cpu, mc.start, mc.end, mc.memCond, mc.result);
				if (mc.hasCond)
				{
					CBreakPoints::ChangeMemCheckAddCond(cpu, mc.start, mc.end, mc.cond);
				}
			});
		}
	}

	endInsertRows();
	return true;
}

void BreakpointModel::refreshData()
{
	Host::RunOnCPUThread([this]() mutable {
		std::vector<BreakpointMemcheck> all_breakpoints;
		std::ranges::move(CBreakPoints::GetBreakpoints(m_cpu.getCpuType(), false), std::back_inserter(all_breakpoints));
		std::ranges::move(CBreakPoints::GetMemChecks(m_cpu.getCpuType()), std::back_inserter(all_breakpoints));

		QtHost::RunOnUIThread([this, breakpoints = std::move(all_breakpoints)]() mutable {
			beginResetModel();
			m_breakpoints = std::move(breakpoints);
			endResetModel();
		});
	});
}

void BreakpointModel::loadBreakpointFromFieldList(QStringList fields)
{
	std::string error;

	bool ok;
	if (fields.size() != BreakpointColumns::COLUMN_COUNT)
	{
		Console.WriteLn("Debugger Breakpoint Model: Invalid number of columns, skipping");
		return;
	}

	const int type = fields[BreakpointColumns::TYPE].toUInt(&ok);
	if (!ok)
	{
		Console.WriteLn("Debugger Breakpoint Model: Failed to parse type '%s', skipping",
			fields[BreakpointColumns::TYPE].toUtf8().constData());
		return;
	}

	// This is how we differentiate between breakpoints and memchecks
	if (type == MEMCHECK_INVALID)
	{
		BreakPoint bp;

		// Address
		bp.addr = fields[BreakpointColumns::OFFSET].toUInt(&ok, 16);
		if (!ok)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse address '%s', skipping",
				fields[BreakpointColumns::OFFSET].toUtf8().constData());
			return;
		}

		// Condition
		if (!fields[BreakpointColumns::CONDITION].isEmpty())
		{
			PostfixExpression expr;
			bp.hasCond = true;
			bp.cond.debug = &m_cpu;

			if (!m_cpu.initExpression(fields[BreakpointColumns::CONDITION].toUtf8().constData(), expr, error))
			{
				Console.WriteLn("Debugger Breakpoint Model: Failed to parse cond '%s', skipping",
					fields[BreakpointModel::CONDITION].toUtf8().constData());
				return;
			}
			bp.cond.expression = expr;
			bp.cond.expressionString = fields[BreakpointColumns::CONDITION].toStdString();
		}

		// Enabled
		bp.enabled = fields[BreakpointColumns::ENABLED].toUInt(&ok);
		if (!ok)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse enable flag '%s', skipping",
				fields[BreakpointColumns::ENABLED].toUtf8().constData());
			return;
		}

		insertBreakpointRows(0, 1, {bp});
	}
	else
	{
		MemCheck mc;
		// Mode
		if (type >= MEMCHECK_INVALID)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse cond type '%s', skipping",
				fields[BreakpointColumns::TYPE].toUtf8().constData());
			return;
		}
		mc.memCond = static_cast<MemCheckCondition>(type);

		// Address
		QString test = fields[BreakpointColumns::OFFSET];
		mc.start = fields[BreakpointColumns::OFFSET].toUInt(&ok, 16);
		if (!ok)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse address '%s', skipping",
				fields[BreakpointColumns::OFFSET].toUtf8().constData());
			return;
		}

		// Size
		mc.end = fields[BreakpointColumns::SIZE_LABEL].toUInt(&ok) + mc.start;
		if (!ok)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse length '%s', skipping",
				fields[BreakpointColumns::SIZE_LABEL].toUtf8().constData());
			return;
		}

		// Condition
		if (!fields[BreakpointColumns::CONDITION].isEmpty())
		{
			PostfixExpression expr;
			mc.hasCond = true;
			mc.cond.debug = &m_cpu;

			if (!m_cpu.initExpression(fields[BreakpointColumns::CONDITION].toUtf8().constData(), expr, error))
			{
				Console.WriteLn("Debugger Breakpoint Model: Failed to parse cond '%s', skipping",
					fields[BreakpointColumns::CONDITION].toUtf8().constData());
				return;
			}
			mc.cond.expression = expr;
			mc.cond.expressionString = fields[BreakpointColumns::CONDITION].toStdString();
		}

		// Result
		const int result = fields[BreakpointColumns::ENABLED].toUInt(&ok);
		if (!ok)
		{
			Console.WriteLn("Debugger Breakpoint Model: Failed to parse result flag '%s', skipping",
				fields[BreakpointColumns::ENABLED].toUtf8().constData());
			return;
		}
		mc.result = static_cast<MemCheckResult>(result);

		insertBreakpointRows(0, 1, {mc});
	}
}

void BreakpointModel::clear()
{
	beginResetModel();
	m_breakpoints.clear();
	endResetModel();
}
