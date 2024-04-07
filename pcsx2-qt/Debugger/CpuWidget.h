// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_CpuWidget.h"

#include "DebugTools/DebugInterface.h"

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

public slots:
	void paintEvent(QPaintEvent* event);

	void onStepInto();
	void onStepOver();
	void onStepOut();

	void onVMPaused();

	void updateBreakpoints();
	void onBPListDoubleClicked(const QModelIndex& index);
	void onBPListContextMenu(QPoint pos);
	void onGotoInMemory(u32 address);

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
	void refreshDebugger();
	void reloadCPUWidgets();

	void saveBreakpointsToDebuggerSettings();
	void saveSavedAddressesToDebuggerSettings();

private:
	std::vector<QTableWidget*> m_registerTableViews;

	QMenu* m_stacklistContextMenu = 0;
	QMenu* m_funclistContextMenu = 0;
	QMenu* m_moduleTreeContextMenu = 0;
	QTimer m_refreshDebuggerTimer;

	Ui::CpuWidget m_ui;

	DebugInterface& m_cpu;

	BreakpointModel m_bpModel;
	ThreadModel m_threadModel;
	QSortFilterProxyModel m_threadProxyModel;
	StackModel m_stackModel;
	SavedAddressesModel m_savedAddressesModel;

	bool m_demangleFunctions = true;
	bool m_moduleView = true;
};
