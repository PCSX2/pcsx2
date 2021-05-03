/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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
#include "GS/Window/GSwxDialog.h"
#include "GS.h"
#include "GSUtil.h"
#include "GSExtra.h"
#include "Renderers/SW/GSRendererSW.h"
#include "Renderers/Null/GSRendererNull.h"
#include "Renderers/Null/GSDeviceNull.h"
#include "Renderers/OpenGL/GSDeviceOGL.h"
#include "Renderers/OpenGL/GSRendererOGL.h"
#include "GSLzma.h"

#include "common/pxStreams.h"
#include "pcsx2/Config.h"

#ifdef _WIN32

#include "Renderers/DX11/GSRendererDX11.h"
#include "Renderers/DX11/GSDevice11.h"
#include "GS/Renderers/DX11/D3D.h"


static HRESULT s_hr = E_FAIL;

#endif

#include <fstream>

// do NOT undefine this/put it above includes, as x11 people love to redefine
// things that make obscure compiler bugs, unless you want to run around and
// debug obscure compiler errors --govanify
#undef None

static GSRenderer* s_gs = NULL;
static u8* s_basemem = NULL;
static int s_vsync = 0;
static bool s_exclusive = true;
static std::string s_renderer_name;
bool gsopen_done = false; // crash guard for GSgetTitleInfo2 and GSKeyEvent (replace with lock?)

#ifndef PCSX2_CORE
static std::atomic_bool s_gs_window_resized{false};
static std::mutex s_gs_window_resized_lock;
static int s_new_gs_window_width = 0;
static int s_new_gs_window_height = 0;
#endif

void GSsetBaseMem(u8* mem)
{
	s_basemem = mem;

	if (s_gs)
	{
		s_gs->SetRegsMem(s_basemem);
	}
}

int GSinit()
{
	if (!GSUtil::CheckSSE())
	{
		return -1;
	}

	// Vector instructions must be avoided when initialising GS since PCSX2
	// can crash if the CPU does not support the instruction set.
	// Initialise it here instead - it's not ideal since we have to strip the
	// const type qualifier from all the affected variables.
	theApp.SetConfigDir();
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

void GSshutdown()
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

void GSclose()
{
	gsopen_done = false;

#ifndef PCSX2_CORE
	// Make sure we don't have any leftover resize events from our last open.
	s_gs_window_resized.store(false);
#endif

	if (s_gs == NULL)
		return;

	s_gs->ResetDevice();

	// Opengl requirement: It must be done before the Detach() of
	// the context
	delete s_gs->m_dev;

	s_gs->m_dev = NULL;
}

int _GSopen(const WindowInfo& wi, const char* title, GSRendererType renderer, int threads = -1)
{
	GSDevice* dev = NULL;

	// Fresh start up or config file changed
	if (renderer == GSRendererType::Undefined)
	{
		renderer = static_cast<GSRendererType>(theApp.GetConfigI("Renderer"));
#ifdef _WIN32
		if (renderer == GSRendererType::Default)
		{
			if (D3D::ShouldPreferD3D())
				renderer = GSRendererType::DX1011_HW;
			else
				renderer = GSRendererType::OGL_HW;
		}
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
	}
	catch (std::exception& ex)
	{
		printf("GS error: Exception caught in GSopen: %s", ex.what());
		return -1;
	}

	s_gs->SetRegsMem(s_basemem);
	s_gs->SetVSync(s_vsync);

	if (!s_gs->CreateDevice(dev, wi))
	{
		// This probably means the user has DX11 configured with a video card that is only DX9
		// compliant.  Cound mean drivr issues of some sort also, but to be sure, that's the most
		// common cause of device creation errors. :)  --air

		GSclose();

		return -1;
	}

	if (renderer == GSRendererType::OGL_HW && theApp.GetConfigI("debug_glsl_shader") == 2)
	{
		printf("GS: test OpenGL shader. Please wait...\n\n");
		static_cast<GSDeviceOGL*>(s_gs->m_dev)->SelfShaderTest();
		printf("\nGS: test OpenGL shader done. It will now exit\n");
		return -1;
	}

	return 0;
}

void GSosdLog(const char* utf8, u32 color)
{
	if (s_gs && s_gs->m_dev)
		s_gs->m_dev->m_osd.Log(utf8);
}

void GSosdMonitor(const char* key, const char* value, u32 color)
{
	if (s_gs && s_gs->m_dev)
		s_gs->m_dev->m_osd.Monitor(key, value);
}

int GSopen2(const WindowInfo& wi, u32 flags)
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
				{
					if (D3D::ShouldPreferD3D())
						current_renderer = GSRendererType::DX1011_HW;
					else
						current_renderer = GSRendererType::OGL_HW;
				}
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

	int retval = _GSopen(wi, "", current_renderer);

	gsopen_done = true;

	return retval;
}

void GSreset()
{
	try
	{
		s_gs->Reset();
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifSoftReset(u32 mask)
{
	try
	{
		s_gs->SoftReset(mask);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSwriteCSR(u32 csr)
{
	try
	{
		s_gs->WriteCSR(csr);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSinitReadFIFO(u8* mem)
{
	GL_PERF("Init Read FIFO1");
	try
	{
		s_gs->InitReadFIFO(mem, 1);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

void GSreadFIFO(u8* mem)
{
	try
	{
		s_gs->ReadFIFO(mem, 1);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

void GSinitReadFIFO2(u8* mem, u32 size)
{
	GL_PERF("Init Read FIFO2");
	try
	{
		s_gs->InitReadFIFO(mem, size);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

void GSreadFIFO2(u8* mem, u32 size)
{
	try
	{
		s_gs->ReadFIFO(mem, size);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

void GSgifTransfer(const u8* mem, u32 size)
{
	try
	{
		s_gs->Transfer<3>(mem, size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer1(u8* mem, u32 addr)
{
	try
	{
		s_gs->Transfer<0>(const_cast<u8*>(mem) + addr, (0x4000 - addr) / 16);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer2(u8* mem, u32 size)
{
	try
	{
		s_gs->Transfer<1>(const_cast<u8*>(mem), size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSgifTransfer3(u8* mem, u32 size)
{
	try
	{
		s_gs->Transfer<2>(const_cast<u8*>(mem), size);
	}
	catch (GSRecoverableError)
	{
	}
}

void GSvsync(int field)
{
	try
	{
		s_gs->VSync(field);
	}
	catch (GSRecoverableError)
	{
	}
	catch (const std::bad_alloc&)
	{
		fprintf(stderr, "GS: Memory allocation error\n");
	}
}

u32 GSmakeSnapshot(char* path)
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

		return s_gs->MakeSnapshot(s + "gs");
	}
	catch (GSRecoverableError)
	{
		return false;
	}
}

void GSkeyEvent(const HostKeyEvent& e)
{
	try
	{
		if (gsopen_done)
		{
			s_gs->KeyEvent(e);
		}
	}
	catch (GSRecoverableError)
	{
	}
}

int GSfreeze(FreezeAction mode, freezeData* data)
{
	try
	{
		if (mode == FreezeAction::Save)
		{
			return s_gs->Freeze(data, false);
		}
		else if (mode == FreezeAction::Size)
		{
			return s_gs->Freeze(data, true);
		}
		else if (mode == FreezeAction::Load)
		{
			return s_gs->Defrost(data);
		}
	}
	catch (GSRecoverableError)
	{
	}

	return 0;
}

void GSconfigure()
{
	try
	{
		if (!GSUtil::CheckSSE())
			return;

		theApp.SetConfigDir();
		theApp.Init();

		if (RunwxDialog())
		{
			theApp.ReloadConfig();
			// Force a reload of the gs state
			theApp.SetCurrentRendererType(GSRendererType::Undefined);
		}
	}
	catch (GSRecoverableError)
	{
	}
}

int GStest()
{
	if (!GSUtil::CheckSSE())
		return -1;

	return 0;
}

void pt(const char* str)
{
	struct tm* current;
	time_t now;

	time(&now);
	current = localtime(&now);

	printf("%02i:%02i:%02i%s", current->tm_hour, current->tm_min, current->tm_sec, str);
}

bool GSsetupRecording(std::string& filename)
{
	if (s_gs == NULL)
	{
		printf("GS: no s_gs for recording\n");
		return false;
	}
#if defined(__unix__) || defined(__APPLE__)
	if (!theApp.GetConfigB("capture_enabled"))
	{
		printf("GS: Recording is disabled\n");
		return false;
	}
#endif
	printf("GS: Recording start command\n");
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

void GSendRecording()
{
	printf("GS: Recording end command\n");
	s_gs->EndCapture();
	pt(" - Capture ended\n");
}

void GSsetGameCRC(u32 crc, int options)
{
	s_gs->SetGameCRC(crc, options);
}

void GSgetTitleInfo2(char* dest, size_t length)
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

void GSsetFrameSkip(int frameskip)
{
	s_gs->SetFrameSkip(frameskip);
}

void GSsetVsync(int vsync)
{
	s_vsync = vsync;

	if (s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

void GSsetExclusive(int enabled)
{
	s_exclusive = !!enabled;

	if (s_gs)
	{
		s_gs->SetVSync(s_vsync);
	}
}

#ifndef PCSX2_CORE
void GSResizeWindow(int width, int height)
{
	std::unique_lock lock(s_gs_window_resized_lock);
	s_new_gs_window_width = width;
	s_new_gs_window_height = height;
	s_gs_window_resized.store(true);
}

bool GSCheckForWindowResize(int* new_width, int* new_height)
{
	if (!s_gs_window_resized.load())
		return false;

	std::unique_lock lock(s_gs_window_resized_lock);
	*new_width = s_new_gs_window_width;
	*new_height = s_new_gs_window_height;
	s_gs_window_resized.store(false);
	return true;
}
#endif

std::string format(const char* fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	int size = vsnprintf(nullptr, 0, fmt, args) + 1;
	va_end(args);

	assert(size > 0);
	std::vector<char> buffer(std::max(1, size));

	va_start(args, fmt);
	vsnprintf(buffer.data(), size, fmt, args);
	va_end(args);

	return {buffer.data()};
}

// Helper path to dump texture
#ifdef _WIN32
const std::string root_sw("c:\\temp1\\_");
const std::string root_hw("c:\\temp2\\_");
#else
#ifdef _M_AMD64
const std::string root_sw("/tmp/GS_SW_dump64/");
const std::string root_hw("/tmp/GS_HW_dump64/");
#else
const std::string root_sw("/tmp/GS_SW_dump32/");
const std::string root_hw("/tmp/GS_HW_dump32/");
#endif
#endif

#ifdef _WIN32

void* vmalloc(size_t size, bool code)
{
	void* ptr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, code ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}

void vmfree(void* ptr, size_t size)
{
	VirtualFree(ptr, 0, MEM_RELEASE);
}

static HANDLE s_fh = NULL;
static u8* s_Next[8];

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_fh == NULL);

	if (repeat >= std::size(s_Next))
	{
		fprintf(stderr, "Memory mapping overflow (%zu >= %u)\n", repeat, static_cast<unsigned>(std::size(s_Next)));
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	s_fh = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, size, nullptr);
	DWORD errorID = ::GetLastError();
	if (s_fh == NULL)
	{
		fprintf(stderr, "Failed to reserve memory. WIN API ERROR:%u\n", errorID);
		return vmalloc(size * repeat, false); // Fallback to default vmalloc
	}

	int mmap_segment_failed = 0;
	void* fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
	for (size_t i = 1; i < repeat; i++)
	{
		void* base = (u8*)fifo + size * i;
		s_Next[i] = (u8*)MapViewOfFileEx(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size, base);
		errorID = ::GetLastError();
		if (s_Next[i] != base)
		{
			mmap_segment_failed++;
			if (mmap_segment_failed > 4)
			{
				fprintf(stderr, "Memory mapping failed after %d attempts, aborting. WIN API ERROR:%u\n", mmap_segment_failed, errorID);
				fifo_free(fifo, size, repeat);
				return vmalloc(size * repeat, false); // Fallback to default vmalloc
			}
			do
			{
				UnmapViewOfFile(s_Next[i]);
				s_Next[i] = 0;
			} while (--i > 0);

			fifo = MapViewOfFile(s_fh, FILE_MAP_ALL_ACCESS, 0, 0, size);
		}
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_fh != NULL);

	if (s_fh == NULL)
	{
		if (ptr != NULL)
			vmfree(ptr, size);
		return;
	}

	UnmapViewOfFile(ptr);

	for (size_t i = 1; i < std::size(s_Next); i++)
	{
		if (s_Next[i] != 0)
		{
			UnmapViewOfFile(s_Next[i]);
			s_Next[i] = 0;
		}
	}

	CloseHandle(s_fh);
	s_fh = NULL;
}

#else

#include <sys/mman.h>
#include <unistd.h>

void* vmalloc(size_t size, bool code)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	int prot = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANONYMOUS;

	if (code)
	{
		prot |= PROT_EXEC;
#if defined(_M_AMD64) && !defined(__APPLE__)
		// macOS doesn't allow any mappings in the first 4GB of address space
		flags |= MAP_32BIT;
#endif
	}

	void* ptr = mmap(NULL, size, prot, flags, -1, 0);
	if (ptr == MAP_FAILED)
		throw std::bad_alloc();
	return ptr;
}

void vmfree(void* ptr, size_t size)
{
	size_t mask = getpagesize() - 1;

	size = (size + mask) & ~mask;

	munmap(ptr, size);
}

static int s_shm_fd = -1;

void* fifo_alloc(size_t size, size_t repeat)
{
	ASSERT(s_shm_fd == -1);

	const char* file_name = "/GS.mem";
	s_shm_fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (s_shm_fd != -1)
	{
		shm_unlink(file_name); // file is deleted but descriptor is still open
	}
	else
	{
		fprintf(stderr, "Failed to open %s due to %s\n", file_name, strerror(errno));
		return nullptr;
	}

	if (ftruncate(s_shm_fd, repeat * size) < 0)
		fprintf(stderr, "Failed to reserve memory due to %s\n", strerror(errno));

	void* fifo = mmap(nullptr, size * repeat, PROT_READ | PROT_WRITE, MAP_SHARED, s_shm_fd, 0);

	for (size_t i = 1; i < repeat; i++)
	{
		void* base = (u8*)fifo + size * i;
		u8* next = (u8*)mmap(base, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, s_shm_fd, 0);
		if (next != base)
			fprintf(stderr, "Fail to mmap contiguous segment\n");
	}

	return fifo;
}

void fifo_free(void* ptr, size_t size, size_t repeat)
{
	ASSERT(s_shm_fd >= 0);

	if (s_shm_fd < 0)
		return;

	munmap(ptr, size * repeat);

	close(s_shm_fd);
	s_shm_fd = -1;
}

#endif

static void* s_hModule;

#ifdef _WIN32

bool GSApp::LoadResource(int id, std::vector<char>& buff, const wchar_t* type)
{
	buff.clear();
	HRSRC hRsrc = FindResource((HMODULE)s_hModule, MAKEINTRESOURCE(id), type != NULL ? type : (LPWSTR)RT_RCDATA);
	if (!hRsrc)
		return false;
	HGLOBAL hGlobal = ::LoadResource((HMODULE)s_hModule, hRsrc);
	if (!hGlobal)
		return false;
	DWORD size = SizeofResource((HMODULE)s_hModule, hRsrc);
	if (!size)
		return false;
	// On Linux resources are always NULL terminated
	// Add + 1 on size to do the same for compatibility sake (required by GSDeviceOGL)
	buff.resize(size + 1);
	memcpy(buff.data(), LockResource(hGlobal), size);
	return true;
}

#else

#include "GS_res.h"

bool GSApp::LoadResource(int id, std::vector<char>& buff, const char* type)
{
	std::string path;
	switch (id)
	{
		case IDR_COMMON_GLSL:
			path = "/GS/res/glsl/common_header.glsl";
			break;
		case IDR_CONVERT_GLSL:
			path = "/GS/res/glsl/convert.glsl";
			break;
		case IDR_FXAA_FX:
			path = "/GS/res/fxaa.fx";
			break;
		case IDR_INTERLACE_GLSL:
			path = "/GS/res/glsl/interlace.glsl";
			break;
		case IDR_MERGE_GLSL:
			path = "/GS/res/glsl/merge.glsl";
			break;
		case IDR_SHADEBOOST_GLSL:
			path = "/GS/res/glsl/shadeboost.glsl";
			break;
		case IDR_TFX_VGS_GLSL:
			path = "/GS/res/glsl/tfx_vgs.glsl";
			break;
		case IDR_TFX_FS_GLSL:
			path = "/GS/res/glsl/tfx_fs.glsl";
			break;
		case IDR_FONT_ROBOTO:
			path = "/GS/res/fonts-roboto/Roboto-Regular.ttf";
			break;
		default:
			printf("LoadResource not implemented for id %d\n", id);
			return false;
	}

	GBytes* bytes = g_resource_lookup_data(GS_res_get_resource(), path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);

	size_t size = 0;
	const void* data = g_bytes_get_data(bytes, &size);

	if (data == nullptr || size == 0)
	{
		printf("Failed to get data for resource: %d\n", id);
		return false;
	}

	buff.clear();
	buff.resize(size + 1);
	memcpy(buff.data(), data, size + 1);

	g_bytes_unref(bytes);

	return true;
}
#endif

size_t GSApp::GetIniString(const char* lpAppName, const char* lpKeyName, const char* lpDefault, char* lpReturnedString, size_t nSize, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value = m_configuration_map[key];
	if (value.empty())
	{
		// save the value for futur call
		m_configuration_map[key] = std::string(lpDefault);
		strcpy(lpReturnedString, lpDefault);
	}
	else
		strcpy(lpReturnedString, value.c_str());

	return 0;
}

bool GSApp::WriteIniString(const char* lpAppName, const char* lpKeyName, const char* pString, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string key(lpKeyName);
	std::string value(pString);
	m_configuration_map[key] = value;

	// Save config to a file
	FILE* f = px_fopen(lpFileName, "w");

	if (f == NULL)
		return false; // FIXME print a nice message

		// Maintain compatibility with GSDumpGUI/old Windows ini.
#ifdef _WIN32
	fprintf(f, "[Settings]\n");
#endif

	for (const auto& entry : m_configuration_map)
	{
		// Do not save the inifile key which is not an option
		if (entry.first.compare("inifile") == 0)
			continue;

		// Only keep option that have a default value (allow to purge old option of the GS.ini)
		if (!entry.second.empty() && m_default_configuration.find(entry.first) != m_default_configuration.end())
			fprintf(f, "%s = %s\n", entry.first.c_str(), entry.second.c_str());
	}
	fclose(f);

	return false;
}

int GSApp::GetIniInt(const char* lpAppName, const char* lpKeyName, int nDefault, const char* lpFileName)
{
	BuildConfigurationMap(lpFileName);

	std::string value = m_configuration_map[std::string(lpKeyName)];
	if (value.empty())
	{
		// save the value for futur call
		SetConfig(lpKeyName, nDefault);
		return nDefault;
	}
	else
		return atoi(value.c_str());
}

GSApp theApp;

GSApp::GSApp()
{
	// Empty constructor causes an illegal instruction exception on an SSE4.2 machine on Windows.
	// Non-empty doesn't, but raises a SIGILL signal when compiled against GCC 6.1.1.
	// So here's a compromise.
#ifdef _WIN32
	Init();
#endif
}

void GSApp::Init()
{
	static bool is_initialised = false;
	if (is_initialised)
		return;
	is_initialised = true;

	m_current_renderer_type = GSRendererType::Undefined;

	m_section = "Settings";

#ifdef _WIN32
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::DX1011_HW), "Direct3D 11", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::OGL_HW), "OpenGL", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::OGL_SW), "Software", ""));
#else // Linux
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::OGL_HW), "OpenGL", ""));
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::OGL_SW), "Software", ""));
#endif

	// The null renderer goes third, it has use for benchmarking purposes in a release build
	m_gs_renderers.push_back(GSSetting(static_cast<u32>(GSRendererType::Null), "Null", ""));

	m_gs_interlace.push_back(GSSetting(0, "None", ""));
	m_gs_interlace.push_back(GSSetting(1, "Weave tff", "saw-tooth"));
	m_gs_interlace.push_back(GSSetting(2, "Weave bff", "saw-tooth"));
	m_gs_interlace.push_back(GSSetting(3, "Bob tff", "use blend if shaking"));
	m_gs_interlace.push_back(GSSetting(4, "Bob bff", "use blend if shaking"));
	m_gs_interlace.push_back(GSSetting(5, "Blend tff", "slight blur, 1/2 fps"));
	m_gs_interlace.push_back(GSSetting(6, "Blend bff", "slight blur, 1/2 fps"));
	m_gs_interlace.push_back(GSSetting(7, "Automatic", "Default"));

	m_gs_upscale_multiplier.push_back(GSSetting(1, "Native", "PS2"));
	m_gs_upscale_multiplier.push_back(GSSetting(2, "2x Native", "~720p"));
	m_gs_upscale_multiplier.push_back(GSSetting(3, "3x Native", "~1080p"));
	m_gs_upscale_multiplier.push_back(GSSetting(4, "4x Native", "~1440p 2K"));
	m_gs_upscale_multiplier.push_back(GSSetting(5, "5x Native", "~1620p"));
	m_gs_upscale_multiplier.push_back(GSSetting(6, "6x Native", "~2160p 4K"));
	m_gs_upscale_multiplier.push_back(GSSetting(7, "7x Native", "~2520p"));
	m_gs_upscale_multiplier.push_back(GSSetting(8, "8x Native", "~2880p"));

	m_gs_max_anisotropy.push_back(GSSetting(0, "Off", "Default"));
	m_gs_max_anisotropy.push_back(GSSetting(2, "2x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(4, "4x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(8, "8x", ""));
	m_gs_max_anisotropy.push_back(GSSetting(16, "16x", ""));

	m_gs_dithering.push_back(GSSetting(0, "Off", ""));
	m_gs_dithering.push_back(GSSetting(2, "Unscaled", "Default"));
	m_gs_dithering.push_back(GSSetting(1, "Scaled", ""));

	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Nearest), "Nearest", ""));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Forced_But_Sprite), "Bilinear", "Forced excluding sprite"));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::Forced), "Bilinear", "Forced"));
	m_gs_bifilter.push_back(GSSetting(static_cast<u32>(BiFiltering::PS2), "Bilinear", "PS2"));

	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::None), "None", "Default"));
	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::PS2), "Trilinear", ""));
	m_gs_trifilter.push_back(GSSetting(static_cast<u32>(TriFiltering::Forced), "Trilinear", "Ultra/Slow"));

	m_gs_generic_list.push_back(GSSetting(-1, "Automatic", "Default"));
	m_gs_generic_list.push_back(GSSetting(0, "Force-Disabled", ""));
	m_gs_generic_list.push_back(GSSetting(1, "Force-Enabled", ""));

	m_gs_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_hack.push_back(GSSetting(1, "Half", ""));
	m_gs_hack.push_back(GSSetting(2, "Full", ""));

	m_gs_offset_hack.push_back(GSSetting(0, "Off", "Default"));
	m_gs_offset_hack.push_back(GSSetting(1, "Normal", "Vertex"));
	m_gs_offset_hack.push_back(GSSetting(2, "Special", "Texture"));
	m_gs_offset_hack.push_back(GSSetting(3, "Special", "Texture - aggressive"));

	m_gs_hw_mipmapping = {
		GSSetting(HWMipmapLevel::Automatic, "Automatic", "Default"),
		GSSetting(HWMipmapLevel::Off, "Off", ""),
		GSSetting(HWMipmapLevel::Basic, "Basic", "Fast"),
		GSSetting(HWMipmapLevel::Full, "Full", "Slow"),
	};

	m_gs_crc_level = {
		GSSetting(CRCHackLevel::Automatic, "Automatic", "Default"),
		GSSetting(CRCHackLevel::None, "None", "Debug"),
		GSSetting(CRCHackLevel::Minimum, "Minimum", "Debug"),
#ifdef _DEBUG
		GSSetting(CRCHackLevel::Partial, "Partial", "OpenGL"),
		GSSetting(CRCHackLevel::Full, "Full", "Direct3D"),
#endif
		GSSetting(CRCHackLevel::Aggressive, "Aggressive", ""),
	};

	m_gs_acc_blend_level.push_back(GSSetting(0, "Minimum", "Fastest"));
	m_gs_acc_blend_level.push_back(GSSetting(1, "Basic", "Recommended"));
	m_gs_acc_blend_level.push_back(GSSetting(2, "Medium", ""));
	m_gs_acc_blend_level.push_back(GSSetting(3, "High", ""));
	m_gs_acc_blend_level.push_back(GSSetting(4, "Full", "Very Slow"));
	m_gs_acc_blend_level.push_back(GSSetting(5, "Ultra", "Ultra Slow"));

	m_gs_acc_blend_level_d3d11.push_back(GSSetting(0, "Minimum", "Fastest"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(1, "Basic", "Recommended"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(2, "Medium", "Debug"));
	m_gs_acc_blend_level_d3d11.push_back(GSSetting(3, "High", "Debug"));

	m_gs_tv_shaders.push_back(GSSetting(0, "None", ""));
	m_gs_tv_shaders.push_back(GSSetting(1, "Scanline filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(2, "Diagonal filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(3, "Triangular filter", ""));
	m_gs_tv_shaders.push_back(GSSetting(4, "Wave filter", ""));

	// clang-format off
	// Avoid to clutter the ini file with useless options
#ifdef _WIN32
	// Per OS option.
	m_default_configuration["adapter_index"]                              = "0";
	m_default_configuration["CaptureFileName"]                            = "";
	m_default_configuration["CaptureVideoCodecDisplayName"]               = "";
	m_default_configuration["debug_d3d"]                                  = "0";
	m_default_configuration["dx_break_on_severity"]                       = "0";
	// D3D Blending option
	m_default_configuration["accurate_blending_unit_d3d11"]               = "1";
#else
	m_default_configuration["linux_replay"]                               = "1";
#endif
	m_default_configuration["aa1"]                                        = "1";
	m_default_configuration["accurate_date"]                              = "1";
	m_default_configuration["accurate_blending_unit"]                     = "1";
	m_default_configuration["AspectRatio"]                                = "1";
	m_default_configuration["autoflush_sw"]                               = "1";
	m_default_configuration["capture_enabled"]                            = "0";
	m_default_configuration["capture_out_dir"]                            = "/tmp/GS_Capture";
	m_default_configuration["capture_threads"]                            = "4";
	m_default_configuration["CaptureHeight"]                              = "480";
	m_default_configuration["CaptureWidth"]                               = "640";
	m_default_configuration["crc_hack_level"]                             = std::to_string(static_cast<s8>(CRCHackLevel::Automatic));
	m_default_configuration["CrcHacksExclusions"]                         = "";
	m_default_configuration["debug_glsl_shader"]                          = "0";
	m_default_configuration["debug_opengl"]                               = "0";
	m_default_configuration["disable_hw_gl_draw"]                         = "0";
	m_default_configuration["dithering_ps2"]                              = "2";
	m_default_configuration["dump"]                                       = "0";
	m_default_configuration["extrathreads"]                               = "2";
	m_default_configuration["extrathreads_height"]                        = "4";
	m_default_configuration["filter"]                                     = std::to_string(static_cast<s8>(BiFiltering::PS2));
	m_default_configuration["force_texture_clear"]                        = "0";
	m_default_configuration["fxaa"]                                       = "0";
	m_default_configuration["interlace"]                                  = "7";
	m_default_configuration["conservative_framebuffer"]                   = "1";
	m_default_configuration["linear_present"]                             = "1";
	m_default_configuration["MaxAnisotropy"]                              = "0";
	m_default_configuration["mipmap"]                                     = "1";
	m_default_configuration["mipmap_hw"]                                  = std::to_string(static_cast<int>(HWMipmapLevel::Automatic));
	m_default_configuration["ModeHeight"]                                 = "480";
	m_default_configuration["ModeWidth"]                                  = "640";
	m_default_configuration["NTSC_Saturation"]                            = "1";
#ifdef _WIN32
	m_default_configuration["osd_fontname"]                               = "C:\\Windows\\Fonts\\my_favorite_font_e_g_tahoma.ttf";
#else
	m_default_configuration["osd_fontname"]                               = "/usr/share/fonts/truetype/my_favorite_font_e_g_DejaVu Sans.ttf";
#endif
	m_default_configuration["osd_color_r"]                                = "0";
	m_default_configuration["osd_color_g"]                                = "160";
	m_default_configuration["osd_color_b"]                                = "255";
	m_default_configuration["osd_color_opacity"]                          = "100";
	m_default_configuration["osd_fontsize"]                               = "25";
	m_default_configuration["osd_log_enabled"]                            = "1";
	m_default_configuration["osd_log_timeout"]                            = "4";
	m_default_configuration["osd_monitor_enabled"]                        = "0";
	m_default_configuration["osd_max_log_messages"]                       = "2";
	m_default_configuration["override_geometry_shader"]                   = "-1";
	m_default_configuration["override_GL_ARB_copy_image"]                 = "-1";
	m_default_configuration["override_GL_ARB_clear_texture"]              = "-1";
	m_default_configuration["override_GL_ARB_clip_control"]               = "-1";
	m_default_configuration["override_GL_ARB_direct_state_access"]        = "-1";
	m_default_configuration["override_GL_ARB_draw_buffers_blend"]         = "-1";
	m_default_configuration["override_GL_ARB_gpu_shader5"]                = "-1";
	m_default_configuration["override_GL_ARB_shader_image_load_store"]    = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture"]             = "-1";
	m_default_configuration["override_GL_ARB_sparse_texture2"]            = "-1";
	m_default_configuration["override_GL_ARB_texture_barrier"]            = "-1";
#ifdef GL_EXT_TEX_SUB_IMAGE
	m_default_configuration["override_GL_ARB_get_texture_sub_image"]      = "-1";
#endif
	m_default_configuration["paltex"]                                     = "0";
	m_default_configuration["png_compression_level"]                      = std::to_string(Z_BEST_SPEED);
	m_default_configuration["preload_frame_with_gs_data"]                 = "0";
	m_default_configuration["Renderer"]                                   = std::to_string(static_cast<int>(GSRendererType::Default));
	m_default_configuration["resx"]                                       = "1024";
	m_default_configuration["resy"]                                       = "1024";
	m_default_configuration["save"]                                       = "0";
	m_default_configuration["savef"]                                      = "0";
	m_default_configuration["savel"]                                      = "5000";
	m_default_configuration["saven"]                                      = "0";
	m_default_configuration["savet"]                                      = "0";
	m_default_configuration["savez"]                                      = "0";
	m_default_configuration["ShadeBoost"]                                 = "0";
	m_default_configuration["ShadeBoost_Brightness"]                      = "50";
	m_default_configuration["ShadeBoost_Contrast"]                        = "50";
	m_default_configuration["ShadeBoost_Saturation"]                      = "50";
	m_default_configuration["shaderfx"]                                   = "0";
	m_default_configuration["shaderfx_conf"]                              = "shaders/GS_FX_Settings.ini";
	m_default_configuration["shaderfx_glsl"]                              = "shaders/GS.fx";
	m_default_configuration["TVShader"]                                   = "0";
	m_default_configuration["upscale_multiplier"]                         = "1";
	m_default_configuration["UserHacks"]                                  = "0";
	m_default_configuration["UserHacks_align_sprite_X"]                   = "0";
	m_default_configuration["UserHacks_AutoFlush"]                        = "0";
	m_default_configuration["UserHacks_DisableDepthSupport"]              = "0";
	m_default_configuration["UserHacks_Disable_Safe_Features"]            = "0";
	m_default_configuration["UserHacks_DisablePartialInvalidation"]       = "0";
	m_default_configuration["UserHacks_CPU_FB_Conversion"]                = "0";
	m_default_configuration["UserHacks_Half_Bottom_Override"]             = "-1";
	m_default_configuration["UserHacks_HalfPixelOffset"]                  = "0";
	m_default_configuration["UserHacks_merge_pp_sprite"]                  = "0";
	m_default_configuration["UserHacks_round_sprite_offset"]              = "0";
	m_default_configuration["UserHacks_SkipDraw"]                         = "0";
	m_default_configuration["UserHacks_SkipDraw_Offset"]                  = "0";
	m_default_configuration["UserHacks_TCOffsetX"]                        = "0";
	m_default_configuration["UserHacks_TCOffsetY"]                        = "0";
	m_default_configuration["UserHacks_TextureInsideRt"]                  = "0";
	m_default_configuration["UserHacks_TriFilter"]                        = std::to_string(static_cast<s8>(TriFiltering::None));
	m_default_configuration["UserHacks_WildHack"]                         = "0";
	m_default_configuration["wrap_gs_mem"]                                = "0";
	m_default_configuration["vsync"]                                      = "0";
	// clang-format on
}

void GSApp::ReloadConfig()
{
	if (m_configuration_map.empty())
		return;

	auto file = m_configuration_map.find("inifile");
	if (file == m_configuration_map.end())
		return;

	// A map was built so reload it
	std::string filename = file->second;
	m_configuration_map.clear();
	BuildConfigurationMap(filename.c_str());
}

void GSApp::BuildConfigurationMap(const char* lpFileName)
{
	// Check if the map was already built
	std::string inifile_value(lpFileName);
	if (inifile_value.compare(m_configuration_map["inifile"]) == 0)
		return;
	m_configuration_map["inifile"] = inifile_value;

	// Load config from file
#ifdef _WIN32
	std::ifstream file(convert_utf8_to_utf16(lpFileName));
#else
	std::ifstream file(lpFileName);
#endif
	if (!file.is_open())
		return;

	std::string line;
	while (std::getline(file, line))
	{
		const auto separator = line.find('=');
		if (separator == std::string::npos)
			continue;

		std::string key = line.substr(0, separator);
		// Trim trailing whitespace
		key.erase(key.find_last_not_of(" \r\t") + 1);

		if (key.empty())
			continue;

		// Only keep options that have a default value so older, no longer used
		// ini options can be purged.
		if (m_default_configuration.find(key) == m_default_configuration.end())
			continue;

		std::string value = line.substr(separator + 1);
		// Trim leading whitespace
		value.erase(0, value.find_first_not_of(" \r\t"));

		m_configuration_map[key] = value;
	}
}

void* GSApp::GetModuleHandlePtr()
{
	return s_hModule;
}

void GSApp::SetConfigDir()
{
	// we need to initialize the ini folder later at runtime than at theApp init, as
	// core settings aren't populated yet, thus we do populate it if needed either when
	// opening GS settings or init -- govanify
	wxString iniName(L"GS.ini");
	m_ini = EmuFolders::Settings.Combine(iniName).GetFullPath().ToUTF8();
}

std::string GSApp::GetConfigS(const char* entry)
{
	char buff[4096] = {0};
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end())
	{
		GetIniString(m_section.c_str(), entry, def->second.c_str(), buff, std::size(buff), m_ini.c_str());
	}
	else
	{
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		GetIniString(m_section.c_str(), entry, "", buff, std::size(buff), m_ini.c_str());
	}

	return {buff};
}

void GSApp::SetConfig(const char* entry, const char* value)
{
	WriteIniString(m_section.c_str(), entry, value, m_ini.c_str());
}

int GSApp::GetConfigI(const char* entry)
{
	auto def = m_default_configuration.find(entry);

	if (def != m_default_configuration.end())
	{
		return GetIniInt(m_section.c_str(), entry, std::stoi(def->second), m_ini.c_str());
	}
	else
	{
		fprintf(stderr, "Option %s doesn't have a default value\n", entry);
		return GetIniInt(m_section.c_str(), entry, 0, m_ini.c_str());
	}
}

bool GSApp::GetConfigB(const char* entry)
{
	return !!GetConfigI(entry);
}

void GSApp::SetConfig(const char* entry, int value)
{
	char buff[32] = {0};

	sprintf(buff, "%d", value);

	SetConfig(entry, buff);
}

void GSApp::SetCurrentRendererType(GSRendererType type)
{
	m_current_renderer_type = type;
}

GSRendererType GSApp::GetCurrentRendererType() const
{
	return m_current_renderer_type;
}
