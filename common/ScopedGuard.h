/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
		if (!m_func.has_value())
			return;

		m_func.value()();
		m_func.reset();
	}

	ScopedGuard(const ScopedGuard&) = delete;
	void operator=(const ScopedGuard&) = delete;

	/// Prevents the function from being invoked when we go out of scope.
	__fi void Cancel()
	{
		m_func.reset();
	}

private:
	std::optional<T> m_func;
};
