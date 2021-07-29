#include "PrecompiledHeader.h"
#include "PString.h"
#include <codecvt>
#include <wchar.h>

#ifdef __cpp_lib_char8_t
PString::operator std::u8string()
{
	std::u8string str(string.begin(), string.end());
	return str;
}
#endif


#ifdef _WIN32
// Non default
PString::PString(const std::wstring& utf16_string)
{
	const int size = WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), nullptr, 0, nullptr, nullptr);
	std::string converted_string(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, utf16_string.c_str(), utf16_string.size(), converted_string.data(), size, nullptr, nullptr);
	string = converted_string;
}

PString::operator std::wstring()
{
	const int size = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), string.size(), nullptr, 0);
	std::wstring converted_string(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, string.c_str(), string.size(), converted_string.data(), size);
	return converted_string;
}
#endif

PString::operator wxString() const
{
	wxString buf(string, string.size());
	return buf;
}

fs::path PString::path() const
{
	fs::path str(fs::u8path(string));
	return str;
}

std::ostream& operator<<(std::ostream& os, const PString& str)
{
	os << str.string;
	return os;
}

std::string PString::mb() const 
{
#ifdef _WIN32
	PString tempS = string;
	std::wstring temp = tempS;
	const int size = WideCharToMultiByte(CP_ACP, 0, temp.c_str(), temp.size(), nullptr, 0, nullptr, nullptr);
	std::string converted_string(size, 0);
	WideCharToMultiByte(CP_ACP, 0, temp.c_str(), temp.size(), converted_string.data(), converted_string.size(), nullptr, nullptr);
	return converted_string;
#else
	return string;
#endif
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
