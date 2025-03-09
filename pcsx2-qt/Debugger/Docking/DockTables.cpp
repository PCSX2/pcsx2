// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockTables.h"

#include "Debugger/DebuggerEvents.h"
#include "Debugger/DisassemblyWidget.h"
#include "Debugger/RegisterWidget.h"
#include "Debugger/StackWidget.h"
#include "Debugger/ThreadWidget.h"
#include "Debugger/Breakpoints/BreakpointWidget.h"
#include "Debugger/Memory/MemorySearchWidget.h"
#include "Debugger/Memory/MemoryViewWidget.h"
#include "Debugger/Memory/SavedAddressesWidget.h"
#include "Debugger/SymbolTree/SymbolTreeWidgets.h"

#include "common/MD5Digest.h"

#include "fmt/format.h"

using namespace DockUtils;

#define DEBUGGER_WIDGET(type, display_name, preferred_location) \
	{ \
		#type, \
		{ \
			[](const DebuggerWidgetParameters& parameters) -> DebuggerWidget* { \
				DebuggerWidget* widget = new type(parameters); \
				widget->handleEvent(DebuggerEvents::Refresh()); \
				return widget; \
			}, \
				display_name, \
				preferred_location \
		} \
	}

const std::map<std::string, DockTables::DebuggerWidgetDescription> DockTables::DEBUGGER_WIDGETS = {
	DEBUGGER_WIDGET(BreakpointWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Breakpoints"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(DisassemblyWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Disassembly"), TOP_RIGHT),
	DEBUGGER_WIDGET(FunctionTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Functions"), TOP_LEFT),
	DEBUGGER_WIDGET(GlobalVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Globals"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(LocalVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Locals"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(MemorySearchWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Memory Search"), TOP_LEFT),
	DEBUGGER_WIDGET(MemoryViewWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Memory"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(ParameterVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Parameters"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(RegisterWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Registers"), TOP_LEFT),
	DEBUGGER_WIDGET(SavedAddressesWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Saved Addresses"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(StackWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Stack"), BOTTOM_MIDDLE),
	DEBUGGER_WIDGET(ThreadWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Threads"), BOTTOM_MIDDLE),
};

#undef DEBUGGER_WIDGET

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
			{"DisassemblyWidget", DefaultDockGroup::TOP_RIGHT},
			/* DefaultDockGroup::BOTTOM */
			{"MemoryViewWidget", DefaultDockGroup::BOTTOM},
			{"BreakpointWidget", DefaultDockGroup::BOTTOM},
			{"ThreadWidget", DefaultDockGroup::BOTTOM},
			{"StackWidget", DefaultDockGroup::BOTTOM},
			{"SavedAddressesWidget", DefaultDockGroup::BOTTOM},
			{"GlobalVariableTreeWidget", DefaultDockGroup::BOTTOM},
			{"LocalVariableTreeWidget", DefaultDockGroup::BOTTOM},
			{"ParameterVariableTreeWidget", DefaultDockGroup::BOTTOM},
			/* DefaultDockGroup::TOP_LEFT */
			{"RegisterWidget", DefaultDockGroup::TOP_LEFT},
			{"FunctionTreeWidget", DefaultDockGroup::TOP_LEFT},
			{"MemorySearchWidget", DefaultDockGroup::TOP_LEFT},
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
			{"DisassemblyWidget", DefaultDockGroup::TOP_RIGHT},
			/* DefaultDockGroup::BOTTOM */
			{"MemoryViewWidget", DefaultDockGroup::BOTTOM},
			{"BreakpointWidget", DefaultDockGroup::BOTTOM},
			{"ThreadWidget", DefaultDockGroup::BOTTOM},
			{"StackWidget", DefaultDockGroup::BOTTOM},
			{"SavedAddressesWidget", DefaultDockGroup::BOTTOM},
			{"GlobalVariableTreeWidget", DefaultDockGroup::BOTTOM},
			{"LocalVariableTreeWidget", DefaultDockGroup::BOTTOM},
			{"ParameterVariableTreeWidget", DefaultDockGroup::BOTTOM},
			/* DefaultDockGroup::TOP_LEFT */
			{"RegisterWidget", DefaultDockGroup::TOP_LEFT},
			{"FunctionTreeWidget", DefaultDockGroup::TOP_LEFT},
			{"MemorySearchWidget", DefaultDockGroup::TOP_LEFT},
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

const std::string& DockTables::hashDefaultLayouts()
{
	static std::string hash;
	if (!hash.empty())
		return hash;

	MD5Digest md5;

	u32 hash_version = 1;
	md5.Update(&hash_version, sizeof(hash_version));

	u32 layout_count = static_cast<u32>(DEFAULT_DOCK_LAYOUTS.size());
	md5.Update(&layout_count, sizeof(layout_count));

	for (const DefaultDockLayout& layout : DEFAULT_DOCK_LAYOUTS)
		hashDefaultLayout(layout, md5);

	u8 digest[16];
	md5.Final(digest);
	hash = fmt::format(
		"{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
		digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
		digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);

	return hash;
}

void DockTables::hashDefaultLayout(const DefaultDockLayout& layout, MD5Digest& md5)
{
	u32 layout_name_size = static_cast<u32>(layout.name.size());
	md5.Update(&layout_name_size, sizeof(layout_name_size));
	md5.Update(layout.name.data(), layout_name_size);

	const char* cpu_name = DebugInterface::cpuName(layout.cpu);
	u32 cpu_name_size = static_cast<u32>(strlen(cpu_name));
	md5.Update(&cpu_name_size, sizeof(cpu_name_size));
	md5.Update(cpu_name, cpu_name_size);

	u32 group_count = static_cast<u32>(layout.groups.size());
	md5.Update(&group_count, sizeof(group_count));

	for (const DefaultDockGroupDescription& group : layout.groups)
		hashDefaultGroup(group, md5);

	u32 widget_count = static_cast<u32>(layout.widgets.size());
	md5.Update(&widget_count, sizeof(widget_count));

	for (const DefaultDockWidgetDescription& widget : layout.widgets)
		hashDefaultDockWidget(widget, md5);

	u32 toolbar_count = static_cast<u32>(layout.toolbars.size());
	md5.Update(&toolbar_count, sizeof(toolbar_count));
	for (const std::string& toolbar : layout.toolbars)
	{
		u32 toolbar_size = toolbar.size();
		md5.Update(&toolbar_size, sizeof(toolbar_size));
		md5.Update(toolbar.data(), toolbar.size());
	}
}

void DockTables::hashDefaultGroup(const DefaultDockGroupDescription& group, MD5Digest& md5)
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

	u32 location_size = static_cast<u32>(strlen(location));
	md5.Update(&location_size, sizeof(location_size));
	md5.Update(location, location_size);

	u32 parent = static_cast<u32>(group.parent);
	md5.Update(&parent, sizeof(parent));
}

void DockTables::hashDefaultDockWidget(const DefaultDockWidgetDescription& widget, MD5Digest& md5)
{
	u32 type_size = static_cast<u32>(widget.type.size());
	md5.Update(&type_size, sizeof(type_size));
	md5.Update(widget.type.data(), type_size);

	u32 group = static_cast<u32>(widget.group);
	md5.Update(&group, sizeof(group));
}
