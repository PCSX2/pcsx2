#ifndef USBPYTHON2BTOOLS_H
#define USBPYTHON2BTOOLS_H
#include <thread>
#include <array>
#include <atomic>
#include "USB/usb-python2/python2proxy.h"
#include "USB/usb-python2/usb-python2.h"
#include "USB/readerwriterqueue/readerwriterqueue.h"


namespace usb_python2
{
	namespace btools
	{
		static const char* APINAME = "btools";

		struct Python2DlgConfig
		{
			int port;
			const char* dev_type;

			const std::vector<wxString> devList;
			const std::vector<wxString> devListGroups;

			Python2DlgConfig(int p, const char* dev_type_, const std::vector<wxString>& devList, const std::vector<wxString>& devListGroups)
				: port(p)
				, dev_type(dev_type_)
				, devList(devList)
				, devListGroups(devListGroups)
			{
			}
		};

		class BToolsInput : public Python2Input
		{
		public:
			BToolsInput(int port, const char* dev_type)
				: Python2Input(port, dev_type)
				, isInterruptReaderThreadRunning(false)
			{
			}

			~BToolsInput()
			{
				Close();
			}

			static int crt_thread_create(int (*proc)(void*), void* ctx, uint32_t stack_sz, unsigned int priority);
			static void crt_thread_destroy(int thread_id);
			static void crt_thread_join(int thread_id, int* result);

			int Open() override;
			int Close() override;
			int ReadPacket(std::vector<uint8_t>& data) override { return 0; };
			int WritePacket(const std::vector<uint8_t>& data) override { return 0; };
			void ReadIo(std::vector<uint8_t>& data) override {}
			int Reset() override { return 0; }

			bool isPassthrough() override { return false; }

			void UpdateKeyStates(std::wstring keybind) override {}
			bool GetKeyState(std::wstring keybind);
			bool GetKeyStateOneShot(std::wstring keybind) override { return false; }
			double GetKeyStateAnalog(std::wstring keybind) override { return 0; }
			bool IsKeybindAvailable(std::wstring keybind) override { return false; }
			bool IsAnalogKeybindAvailable(std::wstring keybind) override { return false; }

			bool set_p3io_lights(uint8_t stateFromGame);

			static const TCHAR* Name()
			{
				return TEXT("BTools");
			}

			static int Configure(int port, const char* dev_type, void* data);

		protected:
			static void InterruptReaderThread(void* ptr);

		private:
			std::thread interruptThread;
			std::atomic<bool> isInterruptReaderThreadRunning;
			uint32_t ddrioState = 0;

			HINSTANCE hDDRIO = nullptr;
			typedef uint32_t(WINAPI ddr_io_read_pad_type)(void);
			ddr_io_read_pad_type* m_ddr_io_read_pad = nullptr;
		};
	} // namespace passthrough
} // namespace usb_python2
#endif
