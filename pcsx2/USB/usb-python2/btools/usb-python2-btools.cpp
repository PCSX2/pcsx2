#include "PrecompiledHeader.h"
#include "USB/USB.h"
#include "usb-python2-btools.h"

#include <wx/fileconf.h>
#include "gui/AppConfig.h"
#include "gui/StringHelpers.h"
#include "USB/shared/inifile_usb.h"

#include <algorithm>
#include <chrono>
#include <bemanitools/ddrio.h>

namespace usb_python2
{
	namespace btools
	{
		struct shim_ctx
		{
			HANDLE barrier;
			int (*proc)(void*);
			void* ctx;
		};

		static unsigned int crt_thread_shim(void* outer_ctx)
		{
			struct shim_ctx* sctx = (struct shim_ctx*)outer_ctx;
			int (*proc)(void*);
			void* inner_ctx;

			proc = sctx->proc;
			inner_ctx = sctx->ctx;

			SetEvent(sctx->barrier);

			return proc(inner_ctx);
		}

		int BToolsInput::crt_thread_create(
			int (*proc)(void*), void* ctx, uint32_t stack_sz, unsigned int priority)
		{

			Console.WriteLn("crt_thread_create");

			struct shim_ctx sctx;
			uintptr_t thread_id;

			sctx.barrier = CreateEvent(NULL, TRUE, FALSE, NULL);
			sctx.proc = proc;
			sctx.ctx = ctx;

			thread_id = _beginthreadex(NULL, stack_sz, (_beginthreadex_proc_type)crt_thread_shim, &sctx, 0, NULL);

			WaitForSingleObject(sctx.barrier, INFINITE);
			CloseHandle(sctx.barrier);

			return (int)thread_id;
		}

		void BToolsInput::crt_thread_destroy(int thread_id)
		{
			Console.WriteLn("crt_thread_destroy");
			CloseHandle((HANDLE)(uintptr_t)thread_id);
		}

		void BToolsInput::crt_thread_join(int thread_id, int* result)
		{
			Console.WriteLn("crt_thread_join");

			WaitForSingleObject((HANDLE)(uintptr_t)thread_id, INFINITE);

			if (result)
			{
				GetExitCodeThread((HANDLE)(uintptr_t)thread_id, (DWORD*)result);
			}
		}

		void BToolsInput::InterruptReaderThread(void* ptr)
		{
			BToolsInput* dev = static_cast<BToolsInput*>(ptr);
			dev->isInterruptReaderThreadRunning = true;

			if (dev->m_ddr_io_read_pad)
			{
				Console.WriteLn("Btools polling thread start");

				while (true)
				{
					//BTools API claims that this method will sleep/prevent banging.
					//so we can safely loop around it.
					dev->ddrioState = dev->m_ddr_io_read_pad();
				}
			}
			else
			{
				Console.Error("Btools Thread: Could not find m_ddr_io_read_pad");
			}

			dev->isInterruptReaderThreadRunning = false;
		}

		int BToolsInput::Open()
		{
			hDDRIO = LoadLibraryA("ddrio.dll");

			if (hDDRIO != nullptr)
			{
				m_ddr_io_read_pad = (ddr_io_read_pad_type*)GetProcAddress(hDDRIO, "ddr_io_read_pad");

				if (!isInterruptReaderThreadRunning)
				{
					if (interruptThread.joinable())
						interruptThread.join();
					interruptThread = std::thread(BToolsInput::InterruptReaderThread, this);
				}

				Console.WriteLn("BToolsInput start");
			}
			else
			{
				Console.Error("Error #%d loading ddrio.dll input. ignoring...", GetLastError());

				return -1;
			}

			return 0;
		}

		int BToolsInput::Close()
		{
			return 0;
		}

		bool BToolsInput::GetKeyState(std::wstring keybind)
		{
			if (keybind == L"Test")
				return ddrioState & (1 << DDR_TEST);
			if (keybind == L"Service")
				return ddrioState & (1 << DDR_SERVICE);
			if (keybind == L"Coin1")
				return ddrioState & (1 << DDR_COIN);

			if (keybind == L"DdrP1Start")
				return ddrioState & (1 << DDR_P1_START);
			if (keybind == L"DdrP1SelectL")
				return ddrioState & (1 << DDR_P1_MENU_LEFT);
			if (keybind == L"DdrP1SelectR")
				return ddrioState & (1 << DDR_P1_MENU_RIGHT);

			if (keybind == L"DdrP1FootUp")
				return ddrioState & (1 << DDR_P1_UP);
			if (keybind == L"DdrP1FootDown")
				return ddrioState & (1 << DDR_P1_DOWN);
			if (keybind == L"DdrP1FootLeft")
				return ddrioState & (1 << DDR_P1_LEFT);
			if (keybind == L"DdrP1FootRight")
				return ddrioState & (1 << DDR_P1_RIGHT);

			if (keybind == L"DdrP2Start")
				return ddrioState & (1 << DDR_P2_START);
			if (keybind == L"DdrP2SelectL")
				return ddrioState & (1 << DDR_P2_MENU_LEFT);
			if (keybind == L"DdrP2SelectR")
				return ddrioState & (1 << DDR_P2_MENU_RIGHT);

			if (keybind == L"DdrP2FootUp")
				return ddrioState & (1 << DDR_P2_UP);
			if (keybind == L"DdrP2FootDown")
				return ddrioState & (1 << DDR_P2_DOWN);
			if (keybind == L"DdrP2FootLeft")
				return ddrioState & (1 << DDR_P2_LEFT);
			if (keybind == L"DdrP2FootRight")
				return ddrioState & (1 << DDR_P2_RIGHT);

			return false;
		}

		void ConfigurePython2Btools(Python2DlgConfig& config);
		int BToolsInput::Configure(int port, const char* dev_type, void* data)
		{
			std::vector<wxString> devList;
			std::vector<wxString> devListGroups;

			const wxString iniPath = StringUtil::UTF8StringToWxString(Path::Combine(EmuFolders::Settings, "Python2.ini"));
			CIniFile ciniFile;

			if (!ciniFile.Load(iniPath.ToStdWstring()))
				return 0;

			auto sections = ciniFile.GetSections();
			for (auto itr = sections.begin(); itr != sections.end(); itr++)
			{
				auto groupName = (*itr)->GetSectionName();
				if (groupName.find(L"GameEntry ") == 0)
				{
					devListGroups.push_back(wxString(groupName));

					auto gameName = (*itr)->GetKeyValue(L"Name");
					if (!gameName.empty())
						devList.push_back(wxString(gameName));
				}
			}

			Python2DlgConfig config(port, dev_type, devList, devListGroups);
			ConfigurePython2Btools(config);

			return 0;
		}
	} // namespace btools
} // namespace usb_python2
