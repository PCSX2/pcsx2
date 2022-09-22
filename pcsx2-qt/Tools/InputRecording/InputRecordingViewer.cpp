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
	static const auto headers = QStringList({"Left Analog", "Right Analog", "Cross", "Square", "Triangle", "Circle", "L1", "R1", "L2", "R2", "⬇️", "➡️", "⬆️", "⬅️", "L3", "R3", "Select", "Start"});
	m_ui.tableWidget->setColumnCount(headers.length());
	m_ui.tableWidget->setHorizontalHeaderLabels(headers);

	// TODO - only port 1 for now
	auto data = m_file.bulkReadPadData(0, m_file.getTotalFrames(), 0);
	m_ui.tableWidget->setRowCount(data.size());
	
	static constexpr auto constructItem_analog = [](u8 analogX, u8 analogY) {
		return new QTableWidgetItem(tr("%1 %2").arg(analogX).arg(analogY));
	};
	static constexpr auto constructItem_pressed = [](bool pressed) {
		return new QTableWidgetItem(tr("%1").arg(pressed));
	};
	static constexpr auto constructItem_pressured = [](bool pressed, u8 pressure) {
		return new QTableWidgetItem(tr("%1 [%2]").arg(pressed).arg(pressure));
	};

	int frameNum = 0;
	for (const auto& frame : data)
	{
		// TODO - disgusting, clean it up
		m_ui.tableWidget->setItem(frameNum, 0,  constructItem_analog   (frame.m_leftAnalogX, frame.m_leftAnalogY));
		m_ui.tableWidget->setItem(frameNum, 1,  constructItem_analog   (frame.m_rightAnalogX, frame.m_rightAnalogY));
		m_ui.tableWidget->setItem(frameNum, 2,  constructItem_pressured(frame.m_crossPressed.m_pressed, frame.m_crossPressure));
		m_ui.tableWidget->setItem(frameNum, 3,  constructItem_pressured(frame.m_squarePressed.m_pressed, frame.m_squarePressure));
		m_ui.tableWidget->setItem(frameNum, 4,  constructItem_pressured(frame.m_trianglePressed.m_pressed, frame.m_trianglePressure));
		m_ui.tableWidget->setItem(frameNum, 5,  constructItem_pressured(frame.m_circlePressed.m_pressed, frame.m_circlePressure));
		m_ui.tableWidget->setItem(frameNum, 6,  constructItem_pressured(frame.m_l1Pressed.m_pressed, frame.m_l1Pressure));
		m_ui.tableWidget->setItem(frameNum, 7,  constructItem_pressured(frame.m_l2Pressed.m_pressed, frame.m_l2Pressure));
		m_ui.tableWidget->setItem(frameNum, 8,  constructItem_pressured(frame.m_r1Pressed.m_pressed, frame.m_r1Pressure));
		m_ui.tableWidget->setItem(frameNum, 9,  constructItem_pressured(frame.m_r1Pressed.m_pressed, frame.m_r2Pressure));
		m_ui.tableWidget->setItem(frameNum, 10, constructItem_pressured(frame.m_downPressed.m_pressed, frame.m_downPressure));
		m_ui.tableWidget->setItem(frameNum, 11, constructItem_pressured(frame.m_rightPressed.m_pressed, frame.m_rightPressure));
		m_ui.tableWidget->setItem(frameNum, 12, constructItem_pressured(frame.m_upPressed.m_pressed, frame.m_upPressure));
		m_ui.tableWidget->setItem(frameNum, 13, constructItem_pressured(frame.m_leftPressed.m_pressed, frame.m_leftPressure));
		m_ui.tableWidget->setItem(frameNum, 14, constructItem_pressed  (frame.m_l3.m_pressed));
		m_ui.tableWidget->setItem(frameNum, 15, constructItem_pressed  (frame.m_r3.m_pressed));
		m_ui.tableWidget->setItem(frameNum, 16, constructItem_pressed  (frame.m_select.m_pressed));
		m_ui.tableWidget->setItem(frameNum, 17, constructItem_pressed  (frame.m_start.m_pressed));
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
		const std::string fileName = fileNames.first().toStdString();
		m_file.OpenExisting(fileName);
		loadTable();
	}
}