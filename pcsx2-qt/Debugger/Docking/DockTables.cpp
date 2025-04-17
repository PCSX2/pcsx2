// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockTables.h"

#include "Debugger/DebuggerEvents.h"
#include "Debugger/DisassemblyView.h"
#include "Debugger/RegisterView.h"
#include "Debugger/StackView.h"
#include "Debugger/ThreadView.h"
#include "Debugger/Breakpoints/BreakpointView.h"
#include "Debugger/Memory/MemorySearchView.h"
#include "Debugger/Memory/MemoryView.h"
#include "Debugger/Memory/SavedAddressesView.h"
#include "Debugger/SymbolTree/SymbolTreeViews.h"

using namespace DockUtils;

static void hashDefaultLayout(const DockTables::DefaultDockLayout& layout, u32& hash);
static void hashDefaultGroup(const DockTables::DefaultDockGroupDescription& group, u32& hash);
static void hashDefaultDockWidget(const DockTables::DefaultDockWidgetDescription& widget, u32& hash);
static void hashNumber(u32 number, u32& hash);
static void hashString(const char* string, u32& hash);

#define DEBUGGER_VIEW(type, display_name, preferred_location) \
	{ \
		#type, \
		{ \
			[](const DebuggerViewParameters& parameters) -> DebuggerView* { \
				DebuggerView* widget = new type(parameters); \
				widget->handleEvent(DebuggerEvents::Refresh()); \
				return widget; \
			}, \
				display_name, \
				preferred_location \
		} \
	}

const std::map<std::string, DockTables::DebuggerViewDescription> DockTables::DEBUGGER_VIEWS = {
	DEBUGGER_VIEW(BreakpointView, QT_TRANSLATE_NOOP("DebuggerView", "Breakpoints"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(DisassemblyView, QT_TRANSLATE_NOOP("DebuggerView", "Disassembly"), TOP_RIGHT),
	DEBUGGER_VIEW(FunctionTreeView, QT_TRANSLATE_NOOP("DebuggerView", "Functions"), TOP_LEFT),
	DEBUGGER_VIEW(GlobalVariableTreeView, QT_TRANSLATE_NOOP("DebuggerView", "Globals"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(LocalVariableTreeView, QT_TRANSLATE_NOOP("DebuggerView", "Locals"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(MemorySearchView, QT_TRANSLATE_NOOP("DebuggerView", "Memory Search"), TOP_LEFT),
	DEBUGGER_VIEW(MemoryView, QT_TRANSLATE_NOOP("DebuggerView", "Memory"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(ParameterVariableTreeView, QT_TRANSLATE_NOOP("DebuggerView", "Parameters"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(RegisterView, QT_TRANSLATE_NOOP("DebuggerView", "Registers"), TOP_LEFT),
	DEBUGGER_VIEW(SavedAddressesView, QT_TRANSLATE_NOOP("DebuggerView", "Saved Addresses"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(StackView, QT_TRANSLATE_NOOP("DebuggerView", "Stack"), BOTTOM_MIDDLE),
	DEBUGGER_VIEW(ThreadView, QT_TRANSLATE_NOOP("DebuggerView", "Threads"), BOTTOM_MIDDLE),
};

#undef DEBUGGER_VIEW

const std::vector<DockTables::DefaultDockLayout> DockTables::DEFAULT_DOCK_LAYOUTS = {
	{
		.name = QT_TRANSLATE_NOOP("DebuggerLayout", "R5900"),
		.cpu = BREAKPOINT_EE,
		.groups = {
			/* [DefaultDockGroup::TOP_RIGHT] = */ {KDDockWidgets::Location_OnRight, DefaultDockGroup::ROOT},
			/* [DefaultDockGroup::BOTTOM]    = */ {KDDockWidgets::Location_OnBottom, DefaultDockGroup::TOP_RIGHT},
			/* [DefaultDockGroup::TOP_LEFT]  = */ {KDDockWidgets::Location_OnLeft, DefaultDockGroup::TOP_RIGHT},
		},
		.widgets = {
			/* DefaultDockGroup::TOP_RIGHT */
			{"DisassemblyView", DefaultDockGroup::TOP_RIGHT},
			/* DefaultDockGroup::BOTTOM */
			{"MemoryView", DefaultDockGroup::BOTTOM},
			{"BreakpointView", DefaultDockGroup::BOTTOM},
			{"ThreadView", DefaultDockGroup::BOTTOM},
			{"StackView", DefaultDockGroup::BOTTOM},
			{"SavedAddressesView", DefaultDockGroup::BOTTOM},
			{"GlobalVariableTreeView", DefaultDockGroup::BOTTOM},
			{"LocalVariableTreeView", DefaultDockGroup::BOTTOM},
			{"ParameterVariableTreeView", DefaultDockGroup::BOTTOM},
			/* DefaultDockGroup::TOP_LEFT */
			{"RegisterView", DefaultDockGroup::TOP_LEFT},
			{"FunctionTreeView", DefaultDockGroup::TOP_LEFT},
			{"MemorySearchView", DefaultDockGroup::TOP_LEFT},
		},
		.toolbars = {
			"toolBarDebug",
			"toolBarFile",
		},
	},
	{
		.name = QT_TRANSLATE_NOOP("DebuggerLayout", "R3000"),
		.cpu = BREAKPOINT_IOP,
		.groups = {
			/* [DefaultDockGroup::TOP_RIGHT] = */ {KDDockWidgets::Location_OnRight, DefaultDockGroup::ROOT},
			/* [DefaultDockGroup::BOTTOM]    = */ {KDDockWidgets::Location_OnBottom, DefaultDockGroup::TOP_RIGHT},
			/* [DefaultDockGroup::TOP_LEFT]  = */ {KDDockWidgets::Location_OnLeft, DefaultDockGroup::TOP_RIGHT},
		},
		.widgets = {
			/* DefaultDockGroup::TOP_RIGHT */
			{"DisassemblyView", DefaultDockGroup::TOP_RIGHT},
			/* DefaultDockGroup::BOTTOM */
			{"MemoryView", DefaultDockGroup::BOTTOM},
			{"BreakpointView", DefaultDockGroup::BOTTOM},
			{"ThreadView", DefaultDockGroup::BOTTOM},
			{"StackView", DefaultDockGroup::BOTTOM},
			{"SavedAddressesView", DefaultDockGroup::BOTTOM},
			{"GlobalVariableTreeView", DefaultDockGroup::BOTTOM},
			{"LocalVariableTreeView", DefaultDockGroup::BOTTOM},
			{"ParameterVariableTreeView", DefaultDockGroup::BOTTOM},
			/* DefaultDockGroup::TOP_LEFT */
			{"RegisterView", DefaultDockGroup::TOP_LEFT},
			{"FunctionTreeView", DefaultDockGroup::TOP_LEFT},
			{"MemorySearchView", DefaultDockGroup::TOP_LEFT},
		},
		.toolbars = {
			"toolBarDebug",
			"toolBarFile",
		},
	},
};

const DockTables::DefaultDockLayout* DockTables::defaultLayout(const std::string& name)
{
	for (const DockTables::DefaultDockLayout& default_layout : DockTables::DEFAULT_DOCK_LAYOUTS)
		if (default_layout.name == name)
			return &default_layout;

	return nullptr;
}

u32 DockTables::hashDefaultLayouts()
{
	static std::optional<u32> hash;
	if (hash.has_value())
		return *hash;

	hash.emplace(0);

	u32 hash_version = 2;
	hashNumber(hash_version, *hash);

	hashNumber(static_cast<u32>(DEFAULT_DOCK_LAYOUTS.size()), *hash);
	for (const DefaultDockLayout& layout : DEFAULT_DOCK_LAYOUTS)
		hashDefaultLayout(layout, *hash);

	return *hash;
}

static void hashDefaultLayout(const DockTables::DefaultDockLayout& layout, u32& hash)
{
	hashString(layout.name.c_str(), hash);
	hashString(DebugInterface::cpuName(layout.cpu), hash);

	hashNumber(static_cast<u32>(layout.groups.size()), hash);
	for (const DockTables::DefaultDockGroupDescription& group : layout.groups)
		hashDefaultGroup(group, hash);

	hashNumber(static_cast<u32>(layout.widgets.size()), hash);
	for (const DockTables::DefaultDockWidgetDescription& widget : layout.widgets)
		hashDefaultDockWidget(widget, hash);

	hashNumber(static_cast<u32>(layout.toolbars.size()), hash);
	for (const std::string& toolbar : layout.toolbars)
		hashString(toolbar.c_str(), hash);
}

static void hashDefaultGroup(const DockTables::DefaultDockGroupDescription& group, u32& hash)
{
	// This is inline here so that it's obvious that changing it will affect the
	// result of the hash.
	const char* location = "";
	switch (group.location)
	{
		case KDDockWidgets::Location_None:
			location = "none";
			break;
		case KDDockWidgets::Location_OnLeft:
			location = "left";
			break;
		case KDDockWidgets::Location_OnTop:
			location = "top";
			break;
		case KDDockWidgets::Location_OnRight:
			location = "right";
			break;
		case KDDockWidgets::Location_OnBottom:
			location = "bottom";
			break;
	}

	hashString(location, hash);
	hashNumber(static_cast<u32>(group.parent), hash);
}

static void hashDefaultDockWidget(const DockTables::DefaultDockWidgetDescription& widget, u32& hash)
{
	hashString(widget.type.c_str(), hash);
	hashNumber(static_cast<u32>(widget.group), hash);
}

static void hashNumber(u32 number, u32& hash)
{
	hash = hash * 31 + number;
}

static void hashString(const char* string, u32& hash)
{
	u32 size = static_cast<u32>(strlen(string));
	hash = hash * 31 + size;
	for (u32 i = 0; i < size; i++)
		hash = hash * 31 + string[i];
}
