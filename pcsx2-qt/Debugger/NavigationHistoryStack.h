// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/DebuggerWindow.h"

#include <QtCore/QTimer>

#include <deque>

enum class NavigationHistoryOperation
{
	INSTANT_PUSH,
	DELAYED_PUSH,
	NO_PUSH
};

/// Data structure for storing navigation history, to be used for the back and
/// forward buttons.
template <typename Element>
class NavigationHistoryStack
{
public:
	NavigationHistoryStack()
		: m_position(m_elements.end())
	{
		m_timer.setInterval(DELAY_MILLISECONDS);

		QObject::connect(&m_timer, &QTimer::timeout, [this]() {
			pushInstantly(std::move(m_pending));
			m_timer.stop();
		});
	}

	NavigationHistoryStack(Element element)
		: NavigationHistoryStack()
	{
		pushInstantly(std::move(element));
	}

	NavigationHistoryStack(const NavigationHistoryStack<Element>&) = delete;
	NavigationHistoryStack<Element>& operator=(const NavigationHistoryStack<Element>&) = delete;

	NavigationHistoryStack(NavigationHistoryStack<Element>&&) = delete;
	NavigationHistoryStack<Element>& operator=(NavigationHistoryStack<Element>&&) = delete;

	/// Push an element onto the stack.
	void push(Element element, NavigationHistoryOperation operation)
	{
		switch (operation)
		{
			case NavigationHistoryOperation::INSTANT_PUSH:
				pushInstantly(std::move(element));
				break;
			case NavigationHistoryOperation::DELAYED_PUSH:
				pushWithDelay(std::move(element));
				break;
			case NavigationHistoryOperation::NO_PUSH:
				break;
		}
	}

	/// Wait a second, and then push an element onto the stack. If another
	/// element is pushed, then that will take priority. This is useful if we're
	/// storing a scroll position, and don't want the stack to be spammed with
	/// new values when the user scrolls, for example.
	void pushWithDelay(Element element)
	{
		m_pending = std::move(element);
		m_timer.start();
	}

	/// Push an element onto the stack instantly.
	void pushInstantly(Element element)
	{
		m_timer.stop();

		if (m_position != m_elements.end() && element == *(m_position - 1))
			return;

		updateNavigationButtons();

		m_elements.erase(m_position, m_elements.end());
		m_elements.push_back(std::move(element));

		if (m_elements.size() > MAX_ELEMENTS)
			m_elements.pop_front();

		m_position = m_elements.end();
	}

	/// Should to user have an option to go back?
	bool canGoBack() const
	{
		return m_elements.begin() != m_elements.end() && m_position > m_elements.begin() + 1;
	}

	/// Should the user have an option to go forward?
	bool canGoForward() const
	{
		return m_position != m_elements.end();
	}

	// Retrieve the current element.
	std::optional<Element> current() const
	{
		if (m_position == m_elements.begin())
			return std::nullopt;

		return *(m_position - 1);
	}

	/// Go back one.
	std::optional<Element> back()
	{
		updateNavigationButtons();

		if (!canGoBack())
			return std::nullopt;

		m_position--;
		return *(m_position - 1);
	}

	/// Go forward one.
	std::optional<Element> forward()
	{
		updateNavigationButtons();

		if (!canGoForward())
			return std::nullopt;

		Element element = *m_position;
		m_position++;
		return element;
	}

	/// Remove all elements.
	void clear()
	{
		m_elements.clear();
		m_position = m_elements.end();
		m_timer.stop();
	}

private:
	void updateNavigationButtons()
	{
		QTimer::singleShot(0, []() {
			if (g_debugger_window)
				g_debugger_window->updateNavigationButtons();
		});
	}

	std::deque<Element> m_elements;
	std::deque<Element>::iterator m_position;

	QTimer m_timer;
	Element m_pending;

	static constexpr int DELAY_MILLISECONDS = 1000;
	static constexpr size_t MAX_ELEMENTS = 1000;
};
