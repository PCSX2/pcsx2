// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "QtHost.h"
#include "Debugger/DebuggerEvents.h"

#include "DebugTools/DebugInterface.h"

#include <QtWidgets/QWidget>

class JsonValueWrapper;

// Container for variables to be passed to the constructor of DebuggerWidget.
struct DebuggerWidgetParameters
{
	QString unique_name;
	DebugInterface* cpu = nullptr;
	std::optional<BreakPointCpu> cpu_override;
	QWidget* parent = nullptr;
};

// The base class for the contents of the dock widgets in the debugger.
class DebuggerWidget : public QWidget
{
	Q_OBJECT

public:
	QString uniqueName() const;

	// Get the translated name that should be displayed for this widget.
	QString displayName() const;
	QString displayNameWithoutSuffix() const;

	QString customDisplayName() const;
	bool setCustomDisplayName(QString display_name);

	bool isPrimary() const;
	void setPrimary(bool is_primary);

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
				DebuggerWidget::sendEventImplementation(event);
			});
			return;
		}

		sendEventImplementation(event);
	}

	// Send all open debugger widgets an event.
	template <typename Event>
	static void broadcastEvent(Event event)
	{
		if (!QtHost::IsOnUIThread())
		{
			QtHost::RunOnUIThread([event = std::move(event)]() {
				DebuggerWidget::broadcastEventImplementation(event);
			});
			return;
		}

		broadcastEventImplementation(event);
	}

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

	// Register a handler member function for the specified type of event.
	template <typename Event, typename SubClass>
	void receiveEvent(bool (SubClass::*function)(const Event& event))
	{
		m_event_handlers.emplace(
			typeid(Event).name(),
			[this, function](const DebuggerEvents::Event& event) -> bool {
				return (*static_cast<SubClass*>(this).*function)(static_cast<const Event&>(event));
			});
	}

	// Call the handler callback for the specified event.
	bool handleEvent(const DebuggerEvents::Event& event);

	// Check if this debugger widget can receive the specified type of event.
	bool acceptsEventType(const char* event_type);

	// Generates context menu actions to send an event to each debugger widget
	// that can receive it. A submenu is generated if the number of possible
	// receivers exceeds max_top_level_actions. If skip_self is true, actions
	// are only generated if the sender and receiver aren't the same object.
	template <typename Event>
	std::vector<QAction*> createEventActions(
		QMenu* menu,
		std::function<std::optional<Event>()> event_func,
		bool skip_self = true,
		u32 max_top_level_actions = 5)
	{
		return createEventActionsImplementation(
			menu, max_top_level_actions, skip_self, typeid(Event).name(), Event::ACTION_PREFIX,
			[event_func]() -> DebuggerEvents::Event* {
				static std::optional<Event> event;
				event = event_func();
				if (!event.has_value())
					return nullptr;

				return static_cast<DebuggerEvents::Event*>(&(*event));
			});
	}

	virtual void toJson(JsonValueWrapper& json);
	virtual bool fromJson(const JsonValueWrapper& json);

	void switchToThisTab();

	bool supportsMultipleInstances();

	void retranslateDisplayName();

	std::optional<int> displayNameSuffixNumber() const;
	void setDisplayNameSuffixNumber(std::optional<int> suffix_number);

	void updateStyleSheet();

	static void goToInDisassembler(u32 address, bool switch_to_tab);
	static void goToInMemoryView(u32 address, bool switch_to_tab);

protected:
	enum Flags
	{
		NO_DEBUGGER_FLAGS = 0,
		// Prevent the user from opening multiple dock widgets of this type.
		DISALLOW_MULTIPLE_INSTANCES = 1 << 0,
		// Apply a stylesheet that gives all the text a monospace font.
		MONOSPACE_FONT = 1 << 1
	};

	DebuggerWidget(const DebuggerWidgetParameters& parameters, u32 flags);

private:
	static void sendEventImplementation(const DebuggerEvents::Event& event);
	static void broadcastEventImplementation(const DebuggerEvents::Event& event);

	std::vector<QAction*> createEventActionsImplementation(
		QMenu* menu,
		u32 max_top_level_actions,
		bool skip_self,
		const char* event_type,
		const char* action_prefix,
		std::function<const DebuggerEvents::Event*()> event_func);

	QString m_unique_name;

	// A user-defined name, or an empty string if no name was specified so that
	// the default names can be retranslated on the fly.
	QString m_custom_display_name;

	QString m_translated_display_name;
	std::optional<int> m_display_name_suffix_number;

	// Primary debugger widgets will be chosen to handle events first. For
	// example, clicking on an address to go to it in the primary memory view.
	bool m_is_primary = false;

	DebugInterface* m_cpu;
	std::optional<BreakPointCpu> m_cpu_override;
	u32 m_flags;

	std::multimap<std::string, std::function<bool(const DebuggerEvents::Event&)>> m_event_handlers;
};
