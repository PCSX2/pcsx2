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

using namespace QtUtils;

MemorySearchWidget::MemorySearchWidget(QWidget* parent)
    : QWidget(parent)
{
	m_ui.setupUi(this);
	this->repaint();

	m_ui.listSearchResults->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.btnSearch, &QPushButton::clicked, this, &MemorySearchWidget::onSearchButtonClicked);
	connect(m_ui.btnFilterSearch, &QPushButton::clicked, this, &MemorySearchWidget::onSearchButtonClicked);
	connect(m_ui.listSearchResults, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) // move back to cpu widget
	{
		emit switchToMemoryViewTab();
		emit goToAddressInMemoryView(item->text().toUInt(nullptr, 16));
	});
	connect(m_ui.listSearchResults->verticalScrollBar(), &QScrollBar::valueChanged, this, &MemorySearchWidget::onSearchResultsListScroll);
	connect(m_ui.listSearchResults, &QListView::customContextMenuRequested, this, &MemorySearchWidget::onListSearchResultsContextMenu);
	connect(m_ui.cmbSearchType, &QComboBox::currentIndexChanged, [this](int i) {
		if (i < 4)
			m_ui.chkSearchHex->setEnabled(true);
		else
			m_ui.chkSearchHex->setEnabled(false);
	});

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
	if (m_searchResults.size() > static_cast<size_t>(selectedResultIndex) && m_searchResults.at(selectedResultIndex) == rowToRemove->data(Qt::UserRole).toUInt())
	{
		m_searchResults.erase(m_searchResults.begin() + selectedResultIndex);
	}
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


template <typename T>
static T readValueAtAddress(DebugInterface* cpu, u32 addr)
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
				const T memValue = std::bit_cast<float, u32>(readValue);
				areValuesEqual = (fBottom < memValue && memValue < fTop);
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				const double dTop = searchValue + 0.00001f;
				const double dBottom = searchValue - 0.00001f;
				const double memValue = std::bit_cast<double, u64>(readValue);
				areValuesEqual = (dBottom < memValue && memValue < dTop);
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
				const T memValue = std::bit_cast<float, u32>(readValue);
				const bool isGreater = memValue > fTop;
				const bool isLesser = memValue < fBottom;
				return isGreaterOperator ? isGreater : isLesser;
			}
			else if (std::is_same_v<T, double>)
			{
				const double dTop = searchValue + 0.00001f;
				const double dBottom = searchValue - 0.00001f;
				const double memValue = std::bit_cast<double, u64>(readValue);
				const bool isGreater = memValue > dTop;
				const bool isLesser = memValue < dBottom;
				return isGreaterOperator ? isGreater : isLesser;
			}

			return isGreaterOperator ? (readValue > searchValue) : (readValue < searchValue);
		}
		default:
			Console.Error("Debugger: Unknown type when doing memory search!");
			return false;
	}
}

template <typename T>
std::vector<u32> searchWorker(DebugInterface* cpu, std::vector<u32> searchAddresses, SearchComparison searchComparison, u32 start, u32 end, T searchValue)
{
	std::vector<u32> hitAddresses;
	const bool isSearchingRange = searchAddresses.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += sizeof(T))
		{
			if (!cpu->isValidAddress(addr))
				continue;
			T readValue = readValueAtAddress<T>(cpu, addr);
			if (memoryValueComparator(searchComparison, searchValue, readValue))
			{
				hitAddresses.push_back(addr);
			}
		}
	}
	else
	{
		for (const u32 addr : searchAddresses)
		{
			if (!cpu->isValidAddress(addr))
				continue;
			T readValue = readValueAtAddress<T>(cpu, addr);
			if (memoryValueComparator(searchComparison, searchValue, readValue))
			{
				hitAddresses.push_back(addr);
			}
		}
	}
	return hitAddresses;
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

static std::vector<u32> searchWorkerByteArray(DebugInterface* cpu, SearchComparison searchComparison, std::vector<u32> searchAddresses, u32 start, u32 end, QByteArray value)
{
	std::vector<u32> hitAddresses;
	const bool isSearchingRange = searchAddresses.size() <= 0;
	if (isSearchingRange)
	{
		for (u32 addr = start; addr < end; addr += 1)
		{
			if (compareByteArrayAtAddress(cpu, searchComparison, addr, value))
			{
				hitAddresses.emplace_back(addr);
				addr += value.length() - 1;
			}
		}
	}
	else
	{
		for (u32 addr : searchAddresses)
		{
			if (compareByteArrayAtAddress(cpu, searchComparison, addr, value))
			{
				hitAddresses.emplace_back(addr);
			}
		}
	}
	return hitAddresses;
}

std::vector<u32> startWorker(DebugInterface* cpu, const SearchType type, const SearchComparison searchComparison, std::vector<u32> searchAddresses, u32 start, u32 end, QString value, int base)
{
	const bool isSigned = value.startsWith("-");
	switch (type)
	{
		case SearchType::ByteType:
			return isSigned ? searchWorker<s8>(cpu, searchAddresses, searchComparison, start, end, value.toShort(nullptr, base)) : searchWorker<u8>(cpu, searchAddresses, searchComparison, start, end, value.toUShort(nullptr, base));
		case SearchType::Int16Type:
			return isSigned ? searchWorker<s16>(cpu, searchAddresses, searchComparison, start, end, value.toShort(nullptr, base)) : searchWorker<u16>(cpu, searchAddresses, searchComparison, start, end, value.toUShort(nullptr, base));
		case SearchType::Int32Type:
			return isSigned ? searchWorker<s32>(cpu, searchAddresses, searchComparison, start, end, value.toInt(nullptr, base)) : searchWorker<u32>(cpu, searchAddresses, searchComparison, start, end, value.toUInt(nullptr, base));
		case SearchType::Int64Type:
			return isSigned ? searchWorker<s64>(cpu, searchAddresses, searchComparison, start, end, value.toLong(nullptr, base)) : searchWorker<s64>(cpu, searchAddresses, searchComparison, start, end, value.toULongLong(nullptr, base));
		case SearchType::FloatType:
			return searchWorker<float>(cpu, searchAddresses, searchComparison, start, end, value.toFloat());
		case SearchType::DoubleType:
			return searchWorker<double>(cpu, searchAddresses, searchComparison, start, end, value.toDouble());
		case SearchType::StringType:
			return searchWorkerByteArray(cpu, searchComparison, searchAddresses, start, end, value.toUtf8());
		case SearchType::ArrayType:
			return searchWorkerByteArray(cpu, searchComparison, searchAddresses, start, end, QByteArray::fromHex(value.toUtf8()));
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

	const SearchType searchType = static_cast<SearchType>(m_ui.cmbSearchType->currentIndex());
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
	const SearchComparison searchComparison = static_cast<SearchComparison>(m_ui.cmbSearchComparison->currentIndex());
	const bool isFilterSearch = sender() == m_ui.btnFilterSearch;
	unsigned long long value;

	const bool isVariableSize = searchType == SearchType::ArrayType || searchType == SearchType::StringType;
	if (isVariableSize && !isFilterSearch && searchComparison == SearchComparison::NotEquals)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Search types Array and String can use the Not Equals search comparison type with new searches."));
		return;
	}

	if (isVariableSize && searchComparison != SearchComparison::Equals && searchComparison != SearchComparison::NotEquals)
	{
		QMessageBox::critical(this, tr("Debugger"), tr("Search types Array and String can only be used with Equals search comparisons."));
		return;
	}

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

	QFutureWatcher<std::vector<u32>>* workerWatcher = new QFutureWatcher<std::vector<u32>>;

	connect(workerWatcher, &QFutureWatcher<std::vector<u32>>::finished, [this, workerWatcher] {
		m_ui.btnSearch->setDisabled(false);

		m_ui.listSearchResults->clear();
		const auto& results = workerWatcher->future().result();

		m_searchResults = results;
		loadSearchResults();
		m_ui.btnFilterSearch->setDisabled(m_ui.listSearchResults->count() == 0);
	});

	m_ui.btnSearch->setDisabled(true);
	std::vector<u32> addresses;
	if (isFilterSearch)
	{
		addresses = m_searchResults;
	}
	QFuture<std::vector<u32>> workerFuture =
		QtConcurrent::run(startWorker, m_cpu, searchType, searchComparison, addresses, searchStart, searchEnd, searchValue, searchHex ? 16 : 10);
	workerWatcher->setFuture(workerFuture);
}

void MemorySearchWidget::onSearchResultsListScroll(u32 value)
{
	bool hasResultsToLoad = static_cast<size_t>(m_ui.listSearchResults->count()) < m_searchResults.size();
	bool scrolledSufficiently = value > (m_ui.listSearchResults->verticalScrollBar()->maximum() * 0.95);

	if (!m_resultsLoadTimer.isActive() && hasResultsToLoad && scrolledSufficiently)
	{
		// Load results once timer ends, allowing us to debounce repeated requests and only do one load.
		m_resultsLoadTimer.start();
	}
}

void MemorySearchWidget::loadSearchResults()
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
		u32 address = m_searchResults.at(numLoaded + i);
		QListWidgetItem* item = new QListWidgetItem(QtUtils::FilledQStringFromValue(address, 16));
		item->setData(Qt::UserRole, address);
		m_ui.listSearchResults->addItem(item);
	}
}
