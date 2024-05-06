// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtCore/QAbstractTableModel>
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "GameListSettingsWidget.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

GameListSettingsWidget::GameListSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.preferEnglishGameList, "UI", "PreferEnglishGameList", false);
	connect(m_ui.preferEnglishGameList, &QCheckBox::checkStateChanged, this, [this]{ emit preferEnglishGameListChanged(); });

	dialog->registerWidgetHelp(m_ui.preferEnglishGameList, tr("Prefer English Titles"), tr("Unchecked"),
		tr("For games with both a title in the game's native language and one in English, prefer the English title."));

	m_ui.searchDirectoryList->setSelectionMode(QAbstractItemView::SingleSelection);
	m_ui.searchDirectoryList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.searchDirectoryList->setAlternatingRowColors(true);
	m_ui.searchDirectoryList->setShowGrid(false);
	m_ui.searchDirectoryList->horizontalHeader()->setHighlightSections(false);
	m_ui.searchDirectoryList->verticalHeader()->hide();
	m_ui.searchDirectoryList->setCurrentIndex({});
	m_ui.searchDirectoryList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

	connect(m_ui.searchDirectoryList, &QTableWidget::customContextMenuRequested, this, &GameListSettingsWidget::onDirectoryListContextMenuRequested);
	connect(m_ui.searchDirectoryList, &QTableWidget::itemSelectionChanged, this, &GameListSettingsWidget::onDirectoryListSelectionChanged);
	connect(m_ui.addSearchDirectoryButton, &QPushButton::clicked, this, &GameListSettingsWidget::onAddSearchDirectoryButtonClicked);
	connect(m_ui.removeSearchDirectoryButton, &QPushButton::clicked, this, &GameListSettingsWidget::onRemoveSearchDirectoryButtonClicked);
	connect(m_ui.addExcludedFile, &QPushButton::clicked, this, &GameListSettingsWidget::onAddExcludedFileButtonClicked);
	connect(m_ui.addExcludedPath, &QPushButton::clicked, this, &GameListSettingsWidget::onAddExcludedPathButtonClicked);
	connect(m_ui.removeExcludedPath, &QPushButton::clicked, this, &GameListSettingsWidget::onRemoveExcludedPathButtonClicked);
	connect(m_ui.excludedPaths, &QListWidget::itemSelectionChanged, this, &GameListSettingsWidget::onExcludedPathsSelectionChanged);
	connect(m_ui.rescanAllGames, &QPushButton::clicked, this, &GameListSettingsWidget::onRescanAllGamesClicked);
	connect(m_ui.scanForNewGames, &QPushButton::clicked, this, &GameListSettingsWidget::onScanForNewGamesClicked);

	refreshDirectoryList();
	refreshExclusionList();
}

GameListSettingsWidget::~GameListSettingsWidget() = default;

bool GameListSettingsWidget::addExcludedPath(const std::string& path)
{
	if (!Host::AddBaseValueToStringList("GameList", "ExcludedPaths", path.c_str()))
		return false;

	Host::CommitBaseSettingChanges();
	m_ui.excludedPaths->addItem(QString::fromStdString(path));
	g_main_window->refreshGameList(false);
	return true;
}

void GameListSettingsWidget::refreshExclusionList()
{
	m_ui.excludedPaths->clear();

	const std::vector<std::string> paths(Host::GetBaseStringListSetting("GameList", "ExcludedPaths"));
	for (const std::string& path : paths)
		m_ui.excludedPaths->addItem(QString::fromStdString(path));

	m_ui.removeExcludedPath->setEnabled(false);
}

bool GameListSettingsWidget::event(QEvent* event)
{
	bool res = QWidget::event(event);

	switch (event->type())
	{
		case QEvent::LayoutRequest:
		case QEvent::Resize:
			QtUtils::ResizeColumnsForTableView(m_ui.searchDirectoryList, {-1, 100});
			break;

		default:
			break;
	}

	return res;
}

void GameListSettingsWidget::addPathToTable(const std::string& path, bool recursive)
{
	const int row = m_ui.searchDirectoryList->rowCount();
	m_ui.searchDirectoryList->insertRow(row);

	QTableWidgetItem* item = new QTableWidgetItem();
	item->setText(QString::fromStdString(path));
	item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
	m_ui.searchDirectoryList->setItem(row, 0, item);

	QCheckBox* cb = new QCheckBox(m_ui.searchDirectoryList);
	m_ui.searchDirectoryList->setCellWidget(row, 1, cb);
	cb->setChecked(recursive);

	connect(cb, &QCheckBox::checkStateChanged, this, [item](Qt::CheckState state) {
		const std::string path(item->text().toStdString());
		if (state == Qt::Checked)
		{
			Host::RemoveBaseValueFromStringList("GameList", "Paths", path.c_str());
			Host::AddBaseValueToStringList("GameList", "RecursivePaths", path.c_str());
		}
		else
		{
			Host::RemoveBaseValueFromStringList("GameList", "RecursivePaths", path.c_str());
			Host::AddBaseValueToStringList("GameList", "Paths", path.c_str());
		}
		Host::CommitBaseSettingChanges();
	});
}

void GameListSettingsWidget::refreshDirectoryList()
{
	QSignalBlocker sb(m_ui.searchDirectoryList);
	while (m_ui.searchDirectoryList->rowCount() > 0)
		m_ui.searchDirectoryList->removeRow(0);

	std::vector<std::string> path_list = Host::GetBaseStringListSetting("GameList", "Paths");
	for (const std::string& entry : path_list)
		addPathToTable(entry, false);

	path_list = Host::GetBaseStringListSetting("GameList", "RecursivePaths");
	for (const std::string& entry : path_list)
		addPathToTable(entry, true);

	m_ui.searchDirectoryList->sortByColumn(0, Qt::AscendingOrder);

	m_ui.removeSearchDirectoryButton->setEnabled(false);
}

void GameListSettingsWidget::addSearchDirectory(const QString& path, bool recursive)
{
	const std::string spath(path.toStdString());
	Host::RemoveBaseValueFromStringList("GameList", recursive ? "Paths" : "RecursivePaths", spath.c_str());
	Host::AddBaseValueToStringList("GameList", recursive ? "RecursivePaths" : "Paths", spath.c_str());
	Host::CommitBaseSettingChanges();
	refreshDirectoryList();
	g_main_window->refreshGameList(false);
}

void GameListSettingsWidget::removeSearchDirectory(const QString& path)
{
	const std::string spath(path.toStdString());
	if (!Host::RemoveBaseValueFromStringList("GameList", "Paths", spath.c_str()) &&
		!Host::RemoveBaseValueFromStringList("GameList", "RecursivePaths", spath.c_str()))
	{
		return;
	}

	Host::CommitBaseSettingChanges();
	refreshDirectoryList();
	g_main_window->refreshGameList(false);
}

void GameListSettingsWidget::onDirectoryListContextMenuRequested(const QPoint& point)
{
	QModelIndexList selection = m_ui.searchDirectoryList->selectionModel()->selectedIndexes();
	if (selection.size() < 1)
		return;

	const int row = selection[0].row();

	QMenu menu;
	menu.addAction(tr("Remove"), [this]() { onRemoveSearchDirectoryButtonClicked(); });
	menu.addSeparator();
	menu.addAction(tr("Open Directory..."), [this, row]() {
		QtUtils::OpenURL(this, QUrl::fromLocalFile(m_ui.searchDirectoryList->item(row, 0)->text()));
	});
	menu.exec(m_ui.searchDirectoryList->mapToGlobal(point));
}

void GameListSettingsWidget::onDirectoryListSelectionChanged()
{
	m_ui.removeSearchDirectoryButton->setEnabled(m_ui.searchDirectoryList->selectionModel()->hasSelection());
}

void GameListSettingsWidget::addSearchDirectory(QWidget* parent_widget)
{
	QString dir =
		QDir::toNativeSeparators(QFileDialog::getExistingDirectory(parent_widget, tr("Select Search Directory")));

	if (dir.isEmpty())
		return;

	QMessageBox::StandardButton selection =
		QMessageBox::question(this, tr("Scan Recursively?"),
			tr("Would you like to scan the directory \"%1\" recursively?\n\nScanning recursively takes "
			   "more time, but will identify files in subdirectories.")
				.arg(dir),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (selection == QMessageBox::Cancel)
		return;

	const bool recursive = (selection == QMessageBox::Yes);
	addSearchDirectory(dir, recursive);
}

void GameListSettingsWidget::onAddSearchDirectoryButtonClicked()
{
	addSearchDirectory(this);
}

void GameListSettingsWidget::onRemoveSearchDirectoryButtonClicked()
{
	const int row = m_ui.searchDirectoryList->currentRow();
	QTableWidgetItem* item = (row >= 0) ? m_ui.searchDirectoryList->takeItem(row, 0) : nullptr;
	if (!item)
		return;

	removeSearchDirectory(item->text());
	delete item;
}

void GameListSettingsWidget::onAddExcludedFileButtonClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this), tr("Select File")));
	if (path.isEmpty())
		return;

	addExcludedPath(path.toStdString());
}

void GameListSettingsWidget::onAddExcludedPathButtonClicked()
{
	QString path =
		QDir::toNativeSeparators(QFileDialog::getExistingDirectory(QtUtils::GetRootWidget(this), tr("Select Directory")));

	if (path.isEmpty())
		return;

	addExcludedPath(path.toStdString());
}

void GameListSettingsWidget::onRemoveExcludedPathButtonClicked()
{
	const int row = m_ui.excludedPaths->currentRow();
	QListWidgetItem* item = (row >= 0) ? m_ui.excludedPaths->takeItem(row) : 0;
	if (!item)
		return;

	if (Host::RemoveBaseValueFromStringList("GameList", "ExcludedPaths", item->text().toUtf8().constData()))
		Host::CommitBaseSettingChanges();

	delete item;

	g_main_window->refreshGameList(false);
}

void GameListSettingsWidget::onExcludedPathsSelectionChanged()
{
	m_ui.removeExcludedPath->setEnabled(!m_ui.excludedPaths->selectedItems().isEmpty());
}

void GameListSettingsWidget::onRescanAllGamesClicked()
{
	g_main_window->refreshGameList(true);
}

void GameListSettingsWidget::onScanForNewGamesClicked()
{
	g_main_window->refreshGameList(false);
}
