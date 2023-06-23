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
#include <QtCore/QDir>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QThread>
#include <QtCore/QVector>
#include <QtWidgets/QWidget>
#include <string>

#include "ui_BIOSSettingsWidget.h"

class SettingsDialog;
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
	BIOSSettingsWidget(SettingsDialog* dialog, QWidget* parent);
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
	SettingsDialog* m_dialog;

	RefreshThread* m_refresh_thread = nullptr;
};
