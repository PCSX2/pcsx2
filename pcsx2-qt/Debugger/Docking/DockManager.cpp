// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockManager.h"

#include "Debugger/DebuggerWindow.h"
#include "Debugger/DisassemblyWidget.h"
#include "Debugger/JsonValueWrapper.h"
#include "Debugger/RegisterWidget.h"
#include "Debugger/StackWidget.h"
#include "Debugger/ThreadWidget.h"
#include "Debugger/Breakpoints/BreakpointWidget.h"
#include "Debugger/Docking/DockViews.h"
#include "Debugger/Docking/LayoutEditorDialog.h"
#include "Debugger/Memory/MemorySearchWidget.h"
#include "Debugger/Memory/MemoryViewWidget.h"
#include "Debugger/Memory/SavedAddressesWidget.h"
#include "Debugger/SymbolTree/SymbolTreeWidgets.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#include <QtCore/QTimer>
#include <QtCore/QtTranslation>
#include <QtWidgets/QMessageBox>

// Independent of the KDDockWidgets file format version number.
const u32 DEBUGGER_LAYOUT_FILE_VERSION = 1;

struct DebuggerWidgetDescription
{
	DebuggerWidget* (*create_widget)(DebugInterface& cpu);
	const char* title;
};

#define DEBUGGER_WIDGET(type, title) \
	{ \
		#type, \
		{ \
			[](DebugInterface& cpu) -> DebuggerWidget* { return new type(cpu); }, \
				title \
		} \
	}

static const std::map<QString, DebuggerWidgetDescription> DEBUGGER_WIDGETS = {
	DEBUGGER_WIDGET(BreakpointWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Breakpoints")),
	DEBUGGER_WIDGET(DisassemblyWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Disassembly")),
	DEBUGGER_WIDGET(FunctionTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Functions")),
	DEBUGGER_WIDGET(GlobalVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Globals")),
	DEBUGGER_WIDGET(LocalVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Locals")),
	DEBUGGER_WIDGET(MemorySearchWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Memory Search")),
	DEBUGGER_WIDGET(MemoryViewWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Memory")),
	DEBUGGER_WIDGET(ParameterVariableTreeWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Parameters")),
	DEBUGGER_WIDGET(RegisterWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Registers")),
	DEBUGGER_WIDGET(SavedAddressesWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Saved Addresses")),
	DEBUGGER_WIDGET(StackWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Stack")),
	DEBUGGER_WIDGET(ThreadWidget, QT_TRANSLATE_NOOP("DebuggerWidget", "Threads")),
};

#undef DEBUGGER_WIDGET

enum DefaultDockGroup
{
	ROOT = -1,
	TOP_RIGHT = 0,
	BOTTOM = 1,
	TOP_LEFT = 2,
	COUNT = 3
};

struct DefaultDockGroupDescription
{
	KDDockWidgets::Location location;
	DefaultDockGroup parent;
};

static const std::vector<DefaultDockGroupDescription> DEFAULT_DOCK_GROUPS = {
	/* [DefaultDockGroup::TOP_RIGHT] = */ {KDDockWidgets::Location_OnRight, DefaultDockGroup::ROOT},
	/* [DefaultDockGroup::BOTTOM]    = */ {KDDockWidgets::Location_OnBottom, DefaultDockGroup::TOP_RIGHT},
	/* [DefaultDockGroup::TOP_LEFT]  = */ {KDDockWidgets::Location_OnLeft, DefaultDockGroup::TOP_RIGHT},
};

struct DefaultDockWidgetDescription
{
	QString type;
	DefaultDockGroup group;
};

static const std::vector<DefaultDockWidgetDescription> DEFAULT_DOCK_WIDGETS = {
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
};

DockManager::DockManager(DebuggerWindow* window)
	: QObject(window)
	, m_window(window)
{
	KDDockWidgets::Config::self().setViewFactory(new DockViewFactory(this));

	loadLayouts();
}

DockManager::~DockManager()
{
	saveLayout(m_current_layout);

	for (Layout& layout : m_layouts)
		for (auto& [unique_name, widget] : layout.widgets)
			delete widget;
}

void DockManager::configureDockingSystem()
{
	KDDockWidgets::Config::self().setFlags(
		KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible |
		KDDockWidgets::Config::Flag_AlwaysShowTabs |
		KDDockWidgets::Config::Flag_AllowReorderTabs |
		KDDockWidgets::Config::Flag_TabsHaveCloseButton |
		KDDockWidgets::Config::Flag_TitleBarIsFocusable);
}

DockManager::LayoutIndex DockManager::createLayout(std::string name, BreakPointCpu cpu, LayoutCreationMode mode)
{
	LayoutIndex layout_index = m_layouts.size();

	Layout& layout = m_layouts.emplace_back();
	layout.name = std::move(name);
	layout.cpu = cpu;

	DebugInterface& debug_interface = r5900Debug;
	if (cpu == BREAKPOINT_IOP)
		debug_interface = r3000Debug;

	switch (mode)
	{
		case DEFAULT_LAYOUT:
		{
			for (size_t i = 0; i < DEFAULT_DOCK_WIDGETS.size(); i++)
			{
				auto iterator = DEBUGGER_WIDGETS.find(DEFAULT_DOCK_WIDGETS[i].type);
				pxAssertRel(iterator != DEBUGGER_WIDGETS.end(), "Invalid default layout.");
				const DebuggerWidgetDescription& dock_description = iterator->second;

				DebuggerWidget* widget = dock_description.create_widget(debug_interface);
				layout.widgets.emplace(DEFAULT_DOCK_WIDGETS[i].type, widget);
			}
			break;
		}
		case CLONE_LAYOUT:
		{
			// TODO
			break;
		}
		case BLANK_LAYOUT:
		{
			// Nothing to do.
			break;
		}
	}

	return layout_index;
}

bool DockManager::deleteLayout(LayoutIndex layout_index)
{
	if (layout_index == m_current_layout)
	{
		LayoutIndex other_layout = INVALID_LAYOUT;
		if (layout_index + 1 <= m_layouts.size())
			other_layout = layout_index + 1;
		else if (layout_index > 0)
			other_layout = layout_index - 1;

		switchToLayout(other_layout);
	}

	Layout& layout = m_layouts.at(layout_index);
	for (auto& [unique_name, widget] : layout.widgets)
		delete widget;

	m_layouts.erase(m_layouts.begin() + layout_index);

	if (m_current_layout > layout_index)
		m_current_layout--;

	return true;
}

void DockManager::switchToLayout(LayoutIndex layout_index)
{
	if (layout_index == m_current_layout)
		return;

	if (m_current_layout != INVALID_LAYOUT)
	{
		freezeCurrentLayout();
		saveLayout(m_current_layout);
	}

	m_current_layout = layout_index;

	if (m_current_layout != INVALID_LAYOUT)
	{
		thawCurrentLayout();
	}
}

void DockManager::loadLayouts()
{
	m_layouts.clear();

	// Load the layouts.
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(
		EmuFolders::DebuggerLayouts.c_str(),
		"*.json",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);

	for (const FILESYSTEM_FIND_DATA& ffd : files)
		loadLayout(ffd.FileName);

	if (m_layouts.empty())
		setupDefaultLayouts();
}

DockManager::LayoutIndex DockManager::loadLayout(const std::string& path)
{
	LayoutIndex layout_index = 0; // = createLayout(name);

	return layout_index;
}

bool DockManager::saveLayouts()
{
	for (s32 i = 0; i < static_cast<s32>(m_layouts.size()); i++)
		if (!saveLayout(i))
			return false;

	return true;
}

bool DockManager::saveLayout(LayoutIndex layout_index)
{
	Layout& layout = m_layouts.at(layout_index);

	// Serialize the layout as JSON.
	rapidjson::Document json(rapidjson::kObjectType);
	rapidjson::Document geometry;

	json.AddMember("format", "PCSX2 Debugger User Interface Layout", json.GetAllocator());
	json.AddMember("version", DEBUGGER_LAYOUT_FILE_VERSION, json.GetAllocator());

	rapidjson::Value name;
	name.SetString(layout.name.c_str(), strlen(layout.name.c_str()));
	json.AddMember("name", name, json.GetAllocator());

	rapidjson::Value widgets(rapidjson::kArrayType);
	for (auto& [unique_name, widget] : layout.widgets)
	{
		rapidjson::Value object(rapidjson::kObjectType);

		JsonValueWrapper wrapper(object, json.GetAllocator());
		widget->toJson(wrapper);

		widgets.PushBack(object, json.GetAllocator());
	}
	json.AddMember("widgets", widgets, json.GetAllocator());

	if (!layout.geometry.isEmpty() && !geometry.Parse(layout.geometry).HasParseError())
		json.AddMember("geometry", geometry, json.GetAllocator());

	rapidjson::StringBuffer string_buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);
	json.Accept(writer);

	// Write out the JSON to a file.
	std::string temp_file_path = Path::Combine(EmuFolders::DebuggerLayouts, layout.name + ".tmp");

	if (!FileSystem::WriteStringToFile(temp_file_path.c_str(), string_buffer.GetString()))
		return false;

	// Generate a name if a file doesn't already exist.
	if (layout.layout_file_path.empty())
		layout.layout_file_path = Path::Combine(EmuFolders::DebuggerLayouts, layout.name + ".json");

	return FileSystem::RenamePath(temp_file_path.c_str(), layout.layout_file_path.c_str());
}

void DockManager::renameLayout(LayoutIndex layout_index, std::string new_name)
{
	static_cast<void>(new_name);
}

void DockManager::setupDefaultLayouts()
{
	switchToLayout(INVALID_LAYOUT);

	m_layouts.clear();

	createLayout("R5900", BREAKPOINT_EE, DEFAULT_LAYOUT);
	createLayout("R3000", BREAKPOINT_IOP, DEFAULT_LAYOUT);

	switchToLayout(0);
}

void DockManager::createWindowsMenu(QMenu* menu)
{
	menu->clear();

	QAction* reset_all_layouts_action = new QAction(tr("Reset All Layouts"), menu);
	connect(reset_all_layouts_action, &QAction::triggered, [this]() {
		QMessageBox::StandardButton result = QMessageBox::question(
			m_window, tr("Confirmation"), tr("Are you sure you want to reset all layouts?"));

		if (result == QMessageBox::Yes)
			setupDefaultLayouts();
	});
	menu->addAction(reset_all_layouts_action);

	menu->addSeparator();

	for (const auto& [type, desc] : DEBUGGER_WIDGETS)
	{
		QAction* action = new QAction(menu);
		action->setText(QCoreApplication::translate("DockWidget", desc.title));
		action->setCheckable(true);
		action->setChecked(true);
		menu->addAction(action);
	}
}

QWidget* DockManager::createLayoutSwitcher(QWidget* menu_bar)
{
	QWidget* container = new QWidget;
	QHBoxLayout* layout = new QHBoxLayout;
	container->setLayout(layout);

	layout->setContentsMargins(0, 2, 0, 0);

	QWidget* menu_wrapper = new QWidget;
	menu_wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	layout->addWidget(menu_wrapper);

	QHBoxLayout* menu_layout = new QHBoxLayout;
	menu_layout->setContentsMargins(0, 4, 0, 4);
	menu_wrapper->setLayout(menu_layout);

	menu_layout->addWidget(menu_bar);

	m_switcher = new QTabBar;
	m_switcher->setContentsMargins(0, 0, 0, 0);
	m_switcher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	m_switcher->setContextMenuPolicy(Qt::CustomContextMenu);
	m_switcher->setMovable(true);
	layout->addWidget(m_switcher);

	updateLayoutSwitcher();

	connect(m_switcher, &QTabBar::tabMoved, this, &DockManager::layoutSwitcherTabMoved);
	connect(m_switcher, &QTabBar::customContextMenuRequested, this, &DockManager::layoutSwitcherContextMenu);

	QWidget* spacer = new QWidget;
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	return container;
}

void DockManager::updateLayoutSwitcher()
{
	if (!m_switcher)
		return;

	disconnect(m_tab_connection);

	for (int i = m_switcher->count(); i > 0; i--)
		m_switcher->removeTab(i - 1);

	for (Layout& layout : m_layouts)
	{
		const char* cpu_name = "EE";
		if (layout.cpu == BREAKPOINT_IOP)
			cpu_name = "IOP";

		layout.switcher_tab_index = m_switcher->addTab(QString("%1 (%2)").arg(layout.name.c_str()).arg(cpu_name));
	}

	m_plus_tab_index = m_switcher->addTab("+");
	m_current_tab_index = m_current_layout;

	m_switcher->setCurrentIndex(m_current_layout);

	m_tab_connection = connect(m_switcher, &QTabBar::currentChanged, this, &DockManager::layoutSwitcherTabChanged);
}

void DockManager::layoutSwitcherTabChanged(s32 index)
{
	if (index == m_plus_tab_index)
	{
		if (m_current_tab_index >= 0 && m_current_tab_index < m_plus_tab_index)
			m_switcher->setCurrentIndex(m_current_tab_index);

		LayoutEditorDialog* dialog = new LayoutEditorDialog(m_window);
		if (dialog->exec() == QDialog::Accepted)
		{
			s32 layout_index = createLayout(dialog->name(), dialog->cpu(), dialog->initial_state());
			switchToLayout(layout_index);
			updateLayoutSwitcher();
		}
	}
	else
	{
		switchToLayout(index);
		m_current_tab_index = index;
	}
}

void DockManager::layoutSwitcherTabMoved(s32 from, s32 to)
{
	updateLayoutSwitcher();
}

void DockManager::layoutSwitcherContextMenu(QPoint pos)
{
	int layout_index = m_switcher->tabAt(pos);
	if (layout_index < 0 || layout_index >= m_plus_tab_index)
		return;

	QMenu* menu = new QMenu(tr("Layout Switcher Context Menu"), m_switcher);

	QAction* edit_action = new QAction(tr("Edit Layout"), menu);
	connect(edit_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index >= static_cast<s32>(m_layouts.size()))
			return;

		Layout& layout = m_layouts[layout_index];

		LayoutEditorDialog* dialog = new LayoutEditorDialog(layout.name, layout.cpu, m_window);

		if (dialog->exec() == QDialog::Accepted)
		{
			layout.name = dialog->name();
			layout.cpu = dialog->cpu();

			for (auto [unique_name, widget] : layout.widgets)
				if (!widget->setCpu(&DebugInterface::get(layout.cpu)))
					recreateDebuggerWidget(unique_name);

			updateLayoutSwitcher();
		}
	});
	menu->addAction(edit_action);

	QAction* delete_action = new QAction(tr("Delete Layout"), menu);
	connect(delete_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index >= static_cast<s32>(m_layouts.size()))
			return;

		deleteLayout(static_cast<LayoutIndex>(layout_index));
		updateLayoutSwitcher();
	});
	menu->addAction(delete_action);

	menu->popup(m_switcher->mapToGlobal(pos));
}

void DockManager::retranslateAll()
{
	if (m_current_layout == INVALID_LAYOUT)
		return;

	for (KDDockWidgets::Core::DockWidget* widget : KDDockWidgets::DockRegistry::self()->dockwidgets())
		retranslateDockWidget(widget);
}

void DockManager::retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget)
{
	Layout& layout = m_layouts.at(m_current_layout);

	auto widget_iterator = layout.widgets.find(dock_widget->uniqueName());
	if (widget_iterator == layout.widgets.end())
		return;

	DebuggerWidget* widget = widget_iterator->second.get();
	if (!widget)
		return;

	auto description_iterator = DEBUGGER_WIDGETS.find(dock_widget->uniqueName());
	if (description_iterator == DEBUGGER_WIDGETS.end())
		return;

	const DebuggerWidgetDescription& description = description_iterator->second;

	QString translated_title = QCoreApplication::translate("DebuggerWidget", description.title);
	std::optional<BreakPointCpu> cpu_override = widget->cpuOverride();

	if (cpu_override.has_value())
	{
		const char* cpu_name = DebugInterface::cpuName(*cpu_override);
		dock_widget->setTitle(QString("%1 (%2)").arg(translated_title).arg(cpu_name));
	}
	else
	{
		dock_widget->setTitle(std::move(translated_title));
	}
}

void DockManager::recreateDebuggerWidget(QString unique_name)
{
	if (m_current_layout == INVALID_LAYOUT)
		return;

	Layout& layout = m_layouts.at(m_current_layout);

	auto debugger_widget_iterator = layout.widgets.find(unique_name);
	if (debugger_widget_iterator == layout.widgets.end())
		return;

	KDDockWidgets::Vector<QString> names{unique_name};
	KDDockWidgets::Vector<KDDockWidgets::Core::DockWidget*> dock_widgets =
		KDDockWidgets::DockRegistry::self()->dockWidgets(names);
	if (dock_widgets.size() != 1)
		return;

	auto description_iterator = DEBUGGER_WIDGETS.find(unique_name);
	if (description_iterator == DEBUGGER_WIDGETS.end())
		return;

	DebuggerWidget* old_debugger_widget = debugger_widget_iterator->second;
	KDDockWidgets::Core::DockWidget* dock_controller = dock_widgets[0];
	const DebuggerWidgetDescription& description = description_iterator->second;

	KDDockWidgets::QtWidgets::DockWidget* dock_view =
		dynamic_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock_controller->view());
	pxAssertRel(dock_view->widget() == old_debugger_widget, "Dock widget ownership confusion error.");

	DebuggerWidget* new_debugger_widget = description.create_widget(DebugInterface::get(layout.cpu));
	new_debugger_widget->setCpuOverride(old_debugger_widget->cpuOverride());
	debugger_widget_iterator->second = new_debugger_widget;

	dock_view->setWidget(new_debugger_widget);

	delete old_debugger_widget;
}

void DockManager::freezeCurrentLayout()
{
	Layout& layout = m_layouts.at(m_current_layout);

	pxAssertRel(!layout.is_frozen, "DockManager::freezeLayout called on already frozen layout.");
	layout.is_frozen = true;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	// Store the geometry of all the dock widgets as JSON.
	layout.geometry = saver.serializeLayout();

	// Delete the dock widgets.
	for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
	{
		// Make sure the dock widget releases ownership of its content.
		KDDockWidgets::QtWidgets::DockWidget* view =
			static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
		view->setWidget(new QWidget());

		delete dock;
	}
}

void DockManager::thawCurrentLayout()
{
	Layout& layout = m_layouts.at(m_current_layout);

	pxAssertRel(layout.is_frozen, "DockManager::thawLayout called on already thawed layout.");
	layout.is_frozen = false;

	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);

	if (layout.geometry.isEmpty())
	{
		// This is a newly created layout with no geometry information.
		populateDefaultLayout();
		retranslateAll();
		return;
	}

	// Create any dock widgets that were previously frozen during this session.
	for (auto& [unique_name, widget] : layout.widgets)
	{
		KDDockWidgets::QtWidgets::DockWidget* view = new KDDockWidgets::QtWidgets::DockWidget(unique_name);
		view->setWidget(widget);
		m_window->addDockWidget(view, KDDockWidgets::Location_OnBottom);
	}

	// Restore the geometry of the dock widgets we just recreated.
	if (!saver.restoreLayout(layout.geometry))
	{
		for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
		{
			// Make sure the dock widget releases ownership of its content.
			KDDockWidgets::QtWidgets::DockWidget* view =
				static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
			view->setWidget(new QWidget());

			delete dock;
		}

		// We failed to restore the geometry, so just setup the default layout.
		populateDefaultLayout();
	}

	retranslateAll();
}

void DockManager::populateDefaultLayout()
{
	Layout& layout = m_layouts.at(m_current_layout);

	KDDockWidgets::QtWidgets::DockWidget* groups[DefaultDockGroup::COUNT] = {};

	for (const DefaultDockWidgetDescription& dock_description : DEFAULT_DOCK_WIDGETS)
	{
		const DefaultDockGroupDescription& group = DEFAULT_DOCK_GROUPS[dock_description.group];

		auto widget_iterator = layout.widgets.find(dock_description.type);
		if (widget_iterator == layout.widgets.end())
			continue;

		const QString& unique_name = widget_iterator->first;
		DebuggerWidget* widget = widget_iterator->second;

		KDDockWidgets::QtWidgets::DockWidget* dock = new KDDockWidgets::QtWidgets::DockWidget(unique_name);
		dock->setWidget(widget);

		if (!groups[dock_description.group])
		{
			KDDockWidgets::QtWidgets::DockWidget* parent = nullptr;
			if (group.parent != DefaultDockGroup::ROOT)
				parent = groups[group.parent];

			m_window->addDockWidget(dock, group.location, parent);

			groups[dock_description.group] = dock;
		}
		else
		{
			groups[dock_description.group]->addDockWidgetAsTab(dock);
		}
	}

	for (KDDockWidgets::Core::Group* group : KDDockWidgets::DockRegistry::self()->groups())
		group->setCurrentTabIndex(0);
}
