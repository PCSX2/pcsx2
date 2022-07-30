#include "USB/usb-python2/usb-python2.h"
#include "acio.h"

namespace usb_python2
{
	class toysmarch_drumpad_device : public input_device
	{

		int read(std::vector<uint8_t>& buf, const size_t requestedLen) override;
		void write(std::vector<uint8_t>& packet) override;

	private:
		Python2Input* p2dev;

	public:
		toysmarch_drumpad_device(Python2Input* device)
		{
			p2dev = device;
		}
	};
} // namespace usb_python2

#pragma once
