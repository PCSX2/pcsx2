#include "RA_Interface.h"

#include <winhttp.h>
#include <cassert>
#include <stdexcept>
#include <string>

#ifndef RA_NOPROGRESS
 #include <ShlObj.h>
#else
 #define IProgressDialog void
#endif

#ifndef CCONV
#define CCONV __cdecl
#endif

#if defined(_M_X64) || defined(__amd64__)
 #define RA_X64
 #define RA_INT_DLL L"RA_Integration-x64.dll"
#else
 #define RA_INT_DLL L"RA_Integration.dll"
#endif

// Initialization
static const char*  (CCONV* _RA_IntegrationVersion)() = nullptr;
static const char*  (CCONV* _RA_HostName)() = nullptr;
static const char*  (CCONV* _RA_HostUrl)() = nullptr;
static int          (CCONV* _RA_InitI)(HWND hMainWnd, int nConsoleID, const char* sClientVer) = nullptr;
static int          (CCONV* _RA_InitOffline)(HWND hMainWnd, int nConsoleID, const char* sClientVer) = nullptr;
static int          (CCONV* _RA_InitClient)(HWND hMainWnd, const char* sClientName, const char* sClientVer) = nullptr;
static int          (CCONV* _RA_InitClientOffline)(HWND hMainWnd, const char* sClientName, const char* sClientVer) = nullptr;
static void         (CCONV* _RA_InstallSharedFunctions)(int(*)(), void(*)(), void(*)(), void(*)(), void(*)(char*), void(*)(), void(*)(const char*)) = nullptr;
static void         (CCONV* _RA_SetForceRepaint)(int bEnable) = nullptr;
static HMENU        (CCONV* _RA_CreatePopupMenu)() = nullptr;
static int          (CCONV* _RA_GetPopupMenuItems)(RA_MenuItem*) = nullptr;
static void         (CCONV* _RA_InvokeDialog)(LPARAM nID) = nullptr;
static void         (CCONV* _RA_SetUserAgentDetail)(const char* sDetail);
static void         (CCONV* _RA_AttemptLogin)(int bBlocking) = nullptr;
static int          (CCONV* _RA_SetConsoleID)(unsigned int nConsoleID) = nullptr;
static void         (CCONV* _RA_ClearMemoryBanks)() = nullptr;
static void         (CCONV* _RA_InstallMemoryBank)(int nBankID, RA_ReadMemoryFunc* pReader, RA_WriteMemoryFunc* pWriter, int nBankSize) = nullptr;
static void         (CCONV* _RA_InstallMemoryBankBlockReader)(int nBankID, RA_ReadMemoryBlockFunc* pReader) = nullptr;
static int          (CCONV* _RA_Shutdown)() = nullptr;
// Overlay
static int          (CCONV* _RA_IsOverlayFullyVisible)() = nullptr;
static void         (CCONV* _RA_SetPaused)(int bIsPaused) = nullptr;
static void         (CCONV* _RA_NavigateOverlay)(ControllerInput* pInput) = nullptr;
static void         (CCONV* _RA_UpdateHWnd)(HWND hMainHWND);
// Game Management
static unsigned int (CCONV* _RA_IdentifyRom)(const BYTE* pROM, unsigned int nROMSize) = nullptr;
static unsigned int (CCONV* _RA_IdentifyHash)(const char* sHash) = nullptr;
static void         (CCONV* _RA_ActivateGame)(unsigned int nGameId) = nullptr;
static int          (CCONV* _RA_OnLoadNewRom)(const BYTE* pROM, unsigned int nROMSize) = nullptr;
static int          (CCONV* _RA_ConfirmLoadNewRom)(int bQuitting) = nullptr;
// Runtime Functionality
static void         (CCONV* _RA_DoAchievementsFrame)() = nullptr;
static void         (CCONV* _RA_SuspendRepaint)() = nullptr;
static void         (CCONV* _RA_ResumeRepaint)() = nullptr;
static void         (CCONV* _RA_UpdateAppTitle)(const char* pMessage) = nullptr;
static const char*  (CCONV* _RA_UserName)() = nullptr;
static int          (CCONV* _RA_HardcoreModeIsActive)(void) = nullptr;
static int          (CCONV* _RA_WarnDisableHardcore)(const char* sActivity) = nullptr;
static void         (CCONV* _RA_OnReset)() = nullptr;
static void         (CCONV* _RA_OnSaveState)(const char* sFilename) = nullptr;
static void         (CCONV* _RA_OnLoadState)(const char* sFilename) = nullptr;
static int          (CCONV* _RA_CaptureState)(char* pBuffer, int nBufferSize) = nullptr;
static void         (CCONV* _RA_RestoreState)(const char* pBuffer) = nullptr;

static HINSTANCE g_hRADLL = nullptr;

void RA_AttemptLogin(int bBlocking)
{
    if (_RA_AttemptLogin != nullptr)
        _RA_AttemptLogin(bBlocking);
}

const char* RA_UserName(void)
{
    if (_RA_UserName != nullptr)
        return _RA_UserName();

    return "";
}

void RA_NavigateOverlay(ControllerInput* pInput)
{
    if (_RA_NavigateOverlay != nullptr)
        _RA_NavigateOverlay(pInput);
}

void RA_UpdateRenderOverlay(HDC hDC, ControllerInput* pInput, float fDeltaTime, RECT* prcSize, bool Full_Screen, bool Paused)
{
    if (_RA_NavigateOverlay != nullptr)
        _RA_NavigateOverlay(pInput);
}

int RA_IsOverlayFullyVisible(void)
{
    if (_RA_IsOverlayFullyVisible != nullptr)
        return _RA_IsOverlayFullyVisible();

    return 0;
}

void RA_UpdateHWnd(HWND hMainWnd)
{
    if (_RA_UpdateHWnd != nullptr)
        _RA_UpdateHWnd(hMainWnd);
}

unsigned int RA_IdentifyRom(BYTE* pROMData, unsigned int nROMSize)
{
    if (_RA_IdentifyRom != nullptr)
        return _RA_IdentifyRom(pROMData, nROMSize);

    return 0;
}

unsigned int RA_IdentifyHash(const char* sHash)
{
    if (_RA_IdentifyHash!= nullptr)
        return _RA_IdentifyHash(sHash);

    return 0;
}

void RA_ActivateGame(unsigned int nGameId)
{
    if (_RA_ActivateGame != nullptr)
        _RA_ActivateGame(nGameId);
}

void RA_OnLoadNewRom(BYTE* pROMData, unsigned int nROMSize)
{
    if (_RA_OnLoadNewRom != nullptr)
        _RA_OnLoadNewRom(pROMData, nROMSize);
}

void RA_ClearMemoryBanks(void)
{
    if (_RA_ClearMemoryBanks != nullptr)
        _RA_ClearMemoryBanks();
}

void RA_InstallMemoryBank(int nBankID, RA_ReadMemoryFunc pReader, RA_WriteMemoryFunc pWriter, int nBankSize)
{
    if (_RA_InstallMemoryBank != nullptr)
        _RA_InstallMemoryBank(nBankID, pReader, pWriter, nBankSize);
}

void RA_InstallMemoryBankBlockReader(int nBankID, RA_ReadMemoryBlockFunc pReader)
{
    if (_RA_InstallMemoryBankBlockReader != nullptr)
        _RA_InstallMemoryBankBlockReader(nBankID, pReader);
}

HMENU RA_CreatePopupMenu(void)
{
    return (_RA_CreatePopupMenu != nullptr) ? _RA_CreatePopupMenu() : nullptr;
}

int RA_GetPopupMenuItems(RA_MenuItem *pItems)
{
    return (_RA_GetPopupMenuItems != nullptr) ? _RA_GetPopupMenuItems(pItems) : 0;
}

void RA_UpdateAppTitle(const char* sCustomMsg)
{
    if (_RA_UpdateAppTitle != nullptr)
        _RA_UpdateAppTitle(sCustomMsg);
}

void RA_HandleHTTPResults(void)
{
}

int RA_ConfirmLoadNewRom(int bIsQuitting)
{
    return _RA_ConfirmLoadNewRom ? _RA_ConfirmLoadNewRom(bIsQuitting) : 1;
}

void RA_InvokeDialog(LPARAM nID)
{
    if (_RA_InvokeDialog != nullptr)
        _RA_InvokeDialog(nID);
}

void RA_SetPaused(bool bIsPaused)
{
    if (_RA_SetPaused != nullptr)
        _RA_SetPaused(bIsPaused);
}

void RA_OnLoadState(const char* sFilename)
{
    if (_RA_OnLoadState != nullptr)
        _RA_OnLoadState(sFilename);
}

void RA_OnSaveState(const char* sFilename)
{
    if (_RA_OnSaveState != nullptr)
        _RA_OnSaveState(sFilename);
}

int RA_CaptureState(char* pBuffer, int nBufferSize)
{
    if (_RA_CaptureState != nullptr)
        return _RA_CaptureState(pBuffer, nBufferSize);

    return 0;
}

void RA_RestoreState(const char* pBuffer)
{
    if (_RA_RestoreState != nullptr)
        _RA_RestoreState(pBuffer);
}

void RA_OnReset(void)
{
    if (_RA_OnReset != nullptr)
        _RA_OnReset();
}

void RA_DoAchievementsFrame(void)
{
    if (_RA_DoAchievementsFrame != nullptr)
        _RA_DoAchievementsFrame();
}

void RA_SetForceRepaint(int bEnable)
{
    if (_RA_SetForceRepaint != nullptr)
        _RA_SetForceRepaint(bEnable);
}

void RA_SuspendRepaint(void)
{
  if (_RA_SuspendRepaint != nullptr)
    _RA_SuspendRepaint();
}

void RA_ResumeRepaint(void)
{
  if (_RA_ResumeRepaint != nullptr)
    _RA_ResumeRepaint();
}

void RA_SetConsoleID(unsigned int nConsoleID)
{
    if (_RA_SetConsoleID != nullptr)
        _RA_SetConsoleID(nConsoleID);
}

int RA_HardcoreModeIsActive(void)
{
    return (_RA_HardcoreModeIsActive != nullptr) ? _RA_HardcoreModeIsActive() : 0;
}

int RA_WarnDisableHardcore(const char* sActivity)
{
    // If Hardcore mode not active, allow the activity.
    if (!RA_HardcoreModeIsActive())
        return 1;

    // DLL function will display a yes/no dialog. If the user chooses yes, the DLL will disable hardcore mode, and the activity can proceed.
    if (_RA_WarnDisableHardcore != nullptr)
        return _RA_WarnDisableHardcore(sActivity);

    // We cannot disable hardcore mode, so just warn the user and prevent the activity.
    std::string sMessage;
    sMessage = "You cannot " + std::string(sActivity) + " while Hardcore mode is active.";
    MessageBoxA(nullptr, sMessage.c_str(), "Warning", MB_OK | MB_ICONWARNING);
    return 0;
}

void RA_DisableHardcore()
{
    // passing nullptr to _RA_WarnDisableHardcore will just disable hardcore mode without prompting.
    if (_RA_WarnDisableHardcore != nullptr)
        _RA_WarnDisableHardcore(nullptr);
}

#ifndef RA_NOPROGRESS

static void FormatByteSize(DWORD nSize, wchar_t* sBuffer, size_t nBufferSize)
{
    if (nSize < 1024)
    {
        swprintf_s(sBuffer, nBufferSize, L"%u bytes", nSize);
        return;
    }

    float fSize = nSize;
    fSize /= 1024.0;
    if (fSize < 1024.0)
    {
        swprintf_s(sBuffer, nBufferSize, L"%.2f KB", fSize);
        return;
    }

    fSize /= 1024.0;
    swprintf_s(sBuffer, nBufferSize, L"%.2f MB", fSize);
}

static void SetProgressMessage(IProgressDialog* pProgressDialog, DWORD nProgress, DWORD nMax)
{
    wchar_t sNum[32];
    std::wstring sMessage;

    FormatByteSize(nProgress, sNum, sizeof(sNum) / sizeof(sNum[0]));
    sMessage.append(sNum);
    sMessage.append(L" of ");

    FormatByteSize(nMax, sNum, sizeof(sNum) / sizeof(sNum[0]));
    sMessage.append(sNum);

    pProgressDialog->SetLine(2, sMessage.c_str(), FALSE, NULL);
}

#endif

static size_t DownloadToFile(char* pData, size_t nDataSize, void* pUserData, DWORD* pStatusCode)
{
    FILE* file = (FILE*)pUserData;
    size_t nBytesWritten = fwrite(pData, 1, nDataSize, file);
    if (nBytesWritten != nDataSize)
    {
        std::string sMessage;
        sMessage = "Error " + std::to_string(errno) + " writing file: " + strerror(errno);
        MessageBoxA(nullptr, sMessage.c_str(), "Error", MB_OK | MB_ICONERROR);
        *pStatusCode = 0;
    }

    return nBytesWritten;
}

typedef struct DownloadBuffer
{
    char* pBuffer;
    size_t nBufferSize;
    size_t nOffset;
} DownloadBuffer;

static size_t DownloadToBuffer(char* pData, size_t nDataSize, void* pUserData, DWORD* pStatusCode)
{
    DownloadBuffer* pBuffer = (DownloadBuffer*)pUserData;
    const size_t nRemaining = pBuffer->nBufferSize - pBuffer->nOffset;
    if (nDataSize > nRemaining)
        nDataSize = nRemaining;

    if (nDataSize > 0)
    {
        memcpy(pBuffer->pBuffer + pBuffer->nOffset, pData, nDataSize);
        pBuffer->nOffset += nDataSize;
    }

    return nDataSize;
}

typedef size_t (DownloadFunc)(char* pData, size_t nDataSize, void* pUserData, DWORD* pStatusCode);

static BOOL DoBlockingHttpCall(const char* sHostUrl, const char* sRequestedPage, const char* sPostData,
  DownloadFunc fnDownload, void* pDownloadUserData, DWORD* pBytesRead, DWORD* pStatusCode, IProgressDialog* pProgressDialog)
{
    BOOL bResults = FALSE, bSuccess = FALSE;
    HINTERNET hSession = nullptr, hConnect = nullptr, hRequest = nullptr;
    size_t nHostnameLen;

    WCHAR wBuffer[1024];
    size_t nTemp;
    DWORD nBytesAvailable = 0;
    DWORD nBytesToRead = 0;
    DWORD nBytesFetched = 0;
    (*pBytesRead) = 0;

    INTERNET_PORT nPort = INTERNET_DEFAULT_HTTP_PORT;
    const char* sHostName = sHostUrl;
    if (_strnicmp(sHostName, "http://", 7) == 0)
    {
        sHostName += 7;
    }
    else if (_strnicmp(sHostName, "https://", 8) == 0)
    {
        sHostName += 8;
        nPort = INTERNET_DEFAULT_HTTPS_PORT;
    }

    const char* sPort = strchr(sHostName, ':');
    if (sPort)
    {
        nHostnameLen = sPort - sHostName;
        nPort = atoi(sPort + 1);
    }
    else
    {
        nHostnameLen = strlen(sHostName);
    }

    // Use WinHttpOpen to obtain a session handle.
    hSession = WinHttpOpen(L"RetroAchievements Client Bootstrap",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    // Specify an HTTP server.
    if (hSession == nullptr)
    {
        *pStatusCode = GetLastError();
    }
    else
    {
#if defined(_MSC_VER) && _MSC_VER >= 1400
        mbstowcs_s(&nTemp, wBuffer, sizeof(wBuffer) / sizeof(wBuffer[0]), sHostName, nHostnameLen);
#else
        nTemp = mbstowcs(wBuffer, sHostName, nHostnameLen);
        if (nTemp > 0)
            wBuffer[nTemp] = '\0';
#endif

        if (nTemp > 0)
        {
            hConnect = WinHttpConnect(hSession, wBuffer, nPort, 0);
        }

        // Create an HTTP Request handle.
        if (hConnect == nullptr)
        {
            *pStatusCode = GetLastError();
        }
        else
        {
#if defined(_MSC_VER) && _MSC_VER >= 1400
            mbstowcs_s(&nTemp, wBuffer, sizeof(wBuffer) / sizeof(wBuffer[0]), sRequestedPage, strlen(sRequestedPage) + 1);
#else
            nTemp = mbstowcs(wBuffer, sRequestedPage, strlen(sRequestedPage) + 1);
            if (nTemp > 0)
                wBuffer[nTemp] = '\0';
#endif

            hRequest = WinHttpOpenRequest(hConnect,
                sPostData ? L"POST" : L"GET",
                wBuffer,
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                (nPort == INTERNET_DEFAULT_HTTPS_PORT) ? WINHTTP_FLAG_SECURE : 0);

            // Send a Request.
            if (hRequest == nullptr)
            {
                *pStatusCode = GetLastError();
            }
            else
            {
                if (sPostData)
                {
                    const size_t nPostDataLength = strlen(sPostData);
                    bResults = WinHttpSendRequest(hRequest,
                        L"Content-Type: application/x-www-form-urlencoded",
                        0, (LPVOID)sPostData, (DWORD)nPostDataLength, (DWORD)nPostDataLength, 0);
                }
                else
                {
                    bResults = WinHttpSendRequest(hRequest,
                        L"Content-Type: application/x-www-form-urlencoded",
                        0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
                }

                if (!bResults || !WinHttpReceiveResponse(hRequest, nullptr))
                {
                    *pStatusCode = GetLastError();
                }
                else
                {
                    const DWORD nBufferSize = 16384;
                    char* pBuffer = (char*)malloc(nBufferSize);
                    DWORD dwSize = sizeof(DWORD);
                    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, pStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
                    bSuccess = (*pStatusCode == 200);
                    const DWORD nDefaultTotalBytes = 3 * 1024 * 1024;
                    DWORD nTotalBytes = nDefaultTotalBytes;

#ifndef RA_NOPROGRESS
                    if (pProgressDialog)
                    {
                        dwSize = nBufferSize;
                        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, pBuffer, &dwSize, WINHTTP_NO_HEADER_INDEX))
                        {
                            nTotalBytes = wcstoul((const wchar_t*)pBuffer, NULL, 10);

                            SetProgressMessage(pProgressDialog, 0, nTotalBytes);
                        }
                    }
#endif

                    while (bSuccess)
                    {
                        nBytesAvailable = 0;
                        WinHttpQueryDataAvailable(hRequest, &nBytesAvailable);
                        if (nBytesAvailable == 0)
                            break;

                        do
                        {
                            if (nBytesAvailable > nBufferSize)
                                nBytesToRead = nBufferSize;
                            else
                                nBytesToRead = nBytesAvailable;

                            nBytesFetched = 0;
                            if (WinHttpReadData(hRequest, pBuffer, nBytesToRead, &nBytesFetched))
                            {
                                size_t nBytesWritten = fnDownload(pBuffer, nBytesFetched, pDownloadUserData, pStatusCode);
                                if (nBytesWritten < nBytesFetched)
                                {
                                    bSuccess = FALSE;
                                    break;
                                }

                                (*pBytesRead) += (DWORD)nBytesWritten;
                                nBytesAvailable -= nBytesFetched;
                            }
                            else
                            {
                                if (*pStatusCode == 200)
                                    *pStatusCode = GetLastError();

                                bSuccess = FALSE;
                                break;
                            }

#ifndef RA_NOPROGRESS
                            if (pProgressDialog)
                            {
                                if (pProgressDialog->HasUserCancelled())
                                {
                                    bSuccess = FALSE;
                                    *pStatusCode = 0;
                                    break;
                                }

                                pProgressDialog->SetProgress(*pBytesRead, nTotalBytes);

                                if (nTotalBytes != nDefaultTotalBytes)
                                    SetProgressMessage(pProgressDialog, *pBytesRead, nTotalBytes);
                            }
#endif
                        } while (nBytesAvailable > 0);
                    };

                    free(pBuffer);
                }

                WinHttpCloseHandle(hRequest);
            }

            WinHttpCloseHandle(hConnect);
        }

        WinHttpCloseHandle(hSession);
    }

    return bSuccess;
}

static BOOL IsNetworkError(DWORD nStatusCode)
{
    switch (nStatusCode)
    {
        case 12002: // timeout
        case 12007: // dns lookup failed
        case 12017: // handle closed before request completed
        case 12019: // handle not initialized
        case 12028: // data not available at this time
        case 12029: // handshake failed
        case 12030: // connection aborted
        case 12031: // connection reset
        case 12032: // explicit request to retry
        case 12152: // response could not be parsed, corrupt?
        case 12163: // lost connection during request
            return TRUE;

        default:
            return FALSE;
    }
}

static BOOL DoBlockingHttpCallWithRetry(const char* sHostUrl, const char* sRequestedPage, const char* sPostData,
  char pBufferOut[], unsigned int nBufferOutSize, DWORD* pBytesRead, DWORD* pStatusCode)
{
    int nRetries = 4;
    do
    {
        DownloadBuffer downloadBuffer;
        memset(&downloadBuffer, 0, sizeof(downloadBuffer));
        downloadBuffer.pBuffer = pBufferOut;
        downloadBuffer.nBufferSize = nBufferOutSize;

        if (DoBlockingHttpCall(sHostUrl, sRequestedPage, sPostData, DownloadToBuffer, &downloadBuffer, pBytesRead, pStatusCode, NULL) != FALSE)
            return TRUE;

        if (!IsNetworkError(*pStatusCode))
            return FALSE;

        --nRetries;
    } while (nRetries);

    return FALSE;
}

static BOOL DoBlockingHttpCallWithRetry(const char* sHostUrl, const char* sRequestedPage, const char* sPostData,
  FILE* pFile, DWORD* pBytesRead, DWORD* pStatusCode, const wchar_t* sMessage)
{
    IProgressDialog* pProgressDialog = NULL;
#ifndef RA_NOPROGRESS
    HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT hr = CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IProgressDialog, (LPVOID*)&pProgressDialog);

    if (SUCCEEDED(hr))
    {
        if (sMessage)
            pProgressDialog->SetLine(1, sMessage, FALSE, NULL);
        pProgressDialog->StartProgressDialog(NULL, NULL, PROGDLG_NORMAL, NULL);
    }
#endif

    BOOL success = FALSE;
    int nRetries = 4;
    do
    {
        fseek(pFile, 0, SEEK_SET);
        if (DoBlockingHttpCall(sHostUrl, sRequestedPage, sPostData, DownloadToFile, pFile, pBytesRead, pStatusCode, pProgressDialog) != FALSE)
        {
            success = TRUE;
            break;
        }

        if (!IsNetworkError(*pStatusCode))
            break;

        --nRetries;
    } while (nRetries);

#ifndef RA_NOPROGRESS
    if (pProgressDialog)
    {
        pProgressDialog->StopProgressDialog();
        pProgressDialog->Release();
    }

    if (hrInit == S_OK)
        CoUninitialize();
#endif

    return success;
}

#ifndef RA_UTEST
static std::wstring GetIntegrationPath(const wchar_t* sFilename)
{
    wchar_t sBuffer[2048];
    DWORD iIndex = GetModuleFileNameW(0, sBuffer, 2048);
    while (iIndex > 0 && sBuffer[iIndex - 1] != '\\' && sBuffer[iIndex - 1] != '/')
        --iIndex;

#if defined(_MSC_VER) && _MSC_VER >= 1400
    wcscpy_s(&sBuffer[iIndex], sizeof(sBuffer)/sizeof(sBuffer[0]) - iIndex, sFilename);
#else
    wcscpy(&sBuffer[iIndex], sFilename);
#endif

    return std::wstring(sBuffer);
}
#endif

static void FetchIntegrationFromWeb(char* sLatestVersionUrl, DWORD* pStatusCode)
{
    DWORD nBytesRead = 0;
    const std::wstring sDownloadFilename = GetIntegrationPath(RA_INT_DLL ".download");
    const std::wstring sOldFilename = GetIntegrationPath(RA_INT_DLL ".old");
    const std::wstring sNewFilename = GetIntegrationPath(RA_INT_DLL);
    std::wstring sFilename = sNewFilename;

#ifdef RA_X64
    if (GetFileAttributesW(sFilename.c_str()) == INVALID_FILE_ATTRIBUTES)
        sFilename = GetIntegrationPath(L"RA_Integration.dll");
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400
    FILE* pf;
    errno_t nErr = _wfopen_s(&pf, sDownloadFilename.c_str(), L"wb");
#else
    FILE* pf = _wfopen(sDownloadFilename.c_str(), L"wb");
#endif

    if (!pf)
    {
#if defined(_MSC_VER) && _MSC_VER >= 1400
        wchar_t sErrBuffer[2048];
        _wcserror_s(sErrBuffer, sizeof(sErrBuffer) / sizeof(sErrBuffer[0]), nErr);

        std::wstring sErrMsg = std::wstring(L"Unable to write " RA_INT_DLL L".download\n") + sErrBuffer;
#else
        std::wstring sErrMsg = std::wstring(L"Unable to write " RA_INT_DLL L".download\n") + _wcserror(errno);
#endif

        MessageBoxW(nullptr, sErrMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
        *pStatusCode = 0;
        return;
    }

    char* pSplit = sLatestVersionUrl + 8; /* skip over protocol */
    while (*pSplit != '/')
    {
        if (!*pSplit)
        {
            *pStatusCode = 997;
            return;
        }
        ++pSplit;
    }
    *pSplit++ = '\0';

    if (DoBlockingHttpCallWithRetry(sLatestVersionUrl, pSplit, nullptr, pf, &nBytesRead, pStatusCode, "Downloading " RA_INT_DLL))
    {
        fclose(pf);

        /* wait up to one second for the DLL to actually be released - sometimes it's not immediate */
        for (int i = 0; i < 10; i++)
        {
            if (GetModuleHandleW(sFilename.c_str()) == nullptr)
                break;

            Sleep(100);
        }

        // delete the last old dll if it's still present
        DeleteFileW(sOldFilename.c_str());

        // if there's a dll present, rename it
        if (GetFileAttributesW(sFilename.c_str()) != INVALID_FILE_ATTRIBUTES &&
            !MoveFileW(sFilename.c_str(), sOldFilename.c_str()))
        {
            MessageBoxW(nullptr, L"Could not rename old dll", L"Error", MB_OK | MB_ICONERROR);
        }
        // rename the download to be the dll
        else if (!MoveFileW(sDownloadFilename.c_str(), sNewFilename.c_str()))
        {
            MessageBoxW(nullptr, L"Could not rename new dll", L"Error", MB_OK | MB_ICONERROR);
        }

        // delete the old dll
        DeleteFileW(sOldFilename.c_str());
    }
    else
    {
        fclose(pf);
    }
}

//Returns the last Win32 error, in string format. Returns an empty string if there is no error.
static std::string GetLastErrorAsString()
{
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return "No error message has been recorded";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

static const char* CCONV _RA_InstallIntegration()
{
    SetErrorMode(0);

    std::wstring sIntegrationPath = GetIntegrationPath(RA_INT_DLL);

    DWORD dwAttrib = GetFileAttributesW(sIntegrationPath.c_str());
#ifdef RA_X64
    if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
        sIntegrationPath = GetIntegrationPath(L"RA_Integration.dll");
        dwAttrib = GetFileAttributesW(sIntegrationPath.c_str());
    }
#endif
    if (dwAttrib == INVALID_FILE_ATTRIBUTES)
        return "0.0";

    g_hRADLL = LoadLibraryW(sIntegrationPath.c_str());
    if (g_hRADLL == nullptr)
    {
        char buffer[1024];
        if (::GetLastError() == ERROR_BAD_EXE_FORMAT)
        {
#ifdef RA_X64
          sprintf_s(buffer, sizeof(buffer), "Could not load RA_Integration-x64.dll (error %d)\nAre you trying to load the 32-bit version?\n", ::GetLastError());
#else
          sprintf_s(buffer, sizeof(buffer), "Could not load RA_Integration.dll (error %d)\nAre you trying to load the 64-bit version?\n", ::GetLastError());
#endif
        }
        else
        {
#ifdef RA_X64
            sprintf_s(buffer, sizeof(buffer), "Could not load RA_Integration-x64.dll (error %d)\n%s\n", ::GetLastError(), GetLastErrorAsString().c_str());
#else
            sprintf_s(buffer, sizeof(buffer), "Could not load RA_Integration.dll (error %d)\n%s\n", ::GetLastError(), GetLastErrorAsString().c_str());
#endif
        }

        MessageBoxA(nullptr, buffer, "Warning", MB_OK | MB_ICONWARNING);

        return "0.0";
    }

    //	Install function pointers one by one

    _RA_IntegrationVersion = (const char* (CCONV*)())                                GetProcAddress(g_hRADLL, "_RA_IntegrationVersion");
    _RA_HostName = (const char* (CCONV*)())                                          GetProcAddress(g_hRADLL, "_RA_HostName");
    _RA_HostUrl = (const char* (CCONV*)())                                           GetProcAddress(g_hRADLL, "_RA_HostUrl");
    _RA_InitI = (int(CCONV*)(HWND, int, const char*))                                GetProcAddress(g_hRADLL, "_RA_InitI");
    _RA_InitOffline = (int(CCONV*)(HWND, int, const char*))                          GetProcAddress(g_hRADLL, "_RA_InitOffline");
    _RA_InitClient = (int(CCONV*)(HWND, const char*, const char*))                   GetProcAddress(g_hRADLL, "_RA_InitClient");
    _RA_InitClientOffline = (int(CCONV*)(HWND, const char*, const char*))            GetProcAddress(g_hRADLL, "_RA_InitClientOffline");
    _RA_InstallSharedFunctions = (void(CCONV*)(int(*)(), void(*)(), void(*)(), void(*)(), void(*)(char*), void(*)(), void(*)(const char*))) GetProcAddress(g_hRADLL, "_RA_InstallSharedFunctionsExt");
    _RA_SetForceRepaint = (void(CCONV*)(int))                                        GetProcAddress(g_hRADLL, "_RA_SetForceRepaint");
    _RA_CreatePopupMenu = (HMENU(CCONV*)(void))                                      GetProcAddress(g_hRADLL, "_RA_CreatePopupMenu");
    _RA_GetPopupMenuItems = (int(CCONV*)(RA_MenuItem*))                              GetProcAddress(g_hRADLL, "_RA_GetPopupMenuItems");
    _RA_InvokeDialog = (void(CCONV*)(LPARAM))                                        GetProcAddress(g_hRADLL, "_RA_InvokeDialog");
    _RA_SetUserAgentDetail = (void(CCONV*)(const char*))                             GetProcAddress(g_hRADLL, "_RA_SetUserAgentDetail");
    _RA_AttemptLogin = (void(CCONV*)(int))                                           GetProcAddress(g_hRADLL, "_RA_AttemptLogin");
    _RA_SetConsoleID = (int(CCONV*)(unsigned int))                                   GetProcAddress(g_hRADLL, "_RA_SetConsoleID");
    _RA_ClearMemoryBanks = (void(CCONV*)())                                          GetProcAddress(g_hRADLL, "_RA_ClearMemoryBanks");
    _RA_InstallMemoryBank = (void(CCONV*)(int, RA_ReadMemoryFunc*, RA_WriteMemoryFunc*, int)) GetProcAddress(g_hRADLL, "_RA_InstallMemoryBank");
    _RA_InstallMemoryBankBlockReader = (void(CCONV*)(int, RA_ReadMemoryBlockFunc*))  GetProcAddress(g_hRADLL, "_RA_InstallMemoryBankBlockReader");
    _RA_Shutdown = (int(CCONV*)())                                                   GetProcAddress(g_hRADLL, "_RA_Shutdown");
    _RA_IsOverlayFullyVisible = (int(CCONV*)())                                      GetProcAddress(g_hRADLL, "_RA_IsOverlayFullyVisible");
    _RA_SetPaused = (void(CCONV*)(int))                                              GetProcAddress(g_hRADLL, "_RA_SetPaused");
    _RA_NavigateOverlay = (void(CCONV*)(ControllerInput*))                           GetProcAddress(g_hRADLL, "_RA_NavigateOverlay");
    _RA_UpdateHWnd = (void(CCONV*)(HWND))                                            GetProcAddress(g_hRADLL, "_RA_UpdateHWnd");
    _RA_IdentifyRom = (unsigned int(CCONV*)(const BYTE*, unsigned int))              GetProcAddress(g_hRADLL, "_RA_IdentifyRom");
    _RA_IdentifyHash = (unsigned int(CCONV*)(const char*))                           GetProcAddress(g_hRADLL, "_RA_IdentifyHash");
    _RA_ActivateGame = (void(CCONV*)(unsigned int))                                  GetProcAddress(g_hRADLL, "_RA_ActivateGame");
    _RA_OnLoadNewRom = (int(CCONV*)(const BYTE*, unsigned int))                      GetProcAddress(g_hRADLL, "_RA_OnLoadNewRom");
    _RA_ConfirmLoadNewRom = (int(CCONV*)(int))                                       GetProcAddress(g_hRADLL, "_RA_ConfirmLoadNewRom");
    _RA_DoAchievementsFrame = (void(CCONV*)())                                       GetProcAddress(g_hRADLL, "_RA_DoAchievementsFrame");
    _RA_SuspendRepaint = (void(CCONV*)())                                            GetProcAddress(g_hRADLL, "_RA_SuspendRepaint");
    _RA_ResumeRepaint = (void(CCONV*)())                                             GetProcAddress(g_hRADLL, "_RA_ResumeRepaint");
    _RA_UpdateAppTitle = (void(CCONV*)(const char*))                                 GetProcAddress(g_hRADLL, "_RA_UpdateAppTitle");
    _RA_UserName = (const char* (CCONV*)())                                          GetProcAddress(g_hRADLL, "_RA_UserName");
    _RA_HardcoreModeIsActive = (int(CCONV*)())                                       GetProcAddress(g_hRADLL, "_RA_HardcoreModeIsActive");
    _RA_WarnDisableHardcore = (int(CCONV*)(const char*))                             GetProcAddress(g_hRADLL, "_RA_WarnDisableHardcore");
    _RA_OnReset = (void(CCONV*)())                                                   GetProcAddress(g_hRADLL, "_RA_OnReset");
    _RA_OnSaveState = (void(CCONV*)(const char*))                                    GetProcAddress(g_hRADLL, "_RA_OnSaveState");
    _RA_OnLoadState = (void(CCONV*)(const char*))                                    GetProcAddress(g_hRADLL, "_RA_OnLoadState");
    _RA_CaptureState = (int(CCONV*)(char*, int))                                     GetProcAddress(g_hRADLL, "_RA_CaptureState");
    _RA_RestoreState = (void(CCONV*)(const char*))                                   GetProcAddress(g_hRADLL, "_RA_RestoreState");

    return _RA_IntegrationVersion ? _RA_IntegrationVersion() : "0.0";
}

static void GetJsonField(const char* sJson, const char* sField, char *pBuffer, size_t nBufferSize)
{
    const size_t nFieldSize = strlen(sField);
    const char* pValue;

    *pBuffer = 0;
    do
    {
        const char* pScan = strstr(sJson, sField);
        if (!pScan)
            return;

        if (pScan[-1] != '"' || pScan[nFieldSize] != '"')
        {
            sJson = pScan + 1;
            continue;
        }

        pScan += nFieldSize + 1;
        while (*pScan == ':' || isspace(*pScan))
            ++pScan;
        if (*pScan != '"')
            return;

        pValue = ++pScan;
        while (*pScan != '"')
        {
            if (!*pScan)
                return;

            ++pScan;
        }

        while (pValue < pScan && nBufferSize > 1)
        {
            if (*pValue == '\\')
                ++pValue;

            *pBuffer++ = *pValue++;
            nBufferSize--;
        }

        *pBuffer = '\0';
        return;

    } while (1);
}

static unsigned long long ParseVersion(const char* sVersion)
{
    char* pPart;

    unsigned long long major = strtoul(sVersion, &pPart, 10);
    if (*pPart == '.')
        ++pPart;

    unsigned long long minor = strtoul(pPart, &pPart, 10);
    if (*pPart == '.')
        ++pPart;

    unsigned long long patch = strtoul(pPart, &pPart, 10);
    if (*pPart == '.')
        ++pPart;

    unsigned long long revision = strtoul(pPart, &pPart, 10);

    // 64-bit max signed value is 9223 37203 68547 75807
    unsigned long long version = (major * 100000) + minor;
    version = (version * 100000) + patch;
    version = (version * 100000) + revision;
    return version;
}

static void RA_InitCommon(HWND hMainHWND, int nEmulatorID, const char* sClientName, const char* sClientVersion)
{
    char sVerInstalled[32];
#if defined(_MSC_VER) && _MSC_VER >= 1400
    strcpy_s(sVerInstalled, sizeof(sVerInstalled), _RA_InstallIntegration());
#else
    strcpy(sVerInstalled, _RA_InstallIntegration());
#endif

    char sHostUrl[256] = "";
    if (_RA_HostUrl != nullptr)
    {
#if defined(_MSC_VER) && _MSC_VER >= 1400
        strcpy_s(sHostUrl, sizeof(sHostUrl), _RA_HostUrl());
#else
        strcpy(sHostUrl, _RA_HostUrl());
#endif
    }
    else if (_RA_HostName != nullptr)
    {
      sprintf_s(sHostUrl, "http://%s", _RA_HostName());
    }

    if (!sHostUrl[0])
    {
#if defined(_MSC_VER) && _MSC_VER >= 1400
        strcpy_s(sHostUrl, sizeof(sHostUrl), "http://retroachievements.org");
#else
        strcpy(sHostUrl, "http://retroachievements.org");
#endif
    }
    else if (_RA_InitOffline != nullptr && strcmp(sHostUrl, "http://OFFLINE") == 0)
    {
        if (sClientName == nullptr)
            _RA_InitOffline(hMainHWND, nEmulatorID, sClientVersion);
        else
            _RA_InitClientOffline(hMainHWND, sClientName, sClientVersion);
        return;
    }

    DWORD nBytesRead = 0;
    DWORD nStatusCode = 0;
    char buffer[1024];
    ZeroMemory(buffer, 1024);

    if (DoBlockingHttpCallWithRetry(sHostUrl, "dorequest.php", "r=latestintegration", buffer, sizeof(buffer), &nBytesRead, &nStatusCode) == FALSE)
    {
        if (_RA_InitOffline != nullptr)
        {
            sprintf_s(buffer, sizeof(buffer), "Cannot access %s (status code %u)\nWorking offline.", sHostUrl, nStatusCode);
            MessageBoxA(hMainHWND, buffer, "Warning", MB_OK | MB_ICONWARNING);

            _RA_InitOffline(hMainHWND, nEmulatorID, sClientVersion);
        }
        else
        {
            sprintf_s(buffer, sizeof(buffer), "Cannot access %s (status code %u)\nPlease try again later.", sHostUrl, nStatusCode);
            MessageBoxA(hMainHWND, buffer, "Warning", MB_OK | MB_ICONWARNING);

            RA_Shutdown();
        }
        return;
    }

    /* remove trailing zeros from client version */
    char* ptr = sVerInstalled + strlen(sVerInstalled);
    while (ptr[-1] == '0' && ptr[-2] == '.' && (ptr - 2) > sVerInstalled)
        ptr -= 2;
    *ptr = '\0';
    if (strchr(sVerInstalled, '.') == NULL)
    {
        *ptr++ = '.';
        *ptr++ = '0';
        *ptr = '\0';
    }

    char sLatestVersionUrl[256];
    char sVersionBuffer[32];
    GetJsonField(buffer, "MinimumVersion", sVersionBuffer, sizeof(sVersionBuffer));
    const unsigned long long nMinimumDLLVer = ParseVersion(sVersionBuffer);

    GetJsonField(buffer, "LatestVersion", sVersionBuffer, sizeof(sVersionBuffer));
    const unsigned long long nLatestDLLVer = ParseVersion(sVersionBuffer);

#ifdef RA_X64
    GetJsonField(buffer, "LatestVersionUrlX64", sLatestVersionUrl, sizeof(sLatestVersionUrl));
#else
    GetJsonField(buffer, "LatestVersionUrl", sLatestVersionUrl, sizeof(sLatestVersionUrl));
#endif

    if (nLatestDLLVer == 0 || !sLatestVersionUrl[0])
    {
        /* NOTE: repurposing sLatestVersionUrl for the error message */
        GetJsonField(buffer, "Error", sLatestVersionUrl, sizeof(sLatestVersionUrl));
        if (sLatestVersionUrl[0])
            sprintf_s(buffer, sizeof(buffer), "Failed to fetch latest integration version.\n\n%s", sLatestVersionUrl);
        else
            sprintf_s(buffer, sizeof(buffer), "The latest integration check did not return a valid response.");

        MessageBoxA(hMainHWND, buffer, "Error", MB_OK | MB_ICONERROR);
        RA_Shutdown();
        return;
    }

    int nMBReply = 0;
    unsigned long long nVerInstalled = ParseVersion(sVerInstalled);
    if (nVerInstalled < nMinimumDLLVer)
    {
        RA_Shutdown(); // Unhook the DLL so we can replace it.

        if (nVerInstalled == 0)
        {
            sprintf_s(buffer, sizeof(buffer), "Install RetroAchievements toolset?\n\n"
                "In order to earn achievements you must download the toolset library.");
        }
        else
        {
            sprintf_s(buffer, sizeof(buffer), "Upgrade RetroAchievements toolset?\n\n"
                "A required upgrade to the toolset is available. If you don't upgrade, you won't be able to earn achievements.\n\n"
                "Latest Version: %s\nInstalled Version: %s", sVersionBuffer, sVerInstalled);
        }

        nMBReply = MessageBoxA(hMainHWND, buffer, "Warning", MB_YESNO | MB_ICONWARNING);
    }
    else if (nVerInstalled < nLatestDLLVer)
    {
        sprintf_s(buffer, sizeof(buffer), "Upgrade RetroAchievements toolset?\n\n"
            "An optional upgrade to the toolset is available.\n\n"
            "Latest Version: %s\nInstalled Version: %s", sVersionBuffer, sVerInstalled);

        nMBReply = MessageBoxA(hMainHWND, buffer, "Warning", MB_YESNO | MB_ICONWARNING);

        if (nMBReply == IDYES)
            RA_Shutdown(); // Unhook the DLL so we can replace it.
    }

    if (nMBReply == IDYES)
    {
        FetchIntegrationFromWeb(sLatestVersionUrl, &nStatusCode);

        if (nStatusCode != 0) // 0 indicates an error was already reported
        {
            if (nStatusCode == 200)
                nVerInstalled = ParseVersion(_RA_InstallIntegration());

            if (nVerInstalled < nLatestDLLVer)
            {
                sprintf_s(buffer, sizeof(buffer), "Failed to update toolset (status code %u).", nStatusCode);
                MessageBoxA(hMainHWND, buffer, "Error", MB_OK | MB_ICONERROR);
            }
        }
    }

    if (nVerInstalled < nMinimumDLLVer)
    {
        RA_Shutdown();

        sprintf_s(buffer, sizeof(buffer), "%s toolset is required to earn achievements.", nVerInstalled == 0 ? "The" : "A newer");
        MessageBoxA(hMainHWND, buffer, "Warning", MB_OK | MB_ICONWARNING);
    }
    else if (sClientName == nullptr)
    {
        if (!_RA_InitI(hMainHWND, nEmulatorID, sClientVersion))
            RA_Shutdown();
    }
    else
    {
        if (!_RA_InitClient(hMainHWND, sClientName, sClientVersion))
            RA_Shutdown();
    }
}

void RA_Init(HWND hMainHWND, int nEmulatorID, const char* sClientVersion)
{
    RA_InitCommon(hMainHWND, nEmulatorID, nullptr, sClientVersion);
}

void RA_InitClient(HWND hMainHWND, const char* sClientName, const char* sClientVersion)
{
    RA_InitCommon(hMainHWND, -1, sClientName, sClientVersion);
}

void RA_SetUserAgentDetail(const char* sDetail)
{
    if (_RA_SetUserAgentDetail != nullptr)
        _RA_SetUserAgentDetail(sDetail);
}

void RA_InstallSharedFunctions(int(*)(void), void(*fpCauseUnpause)(void), void(*fpCausePause)(void), void(*fpRebuildMenu)(void), void(*fpEstimateTitle)(char*), void(*fpResetEmulation)(void), void(*fpLoadROM)(const char*))
{
    if (_RA_InstallSharedFunctions != nullptr)
        _RA_InstallSharedFunctions(nullptr, fpCauseUnpause, fpCausePause, fpRebuildMenu, fpEstimateTitle, fpResetEmulation, fpLoadROM);
}

void RA_Shutdown()
{
    //	Call shutdown on toolchain
    if (_RA_Shutdown != nullptr)
    {
#if defined(__cplusplus) && _HAS_EXCEPTIONS
        try {
#endif
            _RA_Shutdown();
#if defined(__cplusplus) && _HAS_EXCEPTIONS
        }
        catch (std::runtime_error&) {
        }
#endif
    }

    //	Clear func ptrs
    _RA_IntegrationVersion = nullptr;
    _RA_HostName = nullptr;
    _RA_HostUrl = nullptr;
    _RA_InitI = nullptr;
    _RA_InitOffline = nullptr;
    _RA_InitClient = nullptr;
    _RA_InitClientOffline = nullptr;
    _RA_InstallSharedFunctions = nullptr;
    _RA_SetForceRepaint = nullptr;
    _RA_CreatePopupMenu = nullptr;
    _RA_GetPopupMenuItems = nullptr;
    _RA_InvokeDialog = nullptr;
    _RA_SetUserAgentDetail = nullptr;
    _RA_AttemptLogin = nullptr;
    _RA_SetConsoleID = nullptr;
    _RA_ClearMemoryBanks = nullptr;
    _RA_InstallMemoryBank = nullptr;
    _RA_InstallMemoryBankBlockReader = nullptr;
    _RA_Shutdown = nullptr;
    _RA_IsOverlayFullyVisible = nullptr;
    _RA_SetPaused = nullptr;
    _RA_NavigateOverlay = nullptr;
    _RA_UpdateHWnd = nullptr;
    _RA_IdentifyRom = nullptr;
    _RA_IdentifyHash = nullptr;
    _RA_ActivateGame = nullptr;
    _RA_OnLoadNewRom = nullptr;
    _RA_ConfirmLoadNewRom = nullptr;
    _RA_DoAchievementsFrame = nullptr;
    _RA_SuspendRepaint = nullptr;
    _RA_ResumeRepaint = nullptr;
    _RA_UpdateAppTitle = nullptr;
    _RA_UserName = nullptr;
    _RA_HardcoreModeIsActive = nullptr;
    _RA_WarnDisableHardcore = nullptr;
    _RA_OnReset = nullptr;
    _RA_OnSaveState = nullptr;
    _RA_OnLoadState = nullptr;
    _RA_CaptureState = nullptr;
    _RA_RestoreState = nullptr;

    /* unload the DLL */
    if (g_hRADLL)
    {
        FreeLibrary(g_hRADLL);
        g_hRADLL = nullptr;
    }
}
