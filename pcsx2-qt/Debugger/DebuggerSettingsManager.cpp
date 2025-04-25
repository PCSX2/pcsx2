// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerSettingsManager.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QFile>

#include "common/Console.h"
#include "VMManager.h"

std::mutex DebuggerSettingsManager::writeLock;
const QString DebuggerSettingsManager::settingsFileVersion = "0.01";

QJsonObject DebuggerSettingsManager::loadGameSettingsJSON()
{
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

	// Breakpoint descriptions were added at debugger settings file version 0.01. If loading
	// saved breakpoints from a previous version (only 0.00 existed prior), the breakpoints will be
	// missing a description. This code will add in an empty description so that the previous
	// version, 0.00, is compatible with 0.01.
	bool isMissingDescription = false;
	const QJsonValue savedVersionValue = loadGameSettingsJSON().value("Version");
	if (!savedVersionValue.isUndefined())
	{
		isMissingDescription = savedVersionValue.toString().toStdString() == "0.00";
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
		QJsonObject rowObject = rowValue.toObject();

		// Add empty description for saved breakpoints from debugger settings versions prior to 0.01
		if (isMissingDescription)
		{
			rowObject.insert(QString("DESCRIPTION"), QJsonValue(""));
		}

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
