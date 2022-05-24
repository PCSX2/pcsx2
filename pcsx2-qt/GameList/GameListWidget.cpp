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

#include "PrecompiledHeader.h"

#include "common/Assertions.h"
#include "common/StringUtil.h"

#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/HostSettings.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QPixmap>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMenu>

#include "GameListModel.h"
#include "GameListRefreshThread.h"
#include "GameListWidget.h"
#include "QtHost.h"
#include "QtUtils.h"

static const char* SUPPORTED_FORMATS_STRING = QT_TRANSLATE_NOOP(GameListWidget,
	".bin/.iso (ISO Disc Images)\n"
	".chd (Compressed Hunks of Data)\n"
	".cso (Compressed ISO)\n"
	".gz (Gzip Compressed ISO)");

class GameListSortModel final : public QSortFilterProxyModel
{
public:
	explicit GameListSortModel(GameListModel* parent)
		: QSortFilterProxyModel(parent)
		, m_model(parent)
	{
	}

	bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
	{
		// TODO: Search
		return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
	}

	bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override
	{
		return m_model->lessThan(source_left, source_right, source_left.column());
	}

private:
	GameListModel* m_model;
};

GameListWidget::GameListWidget(QWidget* parent /* = nullptr */)
	: QStackedWidget(parent)
{
}

GameListWidget::~GameListWidget() = default;

void GameListWidget::initialize()
{
	m_model = new GameListModel(this);
	m_model->setCoverScale(Host::GetBaseFloatSettingValue("UI", "GameListCoverArtScale", 0.45f));
	m_model->setShowCoverTitles(Host::GetBaseBoolSettingValue("UI", "GameListShowCoverTitles", true));

	m_sort_model = new GameListSortModel(m_model);
	m_sort_model->setSourceModel(m_model);
	m_table_view = new QTableView(this);
	m_table_view->setModel(m_sort_model);
	m_table_view->setSortingEnabled(true);
	m_table_view->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table_view->setContextMenuPolicy(Qt::CustomContextMenu);
	m_table_view->setAlternatingRowColors(true);
	m_table_view->setShowGrid(false);
	m_table_view->setCurrentIndex({});
	m_table_view->horizontalHeader()->setHighlightSections(false);
	m_table_view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	m_table_view->verticalHeader()->hide();
	m_table_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	loadTableViewColumnVisibilitySettings();
	loadTableViewColumnSortSettings();

	connect(m_table_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
		&GameListWidget::onSelectionModelCurrentChanged);
	connect(m_table_view, &QTableView::activated, this, &GameListWidget::onTableViewItemActivated);
	connect(m_table_view, &QTableView::customContextMenuRequested, this,
		&GameListWidget::onTableViewContextMenuRequested);
	connect(m_table_view->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
		&GameListWidget::onTableViewHeaderContextMenuRequested);
	connect(m_table_view->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
		&GameListWidget::onTableViewHeaderSortIndicatorChanged);

	insertWidget(0, m_table_view);

	m_list_view = new GameListGridListView(this);
	m_list_view->setModel(m_sort_model);
	m_list_view->setModelColumn(GameListModel::Column_Cover);
	m_list_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_list_view->setViewMode(QListView::IconMode);
	m_list_view->setResizeMode(QListView::Adjust);
	m_list_view->setUniformItemSizes(true);
	m_list_view->setItemAlignment(Qt::AlignHCenter);
	m_list_view->setContextMenuPolicy(Qt::CustomContextMenu);
	m_list_view->setFrameStyle(QFrame::NoFrame);
	m_list_view->setSpacing(m_model->getCoverArtSpacing());
	updateListFont();

	connect(m_list_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
		&GameListWidget::onSelectionModelCurrentChanged);
	connect(m_list_view, &GameListGridListView::zoomIn, this, &GameListWidget::gridZoomIn);
	connect(m_list_view, &GameListGridListView::zoomOut, this, &GameListWidget::gridZoomOut);
	connect(m_list_view, &QListView::activated, this, &GameListWidget::onListViewItemActivated);
	connect(m_list_view, &QListView::customContextMenuRequested, this, &GameListWidget::onListViewContextMenuRequested);

	insertWidget(1, m_list_view);

	m_empty_widget = new QWidget(this);
	m_empty_ui.setupUi(m_empty_widget);
	m_empty_ui.supportedFormats->setText(qApp->translate("GameListWidget", SUPPORTED_FORMATS_STRING));
	connect(m_empty_ui.addGameDirectory, &QPushButton::clicked, this, [this]() { emit addGameDirectoryRequested(); });
	connect(m_empty_ui.scanForNewGames, &QPushButton::clicked, this, [this]() { refresh(false); });
	insertWidget(2, m_empty_widget);

	if (Host::GetBaseBoolSettingValue("UI", "GameListGridView", false))
		setCurrentIndex(1);
	else
		setCurrentIndex(0);

	resizeTableViewColumnsToFit();
}

bool GameListWidget::isShowingGameList() const
{
	return currentIndex() == 0;
}

bool GameListWidget::isShowingGameGrid() const
{
	return currentIndex() == 1;
}

bool GameListWidget::getShowGridCoverTitles() const
{
	return m_model->getShowCoverTitles();
}

void GameListWidget::refresh(bool invalidate_cache)
{
	cancelRefresh();

	m_refresh_thread = new GameListRefreshThread(invalidate_cache);
	connect(m_refresh_thread, &GameListRefreshThread::refreshProgress, this, &GameListWidget::onRefreshProgress,
		Qt::QueuedConnection);
	connect(m_refresh_thread, &GameListRefreshThread::refreshComplete, this, &GameListWidget::onRefreshComplete,
		Qt::QueuedConnection);
	m_refresh_thread->start();
}

void GameListWidget::cancelRefresh()
{
	if (!m_refresh_thread)
		return;

	m_refresh_thread->cancel();
	m_refresh_thread->wait();
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	pxAssertRel(!m_refresh_thread, "Game list thread should be unreferenced by now");
}

void GameListWidget::onRefreshProgress(const QString& status, int current, int total)
{
	// switch away from the placeholder while we scan, in case we find anything
	if (currentIndex() == 2)
		setCurrentIndex(Host::GetBaseBoolSettingValue("UI", "GameListGridView", false) ? 1 : 0);

	m_model->refresh();
	emit refreshProgress(status, current, total);
}

void GameListWidget::onRefreshComplete()
{
	m_model->refresh();
	emit refreshComplete();

	pxAssertRel(m_refresh_thread, "Has a refresh thread");
	m_refresh_thread->wait();
	delete m_refresh_thread;
	m_refresh_thread = nullptr;

	// if we still had no games, switch to the helper widget
	if (m_model->rowCount() == 0)
		setCurrentIndex(2);
}

void GameListWidget::onSelectionModelCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	const QModelIndex source_index = m_sort_model->mapToSource(current);
	if (!source_index.isValid() || source_index.row() >= static_cast<int>(GameList::GetEntryCount()))
		return;

	emit selectionChanged();
}

void GameListWidget::onTableViewItemActivated(const QModelIndex& index)
{
	const QModelIndex source_index = m_sort_model->mapToSource(index);
	if (!source_index.isValid() || source_index.row() >= static_cast<int>(GameList::GetEntryCount()))
		return;

	emit entryActivated();
}

void GameListWidget::onTableViewContextMenuRequested(const QPoint& point)
{
	emit entryContextMenuRequested(m_table_view->mapToGlobal(point));
}

void GameListWidget::onListViewItemActivated(const QModelIndex& index)
{
	const QModelIndex source_index = m_sort_model->mapToSource(index);
	if (!source_index.isValid() || source_index.row() >= static_cast<int>(GameList::GetEntryCount()))
		return;

	emit entryActivated();
}

void GameListWidget::onListViewContextMenuRequested(const QPoint& point)
{
	emit entryContextMenuRequested(m_list_view->mapToGlobal(point));
}

void GameListWidget::onTableViewHeaderContextMenuRequested(const QPoint& point)
{
	QMenu menu;

	for (int column = 0; column < GameListModel::Column_Count; column++)
	{
		if (column == GameListModel::Column_Cover)
			continue;

		QAction* action = menu.addAction(m_model->getColumnDisplayName(column));
		action->setCheckable(true);
		action->setChecked(!m_table_view->isColumnHidden(column));
		connect(action, &QAction::toggled, [this, column](bool enabled) {
			m_table_view->setColumnHidden(column, !enabled);
			saveTableViewColumnVisibilitySettings(column);
			resizeTableViewColumnsToFit();
		});
	}

	menu.exec(m_table_view->mapToGlobal(point));
}

void GameListWidget::onTableViewHeaderSortIndicatorChanged(int, Qt::SortOrder)
{
	saveTableViewColumnSortSettings();
}

void GameListWidget::listZoom(float delta)
{
	static constexpr float MIN_SCALE = 0.1f;
	static constexpr float MAX_SCALE = 2.0f;

	const float new_scale = std::clamp(m_model->getCoverScale() + delta, MIN_SCALE, MAX_SCALE);
	QtHost::SetBaseFloatSettingValue("UI", "GameListCoverArtScale", new_scale);
	m_model->setCoverScale(new_scale);
	updateListFont();

	m_model->refresh();
}

void GameListWidget::gridZoomIn()
{
	listZoom(0.05f);
}

void GameListWidget::gridZoomOut()
{
	listZoom(-0.05f);
}

void GameListWidget::refreshGridCovers()
{
	m_model->refreshCovers();
}

void GameListWidget::showGameList()
{
	if (currentIndex() == 0 || m_model->rowCount() == 0)
		return;

	QtHost::SetBaseBoolSettingValue("UI", "GameListGridView", false);
	setCurrentIndex(0);
	resizeTableViewColumnsToFit();
}

void GameListWidget::showGameGrid()
{
	if (currentIndex() == 1 || m_model->rowCount() == 0)
		return;

	QtHost::SetBaseBoolSettingValue("UI", "GameListGridView", true);
	setCurrentIndex(1);
}

void GameListWidget::setShowCoverTitles(bool enabled)
{
	if (m_model->getShowCoverTitles() == enabled)
		return;

	QtHost::SetBaseBoolSettingValue("UI", "GameListShowCoverTitles", enabled);
	m_model->setShowCoverTitles(enabled);
	if (isShowingGameGrid())
		m_model->refresh();
}

void GameListWidget::updateListFont()
{
	QFont font;
	font.setPointSizeF(16.0f * m_model->getCoverScale());
	m_list_view->setFont(font);
}

void GameListWidget::resizeEvent(QResizeEvent* event)
{
	QStackedWidget::resizeEvent(event);
	resizeTableViewColumnsToFit();
}

void GameListWidget::resizeTableViewColumnsToFit()
{
	QtUtils::ResizeColumnsForTableView(m_table_view, {
														 45, // type
														 80, // code
														 -1, // title
														 -1, // file title
														 60, // crc
														 80, // size
														 60, // region
														 100 // compatibility
													 });
}

static std::string getColumnVisibilitySettingsKeyName(int column)
{
	return StringUtil::StdStringFromFormat("Show%s",
		GameListModel::getColumnName(static_cast<GameListModel::Column>(column)));
}

void GameListWidget::loadTableViewColumnVisibilitySettings()
{
	static constexpr std::array<bool, GameListModel::Column_Count> DEFAULT_VISIBILITY = {{
		true, // type
		true, // code
		true, // title
		false, // file title
		false, // crc
		true, // size
		true, // region
		true // compatibility
	}};

	for (int column = 0; column < GameListModel::Column_Count; column++)
	{
		const bool visible = Host::GetBaseBoolSettingValue(
			"GameListTableView", getColumnVisibilitySettingsKeyName(column).c_str(), DEFAULT_VISIBILITY[column]);
		m_table_view->setColumnHidden(column, !visible);
	}
}

void GameListWidget::saveTableViewColumnVisibilitySettings()
{
	for (int column = 0; column < GameListModel::Column_Count; column++)
	{
		const bool visible = !m_table_view->isColumnHidden(column);
		QtHost::SetBaseBoolSettingValue("GameListTableView", getColumnVisibilitySettingsKeyName(column).c_str(), visible);
	}
}

void GameListWidget::saveTableViewColumnVisibilitySettings(int column)
{
	const bool visible = !m_table_view->isColumnHidden(column);
	QtHost::SetBaseBoolSettingValue("GameListTableView", getColumnVisibilitySettingsKeyName(column).c_str(), visible);
}

void GameListWidget::loadTableViewColumnSortSettings()
{
	const GameListModel::Column DEFAULT_SORT_COLUMN = GameListModel::Column_Type;
	const bool DEFAULT_SORT_DESCENDING = false;

	const GameListModel::Column sort_column =
		GameListModel::getColumnIdForName(Host::GetBaseStringSettingValue("GameListTableView", "SortColumn"))
			.value_or(DEFAULT_SORT_COLUMN);
	const bool sort_descending =
		Host::GetBaseBoolSettingValue("GameListTableView", "SortDescending", DEFAULT_SORT_DESCENDING);
	m_table_view->sortByColumn(sort_column, sort_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
}

void GameListWidget::saveTableViewColumnSortSettings()
{
	const int sort_column = m_table_view->horizontalHeader()->sortIndicatorSection();
	const bool sort_descending = (m_table_view->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder);

	if (sort_column >= 0 && sort_column < GameListModel::Column_Count)
	{
		QtHost::SetBaseStringSettingValue(
			"GameListTableView", "SortColumn", GameListModel::getColumnName(static_cast<GameListModel::Column>(sort_column)));
	}

	QtHost::SetBaseBoolSettingValue("GameListTableView", "SortDescending", sort_descending);
}

const GameList::Entry* GameListWidget::getSelectedEntry() const
{
	if (currentIndex() == 0)
	{
		const QItemSelectionModel* selection_model = m_table_view->selectionModel();
		if (!selection_model->hasSelection())
			return nullptr;

		const QModelIndexList selected_rows = selection_model->selectedRows();
		if (selected_rows.empty())
			return nullptr;

		const QModelIndex source_index = m_sort_model->mapToSource(selected_rows[0]);
		if (!source_index.isValid())
			return nullptr;

		return GameList::GetEntryByIndex(source_index.row());
	}
	else
	{
		const QItemSelectionModel* selection_model = m_list_view->selectionModel();
		if (!selection_model->hasSelection())
			return nullptr;

		const QModelIndex source_index = m_sort_model->mapToSource(selection_model->currentIndex());
		if (!source_index.isValid())
			return nullptr;

		return GameList::GetEntryByIndex(source_index.row());
	}
}

GameListGridListView::GameListGridListView(QWidget* parent /*= nullptr*/)
	: QListView(parent)
{
}

void GameListGridListView::wheelEvent(QWheelEvent* e)
{
	if (e->modifiers() & Qt::ControlModifier)
	{
		int dy = e->angleDelta().y();
		if (dy != 0)
		{
			if (dy < 0)
				zoomOut();
			else
				zoomIn();

			return;
		}
	}

	QListView::wheelEvent(e);
}
