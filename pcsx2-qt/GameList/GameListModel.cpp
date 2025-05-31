// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GameListModel.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "fmt/format.h"
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QFuture>
#include <QtCore/QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QtGui/QGuiApplication>
#include <QtGui/QIcon>
#include <QtGui/QPainter>

static constexpr std::array<const char*, GameListModel::Column_Count> s_column_names = {
	{"Type", "Code", "Title", "File Title", "CRC", "Time Played", "Last Played", "Size", "Region", "Compatibility", "Cover"}};

static constexpr int COVER_ART_WIDTH = 350;
static constexpr int COVER_ART_HEIGHT = 512;
static constexpr int COVER_ART_SPACING = 32;
static constexpr int MIN_COVER_CACHE_SIZE = 256;

static int DPRScale(int size, qreal dpr)
{
	return static_cast<int>(static_cast<qreal>(size) * dpr);
}

static int DPRUnscale(int size, qreal dpr)
{
	return static_cast<int>(static_cast<qreal>(size) / dpr);
}

static void resizeAndPadPixmap(QPixmap* pm, int expected_width, int expected_height, qreal dpr)
{
	const int dpr_expected_width = DPRScale(expected_width, dpr);
	const int dpr_expected_height = DPRScale(expected_height, dpr);
	if (pm->width() == dpr_expected_width && pm->height() == dpr_expected_height)
		return;

	*pm = pm->scaled(dpr_expected_width, dpr_expected_height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	if (pm->width() == dpr_expected_width && pm->height() == dpr_expected_height)
		return;

	// QPainter works in unscaled coordinates.
	int xoffs = 0;
	int yoffs = 0;
	if (pm->width() < dpr_expected_width)
		xoffs = DPRUnscale((dpr_expected_width - pm->width()) / 2, dpr);
	if (pm->height() < dpr_expected_height)
		yoffs = DPRUnscale((dpr_expected_height - pm->height()) / 2, dpr);

	QPixmap padded_image(dpr_expected_width, dpr_expected_height);
	padded_image.setDevicePixelRatio(dpr);
	padded_image.fill(Qt::transparent);
	QPainter painter;
	if (painter.begin(&padded_image))
	{
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		painter.drawPixmap(xoffs, yoffs, *pm);
		painter.setCompositionMode(QPainter::CompositionMode_Destination);
		painter.fillRect(padded_image.rect(), QColor(0, 0, 0, 0));
		painter.end();
	}

	*pm = padded_image;
}

static QPixmap createPlaceholderImage(const QPixmap& placeholder_pixmap, int width, int height, float scale,
	qreal dpr, const std::string& title)
{
	QPixmap pm(placeholder_pixmap.copy());
	pm.setDevicePixelRatio(dpr);
	if (pm.isNull())
		return QPixmap();

	resizeAndPadPixmap(&pm, width, height, dpr);
	QPainter painter;
	if (painter.begin(&pm))
	{
		QFont font;
		font.setPointSize(std::max(static_cast<int>(32.0f * scale), 1));
		painter.setFont(font);
		painter.setPen(Qt::white);

		const QRect text_rc(0, 0, static_cast<int>(static_cast<float>(width)),
			static_cast<int>(static_cast<float>(height)));
		painter.drawText(text_rc, Qt::AlignCenter | Qt::TextWordWrap, QString::fromStdString(title));
		painter.end();
	}

	return pm;
}

std::optional<GameListModel::Column> GameListModel::getColumnIdForName(std::string_view name)
{
	for (int column = 0; column < Column_Count; column++)
	{
		if (name == s_column_names[column])
			return static_cast<Column>(column);
	}

	return std::nullopt;
}

const char* GameListModel::getColumnName(Column col)
{
	return s_column_names[static_cast<int>(col)];
}

GameListModel::GameListModel(float cover_scale, bool show_cover_titles, qreal dpr, QObject* parent /* = nullptr */)
	: QAbstractTableModel(parent)
	, m_show_titles_for_covers(show_cover_titles)
	, m_dpr{dpr}
{
	loadSettings();
	loadCommonImages();
	setCoverScale(cover_scale);
	setColumnDisplayNames();
}
GameListModel::~GameListModel() = default;

void GameListModel::reloadThemeSpecificImages()
{
	loadThemeSpecificImages();
	refresh();
}

void GameListModel::setCoverScale(float scale)
{
	if (m_cover_scale == scale)
		return;

	m_cover_pixmap_cache.Clear();
	m_cover_scale = scale;
	m_cover_scale_counter.fetch_add(1, std::memory_order_release);
	m_loading_pixmap = QPixmap(getCoverArtWidth(), getCoverArtHeight());
	m_loading_pixmap.fill(QColor(0, 0, 0, 0));

	emit coverScaleChanged();
}

void GameListModel::refreshCovers()
{
	m_cover_pixmap_cache.Clear();
	refresh();
}

void GameListModel::updateCacheSize(int width, int height)
{
	// This is a bit conversative, since it doesn't consider padding, but better to be over than under.
	const int cover_width = getCoverArtWidth();
	const int cover_height = getCoverArtHeight();
	const int num_columns = ((width + (cover_width - 1)) / cover_width);
	const int num_rows = ((height + (cover_height - 1)) / cover_height);
	m_cover_pixmap_cache.SetMaxCapacity(static_cast<int>(std::max(num_columns * num_rows, MIN_COVER_CACHE_SIZE)));
}

void GameListModel::setDevicePixelRatio(qreal dpr)
{
	m_dpr = dpr;
	loadCommonImages();
	refreshCovers();
}

void GameListModel::loadOrGenerateCover(const GameList::Entry* ge)
{
	// Why this counter: Every time we change the cover scale, we increment the counter variable. This way if the scale is changed
	// while there's outstanding jobs, the old jobs won't proceed (at the wrong size), or get added into the grid.
	const u32 counter = m_cover_scale_counter.load(std::memory_order_acquire);

	QFuture<QPixmap> future = QtConcurrent::run([this, entry = *ge, counter]() -> QPixmap {
		QPixmap image;
		if (m_cover_scale_counter.load(std::memory_order_acquire) == counter)
		{
			const std::string cover_path(GameList::GetCoverImagePathForEntry(&entry));
			if (!cover_path.empty())
			{
				image = QPixmap(QString::fromStdString(cover_path));
				if (!image.isNull())
				{
					image.setDevicePixelRatio(m_dpr);
					resizeAndPadPixmap(&image, getCoverArtWidth(), getCoverArtHeight(), m_dpr);
				}
			}
		}

		const std::string& title = entry.GetTitle(m_prefer_english_titles);

		if (image.isNull())
			image = createPlaceholderImage(m_placeholder_pixmap, getCoverArtWidth(), getCoverArtHeight(), m_cover_scale, m_dpr, title);

		if (m_cover_scale_counter.load(std::memory_order_acquire) != counter)
			image = {};

		return image;
	});

	// Context must be 'this' so we run on the UI thread.
	future.then(this, [this, path = ge->path, counter](QPixmap pm) {
		if (m_cover_scale_counter.load(std::memory_order_acquire) != counter)
			return;

		m_cover_pixmap_cache.Insert(std::move(path), std::move(pm));
		invalidateCoverForPath(path);
	});
}

void GameListModel::invalidateCoverForPath(const std::string& path)
{
	// This isn't ideal, but not sure how else we can get the row, when it might change while scanning...
	auto lock = GameList::GetLock();
	const u32 count = GameList::GetEntryCount();
	std::optional<u32> row;
	for (u32 i = 0; i < count; i++)
	{
		if (GameList::GetEntryByIndex(i)->path == path)
		{
			row = i;
			break;
		}
	}
	if (!row.has_value())
	{
		// Game removed?
		return;
	}

	const QModelIndex mi(index(static_cast<int>(row.value()), Column_Cover));
	emit dataChanged(mi, mi, {Qt::DecorationRole});
}

int GameListModel::getCoverArtWidth() const
{
	return std::max(static_cast<int>(static_cast<float>(COVER_ART_WIDTH) * m_cover_scale), 1);
}

int GameListModel::getCoverArtHeight() const
{
	return std::max(static_cast<int>(static_cast<float>(COVER_ART_HEIGHT) * m_cover_scale), 1);
}

int GameListModel::getCoverArtSpacing() const
{
	return std::max(static_cast<int>(static_cast<float>(COVER_ART_SPACING) * m_cover_scale), 1);
}

int GameListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;

	return static_cast<int>(GameList::GetEntryCount());
}

int GameListModel::columnCount(const QModelIndex& parent) const
{
	if (parent.isValid())
		return 0;

	return Column_Count;
}

QString GameListModel::formatTimespan(time_t timespan)
{
	// avoid an extra string conversion
	const u32 hours = static_cast<u32>(timespan / 3600);
	const u32 minutes = static_cast<u32>((timespan % 3600) / 60);
	if (hours > 0)
		return qApp->translate("GameList", "%n hours", "", hours);
	else
		return qApp->translate("GameList", "%n minutes", "", minutes);
}

QVariant GameListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return {};

	const int row = index.row();
	if (row < 0 || row >= static_cast<int>(GameList::GetEntryCount()))
		return {};

	const auto lock = GameList::GetLock();
	const GameList::Entry* ge = GameList::GetEntryByIndex(row);
	if (!ge)
		return {};

	switch (role)
	{
		case Qt::DisplayRole:
		{
			switch (index.column())
			{
				case Column_Serial:
					return QString::fromStdString(ge->serial);

				case Column_Title:
					return QString::fromStdString(ge->GetTitle(m_prefer_english_titles));

				case Column_FileTitle:
					return QtUtils::StringViewToQString(Path::GetFileTitle(ge->path));

				case Column_CRC:
					return QString::fromStdString(fmt::format("{:08X}", ge->crc));

				case Column_TimePlayed:
				{
					if (ge->total_played_time == 0)
						return {};
					else
						return formatTimespan(ge->total_played_time);
				}

				case Column_LastPlayed:
					return QString::fromStdString(GameList::FormatTimestamp(ge->last_played_time));

				case Column_Size:
					return QString("%1 MB").arg(static_cast<double>(ge->total_size) / 1048576.0, 0, 'f', 2);

				case Column_Cover:
				{
					if (m_show_titles_for_covers)
						return QString::fromStdString(ge->GetTitle(m_prefer_english_titles));
					else
						return {};
				}

				default:
					return {};
			}
		}

		case Qt::InitialSortOrderRole:
		{
			switch (index.column())
			{
				case Column_Type:
					return static_cast<int>(ge->type);

				case Column_Serial:
					return QString::fromStdString(ge->serial);

				case Column_Title:
				case Column_Cover:
					return QString::fromStdString(ge->GetTitleSort(m_prefer_english_titles));

				case Column_FileTitle:
					return QtUtils::StringViewToQString(Path::GetFileTitle(ge->path));

				case Column_CRC:
					return static_cast<int>(ge->crc);

				case Column_TimePlayed:
					return static_cast<qlonglong>(ge->total_played_time);

				case Column_LastPlayed:
					return static_cast<qlonglong>(ge->last_played_time);

				case Column_Region:
					return static_cast<int>(ge->region);

				case Column_Compatibility:
					return static_cast<int>(ge->compatibility_rating);

				case Column_Size:
					return static_cast<qulonglong>(ge->total_size);

				default:
					return {};
			}
		}

		case Qt::DecorationRole:
		{
			switch (index.column())
			{
				case Column_Type:
				{
					return m_type_pixmaps[static_cast<u32>(ge->type)];
				}

				case Column_Region:
				{
					return m_region_pixmaps[static_cast<u32>(ge->region)];
				}

				case Column_Compatibility:
				{
					return m_compatibility_pixmaps[static_cast<u32>(
						(static_cast<u32>(ge->compatibility_rating) >= GameList::CompatibilityRatingCount) ?
							GameList::CompatibilityRating::Unknown :
							ge->compatibility_rating)];
				}

				case Column_Cover:
				{
					QPixmap* pm = m_cover_pixmap_cache.Lookup(ge->path);
					if (pm)
						return *pm;

					// We insert the placeholder into the cache, so that we don't repeatedly
					// queue loading jobs for this game.
					const_cast<GameListModel*>(this)->loadOrGenerateCover(ge);
					return *m_cover_pixmap_cache.Insert(ge->path, m_loading_pixmap);
				}
				break;

				default:
					return {};
			}

			default:
				return {};
		}
	}
}

QVariant GameListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= Column_Count)
		return {};

	return m_column_display_names[section];
}

void GameListModel::refresh()
{
	beginResetModel();
	loadSettings();
	endResetModel();
}

bool GameListModel::titlesLessThan(int left_row, int right_row) const
{
	if (left_row < 0 || left_row >= static_cast<int>(GameList::GetEntryCount()) || right_row < 0 ||
		right_row >= static_cast<int>(GameList::GetEntryCount()))
	{
		return false;
	}

	const GameList::Entry* left = GameList::GetEntryByIndex(left_row);
	const GameList::Entry* right = GameList::GetEntryByIndex(right_row);
	return QtHost::LocaleSensitiveCompare(QString::fromStdString(left->GetTitleSort(m_prefer_english_titles)),
			   QString::fromStdString(right->GetTitleSort(m_prefer_english_titles))) < 0;
}

bool GameListModel::lessThan(const QModelIndex& left_index, const QModelIndex& right_index, int column) const
{
	if (!left_index.isValid() || !right_index.isValid())
		return false;

	const int left_row = left_index.row();
	const int right_row = right_index.row();
	if (left_row < 0 || left_row >= static_cast<int>(GameList::GetEntryCount()) || right_row < 0 ||
		right_row >= static_cast<int>(GameList::GetEntryCount()))
	{
		return false;
	}

	const auto lock = GameList::GetLock();
	const GameList::Entry* left = GameList::GetEntryByIndex(left_row);
	const GameList::Entry* right = GameList::GetEntryByIndex(right_row);
	if (!left || !right)
		return false;

	switch (column)
	{
		case Column_Type:
		{
			if (left->type == right->type)
				return titlesLessThan(left_row, right_row);

			return static_cast<int>(left->type) < static_cast<int>(right->type);
		}

		case Column_Serial:
		{
			if (left->serial == right->serial)
				return titlesLessThan(left_row, right_row);
			return (StringUtil::Strcasecmp(left->serial.c_str(), right->serial.c_str()) < 0);
		}

		case Column_Title:
		{
			return titlesLessThan(left_row, right_row);
		}

		case Column_FileTitle:
		{
			const std::string_view file_title_left(Path::GetFileTitle(left->path));
			const std::string_view file_title_right(Path::GetFileTitle(right->path));
			if (file_title_left == file_title_right)
				return titlesLessThan(left_row, right_row);

			const std::size_t smallest = std::min(file_title_left.size(), file_title_right.size());
			return (StringUtil::Strncasecmp(file_title_left.data(), file_title_right.data(), smallest) < 0);
		}

		case Column_Region:
		{
			if (left->region == right->region)
				return titlesLessThan(left_row, right_row);
			return (static_cast<int>(left->region) < static_cast<int>(right->region));
		}

		case Column_Compatibility:
		{
			if (left->compatibility_rating == right->compatibility_rating)
				return titlesLessThan(left_row, right_row);

			return (static_cast<int>(left->compatibility_rating) < static_cast<int>(right->compatibility_rating));
		}

		case Column_Size:
		{
			if (left->total_size == right->total_size)
				return titlesLessThan(left_row, right_row);

			return (left->total_size < right->total_size);
		}

		case Column_CRC:
		{
			if (left->crc == right->crc)
				return titlesLessThan(left_row, right_row);

			return (left->crc < right->crc);
		}

		case Column_TimePlayed:
		{
			if (left->total_played_time == right->total_played_time)
				return titlesLessThan(left_row, right_row);

			return (left->total_played_time < right->total_played_time);
		}

		case Column_LastPlayed:
		{
			if (left->last_played_time == right->last_played_time)
				return titlesLessThan(left_row, right_row);

			return (left->last_played_time < right->last_played_time);
		}

		default:
			return false;
	}
}

void GameListModel::loadSettings()
{
	m_prefer_english_titles = Host::GetBaseBoolSettingValue("UI", "PreferEnglishGameList", false);
}

QIcon GameListModel::getIconForType(GameList::EntryType type)
{
	switch (type)
	{
		case GameList::EntryType::PS2Disc:
		case GameList::EntryType::PS1Disc:
			return QIcon::fromTheme("disc-2-line");

		case GameList::EntryType::ELF:
		default:
			return QIcon::fromTheme("file-settings-line");
	}
}

QIcon GameListModel::getIconForRegion(GameList::Region region)
{
	return QIcon(
		QStringLiteral("%1/icons/flags/%2.svg").arg(QtHost::GetResourcesBasePath()).arg(GameList::RegionToString(region, false)));
}

void GameListModel::loadThemeSpecificImages()
{
	for (u32 type = 0; type < static_cast<u32>(GameList::EntryType::Count); type++)
		m_type_pixmaps[type] = getIconForType(static_cast<GameList::EntryType>(type)).pixmap(QSize(24, 24), m_dpr);

	for (u32 i = 0; i < static_cast<u32>(GameList::Region::Count); i++)
		m_region_pixmaps[i] = getIconForRegion(static_cast<GameList::Region>(i)).pixmap(QSize(36, 26), m_dpr);
}

void GameListModel::loadCommonImages()
{
	loadThemeSpecificImages();

	const QString base_path(QtHost::GetResourcesBasePath());
	for (u32 i = 1; i < GameList::CompatibilityRatingCount; i++)
		m_compatibility_pixmaps[i] = QIcon((QStringLiteral("%1/icons/star-%2.svg").arg(base_path).arg(i - 1))).pixmap(QSize(88, 16), m_dpr);

	m_placeholder_pixmap.load(QStringLiteral("%1/cover-placeholder.png").arg(base_path));
}

void GameListModel::setColumnDisplayNames()
{
	m_column_display_names[Column_Type] = tr("Type");
	m_column_display_names[Column_Serial] = tr("Code");
	m_column_display_names[Column_Title] = tr("Title");
	m_column_display_names[Column_FileTitle] = tr("File Title");
	m_column_display_names[Column_CRC] = tr("CRC");
	m_column_display_names[Column_TimePlayed] = tr("Time Played");
	m_column_display_names[Column_LastPlayed] = tr("Last Played");
	m_column_display_names[Column_Size] = tr("Size");
	m_column_display_names[Column_Region] = tr("Region");
	m_column_display_names[Column_Compatibility] = tr("Compatibility");
}
