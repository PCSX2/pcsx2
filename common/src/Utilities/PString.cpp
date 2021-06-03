#include "PrecompiledHeader.h"
#include "PString.h"
#include <codecvt>
#include <wchar.h>

PString::PString()
{
	count = 0;
	string = "";
}

// Non Default
PString::PString(const char* str)
{
	count = sizeof(str);
	string = str;
}

PString::PString(const PString& rhs)
{
	count = rhs.count;
	string = rhs.string;
}

PString::PString(PString&& rhs)
{
	count = std::move(rhs.count);
	string = std::move(rhs.string);
}

const PString& PString::operator=(const PString& rhs)
{
	count = rhs.count;
	string = rhs.string;
	return *this;
}

PString& PString::operator=(PString& rhs)
{
	count = std::move(rhs.count);
	string = std::move(rhs.string);
	return *this;
}

#ifdef _WIN32
// Non default
PString::PString(const std::wstring& str)
{
	count = str.size();
	char* temp = new char[count];
	const wchar_t* wStr = str.c_str();
	size_t len = (wcslen(wStr) + 1) * sizeof(wchar_t);
	int err = wcstombs_s(&count, temp, len, wStr, len);
	if (err != 0)
	{
		delete temp;
	}

	string = temp;
	delete temp;
}

PString& PString::operator=(const std::wstring& str)
{
	count = str.size();
	char* temp = new char[count];
	const wchar_t* wStr = str.c_str();
	size_t len = (wcslen(wStr) + 1) * sizeof(wchar_t);
	int err = wcstombs_s(&count, temp, len, wStr, len);
	if (err != 0)
	{
		std::cout << "Error: " << strerror(err) << std::endl;	
	}
	else
	{
		string = temp;
		delete temp;
	}
		return *this;
}

PString::operator std::wstring()
{
	wchar_t* wstr = new wchar_t[count];
	int err = mbstowcs(wstr, string.data(), count);
	if (err != 0)
	{
		std::cout << "Error: " << strerror(err) << std::endl;
		delete wstr;
		return L"";
	}
	std::wstring buf(wstr);
	delete wstr;
	return buf;
}
#endif

// Non default
PString::PString(const fs::path path)
{
	count = path.string().size();
	string = path.string();
}

// Non default
PString::PString(const wxString& str)
{
	count = str.size();
	string = std::string(str.utf8_str());
}

PString& PString::operator=(const std::string& str)
{
	count = str.size();
	string = str.data();
	return *this;
}

PString::operator wxString()
{
	wxString buf(data());
	return buf;
}

PString::operator std::string()
{
	std::string str(string);
	return str;
}

PString::operator fs::path()
{
	fs::path str(string);
	return str;
}

void PString::resize(size_t siz)
{
	count = siz;
	string.resize(siz);
}

void PString::resize(size_t siz, char c)
{
	count = siz;
	string.resize(siz);
	string = c;
}

const bool PString::operator==(const PString rhs)
{
	return string == rhs.string;
}

std::ostream& operator<<(std::ostream& os, const PString& str)
{
	os << str.string;
	return os;
}

std::istream& operator>>(std::istream& is, PString& str)
{
	char* streambuf = new char[5000];
	is.get(streambuf, 5000);
	str.string = streambuf;
	return is;
}

std::istream& getline(std::istream& is, PString& str, char delim)
{
	char* streambuf = new char[5000];
	is.get(streambuf, 5000, delim);
	str.string = streambuf;
	return is;
}

PString::~PString()
{
	count = 0;
	string = "";
}
