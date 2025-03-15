// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <common/Pcsx2Types.h>

#include <QtCore/qttranslation.h>

namespace DebuggerEvents
{
	enum EventFlags
	{
		NO_EVENT_FLAGS = 0,
		// Broadcast events to debugger widgets from all layouts rather than
		// just the currently selected one. No effect for sending events to
		// individual debugger widgets.
		ALL_LAYOUTS = 1 << 0
	};

	struct Event
	{
		virtual ~Event() = default;
	};

	// Sent when a debugger widget is first created, and subsequently broadcast
	// to all debugger widgets at regular intervals.
	struct Refresh : Event
	{
		static constexpr const u32 FLAGS = NO_EVENT_FLAGS;
	};

	// Go to the address in a disassembly or memory view and switch to that tab.
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

		bool switch_to_tab = true;

		static constexpr const u32 FLAGS = NO_EVENT_FLAGS;
		static constexpr const char* ACTION_PREFIX = QT_TRANSLATE_NOOP("DebuggerEvents", "Go to in");
	};

	// The state of the VM has changed and widgets should be updated to reflect
	// the new state (e.g. the VM has been paused).
	struct VMUpdate : Event
	{
		static constexpr const u32 FLAGS = NO_EVENT_FLAGS;
	};

	// The VM has been paused. This event will not be sent if the breakpoint
	// code triggered the pause.
	struct VMPaused : Event
	{
		static constexpr const u32 FLAGS = ALL_LAYOUTS;
	};

	// Add the address to the saved addresses list and switch to that tab.
	struct AddToSavedAddresses : Event
	{
		u32 address = 0;
		bool switch_to_tab = true;

		static constexpr const u32 FLAGS = NO_EVENT_FLAGS;
		static constexpr const char* ACTION_PREFIX = QT_TRANSLATE_NOOP("DebuggerEvents", "Add to");
	};
} // namespace DebuggerEvents
