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

// Increment this whenever there is a breaking change to the JSON format.
const u32 DEBUGGER_LAYOUT_FILE_VERSION_MAJOR = 1;

// Increment this whenever there is a non-breaking change to the JSON format.
const u32 DEBUGGER_LAYOUT_FILE_VERSION_MINOR = 0;

DockLayout::DockLayout(
	QString name,
	BreakPointCpu cpu,
	bool is_default,
	const std::string& base_name,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
	, m_base_layout(base_name)
{
	reset();
	save(index);
}

DockLayout::DockLayout(
	QString name,
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
	QString name,
	BreakPointCpu cpu,
	bool is_default,
	const DockLayout& layout_to_clone,
	DockLayout::Index index)
	: m_name(name)
	, m_cpu(cpu)
	, m_is_default(is_default)
	, m_next_unique_name(layout_to_clone.m_next_unique_name)
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
		parameters.unique_name = unique_name;
		parameters.cpu = &DebugInterface::get(cpu);
		parameters.cpu_override = widget_to_clone->cpuOverride();

		DebuggerWidget* new_widget = widget_description->second.create_widget(parameters);
		new_widget->setCustomDisplayName(widget_to_clone->customDisplayName());
		new_widget->setPrimary(widget_to_clone->isPrimary());
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

const QString& DockLayout::name() const
{
	return m_name;
}

void DockLayout::setName(QString name)
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
	pxAssert(m_is_active);
	m_is_active = false;

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
	pxAssert(!m_is_active);
	m_is_active = true;

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
	}
	else
	{
		// Create all the dock widgets.
		KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
		if (!saver.restoreLayout(m_geometry))
		{
			// We've failed to restore the geometry, so just tear down whatever
			// dock widgets may exist and then setup the default layout.
			for (KDDockWidgets::Core::DockWidget* dock : KDDockWidgets::DockRegistry::self()->dockwidgets())
			{
				// Make sure the dock widget releases ownership of its content.
				auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(dock->view());
				view->setWidget(new QWidget());

				delete dock;
			}

			setupDefaultLayout();
		}
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
		pxAssert(widget_iterator != m_widgets.end());

		setPrimaryDebuggerWidget(widget_iterator->second.get(), false);
		delete widget_iterator->second.get();
		m_widgets.erase(widget_iterator);
	}

	updateDockWidgetTitles();
}

bool DockLayout::canReset()
{
	return DockTables::defaultLayout(m_base_layout) != nullptr;
}

void DockLayout::reset()
{
	pxAssert(!m_is_active);

	for (auto& [unique_name, widget] : m_widgets)
	{
		pxAssert(widget.get());

		delete widget;
	}

	m_next_unique_name = 0;
	m_toolbars.clear();
	m_widgets.clear();
	m_geometry.clear();

	const DockTables::DefaultDockLayout* base_layout = DockTables::defaultLayout(m_base_layout);
	if (!base_layout)
		return;

	for (size_t i = 0; i < base_layout->widgets.size(); i++)
	{
		auto iterator = DockTables::DEBUGGER_WIDGETS.find(base_layout->widgets[i].type);
		pxAssertRel(iterator != DockTables::DEBUGGER_WIDGETS.end(), "Invalid default layout.");
		const DockTables::DebuggerWidgetDescription& dock_description = iterator->second;

		DebuggerWidgetParameters parameters;
		parameters.unique_name = generateNewUniqueName(base_layout->widgets[i].type.c_str());
		parameters.cpu = &DebugInterface::get(m_cpu);

		if (parameters.unique_name.isEmpty())
			continue;

		DebuggerWidget* widget = dock_description.create_widget(parameters);
		widget->setPrimary(true);
		m_widgets.emplace(parameters.unique_name, widget);
	}
}

KDDockWidgets::Core::DockWidget* DockLayout::createDockWidget(const QString& name)
{
	pxAssert(m_is_active);
	pxAssert(KDDockWidgets::LayoutSaver::restoreInProgress());

	auto widget_iterator = m_widgets.find(name);
	if (widget_iterator == m_widgets.end())
		return nullptr;

	DebuggerWidget* widget = widget_iterator->second;
	if (!widget)
		return nullptr;

	pxAssert(widget->uniqueName() == name);

	auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
		KDDockWidgets::Config::self().viewFactory()->createDockWidget(name));
	view->setWidget(widget);

	return view->asController<KDDockWidgets::Core::DockWidget>();
}

void DockLayout::updateDockWidgetTitles()
{
	if (!m_is_active)
		return;

	// Translate default debugger widget names.
	for (auto& [unique_name, widget] : m_widgets)
		widget->retranslateDisplayName();

	// Determine if any widgets have duplicate display names.
	std::map<QString, std::vector<DebuggerWidget*>> display_name_to_widgets;
	for (auto& [unique_name, widget] : m_widgets)
		display_name_to_widgets[widget->displayNameWithoutSuffix()].emplace_back(widget.get());

	for (auto& [display_name, widgets] : display_name_to_widgets)
	{
		std::sort(widgets.begin(), widgets.end(),
			[&](const DebuggerWidget* lhs, const DebuggerWidget* rhs) {
				return lhs->uniqueName() < rhs->uniqueName();
			});

		for (size_t i = 0; i < widgets.size(); i++)
		{
			std::optional<int> suffix_number;
			if (widgets.size() != 1)
				suffix_number = static_cast<int>(i + 1);

			widgets[i]->setDisplayNameSuffixNumber(suffix_number);
		}
	}

	// Propagate the new names from the debugger widgets to the dock widgets.
	for (auto& [unique_name, widget] : m_widgets)
	{
		auto [controller, view] = DockUtils::dockWidgetFromName(widget->uniqueName());
		if (!controller)
			continue;

		controller->setTitle(widget->displayName());
	}
}

const std::map<QString, QPointer<DebuggerWidget>>& DockLayout::debuggerWidgets()
{
	return m_widgets;
}

bool DockLayout::hasDebuggerWidget(const QString& unique_name)
{
	return m_widgets.find(unique_name) != m_widgets.end();
}

size_t DockLayout::countDebuggerWidgetsOfType(const char* type)
{
	size_t count = 0;
	for (const auto& [unique_name, widget] : m_widgets)
	{
		if (strcmp(widget->metaObject()->className(), type) == 0)
			count++;
	}

	return count;
}

void DockLayout::createDebuggerWidget(const std::string& type)
{
	pxAssert(m_is_active);

	if (!g_debugger_window)
		return;

	auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(type);
	pxAssert(description_iterator != DockTables::DEBUGGER_WIDGETS.end());

	const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

	DebuggerWidgetParameters parameters;
	parameters.unique_name = generateNewUniqueName(type.c_str());
	parameters.cpu = &DebugInterface::get(m_cpu);

	if (parameters.unique_name.isEmpty())
		return;

	DebuggerWidget* widget = description.create_widget(parameters);
	m_widgets.emplace(parameters.unique_name, widget);

	setPrimaryDebuggerWidget(widget, countDebuggerWidgetsOfType(type.c_str()) == 0);

	auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
		KDDockWidgets::Config::self().viewFactory()->createDockWidget(widget->uniqueName()));
	view->setWidget(widget);

	KDDockWidgets::Core::DockWidget* controller = view->asController<KDDockWidgets::Core::DockWidget>();
	pxAssert(controller);

	DockUtils::insertDockWidgetAtPreferredLocation(controller, description.preferred_location, g_debugger_window);
	updateDockWidgetTitles();
}

void DockLayout::recreateDebuggerWidget(const QString& unique_name)
{
	if (!g_debugger_window)
		return;

	auto debugger_widget_iterator = m_widgets.find(unique_name);
	pxAssert(debugger_widget_iterator != m_widgets.end());

	DebuggerWidget* old_debugger_widget = debugger_widget_iterator->second;

	auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(old_debugger_widget->metaObject()->className());
	pxAssert(description_iterator != DockTables::DEBUGGER_WIDGETS.end());

	const DockTables::DebuggerWidgetDescription& description = description_iterator->second;

	DebuggerWidgetParameters parameters;
	parameters.unique_name = old_debugger_widget->uniqueName();
	parameters.cpu = &DebugInterface::get(m_cpu);
	parameters.cpu_override = old_debugger_widget->cpuOverride();

	DebuggerWidget* new_debugger_widget = description.create_widget(parameters);
	new_debugger_widget->setCustomDisplayName(old_debugger_widget->customDisplayName());
	new_debugger_widget->setPrimary(old_debugger_widget->isPrimary());
	debugger_widget_iterator->second = new_debugger_widget;

	if (m_is_active)
	{
		auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);
		if (view)
			view->setWidget(new_debugger_widget);
	}

	delete old_debugger_widget;
}

void DockLayout::destroyDebuggerWidget(const QString& unique_name)
{
	pxAssert(m_is_active);

	if (!g_debugger_window)
		return;

	auto debugger_widget_iterator = m_widgets.find(unique_name);
	if (debugger_widget_iterator == m_widgets.end())
		return;

	setPrimaryDebuggerWidget(debugger_widget_iterator->second.get(), false);
	delete debugger_widget_iterator->second.get();
	m_widgets.erase(debugger_widget_iterator);

	auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);
	if (!controller)
		return;

	controller->deleteLater();

	updateDockWidgetTitles();
}

void DockLayout::setPrimaryDebuggerWidget(DebuggerWidget* widget, bool is_primary)
{
	bool present = false;
	for (auto& [unique_name, test_widget] : m_widgets)
	{
		if (test_widget.get() == widget)
		{
			present = true;
			break;
		}
	}

	if (!present)
		return;

	if (is_primary)
	{
		// Set the passed widget as the primary widget.
		for (auto& [unique_name, test_widget] : m_widgets)
		{
			if (strcmp(test_widget->metaObject()->className(), widget->metaObject()->className()) == 0)
			{
				test_widget->setPrimary(test_widget.get() == widget);
			}
		}
	}
	else if (widget->isPrimary())
	{
		// Set an arbitrary widget as the primary widget.
		bool next = true;
		for (auto& [unique_name, test_widget] : m_widgets)
		{
			if (test_widget != widget &&
				strcmp(test_widget->metaObject()->className(), widget->metaObject()->className()) == 0)
			{
				test_widget->setPrimary(next);
				next = false;
			}
		}

		// If we haven't set another widget as the primary one we can't make
		// this one not the primary one.
		if (!next)
			widget->setPrimary(false);
	}
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

	if (m_is_active)
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

	std::string name_str = m_name.toStdString();
	json.AddMember("name", rapidjson::Value().SetString(name_str.c_str(), name_str.size()), json.GetAllocator());
	json.AddMember("target", rapidjson::Value().SetString(cpu_name, strlen(cpu_name)), json.GetAllocator());
	json.AddMember("index", static_cast<int>(layout_index), json.GetAllocator());
	json.AddMember("isDefault", m_is_default, json.GetAllocator());
	json.AddMember("nextUniqueName", m_next_unique_name, json.GetAllocator());

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

	std::string safe_name = Path::SanitizeFileName(m_name.toStdString());

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
	pxAssert(!m_is_active);

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

	auto version_hash = json.FindMember("version_hash");
	if (version_hash == json.MemberEnd() || !version_hash->value.IsString())
	{
		Console.Error("Debugger: Layout file '%s' has missing or invalid 'version_hash' property.", path.c_str());
		result = INVALID_FORMAT;
		return;
	}

	if (strcmp(version_hash->value.GetString(), DockTables::hashDefaultLayouts().c_str()) != 0)
		result = DEFAULT_LAYOUT_HASH_MISMATCH;

	auto name = json.FindMember("name");
	if (name != json.MemberEnd() && name->value.IsString())
		m_name = name->value.GetString();
	else
		m_name = QCoreApplication::translate("DockLayout", "Unnamed");

	m_name.truncate(DockUtils::MAX_LAYOUT_NAME_SIZE);

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

	auto next_unique_name = json.FindMember("nextUniqueName");
	if (next_unique_name != json.MemberBegin() && next_unique_name->value.IsInt())
		m_next_unique_name = next_unique_name->value.GetInt();

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

			auto widgets_iterator = m_widgets.find(unique_name->value.GetString());
			if (widgets_iterator != m_widgets.end())
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
			parameters.unique_name = unique_name->value.GetString();
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

	validatePrimaryDebuggerWidgets();
}

void DockLayout::validatePrimaryDebuggerWidgets()
{
	std::map<std::string, std::vector<DebuggerWidget*>> type_to_widgets;
	for (const auto& [unique_name, widget] : m_widgets)
		type_to_widgets[widget->metaObject()->className()].emplace_back(widget.get());

	for (auto& [type, widgets] : type_to_widgets)
	{
		u32 primary_widgets = 0;

		// Make sure at most one widget is marked as primary.
		for (DebuggerWidget* widget : widgets)
		{
			if (widget->isPrimary())
			{
				if (primary_widgets != 0)
					widget->setPrimary(false);

				primary_widgets++;
			}
		}

		// If none of the widgets were marked as primary, just set the first one
		// as the primary one.
		if (primary_widgets == 0)
			widgets[0]->setPrimary(true);
	}
}

void DockLayout::setupDefaultLayout()
{
	pxAssert(m_is_active);

	if (!g_debugger_window)
		return;

	const DockTables::DefaultDockLayout* base_layout = DockTables::defaultLayout(m_base_layout);
	if (!base_layout)
		return;

	std::vector<KDDockWidgets::QtWidgets::DockWidget*> groups(base_layout->groups.size(), nullptr);

	for (const DockTables::DefaultDockWidgetDescription& dock_description : base_layout->widgets)
	{
		const DockTables::DefaultDockGroupDescription& group =
			base_layout->groups[static_cast<u32>(dock_description.group)];

		DebuggerWidget* widget = nullptr;
		for (auto& [unique_name, test_widget] : m_widgets)
			if (test_widget->metaObject()->className() == dock_description.type)
				widget = test_widget;

		if (!widget)
			continue;

		auto view = static_cast<KDDockWidgets::QtWidgets::DockWidget*>(
			KDDockWidgets::Config::self().viewFactory()->createDockWidget(widget->uniqueName()));
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
}

QString DockLayout::generateNewUniqueName(const char* type)
{
	QString name;
	do
	{
		if (m_next_unique_name == INT_MAX)
			return QString();

		// Produce unique names that will lexicographically sort in the order
		// they were allocated. This ensures the #1, #2, etc suffixes for dock
		// widgets with conflicting names will be assigned in the correct order.
		name = QStringLiteral("%1-%2").arg(m_next_unique_name, 16, 10, QLatin1Char('0')).arg(type);
		m_next_unique_name++;
	} while (hasDebuggerWidget(name));
	return name;
}
