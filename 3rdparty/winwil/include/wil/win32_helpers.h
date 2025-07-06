//*********************************************************
//
//    Copyright (c) Microsoft. All rights reserved.
//    This code is licensed under the MIT License.
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
//    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//    PARTICULAR PURPOSE AND NONINFRINGEMENT.
//
//*********************************************************
//! @file
//! Various types and helpers for interfacing with various Win32 APIs
#ifndef __WIL_WIN32_HELPERS_INCLUDED
#define __WIL_WIN32_HELPERS_INCLUDED

#include <minwindef.h>    // FILETIME, HINSTANCE
#include <sysinfoapi.h>   // GetSystemTimeAsFileTime
#include <libloaderapi.h> // GetProcAddress
#include <Psapi.h>        // GetModuleFileNameExW (macro), K32GetModuleFileNameExW
#include <winreg.h>
#include <objbase.h>

#include "common.h"

#if WIL_USE_STL
#include <string>
#if (__WI_LIBCPP_STD_VER >= 17) && WI_HAS_INCLUDE(<string_view>, 1) // Assume present if C++17
#include <string_view>
#endif
#if (__WI_LIBCPP_STD_VER >= 20)
#if WI_HAS_INCLUDE(<bit>, 1) // Assume present if C++20
#include <bit>
#endif
#if WI_HAS_INCLUDE(<compare>, 1) // Assume present if C++20
#include <compare>
#endif
#endif
#endif

/// @cond
#if __cpp_lib_bit_cast >= 201806L
#define __WI_CONSTEXPR_BIT_CAST constexpr
#else
#define __WI_CONSTEXPR_BIT_CAST inline
#endif
/// @endcond

#include "result.h"
#include "resource.h"
#include "wistd_functional.h"
#include "wistd_type_traits.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

/// @cond
namespace wistd
{
#if WIL_USE_STL && (__cpp_lib_three_way_comparison >= 201907L)

using weak_ordering = std::weak_ordering;

#else

struct weak_ordering
{
    static const weak_ordering less;
    static const weak_ordering equivalent;
    static const weak_ordering greater;

    [[nodiscard]] friend constexpr bool operator==(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value == 0;
    }

    [[nodiscard]] friend constexpr bool operator!=(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value != 0;
    }

    [[nodiscard]] friend constexpr bool operator<(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value < 0;
    }

    [[nodiscard]] friend constexpr bool operator>(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value > 0;
    }

    [[nodiscard]] friend constexpr bool operator<=(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value <= 0;
    }

    [[nodiscard]] friend constexpr bool operator>=(const weak_ordering left, std::nullptr_t) noexcept
    {
        return left.m_value >= 0;
    }

    [[nodiscard]] friend constexpr bool operator==(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right == 0;
    }

    [[nodiscard]] friend constexpr bool operator!=(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right != 0;
    }

    [[nodiscard]] friend constexpr bool operator<(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right > 0;
    }

    [[nodiscard]] friend constexpr bool operator>(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right < 0;
    }

    [[nodiscard]] friend constexpr bool operator<=(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right >= 0;
    }

    [[nodiscard]] friend constexpr bool operator>=(std::nullptr_t, const weak_ordering right) noexcept
    {
        return right <= 0;
    }

    signed char m_value;
};

inline constexpr weak_ordering weak_ordering::less{static_cast<signed char>(-1)};
inline constexpr weak_ordering weak_ordering::equivalent{static_cast<signed char>(0)};
inline constexpr weak_ordering weak_ordering::greater{static_cast<signed char>(1)};

#endif
} // namespace wistd
/// @endcond

namespace wil
{
//! Strictly a function of the file system but this is the value for all known file system, NTFS, FAT.
//! CDFs has a limit of 254.
constexpr size_t max_path_segment_length = 255;

//! Character length not including the null, MAX_PATH (260) includes the null.
constexpr size_t max_path_length = 259;

//! 32743 Character length not including the null. This is a system defined limit.
//! The 24 is for the expansion of the roots from "C:" to "\Device\HarddiskVolume4"
//! It will be 25 when there are more than 9 disks.
constexpr size_t max_extended_path_length = 0x7FFF - 24;

//! For {guid} string form. Includes space for the null terminator.
constexpr size_t guid_string_buffer_length = 39;

//! For {guid} string form. Not including the null terminator.
constexpr size_t guid_string_length = 38;

#pragma region String and identifier comparisons
// Using CompareStringOrdinal functions:
//
// Indentifiers require a locale-less (ordinal), and often case-insensitive, comparison (filenames, registry keys, XML node names,
// etc). DO NOT use locale-sensitive (lexical) comparisons for resource identifiers (e.g.wcs*() functions in the CRT).

#if WIL_USE_STL && (__cpp_lib_string_view >= 201606L)
/// @cond
namespace details
{
    [[nodiscard]] inline int CompareStringOrdinal(std::wstring_view left, std::wstring_view right, bool caseInsensitive) WI_NOEXCEPT
    {
        // Casting from size_t (unsigned) to int (signed) should be safe from overrun to a negative,
        // merely truncating the string.  CompareStringOrdinal should be resilient to negatives.
        return ::CompareStringOrdinal(
            left.data(), static_cast<int>(left.size()), right.data(), static_cast<int>(right.size()), caseInsensitive);
    }
} // namespace details
/// @endcond

[[nodiscard]] inline wistd::weak_ordering compare_string_ordinal(std::wstring_view left, std::wstring_view right, bool caseInsensitive) WI_NOEXCEPT
{
    switch (wil::details::CompareStringOrdinal(left, right, caseInsensitive))
    {
    case CSTR_LESS_THAN:
        return wistd::weak_ordering::less;
    case CSTR_GREATER_THAN:
        return wistd::weak_ordering::greater;
    default:
        return wistd::weak_ordering::equivalent;
    }
}
#endif

#pragma endregion

#pragma region FILETIME helpers
// FILETIME duration values. FILETIME is in 100 nanosecond units.
namespace filetime_duration
{
    long long const one_millisecond = 10000LL;
    long long const one_second = 10000000LL;
    long long const one_minute = 10000000LL * 60;        // 600000000    or 600000000LL
    long long const one_hour = 10000000LL * 60 * 60;     // 36000000000  or 36000000000LL
    long long const one_day = 10000000LL * 60 * 60 * 24; // 864000000000 or 864000000000LL
}; // namespace filetime_duration

namespace filetime
{
    constexpr unsigned long long to_int64(const FILETIME& ft) WI_NOEXCEPT
    {
#if __cpp_lib_bit_cast >= 201806L
        return std::bit_cast<unsigned long long>(ft);
#else
        // Cannot reinterpret_cast FILETIME* to unsigned long long*
        // due to alignment differences.
        return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) + ft.dwLowDateTime;
#endif
    }

    __WI_CONSTEXPR_BIT_CAST FILETIME from_int64(unsigned long long i64) WI_NOEXCEPT
    {
#if __cpp_lib_bit_cast >= 201806L
        return std::bit_cast<FILETIME>(i64);
#else
        static_assert(sizeof(i64) == sizeof(FILETIME), "sizes don't match");
        static_assert(__alignof(unsigned long long) >= __alignof(FILETIME), "alignment not compatible with type pun");
        return *reinterpret_cast<FILETIME*>(&i64);
#endif
    }

    __WI_CONSTEXPR_BIT_CAST FILETIME add(_In_ FILETIME const& ft, long long delta100ns) WI_NOEXCEPT
    {
        return from_int64(to_int64(ft) + delta100ns);
    }

    constexpr bool is_empty(const FILETIME& ft) WI_NOEXCEPT
    {
        return (ft.dwHighDateTime == 0) && (ft.dwLowDateTime == 0);
    }

    inline FILETIME get_system_time() WI_NOEXCEPT
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        return ft;
    }

    /// Convert time as units of 100 nanoseconds to milliseconds. Fractional milliseconds are truncated.
    constexpr unsigned long long convert_100ns_to_msec(unsigned long long time100ns) WI_NOEXCEPT
    {
        return time100ns / filetime_duration::one_millisecond;
    }

    /// Convert time as milliseconds to units of 100 nanoseconds.
    constexpr unsigned long long convert_msec_to_100ns(unsigned long long timeMsec) WI_NOEXCEPT
    {
        return timeMsec * filetime_duration::one_millisecond;
    }

#if (defined(_APISETREALTIME_) && (_WIN32_WINNT >= _WIN32_WINNT_WIN7)) || defined(WIL_DOXYGEN)
    /// Returns the current unbiased interrupt-time count, in units of 100 nanoseconds. The unbiased interrupt-time count does not
    /// include time the system spends in sleep or hibernation.
    ///
    /// This API avoids prematurely shortcircuiting timing loops due to system sleep/hibernation.
    ///
    /// This is equivalent to GetTickCount64() except it returns units of 100 nanoseconds instead of milliseconds, and it doesn't
    /// include time the system spends in sleep or hibernation.
    /// For example
    ///
    ///     start = GetTickCount64();
    ///     hibernate();
    ///     ...wake from hibernation 30 minutes later...;
    ///     elapsed = GetTickCount64() - start;
    ///     // elapsed = 30min
    ///
    /// Do the same using unbiased interrupt-time and elapsed is 0 (or nearly so).
    ///
    /// @note This is identical to QueryUnbiasedInterruptTime() but returns the value as a return value (rather than an out
    ///       parameter).
    /// @see https://msdn.microsoft.com/en-us/library/windows/desktop/ee662307(v=vs.85).aspx
    inline unsigned long long QueryUnbiasedInterruptTimeAs100ns() WI_NOEXCEPT
    {
        ULONGLONG now{};
        QueryUnbiasedInterruptTime(&now);
        return now;
    }

    /// Returns the current unbiased interrupt-time count, in units of milliseconds. The unbiased interrupt-time count does not
    /// include time the system spends in sleep or hibernation.
    /// @see QueryUnbiasedInterruptTimeAs100ns
    inline unsigned long long QueryUnbiasedInterruptTimeAsMSec() WI_NOEXCEPT
    {
        return convert_100ns_to_msec(QueryUnbiasedInterruptTimeAs100ns());
    }
#endif // _APISETREALTIME_
} // namespace filetime
#pragma endregion

#pragma region RECT helpers
template <typename rect_type>
constexpr auto rect_width(rect_type const& rect)
{
    return rect.right - rect.left;
}

template <typename rect_type>
constexpr auto rect_height(rect_type const& rect)
{
    return rect.bottom - rect.top;
}

template <typename rect_type>
constexpr auto rect_is_empty(rect_type const& rect)
{
    return (rect.left >= rect.right) || (rect.top >= rect.bottom);
}

template <typename rect_type, typename point_type>
constexpr auto rect_contains_point(rect_type const& rect, point_type const& point)
{
    return (point.x >= rect.left) && (point.x < rect.right) && (point.y >= rect.top) && (point.y < rect.bottom);
}

template <typename rect_type, typename length_type>
constexpr rect_type rect_from_size(length_type x, length_type y, length_type width, length_type height)
{
    rect_type rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + width;
    rect.bottom = y + height;
    return rect;
}
#pragma endregion

// Use to adapt Win32 APIs that take a fixed size buffer into forms that return
// an allocated buffer. Supports many types of string representation.
// See comments below on the expected behavior of the callback.
// Adjust stackBufferLength based on typical result sizes to optimize use and
// to test the boundary cases.
template <typename string_type, size_t stackBufferLength = 256>
HRESULT AdaptFixedSizeToAllocatedResult(string_type& result, wistd::function<HRESULT(PWSTR, size_t, size_t*)> callback) WI_NOEXCEPT
{
    details::string_maker<string_type> maker;

    wchar_t value[stackBufferLength]{};
    size_t valueLengthNeededWithNull{}; // callback returns the number of characters needed including the null terminator.
    RETURN_IF_FAILED_EXPECTED(callback(value, ARRAYSIZE(value), &valueLengthNeededWithNull));
    WI_ASSERT(valueLengthNeededWithNull > 0);
    if (valueLengthNeededWithNull <= ARRAYSIZE(value))
    {
        // Success case as described above, make() adds the space for the null.
        RETURN_IF_FAILED(maker.make(value, valueLengthNeededWithNull - 1));
    }
    else
    {
        // Did not fit in the stack allocated buffer, need to do 2 phase construction.
        // May need to loop more than once if external conditions cause the value to change.
        size_t bufferLength;
        do
        {
            bufferLength = valueLengthNeededWithNull;
            // bufferLength includes the null so subtract that as make() will add space for it.
            RETURN_IF_FAILED(maker.make(nullptr, bufferLength - 1));

            RETURN_IF_FAILED_EXPECTED(callback(maker.buffer(), bufferLength, &valueLengthNeededWithNull));
            WI_ASSERT(valueLengthNeededWithNull > 0);

            // If the value shrunk, then adjust the string to trim off the excess buffer.
            if (valueLengthNeededWithNull < bufferLength)
            {
                RETURN_IF_FAILED(maker.trim_at_existing_null(valueLengthNeededWithNull - 1));
            }
        } while (valueLengthNeededWithNull > bufferLength);
    }
    result = maker.release();
    return S_OK;
}

/** Expands the '%' quoted environment variables in 'input' using ExpandEnvironmentStringsW(); */
template <typename string_type, size_t stackBufferLength = 256>
HRESULT ExpandEnvironmentStringsW(_In_ PCWSTR input, string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            *valueLengthNeededWithNul = ::ExpandEnvironmentStringsW(input, value, static_cast<DWORD>(valueLength));
            RETURN_LAST_ERROR_IF(*valueLengthNeededWithNul == 0);
            return S_OK;
        });
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
/** Searches for a specified file in a specified path using ExpandEnvironmentStringsW(); */
template <typename string_type, size_t stackBufferLength = 256>
HRESULT SearchPathW(_In_opt_ PCWSTR path, _In_ PCWSTR fileName, _In_opt_ PCWSTR extension, string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            *valueLengthNeededWithNul = ::SearchPathW(path, fileName, extension, static_cast<DWORD>(valueLength), value, nullptr);

            if (*valueLengthNeededWithNul == 0)
            {
                // ERROR_FILE_NOT_FOUND is an expected return value for SearchPathW
                const HRESULT searchResult = HRESULT_FROM_WIN32(::GetLastError());
                RETURN_HR_IF_EXPECTED(searchResult, searchResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
                RETURN_IF_FAILED(searchResult);
            }

            // AdaptFixedSizeToAllocatedResult expects that the length will always include the NUL.
            // If the result is copied to the buffer, SearchPathW returns the length of copied string, WITHOUT the NUL.
            // If the buffer is too small to hold the result, SearchPathW returns the length of the required buffer WITH the nul.
            if (*valueLengthNeededWithNul < valueLength)
            {
                (*valueLengthNeededWithNul)++; // It fit, account for the null.
            }
            return S_OK;
        });
}

template <typename string_type, size_t stackBufferLength = 256>
HRESULT QueryFullProcessImageNameW(HANDLE processHandle, _In_ DWORD flags, string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            DWORD lengthToUse = static_cast<DWORD>(valueLength);
            BOOL const success = ::QueryFullProcessImageNameW(processHandle, flags, value, &lengthToUse);
            RETURN_LAST_ERROR_IF((success == FALSE) && (::GetLastError() != ERROR_INSUFFICIENT_BUFFER));

            // On success, return the amount used; on failure, try doubling
            *valueLengthNeededWithNul = success ? (static_cast<size_t>(lengthToUse) + 1) : (static_cast<size_t>(lengthToUse) * 2);
            return S_OK;
        });
}

/** Expands environment strings and checks path existence with SearchPathW */
template <typename string_type, size_t stackBufferLength = 256>
HRESULT ExpandEnvAndSearchPath(_In_ PCWSTR input, string_type& result) WI_NOEXCEPT
{
    wil::unique_cotaskmem_string expandedName;
    RETURN_IF_FAILED((wil::ExpandEnvironmentStringsW<string_type, stackBufferLength>(input, expandedName)));

    // ERROR_FILE_NOT_FOUND is an expected return value for SearchPathW
    const HRESULT searchResult = (wil::SearchPathW<string_type, stackBufferLength>(nullptr, expandedName.get(), nullptr, result));
    RETURN_HR_IF_EXPECTED(searchResult, searchResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
    RETURN_IF_FAILED(searchResult);

    return S_OK;
}
#endif

/** Looks up the environment variable 'key' and fails if it is not found. */
template <typename string_type, size_t initialBufferLength = 128>
inline HRESULT GetEnvironmentVariableW(_In_ PCWSTR key, string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, initialBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            // If the function succeeds, the return value is the number of characters stored in the buffer
            // pointed to by lpBuffer, not including the terminating null character.
            //
            // If lpBuffer is not large enough to hold the data, the return value is the buffer size, in
            // characters, required to hold the string and its terminating null character and the contents of
            // lpBuffer are undefined.
            //
            // If the function fails, the return value is zero. If the specified environment variable was not
            // found in the environment block, GetLastError returns ERROR_ENVVAR_NOT_FOUND.

            ::SetLastError(ERROR_SUCCESS);

            *valueLengthNeededWithNul = ::GetEnvironmentVariableW(key, value, static_cast<DWORD>(valueLength));
            RETURN_LAST_ERROR_IF_EXPECTED((*valueLengthNeededWithNul == 0) && (::GetLastError() != ERROR_SUCCESS));
            if (*valueLengthNeededWithNul < valueLength)
            {
                (*valueLengthNeededWithNul)++; // It fit, account for the null.
            }
            return S_OK;
        });
}

/** Looks up the environment variable 'key' and returns null if it is not found. */
template <typename string_type, size_t initialBufferLength = 128>
HRESULT TryGetEnvironmentVariableW(_In_ PCWSTR key, string_type& result) WI_NOEXCEPT
{
    const auto hr = wil::GetEnvironmentVariableW<string_type, initialBufferLength>(key, result);
    RETURN_HR_IF(hr, FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND)));
    return S_OK;
}

/** Retrieves the fully qualified path for the file containing the specified module loaded
by a given process. Note GetModuleFileNameExW is a macro.*/
template <typename string_type, size_t initialBufferLength = 128>
HRESULT GetModuleFileNameExW(_In_opt_ HANDLE process, _In_opt_ HMODULE module, string_type& path) WI_NOEXCEPT
{
    auto adapter = [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
        DWORD copiedCount{};
        size_t valueUsedWithNul{};
        bool copyFailed{};
        bool copySucceededWithNoTruncation{};
        if (process != nullptr)
        {
            // GetModuleFileNameExW truncates and provides no error or other indication it has done so.
            // The only way to be sure it didn't truncate is if it didn't need the whole buffer. The
            // count copied to the buffer includes the nul-character as well.
            copiedCount = ::GetModuleFileNameExW(process, module, value, static_cast<DWORD>(valueLength));
            valueUsedWithNul = static_cast<size_t>(copiedCount) + 1;
            copyFailed = (0 == copiedCount);
            copySucceededWithNoTruncation = !copyFailed && (copiedCount < valueLength - 1);
        }
        else
        {
            // In cases of insufficient buffer, GetModuleFileNameW will return a value equal to lengthWithNull
            // and set the last error to ERROR_INSUFFICIENT_BUFFER. The count returned does not include
            // the nul-character
            copiedCount = ::GetModuleFileNameW(module, value, static_cast<DWORD>(valueLength));
            valueUsedWithNul = static_cast<size_t>(copiedCount) + 1;
            copyFailed = (0 == copiedCount);
            copySucceededWithNoTruncation = !copyFailed && (copiedCount < valueLength);
        }

        RETURN_LAST_ERROR_IF(copyFailed);

        // When the copy truncated, request another try with more space.
        *valueLengthNeededWithNul = copySucceededWithNoTruncation ? valueUsedWithNul : (valueLength * 2);

        return S_OK;
    };

    return wil::AdaptFixedSizeToAllocatedResult<string_type, initialBufferLength>(path, wistd::move(adapter));
}

/** Retrieves the fully qualified path for the file that contains the specified module.
The module must have been loaded by the current process. The path returned will use the
same format that was specified when the module was loaded. Therefore, the path can be a
long or short file name, and can have the prefix '\\?\'. */
template <typename string_type, size_t initialBufferLength = 128>
HRESULT GetModuleFileNameW(HMODULE module, string_type& path) WI_NOEXCEPT
{
    return wil::GetModuleFileNameExW<string_type, initialBufferLength>(nullptr, module, path);
}

template <typename string_type, size_t stackBufferLength = 256>
HRESULT GetSystemDirectoryW(string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            *valueLengthNeededWithNul = ::GetSystemDirectoryW(value, static_cast<DWORD>(valueLength));
            RETURN_LAST_ERROR_IF(*valueLengthNeededWithNul == 0);
            if (*valueLengthNeededWithNul < valueLength)
            {
                (*valueLengthNeededWithNul)++; // it fit, account for the null
            }
            return S_OK;
        });
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
template <typename string_type, size_t stackBufferLength = 256>
HRESULT GetWindowsDirectoryW(string_type& result) WI_NOEXCEPT
{
    return wil::AdaptFixedSizeToAllocatedResult<string_type, stackBufferLength>(
        result, [&](_Out_writes_(valueLength) PWSTR value, size_t valueLength, _Out_ size_t* valueLengthNeededWithNul) -> HRESULT {
            *valueLengthNeededWithNul = ::GetWindowsDirectoryW(value, static_cast<DWORD>(valueLength));
            RETURN_LAST_ERROR_IF(*valueLengthNeededWithNul == 0);
            if (*valueLengthNeededWithNul < valueLength)
            {
                (*valueLengthNeededWithNul)++; // it fit, account for the null
            }
            return S_OK;
        });
}
#endif

#ifdef WIL_ENABLE_EXCEPTIONS
/** Expands the '%' quoted environment variables in 'input' using ExpandEnvironmentStringsW(); */
template <typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
string_type ExpandEnvironmentStringsW(_In_ PCWSTR input)
{
    string_type result{};
    THROW_IF_FAILED((wil::ExpandEnvironmentStringsW<string_type, stackBufferLength>(input, result)));
    return result;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
/** Searches for a specified file in a specified path using SearchPathW*/
template <typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
string_type TrySearchPathW(_In_opt_ PCWSTR path, _In_ PCWSTR fileName, PCWSTR _In_opt_ extension)
{
    string_type result{};
    HRESULT searchHR = wil::SearchPathW<string_type, stackBufferLength>(path, fileName, extension, result);
    THROW_HR_IF(searchHR, FAILED(searchHR) && (searchHR != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)));
    return result;
}
#endif

/** Looks up the environment variable 'key' and fails if it is not found. */
template <typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 128>
string_type GetEnvironmentVariableW(_In_ PCWSTR key)
{
    string_type result{};
    THROW_IF_FAILED((wil::GetEnvironmentVariableW<string_type, initialBufferLength>(key, result)));
    return result;
}

/** Looks up the environment variable 'key' and returns null if it is not found. */
template <typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 128>
string_type TryGetEnvironmentVariableW(_In_ PCWSTR key)
{
    string_type result{};
    THROW_IF_FAILED((wil::TryGetEnvironmentVariableW<string_type, initialBufferLength>(key, result)));
    return result;
}

template <typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 128>
string_type GetModuleFileNameW(HMODULE module = nullptr /* current process module */)
{
    string_type result{};
    THROW_IF_FAILED((wil::GetModuleFileNameW<string_type, initialBufferLength>(module, result)));
    return result;
}

template <typename string_type = wil::unique_cotaskmem_string, size_t initialBufferLength = 128>
string_type GetModuleFileNameExW(HANDLE process, HMODULE module)
{
    string_type result{};
    THROW_IF_FAILED((wil::GetModuleFileNameExW<string_type, initialBufferLength>(process, module, result)));
    return result;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
template <typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
string_type GetWindowsDirectoryW()
{
    string_type result;
    THROW_IF_FAILED((wil::GetWindowsDirectoryW<string_type, stackBufferLength>(result)));
    return result;
}
#endif

template <typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
string_type GetSystemDirectoryW()
{
    string_type result;
    THROW_IF_FAILED((wil::GetSystemDirectoryW<string_type, stackBufferLength>(result)));
    return result;
}

template <typename string_type = wil::unique_cotaskmem_string, size_t stackBufferLength = 256>
string_type QueryFullProcessImageNameW(HANDLE processHandle = GetCurrentProcess(), DWORD flags = 0)
{
    string_type result{};
    THROW_IF_FAILED((wil::QueryFullProcessImageNameW<string_type, stackBufferLength>(processHandle, flags, result)));
    return result;
}

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

// Lookup a DWORD value under HKLM\...\Image File Execution Options\<current process name>
inline DWORD GetCurrentProcessExecutionOption(PCWSTR valueName, DWORD defaultValue = 0)
{
    auto filePath = wil::GetModuleFileNameW<wil::unique_cotaskmem_string>();
    if (auto lastSlash = wcsrchr(filePath.get(), L'\\'))
    {
        const auto fileName = lastSlash + 1;
        auto keyPath = wil::str_concat<wil::unique_cotaskmem_string>(
            LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\)", fileName);
        DWORD value{}, sizeofValue = sizeof(value);
        if (::RegGetValueW(
                HKEY_LOCAL_MACHINE,
                keyPath.get(),
                valueName,
#ifdef RRF_SUBKEY_WOW6464KEY
                RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
#else
                RRF_RT_REG_DWORD,
#endif
                nullptr,
                &value,
                &sizeofValue) == ERROR_SUCCESS)
        {
            return value;
        }
    }
    return defaultValue;
}

#ifndef DebugBreak // Some code defines 'DebugBreak' to garbage to force build breaks in release builds
// Waits for a debugger to attach to the current process based on registry configuration.
//
// Example:
//     HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\explorer.exe
//         WaitForDebuggerPresent=1
//
// REG_DWORD value of
//     missing or 0 -> don't break
//     1 -> wait for the debugger, continue execution once it is attached
//     2 -> wait for the debugger, break here once attached.
inline void WaitForDebuggerPresent(bool checkRegistryConfig = true)
{
    for (;;)
    {
        auto configValue = checkRegistryConfig ? GetCurrentProcessExecutionOption(L"WaitForDebuggerPresent") : 1;
        if (configValue == 0)
        {
            return; // not configured, don't wait
        }

        if (IsDebuggerPresent())
        {
            if (configValue == 2)
            {
                DebugBreak(); // debugger attached, SHIFT+F11 to return to the caller
            }
            return; // debugger now attached, continue executing
        }
        Sleep(500);
    }
}
#endif
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)

#endif

/** Retrieve the HINSTANCE for the current DLL or EXE using this symbol that
the linker provides for every module. This avoids the need for a global HINSTANCE variable
and provides access to this value for static libraries. */
inline HINSTANCE GetModuleInstanceHandle() WI_NOEXCEPT
{
    return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

// GetModuleHandleExW was added to the app partition in version 22000 of the SDK
#if defined(NTDDI_WIN10_CO) ? WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES) \
                            : WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM | WINAPI_PARTITION_GAMES)
// Use this in threads that can outlive the object or API call that created them.
// Without this COM, or the API caller, can unload the DLL, resulting in a crash.
// It is very important that this be the first object created in the thread proc
// as when this runs down the thread exits and no destructors of objects created before
// it will run.
[[nodiscard]] inline auto get_module_reference_for_thread() noexcept
{
    HMODULE thisModule{};
    FAIL_FAST_IF(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, L"", &thisModule));
    return wil::scope_exit([thisModule] {
        FreeLibraryAndExitThread(thisModule, 0);
    });
}
#endif

/// @cond
namespace details
{
    class init_once_completer
    {
        INIT_ONCE& m_once;
        unsigned long m_flags = INIT_ONCE_INIT_FAILED;

    public:
        init_once_completer(_In_ INIT_ONCE& once) WI_NOEXCEPT : m_once(once)
        {
        }

#pragma warning(push)
#pragma warning(disable : 4702) // https://github.com/Microsoft/wil/issues/2
        void success() WI_NOEXCEPT
        {
            m_flags = 0;
        }
#pragma warning(pop)

        ~init_once_completer() WI_NOEXCEPT
        {
            ::InitOnceComplete(&m_once, m_flags, nullptr);
        }
    };
} // namespace details
/// @endcond

/** Performs one-time initialization
Simplifies using the Win32 INIT_ONCE structure to perform one-time initialization. The provided `func` is invoked
at most once.
~~~~
INIT_ONCE g_init{};
ComPtr<IFoo> g_foo;
HRESULT MyMethod()
{
    bool winner = false;
    RETURN_IF_FAILED(wil::init_once_nothrow(g_init, []
    {
        ComPtr<IFoo> foo;
        RETURN_IF_FAILED(::CoCreateInstance(..., IID_PPV_ARGS(&foo));
        RETURN_IF_FAILED(foo->Startup());
        g_foo = foo;
    }, &winner);
    if (winner)
    {
        RETURN_IF_FAILED(g_foo->Another());
    }
    return S_OK;
}
~~~~
See MSDN for more information on `InitOnceExecuteOnce`.
@param initOnce The INIT_ONCE structure to use as context for initialization.
@param func A function that will be invoked to perform initialization. If this fails, the init call
        fails and the once-init is not marked as initialized. A later caller could attempt to
        initialize it a second time.
@param callerCompleted Set to 'true' if this was the call that caused initialization, false otherwise.
*/
template <typename T>
HRESULT init_once_nothrow(_Inout_ INIT_ONCE& initOnce, T func, _Out_opt_ bool* callerCompleted = nullptr) WI_NOEXCEPT
{
    BOOL pending = FALSE;
    wil::assign_to_opt_param(callerCompleted, false);

    __WIL_PRIVATE_RETURN_IF_WIN32_BOOL_FALSE(InitOnceBeginInitialize(&initOnce, 0, &pending, nullptr));

    if (pending)
    {
        details::init_once_completer completion(initOnce);
        __WIL_PRIVATE_RETURN_IF_FAILED(func());
        completion.success();
        wil::assign_to_opt_param(callerCompleted, true);
    }

    return S_OK;
}

//! Similar to init_once_nothrow, but fails-fast if the initialization step failed. The 'callerComplete' value is
//! returned to the caller instead of being an out-parameter.
template <typename T>
bool init_once_failfast(_Inout_ INIT_ONCE& initOnce, T&& func) WI_NOEXCEPT
{
    bool callerCompleted;

    FAIL_FAST_IF_FAILED(init_once_nothrow(initOnce, wistd::forward<T>(func), &callerCompleted));

    return callerCompleted;
};

//! Returns 'true' if this `init_once` structure has finished initialization, false otherwise.
inline bool init_once_initialized(_Inout_ INIT_ONCE& initOnce) WI_NOEXCEPT
{
    BOOL pending = FALSE;
    return ::InitOnceBeginInitialize(&initOnce, INIT_ONCE_CHECK_ONLY, &pending, nullptr) && !pending;
}

#ifdef WIL_ENABLE_EXCEPTIONS
/** Performs one-time initialization
Simplifies using the Win32 INIT_ONCE structure to perform one-time initialization. The provided `func` is invoked
at most once.
~~~~
INIT_ONCE g_init{};
ComPtr<IFoo> g_foo;
void MyMethod()
{
    bool winner = wil::init_once(g_init, []
    {
        ComPtr<IFoo> foo;
        THROW_IF_FAILED(::CoCreateInstance(..., IID_PPV_ARGS(&foo));
        THROW_IF_FAILED(foo->Startup());
        g_foo = foo;
    });
    if (winner)
    {
        THROW_IF_FAILED(g_foo->Another());
    }
}
~~~~
See MSDN for more information on `InitOnceExecuteOnce`.
@param initOnce The INIT_ONCE structure to use as context for initialization.
@param func A function that will be invoked to perform initialization. If this fails, the init call
        fails and the once-init is not marked as initialized. A later caller could attempt to
        initialize it a second time.
@returns 'true' if this was the call that caused initialization, false otherwise.
*/
template <typename T>
bool init_once(_Inout_ INIT_ONCE& initOnce, T func)
{
    BOOL pending = FALSE;

    THROW_IF_WIN32_BOOL_FALSE(::InitOnceBeginInitialize(&initOnce, 0, &pending, nullptr));

    if (pending)
    {
        details::init_once_completer completion(initOnce);
        func();
        completion.success();
        return true;
    }
    else
    {
        return false;
    }
}
#endif // WIL_ENABLE_EXCEPTIONS

#if WIL_USE_STL && defined(WIL_ENABLE_EXCEPTIONS) && (__cpp_lib_string_view >= 201606L)
/// @cond
namespace details
{
    template <typename RangeT>
    struct deduce_char_type_from_string_range
    {
        template <typename T = RangeT>
        static auto deduce(T& range)
        {
            using std::begin;
            return (*begin(range))[0];
        }

        using type = decltype(deduce(wistd::declval<RangeT&>()));
    };

    template <typename RangeT>
    using deduce_char_type_from_string_range_t = typename deduce_char_type_from_string_range<RangeT>::type;

    // Internal helper span-like type for passing arrays as an iterable collection
    template <typename T>
    struct iterable_span
    {
        T* pointer;
        size_t size;

        constexpr T* begin() const noexcept
        {
            return pointer;
        }

        constexpr T* end() const noexcept
        {
            return pointer + size;
        }
    };
} // namespace details
/// @endcond

//! Flags that control the behavior of `ArgvToCommandLine`
enum class ArgvToCommandLineFlags : uint8_t
{
    None = 0,

    //! By default, arguments are only surrounded by quotes when necessary (i.e. the argument contains a space). When
    //! this flag is specified, all arguments are surrounded with quotes, regardless of whether or not they contain
    //! space(s). This is an optimization as it means that we are not required to search for spaces in each argument and
    //! we don't need to potentially go back and insert a quotation character in the middle of the string after we've
    //! already written part of the argument to the result. That said, wrapping all arguments with quotes can have some
    //! adverse effects with some applications, most notably cmd.exe, which do their own command line processing.
    ForceQuotes = 0x01 << 0,

    //! `CommandLineToArgvW` has an "optimization" that assumes that the first argument is the path to an executable.
    //! Because valid NTFS paths cannot contain quotation characters, `CommandLineToArgvW` disables its quote escaping
    //! logic for the first argument. By default, `ArgvToCommandLine` aims to ensure that an argv array can "round trip"
    //! to a string and back, meaning it tries to replicate this logic in reverse. This flag disables this logic and
    //! escapes backslashes and quotes the same for all arguments, including the first one. This is useful if the output
    //! string is only an intermediate command line string (e.g. if the executable path is prepended later).
    FirstArgumentIsNotPath = 0x01 << 1,
};
DEFINE_ENUM_FLAG_OPERATORS(ArgvToCommandLineFlags);

//! Performs the reverse operation of CommandLineToArgvW.
//! Converts an argv array to a command line string that is guaranteed to "round trip" with CommandLineToArgvW. That is,
//! a call to wil::ArgvToCommandLine followed by a call to ArgvToCommandLineW will produce the same array that was
//! passed to wil::ArgvToCommandLine. Note that the reverse is not true. I.e. calling ArgvToCommandLineW followed by
//! wil::ArgvToCommandLine will not produce the original string due to the optionality of of quotes in the command line
//! string. This functionality is useful in a number of scenarios, most notably:
//!     1.  When implementing a "driver" application. That is, an application that consumes some command line arguments,
//!         translates others into new arguments, and preserves the rest, "forwarding" the resulting command line to a
//!         separate application.
//!     2.  When reading command line arguments from some data storage, e.g. from a JSON array, which then need to get
//!         compiled into a command line string that's used for creating a new process.
//! Unlike CommandLineToArgvW, this function accepts both "narrow" and "wide" strings to support calling both
//! CreateProcessW and CreateProcessA with the result. See the values in @ref wil::ArgvToCommandLineFlags for more
//! information on how to control the behavior of this function as well as scenarios when you may want to use each one.
template <typename RangeT, typename CharT = details::deduce_char_type_from_string_range_t<RangeT>>
inline std::basic_string<CharT> ArgvToCommandLine(RangeT&& range, ArgvToCommandLineFlags flags = ArgvToCommandLineFlags::None)
{
    using string_type = std::basic_string<CharT>;
    using string_view_type = std::basic_string_view<CharT>;

    // Somewhat of a hack to avoid the fact that we can't conditionalize a string literal on a template
    static constexpr const CharT empty_string[] = {'\0'};
    static constexpr const CharT single_quote_string[] = {'"', '\0'};
    static constexpr const CharT space_string[] = {' ', '\0'};
    static constexpr const CharT quoted_space_string[] = {'"', ' ', '"', '\0'};

    static constexpr const CharT search_string_no_space[] = {'\\', '"', '\0'};
    static constexpr const CharT search_string_with_space[] = {'\\', '"', ' ', '\t', '\0'};

    const bool forceQuotes = WI_IsFlagSet(flags, ArgvToCommandLineFlags::ForceQuotes);
    const CharT* const initialSearchString = forceQuotes ? search_string_no_space : search_string_with_space;
    const CharT* prefix = forceQuotes ? single_quote_string : empty_string;
    const CharT* const nextPrefix = forceQuotes ? quoted_space_string : space_string;

    string_type result;
    int index = 0;
    for (auto&& strRaw : range)
    {
        auto currentIndex = index++;
        result += prefix;

        const CharT* searchString = initialSearchString;

        // Info just in case we need to come back and insert quotes
        auto startPos = result.size();
        bool terminateWithQuotes = false; // With forceQuotes == true, this is baked into the prefix

        // We need to escape any quotes and CONDITIONALLY any backslashes
        string_view_type str(strRaw);
        size_t pos = 0;
        while (pos < str.size())
        {
            auto nextPos = str.find_first_of(searchString, pos);
            if ((nextPos != str.npos) && ((str[nextPos] == ' ') || (str[nextPos] == '\t')))
            {
                // Insert the quote now since we'll need to otherwise stomp over data we're about to write
                // NOTE: By updating the search string here, we don't need to worry about manually inserting the
                // character later since we'll just include it in our next iteration
                WI_ASSERT(!forceQuotes);               // Otherwise, shouldn't be part of our search string
                searchString = search_string_no_space; // We're already adding a quote; don't do it again
                result.insert(startPos, 1, '"');
                terminateWithQuotes = true;
            }

            result.append(str, pos, nextPos - pos);
            pos = nextPos;
            if (pos == str.npos)
                break;

            if (str[pos] == '"')
            {
                // Kinda easy case; just escape the quotes, *unless* this is the first argument and we assume a path
                if ((currentIndex > 0) || WI_IsFlagSet(flags, ArgvToCommandLineFlags::FirstArgumentIsNotPath))
                {
                    result.append({'\\', '"'}); // Escape case
                }
                else
                {
                    // Realistically, this likely signals a bug since paths cannot contain quotes. That said, the
                    // behavior of CommandLineToArgvW is to just preserve "interior" quotes, so we do that.
                    // NOTE: 'CommandLineToArgvW' treats "interior" quotes as terminating quotes when the executable
                    // path begins with a quote, even if the next character is not a space. This assert won't catch all
                    // of such issues as we may detect a space, and therefore the need to surroud the argument with
                    // quotes, later in the string; this is best effort. Such arguments wouldn't be valid and are not
                    // representable anyway
                    WI_ASSERT((pos > 0) && !WI_IsFlagSet(flags, ArgvToCommandLineFlags::ForceQuotes) && !terminateWithQuotes);
                    result.push_back('"'); // Not escaping case
                }
                ++pos; // Skip past quote on next search
            }
            else if (str[pos] == '\\')
            {
                // More complex case... Only need to escape if followed by 0+ backslashes and then either a quote or
                // the end of the string and we're adding quotes
                nextPos = str.find_first_not_of(L'\\', pos);

                // NOTE: This is an optimization taking advantage of the fact that doing a double append of 1+
                // backslashes will be functionally equivalent to escaping each one. This copies all of the backslashes
                // once. We _might_ do it again later
                result.append(str, pos, nextPos - pos);

                // If this is the first argument and is being interpreted as a path, we never escape slashes
                if ((currentIndex == 0) && !WI_IsFlagSet(flags, ArgvToCommandLineFlags::FirstArgumentIsNotPath))
                {
                    pos = nextPos;
                    continue;
                }

                if ((nextPos != str.npos) && (str[nextPos] != L'"'))
                {
                    // Simplest case... don't need to escape when followed by a non-quote character
                    pos = nextPos;
                    continue;
                }

                // If this is the end of the string and we're not appending a quotation to the end, we're in the
                // same boat as the above where we don't need to escape
                if ((nextPos == str.npos) && !forceQuotes && !terminateWithQuotes)
                {
                    pos = nextPos;
                    continue;
                }

                // Otherwise, we need to escape all backslashes. See above; this can be done with another append
                result.append(str, pos, nextPos - pos);
                pos = nextPos;
                if (pos != str.npos)
                {
                    // Must be followed by a quote; make sure we escape it, too. NOTE: We should have already early
                    // exited if this argument is being interpreted as an executable path
                    WI_ASSERT(str[pos] == '"');
                    result.append({'\\', '"'});
                    ++pos;
                }
            }
            else
            {
                // Otherwise space, which we handled above
                WI_ASSERT((str[pos] == ' ') || (str[pos] == '\t'));
            }
        }

        if (terminateWithQuotes)
        {
            result.push_back('"');
        }

        prefix = nextPrefix;
    }

    // NOTE: We optimize the force quotes case by including them in the prefix string. We're not appending a prefix
    // anymore, so we need to make sure we close off the string
    if (forceQuotes)
    {
        result.push_back(L'\"');
    }

    return result;
}

template <typename CharT>
inline std::basic_string<wistd::remove_cv_t<CharT>> ArgvToCommandLine(
    int argc, CharT* const* argv, ArgvToCommandLineFlags flags = ArgvToCommandLineFlags::None)
{
    return ArgvToCommandLine(details::iterable_span<CharT* const>{argv, static_cast<size_t>(argc)}, flags);
}
#endif
} // namespace wil

// Macro for calling GetProcAddress(), with type safety for C++ clients
// using the type information from the specified function.
// The return value is automatically cast to match the function prototype of the input function.
//
// Sample usage:
//
// auto sendMail = GetProcAddressByFunctionDeclaration(hinstMAPI, MAPISendMailW);
// if (sendMail)
// {
//    sendMail(0, 0, pmm, MAPI_USE_DEFAULT, 0);
// }
//  Declaration
#define GetProcAddressByFunctionDeclaration(hinst, fn) reinterpret_cast<decltype(::fn)*>(GetProcAddress(hinst, #fn))

#endif // __WIL_WIN32_HELPERS_INCLUDED
