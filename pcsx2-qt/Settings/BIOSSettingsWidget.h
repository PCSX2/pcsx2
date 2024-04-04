// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QVector>
#include <QtWidgets/QWidget>
#include <string>

#include "ui_BIOSSettingsWidget.h"

class SettingsWindow;
class QThread;

class BIOSSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	BIOSSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~BIOSSettingsWidget();

	static void populateList(QTreeWidget* list, const std::string& directory);

private Q_SLOTS:
	void refreshList();

	void listItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous);

	void fastBootChanged();

private:
	Ui::BIOSSettingsWidget m_ui;
	SettingsWindow* m_dialog;
};
