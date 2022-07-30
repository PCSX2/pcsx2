#include "input_device.h"

#include <map>
#include <memory>
#include <vector>

namespace usb_python2
{
#ifdef _MSC_VER
#define BigEndian16(in) _byteswap_ushort(in)
#else
#define BigEndian16(in) __builtin_bswap16(in)
#endif

	struct ACIO_PACKET_HEADER
	{
		uint8_t magic;
		uint8_t addr;
		uint16_t code;
		uint8_t seqNo;
		uint8_t len;
	};

	constexpr uint8_t ACIO_HEADER_MAGIC = 0xaa;
	constexpr uint8_t ACIO_SYNC_BYTE = 0xaa;

	std::vector<uint8_t> acio_unescape_packet(const std::vector<uint8_t>& buffer);
	std::vector<uint8_t> acio_escape_packet(const std::vector<uint8_t>& buffer);

	class acio_device_base : public input_device
	{
	public:
		virtual bool device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& response) = 0;

	protected:
		uint8_t calculate_checksum(std::vector<uint8_t>& buffer, size_t start, size_t len);
		uint8_t calculate_checksum(std::vector<uint8_t>& buffer);
	};

	class acio_device : public acio_device_base
	{
		bool device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& response) { return false; }

		void write(std::vector<uint8_t>& packet);

	public:
		void add_acio_device(int index, std::unique_ptr<acio_device_base> device) noexcept;

		std::map<int, std::unique_ptr<acio_device_base>> devices;
	};
} // namespace usb_python2
#pragma once