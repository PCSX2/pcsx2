#include "PrecompiledHeader.h"

#include "icca.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"

namespace usb_python2
{
	bool acio_icca_device::device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& outputResponse)
	{
		const auto header = (ACIO_PACKET_HEADER*)packet.data();
		const auto code = BigEndian16(header->code);

		std::vector<uint8_t> response;
		bool isEmptyResponse = false;
		if (code == 0x0002)
		{
			const uint8_t resp[] = {
				0x03, 0x00, 0x00, 0x00, // Device ID
				0x00, // Flag
				0x01, // Major version
				0x01, // Minor, Supernova checks that this is >= 0
				0x00, // Version
				'I', 'C', 'C', 'A', // Product code
				'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', '\0', '\0', '\0', '\0', '\0', // Date
				'1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', '\0', '\0', '\0', '\0' // Time
			};

			response.insert(response.end(), std::begin(resp), std::end(resp));
		}
		else if (code == 0x0003)
		{
			// Startup
			response.push_back(0);

			// Reset device state
			accept = false;
			inserted = false;
			isCardInsertPressed = false;
			keyLastActiveState = 0;
			keyLastActiveEvent[0] = keyLastActiveEvent[1] = 0;
		}
		else if (code == 0x0080)
		{
			isEmptyResponse = true;
		}
		else if (code == 0x0131 || code == 0x0134 || code == 0x0135)
		{
			if (code == 0x0135)
			{
				switch (packet[7])
				{
					case 0x00:
						accept = false;
						break;
					case 0x11:
						accept = true;
						break;
					case 0x12:
						accept = false;
						inserted = false;
						isCardInsertPressed = false;
						break;
					default:
						break;
				}
			}

			int curkey = 0;
			if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][0]))
				curkey |= 16;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][1]))
				curkey |= 1;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][2]))
				curkey |= 5;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][3]))
				curkey |= 9;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][4]))
				curkey |= 2;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][5]))
				curkey |= 6;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][6]))
				curkey |= 10;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][7]))
				curkey |= 3;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][8]))
				curkey |= 7;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][9]))
				curkey |= 11;
			else if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][10]))
				curkey |= 4;

			if (p2dev->GetKeyState(keypadIdsByDeviceId[header->addr - 1][11]))
			{
				if (!isCardInsertPressed)
				{
					if (accept || inserted)
						inserted = !inserted;

					isCardInsertPressed = true;
				}
			}
			else
			{
				isCardInsertPressed = false;
			}

			if (inserted && !cardLoaded)
			{
				isCardInsertPressed = false;
				inserted = false;
				std::fill(std::begin(cardId), std::end(cardId), 0);

				auto cardIdStr(FileSystem::ReadFileToString(cardFilename.c_str()));
				if (cardIdStr.has_value())
				{
					auto cardIdStrDecoded = StringUtil::DecodeHex(cardIdStr.value());
					cardLoaded = cardIdStrDecoded.has_value() && cardIdStrDecoded.value().size() >= 8;

					if (cardLoaded)
					{
						auto cardIdStrDecodedVal = cardIdStrDecoded.value();
						std::copy(std::begin(cardIdStrDecodedVal), std::begin(cardIdStrDecodedVal) + 8, cardId);
						Console.WriteLn("Card value: %s", cardIdStr.value().c_str());
					}
				}

				if (!cardLoaded)
					Console.WriteLn("Could not open %s\n", cardFilename.c_str());
			}

			uint8_t readerStatus = inserted ? 2 : 1;
			uint8_t sensorStatus = (accept && inserted && cardLoaded) ? 0x30 : 0;
			uint8_t resp[] = {
				readerStatus,
				sensorStatus,
				0, 0, 0, 0, 0, 0, 0, 0, // Card ID
				0,
				3, // Keypad started
				0, // Key events new
				0, // Key event previous
				0, 0};

			if (inserted)
				std::copy(std::begin(cardId), std::end(cardId), &resp[2]);

			uint8_t ev = 0;
			if (curkey & (keyLastActiveState ^ curkey))
			{
				if (keyLastActiveEvent[0])
					ev = (keyLastActiveEvent[0] + 0x10) & 0xf0;

				ev |= 0x80 | curkey;
				keyLastActiveEvent[1] = keyLastActiveEvent[0];
				keyLastActiveEvent[0] = ev;
			}

			resp[12] = keyLastActiveEvent[0];
			resp[13] = keyLastActiveEvent[1];

			keyLastActiveState = curkey;

			response.insert(response.end(), std::begin(resp), std::end(resp));
		}
		else
		{
			// Just return 0 for anything else
			response.push_back(0);
		}

		if (response.size() > 0 || isEmptyResponse)
		{
			outputResponse.insert(outputResponse.end(), response.begin(), response.end());
			return true;
		}

		if (!inserted)
			cardLoaded = false;

		return false;
	}
} // namespace usb_python2