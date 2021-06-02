#pragma once
#include <iostream>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <locale.h>
#include <string>
#include "Path.h"

class PString
{
private:
	size_t count;
	//std::basic_string<char> string;
	const char* string;
public:
	PString();
	// Non default constructors
	explicit PString(const std::wstring& wString);
	explicit PString(const wxString& str);
	PString(const char* string);
	
	// Copy Constructor
	PString(const PString& rhs);

	bool operator==(const PString rhs);
	
	// Asignment operators
	PString& operator=(const std::wstring&);
	PString& operator=(const std::string&);
	wchar_t* operator=(const PString&);

	// Copy Operator
	const PString& operator=(PString&);
	
	operator std::wstring();
	operator wxString();

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
		return string;

	}
	const char* c_str() const noexcept
	{
		return string;
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
	friend std::istream& operator>> (std::istream& is, PString& str);
	friend std::istream& getline(std::istream& is, PString& s, char delim);
	~PString();
};

