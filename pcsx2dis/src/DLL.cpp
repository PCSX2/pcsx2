#include <Windows.h>
#include <cstdio>

#include "Main.h"
#include "Hacking.h"
#include "GameShark.h"
#include "Windows.h"

#ifdef _DLL
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hNull, LPSTR lpszCmdLine, int nCmdShow);
void GameDataShutdown();
void UpdateList();
void UpdateListScroll();
void UpdateTextbox();
void HandleBreakpoints();

extern UINT32 memoryPointer;
extern void (*pcsx2SetMemory)(UINT32 address, void* data, int datasize);

HINSTANCE myhInst;

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID uselessThing)
{
	myhInst = hInstDll;
	return 1;
}

extern "C" __declspec(dllexport) void InitDll()
{
	WinMain(myhInst, NULL, NULL, 0);
}

GameSharkCode dumbFixedBuffer[1024];
#include "Dialogs.h"
extern "C" __declspec(dllexport) void HandleDll(PCSX2SharedData* sharedData)
{
	static int lastupdate = 0;
	MSG message;
	char info[100];

	// Link PCSX2 data
	lxSharedData = sharedData;

	memoryPointer = (UINT32) sharedData->ps2Memory;

	MainUpdate();
	/*while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) > 0)
	{
		// HACK! We're sending a copy of the keystroke receipt to main_hwnd as well. In fact we might not even let it through to listbox_hwnd.
		if ((message.hwnd == listbox_hwnd && (message.message == WM_KEYDOWN)) || 
			(message.hwnd == wndEdit.hwnd && (message.message == WM_KEYDOWN)))
		{
			MSG message_copy;
			bool relay_message; // Sometimes we might not want to pass on the message...

			memcpy(&message_copy, &message, sizeof (MSG));

			message_copy.hwnd = main_hwnd;

							TranslateMessage (&message_copy);
			relay_message = DispatchMessage  (&message_copy);

			if (! relay_message)
				continue;
		}

		if (! IsDialogMessage(dlgProgress.hwnd, &message))
		{
			TranslateMessage (&message);
			DispatchMessage  (&message);
		}
	}
	
	UpdateListScroll();

	if (GetTickCount() - lastupdate >= 500)
	{
		// Update list (half-second update)
		int sel = SendMessage(listbox_hwnd, LB_GETCURSEL, 0, 0);

		UpdateList();
		UpdateTextbox();

		SendMessage(listbox_hwnd, LB_SETCURSEL, sel, 0);
		lastupdate = GetTickCount();
	}

	HandleBreakpoints();*/

	// Send gameshark codes. Due to threading issues, a dumb fixed buffer is kept.
	// I have no idea how to do sync multiple threads.
	while (lxSharedData->numGameSharkCodes < 0);
	lxSharedData->numGameSharkCodes = -2;

	if (gameSharkCodes)
		memcpy(dumbFixedBuffer, gameSharkCodes, numGameSharkCodes * sizeof (GameSharkCode));

	lxSharedData->gameSharkCodes = dumbFixedBuffer;
	lxSharedData->numGameSharkCodes = numGameSharkCodes;
}

extern "C" __declspec(dllexport) void ShutdownDll()
{
	GameDataShutdown();
}

#endif