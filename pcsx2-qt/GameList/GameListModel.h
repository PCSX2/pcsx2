// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "pcsx2/GameList.h"

#include "common/LRUCache.h"

#include <QtCore/QAbstractTableModel>
#include <QtGui/QPixmap>
#include <algorithm>
#include <atomic>
#include <array>
#include <optional>
#include <unordered_map>

class GameListModel final : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum Column : int
	{
		Column_Type,
		Column_Serial,
		Column_Title,
		Column_FileTitle,
		Column_CRC,
		Column_TimePlayed,
		Column_LastPlayed,
		Column_Size,
		Column_Region,
		Column_Compatibility,
		Column_Cover,

		Column_Count
	};

	static std::optional<Column> getColumnIdForName(const std::string_view name);
	static const char* getColumnName(const Column col);

	static QIcon getIconForType(const GameList::EntryType type);
	static QIcon getIconForRegion(const GameList::Region region);

	GameListModel(const float cover_scale, const bool show_cover_titles, const qreal dpr, QObject* parent = nullptr);
	~GameListModel();

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, const int role = Qt::DisplayRole) const override;
	QVariant headerData(const int section, const Qt::Orientation orientation, const int role = Qt::DisplayRole) const override;

	__fi const QString& getColumnDisplayName(const int column) { return m_column_display_names[column]; }

	void refresh();
	void reloadThemeSpecificImages();

	bool titlesLessThan(const int left_row, const int right_row) const;

	bool lessThan(const QModelIndex& left_index, const QModelIndex& right_index, const int column) const;

	bool getShowCoverTitles() const { return m_show_titles_for_covers; }
	void setShowCoverTitles(const bool enabled) { m_show_titles_for_covers = enabled; }

	float getCoverScale() const { return m_cover_scale; }
	void setCoverScale(const float scale);
	int getCoverArtWidth() const;
	int getCoverArtHeight() const;
	int getCoverArtSpacing() const;
	void refreshCovers();
	void updateCacheSize(const int width, const int height);

	void setDevicePixelRatio(const qreal dpr);

Q_SIGNALS:
	void coverScaleChanged();

private:
	void loadSettings();
	void loadCommonImages();
	void loadThemeSpecificImages();
	void setColumnDisplayNames();
	void loadOrGenerateCover(const GameList::Entry* ge);
	void invalidateCoverForPath(const std::string& path);

	static QString formatTimespan(const time_t timespan);

	float m_cover_scale = 0.0f;
	std::atomic<u32> m_cover_scale_counter{0};
	bool m_show_titles_for_covers = false;
	bool m_prefer_english_titles = false;

	std::array<QString, Column_Count> m_column_display_names;
	std::array<QPixmap, static_cast<u32>(GameList::EntryType::Count)> m_type_pixmaps;
	std::array<QPixmap, static_cast<u32>(GameList::Region::Count)> m_region_pixmaps;
	QPixmap m_placeholder_pixmap;
	QPixmap m_loading_pixmap;
	qreal m_dpr;

	std::array<QPixmap, static_cast<int>(GameList::CompatibilityRatingCount)> m_compatibility_pixmaps;
	mutable LRUCache<std::string, QPixmap> m_cover_pixmap_cache;
};
