#include "USB/usb-python2/usb-python2.h"
#include "acio.h"

namespace usb_python2
{
	class thrilldrive_handle_device : public acio_device_base
	{
	private:
		Python2Input* p2dev;

		void write(std::vector<uint8_t>& packet) {}

	public:
		thrilldrive_handle_device(Python2Input* device) noexcept
		{
			p2dev = device;
		}

		bool device_write(std::vector<uint8_t>& packet, std::vector<uint8_t>& outputResponse);

		int8_t wheelForceFeedback = 0;
		bool wheelCalibrationHack = false;
	};
} // namespace usb_python2

#pragma once
