/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "AppEventListeners.h"
#include <memory>

class BaseCpuUsageProvider
{
public:
	BaseCpuUsageProvider() {}
	virtual ~BaseCpuUsageProvider() = default;

	virtual bool IsImplemented() const=0;
	virtual void UpdateStats()=0;
	virtual int GetEEcorePct() const=0;
	virtual int GetGsPct() const=0;
	virtual int GetVUPct() const=0;
	virtual int GetGuiPct() const=0;
};

struct AllPCSX2Threads
{
	u64		ee, gs, vu, ui;
	u64		update;

	void LoadWithCurrentTimes();
	AllPCSX2Threads operator-( const AllPCSX2Threads& right ) const;
};

class CpuUsageProvider :
	public BaseCpuUsageProvider,
	public EventListener_CoreThread
{
public:
	static const uint QueueDepth = 4;

protected:
	AllPCSX2Threads m_queue[QueueDepth];

	uint	m_writepos;
	u32		m_pct_ee;
	u32		m_pct_gs;
	u32		m_pct_vu;
	u32		m_pct_ui;

public:
	CpuUsageProvider();
	virtual ~CpuUsageProvider() = default;

	bool IsImplemented() const;
	void Reset();
	void UpdateStats();
	int GetEEcorePct() const;
	int GetGsPct() const;
	int GetVUPct() const;
	int GetGuiPct() const;

protected:
	void CoreThread_OnResumed() { Reset(); }
};
