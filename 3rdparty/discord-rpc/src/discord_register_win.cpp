#include "discord_rpc.h"
#include "discord_register.h"

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME
#include <windows.h>
#include <psapi.h>
#include <cstdio>

/**
 * Updated fixes for MinGW and WinXP
 * This block is written the way it does not involve changing the rest of the code
 * Checked to be compiling
 * 1) strsafe.h belongs to Windows SDK and cannot be added to MinGW
 * #include guarded, functions redirected to <string.h> substitutes
 * 2) RegSetKeyValueW and LSTATUS are not declared in <winreg.h>
 * The entire function is rewritten
 */
#ifdef __MINGW32__
#include <wchar.h>
/// strsafe.h fixes
static HRESULT StringCbPrintfW(LPWSTR pszDest, size_t cbDest, LPCWSTR pszFormat, ...)
{
    HRESULT ret;
    va_list va;
    va_start(va, pszFormat);
    cbDest /= 2; // Size is divided by 2 to convert from bytes to wide characters - causes segfault
                 // othervise
    ret = vsnwprintf(pszDest, cbDest, pszFormat, va);
    pszDest[cbDest - 1] = 0; // Terminate the string in case a buffer overflow; -1 will be returned
    va_end(va);
    return ret;
}
#else
#include <cwchar>
#include <strsafe.h>
#endif // __MINGW32__

/// winreg.h fixes
#ifndef LSTATUS
#define LSTATUS LONG
#endif
#ifdef RegSetKeyValueW
#undefine RegSetKeyValueW
#endif
#define RegSetKeyValueW regset
static LSTATUS regset(HKEY hkey,
                      LPCWSTR subkey,
                      LPCWSTR name,
                      DWORD type,
                      const void* data,
                      DWORD len)
{
    HKEY htkey = hkey, hsubkey = nullptr;
    LSTATUS ret;
    if (subkey && subkey[0]) {
        if ((ret = RegCreateKeyExW(hkey, subkey, 0, 0, 0, KEY_ALL_ACCESS, 0, &hsubkey, 0)) !=
            ERROR_SUCCESS)
            return ret;
        htkey = hsubkey;
    }
    ret = RegSetValueExW(htkey, name, 0, type, (const BYTE*)data, len);
    if (hsubkey && hsubkey != hkey)
        RegCloseKey(hsubkey);
    return ret;
}

static void Discord_RegisterW(const wchar_t* applicationId, const wchar_t* command)
{
    // https://msdn.microsoft.com/en-us/library/aa767914(v=vs.85).aspx
    // we want to register games so we can run them as discord-<appid>://
    // Update the HKEY_CURRENT_USER, because it doesn't seem to require special permissions.

    wchar_t exeFilePath[MAX_PATH];
    DWORD exeLen = GetModuleFileNameW(nullptr, exeFilePath, MAX_PATH);
    wchar_t openCommand[1024];

    if (command && command[0]) {
        StringCbPrintfW(openCommand, sizeof(openCommand), L"%s", command);
    }
    else {
        // StringCbCopyW(openCommand, sizeof(openCommand), exeFilePath);
        StringCbPrintfW(openCommand, sizeof(openCommand), L"%s", exeFilePath);
    }

    wchar_t protocolName[64];
    StringCbPrintfW(protocolName, sizeof(protocolName), L"discord-%s", applicationId);
    wchar_t protocolDescription[128];
    StringCbPrintfW(
      protocolDescription, sizeof(protocolDescription), L"URL:Run game %s protocol", applicationId);
    wchar_t urlProtocol = 0;

    wchar_t keyName[256];
    StringCbPrintfW(keyName, sizeof(keyName), L"Software\\Classes\\%s", protocolName);
    HKEY key;
    auto status =
      RegCreateKeyExW(HKEY_CURRENT_USER, keyName, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "Error creating key\n");
        return;
    }
    DWORD len;
    LSTATUS result;
    len = (DWORD)lstrlenW(protocolDescription) + 1;
    result =
      RegSetKeyValueW(key, nullptr, nullptr, REG_SZ, protocolDescription, len * sizeof(wchar_t));
    if (FAILED(result)) {
        fprintf(stderr, "Error writing description\n");
    }

    len = (DWORD)lstrlenW(protocolDescription) + 1;
    result = RegSetKeyValueW(key, nullptr, L"URL Protocol", REG_SZ, &urlProtocol, sizeof(wchar_t));
    if (FAILED(result)) {
        fprintf(stderr, "Error writing description\n");
    }

    result = RegSetKeyValueW(
      key, L"DefaultIcon", nullptr, REG_SZ, exeFilePath, (exeLen + 1) * sizeof(wchar_t));
    if (FAILED(result)) {
        fprintf(stderr, "Error writing icon\n");
    }

    len = (DWORD)lstrlenW(openCommand) + 1;
    result = RegSetKeyValueW(
      key, L"shell\\open\\command", nullptr, REG_SZ, openCommand, len * sizeof(wchar_t));
    if (FAILED(result)) {
        fprintf(stderr, "Error writing command\n");
    }
    RegCloseKey(key);
}

extern "C" DISCORD_EXPORT void Discord_Register(const char* applicationId, const char* command)
{
    wchar_t appId[32];
    MultiByteToWideChar(CP_UTF8, 0, applicationId, -1, appId, 32);

    wchar_t openCommand[1024];
    const wchar_t* wcommand = nullptr;
    if (command && command[0]) {
        const auto commandBufferLen = sizeof(openCommand) / sizeof(*openCommand);
        MultiByteToWideChar(CP_UTF8, 0, command, -1, openCommand, commandBufferLen);
        wcommand = openCommand;
    }

    Discord_RegisterW(appId, wcommand);
}

extern "C" DISCORD_EXPORT void Discord_RegisterSteamGame(const char* applicationId,
                                                         const char* steamId)
{
    wchar_t appId[32];
    MultiByteToWideChar(CP_UTF8, 0, applicationId, -1, appId, 32);

    wchar_t wSteamId[32];
    MultiByteToWideChar(CP_UTF8, 0, steamId, -1, wSteamId, 32);

    HKEY key;
    auto status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) {
        fprintf(stderr, "Error opening Steam key\n");
        return;
    }

    wchar_t steamPath[MAX_PATH];
    DWORD pathBytes = sizeof(steamPath);
    status = RegQueryValueExW(key, L"SteamExe", nullptr, nullptr, (BYTE*)steamPath, &pathBytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || pathBytes < 1) {
        fprintf(stderr, "Error reading SteamExe key\n");
        return;
    }

    DWORD pathChars = pathBytes / sizeof(wchar_t);
    for (DWORD i = 0; i < pathChars; ++i) {
        if (steamPath[i] == L'/') {
            steamPath[i] = L'\\';
        }
    }

    wchar_t command[1024];
    StringCbPrintfW(command, sizeof(command), L"\"%s\" steam://rungameid/%s", steamPath, wSteamId);

    Discord_RegisterW(appId, command);
}
