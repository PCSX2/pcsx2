// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "ui_DebugAnalysisSettingsWidget.h"

#include "Config.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QDialog>

class SettingsWindow;

class DebugAnalysisSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	// Create a widget that will discard any settings changed after it is
	// closed, for use in the dialog opened by the "Analyze" button.
	DebugAnalysisSettingsWidget(QWidget* parent = nullptr);

	// Create a widget that will write back any settings changed to the config
	// system, for use in the settings dialog.
	DebugAnalysisSettingsWidget(SettingsWindow* dialog, QWidget* parent = nullptr);

	// Read all the analysis settings from the widget tree and store them in the
	// output object. This is used by the analysis options dialog to start an
	// analysis run manually.
	void parseSettingsFromWidgets(Pcsx2Config::DebugAnalysisOptions& output);

protected:
	void setupSymbolSourceGrid();
	void symbolSourceCheckStateChanged();
	void saveSymbolSources();

	void setupSymbolFileList();
	void addSymbolFile();
	void removeSymbolFile();
	void saveSymbolFiles();

	void functionScanRangeChanged();

	void updateEnabledStates();

	std::string getStringSettingValue(const char* section, const char* key, const char* default_value = "");
	bool getBoolSettingValue(const char* section, const char* key, bool default_value = false);
	int getIntSettingValue(const char* section, const char* key, int default_value = 0);

	struct SymbolSourceTemp
	{
		QCheckBox* check_box = nullptr;
		bool previous_value = false;
		bool modified_by_user = false;
	};

	enum SymbolFileColumn
	{
		PATH_COLUMN = 0,
		BASE_ADDRESS_COLUMN = 1,
		CONDITION_COLUMN = 2,
		SYMBOL_FILE_COLUMN_COUNT = 3
	};

	SettingsWindow* m_dialog = nullptr;
	std::map<std::string, SymbolSourceTemp> m_symbol_sources;

	QStandardItemModel* m_symbol_file_model;

	Ui::DebugAnalysisSettingsWidget m_ui;
};
