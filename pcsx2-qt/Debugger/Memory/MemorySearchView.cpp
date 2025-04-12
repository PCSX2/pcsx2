// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemorySearchView.h"

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

using SearchComparison = MemorySearchView::SearchComparison;
using SearchType = MemorySearchView::SearchType;
using SearchResult = MemorySearchView::SearchResult;

using namespace QtUtils;

MemorySearchView::MemorySearchView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
{
	m_ui.setupUi(this);
	this->repaint();

	m_ui.listSearchResults->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.btnSearch, &QPushButton::clicked, this, &MemorySearchView::onSearchButtonClicked);
	connect(m_ui.btnFilterSearch, &QPushButton::clicked, this, &MemorySearchView::onSearchButtonClicked);
	connect(m_ui.listSearchResults, &QListWidget::itemDoubleClicked, [](QListWidgetItem* item) {
		goToInMemoryView(item->text().toUInt(nullptr, 16), true);
	});
	connect(m_ui.listSearchResults->verticalScrollBar(), &QScrollBar::valueChanged, this, &MemorySearchView::onSearchResultsListScroll);
	connect(m_ui.listSearchResults, &QListView::customContextMenuRequested, this, &MemorySearchView::onListSearchResultsContextMenu);
	connect(m_ui.cmbSearchType, &QComboBox::currentIndexChanged, this, &MemorySearchView::onSearchTypeChanged);
	connect(m_ui.cmbSearchComparison, &QComboBox::currentIndexChanged, this, &MemorySearchView::onSearchComparisonChanged);

	// Ensures we don't retrigger the load results function unintentionally
	m_resultsLoadTimer.setInterval(100);
	m_resultsLoadTimer.setSingleShot(true);
	connect(&m_resultsLoadTimer, &QTimer::timeout, this, &MemorySearchView::loadSearchResults);

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		update();
		return true;
	});
}

void MemorySearchView::contextRemoveSearchResult()
{
	const QItemSelectionModel* selModel = m_ui.listSearchResults->selectionModel();
	if (!selModel->hasSelection())
		return;

	const int selectedResultIndex = m_ui.listSearchResults->row(m_ui.listSearchResults->selectedItems().first());
	const auto* rowToRemove = m_ui.listSearchResults->takeItem(selectedResultIndex);
	u32 address = rowToRemove->data(Qt::UserRole).toUInt();
	if (m_searchResults.size() > static_cast<size_t>(selectedResultIndex) && m_searchResults.at(selectedResultIndex).getAddress() == address)
	{
		m_searchResults.erase(m_searchResults.begin() + selectedResultIndex);
	}
	delete rowToRemove;
}

void MemorySearchView::contextCopySearchResultAddress()
{
	if (!m_ui.listSearchResults->selectionModel()->hasSelection())
		return;

	const u32 selectedResultIndex = m_ui.listSearchResults->row(m_ui.listSearchResults->selectedItems().first());
	const u32 rowAddress = m_ui.listSearchResults->item(selectedResultIndex)->data(Qt::UserRole).toUInt();
	const QString addressString = FilledQStringFromValue(rowAddress, 16);
	QApplication::clipboard()->setText(addressString);
}

void MemorySearchView::onListSearchResultsContextMenu(QPoint pos)
{
	const QItemSelectionModel* selection_model = m_ui.listSearchResults->selectionModel();
	const QListWidget* list_search_results = m_ui.listSearchResults;

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	if (selection_model->hasSelection())
	{
		connect(menu->addAction(tr("Copy Address")), &QAction::triggered,
			this, &MemorySearchView::contextCopySearchResultAddress);

		createEventActions<DebuggerEvents::GoToAddress>(menu, [list_search_results]() {
			u32 selected_address = list_search_results->selectedItems().first()->data(Qt::UserRole).toUInt();
			DebuggerEvents::GoToAddress event;
			event.address = selected_address;
			return std::optional(event);
		});

		createEventActions<DebuggerEvents::AddToSavedAddresses>(menu, [list_search_results]() {
			u32 selected_address = list_search_results->selectedItems().first()->data(Qt::UserRole).toUInt();
			DebuggerEvents::AddToSavedAddresses event;
			event.address = selected_address;
			return std::optional(event);
		});

		connect(menu->addAction(tr("Remove Result")), &QAction::triggered,
			this, &MemorySearchView::contextRemoveSearchResult);
	}

	menu->popup(m_ui.listSearchResults->viewport()->mapToGlobal(pos));
}

template <typename T>
T readValueAtAddress(DebugInterface* cpu, u32 addr);
template <>
float readValueAtAddress<float>(DebugInterface* cpu, u32 addr)
{
	return std::bit_cast<float>(cpu->read32(addr));
}

template <>
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
bool handleSearchComparison(SearchComparison searchComparison, u32 searchAddress, const SearchResult* priorResult, T searchValue, T readValue)
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
			const T priorValue = priorResult->getValue<T>();
			return memoryValueComparator(SearchComparison::GreaterThan, priorValue, readValue);
			break;
		}
		case SearchComparison::IncreasedBy:
		{
			const T priorValue = priorResult->getValue<T>();
			const T expectedIncrease = searchValue + priorValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedIncrease);
			break;
		}
		case SearchComparison::Decreased:
		{
			const T priorValue = priorResult->getValue<T>();
			return memoryValueComparator(SearchComparison::LessThan, priorValue, readValue);
			break;
		}
		case SearchComparison::DecreasedBy:
		{
			const T priorValue = priorResult->getValue<T>();
			const T expectedDecrease = priorValue - searchValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedDecrease);
			break;
		}
		case SearchComparison::Changed:
		case SearchComparison::NotChanged:
		{
			const T priorValue = priorResult->getValue<T>();
			return memoryValueComparator(isNotOperator ? SearchComparison::Equals : SearchComparison::NotEquals, priorValue, readValue);
			break;
		}
		case SearchComparison::ChangedBy:
		{
			const T priorValue = priorResult->getValue<T>();
			const T expectedIncrease = searchValue + priorValue;
			const T expectedDecrease = priorValue - searchValue;
			return memoryValueComparator(SearchComparison::Equals, readValue, expectedIncrease) || memoryValueComparator(SearchComparison::Equals, readValue, expectedDecrease);
		}
		case SearchComparison::UnknownValue:
		{
			return true;
		}
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			return false;
	}
}

template <typename T>
void searchWorker(DebugInterface* cpu, std::vector<SearchResult>& searchResults, SearchType searchType, SearchComparison searchComparison, u32 start, u32 end, T searchValue)
{
	const bool isSearchingRange = searchResults.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += sizeof(T))
		{
			if (!cpu->isValidAddress(addr))
				continue;

			T readValue = readValueAtAddress<T>(cpu, addr);
			if (handleSearchComparison(searchComparison, addr, nullptr, searchValue, readValue))
			{
				searchResults.push_back(MemorySearchView::SearchResult(addr, QVariant::fromValue(readValue), searchType));
			}
		}
	}
	else
	{
		auto removeIt = std::remove_if(searchResults.begin(), searchResults.end(), [cpu, searchType, searchComparison, searchValue](SearchResult& searchResult) -> bool {
			const u32 addr = searchResult.getAddress();
			if (!cpu->isValidAddress(addr))
				return true;

			const auto readValue = readValueAtAddress<T>(cpu, addr);

			const bool doesMatch = handleSearchComparison(searchComparison, addr, &searchResult, searchValue, readValue);
			if (!doesMatch)
				searchResult = MemorySearchView::SearchResult(addr, QVariant::fromValue(readValue), searchType);

			return !doesMatch;
		});
		searchResults.erase(removeIt, searchResults.end());
	}
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

bool handleArraySearchComparison(DebugInterface* cpu, SearchComparison searchComparison, u32 searchAddress, SearchResult* priorResult, QByteArray searchValue)
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
			QByteArray priorValue = priorResult->getArrayValue();
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

static void searchWorkerByteArray(DebugInterface* cpu, SearchType searchType, SearchComparison searchComparison, std::vector<SearchResult>& searchResults, u32 start, u32 end, QByteArray searchValue)
{
	const bool isSearchingRange = searchResults.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += 1)
		{
			if (!cpu->isValidAddress(addr))
				continue;
			if (handleArraySearchComparison(cpu, searchComparison, addr, nullptr, searchValue))
			{
				searchResults.push_back(MemorySearchView::SearchResult(addr, searchValue, searchType));
				addr += searchValue.length() - 1;
			}
		}
	}
	else
	{
		auto removeIt = std::remove_if(searchResults.begin(), searchResults.end(), [searchComparison, searchType, searchValue, cpu](SearchResult& searchResult) -> bool {
			const u32 addr = searchResult.getAddress();
			if (!cpu->isValidAddress(addr))
				return true;

			const bool doesMatch = handleArraySearchComparison(cpu, searchComparison, addr, &searchResult, searchValue);
			if (doesMatch)
			{
				QByteArray matchValue;
				if (searchComparison == SearchComparison::Equals)
					matchValue = searchValue;
				else if (searchComparison == SearchComparison::NotChanged)
					matchValue = searchResult.getArrayValue();
				else
					matchValue = readArrayAtAddress(cpu, addr, searchValue.length() - 1);
				searchResult = MemorySearchView::SearchResult(addr, matchValue, searchType);
			}
			return !doesMatch;
		});
		searchResults.erase(removeIt, searchResults.end());
	}
}

std::vector<SearchResult> startWorker(DebugInterface* cpu, const SearchType type, const SearchComparison comparison, std::vector<SearchResult> searchResults, u32 start, u32 end, QString value, int base)
{
	const bool isSigned = value.startsWith("-");
	switch (type)
	{
		case SearchType::ByteType:
			isSigned ? searchWorker<s8>(cpu, searchResults, type, comparison, start, end, value.toShort(nullptr, base)) : searchWorker<u8>(cpu, searchResults, type, comparison, start, end, value.toUShort(nullptr, base));
			break;
		case SearchType::Int16Type:
			isSigned ? searchWorker<s16>(cpu, searchResults, type, comparison, start, end, value.toShort(nullptr, base)) : searchWorker<u16>(cpu, searchResults, type, comparison, start, end, value.toUShort(nullptr, base));
			break;
		case SearchType::Int32Type:
			isSigned ? searchWorker<s32>(cpu, searchResults, type, comparison, start, end, value.toInt(nullptr, base)) : searchWorker<u32>(cpu, searchResults, type, comparison, start, end, value.toUInt(nullptr, base));
			break;
		case SearchType::Int64Type:
			isSigned ? searchWorker<s64>(cpu, searchResults, type, comparison, start, end, value.toLongLong(nullptr, base)) : searchWorker<u64>(cpu, searchResults, type, comparison, start, end, value.toULongLong(nullptr, base));
			break;
		case SearchType::FloatType:
			searchWorker<float>(cpu, searchResults, type, comparison, start, end, value.toFloat());
			break;
		case SearchType::DoubleType:
			searchWorker<double>(cpu, searchResults, type, comparison, start, end, value.toDouble());
			break;
		case SearchType::StringType:
			searchWorkerByteArray(cpu, type, comparison, searchResults, start, end, value.toUtf8());
			break;
		case SearchType::ArrayType:
			searchWorkerByteArray(cpu, type, comparison, searchResults, start, end, QByteArray::fromHex(value.toUtf8()));
			break;
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			return {};
	};
	return searchResults;
}

void MemorySearchView::onSearchButtonClicked()
{
	if (!cpu().isAlive())
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

	if (searchComparison != SearchComparison::UnknownValue)
	{
		if (doesSearchComparisonTakeInput(searchComparison))
		{
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
		}

		if (!isFilterSearch &&
			(searchComparison == SearchComparison::Changed ||
				searchComparison == SearchComparison::ChangedBy ||
				searchComparison == SearchComparison::Decreased ||
				searchComparison == SearchComparison::DecreasedBy ||
				searchComparison == SearchComparison::Increased ||
				searchComparison == SearchComparison::IncreasedBy ||
				searchComparison == SearchComparison::NotChanged))
		{
			QMessageBox::critical(this, tr("Debugger"), tr("This search comparison can only be used with filter searches."));
			return;
		}
	}

	if (!isFilterSearch && (searchComparison == SearchComparison::Changed ||
							   searchComparison == SearchComparison::ChangedBy ||
							   searchComparison == SearchComparison::Decreased ||
							   searchComparison == SearchComparison::DecreasedBy ||
							   searchComparison == SearchComparison::Increased ||
							   searchComparison == SearchComparison::IncreasedBy ||
							   searchComparison == SearchComparison::NotChanged))
	{
		QMessageBox::critical(this, tr("Debugger"), tr("This search comparison can only be used with filter searches."));
		return;
	}

	QFutureWatcher<std::vector<SearchResult>>* workerWatcher = new QFutureWatcher<std::vector<SearchResult>>();
	auto onSearchFinished = [this, workerWatcher] {
		m_ui.btnSearch->setDisabled(false);

		m_ui.listSearchResults->clear();
		const auto& results = workerWatcher->future().result();

		m_searchResults = std::move(results);
		loadSearchResults();
		m_ui.resultsCountLabel->setText(QString(tr("%0 results found")).arg(m_searchResults.size()));
		m_ui.btnFilterSearch->setDisabled(m_ui.listSearchResults->count() == 0);
		updateSearchComparisonSelections();
		delete workerWatcher;
	};
	connect(workerWatcher, &QFutureWatcher<std::vector<u32>>::finished, onSearchFinished);

	m_ui.btnSearch->setDisabled(true);
	if (!isFilterSearch)
	{
		m_searchResults.clear();
	}

	QFuture<std::vector<SearchResult>> workerFuture = QtConcurrent::run(startWorker, &cpu(), searchType, searchComparison, std::move(m_searchResults), searchStart, searchEnd, searchValue, searchHex ? 16 : 10);
	workerWatcher->setFuture(workerFuture);
	connect(workerWatcher, &QFutureWatcher<std::vector<SearchResult>>::finished, onSearchFinished);
	m_searchResults.clear();
	m_ui.resultsCountLabel->setText(tr("Searching..."));
	m_ui.resultsCountLabel->setVisible(true);
}

void MemorySearchView::onSearchResultsListScroll(u32 value)
{
	const bool hasResultsToLoad = static_cast<size_t>(m_ui.listSearchResults->count()) < m_searchResults.size();
	const bool scrolledSufficiently = value > (m_ui.listSearchResults->verticalScrollBar()->maximum() * 0.95);
	if (!m_resultsLoadTimer.isActive() && hasResultsToLoad && scrolledSufficiently)
	{
		// Load results once timer ends, allowing us to debounce repeated requests and only do one load.
		m_resultsLoadTimer.start();
	}
}

void MemorySearchView::loadSearchResults()
{
	const u32 numLoaded = m_ui.listSearchResults->count();
	const u32 amountLeftToLoad = m_searchResults.size() - numLoaded;
	if (amountLeftToLoad < 1)
		return;

	const bool isFirstLoad = numLoaded == 0;
	const u32 maxLoadAmount = isFirstLoad ? m_initialResultsLoadLimit : m_numResultsAddedPerLoad;
	const u32 numToLoad = amountLeftToLoad > maxLoadAmount ? maxLoadAmount : amountLeftToLoad;

	for (u32 i = 0; i < numToLoad; i++)
	{
		const u32 address = m_searchResults.at(numLoaded + i).getAddress();
		QListWidgetItem* item = new QListWidgetItem(QtUtils::FilledQStringFromValue(address, 16));
		item->setData(Qt::UserRole, address);
		m_ui.listSearchResults->addItem(item);
	}
}

SearchType MemorySearchView::getCurrentSearchType()
{
	return static_cast<SearchType>(m_ui.cmbSearchType->currentIndex());
}

SearchComparison MemorySearchView::getCurrentSearchComparison()
{
	// Note: The index can't be converted directly to the enum value since we change what comparisons are shown.
	return m_searchComparisonLabelMap.labelToEnum(m_ui.cmbSearchComparison->currentText());
}

bool MemorySearchView::doesSearchComparisonTakeInput(const SearchComparison comparison)
{
	switch (comparison)
	{
		case SearchComparison::Equals:
		case SearchComparison::NotEquals:
		case SearchComparison::GreaterThan:
		case SearchComparison::GreaterThanOrEqual:
		case SearchComparison::LessThan:
		case SearchComparison::LessThanOrEqual:
		case SearchComparison::IncreasedBy:
		case SearchComparison::DecreasedBy:
			return true;
		default:
			return false;
	}
}

void MemorySearchView::onSearchTypeChanged(int newIndex)
{
	if (newIndex < 4)
		m_ui.chkSearchHex->setEnabled(true);
	else
		m_ui.chkSearchHex->setEnabled(false);

	// Clear existing search results when the comparison type changes
	if (m_searchResults.size() > 0 && (int)(m_searchResults.front().getType()) != newIndex)
	{
		m_searchResults.clear();
		m_ui.btnSearch->setDisabled(false);
		m_ui.btnFilterSearch->setDisabled(true);
	}
	updateSearchComparisonSelections();
}

void MemorySearchView::onSearchComparisonChanged(int newValue)
{
	m_ui.txtSearchValue->setEnabled(getCurrentSearchComparison() != SearchComparison::UnknownValue);
}

void MemorySearchView::updateSearchComparisonSelections()
{
	const QString selectedComparisonLabel = m_ui.cmbSearchComparison->currentText();
	const SearchComparison selectedComparison = m_searchComparisonLabelMap.labelToEnum(selectedComparisonLabel);

	const std::vector<SearchComparison> comparisons = getValidSearchComparisonsForState(getCurrentSearchType(), m_searchResults);
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

std::vector<SearchComparison> MemorySearchView::getValidSearchComparisonsForState(SearchType type, std::vector<SearchResult>& existingResults)
{
	const bool hasResults = existingResults.size() > 0;
	std::vector<SearchComparison> comparisons = {SearchComparison::Equals};

	if (type == SearchType::ArrayType || type == SearchType::StringType)
	{
		if (hasResults && existingResults.front().isArrayValue())
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

	if (hasResults && existingResults.front().getType() == type)
	{
		comparisons.push_back(SearchComparison::Increased);
		comparisons.push_back(SearchComparison::IncreasedBy);
		comparisons.push_back(SearchComparison::Decreased);
		comparisons.push_back(SearchComparison::DecreasedBy);
		comparisons.push_back(SearchComparison::Changed);
		comparisons.push_back(SearchComparison::ChangedBy);
		comparisons.push_back(SearchComparison::NotChanged);
	}

	if (!hasResults)
	{
		comparisons.push_back(SearchComparison::UnknownValue);
	}

	return comparisons;
}
