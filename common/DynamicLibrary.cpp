// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
