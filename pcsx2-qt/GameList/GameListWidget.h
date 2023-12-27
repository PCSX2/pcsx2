// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_EmptyGameListWidget.h"
#include "ui_GameListWidget.h"

#include "pcsx2/GameList.h"

#include <QtWidgets/QListView>
#include <QtWidgets/QTableView>

Q_DECLARE_METATYPE(const GameList::Entry*);

class GameListModel;
class GameListSortModel;
class GameListRefreshThread;

class GameListGridListView : public QListView
{
	Q_OBJECT

public:
	GameListGridListView(QWidget* parent = nullptr);

Q_SIGNALS:
	void zoomOut();
	void zoomIn();

protected:
	void wheelEvent(QWheelEvent* e);
};

class GameListWidget : public QWidget
{
	Q_OBJECT

public:
	GameListWidget(QWidget* parent = nullptr);
	~GameListWidget();

	__fi GameListModel* getModel() const { return m_model; }

	void initialize();
	void resizeTableViewColumnsToFit();

	void refresh(bool invalidate_cache);
	void cancelRefresh();
	void reloadThemeSpecificImages();

	bool isShowingGameList() const;
	bool isShowingGameGrid() const;
	bool getShowGridCoverTitles() const;

	const GameList::Entry* getSelectedEntry() const;

	/// Rescans a single file. NOTE: Happens on UI thread.
	void rescanFile(const std::string& path);

Q_SIGNALS:
	void refreshProgress(const QString& status, int current, int total);
	void refreshComplete();

	void selectionChanged();
	void entryActivated();
	void entryContextMenuRequested(const QPoint& point);

	void addGameDirectoryRequested();
	void layoutChange();

private Q_SLOTS:
	void onRefreshProgress(const QString& status, int current, int total);
	void onRefreshComplete();

	void onSelectionModelCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
	void onTableViewItemActivated(const QModelIndex& index);
	void onTableViewContextMenuRequested(const QPoint& point);
	void onTableViewHeaderContextMenuRequested(const QPoint& point);
	void onTableViewHeaderSortIndicatorChanged(int, Qt::SortOrder);
	void onListViewItemActivated(const QModelIndex& index);
	void onListViewContextMenuRequested(const QPoint& point);
	void onCoverScaleChanged();

public Q_SLOTS:
	void showGameList();
	void showGameGrid();
	void setShowCoverTitles(bool enabled);
	void gridZoomIn();
	void gridZoomOut();
	void gridIntScale(int int_scale);
	void refreshGridCovers();

protected:
	void resizeEvent(QResizeEvent* event);

private:
	void loadTableViewColumnVisibilitySettings();
	void saveTableViewColumnVisibilitySettings();
	void saveTableViewColumnVisibilitySettings(int column);
	void loadTableViewColumnSortSettings();
	void saveTableViewColumnSortSettings();
	void listZoom(float delta);
	void updateToolbar();

	Ui::GameListWidget m_ui;

	GameListModel* m_model = nullptr;
	GameListSortModel* m_sort_model = nullptr;
	QTableView* m_table_view = nullptr;
	GameListGridListView* m_list_view = nullptr;

	QWidget* m_empty_widget = nullptr;
	Ui::EmptyGameListWidget m_empty_ui;

	GameListRefreshThread* m_refresh_thread = nullptr;
};
