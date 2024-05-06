// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_SetupWizardDialog.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtWidgets/QDialog>

#include "common/Pcsx2Defs.h"

class SetupWizardDialog final : public QDialog
{
	Q_OBJECT

public:
	SetupWizardDialog();
	~SetupWizardDialog();

private Q_SLOTS:
	bool canShowNextPage();
	void previousPage();
	void nextPage();
	void confirmCancel();

	void themeChanged();
	void languageChanged();

	void refreshBiosList();
	void biosListItemChanged(const QTreeWidgetItem* current, const QTreeWidgetItem* previous);

	void onDirectoryListContextMenuRequested(const QPoint& point);
	void onDirectoryListSelectionChanged();
	void onAddSearchDirectoryButtonClicked();
	void onRemoveSearchDirectoryButtonClicked();
	void refreshDirectoryList();
	void resizeDirectoryListColumns();

	void onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices);
	void onInputDeviceConnected(const QString& identifier, const QString& device_name);
	void onInputDeviceDisconnected(const QString& identifier);

protected:
	void resizeEvent(QResizeEvent* event);

private:
	enum Page : u32
	{
		Page_Language,
		Page_BIOS,
		Page_GameList,
		Page_Controller,
		Page_Complete,
		Page_Count,
	};

	void setupUi();
	void setupLanguagePage();
	void setupBIOSPage();
	void setupGameListPage();
	void setupControllerPage();

	void pageChangedTo(int page);
	void updatePageLabels(int prev_page);
	void updatePageButtons();

	void addPathToTable(const std::string& path, bool recursive);

	void openAutomaticMappingMenu(u32 port, QLabel* update_label);
	void doDeviceAutomaticBinding(u32 port, QLabel* update_label, const QString& device);

	Ui::SetupWizardDialog m_ui;

	std::array<QLabel*, Page_Count> m_page_labels;

	QList<QPair<QString, QString>> m_device_list;
};
