/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USBPRINTER_H
#define USBPRINTER_H

#include "../deviceproxy.h"

#define GET_DEVICE_ID   0
#define GET_PORT_STATUS 1
#define SOFT_RESET      2

#define GET_PORT_STATUS_PAPER_EMPTY      1<<5
#define GET_PORT_STATUS_PAPER_NOT_EMPTY  0<<5
#define GET_PORT_STATUS_SELECTED         1<<4
#define GET_PORT_STATUS_NOT_SELECTED     0<<4
#define GET_PORT_STATUS_NO_ERROR         1<<3
#define GET_PORT_STATUS_ERROR            0<<3

namespace usb_printer
{
	static const char* APINAME = "no_api";

	class PopeggDevice
	{
	public:
		virtual ~PopeggDevice() {}
		static USBDevice* CreateDevice(int port);
		static const TCHAR* Name()
		{
			return TEXT("Sony MPR-G600A Popegg printer");
		}
		static const char* TypeName()
		{
			return "popegg_printer";
		}
		static std::list<std::string> ListAPIs()
		{
			return std::list<std::string>{APINAME};
		}
		static const TCHAR* LongAPIName(const std::string& name)
		{
			return TEXT("NO_API");
		}
		static int Configure(int port, const std::string& api, void* data);
		static int Freeze(int mode, USBDevice* dev, void* data);
		static std::vector<std::string> SubTypes()
		{
			return {};
		}
	};

	static const uint8_t popegg_dev_desciptor[] = {
		0x12,        // bLength
		0x01,        // bDescriptorType (Device)
		0x00, 0x01,  // bcdUSB 1.00
		0x00,        // bDeviceClass (Use class information in the Interface Descriptors)
		0x00,        // bDeviceSubClass 
		0x00,        // bDeviceProtocol 
		0x08,        // bMaxPacketSize0 8
		0x4C, 0x05,  // idVendor 0x054C
		0x3D, 0x00,  // idProduct 0x3D
		0x00, 0x01,  // bcdDevice 2.00
		0x01,        // iManufacturer (String Index)
		0x02,        // iProduct (String Index)
		0x03,        // iSerialNumber (String Index)
		0x01,        // bNumConfigurations 1
	};

	static const uint8_t popegg_config_descriptor[] = {
		0x09,        // bLength
		0x02,        // bDescriptorType (Configuration)
		0x20, 0x00,  // wTotalLength 32
		0x01,        // bNumInterfaces 1
		0x01,        // bConfigurationValue
		0x00,        // iConfiguration (String Index)
		0x40,        // bmAttributes Self Powered
		0x01,        // bMaxPower 2mA

		0x09,        // bLength
		0x04,        // bDescriptorType (Interface)
		0x00,        // bInterfaceNumber 0
		0x00,        // bAlternateSetting
		0x02,        // bNumEndpoints 2
		0x07,        // bInterfaceClass
		0x01,        // bInterfaceSubClass
		0x02,        // bInterfaceProtocol
		0x00,        // iInterface (String Index)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x01,        // bEndpointAddress (OUT/H2D)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)

		0x07,        // bLength
		0x05,        // bDescriptorType (Endpoint)
		0x82,        // bEndpointAddress (IN/D2H)
		0x02,        // bmAttributes (Bulk)
		0x40, 0x00,  // wMaxPacketSize 64
		0x00,        // bInterval 0 (unit depends on device speed)
	};

} // namespace usb_printer
#endif
