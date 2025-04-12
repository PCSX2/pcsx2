// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_SavedAddressesView.h"

#include "SavedAddressesModel.h"

#include "Debugger/DebuggerView.h"

class SavedAddressesView : public DebuggerView
{
	Q_OBJECT

public:
	SavedAddressesView(const DebuggerViewParameters& parameters);

	void openContextMenu(QPoint pos);
	void contextPasteCSV();
	void contextNew();
	void addAddress(u32 address);
	void saveToDebuggerSettings();

private:
	Ui::SavedAddressesView m_ui;

	SavedAddressesModel* m_model;
};
