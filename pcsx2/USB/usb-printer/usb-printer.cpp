// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/usb-printer/usb-printer.h"
#include "USB/USB.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "Config.h"
#include "fmt/format.h"
#include "StateWrapper.h"
#include "Host.h"
#include "IconsFontAwesome5.h"

namespace usb_printer
{
	static const char* sPrinterNames[] = {
		"Sony DPP-MP1",
	};

	typedef struct PrinterState
	{
		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		int selected_printer = 0;
		int cmd_state = 0;
		uint8_t last_command[65] = {};
		int last_command_size = 0;
		std::string print_filename;
		std::FILE* print_file = nullptr;
		int width = 0;
		int height = 0;
		long stride = 0;
		int data_size = 0;
		long data_pos = 0;
	} PrinterState;

	static void usb_printer_handle_reset(USBDevice* dev)
	{
		PrinterState* s = USB_CONTAINER_OF(dev, PrinterState, dev);
		s->cmd_state = 0;
		if (s->print_file)
		{
			std::fclose(s->print_file);
			s->print_file = nullptr;
		}
	}

	static void usb_printer_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		PrinterState* s = USB_CONTAINER_OF(dev, PrinterState, dev);
		int ret = 0;

		ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			return;
		}

		switch (request)
		{
			case ClassInterfaceRequest | GET_DEVICE_ID:
				ret = 2 + StringUtil::Strlcpy((char*)data + 2, sPrinters[s->selected_printer].device_id, length - 2);
				data[0] = ret >> 8;
				data[1] = ret & 0xff;
				p->actual_length = ret;
				break;
			case ClassInterfaceRequest | GET_PORT_STATUS:
				data[0] = GET_PORT_STATUS_PAPER_NOT_EMPTY | GET_PORT_STATUS_SELECTED | GET_PORT_STATUS_NO_ERROR;
				p->actual_length = 1;
				break;
		}
	}

	static void sony_open_file(PrinterState* s)
	{
		char cur_time_str[32];
		const time_t cur_time = time(nullptr);
		strftime(cur_time_str, sizeof(cur_time_str), "%Y_%m_%d_%H_%M_%S", localtime(&cur_time));

		s->print_filename = Path::Combine(EmuFolders::Snapshots, fmt::format("print_{}.bmp", cur_time_str));
		s->print_file = FileSystem::OpenCFile(s->print_filename.c_str(), "wb");
		if (!s->print_file)
		{
			Host::AddIconOSDMessage("USBPrinterOpen", ICON_FA_EXCLAMATION_TRIANGLE,
				fmt::format(TRANSLATE_FS("USB", "Failed to open '{}' for printing."), s->print_filename),
				Host::OSD_ERROR_DURATION);
			return;
		}

		Host::AddIconOSDMessage("USBPrinterOpen", ICON_FA_SAVE,
			fmt::format(TRANSLATE_FS("USB", "Printer saving to '{}'..."), Path::GetFileName(s->print_filename)),
			Host::OSD_INFO_DURATION);

		BMPHeader header = {0};
		header.magic = 0x4D42;
		header.filesize = sizeof(BMPHeader) + 3 * s->width * s->height;
		header.data_offset = sizeof(BMPHeader);
		header.core_header_size = 0x0C;
		header.width = s->width;
		header.height = s->height;
		header.planes = 1;
		header.bpp = 24;
		if (std::fwrite(&header, sizeof(header), 1, s->print_file) != 1)
		{
			Console.Error("Error writing header to print file");
		}

		s->stride = 3 * s->width + 3 - ((3 * s->width + 3) & 3);
		s->data_pos = 0;
		FileSystem::FSeek64(s->print_file, sizeof(BMPHeader) + s->stride * s->height - 1, SEEK_SET);
		char zero = 0;

		if (std::fwrite(&zero, 1, 1, s->print_file) != 1)
		{
			Console.Error("Error writing zero padding to header to print file");
		}
	}

	static void sony_write_data(PrinterState* s, int size, uint8_t* data)
	{
		for (int i = 0; i < size; i++)
		{
			long line = (s->data_pos / 3) / s->width;
			long col = (s->data_pos / 3) % s->width;
			long pos_out = s->stride * (s->height - 1 - line) + 3 * col;
			if (pos_out < 0)
			{
				Console.WriteLn("Printer: Sony: error: pos_out=0x%x", pos_out);
				break;
			}

			// print_file might be null if we're loading a state
			if (s->print_file)
			{
				FileSystem::FSeek64(s->print_file, sizeof(BMPHeader) + pos_out + 2 - s->data_pos % 3, SEEK_SET);

				if (std::fwrite(data + i, 1, 1, s->print_file) != 1)
				{
					Console.Error("Error writing data to print file");
				}
			}
			s->data_pos++;
		}
	}

	static void sony_close_file(PrinterState* s)
	{
		Console.WriteLn("Printer: Sony: done.");
		if (s->print_file)
		{
			std::fclose(s->print_file);
			s->print_file = nullptr;
			s->print_filename = {};
		}
	}

	static void sony_cancel_file(PrinterState* s)
	{
		if (!s->print_file)
			return;

		Console.Warning("Removing incomplete printer file '%s'", s->print_filename.c_str());
		std::fclose(s->print_file);
		s->print_file = nullptr;
		FileSystem::DeleteFilePath(s->print_filename.c_str());
		s->print_filename = {};
	}

	static void usb_printer_handle_data_sony(USBDevice* dev, USBPacket* p)
	{
		struct req_reply
		{
			uint8_t cmd[64];
			uint8_t ret[256];
		};
		const struct req_reply commands[] =
			{
				{{0x1B, 0xE0, 0x00, 0x00, 0x00, 0x0E, 0x00},
					{0x0D, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x01, 0x24, 0x38, 0x03, 0xF2, 0x02, 0x74}},
				{{0x1B, 0xCF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x2D, 0x00},
					{0x00, 0x00, 0x00, 0x29, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
				{{0x1B, 0xCF, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00},
					{0x00, 0x00, 0x00, 0x12, 0x02, 0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x0E, 0x00, 0x00, 0x07}},
				{{0x1B, 0xCF, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00},
					{0x00, 0x00, 0x00, 0x12, 0x03, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x07, 0x00, 0x00, 0x04}},
				{{0x1B, 0xCF, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00},
					{0x00, 0x00, 0x00, 0x0A, 0x04, 0x00, 0x00, 0x18, 0x01, 0x01, 0x01, 0x00, 0x02, 0x00}},
				{{0x1B, 0xCF, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00},
					{0x00, 0x00, 0x00, 0x0E, 0x05, 0x00, 0x02, 0x74, 0x03, 0xF2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00}},
				{{0x1B, 0x12, 0x01, 0x00, 0x00, 0x19, 0x00},
					{0x02, 0xC0, 0x00, 0x15, 0x03, 0xF2, 0x02, 0x74, 0x03, 0xF2, 0x02, 0x74, 0x01, 0x33, 0x00, 0x00,
						0x15, 0x01, 0x03, 0x23, 0x30, 0x31, 0x2E, 0x30, 0x30}}};
		const uint8_t set_size[] = {0x00, 0x00, 0x00, 0x00, 0xa7, 0x00};
		const uint8_t set_data[] = {0x1b, 0xea, 0x00, 0x00, 0x00, 0x00, 0x00};
		const uint8_t print_compl[] = {0x1b, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00};

		PrinterState* s = USB_CONTAINER_OF(dev, PrinterState, dev);
		//const uint8_t ep_nr = p->ep->nr;
		//const uint8_t ep_type = p->ep->type;

		switch (p->pid)
		{
			case USB_TOKEN_OUT:
				s->last_command_size = p->buffer_size;
				usb_packet_copy(p, s->last_command, s->last_command_size);

				if (s->cmd_state == 0 && s->last_command_size > 5)
				{
					if (memcmp(s->last_command, set_size, sizeof(set_size)) == 0)
					{
						s->width = s->last_command[9] << 8 | s->last_command[10];
						s->height = s->last_command[11] << 8 | s->last_command[12];
						Console.WriteLn("Printer: Sony: Size=%dx%d", s->width, s->height);
						sony_open_file(s);
					}
					else if (memcmp(s->last_command, set_data, sizeof(set_data)) == 0)
					{
						s->data_size = s->last_command[8] << 8 | s->last_command[9];
						s->cmd_state = 1;
					}
					else if (memcmp(s->last_command, print_compl, sizeof(print_compl)) == 0)
					{
						sony_close_file(s);
					}
				}
				else if (s->cmd_state == 1 && s->last_command_size > 0)
				{
					sony_write_data(s, s->last_command_size, s->last_command);
					s->data_size -= s->last_command_size;
					if (s->data_size <= 0)
					{
						s->cmd_state = 0;
					}
				}
				break;
			case USB_TOKEN_IN:
				if (s->cmd_state == 0 && s->last_command_size > 0)
				{
					for (uint32_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
					{
						if (s->last_command[1] == 0xCF)
						{
							if (memcmp(s->last_command, commands[i].cmd, 11) == 0)
							{
								int length = commands[i].cmd[9];
								usb_packet_copy(p, (void*)commands[i].ret, length);
								s->last_command_size = 0;
							}
						}
						else
						{
							if (memcmp(s->last_command, commands[i].cmd, 7) == 0)
							{
								int length = commands[i].cmd[5];
								usb_packet_copy(p, (void*)commands[i].ret, length);
								s->last_command_size = 0;
							}
						}
					}
				}
				break;
			default:
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_printer_handle_destroy(USBDevice* dev)
	{
		PrinterState* s = USB_CONTAINER_OF(dev, PrinterState, dev);
		sony_cancel_file(s);
		delete s;
	}

	USBDevice* PrinterDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		PrinterState* s = new PrinterState();

		s->selected_printer = std::min<u32>(subtype, std::size(sPrinters));

		s->dev.speed = USB_SPEED_FULL;
		s->desc.full = &s->desc_dev;
		s->desc.str = sPrinters[subtype].usb_strings;
		if (usb_desc_parse_dev(sPrinters[subtype].device_descriptor, sPrinters[subtype].device_descriptor_size, s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(sPrinters[subtype].config_descriptor, sPrinters[subtype].config_descriptor_size, s->desc_dev) < 0)
			goto fail;

		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_printer_handle_reset;
		s->dev.klass.handle_control = usb_printer_handle_control;
		switch (sPrinters[subtype].protocol)
		{
			case ProtocolSonyUPD:
				s->dev.klass.handle_data = usb_printer_handle_data_sony;
				break;
		}
		s->dev.klass.unrealize = usb_printer_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = s->desc.str[2];

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		usb_printer_handle_reset(&s->dev);
		return &s->dev;

	fail:
		usb_printer_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* PrinterDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "Printer");
	}

	const char* PrinterDevice::TypeName() const
	{
		return "printer";
	}

	bool PrinterDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		PrinterState* s = USB_CONTAINER_OF(dev, PrinterState, dev);

		if (!sw.DoMarker("PrinterDevice"))
			return false;

		sw.Do(&s->selected_printer);
		sw.Do(&s->cmd_state);
		sw.DoBytes(&s->last_command, sizeof(s->last_command));
		sw.Do(&s->last_command_size);
		sw.Do(&s->width);
		sw.Do(&s->height);
		sw.Do(&s->stride);
		sw.Do(&s->data_size);
		sw.Do(&s->data_pos);

		// toss any file being saved when we're loading, since we'd probably
		// end up with a corrupted file otherwise
		if (sw.IsReading())
			sony_cancel_file(s);

		return true;
	}

	std::span<const char*> PrinterDevice::SubTypes() const
	{
		return sPrinterNames;
	}

} // namespace usb_printer
