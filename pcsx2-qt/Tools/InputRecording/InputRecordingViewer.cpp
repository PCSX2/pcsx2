// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "InputRecordingViewer.h"

#include "QtUtils.h"
#include <QtCore/QString>
#include <QtWidgets/QDialog>
#include <QtWidgets/qfiledialog.h>

// TODO - for now this uses a very naive implementation that fills the entire table
// this needs to be replaced with a lazy-loading QTableView implementation
//
// For now, especially for just debugging input recording issues, its good enough!

InputRecordingViewer::InputRecordingViewer(QWidget* parent)
	: QMainWindow(parent)
{
	m_ui.setupUi(this);

	m_ui.tableWidget->setSelectionMode(QAbstractItemView::NoSelection);

	connect(m_ui.actionOpen, &QAction::triggered, this, &InputRecordingViewer::openFile);
	connect(m_ui.actionClose, &QAction::triggered, this, &InputRecordingViewer::closeFile);
}

QTableWidgetItem* InputRecordingViewer::createRowItem(std::tuple<u8, u8> analog)
{
	const auto [left, right] = analog;
	return new QTableWidgetItem(tr("%1 %2").arg(left).arg(right));
}

QTableWidgetItem* InputRecordingViewer::createRowItem(bool pressed)
{
	return new QTableWidgetItem(tr("%1").arg(pressed));
}

QTableWidgetItem* InputRecordingViewer::createRowItem(std::tuple<bool, u8> buttonInfo)
{
	const auto [isPressed, pressure] = buttonInfo;
	return new QTableWidgetItem(tr("%1 [%2]").arg(isPressed).arg(pressure));
}

void InputRecordingViewer::loadTable()
{
	static const auto headers = QStringList({tr("Left Analog"), tr("Right Analog"), tr("Cross"), tr("Square"), tr("Triangle"), tr("Circle"), tr("L1"), tr("R1"), tr("L2"), tr("R2"), tr("D-Pad Down"), tr("D-Pad Right"), tr("D-Pad Up"), tr("D-Pad Left"), tr("L3"), tr("R3"), tr("Select"), tr("Start")});
	m_ui.tableWidget->setColumnCount(headers.length());
	m_ui.tableWidget->setHorizontalHeaderLabels(headers);

	// TODO - only port 1 for now
	auto dataColl = m_file.bulkReadPadData(0, m_file.getTotalFrames(), 0);
	m_ui.tableWidget->setRowCount(dataColl.size());

	int frameNum = 0;
	for (const auto& frameData : dataColl)
	{
		m_ui.tableWidget->setItem(frameNum, 0, createRowItem(frameData.m_leftAnalog));
		m_ui.tableWidget->setItem(frameNum, 1, createRowItem(frameData.m_rightAnalog));
		m_ui.tableWidget->setItem(frameNum, 2, createRowItem(frameData.m_cross));
		m_ui.tableWidget->setItem(frameNum, 3, createRowItem(frameData.m_square));
		m_ui.tableWidget->setItem(frameNum, 4, createRowItem(frameData.m_triangle));
		m_ui.tableWidget->setItem(frameNum, 5, createRowItem(frameData.m_circle));
		m_ui.tableWidget->setItem(frameNum, 6, createRowItem(frameData.m_l1));
		m_ui.tableWidget->setItem(frameNum, 7, createRowItem(frameData.m_l2));
		m_ui.tableWidget->setItem(frameNum, 8, createRowItem(frameData.m_r1));
		m_ui.tableWidget->setItem(frameNum, 9, createRowItem(frameData.m_r2));
		m_ui.tableWidget->setItem(frameNum, 10, createRowItem(frameData.m_down));
		m_ui.tableWidget->setItem(frameNum, 11, createRowItem(frameData.m_right));
		m_ui.tableWidget->setItem(frameNum, 12, createRowItem(frameData.m_up));
		m_ui.tableWidget->setItem(frameNum, 13, createRowItem(frameData.m_left));
		m_ui.tableWidget->setItem(frameNum, 14, createRowItem(frameData.m_l3));
		m_ui.tableWidget->setItem(frameNum, 15, createRowItem(frameData.m_r3));
		m_ui.tableWidget->setItem(frameNum, 16, createRowItem(frameData.m_select));
		m_ui.tableWidget->setItem(frameNum, 17, createRowItem(frameData.m_select));
		frameNum++;
	}
}

void InputRecordingViewer::openFile()
{
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setWindowTitle("Select a File");
	dialog.setNameFilter(tr("Input Recording Files (*.p2m2)"));
	QStringList fileNames;
	if (dialog.exec())
	{
		fileNames = dialog.selectedFiles();
	}
	if (!fileNames.isEmpty())
	{
		const std::string fileName = fileNames.first().toStdString();
		m_file_open = m_file.openExisting(fileName);
		m_ui.actionClose->setEnabled(m_file_open);
		if (m_file_open)
		{
			loadTable();
		} // TODO else error
	}
}

void InputRecordingViewer::closeFile()
{
	if (m_file_open)
	{
		m_file_open = !m_file.close();
		if (!m_file_open)
		{
			m_ui.tableWidget->clearContents();
			m_ui.tableWidget->setRowCount(0);
		}
	} // TODO else error
	m_ui.actionClose->setEnabled(m_file_open);
}
