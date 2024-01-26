// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "DebuggerSettingsManager.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QFile>

#include "common/Console.h"
#include "fmt/core.h"
#include "VMManager.h"
#include "Models/BreakpointModel.h"

std::mutex DebuggerSettingsManager::writeLock;
const QString DebuggerSettingsManager::settingsFileVersion = "0.00";

QJsonObject DebuggerSettingsManager::loadGameSettingsJSON() {
	std::string path = VMManager::GetDebuggerSettingsFilePathForCurrentGame();
	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::ReadOnly))
	{
		Console.WriteLnFmt("Debugger Settings Manager: No Debugger Settings file found for game at: '{}'", path);
		return QJsonObject();
	}
	QByteArray fileContent = file.readAll();
	file.close();

	const QJsonDocument jsonDoc(QJsonDocument::fromJson(fileContent));
	if (jsonDoc.isNull() || !jsonDoc.isObject())
	{
		Console.WriteLnFmt("Debugger Settings Manager: Failed to load contents of settings file for file at: '{}'", path);
		return QJsonObject();
	}

	return jsonDoc.object();
}

void DebuggerSettingsManager::writeJSONToPath(std::string path, QJsonDocument jsonDocument)
{
	QFile file(QString::fromStdString(path));
	if (!file.open(QIODevice::WriteOnly))
	{
		Console.WriteLnFmt("Debugger Settings Manager: Failed to write Debugger Settings file to path: '{}'", path);
		return;
	}
	file.write(jsonDocument.toJson(QJsonDocument::Indented));
	file.close();
}

void DebuggerSettingsManager::loadGameSettings(BreakpointModel* bpModel)
{
	const std::string path = VMManager::GetDebuggerSettingsFilePathForCurrentGame();
	if (path.empty())
		return;

	const QJsonValue breakpointsValue = loadGameSettingsJSON().value("Breakpoints");
	const QString valueToLoad = breakpointsValue.toString();
	if (breakpointsValue.isUndefined() || !breakpointsValue.isArray())
	{
		Console.WriteLnFmt("Debugger Settings Manager: Failed to read Breakpoints array from settings file: '{}'", path);
		return;
	}

	const QJsonArray breakpointsArray = breakpointsValue.toArray();
	for (u32 row = 0; row < breakpointsArray.size(); row++)
	{
		const QJsonValue rowValue = breakpointsArray.at(row);
		if (rowValue.isUndefined() || !rowValue.isObject())
		{
			Console.WriteLn("Debugger Settings Manager: Failed to load invalid Breakpoint object.");
			continue;
		}
		const QJsonObject rowObject = rowValue.toObject();

		QStringList fields;
		u32 col = 0;
		for (auto iter = rowObject.begin(); iter != rowObject.end(); iter++, col++)
		{
			QString headerColKey = bpModel->headerData(col, Qt::Horizontal, Qt::UserRole).toString();
			fields << rowObject.value(headerColKey).toString();
		}
		bpModel->loadBreakpointFromFieldList(fields);
	}
}

void DebuggerSettingsManager::loadGameSettings(SavedAddressesModel* savedAddressesModel)
{
	const std::string path = VMManager::GetDebuggerSettingsFilePathForCurrentGame();
	if (path.empty())
		return;

	const QJsonValue savedAddressesValue = loadGameSettingsJSON().value("SavedAddresses");
	QString valueToLoad = savedAddressesValue.toString();
	if (savedAddressesValue.isUndefined() || !savedAddressesValue.isArray())
	{
		Console.WriteLnFmt("Debugger Settings Manager: Failed to read Saved Addresses array from settings file: '{}'", path);
		return;
	}

	const QJsonArray breakpointsArray = savedAddressesValue.toArray();

	for (u32 row = 0; row < breakpointsArray.size(); row++)
	{
		const QJsonValue rowValue = breakpointsArray.at(row);
		if (rowValue.isUndefined() || !rowValue.isObject())
		{
			Console.WriteLn("Debugger Settings Manager: Failed to load invalid Breakpoint object.");
			continue;
		}
		const QJsonObject rowObject = rowValue.toObject();
		QStringList fields;
		u32 col = 0;
		for (auto iter = rowObject.begin(); iter != rowObject.end(); iter++, col++)
		{
			QString headerColKey = savedAddressesModel->headerData(col, Qt::Horizontal, Qt::UserRole).toString();
			fields << rowObject.value(headerColKey).toString();
		}
		savedAddressesModel->loadSavedAddressFromFieldList(fields);
	}
}

void DebuggerSettingsManager::saveGameSettings(BreakpointModel* bpModel)
{
	saveGameSettings(bpModel, "Breakpoints", BreakpointModel::ExportRole);
}

void DebuggerSettingsManager::saveGameSettings(SavedAddressesModel* savedAddressesModel)
{
	saveGameSettings(savedAddressesModel, "SavedAddresses", Qt::DisplayRole);
}

void DebuggerSettingsManager::saveGameSettings(QAbstractTableModel* abstractTableModel, QString settingsKey, u32 role)
{
	const std::string path = VMManager::GetDebuggerSettingsFilePathForCurrentGame();
	if (path.empty())
		return; 

	const std::lock_guard<std::mutex> lock(writeLock);
	QJsonObject loadedSettings = loadGameSettingsJSON();
	QJsonArray rowsArray;
	QStringList keys;
	for (int col = 0; col < abstractTableModel->columnCount(); ++col)
	{
		keys << abstractTableModel->headerData(col, Qt::Horizontal, Qt::UserRole).toString();
	}

	for (int row = 0; row < abstractTableModel->rowCount(); row++)
	{
		QJsonObject rowObject;
		for (int col = 0; col < abstractTableModel->columnCount(); col++)
		{
			const QModelIndex index = abstractTableModel->index(row, col);
			const QString data = abstractTableModel->data(index, role).toString();
			rowObject.insert(keys[col], QJsonValue::fromVariant(data));
		}
		rowsArray.append(rowObject);
	}
	loadedSettings.insert(settingsKey, rowsArray);
	loadedSettings.insert("Version", settingsFileVersion);
	QJsonDocument doc(loadedSettings);
	writeJSONToPath(path, doc);
}
