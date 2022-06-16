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
}

void InputRecordingViewer::loadTable()
{
	// TODO - only port 1 for now
	auto data = m_file.bulkReadPadData(0, m_file.getTotalFrames(), 0);
	m_ui.tableWidget->setRowCount(data.size());
	auto headers = QStringList({"Left Analog", "Right Analog", "Cross", "Square", "Triangle", "Circle", "L1", "R1", "L2", "R2", "⬇️", "➡️", "⬆️", "⬅️", "L3", "R3", "Select", "Start"});
	m_ui.tableWidget->setColumnCount(headers.length());
	m_ui.tableWidget->setHorizontalHeaderLabels(headers);

	int frameNum = 0;
	for (auto& const frame : data)
	{
		// TODO - disgusting, clean it up
		m_ui.tableWidget->setItem(frameNum, 0, new QTableWidgetItem(tr("%1 %2").arg(frame.leftAnalogX).arg(frame.leftAnalogY)));
		m_ui.tableWidget->setItem(frameNum, 1, new QTableWidgetItem(tr("%1 %2").arg(frame.rightAnalogX).arg(frame.rightAnalogY)));
		m_ui.tableWidget->setItem(frameNum, 2, new QTableWidgetItem(tr("%1 [%2]").arg(frame.crossPressed).arg(frame.crossPressure)));
		m_ui.tableWidget->setItem(frameNum, 3, new QTableWidgetItem(tr("%1 [%2]").arg(frame.squarePressed).arg(frame.squarePressure)));
		m_ui.tableWidget->setItem(frameNum, 4, new QTableWidgetItem(tr("%1 [%2]").arg(frame.trianglePressed).arg(frame.trianglePressure)));
		m_ui.tableWidget->setItem(frameNum, 5, new QTableWidgetItem(tr("%1 [%2]").arg(frame.circlePressed).arg(frame.circlePressure)));
		m_ui.tableWidget->setItem(frameNum, 6, new QTableWidgetItem(tr("%1 [%2]").arg(frame.l1Pressed).arg(frame.l1Pressure)));
		m_ui.tableWidget->setItem(frameNum, 7, new QTableWidgetItem(tr("%1 [%2]").arg(frame.l2Pressed).arg(frame.l2Pressure)));
		m_ui.tableWidget->setItem(frameNum, 8, new QTableWidgetItem(tr("%1 [%2]").arg(frame.r1Pressed).arg(frame.r1Pressure)));
		m_ui.tableWidget->setItem(frameNum, 9, new QTableWidgetItem(tr("%1 [%2]").arg(frame.r1Pressed).arg(frame.r2Pressure)));
		m_ui.tableWidget->setItem(frameNum, 10, new QTableWidgetItem(tr("%1 [%2]").arg(frame.downPressed).arg(frame.downPressure)));
		m_ui.tableWidget->setItem(frameNum, 11, new QTableWidgetItem(tr("%1 [%2]").arg(frame.rightPressed).arg(frame.rightPressure)));
		m_ui.tableWidget->setItem(frameNum, 12, new QTableWidgetItem(tr("%1 [%2]").arg(frame.upPressed).arg(frame.upPressure)));
		m_ui.tableWidget->setItem(frameNum, 13, new QTableWidgetItem(tr("%1 [%2]").arg(frame.leftPressed).arg(frame.leftPressure)));
		m_ui.tableWidget->setItem(frameNum, 14, new QTableWidgetItem(tr("%1").arg(frame.l3)));
		m_ui.tableWidget->setItem(frameNum, 15, new QTableWidgetItem(tr("%1").arg(frame.r3)));
		m_ui.tableWidget->setItem(frameNum, 16, new QTableWidgetItem(tr("%1").arg(frame.select)));
		m_ui.tableWidget->setItem(frameNum, 17, new QTableWidgetItem(tr("%1").arg(frame.start)));
		frameNum++;
	}
}

void InputRecordingViewer::openFile() {
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
		std::string fileName = fileNames.first().toStdString();
		m_file.OpenExisting(fileName);
		loadTable();
	}
}