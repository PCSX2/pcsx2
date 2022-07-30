#include "thrilldrive_handle.h"

namespace usb_python2
{
	bool thrilldrive_handle_device::device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& outputResponse)
	{
		const auto header = (ACIO_PACKET_HEADER*)packet.data();
		const auto code = BigEndian16(header->code);

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
				'H', 'N', 'D', 'L', // Product code
				'O', 'c', 't', ' ', '2', '6', ' ', '2', '0', '0', '5', '\0', '\0', '\0', '\0', '\0', // Date
				'1', '3', ' ', ':', ' ', '5', '5', ' ', ':', ' ', '0', '3', '\0', '\0', '\0', '\0' // Time
			};
			response.insert(response.end(), std::begin(resp), std::end(resp));
		}
		else if (code == 0x0100)
		{
			// Init
			wheelForceFeedback = 0;
			wheelCalibrationHack = false;
			response.push_back(0);
		}
		else if (code == 0x0120)
		{
			// Force feedback
			uint8_t resp[4] = {0};

			resp[0] = 0; // Status

			int8_t ffb1 = packet[6]; // Controls X?
			//int8_t ffb2 = packet[7];
			//int8_t ffb3 = packet[8];
			int8_t ffb4 = packet[9];

			int16_t* val = (int16_t*)&resp[2];
			*val = BigEndian16(0); // What is this value?

			wheelForceFeedback = ffb1;
			wheelCalibrationHack = ffb4 == -2;

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