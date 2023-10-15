/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
