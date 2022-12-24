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

BreakpointDialog::BreakpointDialog(QWidget* parent, DebugInterface* cpu)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_purpose(BPDIALOG_PURPOSE::CREATE)
{
	m_ui.setupUi(this);

	m_ui.grpType->setEnabled(true);
	m_ui.txtAddress->setEnabled(true);

	connect(m_ui.rdoExecute, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	connect(m_ui.rdoMemory, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
}

BreakpointDialog::BreakpointDialog(QWidget* parent, DebugInterface* cpu, BreakPoint* bp)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_purpose(BPDIALOG_PURPOSE::EDIT_BP)
	, m_bp(bp)
{
	m_ui.setupUi(this);

	m_ui.txtAddress->setText(QtUtils::FilledQStringFromValue(m_bp->addr, 16));
	if (m_bp->hasCond)
		m_ui.txtCondition->setText(QString::fromLocal8Bit(&m_bp->cond.expressionString[0]));
	m_ui.chkEnable->setChecked(m_bp->enabled);

	connect(m_ui.rdoExecute, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	connect(m_ui.rdoMemory, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	m_ui.rdoExecute->toggle();
}

BreakpointDialog::BreakpointDialog(QWidget* parent, DebugInterface* cpu, MemCheck* mc)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_purpose(BPDIALOG_PURPOSE::EDIT_MC)
	, m_mc(mc)
{
	m_ui.setupUi(this);

	m_ui.txtAddress->setText(QtUtils::FilledQStringFromValue(m_mc->start, 16));
	m_ui.txtSize->setText(QString::number(m_mc->end - m_mc->start, 16).toUpper());
	m_ui.chkLog->setChecked(m_mc->result & MEMCHECK_LOG);
	m_ui.chkEnable->setChecked(m_mc->result & MEMCHECK_BREAK);
	m_ui.chkChange->setChecked(m_mc->cond & MEMCHECK_WRITE_ONCHANGE);
	m_ui.chkRead->setChecked(m_mc->cond & MEMCHECK_READ);
	m_ui.chkWrite->setChecked(m_mc->cond & MEMCHECK_WRITE);

	connect(m_ui.rdoExecute, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	connect(m_ui.rdoMemory, &QRadioButton::toggled, this, &BreakpointDialog::onRdoButtonToggled);
	m_ui.rdoMemory->toggle();
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

// When we are creating a breakpoint, use m_bp or m_mc only as a place to store information for the new breakpoint
// When we are modifying a breakpoint, m_bp or m_mc is a pointer to the breakpoint we are updating
void BreakpointDialog::accept()
{
	if (m_purpose == BPDIALOG_PURPOSE::CREATE)
	{
		if (m_ui.rdoExecute->isChecked())
		{
			m_bp = new BreakPoint;
			m_bp->cpu = m_cpu->getCpuType();
			m_bp->cond.debug = m_cpu;
		}
		else
		{
			m_mc = new MemCheck;
			m_mc->cpu = m_cpu->getCpuType();
		}
	}

	PostfixExpression expression;

	// Validate the address
	u64 address;
	if (!m_cpu->initExpression(m_ui.txtAddress->text().toLocal8Bit(), expression) || !m_cpu->parseExpression(expression, address))
	{
		QMessageBox::warning(this, tr("Error"), tr("Invalid address \"%1\"").arg(m_ui.txtAddress->text()));
		return;
	}

	u64 size;
	if (m_ui.rdoMemory->isChecked())
	{
		if (!m_cpu->initExpression(m_ui.txtSize->text().toLocal8Bit(), expression) || !m_cpu->parseExpression(expression, size) || !size)
		{
			QMessageBox::warning(this, tr("Error"), tr("Invalid size \"%1\"").arg(m_ui.txtSize->text()));
			return;
		}

		m_mc->start = address;
		const bool changedSize = m_mc->end != (m_mc->start + size);
		const u32 prevEnd = m_mc->end;
		m_mc->end = m_mc->start + size;

		int condition = 0;
		if (m_ui.chkRead->isChecked())
			condition |= MEMCHECK_READ;
		if (m_ui.chkWrite->isChecked())
			condition |= MEMCHECK_WRITE;
		if (m_ui.chkChange->isChecked())
			condition |= MEMCHECK_WRITE_ONCHANGE;

		int result = 0;
		if (m_ui.chkEnable->isChecked())
			result |= MEMCHECK_BREAK;
		if (m_ui.chkLog->isChecked())
			result |= MEMCHECK_LOG;

		if (m_purpose == BPDIALOG_PURPOSE::CREATE)
		{
			Host::RunOnCPUThread([this, condition, result] {
				CBreakPoints::AddMemCheck(m_cpu->getCpuType(), m_mc->start, m_mc->end, (MemCheckCondition)condition, (MemCheckResult)result);
				delete m_mc;
			});
		}
		else
		{
			Host::RunOnCPUThread([this, mc = *m_mc, condition, result, prevEnd, changedSize] {
				if (changedSize)
				{
					CBreakPoints::RemoveMemCheck(m_cpu->getCpuType(), mc.start, prevEnd);
					CBreakPoints::AddMemCheck(m_cpu->getCpuType(), mc.start, mc.end, static_cast<MemCheckCondition>(condition), static_cast<MemCheckResult>(result));
				}
				else
				{
					CBreakPoints::ChangeMemCheck(m_cpu->getCpuType(), mc.start, mc.end, static_cast<MemCheckCondition>(condition), static_cast<MemCheckResult>(result));
				}
			});
		}
	}
	else
	{
		if (m_purpose == BPDIALOG_PURPOSE::CREATE)
			Host::RunOnCPUThread([this, address] {
				CBreakPoints::AddBreakPoint(m_cpu->getCpuType(), address);
			});

		// Validate the condition
		// TODO: Expression management in the core should be updated to make this prettier
		if (!m_ui.txtCondition->text().isEmpty())
		{
			auto strData = m_ui.txtCondition->text().toLocal8Bit();
			expression.clear();
			if (!m_cpu->initExpression(strData.constData(), expression))
			{
				QMessageBox::warning(this, tr("Error"), tr("Invalid condition \"%1\"").arg(strData));
				return;
			}

			m_bp->cond.expression = expression;
			strncpy(&m_bp->cond.expressionString[0], strData.constData(), sizeof(m_bp->cond.expressionString));

			Host::RunOnCPUThread([this, address] {
				CBreakPoints::ChangeBreakPointAddCond(m_cpu->getCpuType(), address, m_bp->cond);
			});
		}
		else
		{
			Host::RunOnCPUThread([this, address] { CBreakPoints::ChangeBreakPointRemoveCond(m_cpu->getCpuType(), address); }, true);
		}
		m_bp->addr = address;
	}

	QDialog::accept();
}
