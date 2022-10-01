#include <windows.h>

// outsmart GCC's missing-declarations warning
BOOL WINAPI DllMain(HMODULE, DWORD, LPVOID);
BOOL WINAPI DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
