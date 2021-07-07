#include "PrecompiledHeader.h"
#include "PString.h"
#include <codecvt>
#include <wchar.h>

const PString& PString::operator=(const PString& rhs)
{
	string = rhs.string;
	return *this;
}

PString& PString::operator=(PString&& rhs)
{
	string = std::move(rhs.string);
	return *this;
}

#ifdef _WIN32
// Non default
PString::PString(const std::wstring& utf16_string)
{
	const int size = WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), nullptr, 0, nullptr, nullptr);
	std::string converted_string(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), converted_string.data(), converted_string.size(), nullptr, nullptr);
	string = converted_string;
}

PString::operator std::wstring()
{
	int size = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, nullptr, 0);
	std::vector<wchar_t> converted_string(size);
	MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, converted_string.data(), converted_string.size());
	return { converted_string.data() };
}
#endif

PString::operator wxString() const
{
	wxString buf(data());
	return buf;
}

PString::operator std::string() const
{
	std::string str(string);
	return str;
}

PString::operator fs::path() const
{
	fs::path str(string);
	return str;
}

void PString::resize(size_t siz, char c)
{
	string.resize(siz);
	string = c;
}

std::ostream& operator<<(std::ostream& os, const PString& str)
{
	os << str.string;
	return os;
}

/*std::istream& operator>>(std::istream& is, PString& str)
{
	char* streambuf = new char[5000];
	is.get(streambuf, 5000);
	str.string = streambuf;
	return is;
}*/

/*std::istream& getline(std::istream& is, PString& str, char delim)
{
	char* streambuf = new char[5000];
	is.get(streambuf, 5000, delim);
	str.string = streambuf;
	return is;
}*/
