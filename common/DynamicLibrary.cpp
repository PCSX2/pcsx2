/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "common/PrecompiledHeader.h"

#include "common/DynamicLibrary.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"

#include <cstring>
#include "fmt/format.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#else
#include <dlfcn.h>
#endif

using namespace Common;

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::DynamicLibrary(const char* filename)
{
	Open(filename);
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& move)
	: m_handle(move.m_handle)
{
	move.m_handle = nullptr;
}

DynamicLibrary::~DynamicLibrary()
{
	Close();
}

std::string DynamicLibrary::GetUnprefixedFilename(const char* filename)
{
#if defined(_WIN32)
	return std::string(filename) + ".dll";
#elif defined(__APPLE__)
	return std::string(filename) + ".dylib";
#else
	return std::string(filename) + ".so";
#endif
}

std::string DynamicLibrary::GetVersionedFilename(const char* libname, int major, int minor)
{
#if defined(_WIN32)
	if (major >= 0 && minor >= 0)
		return fmt::format("{}-{}-{}.dll", libname, major, minor);
	else if (major >= 0)
		return fmt::format("{}-{}.dll", libname, major);
	else
		return fmt::format("{}.dll", libname);
#elif defined(__APPLE__)
	const char* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
	if (major >= 0 && minor >= 0)
		return fmt::format("{}{}.{}.{}.dylib", prefix, libname, major, minor);
	else if (major >= 0)
		return fmt::format("{}{}.{}.dylib", prefix, libname, major);
	else
		return fmt::format("{}{}.dylib", prefix, libname);
#else
	const char* prefix = std::strncmp(libname, "lib", 3) ? "lib" : "";
	if (major >= 0 && minor >= 0)
		return fmt::format("{}{}.so.{}.{}", prefix, libname, major, minor);
	else if (major >= 0)
		return fmt::format("{}{}.so.{}", prefix, libname, major);
	else
		return fmt::format("{}{}.so", prefix, libname);
#endif
}

bool DynamicLibrary::Open(const char* filename)
{
#ifdef _WIN32
	m_handle = reinterpret_cast<void*>(LoadLibraryW(StringUtil::UTF8StringToWideString(filename).c_str()));
	if (!m_handle)
	{
		Console.Error(fmt::format("(DynamicLibrary) Loading {} failed: {}", filename, GetLastError()));
		return false;
	}

	return true;
#else
	m_handle = dlopen(filename, RTLD_NOW);
	if (!m_handle)
	{
		const char* err = dlerror();
		Console.Error(fmt::format("(DynamicLibrary) Loading {} failed: {}", filename, err ? err : ""));
		return false;
	}

	return true;
#endif
}

void DynamicLibrary::Close()
{
	if (!IsOpen())
		return;

#ifdef _WIN32
	FreeLibrary(reinterpret_cast<HMODULE>(m_handle));
#else
	dlclose(m_handle);
#endif
	m_handle = nullptr;
}

void* DynamicLibrary::GetSymbolAddress(const char* name) const
{
#ifdef _WIN32
	return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(m_handle), name));
#else
	return reinterpret_cast<void*>(dlsym(m_handle, name));
#endif
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& move)
{
	Close();
	m_handle = move.m_handle;
	move.m_handle = nullptr;
	return *this;
}
