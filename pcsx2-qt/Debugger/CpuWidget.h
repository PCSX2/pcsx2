// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_CpuWidget.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/BiosDebugData.h"
#include "DebugTools/MipsStackWalk.h"

#include "Models/BreakpointModel.h"
#include "Models/ThreadModel.h"
#include "Models/StackModel.h"
#include "Models/SavedAddressesModel.h"

#include "QtHost.h"
#include <QtWidgets/QWidget>
#include <QtWidgets/QTableWidget>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>

#include <vector>

using namespace MipsStackWalk;

class CpuWidget final : public QWidget
{
	Q_OBJECT

public:
	CpuWidget(QWidget* parent, DebugInterface& cpu);
	~CpuWidget();

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
	void paintEvent(QPaintEvent* event);

	void onStepInto();
	void onStepOver();
	void onStepOut();

	void onVMPaused();

	void updateBreakpoints();
	void onBPListDoubleClicked(const QModelIndex& index);
	void onBPListContextMenu(QPoint pos);

	void contextBPListCopy();
	void contextBPListDelete();
	void contextBPListNew();
	void contextBPListEdit();
	void contextBPListPasteCSV();

	void onSavedAddressesListContextMenu(QPoint pos);
	void contextSavedAddressesListPasteCSV();
	void contextSavedAddressesListNew();
	void addAddressToSavedAddressesList(u32 address);

	void updateThreads();
	void onThreadListDoubleClick(const QModelIndex& index);
	void onThreadListContextMenu(QPoint pos);

	void updateStackFrames();
	void onStackListContextMenu(QPoint pos);
	void onStackListDoubleClick(const QModelIndex& index);

	void updateFunctionList(bool whenEmpty = false);
	void onFuncListContextMenu(QPoint pos);
	void onFuncListDoubleClick(QListWidgetItem* item);
	bool getDemangleFunctions() const { return m_demangleFunctions; }
	void onModuleTreeContextMenu(QPoint pos);
	void onModuleTreeDoubleClick(QTreeWidgetItem* item);
	void reloadCPUWidgets()
	{
		if (!QtHost::IsOnUIThread())
		{
			QtHost::RunOnUIThread(CBreakPoints::GetUpdateHandler());
			return;
		}

		updateBreakpoints();
		updateThreads();
		updateStackFrames();

		m_ui.registerWidget->update();
		m_ui.disassemblyWidget->update();
		m_ui.memoryviewWidget->update();
	};

	void onSearchButtonClicked();
	void onSearchResultsListScroll(u32 value);
	void loadSearchResults();
	void contextSearchResultGoToDisassembly();
	void contextRemoveSearchResult();
	void onListSearchResultsContextMenu(QPoint pos);

	void saveBreakpointsToDebuggerSettings();
	void saveSavedAddressesToDebuggerSettings();

private:
	std::vector<QTableWidget*> m_registerTableViews;
	std::vector<u32> m_searchResults;

	QMenu* m_stacklistContextMenu = 0;
	QMenu* m_funclistContextMenu = 0;
	QMenu* m_moduleTreeContextMenu = 0;

	Ui::CpuWidget m_ui;

	DebugInterface& m_cpu;

	BreakpointModel m_bpModel;
	ThreadModel m_threadModel;
	QSortFilterProxyModel m_threadProxyModel;
	StackModel m_stackModel;
	SavedAddressesModel m_savedAddressesModel;
	QTimer m_resultsLoadTimer;

	bool m_demangleFunctions = true;
	bool m_moduleView = true;
	u32 m_initialResultsLoadLimit = 20000;
	u32 m_numResultsAddedPerLoad = 10000;
};
