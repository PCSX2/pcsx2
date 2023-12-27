// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QAbstractTableModel>
#include <QtWidgets/QHeaderView>

#include "DebugTools/DebugInterface.h"
#include "DebugTools/BiosDebugData.h"

#include <map>

class ThreadModel : public QAbstractTableModel
{
	Q_OBJECT

public:
	enum ThreadColumns : int
	{
		ID = 0,
		PC,
		ENTRY,
		PRIORITY,
		STATE,
		WAIT_TYPE,
		COLUMN_COUNT
	};

	static constexpr QHeaderView::ResizeMode HeaderResizeModes[ThreadColumns::COLUMN_COUNT] =
	{
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::ResizeToContents,
		QHeaderView::ResizeMode::Stretch,
		QHeaderView::ResizeMode::Stretch,
	};

	explicit ThreadModel(DebugInterface& cpu, QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

	void refreshData();

private:
	const std::map<ThreadStatus, QString> ThreadStateStrings{
		//ADDING I18N comments here because the context string added by QtLinguist does not mention that these are thread states.
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_BAD, tr("BAD")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_RUN, tr("RUN")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_READY, tr("READY")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_WAIT, tr("WAIT")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_SUSPEND, tr("SUSPEND")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_WAIT_SUSPEND, tr("WAIT SUSPEND")},
		//: Refers to a Thread State in the Debugger.
		{ThreadStatus::THS_DORMANT, tr("DORMANT")},
	};

	const std::map<WaitState, QString> ThreadWaitStrings{
		//ADDING I18N comments here because the context string added by QtLinguist does not mention that these are thread wait states.
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::NONE, tr("NONE")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::WAKEUP_REQ, tr("WAKEUP REQUEST")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::SEMA, tr("SEMAPHORE")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::SLEEP, tr("SLEEP")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::DELAY, tr("DELAY")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::EVENTFLAG, tr("EVENTFLAG")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::MBOX, tr("MBOX")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::VPOOL, tr("VPOOL")},
		//: Refers to a Thread Wait State in the Debugger.
		{WaitState::FIXPOOL, tr("FIXPOOL")},
	};

	DebugInterface& m_cpu;
};
