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

#include "PrecompiledHeader.h"
#include "../qemu-usb/vl.h"
#include "../qemu-usb/desc.h"
#include "../shared/inifile_usb.h"
#include "usb-printer.h"

#ifndef O_BINARY
	#define O_BINARY 0
#endif

namespace usb_printer
{
	typedef struct PopeggState
	{
		USBDevice dev;
		USBDesc desc;
		USBDescDevice desc_dev;
		int cmd_state;
		int print_file;
	} PopeggState;

	static const USBDescStrings popegg_desc_strings = {
		"",
		"Sony",
		"MPR-G600",
		"3H2IJg"};

	static void usb_printer_handle_reset(USBDevice* dev)
	{
		PopeggState* s = (PopeggState*)dev;
		s->cmd_state = 0;
		if (s->print_file > 0)
		{
			close(s->print_file);
		}
		s->print_file = -1;
	}

	static void usb_printer_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
									   int index, int length, uint8_t* data)
	{
		PopeggState* s = (PopeggState*)dev;
		int ret = 0;

		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			case ClassInterfaceRequest | GET_DEVICE_ID:
				p->actual_length = sprintf((char*)data, "MDL:MPR-G600;");
				break;
			case ClassInterfaceRequest | GET_PORT_STATUS:
				if (s->print_file > 0)
				{
					Console.WriteLn("Popegg: done.");
					close(s->print_file);
					s->print_file = -1;
				}
				data[0] = GET_PORT_STATUS_PAPER_NOT_EMPTY | GET_PORT_STATUS_SELECTED | GET_PORT_STATUS_NO_ERROR;
				p->actual_length = 1;
				break;
		}
	}

	static void usb_printer_handle_data(USBDevice* dev, USBPacket* p)
	{
		PopeggState* s = (PopeggState*)dev;
		unsigned char data[896] = {0};
		int actual_length = 0;
		const uint8_t ep_nr = p->ep->nr;
		const uint8_t ep_type = p->ep->type;

		switch (p->pid)
		{
			case USB_TOKEN_OUT:
				usb_packet_copy(p, data, MIN(p->iov.size, sizeof(data)));
				if (data[0] == 0x1b && data[1] == 0x5b && data[2] == 0x4b && data[3] == 0x1c)
				{
					s->cmd_state = 1;
				} else if (s->print_file < 0
					&& data[0] == 0x1b && data[1] == 0x5b && data[2] == 0x4b && data[3] == 0x02)
				{
					char filepath[32];
					const time_t cur_time = time(nullptr);
					strftime(filepath, sizeof(filepath), "print_%Y_%m_%d_%H_%M_%S.raw", localtime(&cur_time));
					s->print_file = open(filepath, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 666);
					Console.WriteLn("Popegg: Printing... ");
				}

				if (s->print_file >= 0)
				{
					write(s->print_file, &data[0], p->iov.size);
				}
				break;
			case USB_TOKEN_IN:
				if (s->cmd_state == 1)
				{
					const unsigned int len = sprintf((char*)data + 2, "xxDWS:NO;DOC:4,00,NO;DSC:NO;DBS:NO;DJS:NO;");
					data[0] = 0x00;
					data[1] = 1 + len;
					actual_length = 2 + len;
					s->cmd_state = 0;
				}
				else
				{
					const unsigned char status[] = {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff};
					memcpy(data, &status[0], 12);
					actual_length = 12;
				}
				usb_packet_copy(p, data, MIN(actual_length, sizeof(data)));
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_printer_handle_destroy(USBDevice* dev)
	{
		PopeggState* s = (PopeggState*)dev;
		delete s;
	}

	USBDevice* PopeggDevice::CreateDevice(int port)
	{
		PopeggState* s = new PopeggState();
		std::string api = *PopeggDevice::ListAPIs().begin();

		s->dev.speed = USB_SPEED_FULL;

		s->desc.full = &s->desc_dev;
		s->desc.str = popegg_desc_strings;
		if (usb_desc_parse_dev((const uint8_t*)&popegg_dev_desciptor, sizeof(popegg_dev_desciptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config((const uint8_t*)popegg_config_descriptor, sizeof(popegg_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_printer_handle_reset;
		s->dev.klass.handle_control = usb_printer_handle_control;
		s->dev.klass.handle_data = usb_printer_handle_data;
		s->dev.klass.unrealize = usb_printer_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = popegg_desc_strings[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		usb_printer_handle_reset((USBDevice*)s);
		return (USBDevice*)s;

	fail:
		usb_printer_handle_destroy((USBDevice*)s);
		return nullptr;
	}

	int PopeggDevice::Configure(int port, const std::string& api, void* data)
	{
		return 0;
	}

	int PopeggDevice::Freeze(int mode, USBDevice* dev, void* data)
	{
		return 0;
	}

} // namespace usb_printer
