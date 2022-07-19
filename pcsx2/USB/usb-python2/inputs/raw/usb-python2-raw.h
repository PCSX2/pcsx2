#ifndef USBPYTHON2RAW_H
#define USBPYTHON2RAW_H
#include <thread>
#include <array>
#include <atomic>
#include "USB/usb-python2/python2proxy.h"
#include "USB/usb-python2/usb-python2.h"
#include "USB/shared/rawinput_usb.h"

namespace usb_python2
{
#define CHECK(exp) \
	do \
	{ \
		if (!(exp)) \
			goto Error; \
	} while (0)
#define SAFE_FREE(p) \
	do \
	{ \
		if (p) \
		{ \
			free(p); \
			(p) = NULL; \
		} \
	} while (0)

	namespace raw
	{
		struct InputStateUpdate
		{
			wxDateTime timestamp;
			bool state;
		};

		const std::vector<TCHAR*> axisLabelList = {
			TEXT("X"),
			TEXT("Y"),
			TEXT("Z"),
			TEXT("RX"),
			TEXT("RY"),
			TEXT("RZ")};

		const std::vector<TCHAR*> buttonDefaultOneshotList = {
			TEXT("DmHihat"),
			TEXT("DmSnare"),
			TEXT("DmHighTom"),
			TEXT("DmLowTom"),
			TEXT("DmCymbal"),
			TEXT("DmBassDrum"),

			TEXT("GfP1Pick"),
			TEXT("GfP2Pick")};

		const std::vector<TCHAR*> buttonLabelList = {
			// Machine
			TEXT("Test"),
			TEXT("Service"),
			TEXT("Coin1"),
			TEXT("Coin2"),

			// Guitar Freaks
			TEXT("GfP1Start"),
			TEXT("GfP1Pick"),
			TEXT("GfP1Wail"),
			TEXT("GfP1EffectInc"),
			TEXT("GfP1EffectDec"),
			TEXT("GfP1NeckR"),
			TEXT("GfP1NeckG"),
			TEXT("GfP1NeckB"),

			TEXT("GfP2Start"),
			TEXT("GfP2Pick"),
			TEXT("GfP2Wail"),
			TEXT("GfP2EffectInc"),
			TEXT("GfP2EffectDec"),
			TEXT("GfP2NeckR"),
			TEXT("GfP2NeckG"),
			TEXT("GfP2NeckB"),

			// Drummania
			TEXT("DmStart"),
			TEXT("DmSelectL"),
			TEXT("DmSelectR"),
			TEXT("DmHihat"),
			TEXT("DmSnare"),
			TEXT("DmHighTom"),
			TEXT("DmLowTom"),
			TEXT("DmCymbal"),
			TEXT("DmBassDrum"),

			// DDR
			TEXT("DdrP1Start"),
			TEXT("DdrP1SelectL"),
			TEXT("DdrP1SelectR"),
			TEXT("DdrP1FootLeft"),
			TEXT("DdrP1FootDown"),
			TEXT("DdrP1FootUp"),
			TEXT("DdrP1FootRight"),

			TEXT("DdrP2Start"),
			TEXT("DdrP2SelectL"),
			TEXT("DdrP2SelectR"),
			TEXT("DdrP2FootLeft"),
			TEXT("DdrP2FootDown"),
			TEXT("DdrP2FootUp"),
			TEXT("DdrP2FootRight"),

			// Thrill Drive
			TEXT("ThrillDriveStart"),
			TEXT("ThrillDriveGearUp"),
			TEXT("ThrillDriveGearDown"),
			TEXT("ThrillDriveWheelAnalog"),
			TEXT("ThrillDriveWheelLeft"),
			TEXT("ThrillDriveWheelRight"),
			TEXT("ThrillDriveAccelAnalog"),
			TEXT("ThrillDriveAccel"),
			TEXT("ThrillDriveBrake"),
			TEXT("ThrillDriveBrakeAnalog"),
			TEXT("ThrillDriveSeatbelt"),

			// Toy's March
			TEXT("ToysMarchP1Start"),
			TEXT("ToysMarchP1SelectL"),
			TEXT("ToysMarchP1SelectR"),
			TEXT("ToysMarchP1DrumL"),
			TEXT("ToysMarchP1DrumR"),
			TEXT("ToysMarchP1Cymbal"),

			TEXT("ToysMarchP2Start"),
			TEXT("ToysMarchP2SelectL"),
			TEXT("ToysMarchP2SelectR"),
			TEXT("ToysMarchP2DrumL"),
			TEXT("ToysMarchP2DrumR"),
			TEXT("ToysMarchP2Cymbal"),

			// Dance 86.4
			TEXT("Dance864P1Start"),
			TEXT("Dance864P1Left"),
			TEXT("Dance864P1Right"),
			TEXT("Dance864P1PadLeft"),
			TEXT("Dance864P1PadCenter"),
			TEXT("Dance864P1PadRight"),

			TEXT("Dance864P2Start"),
			TEXT("Dance864P2Left"),
			TEXT("Dance864P2Right"),
			TEXT("Dance864P2PadLeft"),
			TEXT("Dance864P2PadCenter"),
			TEXT("Dance864P2PadRight"),

			// ICCA Card Reader
			TEXT("KeypadP1_0"),
			TEXT("KeypadP1_1"),
			TEXT("KeypadP1_2"),
			TEXT("KeypadP1_3"),
			TEXT("KeypadP1_4"),
			TEXT("KeypadP1_5"),
			TEXT("KeypadP1_6"),
			TEXT("KeypadP1_7"),
			TEXT("KeypadP1_8"),
			TEXT("KeypadP1_9"),
			TEXT("KeypadP1_00"),
			TEXT("KeypadP1InsertEject"),
			TEXT("KeypadP2_0"),
			TEXT("KeypadP2_1"),
			TEXT("KeypadP2_2"),
			TEXT("KeypadP2_3"),
			TEXT("KeypadP2_4"),
			TEXT("KeypadP2_5"),
			TEXT("KeypadP2_6"),
			TEXT("KeypadP2_7"),
			TEXT("KeypadP2_8"),
			TEXT("KeypadP2_9"),
			TEXT("KeypadP2_00"),
			TEXT("KeypadP2InsertEject"),
		};

		class RawInputPad : public Python2Input, shared::rawinput::ParseRawInputCB
		{
		public:
			RawInputPad(int port, const char* dev_type)
				: Python2Input(port, dev_type)
			{
				if (!InitHid())
					throw UsbPython2Error("InitHid() failed!");
			}
			~RawInputPad()
			{
				Close();
			}
			int Open() override;
			int Close() override;
			int ReadPacket(std::vector<uint8_t>& data) override { return 0; };
			int WritePacket(const std::vector<uint8_t>& data) override { return 0; };
			void ReadIo(std::vector<uint8_t>& data) override {}
			int Reset() override { return 0; }
			void ParseRawInput(PRAWINPUT pRawInput);

			bool isPassthrough() override { return false; }

			static const TCHAR* Name()
			{
				return TEXT("Raw Input");
			}

			void UpdateKeyStates(TSTDSTRING keybind) override;
			bool GetKeyState(TSTDSTRING keybind) override;
			bool GetKeyStateOneShot(TSTDSTRING keybind) override;
			double GetKeyStateAnalog(TSTDSTRING keybind) override;
			bool IsAnalogKeybindAvailable(TSTDSTRING keybind) override;

			static int Configure(int port, const char* dev_type, void* data);

		protected:
		private:
		};

		enum KeybindType
		{
			KeybindType_Button = 0,
			KeybindType_Axis,
			KeybindType_Hat,
			KeybindType_Keyboard
		};

		struct KeyMapping
		{
			uint32_t uniqueId;
			uint32_t keybindId;
			uint32_t bindType; // 0 = button, 1 = axis, 2 = hat, 3 = keyboard
			uint32_t value;
			bool isOneshot; // Immediately trigger an off after on
		};

		struct Mappings
		{
			std::vector<KeyMapping> mappings;

			TSTDSTRING devName;
#if _WIN32
			TSTDSTRING hidPath;
#endif
		};

#ifndef PCSX2_CORE
		struct Python2DlgConfig
		{
			int port;
			const char* dev_type;

			const std::vector<TSTDSTRING> devList;
			const std::vector<TSTDSTRING> devListGroups;

			Python2DlgConfig(int p, const char* dev_type_, const std::vector<TSTDSTRING>& devList, const std::vector<TSTDSTRING>& devListGroups)
				: port(p)
				, dev_type(dev_type_)
				, devList(devList)
				, devListGroups(devListGroups)
			{
			}
		};
#endif

		typedef std::vector<Mappings> MapVector;
		static MapVector mapVector;
		static std::map<HANDLE, Mappings*> mappings;

		static std::map<TSTDSTRING, std::deque<InputStateUpdate>> keyStateUpdates;
		static std::map<TSTDSTRING, bool> isOneshotState;
		static std::map<TSTDSTRING, bool> currentKeyStates;
		static std::map<TSTDSTRING, double> currentInputStateAnalog;

		static std::map<uint16_t, bool> keyboardButtonIsPressed;
		static std::map<TSTDSTRING, std::map<uint32_t, bool>> gamepadButtonIsPressed;

		void LoadMappings(const char* dev_type, MapVector& maps);
		void SaveMappings(const char* dev_type, MapVector& maps);

	} // namespace raw
} // namespace usb_python2
#endif
