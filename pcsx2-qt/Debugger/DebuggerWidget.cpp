// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebuggerWidget.h"

#include "Debugger/DebuggerWindow.h"
#include "Debugger/JsonValueWrapper.h"
#include "Debugger/Docking/DockManager.h"
#include "Debugger/Docking/DockTables.h"

#include "DebugTools/DebugInterface.h"

#include "common/Assertions.h"

DebugInterface& DebuggerWidget::cpu() const
{
	if (m_cpu_override.has_value())
		return DebugInterface::get(*m_cpu_override);

	pxAssertRel(m_cpu, "DebuggerWidget::cpu called on object with null cpu.");
	return *m_cpu;
}

QString DebuggerWidget::displayName()
{
	auto description = DockTables::DEBUGGER_WIDGETS.find(metaObject()->className());
	if (description == DockTables::DEBUGGER_WIDGETS.end())
		return QString();

	return QCoreApplication::translate("DebuggerWidget", description->second.display_name);
}

bool DebuggerWidget::setCpu(DebugInterface& new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu = &new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

std::optional<BreakPointCpu> DebuggerWidget::cpuOverride() const
{
	return m_cpu_override;
}

bool DebuggerWidget::setCpuOverride(std::optional<BreakPointCpu> new_cpu)
{
	BreakPointCpu before = cpu().getCpuType();
	m_cpu_override = new_cpu;
	BreakPointCpu after = cpu().getCpuType();
	return before == after;
}

bool DebuggerWidget::handleEvent(const DebuggerEvents::Event& event)
{
	auto [begin, end] = m_event_handlers.equal_range(typeid(event).name());
	for (auto handler = begin; handler != end; handler++)
		if (handler->second(event))
		{
			if ((event.flags & DebuggerEvents::SWITCH_TO_RECEIVER) && g_debugger_window)
				g_debugger_window->dockManager().switchToDebuggerWidget(this);

			return true;
		}

	return false;
}

bool DebuggerWidget::acceptsEventType(const char* event_type)
{
	auto [begin, end] = m_event_handlers.equal_range(event_type);
	return begin != end;
}

void DebuggerWidget::goToInDisassembler(u32 address, u32 flags)
{
	DebuggerEvents::GoToAddress event;
	event.address = address;
	event.filter = DebuggerEvents::GoToAddress::DISASSEMBLER;
	event.flags = flags;
	DebuggerWidget::sendEvent(std::move(event));
}

void DebuggerWidget::goToInMemoryView(u32 address, u32 flags)
{
	DebuggerEvents::GoToAddress event;
	event.address = address;
	event.filter = DebuggerEvents::GoToAddress::MEMORY_VIEW;
	event.flags = flags;
	DebuggerWidget::sendEvent(std::move(event));
}


void DebuggerWidget::toJson(JsonValueWrapper& json)
{
}

bool DebuggerWidget::fromJson(JsonValueWrapper& json)
{
	return true;
}

void DebuggerWidget::applyMonospaceFont()
{
	// Easiest way to handle cross platform monospace fonts
	// There are issues related to TabWidget -> Children font inheritance otherwise
#if defined(WIN32)
	setStyleSheet(QStringLiteral("font: 10pt 'Lucida Console'"));
#elif defined(__APPLE__)
	setStyleSheet(QStringLiteral("font: 10pt 'Monaco'"));
#else
	setStyleSheet(QStringLiteral("font: 10pt 'Monospace'"));
#endif
}

DebuggerWidget::DebuggerWidget(const DebuggerWidgetParameters& parameters)
	: QWidget(parameters.parent)
	, m_cpu(parameters.cpu)
	, m_cpu_override(parameters.cpu_override)
{
}

void DebuggerWidget::sendEventImplementation(const DebuggerEvents::Event& event)
{
	if (!g_debugger_window)
		return;

	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerWidgets())
		if (widget->handleEvent(event))
			return;
}

void DebuggerWidget::broadcastEventImplementation(const DebuggerEvents::Event& event)
{
	if (!g_debugger_window)
		return;

	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerWidgets())
		widget->handleEvent(event);
}

std::vector<QAction*> DebuggerWidget::createEventActionsImplementation(
	QMenu* menu,
	u32 max_top_level_actions,
	bool skip_self,
	const char* event_type,
	const char* event_text,
	std::function<const DebuggerEvents::Event*()> event_func)
{
	if (!g_debugger_window)
		return {};

	std::vector<DebuggerWidget*> receivers;
	for (const auto& [unique_name, widget] : g_debugger_window->dockManager().debuggerWidgets())
		if ((!skip_self || widget != this) && widget->acceptsEventType(event_type))
			receivers.emplace_back(widget);

	QMenu* submenu = nullptr;
	if (receivers.size() > max_top_level_actions)
	{
		QString title_format = QCoreApplication::translate("DebuggerEvent", "%1 in...");
		submenu = new QMenu(title_format.arg(QCoreApplication::translate("DebuggerEvent", event_text)), menu);
	}

	std::vector<QAction*> actions;
	for (size_t i = 0; i < receivers.size(); i++)
	{
		DebuggerWidget* receiver = receivers[i];

		QAction* action;
		if (!submenu || i + 1 < max_top_level_actions)
		{
			QString title_format = QCoreApplication::translate("DebuggerEvent", "%1 in %2");
			QString event_title = QCoreApplication::translate("DebuggerEvent", event_text);
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
