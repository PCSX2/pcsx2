// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <QtWidgets/QDialog>

#include <mutex>

#include "Models/BreakpointModel.h"
#include "Models/SavedAddressesModel.h"

class DebuggerSettingsManager final
{
public:
	DebuggerSettingsManager(QWidget* parent = nullptr);
	~DebuggerSettingsManager();

	static void loadGameSettings(BreakpointModel* bpModel);
	static void loadGameSettings(SavedAddressesModel* savedAddressesModel);
	static void saveGameSettings(BreakpointModel* bpModel);
	static void saveGameSettings(SavedAddressesModel* savedAddressesModel);
	static void saveGameSettings(QAbstractTableModel* abstractTableModel, QString settingsKey, u32 role);

private:
	static std::mutex writeLock;
	static void writeJSONToPath(std::string path, QJsonDocument jsonDocument);
	static QJsonObject loadGameSettingsJSON();
	const static QString settingsFileVersion;
};
