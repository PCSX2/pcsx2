// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
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

// TODO: Move to core.
struct BIOSInfo
{
	std::string filename;
	std::string description;
	std::string zone;
	u32 version;
	u32 region;
};
Q_DECLARE_METATYPE(BIOSInfo);

class BIOSSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	BIOSSettingsWidget(SettingsWindow* dialog, QWidget* parent);
	~BIOSSettingsWidget();

	class RefreshThread final : public QThread
	{
	public:
		RefreshThread(QWidget* parent, const QString& directory);
		~RefreshThread();

	protected:
		void run() override;

	private:
		QString m_directory;
	};

	static void populateList(QTreeWidget* list, const QVector<BIOSInfo>& items);

private Q_SLOTS:
	void refreshList();

	void listItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous);
	void listRefreshed(const QVector<BIOSInfo>& items);

	void fastBootChanged();

private:
	Ui::BIOSSettingsWidget m_ui;
	SettingsWindow* m_dialog;

	RefreshThread* m_refresh_thread = nullptr;
};
