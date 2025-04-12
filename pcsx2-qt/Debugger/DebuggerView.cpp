// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerView.h"

#include "Debugger/DebuggerWindow.h"
#include "Debugger/JsonValueWrapper.h"
#include "Debugger/Docking/DockManager.h"
#include "Debugger/Docking/DockTables.h"

#include "DebugTools/DebugInterface.h"

#include "common/Assertions.h"

DebuggerView::DebuggerView(const DebuggerViewParameters& parameters, u32 flags)
	: QWidget(parameters.parent)
	, m_id(parameters.id)
	, m_unique_name(parameters.unique_name)
	, m_cpu(parameters.cpu)
	, m_cpu_override(parameters.cpu_override)
	, m_flags(flags)
{
	updateStyleSheet();
}

DebugInterface& DebuggerView::cpu() const
{
	if (m_cpu_override.has_value())
		return DebugInterface::get(*m_cpu_override);

	pxAssertRel(m_cpu, "DebuggerView::cpu called on object with null cpu.");
	return *m_cpu;
}

QString DebuggerView::uniqueName() const
{
	return m_unique_name;
}

u64 DebuggerView::id() const
{
	return m_id;
}

QString DebuggerView::displayName() const
{
	QString name = displayNameWithoutSuffix();

	// If there are multiple debugger views with the same name, append a number
	// to the display name.
	if (m_display_name_suffix_number.has_value())
		name = tr("%1 #%2").arg(name).arg(*m_display_name_suffix_number);

	if (m_cpu_override)
		name = tr("%1 (%2)").arg(name).arg(DebugInterface::cpuName(*m_cpu_override));

	return name;
}

QString DebuggerView::displayNameWithoutSuffix() const
{
	return m_translated_display_name;
}

QString DebuggerView::customDisplayName() const
{
	return m_custom_display_name;
}

bool DebuggerView::setCustomDisplayName(QString display_name)
{
	if (display_name.size() > DockUtils::MAX_DOCK_WIDGET_NAME_SIZE)
		return false;

	m_custom_display_name = display_name;
	return true;
}

bool DebuggerView::isPrimary() const
{
	return m_is_primary;
}

void DebuggerView::setPrimary(bool is_primary)
{
	m_is_primary = is_primary;
}

bool DebuggerView::setCpu(DebugInterface& new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu = &new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

std::optional<BreakPointCpu> DebuggerView::cpuOverride() const
{
	return m_cpu_override;
}

bool DebuggerView::setCpuOverride(std::optional<BreakPointCpu> new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu_override = new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

bool DebuggerView::handleEvent(const DebuggerEvents::Event& event)
{
	auto [begin, end] = m_event_handlers.equal_range(typeid(event).name());
	for (auto handler = begin; handler != end; handler++)
		if (handler->second(event))
			return true;

	return false;
}

bool DebuggerView::acceptsEventType(const char* event_type)
{
	auto [begin, end] = m_event_handlers.equal_range(event_type);
	return begin != end;
}


void DebuggerView::toJson(JsonValueWrapper& json)
{
	std::string custom_display_name_str = m_custom_display_name.toStdString();
	rapidjson::Value custom_display_name;
	custom_display_name.SetString(custom_display_name_str.c_str(), custom_display_name_str.size(), json.allocator());
	json.value().AddMember("customDisplayName", custom_display_name, json.allocator());

	json.value().AddMember("isPrimary", m_is_primary, json.allocator());
}

bool DebuggerView::fromJson(const JsonValueWrapper& json)
{
	auto custom_display_name = json.value().FindMember("customDisplayName");
	if (custom_display_name != json.value().MemberEnd() && custom_display_name->value.IsString())
	{
		m_custom_display_name = QString(custom_display_name->value.GetString());
		m_custom_display_name.truncate(DockUtils::MAX_DOCK_WIDGET_NAME_SIZE);
	}

	auto is_primary = json.value().FindMember("isPrimary");
	if (is_primary != json.value().MemberEnd() && is_primary->value.IsBool())
		m_is_primary = is_primary->value.GetBool();

	return true;
}

void DebuggerView::switchToThisTab()
{
	g_debugger_window->dockManager().switchToDebuggerView(this);
}

bool DebuggerView::supportsMultipleInstances()
{
	return !(m_flags & DISALLOW_MULTIPLE_INSTANCES);
}

void DebuggerView::retranslateDisplayName()
{
	if (!m_custom_display_name.isEmpty())
	{
		m_translated_display_name = m_custom_display_name;
	}
	else
	{
		auto description = DockTables::DEBUGGER_VIEWS.find(metaObject()->className());
		if (description != DockTables::DEBUGGER_VIEWS.end())
			m_translated_display_name = QCoreApplication::translate("DebuggerView", description->second.display_name);
		else
			m_translated_display_name = QString();
	}
}

std::optional<int> DebuggerView::displayNameSuffixNumber() const
{
	return m_display_name_suffix_number;
}

void DebuggerView::setDisplayNameSuffixNumber(std::optional<int> suffix_number)
{
	m_display_name_suffix_number = suffix_number;
}

void DebuggerView::updateStyleSheet()
{
	QString stylesheet;

	if (m_flags & MONOSPACE_FONT)
	{
		// Easiest way to handle cross platform monospace fonts
		// There are issues related to TabWidget -> Children font inheritance otherwise
#if defined(WIN32)
		stylesheet += QStringLiteral("font-family: 'Lucida Console';");
#elif defined(__APPLE__)
		stylesheet += QStringLiteral("font-family: 'Monaco';");
#else
		stylesheet += QStringLiteral("font-family: 'Monospace';");
#endif
	}

	// HACK: Make the font size smaller without applying a stylesheet to the
	// whole window (which would impact performance).
	if (g_debugger_window)
		stylesheet += QString("font-size: %1pt;").arg(g_debugger_window->fontSize());

	setStyleSheet(stylesheet);
}

void DebuggerView::goToInDisassembler(u32 address, bool switch_to_tab)
{
	DebuggerEvents::GoToAddress event;
	event.address = address;
	event.filter = DebuggerEvents::GoToAddress::DISASSEMBLER;
	event.switch_to_tab = switch_to_tab;
	DebuggerView::sendEvent(std::move(event));
}

void DebuggerView::goToInMemoryView(u32 address, bool switch_to_tab)
{
	DebuggerEvents::GoToAddress event;
	event.address = address;
	event.filter = DebuggerEvents::GoToAddress::MEMORY_VIEW;
	event.switch_to_tab = switch_to_tab;
	DebuggerView::sendEvent(std::move(event));
}

void DebuggerView::sendEventImplementation(const DebuggerEvents::Event& event)
{
	if (!g_debugger_window)
		return;

	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerViews())
		if (widget->isPrimary() && widget->handleEvent(event))
			return;

	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerViews())
		if (!widget->isPrimary() && widget->handleEvent(event))
			return;
}

void DebuggerView::broadcastEventImplementation(const DebuggerEvents::Event& event)
{
	if (!g_debugger_window)
		return;

	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerViews())
		widget->handleEvent(event);
}

std::vector<QAction*> DebuggerView::createEventActionsImplementation(
	QMenu* menu,
	u32 max_top_level_actions,
	bool skip_self,
	const char* event_type,
	const char* action_prefix,
	std::function<const DebuggerEvents::Event*()> event_func)
{
	if (!g_debugger_window)
		return {};

	std::vector<DebuggerView*> receivers;
	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerViews())
		if ((!skip_self || widget != this) && widget->acceptsEventType(event_type))
			receivers.emplace_back(widget);

	std::sort(receivers.begin(), receivers.end(), [&](const DebuggerView* lhs, const DebuggerView* rhs) {
		if (lhs->displayNameWithoutSuffix() == rhs->displayNameWithoutSuffix())
			return lhs->displayNameSuffixNumber() < rhs->displayNameSuffixNumber();

		return lhs->displayNameWithoutSuffix() < rhs->displayNameWithoutSuffix();
	});

	QMenu* submenu = nullptr;
	if (receivers.size() > max_top_level_actions)
	{
		QString title_format = QCoreApplication::translate("DebuggerEvent", "%1...");
		submenu = new QMenu(title_format.arg(QCoreApplication::translate("DebuggerEvent", action_prefix)), menu);
	}

	std::vector<QAction*> actions;
	for (size_t i = 0; i < receivers.size(); i++)
	{
		DebuggerView* receiver = receivers[i];

		QAction* action;
		if (!submenu || i + 1 < max_top_level_actions)
		{
			QString title_format = QCoreApplication::translate("DebuggerEvent", "%1 %2");
			QString event_title = QCoreApplication::translate("DebuggerEvent", action_prefix);
			QString title = title_format.arg(event_title).arg(receiver->displayName());
			action = new QAction(title, menu);
			menu->addAction(action);
		}
		else
		{
			action = new QAction(receiver->displayName(), submenu);
			submenu->addAction(action);
		}

		connect(action, &QAction::triggered, receiver, [receiver, event_func]() {
			const DebuggerEvents::Event* event = event_func();
			if (event)
				receiver->handleEvent(*event);
		});

		actions.emplace_back(action);
	}

	if (submenu)
		menu->addMenu(submenu);

	return actions;
}
