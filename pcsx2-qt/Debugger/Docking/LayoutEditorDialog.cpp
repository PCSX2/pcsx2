// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "LayoutEditorDialog.h"

LayoutEditorDialog::LayoutEditorDialog(QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowTitle(tr("New Layout"));

	setupComboBoxes(BREAKPOINT_EE, DockManager::DEFAULT_LAYOUT);
}

LayoutEditorDialog::LayoutEditorDialog(
	std::string& name, BreakPointCpu cpu, QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowTitle(tr("Edit Layout"));

	m_ui.nameEditor->setText(QString::fromStdString(name));

	setupComboBoxes(cpu, DockManager::DEFAULT_LAYOUT);

	m_ui.initialStateLabel->hide();
	m_ui.initialStateEditor->hide();
}

std::string LayoutEditorDialog::name()
{
	return m_ui.nameEditor->text().toStdString();
}

BreakPointCpu LayoutEditorDialog::cpu()
{
	return static_cast<BreakPointCpu>(m_ui.cpuEditor->currentData().toInt());
}

DockManager::LayoutCreationMode LayoutEditorDialog::initial_state()
{
	return static_cast<DockManager::LayoutCreationMode>(m_ui.initialStateEditor->currentData().toInt());
}

void LayoutEditorDialog::setupComboBoxes(BreakPointCpu cpu, DockManager::LayoutCreationMode initial_state)
{
	m_ui.cpuEditor->addItem(tr("EE"), BREAKPOINT_EE);
	m_ui.cpuEditor->addItem(tr("IOP"), BREAKPOINT_IOP);

	for (int i = 0; i < m_ui.cpuEditor->count(); i++)
		if (m_ui.cpuEditor->itemData(i).toInt() == cpu)
			m_ui.cpuEditor->setCurrentIndex(i);

	m_ui.initialStateEditor->addItem(tr("Create Default Layout"), DockManager::DEFAULT_LAYOUT);
	m_ui.initialStateEditor->addItem(tr("Create Blank Layout"), DockManager::BLANK_LAYOUT);
	m_ui.initialStateEditor->addItem(tr("Clone Current Layout"), DockManager::CLONE_LAYOUT);

	for (int i = 0; i < m_ui.initialStateEditor->count(); i++)
		if (m_ui.initialStateEditor->itemData(i).toInt() == initial_state)
			m_ui.initialStateEditor->setCurrentIndex(i);
}
