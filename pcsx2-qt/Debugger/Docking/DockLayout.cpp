// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockLayout.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/DebuggerWindow.h"
#include "Debugger/JsonValueWrapper.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/Layout.h>
#include <kddockwidgets/core/ViewFactory.h>
#include <kddockwidgets/qtwidgets/Group.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

const char* DEBUGGER_LAYOUT_FILE_FORMAT = "PCSX2 Debugger User Interface Layout";

// Increment this whenever the JSON format changes (excluding the contents of
// the geometry object which is managed by KDDockWidgets).
const u32 DEBUGGER_LAYOUT_FILE_VERSION_MAJOR = 1;

// Increment this whenever you want all the default layouts to be reset. If you
// modify the DockTables::DEFAULT_DOCK_LAYOUTS table you won't need to increment
// this as they will be reset automatically.
const u32 DEBUGGER_LAYOUT_FILE_VERSION_MINOR = 0;

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	const DockTables::DefaultDockLayout& default_layout,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
	, m_base_layout(default_layout.name)
{
	for (size_t i = 0; i < default_layout.widgets.size(); i++)
	{
		auto iterator = DockTables::DEBUGGER_WIDGETS.find(QString::fromStdString(default_layout.widgets[i].type));
		pxAssertRel(iterator != DockTables::DEBUGGER_WIDGETS.end(), "Invalid default layout.");
		const DockTables::DebuggerWidgetDescription& dock_description = iterator->second;

		DebuggerWidgetParameters parameters;
		parameters.cpu = &DebugInterface::get(cpu);
		DebuggerWidget* widget = dock_description.create_widget(parameters);
		m_widgets.emplace(QString::fromStdString(default_layout.widgets[i].type), widget);
	}

	save(index);
}

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
{
	save(index);
}

DockLayout::DockLayout(
	std::string name,
	BreakPointCpu cpu,
	bool is_default,
	const DockLayout& layout_to_clone,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
	, m_base_layout(layout_to_clone.m_base_layout)
	, m_toolbars(layout_to_clone.m_toolbars)
	, m_geometry(layout_to_clone.m_geometry)
{
	for (const auto& [unique_name, widget_to_clone] : layout_to_clone.m_widgets)
	{
		auto widget_description = DockTables::DEBUGGER_WIDGETS.find(widget_to_clone->metaObject()->className());
		if (widget_description == DockTables::DEBUGGER_WIDGETS.end())
			continue;

		DebuggerWidgetParameters parameters;
		parameters.cpu = &DebugInterface::get(cpu);
		parameters.cpu_override = widget_to_clone->cpuOverride();
		DebuggerWidget* new_widget = widget_description->second.create_widget(parameters);
		m_widgets.emplace(unique_name, new_widget);
	}

	save(index);
}

DockLayout::DockLayout(
	const std::string& path,
	DockLayout::LoadResult& result,
	DockLayout::Index& index_last_session,
	DockLayout::Index index)
{
	load(path, result, index_last_session);
}

DockLayout::~DockLayout()
{
	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		delete widget;
	}
}

const std::string& DockLayout::name() const
{
	return m_name;
}

void DockLayout::setName(std::string name)
{
	m_name = std::move(name);
}

BreakPointCpu DockLayout::cpu() const
{
	return m_cpu;
}

bool DockLayout::isDefault() const
{
	return m_is_default;
}

void DockLayout::setCpu(BreakPointCpu cpu)
{
	m_cpu = cpu;

	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		if (!widget->setCpu(DebugInterface::get(cpu)))
			recreateDebuggerWidget(unique_name);
	}
}

void DockLayout::freeze()
{
	pxAssert(!m_is_frozen);
	m_is_frozen = true;

	if (g_debugger_window)
		m_toolbars = g_debugger_window->saveState();

	// Store the geometry of all the dock widgets as JSON.
	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
	m_geometry = saver.serializeLayout();

	// Delete the dock widgets.
	for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
	{
		// Make sure the dock widget releases ownership of its content.
		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
		view->setWidget(new QWidget());

		delete dock;
	}
}

void DockLayout::thaw()
{
	pxAssert(m_is_frozen);
	m_is_frozen = false;

	if (!g_debugger_window)
		return;

	// Restore the state of the toolbars.
	if (m_toolbars.isEmpty())
	{
		const DockTables::DefaultDockLayout* base_layout = DockTables::defaultLayout(m_base_layout);
		if (base_layout)
		{
			for (QToolBar* toolbar : g_debugger_window->findChildren<QToolBar*>())
				if (base_layout->toolbars.contains(toolbar->objectName().toStdString()))
					toolbar->show();
		}
	}
	else
	{
		g_debugger_window->restoreState(m_toolbars);
	}

	if (m_geometry.isEmpty())
	{
		// This is a newly created layout with no geometry information.
		setupDefaultLayout();
		return;
	}

	// Restore the geometry of the dock widgets we just recreated.
	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
	if (!saver.restoreLayout(m_geometry))
	{
		// We've failed to restore the geometry, so just tear down whatever dock
		// widgets may exist and then setup the default layout.
		for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
		{
			// Make sure the dock widget releases ownership of its content.
			auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
			view->setWidget(new QWidget());

			delete dock;
		}

		setupDefaultLayout();
		return;
	}

	// Check that all the dock widgets have been restored correctly.
	std::vector<QString> orphaned_debugger_widgets;
	for (auto& [unique_name, widget] : m_widgets)
	{
		auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);
		if (!controller || !view)
		{
			Console.Error("Debugger: Failed to restore dock widget '%s'.", unique_name.toStdString().c_str());
			orphaned_debugger_widgets.emplace_back(unique_name);
		}
	}

	// Delete any debugger widgets that haven't been restored correctly.
	for (const QString& unique_name : orphaned_debugger_widgets)
	{
		auto widget_iterator = m_widgets.find(unique_name);
		delete widget_iterator->second.get();
		m_widgets.erase(widget_iterator);
	}

	retranslateDockWidgets();
}

KDDockWidgets::Core::DockWidget* DockLayout::createDockWidget(const QString& name)
{
	pxAssert(!m_is_frozen);
	pxAssert(KDDockWidgets::LayoutSaver::restoreInProgress());

	auto widget_iterator = m_widgets.find(name);
	if (widget_iterator == m_widgets.end())
		return nullptr;

	DebuggerWidget* widget = widget_iterator->second;
	pxAssert(widget);

	auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
		KDDockWidgets::Config::self().viewFactory()->createDockWidget(name));
	view->setWidget(widget);

	return view->asController<KDDockWidgets::Core::DockWidget>();
}

void DockLayout::retranslateDockWidgets()
{
	for (KDDockWidgets::Core::DockWidget* widget : KDDockWidgets::DockRegistry::self()->dockwidgets())
		retranslateDockWidget(widget);
}

void DockLayout::retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget)
{
	pxAssert(!m_is_frozen);

	auto widget_iterator = m_widgets.find(dock_widget->uniqueName());
	if (widget_iterator == m_widgets.end())
		return;

	DebuggerWidget* widget = widget_iterator->second.get();
	if (!widget)
		return;
	;
	std::optional<BreakPointCpu> cpu_override = widget->cpuOverride();

	if (cpu_override.has_value())
	{
		const char* cpu_name = DebugInterface::cpuName(*cpu_override);
		dock_widget->setTitle(QString("%1 (%2)").arg(widget->displayName()).arg(cpu_name));
	}
	else
	{
		dock_widget->setTitle(std::move(widget->displayName()));
	}
}

void DockLayout::dockWidgetClosed(KDDockWidgets::Core::DockWidget* dock_widget)
{
	// The LayoutSaver class will close a bunch of dock widgets. We only want to
	// delete the dock widgets when they're being closed by the user.
	if (KDDockWidgets::LayoutSaver::restoreInProgress())
		return;

	auto debugger_widget_iterator = m_widgets.find(dock_widget->uniqueName());
	if (debugger_widget_iterator == m_widgets.end())
		return;

	m_widgets.erase(debugger_widget_iterator);
	dock_widget->deleteLater();
}

const std::map<QString, QPointer<DebuggerWidget>>& DockLayout::debuggerWidgets()
{
	return m_widgets;
}

bool DockLayout::hasDebuggerWidget(QString unique_name)
{
	return m_widgets.find(unique_name) != m_widgets.end();
}

void DockLayout::toggleDebuggerWidget(QString unique_name)
{
	pxAssert(!m_is_frozen);

	if (!g_debugger_window)
		return;

	auto debugger_widget_iterator = m_widgets.find(unique_name);
	auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);

	if (debugger_widget_iterator == m_widgets.end())
	{
		// Create the dock widget.
		if (controller)
			return;

		auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(unique_name);
		if (description_iterator == DockTables::DEBUGGER_WIDGETS.end())
			return;

		const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

		DebuggerWidgetParameters parameters;
		parameters.cpu = &DebugInterface::get(m_cpu);
		DebuggerWidget* widget = description.create_widget(parameters);
		m_widgets.emplace(unique_name, widget);

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(unique_name));
		view->setWidget(widget);

		KDDockWidgets::Core::DockWidget* controller = view->asController<KDDockWidgets::Core::DockWidget>();
		if (!controller)
		{
			delete view;
			return;
		}

		DockUtils::insertDockWidgetAtPreferredLocation(controller, description.preferred_location, g_debugger_window);
		retranslateDockWidget(controller);
	}
	else
	{
		// Delete the dock widget.
		if (!controller)
			return;

		m_widgets.erase(debugger_widget_iterator);
		delete controller;
	}
}

void DockLayout::recreateDebuggerWidget(QString unique_name)
{
	pxAssert(!m_is_frozen);

	auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);
	if (!controller || !view)
		return;

	auto debugger_widget_iterator = m_widgets.find(unique_name);
	if (debugger_widget_iterator == m_widgets.end())
		return;

	DebuggerWidget* old_debugger_widget = debugger_widget_iterator->second;
	pxAssert(old_debugger_widget == view->widget());

	auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(old_debugger_widget->metaObject()->className());
	if (description_iterator == DockTables::DEBUGGER_WIDGETS.end())
		return;

	const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

	DebuggerWidgetParameters parameters;
	parameters.cpu = &DebugInterface::get(m_cpu);
	parameters.cpu_override = old_debugger_widget->cpuOverride();
	DebuggerWidget* new_debugger_widget = description.create_widget(parameters);
	debugger_widget_iterator->second = new_debugger_widget;

	view->setWidget(new_debugger_widget);

	delete old_debugger_widget;
}

void DockLayout::deleteFile()
{
	if (m_layout_file_path.empty())
		return;

	if (!FileSystem::DeleteFilePath(m_layout_file_path.c_str()))
		Console.Error("Debugger: Failed to delete layout file '%s'.", m_layout_file_path.c_str());
}
bool DockLayout::save(DockLayout::Index layout_index)
{
	if (!g_debugger_window)
		return false;

	if (!m_is_frozen)
	{
		m_toolbars = g_debugger_window->saveState();

		// Store the geometry of all the dock widgets as JSON.
		KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
		m_geometry = saver.serializeLayout();
	}

	// Serialize the layout as JSON.
	rapidjson::Document json(rapidjson::kObjectType);
	rapidjson::Document geometry;

	const char* cpu_name = DebugInterface::cpuName(m_cpu);
	const std::string& default_layouts_hash = DockTables::hashDefaultLayouts();

	rapidjson::Value format;
	format.SetString(DEBUGGER_LAYOUT_FILE_FORMAT, strlen(DEBUGGER_LAYOUT_FILE_FORMAT));
	json.AddMember("format", format, json.GetAllocator());

	json.AddMember("version_major", DEBUGGER_LAYOUT_FILE_VERSION_MAJOR, json.GetAllocator());
	json.AddMember("version_minor", DEBUGGER_LAYOUT_FILE_VERSION_MINOR, json.GetAllocator());
	rapidjson::Value version_hash;
	version_hash.SetString(default_layouts_hash.c_str(), default_layouts_hash.size());
	json.AddMember("version_hash", version_hash, json.GetAllocator());

	json.AddMember("name", rapidjson::Value().SetString(m_name.c_str(), m_name.size()), json.GetAllocator());
	json.AddMember("target", rapidjson::Value().SetString(cpu_name, strlen(cpu_name)), json.GetAllocator());
	json.AddMember("index", static_cast<int>(layout_index), json.GetAllocator());
	json.AddMember("isDefault", m_is_default, json.GetAllocator());

	if (!m_base_layout.empty())
	{
		rapidjson::Value base_layout;
		base_layout.SetString(m_base_layout.c_str(), m_base_layout.size());
		json.AddMember("baseLayout", base_layout, json.GetAllocator());
	}

	if (!m_toolbars.isEmpty())
	{
		std::string toolbars_str = m_toolbars.toBase64().toStdString();
		rapidjson::Value toolbars;
		toolbars.SetString(toolbars_str.data(), toolbars_str.size(), json.GetAllocator());
		json.AddMember("toolbars", toolbars, json.GetAllocator());
	}

	rapidjson::Value widgets(rapidjson::kArrayType);
	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		rapidjson::Value object(rapidjson::kObjectType);

		std::string name_str = unique_name.toStdString();
		rapidjson::Value name;
		name.SetString(name_str.c_str(), name_str.size(), json.GetAllocator());
		object.AddMember("uniqueName", name, json.GetAllocator());

		const char* type_str = widget->metaObject()->className();
		rapidjson::Value type;
		type.SetString(type_str, strlen(type_str), json.GetAllocator());
		object.AddMember("type", type, json.GetAllocator());

		if (widget->cpuOverride().has_value())
		{
			const char* cpu_name = DebugInterface::cpuName(*widget->cpuOverride());

			rapidjson::Value target;
			target.SetString(cpu_name, strlen(cpu_name));
			object.AddMember("target", target, json.GetAllocator());
		}

		JsonValueWrapper wrapper(object, json.GetAllocator());
		widget->toJson(wrapper);

		widgets.PushBack(object, json.GetAllocator());
	}
	json.AddMember("widgets", widgets, json.GetAllocator());

	if (!m_geometry.isEmpty() && !geometry.Parse(m_geometry).HasParseError())
		json.AddMember("geometry", geometry, json.GetAllocator());

	rapidjson::StringBuffer string_buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);
	json.Accept(writer);

	std::string safe_name = Path::SanitizeFileName(m_name);

	// Create a temporary file first so that we don't corrupt an existing file
	// in the case that we succeed in opening the file but fail to write our
	// data to it.
	std::string temp_file_path = Path::Combine(EmuFolders::DebuggerLayouts, safe_name + ".tmp");

	if (!FileSystem::WriteStringToFile(temp_file_path.c_str(), string_buffer.GetString()))
	{
		Console.Error("Debugger: Failed to save temporary layout file '%s'.", temp_file_path.c_str());
		FileSystem::DeleteFilePath(temp_file_path.c_str());
		return false;
	}

	// Now move the layout to its final location.
	std::string file_path = Path::Combine(EmuFolders::DebuggerLayouts, safe_name + ".json");

	if (!FileSystem::RenamePath(temp_file_path.c_str(), file_path.c_str()))
	{
		Console.Error("Debugger: Failed to move layout file to '%s'.", file_path.c_str());
		FileSystem::DeleteFilePath(temp_file_path.c_str());
		return false;
	}

	// If the layout has been renamed we need to delete the old file.
	if (file_path != m_layout_file_path)
		deleteFile();

	m_layout_file_path = std::move(file_path);

	return true;
}

void DockLayout::load(
	const std::string& path,
	LoadResult& result,
	DockLayout::Index& index_last_session)
{
	pxAssert(m_is_frozen);

	result = SUCCESS;

	std::optional<std::string> text = FileSystem::ReadFileToString(path.c_str());
	if (!text.has_value())
	{
		Console.Error("Debugger: Failed to open layout file '%s'.", path.c_str());
		result = FILE_NOT_FOUND;
		return;
	}

	rapidjson::Document json;
	if (json.Parse(text->c_str()).HasParseError() || !json.IsObject())
	{
		Console.Error("Debugger: Failed to parse layout file '%s' as JSON.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	auto format = json.FindMember("format");
	if (format == json.MemberEnd() ||
		!format->value.IsString() ||
		strcmp(format->value.GetString(), DEBUGGER_LAYOUT_FILE_FORMAT) != 0)
	{
		Console.Error("Debugger: Layout file '%s' has missing or invalid 'format' property.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	auto version_major = json.FindMember("version_major");
	if (version_major == json.MemberEnd() || !version_major->value.IsInt())
	{
		Console.Error("Debugger: Layout file '%s' has missing or invalid 'version_major' property.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	if (version_major->value.GetInt() != DEBUGGER_LAYOUT_FILE_VERSION_MAJOR)
	{
		result = MAJOR_VERSION_MISMATCH;
		return;
	}

	auto version_minor = json.FindMember("version_minor");
	if (version_minor == json.MemberEnd() || !version_minor->value.IsInt())
	{
		Console.Error("Debugger: Layout file '%s' has missing or invalid 'version_minor' property.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	if (version_minor->value.GetInt() != DEBUGGER_LAYOUT_FILE_VERSION_MINOR)
		result = MINOR_VERSION_MISMATCH;

	auto version_hash = json.FindMember("version_hash");
	if (version_hash == json.MemberEnd() || !version_hash->value.IsString())
	{
		Console.Error("Debugger: Layout file '%s' has missing or invalid 'version_hash' property.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	if (strcmp(version_hash->value.GetString(), DockTables::hashDefaultLayouts().c_str()) != 0)
		result = MINOR_VERSION_MISMATCH;

	auto name = json.FindMember("name");
	if (name != json.MemberEnd() && name->value.IsString())
		m_name = name->value.GetString();
	else
		m_name = QCoreApplication::translate("DockLayout", "Unnamed").toStdString();

	auto target = json.FindMember("target");
	m_cpu = BREAKPOINT_EE;
	if (target != json.MemberEnd() && target->value.IsString())
	{
		for (BreakPointCpu cpu : DEBUG_CPUS)
			if (strcmp(DebugInterface::cpuName(cpu), target->value.GetString()) == 0)
				m_cpu = cpu;
	}

	auto index = json.FindMember("index");
	if (index != json.MemberEnd() && index->value.IsInt())
		index_last_session = index->value.GetInt();

	auto is_default = json.FindMember("isDefault");
	if (is_default != json.MemberEnd() && is_default->value.IsBool())
		m_is_default = is_default->value.GetBool();

	auto base_layout = json.FindMember("baseLayout");
	if (base_layout != json.MemberEnd() && base_layout->value.IsString())
		m_base_layout = base_layout->value.GetString();

	auto toolbars = json.FindMember("toolbars");
	if (toolbars != json.MemberEnd() && toolbars->value.IsString())
		m_toolbars = QByteArray::fromBase64(toolbars->value.GetString());

	auto widgets = json.FindMember("widgets");
	if (widgets != json.MemberEnd() && widgets->value.IsArray())
	{
		for (rapidjson::Value& object : widgets->value.GetArray())
		{
			auto unique_name = object.FindMember("uniqueName");
			if (unique_name == object.MemberEnd() || !unique_name->value.IsString())
				continue;

			auto type = object.FindMember("type");
			if (type == object.MemberEnd() || !type->value.IsString())
				continue;

			auto description = DockTables::DEBUGGER_WIDGETS.find(type->value.GetString());
			if (description == DockTables::DEBUGGER_WIDGETS.end())
				continue;

			std::optional<BreakPointCpu> cpu_override;

			auto target = object.FindMember("target");
			if (target != object.MemberEnd() && target->value.IsString())
			{
				for (BreakPointCpu cpu : DEBUG_CPUS)
					if (strcmp(DebugInterface::cpuName(cpu), target->value.GetString()) == 0)
						cpu_override = cpu;
			}

			DebuggerWidgetParameters parameters;
			parameters.cpu = &DebugInterface::get(m_cpu);
			parameters.cpu_override = cpu_override;
			DebuggerWidget* widget = description->second.create_widget(parameters);

			JsonValueWrapper wrapper(object, json.GetAllocator());
			if (!widget->fromJson(wrapper))
			{
				delete widget;
				continue;
			}

			m_widgets.emplace(unique_name->value.GetString(), widget);
		}
	}

	auto geometry = json.FindMember("geometry");
	if (geometry != json.MemberEnd() && geometry->value.IsObject())
	{
		rapidjson::StringBuffer string_buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
		geometry->value.Accept(writer);

		m_geometry = QByteArray(string_buffer.GetString(), string_buffer.GetSize());
	}

	m_layout_file_path = path;
}

void DockLayout::setupDefaultLayout()
{
	pxAssert(!m_is_frozen);

	if (m_base_layout.empty() || !g_debugger_window)
		return;

	const DockTables::DefaultDockLayout* base_layout = DockTables::defaultLayout(m_base_layout);
	if (!base_layout)
		return;

	std::vector<KDDockWidgets::QtWidgets::DockWidget*> groups(base_layout->groups.size(), nullptr);

	for (const DockTables::DefaultDockWidgetDescription& dock_description : base_layout->widgets)
	{
		const DockTables::DefaultDockGroupDescription& group =
			base_layout->groups[static_cast<u32>(dock_description.group)];

		auto widget_iterator = m_widgets.find(QString::fromStdString(dock_description.type));
		if (widget_iterator == m_widgets.end())
			continue;

		const QString& unique_name = widget_iterator->first;
		DebuggerWidget* widget = widget_iterator->second;

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(unique_name));
		view->setWidget(widget);

		if (!groups[static_cast<u32>(dock_description.group)])
		{
			KDDockWidgets::QtWidgets::DockWidget* parent = nullptr;
			if (group.parent != DockTables::DefaultDockGroup::ROOT)
				parent = groups[static_cast<u32>(group.parent)];

			g_debugger_window->addDockWidget(view, group.location, parent);

			groups[static_cast<u32>(dock_description.group)] = view;
		}
		else
		{
			groups[static_cast<u32>(dock_description.group)]->addDockWidgetAsTab(view);
		}
	}

	for (KDDockWidgets::Core::Group* group : KDDockWidgets::DockRegistry::self()->groups())
		group->setCurrentTabIndex(0);

	retranslateDockWidgets();
}
