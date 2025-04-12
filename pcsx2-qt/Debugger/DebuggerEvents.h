// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <common/Pcsx2Types.h>

#include <QtCore/qttranslation.h>

namespace DebuggerEvents
{
	struct Event
	{
		virtual ~Event() = default;
	};

	// Sent when a debugger view is first created, and subsequently broadcast to
	// all debugger views at regular intervals.
	struct Refresh : Event
	{
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

		static constexpr const char* ACTION_PREFIX = QT_TRANSLATE_NOOP("DebuggerEvents", "Go to in");
	};

	// The state of the VM has changed and views should be updated to reflect
	// the new state (e.g. the VM has been paused).
	struct VMUpdate : Event
	{
	};

	// Add the address to the saved addresses list and switch to that tab.
	struct AddToSavedAddresses : Event
	{
		u32 address = 0;
		bool switch_to_tab = true;

		static constexpr const char* ACTION_PREFIX = QT_TRANSLATE_NOOP("DebuggerEvents", "Add to");
	};
} // namespace DebuggerEvents
