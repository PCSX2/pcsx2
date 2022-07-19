#ifndef USBPYTHON2NATIVE_H
#define USBPYTHON2NATIVE_H
#include <thread>
#include <array>
#include <atomic>
#include <chrono>
#include "Frontend/InputManager.h"
#include "USB/usb-python2/python2proxy.h"
#include "USB/usb-python2/usb-python2.h"

namespace usb_python2
{
	namespace native
	{
		static const char* APINAME = "native";

		struct InputStateUpdate
		{
			std::chrono::steady_clock::time_point timestamp;
			bool state;
		};

		class NativeInput : public Python2Input
		{
		public:
			NativeInput(int port, const char* dev_type)
				: Python2Input(port, dev_type)
			{
			}
			~NativeInput()
			{
				Close();
			}
			int Open() override;
			int Close() override;
			int ReadPacket(std::vector<uint8_t>& data) override { return 0; }
			int WritePacket(const std::vector<uint8_t>& data) override { return 0; }
			void ReadIo(std::vector<uint8_t>& data) override {}
			int Reset() override;

			bool isPassthrough() override { return false; }

			InputInterceptHook::CallbackResult ParseInput(InputBindingKey key, float value);

			static const TCHAR* Name()
			{
				return TEXT("Raw Input");
			}

			void UpdateKeyStates(TSTDSTRING keybind) override;
			bool GetKeyState(TSTDSTRING keybind) override;
			bool GetKeyStateOneShot(TSTDSTRING keybind) override;
			double GetKeyStateAnalog(TSTDSTRING keybind) override;
			bool IsAnalogKeybindAvailable(TSTDSTRING keybind) override;

			static int Configure(int port, const char* dev_type, void* data) { return 0; }

		protected:
		private:
		};

		enum KeybindType
		{
			KeybindType_Button = 0,
			KeybindType_Axis,
			KeybindType_Hat,
			KeybindType_Keyboard,
			KeybindType_Motor,
		};

		struct KeyMapping
		{
			TSTDSTRING inputKey;
			TSTDSTRING keybind;
			double analogDeadzone;
			double analogSensitivity;
			double motorScale;
			bool isOneshot; // Immediately trigger an off after on
		};

		static std::map<TSTDSTRING, std::deque<InputStateUpdate>> keyStateUpdates;
		static std::map<TSTDSTRING, bool> isOneshotState;
		static std::map<TSTDSTRING, bool> currentKeyStates;
		static std::map<TSTDSTRING, int> currentInputStateKeyboard;
		static std::map<TSTDSTRING, int> currentInputStatePad;
		static std::map<TSTDSTRING, double> currentInputStateAnalog;

		static std::map<TSTDSTRING, bool> keyboardButtonIsPressed;
		static std::map<uint32_t, bool> gamepadButtonIsPressed;

	} // namespace native
} // namespace usb_python2
#endif
