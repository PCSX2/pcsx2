// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockManager.h"

#include "DebuggerWindow.h"
#include "DisassemblyWidget.h"
#include "RegisterWidget.h"
#include "StackWidget.h"
#include "ThreadWidget.h"
#include "Breakpoints/BreakpointWidget.h"
#include "Memory/MemorySearchWidget.h"
#include "Memory/MemoryViewWidget.h"
#include "Memory/SavedAddressesWidget.h"
#include "SymbolTree/SymbolTreeWidgets.h"

#include <kddockwidgets/Config.h>

#include <QtCore/QTimer>
#include <QtCore/QtTranslation>

#define FOR_EACH_DEBUGGER_DOCK_WIDGET \
	/* Top right. */ \
	X(DisassemblyWidget, QT_TRANSLATE_NOOP("DockWidget", "Disassembly"), OnRight, Root) \
	/* Bottom. */ \
	X(MemoryViewWidget, QT_TRANSLATE_NOOP("DockWidget", "Memory"), OnBottom, DisassemblyWidget) \
	X(BreakpointWidget, QT_TRANSLATE_NOOP("DockWidget", "Breakpoints"), None, MemoryViewWidget) \
	X(ThreadWidget, QT_TRANSLATE_NOOP("DockWidget", "Threads"), None, MemoryViewWidget) \
	X(StackWidget, QT_TRANSLATE_NOOP("DockWidget", "Stack"), None, MemoryViewWidget) \
	X(SavedAddressesWidget, QT_TRANSLATE_NOOP("DockWidget", "Saved Addresses"), None, MemoryViewWidget) \
	X(GlobalVariableTreeWidget, QT_TRANSLATE_NOOP("DockWidget", "Globals"), None, MemoryViewWidget) \
	X(LocalVariableTreeWidget, QT_TRANSLATE_NOOP("DockWidget", "Locals"), None, MemoryViewWidget) \
	X(ParameterVariableTreeWidget, QT_TRANSLATE_NOOP("DockWidget", "Parameters"), None, MemoryViewWidget) \
	/* Top left. */ \
	X(RegisterWidget, QT_TRANSLATE_NOOP("DockWidget", "Registers"), OnLeft, DisassemblyWidget) \
	X(FunctionTreeWidget, QT_TRANSLATE_NOOP("DockWidget", "Functions"), None, RegisterWidget) \
	X(MemorySearchWidget, QT_TRANSLATE_NOOP("DockWidget", "Memory Search"), None, RegisterWidget)

DockManager::DockManager(DebuggerWindow* window)
	: m_window(window)
{
	createDefaultLayout("R5900", r5900Debug);
	//createDefaultLayout("R3000", r3000Debug);
	loadLayouts();
}

void DockManager::configure_docking_system()
{
	KDDockWidgets::Config::self().setFlags(
		KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible |
		KDDockWidgets::Config::Flag_AlwaysShowTabs |
		KDDockWidgets::Config::Flag_AllowReorderTabs |
		KDDockWidgets::Config::Flag_TabsHaveCloseButton |
		KDDockWidgets::Config::Flag_TitleBarIsFocusable);
}

const std::vector<DockManager::Layout>& DockManager::layouts()
{
	return m_layouts;
}

void DockManager::switchToLayout(size_t layout)
{
	//m_layouts.at(m_current_layout).dock_manager->setParent(nullptr);
	//m_window->setCentralWidget(m_layouts.at(layout).dock_manager);
	//m_current_layout = layout;
}

size_t DockManager::cloneLayout(size_t existing_layout, std::string new_name)
{
	return 0;
}

bool DockManager::deleteLayout(size_t layout)
{
	return false;
}

void DockManager::loadLayouts()
{
}

void DockManager::saveLayouts()
{
}

size_t DockManager::createDefaultLayout(const char* name, DebugInterface& cpu)
{
	size_t index = m_layouts.size();

	Layout& layout = m_layouts.emplace_back();
	layout.name = name;
	layout.cpu = cpu.getCpuType();
	layout.user_defined = false;

	KDDockWidgets::QtWidgets::DockWidget* dock_Root = nullptr;
#define X(Type, title, Location, Parent) \
	KDDockWidgets::QtWidgets::DockWidget* dock_##Type = new KDDockWidgets::QtWidgets::DockWidget(title); \
	dock_##Type->setWidget(new Type(cpu)); \
	if (KDDockWidgets::Location_##Location != KDDockWidgets::Location_None) \
		m_window->addDockWidget(dock_##Type, KDDockWidgets::Location_##Location, dock_##Parent); \
	else \
		dock_##Parent->addDockWidgetAsTab(dock_##Type);
	FOR_EACH_DEBUGGER_DOCK_WIDGET
#undef X

	return index;
}
