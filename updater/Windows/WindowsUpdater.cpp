/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Updater.h"
#include "Windows/resource.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/ProgressCallback.h"
#include "common/RedtapeWindows.h"

#include <CommCtrl.h>
#include <shellapi.h>

class Win32ProgressCallback final : public BaseProgressCallback
{
public:
	Win32ProgressCallback();

	void PushState() override;
	void PopState() override;

	void SetCancellable(bool cancellable) override;
	void SetTitle(const char* title) override;
	void SetStatusText(const char* text) override;
	void SetProgressRange(u32 range) override;
	void SetProgressValue(u32 value) override;

	void DisplayError(const char* message) override;
	void DisplayWarning(const char* message) override;
	void DisplayInformation(const char* message) override;
	void DisplayDebugMessage(const char* message) override;

	void ModalError(const char* message) override;
	bool ModalConfirmation(const char* message) override;
	void ModalInformation(const char* message) override;

private:
	enum : int
	{
		WINDOW_WIDTH = 600,
		WINDOW_HEIGHT = 300,
		WINDOW_MARGIN = 10,
		SUBWINDOW_WIDTH = WINDOW_WIDTH - 20 - WINDOW_MARGIN - WINDOW_MARGIN,
	};

	bool Create();
	void Destroy();
	void Redraw(bool force);
	void PumpMessages();

	static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	HWND m_window_hwnd{};
	HWND m_text_hwnd{};
	HWND m_progress_hwnd{};
	HWND m_list_box_hwnd{};

	int m_last_progress_percent = -1;
};

Win32ProgressCallback::Win32ProgressCallback()
	: BaseProgressCallback()
{
	Create();
}

void Win32ProgressCallback::PushState()
{
	BaseProgressCallback::PushState();
}

void Win32ProgressCallback::PopState()
{
	BaseProgressCallback::PopState();
	Redraw(true);
}

void Win32ProgressCallback::SetCancellable(bool cancellable)
{
	BaseProgressCallback::SetCancellable(cancellable);
	Redraw(true);
}

void Win32ProgressCallback::SetTitle(const char* title)
{
	SetWindowTextW(m_window_hwnd, StringUtil::UTF8StringToWideString(title).c_str());
}

void Win32ProgressCallback::SetStatusText(const char* text)
{
	BaseProgressCallback::SetStatusText(text);
	Redraw(true);
}

void Win32ProgressCallback::SetProgressRange(u32 range)
{
	BaseProgressCallback::SetProgressRange(range);
	Redraw(false);
}

void Win32ProgressCallback::SetProgressValue(u32 value)
{
	BaseProgressCallback::SetProgressValue(value);
	Redraw(false);
}

bool Win32ProgressCallback::Create()
{
	static const wchar_t* CLASS_NAME = L"PCSX2Win32ProgressCallbackWindow";
	static bool class_registered = false;

	if (!class_registered)
	{
		InitCommonControls();

		WNDCLASSEX wc = {};
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpfnWndProc = WndProcThunk;
		wc.hInstance = GetModuleHandle(nullptr);
		wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor = LoadCursor(NULL, IDC_WAIT);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = CLASS_NAME;
		if (!RegisterClassExW(&wc))
		{
			MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK);
			return false;
		}

		class_registered = true;
	}

	m_window_hwnd =
		CreateWindowExW(WS_EX_CLIENTEDGE, CLASS_NAME, L"Win32ProgressCallback", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, GetModuleHandle(nullptr), this);
	if (!m_window_hwnd)
	{
		MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK);
		return false;
	}

	SetWindowLongPtr(m_window_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	ShowWindow(m_window_hwnd, SW_SHOW);
	PumpMessages();
	return true;
}

void Win32ProgressCallback::Destroy()
{
	if (!m_window_hwnd)
		return;

	DestroyWindow(m_window_hwnd);
	m_window_hwnd = {};
	m_text_hwnd = {};
	m_progress_hwnd = {};
}

void Win32ProgressCallback::PumpMessages()
{
	MSG msg;
	while (PeekMessageW(&msg, m_window_hwnd, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}

void Win32ProgressCallback::Redraw(bool force)
{
	const int percent =
		static_cast<int>((static_cast<float>(m_progress_value) / static_cast<float>(m_progress_range)) * 100.0f);
	if (percent == m_last_progress_percent && !force)
	{
		PumpMessages();
		return;
	}

	m_last_progress_percent = percent;

	SendMessageW(m_progress_hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, m_progress_range));
	SendMessageW(m_progress_hwnd, PBM_SETPOS, static_cast<WPARAM>(m_progress_value), 0);
	SetWindowTextW(m_text_hwnd, StringUtil::UTF8StringToWideString(m_status_text).c_str());
	RedrawWindow(m_text_hwnd, nullptr, nullptr, RDW_INVALIDATE);
	PumpMessages();
}

LRESULT CALLBACK Win32ProgressCallback::WndProcThunk(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Win32ProgressCallback* cb;
	if (msg == WM_CREATE)
	{
		const CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
		cb = static_cast<Win32ProgressCallback*>(cs->lpCreateParams);
	}
	else
	{
		cb = reinterpret_cast<Win32ProgressCallback*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	}

	return cb->WndProc(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK Win32ProgressCallback::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
		case WM_CREATE:
		{
			const CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lparam);
			HFONT default_font = reinterpret_cast<HFONT>(GetStockObject(ANSI_VAR_FONT));
			SendMessageW(hwnd, WM_SETFONT, WPARAM(default_font), TRUE);

			int y = WINDOW_MARGIN;

			m_text_hwnd = CreateWindowExW(0, L"Static", nullptr, WS_VISIBLE | WS_CHILD, WINDOW_MARGIN, y, SUBWINDOW_WIDTH, 16,
				hwnd, nullptr, cs->hInstance, nullptr);
			SendMessageW(m_text_hwnd, WM_SETFONT, WPARAM(default_font), TRUE);
			y += 16 + WINDOW_MARGIN;

			m_progress_hwnd = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_VISIBLE | WS_CHILD, WINDOW_MARGIN, y,
				SUBWINDOW_WIDTH, 32, hwnd, nullptr, cs->hInstance, nullptr);
			y += 32 + WINDOW_MARGIN;

			m_list_box_hwnd =
				CreateWindowExW(0, L"LISTBOX", nullptr, WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_BORDER | LBS_NOSEL,
					WINDOW_MARGIN, y, SUBWINDOW_WIDTH, 170, hwnd, nullptr, cs->hInstance, nullptr);
			SendMessageW(m_list_box_hwnd, WM_SETFONT, WPARAM(default_font), TRUE);
			y += 170;
		}
		break;

		default:
			return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	return 0;
}

void Win32ProgressCallback::DisplayError(const char* message)
{
	Console.Error(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	PumpMessages();
}

void Win32ProgressCallback::DisplayWarning(const char* message)
{
	Console.Warning(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	PumpMessages();
}

void Win32ProgressCallback::DisplayInformation(const char* message)
{
	Console.WriteLn(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
	PumpMessages();
}

void Win32ProgressCallback::DisplayDebugMessage(const char* message)
{
	Console.WriteLn(message);
}

void Win32ProgressCallback::ModalError(const char* message)
{
	PumpMessages();
	MessageBoxW(m_window_hwnd, StringUtil::UTF8StringToWideString(message).c_str(), L"Error", MB_ICONERROR | MB_OK);
	PumpMessages();
}

bool Win32ProgressCallback::ModalConfirmation(const char* message)
{
	PumpMessages();
	bool result = MessageBoxW(m_window_hwnd, StringUtil::UTF8StringToWideString(message).c_str(), L"Confirmation", MB_ICONQUESTION | MB_YESNO) == IDYES;
	PumpMessages();
	return result;
}

void Win32ProgressCallback::ModalInformation(const char* message)
{
	MessageBoxW(m_window_hwnd, StringUtil::UTF8StringToWideString(message).c_str(), L"Information", MB_ICONINFORMATION | MB_OK);
}


static void WaitForProcessToExit(int process_id)
{
	HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, process_id);
	if (!hProcess)
		return;

	WaitForSingleObject(hProcess, INFINITE);
	CloseHandle(hProcess);
}

#include "UpdaterExtractor.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	Win32ProgressCallback progress;

	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(lpCmdLine, &argc);
	if (!argv || argc <= 0)
	{
		progress.ModalError("Failed to parse command line.");
		return 1;
	}
	if (argc != 4)
	{
		progress.ModalError("Expected 4 arguments: parent process id, output directory, update zip, program to "
							"launch.\n\nThis program is not intended to be run manually, please use the Qt frontend and "
							"click Help->Check for Updates.");
		LocalFree(argv);
		return 1;
	}

	const int parent_process_id = StringUtil::FromChars<int>(StringUtil::WideStringToUTF8String(argv[0])).value_or(0);
	const std::string destination_directory = StringUtil::WideStringToUTF8String(argv[1]);
	const std::string zip_path = StringUtil::WideStringToUTF8String(argv[2]);
	const std::wstring program_to_launch(argv[3]);
	LocalFree(argv);

	if (parent_process_id <= 0 || destination_directory.empty() || zip_path.empty() || program_to_launch.empty())
	{
		progress.ModalError("One or more parameters is empty.");
		return 1;
	}

	Updater::SetupLogging(&progress, destination_directory);

	progress.SetFormattedStatusText("Waiting for parent process %d to exit...", parent_process_id);
	WaitForProcessToExit(parent_process_id);

	Updater updater(&progress);
	if (!updater.Initialize(destination_directory))
	{
		progress.ModalError("Failed to initialize updater.");
		return 1;
	}

	if (!updater.OpenUpdateZip(zip_path.c_str()))
	{
		progress.DisplayFormattedModalError("Could not open update zip '%s'. Update not installed.", zip_path.c_str());
		return 1;
	}

	if (!updater.PrepareStagingDirectory())
	{
		progress.ModalError("Failed to prepare staging directory. Update not installed.");
		return 1;
	}

	if (!updater.StageUpdate())
	{
		progress.ModalError("Failed to stage update. Update not installed.");
		return 1;
	}

	if (!updater.CommitUpdate())
	{
		progress.ModalError(
			"Failed to commit update. Your installation may be corrupted, please re-download a fresh version from GitHub.");
		return 1;
	}

	updater.CleanupStagingDirectory();
	updater.RemoveUpdateZip();

	progress.ModalInformation("Update complete.");

	progress.DisplayFormattedInformation("Launching '%s'...",
		StringUtil::WideStringToUTF8String(program_to_launch).c_str());
	ShellExecuteW(nullptr, L"open", program_to_launch.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	return 0;
}
