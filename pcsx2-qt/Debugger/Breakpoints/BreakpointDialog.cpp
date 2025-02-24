// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
			m_ui.txtCondition->setText(QString::fromStdString(bp->cond.expressionString));
	}
	else if (const auto* mc = std::get_if<MemCheck>(&bp_mc))
	{
		m_ui.rdoMemory->setChecked(true);

		m_ui.txtAddress->setText(QtUtils::FilledQStringFromValue(mc->start, 16));
		m_ui.txtSize->setText(QtUtils::FilledQStringFromValue(mc->end - mc->start, 16));

		m_ui.chkRead->setChecked(mc->memCond & MEMCHECK_READ);
		m_ui.chkWrite->setChecked(mc->memCond & MEMCHECK_WRITE);
		m_ui.chkChange->setChecked(mc->memCond & MEMCHECK_WRITE_ONCHANGE);

		m_ui.chkEnable->setChecked(mc->result & MEMCHECK_BREAK);
		m_ui.chkLog->setChecked(mc->result & MEMCHECK_LOG);

		if (mc->hasCond)
			m_ui.txtCondition->setText(QString::fromStdString(mc->cond.expressionString));
	}
}

BreakpointDialog::~BreakpointDialog()
{
}

void BreakpointDialog::onRdoButtonToggled()
{
	const bool isExecute = m_ui.rdoExecute->isChecked();

	m_ui.grpMemory->setEnabled(!isExecute);

	m_ui.chkLog->setEnabled(!isExecute);
}

void BreakpointDialog::accept()
{
	std::string error;

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
		if (!m_cpu->evaluateExpression(m_ui.txtAddress->text().toStdString().c_str(), address, error))
		{
			QMessageBox::warning(this, tr("Invalid Address"), QString::fromStdString(error));
			return;
		}

		bp->addr = address;

		bp->enabled = m_ui.chkEnable->isChecked();

		if (!m_ui.txtCondition->text().isEmpty())
		{
			bp->hasCond = true;
			bp->cond.debug = m_cpu;

			if (!m_cpu->initExpression(m_ui.txtCondition->text().toStdString().c_str(), expr, error))
			{
				QMessageBox::warning(this, tr("Invalid Condition"), QString::fromStdString(error));
				return;
			}

			bp->cond.expression = expr;
			bp->cond.expressionString = m_ui.txtCondition->text().toStdString();
		}
	}
	if (auto* mc = std::get_if<MemCheck>(&m_bp_mc))
	{
		u64 startAddress;
		if (!m_cpu->evaluateExpression(m_ui.txtAddress->text().toStdString().c_str(), startAddress, error))
		{
			QMessageBox::warning(this, tr("Invalid Address"), QString::fromStdString(error));
			return;
		}

		u64 size;
		if (!m_cpu->evaluateExpression(m_ui.txtSize->text().toStdString().c_str(), size, error) || !size)
		{
			QMessageBox::warning(this, tr("Invalid Size"), QString::fromStdString(error));
			return;
		}

		mc->start = startAddress;
		mc->end = startAddress + size;

		if (!m_ui.txtCondition->text().isEmpty())
		{
			mc->hasCond = true;
			mc->cond.debug = m_cpu;

			PostfixExpression expr;
			if (!m_cpu->initExpression(m_ui.txtCondition->text().toStdString().c_str(), expr, error))
			{
				QMessageBox::warning(this, tr("Invalid Condition"), QString::fromStdString(error));
				return;
			}

			mc->cond.expression = expr;
			mc->cond.expressionString = m_ui.txtCondition->text().toStdString();
		}

		int condition = 0;
		if (m_ui.chkRead->isChecked())
			condition |= MEMCHECK_READ;
		if (m_ui.chkWrite->isChecked())
			condition |= MEMCHECK_WRITE;
		if (m_ui.chkChange->isChecked())
			condition |= MEMCHECK_WRITE_ONCHANGE;

		mc->memCond = static_cast<MemCheckCondition>(condition);

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
