#include "thrilldrive_belt.h"

namespace usb_python2
{
	bool thrilldrive_belt_device::device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& outputResponse)
	{
		const auto header = (ACIO_PACKET_HEADER*)packet.data();
		const auto code = BigEndian16(header->code);

		if (p2dev->GetKeyState(L"ThrillDriveSeatbelt") != 0)
		{
			if (!seatBeltButtonPressed)
				seatBeltStatus = !seatBeltStatus;

			seatBeltButtonPressed = true;
		}
		else
		{
			seatBeltButtonPressed = false;
		}

		std::vector<uint8_t> response;
		bool isEmptyResponse = false;
		if (code == 0x0002)
		{
			// Not the real information for this device
			const uint8_t resp[] = {
				0x03, 0x00, 0x00, 0x00, // Device ID
				0x00, // Flag
				0x01, // Major version
				0x01, // Minor
				0x00, // Version
				'B', 'E', 'L', 'T', // Product code
				'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', '\0', '\0', '\0', '\0', '\0', // Date
				'1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', '\0', '\0', '\0', '\0' // Time
			};
			response.insert(response.end(), std::begin(resp), std::end(resp));
		}
		else if (code == 0x0100)
		{
			// Init
			seatBeltStatus = false;
			seatBeltButtonPressed = false;
			response.push_back(0);
		}
		else if (code == 0x0102)
		{
			// Something to do with the motor? Sent when you select the seatbelt output test.
			response.push_back(0);
		}
		else if (code == 0x0110)
		{
			// ?
			response.push_back(0);
		}
		else if (code == 0x0111)
		{
			// ?
			uint8_t resp[4] = {0};
			response.insert(response.end(), std::begin(resp), std::end(resp));
		}
		else if (code == 0x0113)
		{
			uint8_t resp[3] = {0};
			resp[0] = 0;
			resp[1] = 0;
			resp[2] = seatBeltStatus ? 0 : 0xff; // 0 = fastened
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

		return false;
	}
} // namespace usb_python2
