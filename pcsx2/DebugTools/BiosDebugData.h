// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "common/Pcsx2Types.h"
#include <memory>
#include <vector>
#include "ps2/BiosTools.h"

enum class ThreadStatus
{
	THS_BAD = 0x00,
	THS_RUN = 0x01,
	THS_READY = 0x02,
	THS_WAIT = 0x04,
	THS_SUSPEND = 0x08,
	THS_WAIT_SUSPEND = 0x0C,
	THS_DORMANT = 0x10,
};


struct EEInternalThread
{ // internal struct
	u32 prev;
	u32 next;
	int status;
	u32 entry;
	u32 stack;
	u32 gpReg;
	short currentPriority;
	short initPriority;
	int waitType;
	int semaId;
	int wakeupCount;
	int attr;
	int option;
	u32 entry_init;
	int argc;
	u32 argstring;
	u32 stack_bottom; //FIXME
	int stackSize;
	u32 root;
	u32 heap_base;
};

// Not the full struct, just what we care about
struct IOPInternalThread
{
	u32 tid;
	u32 PC;
	u32 stackTop;
	u32 SavedSP;
	u32 status;
	u32 entrypoint;
	u32 waitstate;
	u32 initPriority;
};

enum class IOPWaitStatus
{
	TSW_SLEEP = 1,
	TSW_DELAY = 2,
	TSW_SEMA = 3,
	TSW_EVENTFLAG = 4,
	TSW_MBX = 5,
	TSW_VPL = 6,
	TSW_FPL = 7,
};

enum class EEWaitStatus
{
	WAIT_NONE = 0,
	WAIT_WAKEUP_REQ = 1,
	WAIT_SEMA = 2,
};

enum class WaitState
{
	NONE,
	WAKEUP_REQ,
	SEMA,
	SLEEP,
	DELAY,
	EVENTFLAG,
	MBOX,
	VPOOL,
	FIXPOOL,
};

class BiosThread
{
public:
	virtual ~BiosThread() = default;
	[[nodiscard]] virtual u32 TID() const = 0;
	[[nodiscard]] virtual u32 PC() const = 0;
	[[nodiscard]] virtual ThreadStatus Status() const = 0;
	[[nodiscard]] virtual WaitState Wait() const = 0;
	[[nodiscard]] virtual u32 EntryPoint() const = 0;
	[[nodiscard]] virtual u32 StackTop() const = 0;
	[[nodiscard]] virtual u32 Priority() const = 0;
};

class EEThread : public BiosThread
{
public:
	EEThread(int tid, EEInternalThread th)
		: tid(tid)
		, data(th)
	{
	}
	~EEThread() override = default;

	[[nodiscard]] u32 TID() const override { return tid; };
	[[nodiscard]] u32 PC() const override { return data.entry; };
	[[nodiscard]] ThreadStatus Status() const override { return static_cast<ThreadStatus>(data.status); };
	[[nodiscard]] WaitState Wait() const override
	{
		auto wait = static_cast<EEWaitStatus>(data.waitType);
		switch (wait)
		{
			case EEWaitStatus::WAIT_NONE:
				return WaitState::NONE;
			case EEWaitStatus::WAIT_WAKEUP_REQ:
				return WaitState::WAKEUP_REQ;
			case EEWaitStatus::WAIT_SEMA:
				return WaitState::SEMA;
		}
		return WaitState::NONE;
	};
	[[nodiscard]] u32 EntryPoint() const override { return data.entry_init; };
	[[nodiscard]] u32 StackTop() const override { return data.stack; };
	[[nodiscard]] u32 Priority() const override { return data.currentPriority; };

private:
	u32 tid;
	EEInternalThread data;
};

class IOPThread : public BiosThread
{
public:
	IOPThread(IOPInternalThread th)
		: data(th)
	{
	}
	~IOPThread() override = default;

	[[nodiscard]] u32 TID() const override { return data.tid; };
	[[nodiscard]] u32 PC() const override { return data.PC; };
	[[nodiscard]] ThreadStatus Status() const override { return static_cast<ThreadStatus>(data.status); };
	[[nodiscard]] WaitState Wait() const override
	{
		auto wait = static_cast<IOPWaitStatus>(data.waitstate);
		switch (wait)
		{
			case IOPWaitStatus::TSW_DELAY:
				return WaitState::DELAY;
			case IOPWaitStatus::TSW_EVENTFLAG:
				return WaitState::EVENTFLAG;
			case IOPWaitStatus::TSW_SLEEP:
				return WaitState::SLEEP;
			case IOPWaitStatus::TSW_SEMA:
				return WaitState::SEMA;
			case IOPWaitStatus::TSW_MBX:
				return WaitState::MBOX;
			case IOPWaitStatus::TSW_VPL:
				return WaitState::VPOOL;
			case IOPWaitStatus::TSW_FPL:
				return WaitState::FIXPOOL;
		}
		return WaitState::NONE;
	};
	[[nodiscard]] u32 EntryPoint() const override { return data.entrypoint; };
	[[nodiscard]] u32 StackTop() const override { return data.stackTop; };
	[[nodiscard]] u32 Priority() const override { return data.initPriority; };

private:
	IOPInternalThread data;
};

std::vector<std::unique_ptr<BiosThread>> getIOPThreads();
std::vector<std::unique_ptr<BiosThread>> getEEThreads();
