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

#pragma once

#include "ui_InputRecordingViewer.h"

#include "pcsx2/Recording/InputRecordingFile.h"

class InputRecordingViewer final : public QMainWindow
{
	Q_OBJECT

public:
	explicit InputRecordingViewer(QWidget* parent = nullptr);
	~InputRecordingViewer() = default;

private Q_SLOTS:
	void openFile();
	void closeFile();

private:
	Ui::InputRecordingViewer m_ui;

	InputRecordingFile m_file;
	bool m_file_open;

	void loadTable();
	QTableWidgetItem* createRowItem(std::tuple<u8, u8> analog);
	QTableWidgetItem* createRowItem(bool pressed);
	QTableWidgetItem* createRowItem(std::tuple<bool, u8> buttonInfo);
};
