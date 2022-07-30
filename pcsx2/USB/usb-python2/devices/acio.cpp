#include "acio.h"

#include <algorithm>
#include <numeric>

namespace usb_python2
{
	std::vector<uint8_t> acio_unescape_packet(const std::vector<uint8_t>& buffer)
	{
		std::vector<uint8_t> output;
		bool invert = false;

		for (size_t i = 0; i < buffer.size(); i++)
		{
			if (buffer[i] == 0xff)
			{
				invert = true;
			}
			else
			{
				if (invert)
					output.push_back(~buffer[i]);
				else
					output.push_back(buffer[i]);

				invert = false;
			}
		}

		return output;
	}

	std::vector<uint8_t> acio_escape_packet(const std::vector<uint8_t>& buffer)
	{
		std::vector<uint8_t> output;

		for (size_t i = 0; i < buffer.size(); i++)
		{
			if (buffer[i] == 0xaa || buffer[i] == 0xff)
			{
				output.push_back(0xff);
				output.push_back(~buffer[i]);
			}
			else
			{
				output.push_back(buffer[i]);
			}
		}

		return output;
	}

	uint8_t acio_device_base::calculate_checksum(std::vector<uint8_t>& buffer, size_t start, size_t len)
	{
		return std::accumulate(buffer.begin() + start, buffer.begin() + start + len, 0);
	}

	uint8_t acio_device_base::calculate_checksum(std::vector<uint8_t>& buffer)
	{
		return calculate_checksum(buffer, 0, buffer.size());
	}

	void acio_device::add_acio_device(int index, std::unique_ptr<acio_device_base> device) noexcept
	{
		devices[index] = std::move(device);
	}

	void acio_device::write(std::vector<uint8_t>& packet)
	{
		if (!isOpen)
			return;

		if (packet.size() < sizeof(ACIO_PACKET_HEADER))
			return;

		size_t syncByteCount = 0;
		for (size_t i = 0; i < packet.size(); i++)
		{
			if (packet[i] == ACIO_SYNC_BYTE)
				syncByteCount++;
			else
				break;
		}

		if (syncByteCount == packet.size())
		{
			add_packet(packet);
			return;
		}

		const auto header = (ACIO_PACKET_HEADER*)packet.data();
		if (header->magic != ACIO_HEADER_MAGIC || packet.size() < header->len + sizeof(ACIO_PACKET_HEADER) + 1)
			return;

		const auto expectedChecksum = packet[header->len + sizeof(ACIO_PACKET_HEADER)];

		// Verify checksum
		const auto calculatedChecksum = calculate_checksum(packet, 1, header->len + sizeof(ACIO_PACKET_HEADER) - 1);

		if (expectedChecksum != calculatedChecksum)
		{
			/*
			printf("ACIO packet checksum bad! %02x vs %02x: ", expectedChecksum, calculatedChecksum);
			for (int i = 0; i < packet.size(); i++)
			{
				printf("%02x ", packet[i]);
			}
			printf("\n");
			*/

			return;
		}

		std::vector<uint8_t> response;
		const auto code = BigEndian16(header->code);
		bool isResponseAccepted = false;
		if (header->addr == 0 && code == 0x0001)
		{
			response.push_back(devices.size());
			isResponseAccepted = true;
		}
		else if (devices.find(header->addr) != devices.end())
		{
			isResponseAccepted = devices[header->addr]->device_write(packet, response);
		}

		if (isResponseAccepted)
		{
			const auto payloadLen = response.size();
			response.insert(response.begin(), packet.begin() + 1, packet.begin() + 5);
			response[0] |= 0x80; // Set as response
			response.insert(response.begin() + 4, payloadLen);
			response.push_back(calculate_checksum(response));
			response = acio_escape_packet(response);
			response.insert(response.begin(), 0xaa);
		}

		add_packet(response);

#ifdef PCSX2_DEVBUILD
		printf("acio_device response: ");
		for (size_t i = 0; i < response.size(); i++)
		{
			printf("%02x ", response[i]);
		}
		printf("\n");
#endif
	}
} // namespace usb_python2