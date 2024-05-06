// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <string>

class Error;

/**
 * Provides a platform-independent interface for loading a dynamic library and retrieving symbols.
 * The interface maintains an internal reference count to allow one handle to be shared between
 * multiple users.
 */
class DynamicLibrary final
{
public:
	/// Default constructor, does not load a library.
	DynamicLibrary();

	/// Automatically loads the specified library. Call IsOpen() to check validity before use.
	DynamicLibrary(const char* filename);

	/// Move constructor, transfers ownership.
	DynamicLibrary(DynamicLibrary&& move);

	/// Closes the library.
	~DynamicLibrary();

	/// Returns the specified library name with the platform-specific suffix added.
	static std::string GetUnprefixedFilename(const char* filename);

	/// Returns the specified library name in platform-specific format.
	/// Major/minor versions will not be included if set to -1.
	/// If libname already contains the "lib" prefix, it will not be added again.
	/// Windows: LIBNAME-MAJOR-MINOR.dll
	/// Linux: libLIBNAME.so.MAJOR.MINOR
	/// Mac: libLIBNAME.MAJOR.MINOR.dylib
	static std::string GetVersionedFilename(const char* libname, int major = -1, int minor = -1);

	/// Returns true if a module is loaded, otherwise false.
	bool IsOpen() const { return m_handle != nullptr; }

	/// Loads (or replaces) the handle with the specified library file name.
	/// Returns true if the library was loaded and can be used.
	bool Open(const char* filename, Error* error);

	/// Adopts, or takes ownership of an existing opened library.
	void Adopt(void* handle);

	/// Unloads the library, any function pointers from this library are no longer valid.
	void Close();

	/// Returns the address of the specified symbol (function or variable) as an untyped pointer.
	/// If the specified symbol does not exist in this library, nullptr is returned.
	void* GetSymbolAddress(const char* name) const;

	/// Obtains the address of the specified symbol, automatically casting to the correct type.
	/// Returns true if the symbol was found and assigned, otherwise false.
	template <typename T>
	bool GetSymbol(const char* name, T* ptr) const
	{
		*ptr = reinterpret_cast<T>(GetSymbolAddress(name));
		return *ptr != nullptr;
	}

	/// Returns the opaque OS-specific handle.
	void* GetHandle() const { return m_handle; }

	/// Move assignment, transfer ownership.
	DynamicLibrary& operator=(DynamicLibrary&& move);

private:
	DynamicLibrary(const DynamicLibrary&) = delete;
	DynamicLibrary& operator=(const DynamicLibrary&) = delete;

	/// Platform-dependent data type representing a dynamic library handle.
	void* m_handle = nullptr;
};
