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
PString::PString(const std::wstring& str)
{
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
