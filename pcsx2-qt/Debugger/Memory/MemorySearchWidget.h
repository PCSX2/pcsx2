// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_MemorySearchWidget.h"

#include "Debugger/DebuggerWidget.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <QtCore/QMap>

class MemorySearchWidget final : public DebuggerWidget
{
	Q_OBJECT

public:
	MemorySearchWidget(const DebuggerWidgetParameters& parameters);
	~MemorySearchWidget() = default;

	enum class SearchType
	{
		ByteType,
		Int16Type,
		Int32Type,
		Int64Type,
		FloatType,
		DoubleType,
		StringType,
		ArrayType
	};

	// Note: The order of these enum values must reflect the order in thee Search Comparison combobox.
	enum class SearchComparison
	{
		Equals,
		NotEquals,
		GreaterThan,
		GreaterThanOrEqual,
		LessThan,
		LessThanOrEqual,
		Increased,
		IncreasedBy,
		Decreased,
		DecreasedBy,
		Changed,
		ChangedBy,
		NotChanged,
		UnknownValue,
		Invalid
	};

	class SearchComparisonLabelMap
	{
	public:
		SearchComparisonLabelMap()
		{
			insert(SearchComparison::Equals, tr("Equals"));
			insert(SearchComparison::NotEquals, tr("Not Equals"));
			insert(SearchComparison::GreaterThan, tr("Greater Than"));
			insert(SearchComparison::GreaterThanOrEqual, tr("Greater Than Or Equal"));
			insert(SearchComparison::LessThan, tr("Less Than"));
			insert(SearchComparison::LessThanOrEqual, tr("Less Than Or Equal"));
			insert(SearchComparison::Increased, tr("Increased"));
			insert(SearchComparison::IncreasedBy, tr("Increased By"));
			insert(SearchComparison::Decreased, tr("Decreased"));
			insert(SearchComparison::DecreasedBy, tr("Decreased By"));
			insert(SearchComparison::Changed, tr("Changed"));
			insert(SearchComparison::ChangedBy, tr("Changed By"));
			insert(SearchComparison::NotChanged, tr("Not Changed"));
			insert(SearchComparison::UnknownValue, tr("Unknown Initial Value"));
			insert(SearchComparison::Invalid, "");
		}
		SearchComparison labelToEnum(QString comparisonLabel)
		{
			return labelToEnumMap.value(comparisonLabel, SearchComparison::Invalid);
		}
		QString enumToLabel(SearchComparison comparison)
		{
			return enumToLabelMap.value(comparison, "");
		}

	private:
		QMap<SearchComparison, QString> enumToLabelMap;
		QMap<QString, SearchComparison> labelToEnumMap;
		void insert(SearchComparison comparison, QString comparisonLabel)
		{
			enumToLabelMap.insert(comparison, comparisonLabel);
			labelToEnumMap.insert(comparisonLabel, comparison);
		};
	};

	class SearchResult
	{
	private:
		u32 address;
		QVariant value;
		SearchType type;

	public:
		SearchResult() {}
		SearchResult(u32 address, const QVariant& value, SearchType type)
			: address(address)
			, value(value)
			, type(type)
		{
		}
		bool isIntegerValue() const { return type == SearchType::ByteType || type == SearchType::Int16Type || type == SearchType::Int32Type || type == SearchType::Int64Type; }
		bool isFloatValue() const { return type == SearchType::FloatType; }
		bool isDoubleValue() const { return type == SearchType::DoubleType; }
		bool isArrayValue() const { return type == SearchType::ArrayType || type == SearchType::StringType; }
		u32 getAddress() const { return address; }
		SearchType getType() const { return type; }
		QByteArray getArrayValue() const { return isArrayValue() ? value.toByteArray() : QByteArray(); }

		template <typename T>
		T getValue() const
		{
			return value.value<T>();
		}
	};

public slots:
	void onSearchButtonClicked();
	void onSearchResultsListScroll(u32 value);
	void onSearchTypeChanged(int newIndex);
	void onSearchComparisonChanged(int newIndex);
	void loadSearchResults();
	void contextRemoveSearchResult();
	void contextCopySearchResultAddress();
	void onListSearchResultsContextMenu(QPoint pos);

private:
	std::vector<SearchResult> m_searchResults;
	SearchComparisonLabelMap m_searchComparisonLabelMap;
	Ui::MemorySearchWidget m_ui;
	QTimer m_resultsLoadTimer;

	u32 m_initialResultsLoadLimit = 20000;
	u32 m_numResultsAddedPerLoad = 10000;

	void updateSearchComparisonSelections();
	std::vector<SearchComparison> getValidSearchComparisonsForState(SearchType type, std::vector<SearchResult>& existingResults);
	SearchType getCurrentSearchType();
	SearchComparison getCurrentSearchComparison();
	bool doesSearchComparisonTakeInput(SearchComparison comparison);
};
