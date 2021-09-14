/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "StringUtil.h"
#include <cctype>
#include <codecvt>
#include <cstdio>
#include <sstream>

#ifdef _WIN32
#include "RedtapeWindows.h"
#endif

namespace StringUtil
{
	std::string StdStringFromFormat(const char* format, ...)
	{
		std::va_list ap;
		va_start(ap, format);
		std::string ret = StdStringFromFormatV(format, ap);
		va_end(ap);
		return ret;
	}

	std::string StdStringFromFormatV(const char* format, std::va_list ap)
	{
		std::va_list ap_copy;
		va_copy(ap_copy, ap);

#ifdef _WIN32
		int len = _vscprintf(format, ap_copy);
#else
		int len = std::vsnprintf(nullptr, 0, format, ap_copy);
#endif
		va_end(ap_copy);

		std::string ret;
		ret.resize(len);
		std::vsnprintf(ret.data(), ret.size() + 1, format, ap);
		return ret;
	}

	bool WildcardMatch(const char* subject, const char* mask, bool case_sensitive /*= true*/)
	{
		if (case_sensitive)
		{
			const char* cp = nullptr;
			const char* mp = nullptr;

			while ((*subject) && (*mask != '*'))
			{
				if ((*mask != '?') && (std::tolower(*mask) != std::tolower(*subject)))
					return false;

				mask++;
				subject++;
			}

			while (*subject)
			{
				if (*mask == '*')
				{
					if (*++mask == 0)
						return true;

					mp = mask;
					cp = subject + 1;
				}
				else
				{
					if ((*mask == '?') || (std::tolower(*mask) == std::tolower(*subject)))
					{
						mask++;
						subject++;
					}
					else
					{
						mask = mp;
						subject = cp++;
					}
				}
			}

			while (*mask == '*')
			{
				mask++;
			}

			return *mask == 0;
		}
		else
		{
			const char* cp = nullptr;
			const char* mp = nullptr;

			while ((*subject) && (*mask != '*'))
			{
				if ((*mask != *subject) && (*mask != '?'))
					return false;

				mask++;
				subject++;
			}

			while (*subject)
			{
				if (*mask == '*')
				{
					if (*++mask == 0)
						return true;

					mp = mask;
					cp = subject + 1;
				}
				else
				{
					if ((*mask == *subject) || (*mask == '?'))
					{
						mask++;
						subject++;
					}
					else
					{
						mask = mp;
						subject = cp++;
					}
				}
			}

			while (*mask == '*')
			{
				mask++;
			}

			return *mask == 0;
		}
	}

	std::size_t Strlcpy(char* dst, const char* src, std::size_t size)
	{
		std::size_t len = std::strlen(src);
		if (len < size)
		{
			std::memcpy(dst, src, len + 1);
		}
		else
		{
			std::memcpy(dst, src, size - 1);
			dst[size - 1] = '\0';
		}
		return len;
	}

	std::size_t Strlcpy(char* dst, const std::string_view& src, std::size_t size)
	{
		std::size_t len = src.length();
		if (len < size)
		{
			std::memcpy(dst, src.data(), len);
			dst[len] = '\0';
		}
		else
		{
			std::memcpy(dst, src.data(), size - 1);
			dst[size - 1] = '\0';
		}
		return len;
	}

	std::optional<std::vector<u8>> DecodeHex(const std::string_view& in)
	{
		std::vector<u8> data;
		data.reserve(in.size() / 2);

		for (size_t i = 0; i < in.size() / 2; i++)
		{
			std::optional<u8> byte = StringUtil::FromChars<u8>(in.substr(i * 2, 2), 16);
			if (byte.has_value())
				data.push_back(*byte);
			else
				return std::nullopt;
		}

		return {data};
	}

	std::string EncodeHex(const u8* data, int length)
	{
		std::stringstream ss;
		for (int i = 0; i < length; i++)
			ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);

		return ss.str();
	}

#ifdef _WIN32

	std::wstring UTF8StringToWideString(const std::string_view& str)
	{
		std::wstring ret;
		if (!UTF8StringToWideString(ret, str))
			return {};

		return ret;
	}

	bool UTF8StringToWideString(std::wstring& dest, const std::string_view& str)
	{
		int wlen = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
		if (wlen < 0)
			return false;

		dest.resize(wlen);
		if (wlen > 0 && MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), dest.data(), wlen) < 0)
			return false;

		return true;
	}

	std::string WideStringToUTF8String(const std::wstring_view& str)
	{
		std::string ret;
		if (!WideStringToUTF8String(ret, str))
			return {};

		return ret;
	}

	bool WideStringToUTF8String(std::string& dest, const std::wstring_view& str)
	{
		int mblen = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
		if (mblen < 0)
			return false;

		dest.resize(mblen);
		if (mblen > 0 && WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), dest.data(), mblen,
							 nullptr, nullptr) < 0)
		{
			return false;
		}

		return true;
	}

#endif

} // namespace StringUtil
