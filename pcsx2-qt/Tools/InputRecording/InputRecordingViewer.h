// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
