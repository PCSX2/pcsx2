/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSdx.h"
#include "GSUtil.h"
#include "Renderers/SW/GSRendererSW.h"
#include "Renderers/Null/GSRendererNull.h"
#include "Renderers/Null/GSDeviceNull.h"
#include "Renderers/OpenGL/GSDeviceOGL.h"
#include "Renderers/OpenGL/GSRendererOGL.h"
#include "GSLzma.h"

#ifdef _WIN32

#include "Renderers/DX11/GSRendererDX11.h"
#include "Renderers/DX11/GSDevice11.h"
#include "Window/GSWndDX.h"
#include "Window/GSWndWGL.h"
#include "Window/GSSettingsDlg.h"

static HRESULT s_hr = E_FAIL;

#else

#include "Window/GSWndEGL.h"

#ifdef __APPLE__
#include <gtk/gtk.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

extern bool RunLinuxDialog();

#endif

#define PS2E_LT_GS 0x01
#define PS2E_GS_VERSION 0x0006
#define PS2E_X86 0x01    // 32 bit
#define PS2E_X86_64 0x02 // 64 bit

static GSRenderer* s_gs = NULL;
static void (*s_irq)() = NULL;
static uint8* s_basemem = NULL;
static int s_vsync = 0;
static bool s_exclusive = true;
static std::string s_renderer_name;
bool gsopen_done = false; // crash guard for GSgetTitleInfo2 and GSKeyEvent (replace with lock?)

EXPORT_C_(uint32) PS2EgetLibType()
{
	return PS2E_LT_GS;
}

EXPORT_C_(const char*) PS2EgetLibName()
{
	return GSUtil::GetLibName();
}

EXPORT_C_(uint32) PS2EgetLibVersion2(uint32 type)
{
	const uint32 revision = 1;
	const uint32 build = 2;

	return (build << 0) | (revision << 8) | (PS2E_GS_VERSION << 16) | (PLUGIN_VERSION << 24);
}

EXPORT_C_(uint32) PS2EgetCpuPlatform()
{
#ifdef _M_AMD64

	return PS2E_X86_64;

#else

	return PS2E_X86;

#endif
}

EXPORT_C GSsetBaseMem(uint8* mem)
{
	s_basemem = mem;

	if (s_gs)
	{
		s_gs->SetRegsMem(s_basemem);
	}
}

EXPORT_C GSsetSettingsDir(const char* dir)
{
	theApp.SetConfigDir(dir);
}

EXPORT_C_(int) GSinit()
{
	if (!GSUtil::CheckSSE())
	{
		return -1;
	}

	// Vector instructions must be avoided when initialising GSdx since PCSX2
	// can crash if the CPU does not support the instruction set.
	// Initialise it here instead - it's not ideal since we have to strip the
	// const type qualifier from all the affected variables.
	theApp.Init();

	GSUtil::Init();

	if (g_const == nullptr)
		return -1;
	else
		g_const->Init();

#ifdef _WIN32
	s_hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

	return 0;
}

EXPORT_C GSshutdown()
{
	gsopen_done = false;

	delete s_gs;
	s_gs = nullptr;

	theApp.SetCurrentRendererType(GSRendererType::Undefined);

#ifdef _WIN32
	if (SUCCEEDED(s_hr))
	{
		::CoUninitialize();

		s_hr = E_FAIL;
	}
#endif
}

EXPORT_C GSclose()
{
	gsopen_done = false;

	if (s_gs == NULL)
		return;

	s_gs->ResetDevice();

	// Opengl requirement: It must be done before the Detach() of
	// the context
	delete s_gs->m_dev;

	s_gs->m_dev = NULL;

	if (s_gs->m_wnd)
	{
		s_gs->m_wnd->Detach();
	}
}

static int _GSopen(void** dsp, const char* title, GSRendererType renderer, int threads = -1)
{
	GSDevice* dev = NULL;
	bool old_api = *dsp == NULL;

	// Fresh start up or config file changed
	if (renderer == GSRendererType::Undefined)
	{
		renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));
#ifdef _WIN32
		if (renderer == GSRendererType::Default)
			renderer = GSUtil::GetBestRenderer();
#endif
	}

	if (threads == -1)
	{
		threads = theApp.GetConfigI("extrathreads");
	}

	try
	{
		if (theApp.GetCurrentRendererType() != renderer)
		{
			// Emulator has made a render change request, which requires a completely
			// new s_gs -- if the emu doesn't save/restore the GS state across this
			// GSopen call then they'll get corrupted graphics, but that's not my problem.

			delete s_gs;

			s_gs = NULL;

			theApp.SetCurrentRendererType(renderer);
		}

		std::shared_ptr<GSWnd> window;
		{
			// Select the window first to detect the GL requirement
			std::vector<std::shared_ptr<GSWnd>> wnds;
			switch (renderer)
			{
				case GSRendererType::OGL_HW:
				case GSRendererType::OGL_SW:
#if defined(__unix__)
					// Note: EGL code use GLX otherwise maybe it could be also compatible with Windows
					// Yes OpenGL code isn't complicated enough !
					switch (GSWndEGL::SelectPlatform())
					{
#if GS_EGL_X11
						case EGL_PLATFORM_X11_KHR:
							wnds.push_back(std::make_shared<GSWndEGL_X11>());
							break;
#endif
#if GS_EGL_WL
						case EGL_PLATFORM_WAYLAND_KHR:
							wnds.push_back(std::make_shared<GSWndEGL_WL>());
							break;
#endif
						default:
							break;
					}
#elif defined(__APPLE__)
					// No windows available for macOS at the moment
#else
					wnds.push_back(std::make_shared<GSWndWGL>());
#endif
					break;
				default:
#ifdef _WIN32
					wnds.push_back(std::make_shared<GSWndDX>());
#elif defined(__APPLE__)
					// No windows available for macOS at the moment
#else
					wnds.push_back(std::make_shared<GSWndEGL_X11>());
#endif
					break;
			}

			int w = theApp.GetConfigI("ModeWidth");
			int h = theApp.GetConfigI("ModeHeight");
#if defined(__unix__)
			void* win_handle = (void*)((uptr*)(dsp) + 1);
#else
			void* win_handle = *dsp;
#endif

			for (auto& wnd : wnds)
			{
				try
				{
					if (old_api)
					{
						// old-style API expects us to create and manage our own window:
						wnd->Create(title, w, h);

						wnd->Show();

						*dsp = wnd->GetDisplay();
					}
					else
					{
						wnd->Attach(win_handle, false);
					}

					window = wnd; // Previous code will throw if window isn't supported

					break;
				}
				catch (GSDXRecoverableError)
				{
					wnd->Detach();
				}
			}

			if (!window)
			{
				GSclose();

				return -1;
			}
		}

		std::string renderer_name;

		switch (renderer)
		{
			default:
#ifdef _WIN32
			case GSRendererType::DX1011_HW:
				dev = new GSDevice11();
				s_renderer_name = "D3D11";
				renderer_name = "Direct3D 11";
				break;
#endif
			case GSRendererType::OGL_HW:
				dev = new GSDeviceOGL();
				s_renderer_name = "OGL";
				renderer_name = "OpenGL";
				break;
			case GSRendererType::OGL_SW:
				dev = new GSDeviceOGL();
				s_renderer_name = "SW";
				renderer_name = "Software";
				break;
			case GSRendererType::Null:
				dev = new GSDeviceNull();
				s_renderer_name = "NULL";
				renderer_name = "Null";
				break;
		}

		printf("Current Renderer: %s\n", renderer_name.c_str());

		if (dev == NULL)
		{
			return -1;
		}

		if (s_gs == NULL)
		{
			switch (renderer)
			{
				default:
#ifdef _WIN32
				case GSRendererType::DX1011_HW:
					s_gs = (GSRenderer*)new GSRendererDX11();
					break;
#endif
				case GSRendererType::OGL_HW:
					s_gs = (GSRenderer*)new GSRendererOGL();
					break;
				case GSRendererType::OGL_SW:
					s_gs = new GSRendererSW(threads);
					break;
				case GSRendererType::Null:
					s_gs = new GSRendererNull();
					break;
			}
			if (s_gs == NULL)
				return -1;
		}

		s_gs->m_wnd = window;
	}
	catch (std::exception& ex)
	{
		// Allowing std exceptions to escape the scope of the plugin callstack could
		// be problematic, because of differing typeids between DLL and EXE compilations.
		// ('new' could throw std::alloc)

		printf("GSdx error: Exception caught in GSopen: %s", ex.what());

		return -1;
	}

	s_gs->SetRegsMem(s_basemem);
	s_gs->SetIrqCallback(s_irq);
	s_gs->SetVSync(s_vsync);

	if (!old_api)
		s_gs->SetMultithreaded(true);

	if (!s_gs->CreateDevice(dev))
	{
		// This probably means the user has DX11 configured with a video card that is only DX9
		// compliant.  Cound mean drivr issues of some sort also, but to be sure, that's the most
		// common cause of device creation errors. :)  --air

		GSclose();

		return -1;
	}

	if (renderer == GSRendererType::OGL_HW && theApp.GetConfigI("debug_glsl_shader") == 2)
	{
		printf("GSdx: test OpenGL shader. Please wait...\n\n");
		static_cast<GSDeviceOGL*>(s_gs->m_dev)->SelfShaderTest();
		printf("\nGSdx: test OpenGL shader done. It will now exit\n");
		return -1;
	}

	return 0;
}

EXPORT_C_(void) GSosdLog(const char* utf8, uint32 color)
{
	if (s_gs && s_gs->m_dev)
		s_gs->m_dev->m_osd.Log(utf8);
}

EXPORT_C_(void) GSosdMonitor(const char* key, const char* value, uint32 color)
{
	if (s_gs && s_gs->m_dev)
		s_gs->m_dev->m_osd.Monitor(key, value);
}

EXPORT_C_(int) GSopen2(void** dsp, uint32 flags)
{
	static bool stored_toggle_state = false;
	const bool toggle_state = !!(flags & 4);
	GSRendererType current_renderer = static_cast<GSRendererType>(flags >> 24);
	if (current_renderer == GSRendererType::NO_RENDERER)
		current_renderer = theApp.GetCurrentRendererType();

	if (current_renderer != GSRendererType::Undefined && stored_toggle_state != toggle_state)
	{
		// SW -> HW and HW -> SW (F9 Switch)
		switch (current_renderer)
		{
#ifdef _WIN32
			case GSRendererType::DX1011_HW:
				current_renderer = GSRendererType::OGL_SW;
				break;
#endif
			case GSRendererType::OGL_SW:
#ifdef _WIN32
			{
				const auto config_renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));

				if (current_renderer == config_renderer)
					current_renderer = GSUtil::GetBestRenderer();
				else
					current_renderer = config_renderer;
			}
#else
				current_renderer = GSRendererType::OGL_HW;
#endif
			break;
			case GSRendererType::OGL_HW:
				current_renderer = GSRendererType::OGL_SW;
				break;
			default:
				current_renderer = GSRendererType::OGL_SW;
				break;
		}
	}
	stored_toggle_state = toggle_state;

	int retval = _GSopen(dsp, "", current_renderer);

	if (s_gs != NULL)
		s_gs->SetAspectRatio(0); // PCSX2 manages the aspect ratios

	gsopen_done = true;

	return retval;
}

EXPORT_C_(int) GSopen(void** dsp, const char* title, int mt)
{
	GSRendererType renderer = GSRendererType::Default;

	// Legacy GUI expects to acquire vsync from the configuration files.

	s_vsync = theApp.GetConfigI("vsync");

	if (mt == 2)
	{
		// pcsx2 sent a switch renderer request
		mt = 1;
	}
	else
	{
		// normal init

		renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));
	}

	*dsp = NULL;

	int retval = _GSopen(dsp, title, renderer);

	if (retval == 0 && s_gs)
	{
		s_gs->SetMultithreaded(!!mt);
	}

	gsopen_done = true;

	return retval;
}

EXPORT_C GSreset()
{
	try
	{
		s_gs->Reset();
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifSoftReset(uint32 mask)
{
	try
	{
		s_gs->SoftReset(mask);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSwriteCSR(uint32 csr)
{
	try
	{
		s_gs->WriteCSR(csr);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSinitReadFIFO(uint8* mem)
{
	GL_PERF("Init Read FIFO1");
	try
	{
		s_gs->InitReadFIFO(mem, 1);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSreadFIFO(uint8* mem)
{
	try
	{
		s_gs->ReadFIFO(mem, 1);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSinitReadFIFO2(uint8* mem, uint32 size)
{
	GL_PERF("Init Read FIFO2");
	try
	{
		s_gs->InitReadFIFO(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSreadFIFO2(uint8* mem, uint32 size)
{
	try
	{
		s_gs->ReadFIFO(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C GSgifTransfer(const uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<3>(mem, size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer1(uint8* mem, uint32 addr)
{
	try
	{
		s_gs->Transfer<0>(const_cast<uint8*>(mem) + addr, (0x4000 - addr) / 16);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer2(uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<1>(const_cast<uint8*>(mem), size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSgifTransfer3(uint8* mem, uint32 size)
{
	try
	{
		s_gs->Transfer<2>(const_cast<uint8*>(mem), size);
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C GSvsync(int field)
{
	try
	{
#ifdef _WIN32

		if (s_gs->m_wnd->IsManaged())
		{
			MSG msg;

			memset(&msg, 0, sizeof(msg));

			while (msg.message != WM_QUIT && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

#endif

		s_gs->VSync(field);
	}
	catch (GSDXRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GSdx: Memory allocation error\n");
	}
}

EXPORT_C_(uint32) GSmakeSnapshot(char* path)
{
	try
	{
		std::string s{path};

		if (!s.empty())
		{
			// Allows for providing a complete path
			std::string extension = s.substr(s.size() - 4, 4);
#ifdef _WIN32
			std::transform(extension.begin(), extension.end(), extension.begin(), (char(_cdecl*)(int))tolower);
#else
			std::transform(extension.begin(), extension.end(), extension.begin(), tolower);
#endif
			if (extension == ".png")
				return s_gs->MakeSnapshot(s);
			else if (s[s.length() - 1] != DIRECTORY_SEPARATOR)
				s = s + DIRECTORY_SEPARATOR;
		}

		return s_gs->MakeSnapshot(s + "gsdx");
	}
	catch (GSDXRecoverableError)
	{
		return false;
	}
}

EXPORT_C GSkeyEvent(GSKeyEventData* e)
{
	try
	{
		if (gsopen_done)
		{
			s_gs->KeyEvent(e);
		}
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C_(int) GSfreeze(int mode, GSFreezeData* data)
{
	try
	{
		if (mode == FREEZE_SAVE)
		{
			return s_gs->Freeze(data, false);
		}
		else if (mode == FREEZE_SIZE)
		{
			return s_gs->Freeze(data, true);
		}
		else if (mode == FREEZE_LOAD)
		{
			return s_gs->Defrost(data);
		}
	}
	catch (GSDXRecoverableError)
	{
	}

	return 0;
}

EXPORT_C GSconfigure()
{
	try
	{
		if (!GSUtil::CheckSSE())
			return;

		theApp.Init();

#ifdef _WIN32
		GSDialog::InitCommonControls();
		if (GSSettingsDlg().DoModal() == IDOK)
		{
			// Force a reload of the gs state
			theApp.SetCurrentRendererType(GSRendererType::Undefined);
		}

#elif defined(__APPLE__)
		// Rest of macOS UI doesn't use GTK so we need to init it now
		gtk_init(nullptr, nullptr);
		// GTK expects us to be using its event loop, rather than Cocoa's
		// If we call its stuff right now, it'll attempt to drain a static autorelease pool that was already drained by Cocoa (see https://github.com/GNOME/gtk/blob/8c1072fad1cb6a2e292fce2441b4a571f173ce0f/gdk/quartz/gdkeventloop-quartz.c#L640-L646)
		// We can convince it that touching that pool would be unsafe by running all GTK calls within a CFRunLoop
		// (Blocks submitted to the main queue by dispatch_async are run by its CFRunLoop)
		dispatch_async(dispatch_get_main_queue(), ^{
			if (RunLinuxDialog())
			{
				theApp.ReloadConfig();
				// Force a reload of the gs state
				theApp.SetCurrentRendererType(GSRendererType::Undefined);
			}
		});
#else

		if (RunLinuxDialog())
		{
			theApp.ReloadConfig();
			// Force a reload of the gs state
			theApp.SetCurrentRendererType(GSRendererType::Undefined);
		}

#endif
	}
	catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C_(int) GStest()
{
	if (!GSUtil::CheckSSE())
		return -1;

	return 0;
}

EXPORT_C GSabout()
{
}

EXPORT_C GSirqCallback(void (*irq)())
{
	s_irq = irq;

	if (s_gs)
	{
		s_gs->SetIrqCallback(s_irq);
	}
}

void pt(const char* str)
{
	struct tm* current;
	time_t now;

	time(&now);
	current = localtime(&now);

	printf("%02i:%02i:%02i%s", current->tm_hour, current->tm_min, current->tm_sec, str);
}

EXPORT_C_(bool) GSsetupRecording(std::string& filename)
{
	if (s_gs == NULL)
	{
		printf("GSdx: no s_gs for recording\n");
		return false;
	}
#if defined(__unix__) || defined(__APPLE__)
	if (!theApp.GetConfigB("capture_enabled"))
	{
		printf("GSdx: Recording is disabled\n");
		return false;
	}
#endif
	printf("GSdx: Recording start command\n");
	if (s_gs->BeginCapture(filename))
	{
		pt(" - Capture started\n");
		return true;
	}
	else
	{
		pt(" - Capture cancelled\n");
		return false;
	}
}

EXPORT_C_(void) GSendRecording()
{
	printf("GSdx: Recording end command\n");
	s_gs->EndCapture();
	pt(" - Capture ended\n");
}

EXPORT_C GSsetGameCRC(uint32 crc, int options)
{
	s_gs->SetGameCRC(crc, options);
}

EXPORT_C GSgetLastTag(uint32* tag)
{
	s_gs->GetLastTag(tag);
}

EXPORT_C GSgetTitleInfo2(char* dest, size_t length)
{
	std::string s;
	s.append(s_renderer_name);
	// TODO: this gets called from a different thread concurrently with GSOpen (on linux)
	if (gsopen_done && s_gs != NULL && s_gs->m_GStitleInfoBuffer[0])
	{
		std::lock_guard<std::mutex> lock(s_gs->m_pGSsetTitle_Crit);

		s.append(" | ").append(s_gs->m_GStitleInfoBuffer);

		if (s.size() > length - 1)
		{
			s = s.substr(0, length - 1);
		}
	}

	strcpy(dest, s.c_str());
}

EXPORT_C GSsetFrameSkip(int frameskip)
{
	s_gs->SetFrameSkip(frameskip);
}

EXPORT_C GSsetVsync(int vsync)
{
	s_vsync = vsync;

	if (s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

EXPORT_C GSsetExclusive(int enabled)
{
	s_exclusive = !!enabled;

	if (s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

#ifdef _WIN32

#include <io.h>
#include <fcntl.h>

class Console
{
	HANDLE m_console;
	std::string m_title;

public:
	Console::Console(LPCSTR title, bool open)
		: m_console(NULL)
		, m_title(title)
	{
		if (open)
			Open();
	}

	Console::~Console()
	{
		Close();
	}

	void Console::Open()
	{
		if (m_console == NULL)
		{
			CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

			AllocConsole();

			std::wstring tmp = std::wstring(m_title.begin(), m_title.end());
			SetConsoleTitle(tmp.c_str());

			m_console = GetStdHandle(STD_OUTPUT_HANDLE);

			COORD size;

			size.X = 100;
			size.Y = 300;

			SetConsoleScreenBufferSize(m_console, size);

			GetConsoleScreenBufferInfo(m_console, &csbiInfo);

			SMALL_RECT rect;

			rect = csbiInfo.srWindow;
			rect.Right = rect.Left + 99;
			rect.Bottom = rect.Top + 64;

			SetConsoleWindowInfo(m_console, TRUE, &rect);

			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);

			setvbuf(stdout, nullptr, _IONBF, 0);
			setvbuf(stderr, nullptr, _IONBF, 0);
		}
	}

	void Console::Close()
	{
		if (m_console != NULL)
		{
			FreeConsole();

			m_console = NULL;
		}
	}
};

// lpszCmdLine:
//   First parameter is the renderer.
//   Second parameter is the gs file to load and run.

EXPORT_C GSReplay(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	GSRendererType renderer = GSRendererType::Undefined;

	{
		char* start = lpszCmdLine;
		char* end = NULL;
		long n = strtol(lpszCmdLine, &end, 10);
		if (end > start)
		{
			renderer = static_cast<GSRendererType>(n);
			lpszCmdLine = end;
		}
	}

	while (*lpszCmdLine == ' ')
		lpszCmdLine++;

	::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	Console console{"GSdx", true};

	const std::string f{lpszCmdLine};
	const bool is_xz = f.size() >= 4 && f.compare(f.size() - 3, 3, ".xz") == 0;

	auto file = is_xz
		? std::unique_ptr<GSDumpFile>{std::make_unique<GSDumpLzma>(lpszCmdLine, nullptr)}
		: std::unique_ptr<GSDumpFile>{std::make_unique<GSDumpRaw>(lpszCmdLine, nullptr)};

	GSinit();

	std::array<uint8, 0x2000> regs;
	GSsetBaseMem(regs.data());

	s_vsync = theApp.GetConfigI("vsync");

	HWND hWnd = nullptr;

	_GSopen((void**)&hWnd, "", renderer);

	uint32 crc;
	file->Read(&crc, 4);
	GSsetGameCRC(crc, 0);

	{
		GSFreezeData fd;
		file->Read(&fd.size, 4);
		std::vector<uint8> freeze_data(fd.size);
		fd.data = freeze_data.data();
		file->Read(fd.data, fd.size);
		GSfreeze(FREEZE_LOAD, &fd);
	}

	file->Read(regs.data(), 0x2000);

	GSvsync(1);

	struct Packet
	{
		uint8 type, param;
		uint32 size, addr;
		std::vector<uint8> buff;
	};

	auto read_packet = [&file](uint8 type) {
		Packet p;
		p.type = type;

		switch (p.type)
		{
			case 0:
				file->Read(&p.param, 1);
				file->Read(&p.size, 4);
				switch (p.param)
				{
					case 0:
						p.buff.resize(0x4000);
						p.addr = 0x4000 - p.size;
						file->Read(&p.buff[p.addr], p.size);
						break;
					case 1:
					case 2:
					case 3:
						p.buff.resize(p.size);
						file->Read(p.buff.data(), p.size);
						break;
				}
				break;
			case 1:
				file->Read(&p.param, 1);
				break;
			case 2:
				file->Read(&p.size, 4);
				break;
			case 3:
				p.buff.resize(0x2000);
				file->Read(p.buff.data(), 0x2000);
				break;
		}

		return p;
	};

	std::list<Packet> packets;
	uint8 type;
	while (file->Read(&type, 1))
		packets.push_back(read_packet(type));

	Sleep(100);

	std::vector<uint8> buff;
	while (IsWindowVisible(hWnd))
	{
		for (auto& p : packets)
		{
			switch (p.type)
			{
				case 0:
					switch(p.param)
					{
						case 0: GSgifTransfer1(p.buff.data(), p.addr); break;
						case 1: GSgifTransfer2(p.buff.data(), p.size / 16); break;
						case 2: GSgifTransfer3(p.buff.data(), p.size / 16); break;
						case 3: GSgifTransfer(p.buff.data(), p.size / 16); break;
					}
					break;
				case 1:
					GSvsync(p.param);
					break;
				case 2:
					if(buff.size() < p.size) buff.resize(p.size);
					GSreadFIFO2(p.buff.data(), p.size / 16);
					break;
				case 3:
					memcpy(regs.data(), p.buff.data(), 0x2000);
					break;
			}
		}
	}

	Sleep(100);

	GSclose();
	GSshutdown();
}

EXPORT_C GSBenchmark(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	Console console("GSdx", true);

	if (1)
	{
		GSLocalMemory* mem = new GSLocalMemory();

		static struct {int psm; const char* name;} s_format[] =
		{
			{PSM_PSMCT32, "32"},
			{PSM_PSMCT24, "24"},
			{PSM_PSMCT16, "16"},
			{PSM_PSMCT16S, "16S"},
			{PSM_PSMT8, "8"},
			{PSM_PSMT4, "4"},
			{PSM_PSMT8H, "8H"},
			{PSM_PSMT4HL, "4HL"},
			{PSM_PSMT4HH, "4HH"},
			{PSM_PSMZ32, "32Z"},
			{PSM_PSMZ24, "24Z"},
			{PSM_PSMZ16, "16Z"},
			{PSM_PSMZ16S, "16ZS"},
		};

		uint8* ptr = (uint8*)_aligned_malloc(1024 * 1024 * 4, 32);

		for (int i = 0; i < 1024 * 1024 * 4; i++)
			ptr[i] = (uint8)i;

		//

		for (int tbw = 5; tbw <= 10; tbw++)
		{
			int n = 256 << ((10 - tbw) * 2);

			int w = 1 << tbw;
			int h = 1 << tbw;

			printf("%d x %d\n\n", w, h);

			for (size_t i = 0; i < countof(s_format); i++)
			{
				const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[s_format[i].psm];

				GSLocalMemory::writeImage wi = psm.wi;
				GSLocalMemory::readImage ri = psm.ri;
				GSLocalMemory::readTexture rtx = psm.rtx;
				GSLocalMemory::readTexture rtxP = psm.rtxP;

				GIFRegBITBLTBUF BITBLTBUF;

				BITBLTBUF.SBP = 0;
				BITBLTBUF.SBW = w / 64;
				BITBLTBUF.SPSM = s_format[i].psm;
				BITBLTBUF.DBP = 0;
				BITBLTBUF.DBW = w / 64;
				BITBLTBUF.DPSM = s_format[i].psm;

				GIFRegTRXPOS TRXPOS;

				TRXPOS.SSAX = 0;
				TRXPOS.SSAY = 0;
				TRXPOS.DSAX = 0;
				TRXPOS.DSAY = 0;

				GIFRegTRXREG TRXREG;

				TRXREG.RRW = w;
				TRXREG.RRH = h;

				GSVector4i r(0, 0, w, h);

				GIFRegTEX0 TEX0;

				TEX0.TBP0 = 0;
				TEX0.TBW = w / 64;

				GIFRegTEXA TEXA;

				TEXA.TA0 = 0;
				TEXA.TA1 = 0x80;
				TEXA.AEM = 0;

				int trlen = w * h * psm.trbpp / 8;
				int len = w * h * psm.bpp / 8;

				clock_t start, end;

				printf("[%4s] ", s_format[i].name);

				start = clock();

				for (int j = 0; j < n; j++)
				{
					int x = 0;
					int y = 0;

					(mem->*wi)(x, y, ptr, trlen, BITBLTBUF, TRXPOS, TRXREG);
				}

				end = clock();

				printf("%6d %6d | ", (int)((float)trlen * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				start = clock();

				for (int j = 0; j < n; j++)
				{
					int x = 0;
					int y = 0;

					(mem->*ri)(x, y, ptr, trlen, BITBLTBUF, TRXPOS, TRXREG);
				}

				end = clock();

				printf("%6d %6d | ", (int)((float)trlen * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				const GSOffset* off = mem->GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

				start = clock();

				for (int j = 0; j < n; j++)
				{
					(mem->*rtx)(off, r, ptr, w * 4, TEXA);
				}

				end = clock();

				printf("%6d %6d ", (int)((float)len * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				if (psm.pal > 0)
				{
					start = clock();

					for (int j = 0; j < n; j++)
					{
						(mem->*rtxP)(off, r, ptr, w, TEXA);
					}

					end = clock();

					printf("| %6d %6d ", (int)((float)len * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));
				}

				printf("\n");
			}

			printf("\n");
		}

		_aligned_free(ptr);

		delete mem;
	}

	//

	if (0)
	{
		GSLocalMemory* mem = new GSLocalMemory();

		uint8* ptr = (uint8*)_aligned_malloc(1024 * 1024 * 4, 32);

		for (int i = 0; i < 1024 * 1024 * 4; i++)
			ptr[i] = (uint8)i;

		const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[PSM_PSMCT32];

		GSLocalMemory::writeImage wi = psm.wi;

		GIFRegBITBLTBUF BITBLTBUF;

		BITBLTBUF.DBP = 0;
		BITBLTBUF.DBW = 32;
		BITBLTBUF.DPSM = PSM_PSMCT32;

		GIFRegTRXPOS TRXPOS;

		TRXPOS.DSAX = 0;
		TRXPOS.DSAY = 1;

		GIFRegTRXREG TRXREG;

		TRXREG.RRW = 256;
		TRXREG.RRH = 256;

		int trlen = 256 * 256 * psm.trbpp / 8;

		int x = 0;
		int y = 0;

		(mem->*wi)(x, y, ptr, trlen, BITBLTBUF, TRXPOS, TRXREG);

		delete mem;
	}

	//

	PostQuitMessage(0);
}

#endif

#if defined(__unix__) || defined(__APPLE__)

inline unsigned long timeGetTime()
{
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (unsigned long)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

// Note
EXPORT_C GSReplay(char* lpszCmdLine, int renderer)
{
	GLLoader::in_replayer = true;
	// Required by multithread driver
#ifndef __APPLE__
	XInitThreads();
#endif

	GSinit();

	GSRendererType m_renderer;
	// Allow to easyly switch between SW/HW renderer -> this effectively removes the ability to select the renderer by function args
	m_renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));

	if (m_renderer != GSRendererType::OGL_HW && m_renderer != GSRendererType::OGL_SW)
	{
		fprintf(stderr, "wrong renderer selected %d\n", static_cast<int>(m_renderer));
		return;
	}

	struct Packet
	{
		uint8 type, param;
		uint32 size, addr;
		std::vector<uint8> buff;
	};

	std::list<Packet*> packets;
	std::vector<uint8> buff;
	uint8 regs[0x2000];

	GSsetBaseMem(regs);

	s_vsync = theApp.GetConfigI("vsync");
	int finished = theApp.GetConfigI("linux_replay");
	bool repack_dump = (finished < 0);

	if (theApp.GetConfigI("dump"))
	{
		fprintf(stderr, "Dump is enabled. Replay will be disabled\n");
		finished = 1;
	}

	long frame_number = 0;

	void* hWnd = NULL;
	int err = _GSopen((void**)&hWnd, "", m_renderer);
	if (err != 0)
	{
		fprintf(stderr, "Error failed to GSopen\n");
		return;
	}
	if (s_gs->m_wnd == NULL)
		return;

	{ // Read .gs content
		std::string f(lpszCmdLine);
		bool is_xz = (f.size() >= 4) && (f.compare(f.size() - 3, 3, ".xz") == 0);
		if (is_xz)
			f.replace(f.end() - 6, f.end(), "_repack.gs");
		else
			f.replace(f.end() - 3, f.end(), "_repack.gs");

		GSDumpFile* file = is_xz
			? (GSDumpFile*) new GSDumpLzma(lpszCmdLine, repack_dump ? f.c_str() : nullptr)
			: (GSDumpFile*) new GSDumpRaw(lpszCmdLine, repack_dump ? f.c_str() : nullptr);

		uint32 crc;
		file->Read(&crc, 4);
		GSsetGameCRC(crc, 0);

		GSFreezeData fd;
		file->Read(&fd.size, 4);
		fd.data = new uint8[fd.size];
		file->Read(fd.data, fd.size);

		GSfreeze(FREEZE_LOAD, &fd);
		delete[] fd.data;

		file->Read(regs, 0x2000);

		uint8 type;
		while (file->Read(&type, 1))
		{
			Packet* p = new Packet();

			p->type = type;

			switch (type)
			{
				case 0:
					file->Read(&p->param, 1);
					file->Read(&p->size, 4);

					switch (p->param)
					{
						case 0:
							p->buff.resize(0x4000);
							p->addr = 0x4000 - p->size;
							file->Read(&p->buff[p->addr], p->size);
							break;
						case 1:
						case 2:
						case 3:
							p->buff.resize(p->size);
							file->Read(&p->buff[0], p->size);
							break;
					}

					break;

				case 1:
					file->Read(&p->param, 1);
					frame_number++;

					break;

				case 2:
					file->Read(&p->size, 4);

					break;

				case 3:
					p->buff.resize(0x2000);

					file->Read(&p->buff[0], 0x2000);

					break;
			}

			packets.push_back(p);

			if (repack_dump && frame_number > -finished)
				break;
		}

		delete file;
	}

	sleep(2);


	frame_number = 0;

	// Init vsync stuff
	GSvsync(1);

	while (finished > 0)
	{
		for (auto i = packets.begin(); i != packets.end(); i++)
		{
			Packet* p = *i;

			switch (p->type)
			{
				case 0:

					switch (p->param)
					{
						case 0: GSgifTransfer1(&p->buff[0], p->addr); break;
						case 1: GSgifTransfer2(&p->buff[0], p->size / 16); break;
						case 2: GSgifTransfer3(&p->buff[0], p->size / 16); break;
						case 3: GSgifTransfer(&p->buff[0], p->size / 16); break;
					}

					break;

				case 1:

					GSvsync(p->param);
					frame_number++;

					break;

				case 2:

					if (buff.size() < p->size)
						buff.resize(p->size);

					GSreadFIFO2(&buff[0], p->size / 16);

					break;

				case 3:

					memcpy(regs, &p->buff[0], 0x2000);

					break;
			}
		}

		if (finished >= 200)
		{
			; // Nop for Nvidia Profiler
		}
		else if (finished > 90)
		{
			sleep(1);
		}
		else
		{
			finished--;
		}
	}

	static_cast<GSDeviceOGL*>(s_gs->m_dev)->GenerateProfilerData();

#ifdef ENABLE_OGL_DEBUG_MEM_BW
	unsigned long total_frame_nb = std::max(1l, frame_number) << 10;
	fprintf(stderr, "memory bandwith. T: %f KB/f. V: %f KB/f. U: %f KB/f\n",
			(float)g_real_texture_upload_byte / (float)total_frame_nb,
			(float)g_vertex_upload_byte / (float)total_frame_nb,
			(float)g_uniform_upload_byte / (float)total_frame_nb);
#endif

	for (auto i = packets.begin(); i != packets.end(); i++)
	{
		delete *i;
	}

	packets.clear();

	sleep(2);

	GSclose();
	GSshutdown();
}
#endif
