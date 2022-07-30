#include <cstdint>
#include <vector>

namespace usb_python2
{
	class input_device
	{
	public:
		virtual ~input_device() = default;

		virtual void write(std::vector<uint8_t>& buf) = 0;

		void open() noexcept { isOpen = true; }
		void close() noexcept { isOpen = false; }
		void clear() noexcept { response_buffer.clear(); }

		virtual int read(std::vector<uint8_t>& buf, const size_t requestedLen)
		{
			if (!isOpen || response_buffer.size() == 0)
				return 0;

			const auto sliceStart = response_buffer.begin();
			const auto sliceEnd = response_buffer.begin() + (requestedLen > response_buffer.size() ? response_buffer.size() : requestedLen);
			buf.insert(buf.end(), sliceStart, sliceEnd);

			response_buffer.erase(sliceStart, sliceEnd);

			return sliceEnd - sliceStart;
		}

	protected:
		bool isOpen = false;

		std::vector<uint8_t> response_buffer;

		void add_packet(const std::vector<uint8_t>& buffer)
		{
			if (buffer.size() > 0)
				response_buffer.insert(response_buffer.end(), buffer.begin(), buffer.end());
		}
	};
} // namespace usb_python2
#pragma once
