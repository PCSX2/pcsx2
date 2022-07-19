#include "usb-python2.h"
#include "python2proxy.h"

namespace usb_python2
{
	namespace noop
	{
		static const char* APINAME = "noop";

		class NOOP : public Python2Input
		{
		public:
			NOOP(int port, const char* dev_type)
				: Python2Input(port, dev_type)
			{
			}
			~NOOP() {}
			int Open() { return 0; }
			int Close() { return 0; }
			int TokenIn(uint8_t* buf, int len) { return len; }
			int TokenOut(const uint8_t* data, int len) { return len; }
			int ReadPacket(std::vector<uint8_t>& data) { return 0; }
			int WritePacket(const std::vector<uint8_t>& data) { return 0; }
			void ReadIo(std::vector<uint8_t>& data) {}
			int Reset() { return 0; }
			bool isPassthrough() { return false; }

			void UpdateKeyStates(std::string keybind) {};
			bool GetKeyState(std::string keybind) { return false; };
			bool GetKeyStateOneShot(std::string keybind) { return false; };
			double GetKeyStateAnalog(std::string keybind) { return 0; };
			bool IsAnalogKeybindAvailable(std::string keybind) { return false; };

			static const TCHAR* Name()
			{
				return TEXT("NOOP");
			}

			static int Configure(int port, const std::string& api, void* data)
			{
				return RESULT_CANCELED;
			}
		};

	} // namespace noop
} // namespace usb_python2
