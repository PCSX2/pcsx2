#ifndef USBPYTHON2PASSTHROUGH_H
#define USBPYTHON2PASSTHROUGH_H
#include <thread>
#include <array>
#include <atomic>
#include "USB/usb-python2/python2proxy.h"
#include "USB/usb-python2/usb-python2.h"
#include "USB/readerwriterqueue/readerwriterqueue.h"

#include "libusb-1.0/libusb.h"


namespace usb_python2
{
	namespace passthrough
	{
		static const char* APINAME = "passthrough";

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

		class PassthroughInput : public Python2Input
		{
		public:
			PassthroughInput(int port, const char* dev_type)
				: Python2Input(port, dev_type)
				, ctx(NULL)
				, handle(NULL)
				, isInterruptReaderThreadRunning(false)
			{
			}

			~PassthroughInput()
			{
				Close();
			}

			int Open() override;
			int Close() override;
			int ReadPacket(std::vector<uint8_t>& data) override;
			int WritePacket(const std::vector<uint8_t>& data) override;
			void ReadIo(std::vector<uint8_t>& data) override;
			int Reset() override { return 0; }

			bool isPassthrough() override { return true; }

			void UpdateKeyStates(std::wstring keybind) override {}
			bool GetKeyState(std::wstring keybind) override { return false; }
			bool GetKeyStateOneShot(std::wstring keybind) override { return false; }
			double GetKeyStateAnalog(std::wstring keybind) override { return 0; }
			bool IsKeybindAvailable(std::wstring keybind) override { return false; }
			bool IsAnalogKeybindAvailable(std::wstring keybind) override { return false; }

			static const TCHAR* Name()
			{
				return TEXT("Passthrough");
			}

			static int Configure(int port, const char* dev_type, void* data);

		protected:
			static void InterruptReaderThread(void* ptr);

		private:
			libusb_context* ctx = NULL;
			libusb_device_handle* handle = NULL;

			std::thread interruptThread;
			std::atomic<bool> isInterruptReaderThreadRunning;
			uint8_t ioData[12];
		};
	} // namespace passthrough
} // namespace usb_python2
#endif
