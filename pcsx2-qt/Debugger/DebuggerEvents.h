// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <common/Pcsx2Types.h>

#include <QtCore/qttranslation.h>

namespace DebuggerEvents
{
	enum Flags
	{
		NO_FLAGS = 0,
		// Set the debugger widget receiving this event as the current tab for
		// its group.
		SWITCH_TO_RECEIVER = 1 << 0
	};

	struct Event
	{
		u32 flags = NO_FLAGS;

		virtual ~Event() = default;
	};

	// Sent when a debugger widget is first created, and subsequently broadcast
	// to all debugger widgets at regular intervals.
	struct Refresh : Event
	{
	};

	struct GoToAddress : Event
	{
		enum Filter
		{
			NONE,
			DISASSEMBLER,
			MEMORY_VIEW
		};

		u32 address = 0;

		// Prevent the memory view from handling events for jumping to functions
		// and vice versa.
		Filter filter = NONE;

		static constexpr const char* TEXT = QT_TRANSLATE_NOOP("DebuggerEvent", "Go to");
	};

	// The state of the VM has changed and widgets should be updated to reflect
	// the new state (e.g. the VM has been paused).
	struct VMUpdate : Event
	{
	};

	struct BreakpointsChanged : Event
	{
	};

	struct AddToSavedAddresses : Event
	{
		u32 address = 0;
	};
} // namespace DebuggerEvents
