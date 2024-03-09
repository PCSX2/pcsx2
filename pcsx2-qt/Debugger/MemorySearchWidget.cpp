// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "MemorySearchWidget.h"

#include "DebugTools/DebugInterface.h"

#include "QtUtils.h"

#include "common/Console.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QMessageBox>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QFutureWatcher>
#include <QtGui/QPainter>

using SearchComparison = MemorySearchWidget::SearchComparison;
using SearchType = MemorySearchWidget::SearchType;
using SearchResult = MemorySearchWidget::SearchResult;
using SearchResults = QMap<u32, MemorySearchWidget::SearchResult>;

using namespace QtUtils;

MemorySearchWidget::MemorySearchWidget(QWidget* parent)
    : QWidget(parent)
{
	m_ui.setupUi(this);
	this->repaint();

	m_ui.listSearchResults->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.btnSearch, &QPushButton::clicked, this, &MemorySearchWidget::onSearchButtonClicked);
	connect(m_ui.btnFilterSearch, &QPushButton::clicked, this, &MemorySearchWidget::onSearchButtonClicked);
	connect(m_ui.listSearchResults, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item)
	{
		emit switchToMemoryViewTab();
		emit goToAddressInMemoryView(item->text().toUInt(nullptr, 16));
	});
	connect(m_ui.listSearchResults->verticalScrollBar(), &QScrollBar::valueChanged, this, &MemorySearchWidget::onSearchResultsListScroll);
	connect(m_ui.listSearchResults, &QListView::customContextMenuRequested, this, &MemorySearchWidget::onListSearchResultsContextMenu);
	connect(m_ui.cmbSearchType, &QComboBox::currentIndexChanged, this, &MemorySearchWidget::onSearchTypeChanged);

	// Ensures we don't retrigger the load results function unintentionally
	m_resultsLoadTimer.setInterval(100);
	m_resultsLoadTimer.setSingleShot(true);
	connect(&m_resultsLoadTimer, &QTimer::timeout, this, &MemorySearchWidget::loadSearchResults);
}

void MemorySearchWidget::setCpu(DebugInterface* cpu)
{
	m_cpu = cpu;
}

void MemorySearchWidget::contextSearchResultGoToDisassembly()
{
	const QItemSelectionModel* selModel = m_ui.listSearchResults->selectionModel();
	if (!selModel->hasSelection())
		return;

	u32 selectedAddress = m_ui.listSearchResults->selectedItems().first()->data(Qt::UserRole).toUInt();
	emit goToAddressInDisassemblyView(selectedAddress);
}

void MemorySearchWidget::contextRemoveSearchResult()
{
	const QItemSelectionModel* selModel = m_ui.listSearchResults->selectionModel();
	if (!selModel->hasSelection())
		return;

	const int selectedResultIndex = m_ui.listSearchResults->row(m_ui.listSearchResults->selectedItems().first());
	const auto* rowToRemove = m_ui.listSearchResults->takeItem(selectedResultIndex);
	u32 address = rowToRemove->data(Qt::UserRole).toUInt();
	m_searchResultsMap.remove(address);
	delete rowToRemove;
}

void MemorySearchWidget::contextCopySearchResultAddress()
{
	if (!m_ui.listSearchResults->selectionModel()->hasSelection())
		return;

	const u32 selectedResultIndex = m_ui.listSearchResults->row(m_ui.listSearchResults->selectedItems().first());
	const u32 rowAddress = m_ui.listSearchResults->item(selectedResultIndex)->data(Qt::UserRole).toUInt();
	const QString addressString = FilledQStringFromValue(rowAddress, 16);
	QApplication::clipboard()->setText(addressString);
}

void MemorySearchWidget::onListSearchResultsContextMenu(QPoint pos)
{
	QMenu* contextMenu = new QMenu(tr("Search Results List Context Menu"), m_ui.listSearchResults);
	const QItemSelectionModel* selModel = m_ui.listSearchResults->selectionModel();
	const auto listSearchResults = m_ui.listSearchResults;

	if (selModel->hasSelection())
	{
		QAction* copyAddressAction = new QAction(tr("Copy Address"), m_ui.listSearchResults);
		connect(copyAddressAction, &QAction::triggered, this, &MemorySearchWidget::contextCopySearchResultAddress);
		contextMenu->addAction(copyAddressAction);

		QAction* goToDisassemblyAction = new QAction(tr("Go to in Disassembly"), m_ui.listSearchResults);
		connect(goToDisassemblyAction, &QAction::triggered, this, &MemorySearchWidget::contextSearchResultGoToDisassembly);
		contextMenu->addAction(goToDisassemblyAction);

		QAction* addToSavedAddressesAction = new QAction(tr("Add to Saved Memory Addresses"), m_ui.listSearchResults);
		connect(addToSavedAddressesAction, &QAction::triggered, this, [this, listSearchResults]() {
			u32 selectedAddress = listSearchResults->selectedItems().first()->data(Qt::UserRole).toUInt();
			emit addAddressToSavedAddressesList(selectedAddress);
		});
		contextMenu->addAction(addToSavedAddressesAction);

		QAction* removeResultAction = new QAction(tr("Remove Result"), m_ui.listSearchResults);
		connect(removeResultAction, &QAction::triggered, this, &MemorySearchWidget::contextRemoveSearchResult);
		contextMenu->addAction(removeResultAction);
	}

	contextMenu->popup(m_ui.listSearchResults->viewport()->mapToGlobal(pos));
}

template<typename T>
T readValueAtAddress(DebugInterface* cpu, u32 addr);
template<>
float readValueAtAddress<float>(DebugInterface* cpu, u32 addr)
{
	return std::bit_cast<float>(cpu->read32(addr));
}

template<>
double readValueAtAddress<double>(DebugInterface* cpu, u32 addr)
{
	return std::bit_cast<double>(cpu->read64(addr));
}

template <typename T>
T readValueAtAddress(DebugInterface* cpu, u32 addr)
{
	T val = 0;
	switch (sizeof(T))
	{
		case sizeof(u8):
			val = cpu->read8(addr);
			break;
		case sizeof(u16):
			val = cpu->read16(addr);
			break;
		case sizeof(u32):
		{
			val = cpu->read32(addr);
			break;
		}
		case sizeof(u64):
		{
			val = cpu->read64(addr);
			break;
		}
	}
	return val;
}

template <typename T>
static bool memoryValueComparator(SearchComparison searchComparison, T searchValue, T readValue)
{
	const bool isNotOperator = searchComparison == SearchComparison::NotEquals;
	switch (searchComparison)
	{
		case SearchComparison::Equals:
		case SearchComparison::NotEquals:
		{
			bool areValuesEqual = false;
			if constexpr (std::is_same_v<T, float>)
			{
				const T fTop = searchValue + 0.00001f;
				const T fBottom = searchValue - 0.00001f;
				areValuesEqual = (fBottom < readValue && readValue < fTop);
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				const double dTop = searchValue + 0.00001f;
				const double dBottom = searchValue - 0.00001f;
				areValuesEqual = (dBottom < readValue && readValue < dTop);
			}
			else
			{
				areValuesEqual = searchValue == readValue;
			}
			return isNotOperator ? !areValuesEqual : areValuesEqual;
			break;
		}
		case SearchComparison::GreaterThan:
		case SearchComparison::GreaterThanOrEqual:
		case SearchComparison::LessThan:
		case SearchComparison::LessThanOrEqual:
		{
			const bool hasEqualsCheck = searchComparison == SearchComparison::GreaterThanOrEqual || searchComparison == SearchComparison::LessThanOrEqual;
			if (hasEqualsCheck && memoryValueComparator(SearchComparison::Equals, searchValue, readValue))
				return true;

			const bool isGreaterOperator = searchComparison == SearchComparison::GreaterThan || searchComparison == SearchComparison::GreaterThanOrEqual;
			if (std::is_same_v<T, float>)
			{
				const T fTop = searchValue + 0.00001f;
				const T fBottom = searchValue - 0.00001f;
				const bool isGreater = readValue > fTop;
				const bool isLesser = readValue < fBottom;
				return isGreaterOperator ? isGreater : isLesser;
			}
			else if (std::is_same_v<T, double>)
			{
				const double dTop = searchValue + 0.00001f;
				const double dBottom = searchValue - 0.00001f;
				const bool isGreater = readValue > dTop;
				const bool isLesser = readValue < dBottom;
				return isGreaterOperator ? isGreater : isLesser;
			}

			return isGreaterOperator ? (readValue > searchValue) : (readValue < searchValue);
		}
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			return false;
	}
}

// Handles the comparison of the read value against either the search value, or if existing searchResults are available, the value at the same address in the searchResultsMap
template <typename T>
bool handleSearchComparison(SearchComparison searchComparison, u32 searchAddress, SearchResults searchResults, T searchValue, T readValue)
{
	const bool isNotOperator = searchComparison == SearchComparison::NotEquals || searchComparison == SearchComparison::NotChanged;
	switch (searchComparison) 
	{
		case SearchComparison::Equals:
		case SearchComparison::NotEquals:
		case SearchComparison::GreaterThan:
		case SearchComparison::GreaterThanOrEqual:
		case SearchComparison::LessThan:
		case SearchComparison::LessThanOrEqual:
		{
			return memoryValueComparator(searchComparison, searchValue, readValue);
			break;
		}
		case SearchComparison::Increased:
		{
			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			return memoryValueComparator(SearchComparison::GreaterThan, priorValue, readValue);
			break;
		}
		case SearchComparison::IncreasedBy:
		{

			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			const T expectedIncrease = searchValue + priorValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedIncrease);
			break;
		}
		case SearchComparison::Decreased:
		{
			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			return memoryValueComparator(SearchComparison::LessThan, priorValue, readValue);
			break;
		}
		case SearchComparison::DecreasedBy:
		{
			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			const T expectedDecrease = priorValue - searchValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedDecrease);
			break;
		}
		case SearchComparison::Changed:
		case SearchComparison::NotChanged:
		{
			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			return memoryValueComparator(isNotOperator ? SearchComparison::Equals : SearchComparison::NotEquals, priorValue, readValue);
			break;
		}
		case SearchComparison::ChangedBy:
		{
			const T priorValue = searchResults.value(searchAddress).getValue<T>();
			const T expectedIncrease = searchValue + priorValue;
			const T expectedDecrease = priorValue - searchValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedIncrease) || memoryValueComparator(SearchComparison::Equals, readValue, expectedDecrease);
		}
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			return false;
	}
}

template <typename T>
SearchResults searchWorker(DebugInterface* cpu, SearchResults searchResults, SearchType searchType, SearchComparison searchComparison, u32 start, u32 end, T searchValue)
{
	SearchResults newSearchResults;
	const bool isSearchingRange = searchResults.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += sizeof(T))
		{
			if (!cpu->isValidAddress(addr))
				continue;
			
			T readValue = readValueAtAddress<T>(cpu, addr);
			if (handleSearchComparison(searchComparison, addr, searchResults, searchValue, readValue))
			{
				newSearchResults.insert(addr, MemorySearchWidget::SearchResult(addr, QVariant::fromValue(readValue), searchType));
			}
		}
	}
	else
	{
		for (const MemorySearchWidget::SearchResult& searchResult : searchResults)
		{
			const u32 addr = searchResult.getAddress();
			if (!cpu->isValidAddress(addr))
				continue;
			T readValue = readValueAtAddress<T>(cpu, addr);
			if (handleSearchComparison(searchComparison, addr, searchResults, searchValue, readValue))
			{
				newSearchResults.insert(addr, MemorySearchWidget::SearchResult(addr, QVariant::fromValue(readValue), searchType));
			}
		}
	}
	return newSearchResults;
}

static bool compareByteArrayAtAddress(DebugInterface* cpu, SearchComparison searchComparison, u32 addr, QByteArray value)
{
	const bool isNotOperator = searchComparison == SearchComparison::NotEquals;
	for (qsizetype i = 0; i < value.length(); i++)
	{
		const char nextByte = cpu->read8(addr + i);
		switch (searchComparison)
		{
			case SearchComparison::Equals:
			{
				if (nextByte != value[i])
					return false;
				break;
			}
			case SearchComparison::NotEquals:
			{
				if (nextByte != value[i])
					return true;
				break;
			}
			default:
			{
				Console.Error("Debugger: Unknown search comparison when doing memory search");
				return false;
			}
		}
	}
	return !isNotOperator;
}

bool handleArraySearchComparison(DebugInterface* cpu, SearchComparison searchComparison, u32 searchAddress, SearchResults searchResults, QByteArray searchValue)
{
	const bool isNotOperator = searchComparison == SearchComparison::NotEquals || searchComparison == SearchComparison::NotChanged;
	switch (searchComparison)
	{
		case SearchComparison::Equals:
		case SearchComparison::NotEquals:
		{
			return compareByteArrayAtAddress(cpu, searchComparison, searchAddress, searchValue);
			break;
		}
		case SearchComparison::Changed:
		case SearchComparison::NotChanged:
		{
			QByteArray priorValue = searchResults.value(searchAddress).getArrayValue();
			return compareByteArrayAtAddress(cpu, isNotOperator ? SearchComparison::Equals : SearchComparison::NotEquals, searchAddress, priorValue);
			break;
		}
		default:
		{
			Console.Error("Debugger: Unknown search comparison when doing memory search");
			return false;
		}
	}
	// Default to no match found unless the comparison is a NotEquals
	return isNotOperator;
}

static QByteArray readArrayAtAddress(DebugInterface* cpu, u32 address, u32 length)
{
	QByteArray readArray;
	for (u32 i = address; i < address + length; i++)
	{
		readArray.append(cpu->read8(i));
	}
	return readArray;
}

static SearchResults searchWorkerByteArray(DebugInterface* cpu, SearchType searchType, SearchComparison searchComparison, SearchResults searchResults, u32 start, u32 end, QByteArray searchValue)
{
	SearchResults newResults;
	const bool isSearchingRange = searchResults.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += 1)
		{
			if (!cpu->isValidAddress(addr))
				continue;
			if (handleArraySearchComparison(cpu, searchComparison, addr, searchResults, searchValue))
			{
				newResults.insert(addr, MemorySearchWidget::SearchResult(addr, searchValue, searchType));
				addr += searchValue.length() - 1;
			}
		}
	}
	else
	{
		for (MemorySearchWidget::SearchResult searchResult : searchResults)
		{
			const u32 addr = searchResult.getAddress();
			if (!cpu->isValidAddress(addr))
				continue;
			if (handleArraySearchComparison(cpu, searchComparison, addr, searchResults, searchValue))
			{
				QByteArray matchValue;
				if (searchComparison == SearchComparison::Equals)
					matchValue = searchValue;
				else if (searchComparison == SearchComparison::NotChanged)
					matchValue = searchResult.getArrayValue();
				else
					matchValue = readArrayAtAddress(cpu, addr, searchValue.length() - 1);
				newResults.insert(addr, MemorySearchWidget::SearchResult(addr, matchValue, searchType));
			}
		}
	}
	return newResults;
}

SearchResults startWorker(DebugInterface* cpu, const SearchType type, const SearchComparison comparison, SearchResults searchResults, u32 start, u32 end, QString value, int base)
{
	const bool isSigned = value.startsWith("-");
	switch (type)
	{
		case SearchType::ByteType:
			return isSigned ? searchWorker<s8>(cpu, searchResults, type, comparison, start, end, value.toShort(nullptr, base)) : searchWorker<u8>(cpu, searchResults, type, comparison, start, end, value.toUShort(nullptr, base));
		case SearchType::Int16Type:
			return isSigned ? searchWorker<s16>(cpu, searchResults, type, comparison, start, end, value.toShort(nullptr, base)) : searchWorker<u16>(cpu, searchResults, type, comparison, start, end, value.toUShort(nullptr, base));
		case SearchType::Int32Type:
			return isSigned ? searchWorker<s32>(cpu, searchResults, type, comparison, start, end, value.toInt(nullptr, base)) : searchWorker<u32>(cpu, searchResults, type, comparison, start, end, value.toUInt(nullptr, base));
		case SearchType::Int64Type:
			return isSigned ? searchWorker<s64>(cpu, searchResults, type, comparison, start, end, value.toLong(nullptr, base)) : searchWorker<s64>(cpu, searchResults, type, comparison, start, end, value.toULongLong(nullptr, base));
		case SearchType::FloatType:
			return searchWorker<float>(cpu, searchResults, type, comparison, start, end, value.toFloat());
		case SearchType::DoubleType:
			return searchWorker<double>(cpu, searchResults, type, comparison, start, end, value.toDouble());
		case SearchType::StringType:
			return searchWorkerByteArray(cpu, type, comparison, searchResults, start, end, value.toUtf8());
		case SearchType::ArrayType:
			return searchWorkerByteArray(cpu, type, comparison, searchResults, start, end, QByteArray::fromHex(value.toUtf8()));
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			break;
	};
	return {};
}

void MemorySearchWidget::onSearchButtonClicked()
{
	if (!m_cpu->isAlive())
		return;

	const SearchType searchType = getCurrentSearchType();
	const bool searchHex = m_ui.chkSearchHex->isChecked();

	bool ok;
	const u32 searchStart = m_ui.txtSearchStart->text().toUInt(&ok, 16);

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid start address"));
		return;
	}

	const u32 searchEnd = m_ui.txtSearchEnd->text().toUInt(&ok, 16);

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid end address"));
		return;
	}

	if (searchStart >= searchEnd)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Start address can't be equal to or greater than the end address"));
		return;
	}

	const QString searchValue = m_ui.txtSearchValue->text();
	const SearchComparison searchComparison = getCurrentSearchComparison();
	const bool isFilterSearch = sender() == m_ui.btnFilterSearch;
	unsigned long long value;

	switch (searchType)
	{
		case SearchType::ByteType:
		case SearchType::Int16Type:
		case SearchType::Int32Type:
		case SearchType::Int64Type:
			value = searchValue.toULongLong(&ok, searchHex ? 16 : 10);
			break;
		case SearchType::FloatType:
		case SearchType::DoubleType:
			searchValue.toDouble(&ok);
			break;
		case SearchType::StringType:
			ok = !searchValue.isEmpty();
			break;
		case SearchType::ArrayType:
			ok = !searchValue.trimmed().isEmpty();
			break;
	}

	if (!ok)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Invalid search value"));
		return;
	}

	switch (searchType)
	{
		case SearchType::ArrayType:
		case SearchType::StringType:
		case SearchType::DoubleType:
		case SearchType::FloatType:
			break;
		case SearchType::Int64Type:
			if (value <= std::numeric_limits<unsigned long long>::max())
				break;
		case SearchType::Int32Type:
			if (value <= std::numeric_limits<unsigned long>::max())
				break;
		case SearchType::Int16Type:
			if (value <= std::numeric_limits<unsigned short>::max())
				break;
		case SearchType::ByteType:
			if (value <= std::numeric_limits<unsigned char>::max())
				break;
		default:
			QMessageBox::critical(this, tr("Debugger"), tr("Value is larger than type"));
			return;
	}

	QFutureWatcher<SearchResults>* workerWatcher = new QFutureWatcher<SearchResults>();
	auto onSearchFinished = [this, workerWatcher] {
		m_ui.btnSearch->setDisabled(false);

		m_ui.listSearchResults->clear();
		const auto& results = workerWatcher->future().result();

		m_searchResultsMap = results;
		loadSearchResults();
		m_ui.resultsCountLabel->setText(QString(tr("%0 results found")).arg(results.size()));
		m_ui.btnFilterSearch->setDisabled(m_ui.listSearchResults->count() == 0);
		updateSearchComparisonSelections();
	};
	connect(workerWatcher, &QFutureWatcher<std::vector<u32>>::finished, onSearchFinished);

	m_ui.btnSearch->setDisabled(true);
	SearchResults searchResultsMap;
	if (isFilterSearch)
	{
		searchResultsMap = m_searchResultsMap;
	}

	QFuture<SearchResults> workerFuture = QtConcurrent::run(startWorker, m_cpu, searchType, searchComparison, searchResultsMap, searchStart, searchEnd, searchValue, searchHex ? 16 : 10);
	workerWatcher->setFuture(workerFuture);
	connect(workerWatcher, &QFutureWatcher<SearchResults>::finished, onSearchFinished);
	m_ui.resultsCountLabel->setText(tr("Searching..."));
	m_ui.resultsCountLabel->setVisible(true);
}

void MemorySearchWidget::onSearchResultsListScroll(u32 value)
{
	const bool hasResultsToLoad = static_cast<qsizetype>(m_ui.listSearchResults->count()) < m_searchResultsMap.size();
	const bool scrolledSufficiently = value > (m_ui.listSearchResults->verticalScrollBar()->maximum() * 0.95);
	if (!m_resultsLoadTimer.isActive() && hasResultsToLoad && scrolledSufficiently)
	{
		// Load results once timer ends, allowing us to debounce repeated requests and only do one load.
		m_resultsLoadTimer.start();
	}
}

void MemorySearchWidget::loadSearchResults()
{
	const u32 numLoaded = m_ui.listSearchResults->count();
	const u32 amountLeftToLoad = m_searchResultsMap.size() - numLoaded;
	if (amountLeftToLoad < 1)
		return;

	const bool isFirstLoad = numLoaded == 0;
	const u32 maxLoadAmount = isFirstLoad ? m_initialResultsLoadLimit : m_numResultsAddedPerLoad;
	const u32 numToLoad = amountLeftToLoad > maxLoadAmount ? maxLoadAmount : amountLeftToLoad;

	const auto addresses = m_searchResultsMap.keys();
	for (u32 i = 0; i < numToLoad; i++)
	{
		const u32 address = addresses.at(numLoaded + i);
		QListWidgetItem* item = new QListWidgetItem(QtUtils::FilledQStringFromValue(address, 16));
		item->setData(Qt::UserRole, address);
		m_ui.listSearchResults->addItem(item);
	}
}

SearchType MemorySearchWidget::getCurrentSearchType()
{
	return static_cast<SearchType>(m_ui.cmbSearchType->currentIndex());
}

SearchComparison MemorySearchWidget::getCurrentSearchComparison()
{
	// Note: The index can't be converted directly to the enum value since we change what comparisons are shown.
	return m_searchComparisonLabelMap.labelToEnum(m_ui.cmbSearchComparison->currentText());
}

void MemorySearchWidget::onSearchTypeChanged(int newIndex)
{
	if (newIndex < 4)
		m_ui.chkSearchHex->setEnabled(true);
	else
		m_ui.chkSearchHex->setEnabled(false);

	// Clear existing search results when the comparison type changes
	if (m_searchResultsMap.size() > 0 && (int)(m_searchResultsMap.first().getType()) != newIndex)
	{
		m_searchResultsMap.clear();
		m_ui.btnSearch->setDisabled(false);
		m_ui.btnFilterSearch->setDisabled(true);
	}
	updateSearchComparisonSelections();
}

void MemorySearchWidget::updateSearchComparisonSelections()
{
	const QString selectedComparisonLabel = m_ui.cmbSearchComparison->currentText();
	const SearchComparison selectedComparison = m_searchComparisonLabelMap.labelToEnum(selectedComparisonLabel);

	const std::vector<SearchComparison> comparisons = getValidSearchComparisonsForState(getCurrentSearchType(), m_searchResultsMap);
	m_ui.cmbSearchComparison->clear();
	for (const SearchComparison comparison : comparisons)
	{
		m_ui.cmbSearchComparison->addItem(m_searchComparisonLabelMap.enumToLabel(comparison));
	}

	// Preserve selection if applicable
	if (selectedComparison == SearchComparison::Invalid)
		return;
	if (std::find(comparisons.begin(), comparisons.end(), selectedComparison) != comparisons.end())
		m_ui.cmbSearchComparison->setCurrentText(selectedComparisonLabel);
}

std::vector<SearchComparison> MemorySearchWidget::getValidSearchComparisonsForState(SearchType type, SearchResults existingResults)
{
	const bool hasResults = existingResults.size() > 0;
	std::vector<SearchComparison> comparisons = { SearchComparison::Equals };

	if (type == SearchType::ArrayType || type == SearchType::StringType)
	{
		if (hasResults && existingResults.first().isArrayValue())
		{
			comparisons.push_back(SearchComparison::NotEquals);
			comparisons.push_back(SearchComparison::Changed);
			comparisons.push_back(SearchComparison::NotChanged);
		}
		return comparisons;
	}
	comparisons.push_back(SearchComparison::NotEquals);
	comparisons.push_back(SearchComparison::GreaterThan);
	comparisons.push_back(SearchComparison::GreaterThanOrEqual);
	comparisons.push_back(SearchComparison::LessThan);
	comparisons.push_back(SearchComparison::LessThanOrEqual);

	if (hasResults && existingResults.first().getType() == type)
	{
		comparisons.push_back(SearchComparison::Increased);
		comparisons.push_back(SearchComparison::IncreasedBy);
		comparisons.push_back(SearchComparison::Decreased);
		comparisons.push_back(SearchComparison::DecreasedBy);
		comparisons.push_back(SearchComparison::Changed);
		comparisons.push_back(SearchComparison::ChangedBy);
		comparisons.push_back(SearchComparison::NotChanged);
	}
	return comparisons;
}
