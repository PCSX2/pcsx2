// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_SavedAddressesWidget.h"

#include "SavedAddressesModel.h"

#include "Debugger/DebuggerWidget.h"

class SavedAddressesWidget : public DebuggerWidget
{
	Q_OBJECT

public:
	SavedAddressesWidget(const DebuggerWidgetParameters& parameters);

	void openContextMenu(QPoint pos);
	void contextPasteCSV();
	void contextNew();
	void addAddress(u32 address);
	void saveToDebuggerSettings();

private:
	Ui::SavedAddressesWidget m_ui;

	SavedAddressesModel* m_model;
};
