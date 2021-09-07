/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2021 PCSX2 Dev Team
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

#ifdef _WIN32
#include <Windows.h>
#include <wil/com.h>
#else
#include <dlfcn.h>
#endif

#include <fmt/core.h>
#include "common/DynamicLibrary.h"

namespace Common
{
#if defined(_WIN32)
	struct DynamicLibrary::Impl
	{
		static constexpr const char* ExtName()
		{
			return "dll";
		}

		bool Open(const char* name)
		{
			constexpr DWORD flags =
				LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
				LOAD_LIBRARY_SEARCH_SYSTEM32;

			m_handle.reset(LoadLibraryExA(name, nullptr, flags));

			return IsOpen();
		}

		void Close()
		{
			m_handle.reset();
		}

		void* GetSymbolByName(const char* name)
		{
			auto sym = GetProcAddress(m_handle.get(), name);

			return reinterpret_cast<void*>(sym);
		}

		void* GetSymbolByOrdinal(const uint ordinal)
		{
			auto sym = GetProcAddress(m_handle.get(), reinterpret_cast<LPCSTR>(ordinal));

			return reinterpret_cast<void*>(sym);
		}

		bool IsOpen() const
		{
			return m_handle != nullptr;
		}

		wil::unique_hmodule m_handle{ nullptr };
	};
#else
	struct DynamicLibrary::Impl
	{
		static constexpr const char* ExtName()
		{
#if defined(__APPLE__)
			return "dylib";
#else
			return "so";
#endif
		}

		bool Open(const char* name)
		{
			m_handle = dlopen(name, RTLD_NOW);

			return IsOpen();
		}

		void Close()
		{
			if (m_handle)
				dlclose(m_handle);

			m_handle = nullptr;
		}

		void* GetSymbolByName(const char* name)
		{
			return dlsym(m_handle, name);
		}

		void* GetSymbolByOrdinal(const int /*ordinal*/)
		{
			return nullptr;
		}

		bool IsOpen() const
		{
			return m_handle != nullptr;
		}

		void* m_handle{ nullptr };
	};
#endif

	DynamicLibrary::DynamicLibrary()
		: m_impl(std::make_unique<Impl>())
	{
	}

	DynamicLibrary::DynamicLibrary(const char* path)
		: m_impl(std::make_unique<Impl>())
	{
		Open(path);
	}

	DynamicLibrary::~DynamicLibrary()
	{
		m_impl->Close();
	}

	bool DynamicLibrary::Open(const char* path)
	{
		const std::string full_name =
			fmt::format("{}.{}", path, Impl::ExtName());

		return m_impl->Open(full_name.data());
	}

	void DynamicLibrary::Close()
	{
		m_impl->Close();
	}

	bool DynamicLibrary::IsOpen() const
	{
		return m_impl->IsOpen();
	}

	void* DynamicLibrary::GetSymbolByName(const char* name) const
	{
		if (!IsOpen())
			return nullptr;

		return m_impl->GetSymbolByName(name);
	}

	void* DynamicLibrary::GetSymbolByOrdinal(const uint id) const
	{
		if (!IsOpen())
			return nullptr;

		return m_impl->GetSymbolByOrdinal(id);
	}
}