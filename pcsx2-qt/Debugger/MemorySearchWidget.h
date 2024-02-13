// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_MemorySearchWidget.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>

class MemorySearchWidget final : public QWidget
{
    Q_OBJECT

public:
	MemorySearchWidget(QWidget* parent);
    ~MemorySearchWidget() = default;
	void setCpu(DebugInterface* cpu);

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
		LessThanOrEqual
	};

public slots:
	void onSearchButtonClicked();
	void onSearchResultsListScroll(u32 value);
	void loadSearchResults();
	void contextSearchResultGoToDisassembly();
	void contextRemoveSearchResult();
	void contextCopySearchResultAddress();
	void onListSearchResultsContextMenu(QPoint pos);

signals:
	void addAddressToSavedAddressesList(u32 address);
	void goToAddressInDisassemblyView(u32 address);
	void goToAddressInMemoryView(u32 address);
	void switchToMemoryViewTab();

private:
    std::vector<u32> m_searchResults;

    Ui::MemorySearchWidget m_ui;

    DebugInterface* m_cpu;
    QTimer m_resultsLoadTimer;

    u32 m_initialResultsLoadLimit = 20000;
	u32 m_numResultsAddedPerLoad = 10000;
};
