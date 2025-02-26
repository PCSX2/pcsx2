// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "LayoutEditorDialog.h"

#include "Debugger/Docking/DockTables.h"

#include <QtWidgets/QPushButton>

Q_DECLARE_METATYPE(LayoutEditorDialog::InitialState);

LayoutEditorDialog::LayoutEditorDialog(NameValidator name_validator, bool can_clone_current_layout, QWidget* parent)
	: QDialog(parent)
	, m_name_validator(name_validator)
{
	m_ui.setupUi(this);

	setWindowTitle(tr("New Layout"));

	setupInputWidgets(BREAKPOINT_EE, can_clone_current_layout);

	onNameChanged();
}

LayoutEditorDialog::LayoutEditorDialog(
	const QString& name, BreakPointCpu cpu, NameValidator name_validator, QWidget* parent)
	: QDialog(parent)
	, m_name_validator(name_validator)
{
	m_ui.setupUi(this);

	setWindowTitle(tr("Edit Layout"));

	m_ui.nameEditor->setText(name);

	setupInputWidgets(cpu, {});

	m_ui.initialStateLabel->hide();
	m_ui.initialStateEditor->hide();

	onNameChanged();
}

QString LayoutEditorDialog::name()
{
	return m_ui.nameEditor->text();
}

BreakPointCpu LayoutEditorDialog::cpu()
{
	return static_cast<BreakPointCpu>(m_ui.cpuEditor->currentData().toInt());
}

LayoutEditorDialog::InitialState LayoutEditorDialog::initialState()
{
	return m_ui.initialStateEditor->currentData().value<InitialState>();
}

void LayoutEditorDialog::setupInputWidgets(BreakPointCpu cpu, bool can_clone_current_layout)
{
	connect(m_ui.nameEditor, &QLineEdit::textChanged, this, &LayoutEditorDialog::onNameChanged);

	for (BreakPointCpu cpu : DEBUG_CPUS)
	{
		const char* long_cpu_name = DebugInterface::longCpuName(cpu);
		const char* cpu_name = DebugInterface::cpuName(cpu);
		QString text = QString("%1 (%2)").arg(long_cpu_name).arg(cpu_name);
		m_ui.cpuEditor->addItem(text, cpu);
	}

	for (int i = 0; i < m_ui.cpuEditor->count(); i++)
		if (m_ui.cpuEditor->itemData(i).toInt() == cpu)
			m_ui.cpuEditor->setCurrentIndex(i);

	for (size_t i = 0; i < DockTables::DEFAULT_DOCK_LAYOUTS.size(); i++)
		m_ui.initialStateEditor->addItem(
			tr("Create Default \"%1\" Layout").arg(tr(DockTables::DEFAULT_DOCK_LAYOUTS[i].name.c_str())),
			QVariant::fromValue(InitialState(DEFAULT_LAYOUT, i)));

	m_ui.initialStateEditor->addItem(tr("Create Blank Layout"), QVariant::fromValue(InitialState(BLANK_LAYOUT, 0)));

	if (can_clone_current_layout)
		m_ui.initialStateEditor->addItem(tr("Clone Current Layout"), QVariant::fromValue(InitialState(CLONE_LAYOUT, 0)));

	m_ui.initialStateEditor->setCurrentIndex(0);
}

void LayoutEditorDialog::onNameChanged()
{
	QString error_message;

	if (m_ui.nameEditor->text().isEmpty())
	{
		error_message = tr("Name is empty.");
	}
	else if (m_ui.nameEditor->text().size() > DockUtils::MAX_LAYOUT_NAME_SIZE)
	{
		error_message = tr("Name too long.");
	}
	else if (!m_name_validator(m_ui.nameEditor->text()))
	{
		error_message = tr("A layout with that name already exists.");
	}

	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(error_message.isEmpty());
	m_ui.errorMessage->setText(error_message);
}
