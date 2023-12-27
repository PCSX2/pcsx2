// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Defs.h"
#include <optional>
#include <utility>

/// ScopedGuard provides an object which runs a function (usually a lambda) when
/// it goes out of scope. This can be useful for releasing resources or handles
/// which do not normally have C++ types to automatically release.
template <typename T>
class ScopedGuard final
{
public:
	__fi ScopedGuard(T&& func)
		: m_func(std::forward<T>(func))
	{
	}
	__fi ScopedGuard(ScopedGuard&& other)
		: m_func(std::move(other.m_func))
	{
		other.m_func = nullptr;
	}

	__fi ~ScopedGuard()
	{
		Run();
	}

	ScopedGuard(const ScopedGuard&) = delete;
	void operator=(const ScopedGuard&) = delete;

	/// Runs the destructor function now instead of when we go out of scope.
	__fi void Run()
	{
		if (!m_func.has_value())
			return;

		m_func.value()();
		m_func.reset();
	}

	/// Prevents the function from being invoked when we go out of scope.
	__fi void Cancel()
	{
		m_func.reset();
	}

private:
	std::optional<T> m_func;
};
