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
#include "GSRendererSW.h"
#include "GSRendererNull.h"
#include "GSDeviceNull.h"
#include "GSDeviceOGL.h"
#include "GSRendererOGL.h"
#include "GSRendererCL.h"

#ifdef _WINDOWS

#include "GSRendererDX9.h"
#include "GSRendererDX11.h"
#include "GSDevice9.h"
#include "GSDevice11.h"
#include "GSWndDX.h"
#include "GSWndWGL.h"
#include "GSRendererCS.h"
#include "GSSettingsDlg.h"

static HRESULT s_hr = E_FAIL;

#else

#include "GSWndOGL.h"
#include "GSWndEGL.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

extern bool RunLinuxDialog();

#endif

#define PS2E_LT_GS 0x01
#define PS2E_GS_VERSION 0x0006
#define PS2E_X86 0x01   // 32 bit
#define PS2E_X86_64 0x02   // 64 bit

static GSRenderer* s_gs = NULL;
static void (*s_irq)() = NULL;
static uint8* s_basemem = NULL;
static GSRendererType s_renderer = GSRendererType::Undefined;
static bool s_framelimit = true;
static bool s_vsync = false;
static bool s_exclusive = true;
static const char *s_renderer_name = "";
static const char *s_renderer_type = "";
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
	const uint32 build = 0;

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

	if(s_gs)
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
	if(!GSUtil::CheckSSE())
	{
		return -1;
	}

#ifdef _WINDOWS

	s_hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if(!GSUtil::CheckDirectX())
	{
		return -1;
	}

#endif

	return 0;
}

EXPORT_C GSshutdown()
{
	gsopen_done = false;

	delete s_gs;

	s_gs = NULL;

	s_renderer = GSRendererType::Undefined;

#ifdef _WINDOWS

	if(SUCCEEDED(s_hr))
	{
		::CoUninitialize();

		s_hr = E_FAIL;
	}

#endif
}

EXPORT_C GSclose()
{
	gsopen_done = false;

	if(s_gs == NULL) return;

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

	if(renderer == GSRendererType::Undefined)
	{
		renderer = static_cast<GSRendererType>(theApp.GetConfig("Renderer", static_cast<int>(GSRendererType::Default)));
	}

	if(threads == -1)
	{
		threads = theApp.GetConfig("extrathreads", 0);
	}

	GSWnd* wnd[2] = { NULL, NULL };

	try
	{
		if (s_renderer != renderer)
		{
			// Emulator has made a render change request, which requires a completely
			// new s_gs -- if the emu doesn't save/restore the GS state across this
			// GSopen call then they'll get corrupted graphics, but that's not my problem.

			delete s_gs;

			s_gs = NULL;
		}

		const char* renderer_fullname = "";
		const char* renderer_mode = "";

		switch (renderer)
		{		
		case GSRendererType::DX9_SW:
		case GSRendererType::DX1011_SW:
		case GSRendererType::Null_SW:
		case GSRendererType::OGL_SW:
			renderer_mode = "(Software mode)";
			break;
		case GSRendererType::DX9_Null:
		case GSRendererType::DX1011_Null:
		case GSRendererType::Null_Null:
			renderer_mode = "(Null mode)";
			break;
		case GSRendererType::DX9_OpenCL:
		case GSRendererType::DX1011_OpenCL:
		case GSRendererType::Null_OpenCL:
		case GSRendererType::OGL_OpenCL:
			renderer_mode = "(OpenCL)";
			break;
		default:
			renderer_mode = "(Hardware mode)";
			break;
		}

		switch (renderer)
		{
		default:
#ifdef _WINDOWS
		case GSRendererType::DX9_HW:
		case GSRendererType::DX9_SW:
		case GSRendererType::DX9_Null:
		case GSRendererType::DX9_OpenCL:
			dev = new GSDevice9();
			s_renderer_name = " D3D9";
			renderer_fullname = "Direct3D9";
			break;
		case GSRendererType::DX1011_HW:
		case GSRendererType::DX1011_SW:
		case GSRendererType::DX1011_Null:
		case GSRendererType::DX1011_OpenCL:
			dev = new GSDevice11();
			s_renderer_name = " D3D11";
			renderer_fullname = "Direct3D11";
			break;
#endif
		case GSRendererType::Null_HW:
		case GSRendererType::Null_SW:
		case GSRendererType::Null_Null:
		case GSRendererType::Null_OpenCL:
			dev = new GSDeviceNull();
			s_renderer_name = " Null";
			renderer_fullname = "Null";
			break;
		case GSRendererType::OGL_HW:
		case GSRendererType::OGL_SW:
		case GSRendererType::OGL_OpenCL:
			dev = new GSDeviceOGL();
			s_renderer_name = " OGL";
			renderer_fullname = "OpenGL";
			break;
		}

		printf("Current Renderer: %s %s\n", renderer_fullname, renderer_mode);

		if (dev == NULL)
		{
			return -1;
		}

		if (s_gs == NULL)
		{
			switch (renderer)
			{
			default:
#ifdef _WINDOWS
			case GSRendererType::DX9_HW:
				s_gs = (GSRenderer*)new GSRendererDX9();
				s_renderer_type = " HW";
				break;
			case GSRendererType::DX1011_HW:
				s_gs = (GSRenderer*)new GSRendererDX11();
				s_renderer_type = " HW";
				break;
#endif
			case GSRendererType::OGL_HW:
				s_gs = (GSRenderer*)new GSRendererOGL();
				s_renderer_type = " HW";
				break;
			case GSRendererType::DX9_SW:
			case GSRendererType::DX1011_SW:
			case GSRendererType::Null_SW:
			case GSRendererType::OGL_SW:
				s_gs = new GSRendererSW(threads);
				s_renderer_type = " SW";
				break;
			case GSRendererType::DX9_Null:
			case GSRendererType::DX1011_Null:
			case GSRendererType::Null_Null:
				s_gs = new GSRendererNull();
				s_renderer_type = " Null";
				break;
			case GSRendererType::DX9_OpenCL:
			case GSRendererType::DX1011_OpenCL:
			case GSRendererType::Null_OpenCL:
			case GSRendererType::OGL_OpenCL:
#ifdef ENABLE_OPENCL
				s_gs = new GSRendererCL();
				s_renderer_type = " OCL";
#else
				printf("GSdx error: OpenCL is disabled\n");
#endif
				break;
			}
			if (s_gs == NULL)
				return -1;

			s_renderer = renderer;
		}

		if (s_gs->m_wnd == NULL)
		{
#ifdef _WINDOWS
			switch (renderer)
			{
			case GSRendererType::OGL_HW:
			case GSRendererType::OGL_SW:
			case GSRendererType::OGL_OpenCL:
				s_gs->m_wnd = new GSWndWGL();
				break;
			default:
				s_gs->m_wnd = new GSWndDX();
				break;
			}
#else
#ifdef EGL_SUPPORTED
			wnd[0] = new GSWndEGL();
			wnd[1] = new GSWndOGL();
#else
			wnd[0] = new GSWndOGL();
#endif
#endif
		}
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
	s_gs->SetFrameLimit(s_framelimit);

	if(*dsp == NULL)
	{
		// old-style API expects us to create and manage our own window:

		int w = theApp.GetConfig("ModeWidth", 0);
		int h = theApp.GetConfig("ModeHeight", 0);

#ifdef __linux__
		for(uint32 i = 0; i < 2; i++) {
			try
			{
				if (wnd[i] == NULL) continue;

				wnd[i]->Create(title, w, h);
				s_gs->m_wnd = wnd[i];

				if (i == 0) delete wnd[1];

				break;
			}
			catch (GSDXRecoverableError)
			{
				wnd[i]->Detach();
				delete wnd[i];
			}
		}
		if (s_gs->m_wnd == NULL)
		{
			GSclose();

			return -1;
		}
#endif
#ifdef _WINDOWS
		if(!s_gs->CreateWnd(title, w, h))
		{
			GSclose();

			return -1;
		}
#endif

		s_gs->m_wnd->Show();

		*dsp = s_gs->m_wnd->GetDisplay();
	}
	else
	{
		s_gs->SetMultithreaded(true);

#ifdef __linux__
		if (s_gs->m_wnd) {
			// A window was already attached to s_gs so we also
			// need to restore the window state (Attach)
			s_gs->m_wnd->Attach((void*)((uptr*)(dsp)+1), false);
		} else {
			// No window found, try to attach a GLX win and retry 
			// with EGL win if failed.
			for(uint32 i = 0; i < 2; i++) {
				try
				{
					if (wnd[i] == NULL) continue;

					wnd[i]->Attach((void*)((uptr*)(dsp)+1), false);
					s_gs->m_wnd = wnd[i];

					if (i == 0) delete wnd[1];

					break;
				}
				catch (GSDXRecoverableError)
				{
					wnd[i]->Detach();
					delete wnd[i];
				}
			}
		}
#endif
#ifdef _WINDOWS
		try
		{
			s_gs->m_wnd->Attach(*dsp, false);
		}
		catch (GSDXRecoverableError)
		{
			s_gs->m_wnd->Detach();
			delete s_gs->m_wnd;
			s_gs->m_wnd = NULL;
		}
#endif
		if (s_gs->m_wnd == NULL)
		{
			return -1;
		}
	}

	if(!s_gs->CreateDevice(dev))
	{
		// This probably means the user has DX11 configured with a video card that is only DX9
		// compliant.  Cound mean drivr issues of some sort also, but to be sure, that's the most
		// common cause of device creation errors. :)  --air

		GSclose();

		return -1;
	}

	if (renderer == GSRendererType::OGL_HW && theApp.GetConfig("debug_glsl_shader", 0) == 2) {
		printf("GSdx: test OpenGL shader. Please wait...\n\n");
		static_cast<GSDeviceOGL*>(s_gs->m_dev)->SelfShaderTest();
		printf("\nGSdx: test OpenGL shader done. It will now exit\n");
		return -1;
	}
	
	return 0;
}

EXPORT_C_(int) GSopen2(void** dsp, uint32 flags)
{
	static bool stored_toggle_state = false;
	bool toggle_state = !!(flags & 4);

	GSRendererType renderer = s_renderer;
	// Fresh start up or config file changed
	if (renderer == GSRendererType::Undefined)
	{
		renderer = static_cast<GSRendererType>(theApp.GetConfig("Renderer", static_cast<int>(GSRendererType::Default)));
	}
	else if (stored_toggle_state != toggle_state)
	{
#ifdef _WIN32
		GSRendererType best_sw_renderer = GSUtil::CheckDirect3D11Level() >= D3D_FEATURE_LEVEL_10_0 ? GSRendererType::DX1011_SW : GSRendererType::DX9_SW;


		switch (renderer) {
			// Use alternative renderer (SW if currently using HW renderer, and vice versa, keeping the same API and API version)
		case GSRendererType::DX9_SW: renderer = GSRendererType::DX9_HW; break;
		case GSRendererType::DX9_HW: renderer = GSRendererType::DX9_SW; break;
		case GSRendererType::DX1011_SW: renderer = GSRendererType::DX1011_HW; break;
		case GSRendererType::DX1011_HW: renderer = GSRendererType::DX1011_SW; break;
		case GSRendererType::OGL_SW: renderer = GSRendererType::OGL_HW; break;
		case GSRendererType::OGL_HW: renderer = GSRendererType::OGL_SW; break;
		default: renderer = best_sw_renderer; break;// If wasn't using one of the above mentioned ones, use best SW renderer.

		}

#endif
#ifdef __linux__
		switch(renderer) {
			// Use alternative renderer (SW if currently using HW renderer, and vice versa)
		case GSRendererType::OGL_SW: renderer = GSRendererType::OGL_HW; break;
		case GSRendererType::OGL_HW: renderer = GSRendererType::OGL_SW; break;
		default: renderer = GSRendererType::OGL_SW; break; // fallback to OGL SW
		}
#endif
	}
	stored_toggle_state = toggle_state;

	int retval = _GSopen(dsp, "", renderer);

	if (s_gs != NULL)
		s_gs->SetAspectRatio(0);	 // PCSX2 manages the aspect ratios

	gsopen_done = true;

	return retval;
}

EXPORT_C_(int) GSopen(void** dsp, const char* title, int mt)
{
	/*
	if(!XInitThreads()) return -1;

	Display* display = XOpenDisplay(0);

	XCloseDisplay(display);
	*/

	GSRendererType renderer = GSRendererType::Default;

	// Legacy GUI expects to acquire vsync from the configuration files.

	s_vsync = !!theApp.GetConfig("vsync", 0);

	if(mt == 2)
	{
		// pcsx2 sent a switch renderer request

#ifdef _WINDOWS

		renderer = GSUtil::CheckDirect3D11Level() >= D3D_FEATURE_LEVEL_10_0 ? GSRendererType::DX1011_SW : GSRendererType::DX9_SW;

#endif

		mt = 1;
	}
	else
	{
		// normal init

		renderer = static_cast<GSRendererType>(theApp.GetConfig("Renderer", static_cast<int>(GSRendererType::Default)));
	}

	*dsp = NULL;

	int retval = _GSopen(dsp, title, renderer);

	if(retval == 0 && s_gs)
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
#ifdef _WINDOWS

		if(s_gs->m_wnd->IsManaged())
		{
			MSG msg;

			memset(&msg, 0, sizeof(msg));

			while(msg.message != WM_QUIT && PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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
}

EXPORT_C_(uint32) GSmakeSnapshot(char* path)
{
	try
	{
		string s(path);

		if(!s.empty() && s[s.length() - 1] != DIRECTORY_SEPARATOR)
		{
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
		if(gsopen_done)
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
		if(mode == FREEZE_SAVE)
		{
			return s_gs->Freeze(data, false);
		}
		else if(mode == FREEZE_SIZE)
		{
			return s_gs->Freeze(data, true);
		}
		else if(mode == FREEZE_LOAD)
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
		if(!GSUtil::CheckSSE()) return;

#ifdef _WINDOWS
		GSDialog::InitCommonControls();
		if(GSSettingsDlg().DoModal() == IDOK)
		{
			if(s_gs != NULL && s_gs->m_wnd->IsManaged())
			{
				// Legacy apps like gsdxgui expect this...

				GSshutdown();
			}
			// Force a reload of the gs state
			s_renderer = GSRendererType::Undefined;
		}

#else

		if (RunLinuxDialog()) {
			theApp.ReloadConfig();
			// Force a reload of the gs state
			s_renderer = GSRendererType::Undefined;
		}

#endif

	} catch (GSDXRecoverableError)
	{
	}
}

EXPORT_C_(int) GStest()
{
	if(!GSUtil::CheckSSE())
	{
		return -1;
	}

#ifdef _WINDOWS

	s_hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if(!GSUtil::CheckDirectX())
	{
		if(SUCCEEDED(s_hr))
		{
			::CoUninitialize();
		}

		s_hr = E_FAIL;

		return -1;
	}

	if(SUCCEEDED(s_hr))
	{
		::CoUninitialize();
	}

	s_hr = E_FAIL;

#endif

	return 0;
}

EXPORT_C GSabout()
{
}

EXPORT_C GSirqCallback(void (*irq)())
{
	s_irq = irq;

	if(s_gs)
	{
		s_gs->SetIrqCallback(s_irq);
	}
}

void pt(const char* str){
	struct tm *current;
	time_t now;
	
	time(&now);
	current = localtime(&now);

	printf("%02i:%02i:%02i%s", current->tm_hour, current->tm_min, current->tm_sec, str);
}

EXPORT_C_(int) GSsetupRecording(int start, void* data)
{
	if (s_gs == NULL) {
		printf("GSdx: no s_gs for recording\n");
		return 0;
	}
#ifdef __linux__
	if (theApp.GetConfig("capture_enabled", 0)) {
		printf("GSdx: Recording is disabled\n");
		return 0;
	}
#endif

	if(start & 1)
	{
		printf("GSdx: Recording start command\n");
		if (s_gs->BeginCapture()) {
			pt(" - Capture started\n");
		} else {
			pt(" - Capture cancelled\n");
			return 0;
		}
	}
	else
	{
		printf("GSdx: Recording end command\n");
		s_gs->EndCapture();
		pt(" - Capture ended\n");
	}

	return 1;
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
	string s = "GSdx";
	s.append(s_renderer_name).append(s_renderer_type);

	// TODO: this gets called from a different thread concurrently with GSOpen (on linux)
	if (gsopen_done && s_gs != NULL && s_gs->m_GStitleInfoBuffer[0])
	{
		std::lock_guard<std::mutex> lock(s_gs->m_pGSsetTitle_Crit);

		s.append(" | ").append(s_gs->m_GStitleInfoBuffer);

		if(s.size() > length - 1)
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

EXPORT_C GSsetVsync(int enabled)
{
	s_vsync = !!enabled;

	if(s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

EXPORT_C GSsetExclusive(int enabled)
{
	s_exclusive = !!enabled;

	if(s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

EXPORT_C GSsetFrameLimit(int limit)
{
	s_framelimit = !!limit;

	if(s_gs)
	{
		s_gs->SetFrameLimit(s_framelimit);
	}
}

#ifdef _WINDOWS

#include <io.h>
#include <fcntl.h>

class Console
{
	HANDLE m_console;
	string m_title;

public:
	Console::Console(LPCSTR title, bool open)
		: m_console(NULL)
		, m_title(title)
	{
		if(open) Open();
	}

	Console::~Console()
	{
		Close();
	}

	void Console::Open()
	{
		if(m_console == NULL)
		{
			CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

			AllocConsole();

			SetConsoleTitle(m_title.c_str());

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

			*stdout = *_fdopen(_open_osfhandle((long)m_console, _O_TEXT), "w");

			setvbuf(stdout, NULL, _IONBF, 0);
		}
	}

	void Console::Close()
	{
		if(m_console != NULL)
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
		if(end > start) {renderer = static_cast<GSRendererType>(n); lpszCmdLine = end;}
	}

	while(*lpszCmdLine == ' ') lpszCmdLine++;

	::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	if(FILE* fp = fopen(lpszCmdLine, "rb"))
	{
		Console console("GSdx", true);

		GSinit();

		uint8 regs[0x2000];
		GSsetBaseMem(regs);

		s_vsync = !!theApp.GetConfig("vsync", 0);

		HWND hWnd = NULL;

		_GSopen((void**)&hWnd, "", renderer);

		uint32 crc;
		fread(&crc, 4, 1, fp);
		GSsetGameCRC(crc, 0);

		GSFreezeData fd;
		fread(&fd.size, 4, 1, fp);
		fd.data = new uint8[fd.size];
		fread(fd.data, fd.size, 1, fp);
		GSfreeze(FREEZE_LOAD, &fd);
		delete [] fd.data;

		fread(regs, 0x2000, 1, fp);

		long start = ftell(fp);

		GSvsync(1);

		struct Packet {uint8 type, param; uint32 size, addr; vector<uint8> buff;};

		list<Packet*> packets;
		vector<uint8> buff;
		int type;

		while((type = fgetc(fp)) != EOF)
		{
			Packet* p = new Packet();

			p->type = (uint8)type;

			switch(type)
			{
			case 0:
				
				p->param = (uint8)fgetc(fp);

				fread(&p->size, 4, 1, fp);

				switch(p->param)
				{
				case 0:
					p->buff.resize(0x4000);
					p->addr = 0x4000 - p->size;
					fread(&p->buff[p->addr], p->size, 1, fp);
					break;
				case 1:
				case 2:
				case 3:
					p->buff.resize(p->size);
					fread(&p->buff[0], p->size, 1, fp);
					break;
				}

				break;

			case 1:

				p->param = (uint8)fgetc(fp);

				break;

			case 2:

				fread(&p->size, 4, 1, fp);

				break;

			case 3:

				p->buff.resize(0x2000);

				fread(&p->buff[0], 0x2000, 1, fp);

				break;
			}

			packets.push_back(p);
		}

		Sleep(100);

		while(IsWindowVisible(hWnd))
		{
			for(list<Packet*>::iterator i = packets.begin(); i != packets.end(); i++)
			{
				Packet* p = *i;

				switch(p->type)
				{
				case 0:

					switch(p->param)
					{
					case 0: GSgifTransfer1(&p->buff[0], p->addr); break;
					case 1: GSgifTransfer2(&p->buff[0], p->size / 16); break;
					case 2: GSgifTransfer3(&p->buff[0], p->size / 16); break;
					case 3: GSgifTransfer(&p->buff[0], p->size / 16); break;
					}

					break;

				case 1:

					GSvsync(p->param);

					break;

				case 2:

					if(buff.size() < p->size) buff.resize(p->size);

					GSreadFIFO2(&buff[0], p->size / 16);

					break;

				case 3:

					memcpy(regs, &p->buff[0], 0x2000);

					break;
				}
			}
		}

		for(list<Packet*>::iterator i = packets.begin(); i != packets.end(); i++)
		{
			delete *i;
		}

		packets.clear();

		Sleep(100);


		/*
		vector<uint8> buff;
		bool exit = false;

		int round = 0;

		while(!exit)
		{
			uint32 index;
			uint32 size;
			uint32 addr;

			int pos;

			switch(fgetc(fp))
			{
			case EOF:
				fseek(fp, start, 0);
				exit = !IsWindowVisible(hWnd);
				//exit = ++round == 60;
				break;

			case 0:
				index = fgetc(fp);
				fread(&size, 4, 1, fp);

				switch(index)
				{
				case 0:
					if(buff.size() < 0x4000) buff.resize(0x4000);
					addr = 0x4000 - size;
					fread(&buff[addr], size, 1, fp);
					GSgifTransfer1(&buff[0], addr);
					break;

				case 1:
					if(buff.size() < size) buff.resize(size);
					fread(&buff[0], size, 1, fp);
					GSgifTransfer2(&buff[0], size / 16);
					break;

				case 2:
					if(buff.size() < size) buff.resize(size);
					fread(&buff[0], size, 1, fp);
					GSgifTransfer3(&buff[0], size / 16);
					break;

				case 3:
					if(buff.size() < size) buff.resize(size);
					fread(&buff[0], size, 1, fp);
					GSgifTransfer(&buff[0], size / 16);
					break;
				}

				break;

			case 1:
				GSvsync(fgetc(fp));
				exit = !IsWindowVisible(hWnd);
				break;

			case 2:
				fread(&size, 4, 1, fp);
				if(buff.size() < size) buff.resize(size);
				GSreadFIFO2(&buff[0], size / 16);
				break;

			case 3:
				fread(regs, 0x2000, 1, fp);
				break;
			}
		}
		*/

		GSclose();
		GSshutdown();

		fclose(fp);
	}
}

EXPORT_C GSBenchmark(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
	::SetPriorityClass(::GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	Console console("GSdx", true);

	if(1)
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

		for(int i = 0; i < 1024 * 1024 * 4; i++) ptr[i] = (uint8)i;

		//

		for(int tbw = 5; tbw <= 10; tbw++)
		{
			int n = 256 << ((10 - tbw) * 2);

			int w = 1 << tbw;
			int h = 1 << tbw;

			printf("%d x %d\n\n", w, h);

			for(size_t i = 0; i < countof(s_format); i++)
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

				for(int j = 0; j < n; j++)
				{
					int x = 0;
					int y = 0;

					(mem->*wi)(x, y, ptr, trlen, BITBLTBUF, TRXPOS, TRXREG);
				}

				end = clock();

				printf("%6d %6d | ", (int)((float)trlen * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				start = clock();

				for(int j = 0; j < n; j++)
				{
					int x = 0;
					int y = 0;

					(mem->*ri)(x, y, ptr, trlen, BITBLTBUF, TRXPOS, TRXREG);
				}

				end = clock();

				printf("%6d %6d | ", (int)((float)trlen * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				const GSOffset* off = mem->GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

				start = clock();

				for(int j = 0; j < n; j++)
				{
					(mem->*rtx)(off, r, ptr, w * 4, TEXA);
				}

				end = clock();

				printf("%6d %6d ", (int)((float)len * n / (end - start) / 1000), (int)((float)(w * h) * n / (end - start) / 1000));

				if(psm.pal > 0)
				{
					start = clock();

					for(int j = 0; j < n; j++)
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

	if(0)
	{
		GSLocalMemory* mem = new GSLocalMemory();

		uint8* ptr = (uint8*)_aligned_malloc(1024 * 1024 * 4, 32);

		for(int i = 0; i < 1024 * 1024 * 4; i++) ptr[i] = (uint8)i;

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

#ifdef __linux__

#include <sys/time.h>
#include <sys/timeb.h>	// ftime(), struct timeb
#include "GSLzma.h"

inline unsigned long timeGetTime()
{
	timeb t;
	ftime(&t);

	return (unsigned long)(t.time*1000 + t.millitm);
}

// Note
EXPORT_C GSReplay(char* lpszCmdLine, int renderer)
{
	GLLoader::in_replayer = true;

	GSRendererType m_renderer;
	// Allow to easyly switch between SW/HW renderer -> this effectively removes the ability to select the renderer by function args
	m_renderer = static_cast<GSRendererType>(theApp.GetConfig("Renderer", static_cast<int>(GSRendererType::Default)));
	// alternatively:
	// m_renderer = static_cast<GSRendererType>(renderer);

	if (m_renderer != GSRendererType::OGL_HW && m_renderer != GSRendererType::OGL_SW)
	{
		fprintf(stderr, "wrong renderer selected %d\n", static_cast<int>(m_renderer));
		return;
	}

	struct Packet {uint8 type, param; uint32 size, addr; vector<uint8> buff;};

	list<Packet*> packets;
	vector<uint8> buff;
	vector<float> stats;
	stats.clear();
	uint8 regs[0x2000];

	GSinit();

	GSsetBaseMem(regs);

	s_vsync = !!theApp.GetConfig("vsync", 0);

	void* hWnd = NULL;

	int err = _GSopen((void**)&hWnd, "", m_renderer);
	if (err != 0) {
		fprintf(stderr, "Error failed to GSopen\n");
		return;
	}
	if (s_gs->m_wnd == NULL) return;

	{ // Read .gs content
		std::string f(lpszCmdLine);
#ifdef LZMA_SUPPORTED
		GSDumpFile* file = (f.size() >= 4) && (f.compare(f.size()-3, 3, ".xz") == 0)
			? (GSDumpFile*) new GSDumpLzma(lpszCmdLine)
			: (GSDumpFile*) new GSDumpRaw(lpszCmdLine);
#else
		GSDumpFile* file = new GSDumpRaw(lpszCmdLine);
#endif

		uint32 crc;
		file->Read(&crc, 4);
		GSsetGameCRC(crc, 0);

		GSFreezeData fd;
		file->Read(&fd.size, 4);
		fd.data = new uint8[fd.size];
		file->Read(fd.data, fd.size);

		GSfreeze(FREEZE_LOAD, &fd);
		delete [] fd.data;

		file->Read(regs, 0x2000);

		GSvsync(1);


		while(!file->IsEof())
		{
			uint8 type;
			file->Read(&type, 1);

			Packet* p = new Packet();

			p->type = type;

			switch(type)
			{
			case 0:
				file->Read(&p->param, 1);
				file->Read(&p->size, 4);

				switch(p->param)
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
		}

		delete file;
	}

	sleep(1);

	//while(IsWindowVisible(hWnd))
	//FIXME map?
	int finished = theApp.GetConfig("linux_replay", 1);
	if (theApp.GetConfig("dump", 0)) {
		fprintf(stderr, "Dump is enabled. Replay will be disabled\n");
		finished = 1;
	}
	unsigned long frame_number = 0;
	unsigned long total_frame_nb = 0;
	while(finished > 0)
	{
		frame_number = 0;
		unsigned long start = timeGetTime();
		for(auto i = packets.begin(); i != packets.end(); i++)
		{
			Packet* p = *i;

			switch(p->type)
			{
				case 0:

					switch(p->param)
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

					if(buff.size() < p->size) buff.resize(p->size);

					GSreadFIFO2(&buff[0], p->size / 16);

					break;

				case 3:

					memcpy(regs, &p->buff[0], 0x2000);

					break;
			}
		}

		// Ensure the rendering is complete to measure correctly the time.
		glFinish();

		if (finished > 90) {
			sleep(1);
		} else {
			unsigned long end = timeGetTime();
			frame_number = std::max(1ul, frame_number); // avoid a potential division by 0

			fprintf(stderr, "The %ld frames of the scene was render on %ldms\n", frame_number, end - start);
			fprintf(stderr, "A means of %fms by frame\n", (float)(end - start)/(float)frame_number);

			stats.push_back((float)(end - start));

			finished--;
			total_frame_nb += frame_number;
		}
	}

	if (theApp.GetConfig("linux_replay", 1) > 1) {
		// Print some nice stats
		// Skip first frame (shader compilation populate the result)
		// it divides by 10 the standard deviation...
		float n = (float)theApp.GetConfig("linux_replay", 1) - 1.0f;
		float mean = 0;
		float sd = 0;
		for (auto i = stats.begin()+1; i != stats.end(); i++) {
			mean += *i;
		}
		mean = mean/n;
		for (auto i = stats.begin()+1; i != stats.end(); i++) {
			sd += pow((*i)-mean, 2);
		}
		sd = sqrt(sd/n);

		fprintf(stderr, "\n\nMean: %fms\n", mean);
		fprintf(stderr, "Standard deviation: %fms\n", sd);
		fprintf(stderr, "Mean by frame: %fms (%ffps)\n", mean/(float)frame_number, 1000.0f*frame_number/mean);
		fprintf(stderr, "Standard deviatin by frame: %fms\n", sd/(float)frame_number);
	}
#ifdef ENABLE_OGL_DEBUG_MEM_BW
	total_frame_nb *= 1024;
	fprintf(stderr, "memory bandwith. T: %f KB/f. V: %f KB/f. U: %f KB/f\n",
			(float)g_real_texture_upload_byte/(float)total_frame_nb,
			(float)g_vertex_upload_byte/(float)total_frame_nb,
			(float)g_uniform_upload_byte/(float)total_frame_nb
		   );
#endif

	for(auto i = packets.begin(); i != packets.end(); i++)
	{
		delete *i;
	}

	packets.clear();

	sleep(1);

	GSclose();
	GSshutdown();
}
#endif

