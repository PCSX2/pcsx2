// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Updater.h"
#include "Windows/resource.h"

#include "common/FileSystem.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/ProgressCallback.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include <CommCtrl.h>
#include <shellapi.h>
#include <Shobjidl.h>

#include <thread>

#include <wil/resource.h>
#include <wil/win32_helpers.h>

#pragma comment(lib, "synchronization.lib")

class Win32ProgressCallback final : public BaseProgressCallback
{
public:
	Win32ProgressCallback();
	~Win32ProgressCallback() override;

	void PushState() override;
	void PopState() override;

	void SetCancellable(bool cancellable) override;
	void SetTitle(const char* title) override;
	void SetStatusText(const char* text) override;
	void SetProgressRange(u32 range) override;
	void SetProgressValue(u32 value) override;
	void SetProgressState(ProgressState state) override;

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

	void UIThreadProc(wil::slim_event_manual_reset* creationEvent);

	std::thread m_ui_thread;

	HWND m_window_hwnd{};
	HWND m_text_hwnd{};
	HWND m_progress_hwnd{};
	HWND m_list_box_hwnd{};

	wil::com_ptr_nothrow<ITaskbarList3> m_taskbar_list;

	int m_last_progress_percent = -1;
	bool m_com_initialized = false;

	static inline const UINT s_uTBBC = RegisterWindowMessageW(L"TaskbarButtonCreated");
	static constexpr UINT WMAPP_SETTASKBARPROGRESS = WM_USER+0;
	static constexpr UINT WMAPP_SETTASKBARSTATE = WM_USER+1;
};

Win32ProgressCallback::Win32ProgressCallback()
	: BaseProgressCallback()
{
	wil::slim_event_manual_reset creationEvent;
	m_ui_thread = std::thread(&Win32ProgressCallback::UIThreadProc, this, &creationEvent);
	creationEvent.wait();
}

Win32ProgressCallback::~Win32ProgressCallback()
{
	if (m_window_hwnd)
		PostMessageW(m_window_hwnd, WM_CLOSE, 0, 0);
	m_ui_thread.join();
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

void Win32ProgressCallback::SetProgressState(ProgressState state)
{
	BaseProgressCallback::SetProgressState(state);
	Redraw(true);
}

void Win32ProgressCallback::UIThreadProc(wil::slim_event_manual_reset* creationEvent)
{
	if (Create())
	{
		creationEvent->SetEvent();

		MSG msg;
		while (GetMessageW(&msg, m_window_hwnd, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		Destroy();
	}
	else
	{
		creationEvent->SetEvent();
	}
}

bool Win32ProgressCallback::Create()
{
	static const wchar_t* CLASS_NAME = L"PCSX2Win32ProgressCallbackWindow";
	static bool class_registered = false;

	if (!class_registered)
	{
		InitCommonControls();

		WNDCLASSEX wc = {sizeof(wc)};
		wc.lpfnWndProc = WndProcThunk;
		wc.hInstance = wil::GetModuleInstanceHandle();
		wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor = LoadCursor(NULL, IDC_WAIT);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.style = CS_NOCLOSE;
		wc.lpszClassName = CLASS_NAME;
		if (!RegisterClassExW(&wc))
		{
			MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK);
			return false;
		}

		class_registered = true;
	}

	m_com_initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

	m_window_hwnd =
		CreateWindowExW(WS_EX_CLIENTEDGE, CLASS_NAME, L"Win32ProgressCallback", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT,
			CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, nullptr, nullptr, wil::GetModuleInstanceHandle(), this);
	if (!m_window_hwnd)
	{
		MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK);
		return false;
	}

	ShowWindow(m_window_hwnd, SW_SHOW);

	// Pump messages manually just this one time to give the chance for the taskbar icon to be created etc.
	PumpMessages();
	return true;
}

void Win32ProgressCallback::Destroy()
{
	m_taskbar_list.reset();

	if (m_window_hwnd)
	{
		DestroyWindow(m_window_hwnd);
		m_window_hwnd = {};
		m_text_hwnd = {};
		m_progress_hwnd = {};
	}

	if (m_com_initialized)
		CoUninitialize();
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
		return;
	}

	m_last_progress_percent = percent;

	const LONG_PTR style = GetWindowLongPtrW(m_progress_hwnd, GWL_STYLE);
	LONG_PTR newStyle = style;
	if (m_progress_state == ProgressState::Normal)
	{
		newStyle = style & ~(PBS_MARQUEE);
		SendMessageW(m_window_hwnd, WMAPP_SETTASKBARPROGRESS, m_progress_value, m_progress_range);
	}
	else if (m_progress_state == ProgressState::Indeterminate)
	{
		newStyle = style | PBS_MARQUEE;
		SendMessageW(m_window_hwnd, WMAPP_SETTASKBARSTATE, TBPF_INDETERMINATE, 0);
	}
	
	if (style != newStyle)
	{
		SetWindowLongPtrW(m_progress_hwnd, GWL_STYLE, newStyle);
		SendMessageW(m_progress_hwnd, PBM_SETMARQUEE, m_progress_state == ProgressState::Indeterminate, 0);
	}
	if (m_progress_state != ProgressState::Indeterminate)
	{
		SendMessageW(m_progress_hwnd, PBM_SETRANGE, 0, MAKELPARAM(0, m_progress_range));
		SendMessageW(m_progress_hwnd, PBM_SETPOS, static_cast<WPARAM>(m_progress_value), 0);
	}
	SetWindowTextW(m_text_hwnd, StringUtil::UTF8StringToWideString(m_status_text).c_str());
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

			// In case the application is run elevated, allow the
			// TaskbarButtonCreated message through.
			ChangeWindowMessageFilterEx(hwnd, s_uTBBC, MSGFLT_ALLOW, nullptr);

			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		}
		break;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WMAPP_SETTASKBARPROGRESS:
		{
			if (m_taskbar_list)
				m_taskbar_list->SetProgressValue(m_window_hwnd, wparam, lparam);
		}
		break;

		case WMAPP_SETTASKBARSTATE:
		{
			if (m_taskbar_list)
				m_taskbar_list->SetProgressState(m_window_hwnd, static_cast<TBPFLAG>(wparam));
		}
		break;

		default:
			if (msg == s_uTBBC)
			{
				if (m_com_initialized)
				{
					m_taskbar_list = wil::CoCreateInstanceNoThrow<ITaskbarList3>(CLSID_TaskbarList);
					if (m_taskbar_list)
					{
						if (FAILED(m_taskbar_list->HrInit()))
						{
							m_taskbar_list.reset();
						}
					}
				}

				return 0;
			}
			return DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	return 0;
}

void Win32ProgressCallback::DisplayError(const char* message)
{
	Console.Error(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

void Win32ProgressCallback::DisplayWarning(const char* message)
{
	Console.Warning(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

void Win32ProgressCallback::DisplayInformation(const char* message)
{
	Console.WriteLn(message);
	SendMessageW(m_list_box_hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtil::UTF8StringToWideString(message).c_str()));
	SendMessageW(m_list_box_hwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

void Win32ProgressCallback::DisplayDebugMessage(const char* message)
{
	Console.WriteLn(message);
}

void Win32ProgressCallback::ModalError(const char* message)
{
	MessageBoxW(m_window_hwnd, StringUtil::UTF8StringToWideString(message).c_str(), L"Error", MB_ICONERROR | MB_OK);
}

bool Win32ProgressCallback::ModalConfirmation(const char* message)
{
	return MessageBoxW(m_window_hwnd, StringUtil::UTF8StringToWideString(message).c_str(), L"Confirmation", MB_ICONQUESTION | MB_YESNO) == IDYES;
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
	Win32ProgressCallback progress;

	const bool com_initialized = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	const ScopedGuard com_guard = [com_initialized]() {
		if (com_initialized)
			CoUninitialize();
	};

	int argc = 0;
	wil::unique_hlocal_ptr<LPWSTR[]> argv(CommandLineToArgvW(GetCommandLineW(), &argc));
	if (!argv || argc <= 0)
	{
		progress.ModalError("Failed to parse command line.");
		return 1;
	}
	if (argc != 5)
	{
		progress.ModalError("Expected 4 arguments: parent process id, output directory, update zip, program to "
							"launch.\n\nThis program is not intended to be run manually, please use the main PCSX2 application and "
							"click Help->Check for Updates.");
		return 1;
	}

	const int parent_process_id = StringUtil::FromChars<int>(StringUtil::WideStringToUTF8String(argv[1])).value_or(0);
	const std::string destination_directory = StringUtil::WideStringToUTF8String(argv[2]);
	const std::string zip_path = StringUtil::WideStringToUTF8String(argv[3]);
	const std::wstring program_to_launch(argv[4]);
	argv.reset();

	if (parent_process_id <= 0 || destination_directory.empty() || zip_path.empty() || program_to_launch.empty())
	{
		progress.ModalError("One or more parameters is empty.");
		return 1;
	}

	Updater::SetupLogging(&progress, destination_directory);
	Updater updater(&progress);

	progress.SetFormattedStatusText("Waiting for parent process %d to exit...", parent_process_id);
	progress.SetProgressState(ProgressCallback::ProgressState::Indeterminate);
	WaitForProcessToExit(parent_process_id);

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
			"Failed to commit update. Your installation may be corrupted, please re-download a fresh version from pcsx2.net.");
		return 1;
	}

	updater.CleanupStagingDirectory();
	updater.RemoveUpdateZip();

	// Rename the new executable to match the existing one
	if (std::string actual_exe = updater.FindPCSX2Exe(); !actual_exe.empty())
	{
		const std::string full_path = destination_directory + FS_OSPATH_SEPARATOR_STR + actual_exe;
		progress.DisplayFormattedInformation("Moving '%s' to '%S'", full_path.c_str(), program_to_launch.c_str());
		const bool ok = MoveFileExW(FileSystem::GetWin32Path(full_path).c_str(),
			FileSystem::GetWin32Path(StringUtil::WideStringToUTF8String(program_to_launch)).c_str(),
			MOVEFILE_REPLACE_EXISTING);
		if (!ok)
		{
			progress.DisplayFormattedModalError("Failed to rename '%s' to %S", full_path.c_str(), program_to_launch.c_str());
			return 1;
		}
	}
	else
	{
		progress.ModalError("Couldn't find PCSX2 in update package, please re-download a fresh version from GitHub.");
		return 1;
	}

	progress.DisplayFormattedInformation("Launching '%s'...",
		StringUtil::WideStringToUTF8String(program_to_launch).c_str());
	ShellExecuteW(nullptr, L"open", program_to_launch.c_str(), L"-updatecleanup", nullptr, SW_SHOWNORMAL);
	return 0;
}
