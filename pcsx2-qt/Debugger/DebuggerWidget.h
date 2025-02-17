// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "QtHost.h"
#include "Debugger/DebuggerEvents.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>

class JsonValueWrapper;

// The base class for the contents of the dock widgets in the debugger.
class DebuggerWidget : public QWidget
{
	Q_OBJECT

public:
	// Get the translated name that should be displayed for this widget.
	QString displayName();

	// Get the effective debug interface associated with this particular widget
	// if it's set, otherwise return the one associated with the layout that
	// contains this widget.
	DebugInterface& cpu() const;

	// Set the debug interface associated with the layout. If false is returned,
	// we have to recreate the object.
	bool setCpu(DebugInterface& new_cpu);

	// Get the CPU associated with this particular widget.
	std::optional<BreakPointCpu> cpuOverride() const;

	// Set the CPU associated with the individual dock widget. If false is
	// returned, we have to recreate the object.
	bool setCpuOverride(std::optional<BreakPointCpu> new_cpu);

	// Send each open debugger widget an event in turn, until one handles it.
	template <typename Event>
	static void sendEvent(Event event)
	{
		if (!QtHost::IsOnUIThread())
		{
			QtHost::RunOnUIThread([event = std::move(event)]() {
				DebuggerWidget::sendEventOnUIThread(event);
			});
			return;
		}

		sendEventOnUIThread(event);
	}

	static void sendEventOnUIThread(const DebuggerEvents::Event& event);

	// Send all open debugger widgets an event.
	template <typename Event>
	static void broadcastEvent(Event event)
	{
		if (!QtHost::IsOnUIThread())
		{
			QtHost::RunOnUIThread([event = std::move(event)]() {
				DebuggerWidget::broadcastEventOnUIThread(event);
			});
			return;
		}

		broadcastEventOnUIThread(event);
	}

	static void broadcastEventOnUIThread(const DebuggerEvents::Event& event);

	// Call the handler callback for the specified event.
	bool handleEvent(const DebuggerEvents::Event& event);

	// Register a handler callback for the specified type of event.
	template <typename Event>
	void receiveEvent(std::function<bool(const Event&)> callback)
	{
		m_event_handlers.emplace(
			typeid(Event).name(),
			[callback](const DebuggerEvents::Event& event) -> bool {
				return callback(static_cast<const Event&>(event));
			});
	}

	// Check if this debug widget can receive the specified type of event.
	bool acceptsEventType(const char* event_type);

	// Generates context menu actions to send an event to each debugger widget
	// that can receive it. A submenu is generated if the number of possible
	// receivers exceeds max_top_level_actions.
	template <typename Event>
	std::vector<QAction*> createEventActions(
		QMenu* menu, std::function<std::optional<Event>()> event_func, u32 max_top_level_actions = 5)
	{
		return createEventActions(
			menu, max_top_level_actions, typeid(Event).name(), Event::TEXT,
			[event_func]() {
				static std::optional<Event> event;
				event = event_func();
				if (!event.has_value())
					return static_cast<DebuggerEvents::Event*>(nullptr);

				return static_cast<DebuggerEvents::Event*>(&(*event));
			});
	}

	std::vector<QAction*> createEventActions(
		QMenu* menu,
		u32 max_top_level_actions,
		const char* event_type,
		const char* event_text,
		std::function<const DebuggerEvents::Event*()> event_func);

	static void goToInPrimaryDisassembler(u32 address, u32 flags = DebuggerEvents::NO_FLAGS);
	static void goToInPrimaryMemoryView(u32 address, u32 flags = DebuggerEvents::NO_FLAGS);

	virtual void toJson(JsonValueWrapper& json);
	virtual bool fromJson(JsonValueWrapper& json);

	void applyMonospaceFont();

protected:
	DebuggerWidget(DebugInterface* cpu, QWidget* parent = nullptr);

private:
	DebugInterface* m_cpu;
	std::optional<BreakPointCpu> m_cpu_override;
	std::multimap<std::string, std::function<bool(const DebuggerEvents::Event&)>> m_event_handlers;
};
