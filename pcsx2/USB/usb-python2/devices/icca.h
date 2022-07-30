#include "USB/usb-python2/usb-python2.h"
#include "acio.h"

namespace usb_python2
{
	class acio_icca_device : public acio_device_base
	{
	private:
		Python2Input* p2dev;

	protected:
		uint8_t keyLastActiveState = 0;
		uint8_t keyLastActiveEvent[2] = {0, 0};
		bool accept = false;
		bool inserted = false;
		bool isCardInsertPressed = false;
		bool isKeypadSwapped = false;
		bool isKeypadSwapPressed = false;

		bool cardLoaded = false;
		uint8_t cardId[8] = {0};
		std::string cardFilename = "";

		std::wstring keypadIdsByDeviceId[2][12] = {
			{L"KeypadP1_0",
				L"KeypadP1_1",
				L"KeypadP1_2",
				L"KeypadP1_3",
				L"KeypadP1_4",
				L"KeypadP1_5",
				L"KeypadP1_6",
				L"KeypadP1_7",
				L"KeypadP1_8",
				L"KeypadP1_9",
				L"KeypadP1_00",
				L"KeypadP1InsertEject"},
			{L"KeypadP2_0",
				L"KeypadP2_1",
				L"KeypadP2_2",
				L"KeypadP2_3",
				L"KeypadP2_4",
				L"KeypadP2_5",
				L"KeypadP2_6",
				L"KeypadP2_7",
				L"KeypadP2_8",
				L"KeypadP2_9",
				L"KeypadP2_00",
				L"KeypadP2InsertEject"},
		};

		void write(std::vector<uint8_t>& packet) {}

	public:
		acio_icca_device(Python2Input* device)
		{
			p2dev = device;
		}

		acio_icca_device(Python2Input* device, std::string targetCardFilename)
		{
			p2dev = device;
			cardFilename = targetCardFilename;
		}

		bool device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& outputResponse);
	};
} // namespace usb_python2
#pragma once
