/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "BreakpointDialog.h"
#include "DebugTools/Breakpoints.h"

#include "QtUtils.h"
#include "QtHost.h"
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>

BreakpointDialog::BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakpointModel& model)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_purpose(PURPOSE::CREATE)
	, m_bpModel(model)
{
	m_ui.setupUi(this);

	m_ui.grpType->setEnabled(true);
	m_ui.txtAddress->setEnabled(true);

	connect(m_ui.rdoExecute, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	connect(m_ui.rdoMemory, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
}

BreakpointDialog::BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakpointModel& model, BreakpointMemcheck bp_mc, int rowIndex)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_purpose(PURPOSE::EDIT)
	, m_bpModel(model)
	, m_bp_mc(bp_mc)
	, m_rowIndex(rowIndex)
{
	m_ui.setupUi(this);

	connect(m_ui.rdoExecute, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	connect(m_ui.rdoMemory, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);

	if (const auto* bp = std::get_if<BreakPoint>(&bp_mc))
	{
		m_ui.rdoExecute->setChecked(true);
		m_ui.chkEnable->setChecked(bp->enabled);
		m_ui.txtAddress->setText(QtUtils::FilledQStringFromValue(bp->addr, 16));

		if (bp->hasCond)
			m_ui.txtCondition->setText(QString::fromLocal8Bit(bp->cond.expressionString));
	}
	else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
	{
		m_ui.rdoMemory->setChecked(true);

		m_ui.txtAddress->setText(QtUtils::FilledQStringFromValue(mc->start, 16));
		m_ui.txtSize->setText(QtUtils::FilledQStringFromValue(mc->end - mc->start, 16));

		m_ui.chkRead->setChecked(mc->cond & MEMCHECK_READ);
		m_ui.chkWrite->setChecked(mc->cond & MEMCHECK_WRITE);
		m_ui.chkChange->setChecked(mc->cond & MEMCHECK_WRITE_ONCHANGE);

		m_ui.chkEnable->setChecked(mc->result & MEMCHECK_BREAK);
		m_ui.chkLog->setChecked(mc->result & MEMCHECK_LOG);
	}
}

BreakpointDialog::~BreakpointDialog()
{
}

void BreakpointDialog::onRdoButtonToggled()
{
	const bool isExecute = m_ui.rdoExecute->isChecked();

	m_ui.grpExecute->setEnabled(isExecute);
	m_ui.grpMemory->setEnabled(!isExecute);

	m_ui.chkLog->setEnabled(!isExecute);
}

void BreakpointDialog::accept()
{
	if (m_purpose == PURPOSE::CREATE)
	{
		if (m_ui.rdoExecute->isChecked())
			m_bp_mc = BreakPoint();
		else if (m_ui.rdoMemory->isChecked())
			m_bp_mc = MemCheck();
	}

	if (auto* bp = std::get_if<BreakPoint>(&m_bp_mc))
	{
		PostfixExpression expr;

		u64 address;
		if (!m_cpu->initExpression(m_ui.txtAddress->text().toLocal8Bit().constData(), expr) ||
			!m_cpu->parseExpression(expr, address))
		{
			QMessageBox::warning(this, tr("Error"), tr("Invalid address \"%1\"").arg(m_ui.txtAddress->text()));
			return;
		}

		bp->addr = address;

		bp->enabled = m_ui.chkEnable->isChecked();

		if (!m_ui.txtCondition->text().isEmpty())
		{
			bp->hasCond = true;
			bp->cond.debug = m_cpu;

			if (!m_cpu->initExpression(m_ui.txtCondition->text().toLocal8Bit().constData(), expr))
			{
				QMessageBox::warning(this, tr("Error"), tr("Invalid condition \"%1\"").arg(getExpressionError()));
				return;
			}

			bp->cond.expression = expr;
			strncpy(&bp->cond.expressionString[0], m_ui.txtCondition->text().toLocal8Bit().constData(),
				sizeof(bp->cond.expressionString));
		}
	}
	if (auto* mc = std::get_if<MemCheck>(&m_bp_mc))
	{
		PostfixExpression expr;

		u64 startAddress;
		if (!m_cpu->initExpression(m_ui.txtAddress->text().toLocal8Bit().constData(), expr) ||
			!m_cpu->parseExpression(expr, startAddress))
		{
			QMessageBox::warning(this, tr("Error"), tr("Invalid address \"%1\"").arg(m_ui.txtAddress->text()));
			return;
		}

		u64 size;
		if (!m_cpu->initExpression(m_ui.txtSize->text().toLocal8Bit(), expr) ||
			!m_cpu->parseExpression(expr, size) || !size)
		{
			QMessageBox::warning(this, tr("Error"), tr("Invalid size \"%1\"").arg(m_ui.txtSize->text()));
			return;
		}

		mc->start = startAddress;
		mc->end = startAddress + size;

		int condition = 0;
		if (m_ui.chkRead->isChecked())
			condition |= MEMCHECK_READ;
		if (m_ui.chkWrite->isChecked())
			condition |= MEMCHECK_WRITE;
		if (m_ui.chkChange->isChecked())
			condition |= MEMCHECK_WRITE_ONCHANGE;

		mc->cond = static_cast<MemCheckCondition>(condition);

		int result = 0;
		if (m_ui.chkEnable->isChecked())
			result |= MEMCHECK_BREAK;
		if (m_ui.chkLog->isChecked())
			result |= MEMCHECK_LOG;

		mc->result = static_cast<MemCheckResult>(result);
	}


	if (m_purpose == PURPOSE::EDIT)
	{
		m_bpModel.removeRows(m_rowIndex, 1);
	}

	m_bpModel.insertBreakpointRows(0, 1, {m_bp_mc});

	QDialog::accept();
}
