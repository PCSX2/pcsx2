/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "rawinput_usb.h"
#include <cstdio>
#include <vector>
#include <algorithm>
#include <mutex>
#include "../platcompat.h"

extern HINSTANCE hInst;

namespace shared
{
	namespace rawinput
	{

		static std::vector<ParseRawInputCB*> callbacks;

		HWND msgWindow = nullptr;
		WNDPROC eatenWndProc = nullptr;
		HWND eatenWnd = nullptr;
		HHOOK hHook = nullptr, hHookWnd = nullptr, hHookKB = nullptr;
		bool skipInput = false;
		std::mutex cb_mutex;

		void RegisterCallback(ParseRawInputCB* cb)
		{
			std::scoped_lock<std::mutex> lk(cb_mutex);
			if (cb && std::find(callbacks.begin(), callbacks.end(), cb) == callbacks.end())
				callbacks.push_back(cb);
		}

		void UnregisterCallback(ParseRawInputCB* cb)
		{
			std::scoped_lock<std::mutex> lk(cb_mutex);
			auto it = std::find(callbacks.begin(), callbacks.end(), cb);
			if (it != callbacks.end())
				callbacks.erase(it);
		}

		static POINT origCursorPos;
		static POINT center;
		static bool cursorCaptured = false;

		static void WindowResized(HWND hWnd)
		{
			RECT r;
			GetWindowRect(hWnd, &r);
			ClipCursor(&r);
			center.x = (r.left + r.right) / 2;
			center.y = (r.top + r.bottom) / 2;
			SetCursorPos(center.x, center.y);
		}

		static void CursorCapture(HWND hWnd)
		{
			Console.WriteLn("Capture cursor\n");
			SetCapture(hWnd);
			ShowCursor(0);

			GetCursorPos(&origCursorPos);

			RECT r;
			GetWindowRect(hWnd, &r);
			ClipCursor(&r);
			center.x = (r.left + r.right) / 2;
			center.y = (r.top + r.bottom) / 2;
			SetCursorPos(center.x, center.y);
			cursorCaptured = true;
		}

		static void CursorRelease()
		{
			Console.WriteLn("Release cursor\n");
			if (cursorCaptured)
			{
				ClipCursor(0);
				ReleaseCapture();
				ShowCursor(1);
				SetCursorPos(origCursorPos.x, origCursorPos.y);
				cursorCaptured = false;
			}
		}

		static void ToggleCursor(HWND hWnd, RAWKEYBOARD& k)
		{
			static bool shiftDown = false;

			if (k.VKey == VK_SHIFT || k.VKey == VK_LSHIFT || k.VKey == VK_RSHIFT)
				shiftDown = !(k.Flags & RI_KEY_BREAK);

			if (shiftDown && k.VKey == VK_F11 && !k.Flags)
			{
				if (!cursorCaptured)
					CursorCapture(hWnd);
				else
					CursorRelease();
			}
		}

		static int RegisterRaw(HWND hWnd)
		{
			msgWindow = hWnd;
			RAWINPUTDEVICE Rid[4];
			Rid[0].usUsagePage = 0x01;
			Rid[0].usUsage = HID_USAGE_GENERIC_GAMEPAD;
			Rid[0].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds game pad
			Rid[0].hwndTarget = hWnd;

			Rid[1].usUsagePage = 0x01;
			Rid[1].usUsage = HID_USAGE_GENERIC_JOYSTICK;
			Rid[1].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE; // adds joystick
			Rid[1].hwndTarget = hWnd;

			Rid[2].usUsagePage = 0x01;
			Rid[2].usUsage = HID_USAGE_GENERIC_KEYBOARD;
			Rid[2].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE; // | RIDEV_NOLEGACY;   // adds HID keyboard //and also !ignores legacy keyboard messages
			Rid[2].hwndTarget = hWnd;

			Rid[3].usUsagePage = 0x01;
			Rid[3].usUsage = HID_USAGE_GENERIC_MOUSE;
			Rid[3].dwFlags = hWnd ? RIDEV_INPUTSINK : RIDEV_REMOVE;
			Rid[3].hwndTarget = hWnd;

			if (RegisterRawInputDevices(Rid, countof(Rid), sizeof(Rid[0])) == FALSE)
			{
				//registration failed. Call GetLastError for the cause of the error.
				Console.Warning("Could not (de)register raw input devices.\n");
				return 0;
			}
			return 1;
		}

		static LRESULT CALLBACK MyWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uMsg)
			{
				case WM_ACTIVATE:
					Console.WriteLn("******      WM_ACTIVATE        ****** %p %d\n", hWnd, LOWORD(wParam) != WA_INACTIVE);
					skipInput = LOWORD(wParam) == WA_INACTIVE;
					break;
				case WM_SETFOCUS:
					Console.WriteLn("******      WM_SETFOCUS        ****** %p\n", hWnd);
					skipInput = false;
					break;
				case WM_KILLFOCUS:
					Console.WriteLn("******      WM_KILLFOCUS        ****** %p\n", hWnd);
					skipInput = true;
					break;
			}
			return 0;
		}

		static LRESULT CALLBACK RawInputProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			PRAWINPUT pRawInput;
			UINT bufferSize = 0;

			switch (uMsg)
			{
				case WM_CREATE:
					if (eatenWnd == nullptr)
						RegisterRaw(hWnd);
					break;
				case WM_INPUT:
				{
					if (skipInput)
						break;

					GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &bufferSize, sizeof(RAWINPUTHEADER));
					pRawInput = (PRAWINPUT)malloc(bufferSize);

					if (!pRawInput)
						break;

					if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, pRawInput, &bufferSize, sizeof(RAWINPUTHEADER)) > 0)
					{

						if (pRawInput->header.dwType == RIM_TYPEKEYBOARD)
							ToggleCursor(hWnd, pRawInput->data.keyboard);

						std::lock_guard<std::mutex> lk(cb_mutex);
						for (auto cb : callbacks)
							cb->ParseRawInput(pRawInput);
					}

					free(pRawInput);
					break;
				}
				case WM_ACTIVATE:
					Console.WriteLn("******      WM_ACTIVATE        ****** %p %d\n", hWnd, LOWORD(wParam) != WA_INACTIVE);
					skipInput = LOWORD(wParam) == WA_INACTIVE;
					if (LOWORD(wParam) == WA_INACTIVE)
						CursorRelease();
					break;
				case WM_SETFOCUS:
					Console.WriteLn("******      WM_SETFOCUS        ****** %p\n", hWnd);
					//skipInput = false; //TODO when the hell is WM_SETFOCUS sent? seems like only when mouse is capped
					break;
				case WM_KILLFOCUS:
					Console.WriteLn("******      WM_KILLFOCUS        ****** %p\n", hWnd);
					//skipInput = true;
					break;
				case WM_SIZE:
					if (cursorCaptured)
						WindowResized(hWnd);
					break;
				case WM_DESTROY:
					if (eatenWnd == nullptr)
						RegisterRaw(nullptr);
					Uninitialize();
					break;
			}

			if (eatenWndProc)
				return CallWindowProc(eatenWndProc, hWnd, uMsg, wParam, lParam);
			//else
			//	return DefWindowProc(hWnd, uMsg, wParam, lParam);
			return 0;
		}

		static LRESULT CALLBACK HookProc(INT code, WPARAM wParam, LPARAM lParam)
		{
			MSG* msg = reinterpret_cast<MSG*>(lParam);

			//Console.Warning("hook: %d, %d, %d\n", code, wParam, lParam);
			if (code == HC_ACTION)
				RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
			return CallNextHookEx(hHook, code, wParam, lParam);
		}

		static LRESULT CALLBACK HookWndProc(INT code, WPARAM wParam, LPARAM lParam)
		{
			MSG* msg = reinterpret_cast<MSG*>(lParam);

			//Console.Warning("hook: %d, %d, %d\n", code, wParam, lParam);
			if (code == HC_ACTION)
				MyWndProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
			return CallNextHookEx(hHookWnd, code, wParam, lParam);
		}

		static LRESULT CALLBACK KBHookProc(INT code, WPARAM wParam, LPARAM lParam)
		{
			Console.Warning("kb hook: %d, %zd, %zd\n", code, wParam, lParam);
			KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
			//if(code == HC_ACTION)
			//	RawInputProc(msg->hwnd, msg->message, msg->wParam, msg->lParam);
			return CallNextHookEx(0, code, wParam, lParam);
		}

		int Initialize(void* ptr)
		{
			HWND hWnd = reinterpret_cast<HWND>(ptr);
			if (!InitHid())
				return 0;

#if 0
	if (!RegisterRaw(hWnd))
		return 0;
	hHook = SetWindowsHookEx(WH_GETMESSAGE, HookProc, hInst, 0);
	//hHookWnd = SetWindowsHookEx(WH_CALLWNDPROC, HookWndProc, hInst, 0);
	//hHookKB = SetWindowsHookEx(WH_KEYBOARD_LL, KBHookProc, hInst, 0);
	//int err = GetLastError();
#else
			eatenWnd = hWnd;
			eatenWndProc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)RawInputProc);
			RegisterRaw(hWnd);
#endif
			return 1;
		}

		void Uninitialize()
		{
			if (hHook)
			{
				UnhookWindowsHookEx(hHook);
				//UnhookWindowsHookEx(hHookKB);
				hHook = 0;
			}
			if (eatenWnd)
				RegisterRaw(nullptr);
			if (eatenWnd && eatenWndProc)
				SetWindowLongPtr(eatenWnd, GWLP_WNDPROC, (LONG_PTR)eatenWndProc);
			eatenWndProc = nullptr;
			eatenWnd = nullptr;
		}

	} // namespace rawinput
} // namespace shared
