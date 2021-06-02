#pragma once
#include "ghc/filesystem.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string>
#include "Path.h"

namespace fs = ghc::filesystem;

class PString
{
private:
	size_t count;
	std::basic_string<char> string;
	//const char* string;
public:
	PString();

	// Non default constructors
	PString(const char* string);
	PString(const fs::path path);
	PString(const wxString& str);

	#ifdef _WIN32
	PString(const std::wstring& wString);
	PString& operator=(const std::wstring&);
	operator std::wstring();
	#endif

	#ifdef __cpp_lib_char8_t
	PString(const std::u8string str);
	PString& operator=(const std::u8string&);
	operator std::u8string();
	#endif

	// Copy Constructor
	PString(const PString& rhs);

	PString(PString&& move);

	bool operator==(const PString rhs);
	
	PString& operator=(const std::string&);
	PString& operator=(PString&);

	// Copy Operator
	const PString& operator=(const PString&);
	
	operator std::string();
	operator wxString();
	operator fs::path();

	size_t capacity() const noexcept
	{
		return count;
	}

	size_t size() const noexcept
	{
		return sizeof(string);
	}

	char& at(size_t pos);
	const char& at(size_t pos) const;

	void resize(size_t n);
	void resize(size_t n, char c);

	const char* data() const
	{
		return string.data();
	}
	const char* c_str() const noexcept
	{
		return string.c_str();
	}

	bool empty() const noexcept
	{
		return count == 0;
	}

	char operator[](size_t index)
	{
		return string[index];
	}

	friend std::ostream& operator<<(std::ostream & os, const PString& str);
	friend std::istream& operator>>(std::istream& is, PString& str);
	friend std::istream& getline(std::istream& is, PString& s, char delim);
	~PString();
};

