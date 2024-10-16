// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Assertions.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "IopDma.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winioctl.h>
#else
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <unistd.h>
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "DEV9.h"
#include "Config.h"
#include "smap.h"

#ifdef _WIN32
#pragma warning(disable : 4244)
#endif

dev9Struct dev9;

//#define HDD_48BIT

#if defined(__i386__) && !defined(_WIN32)

static __inline__ unsigned long long GetTickCount(void)
{
	unsigned long long int x;
	__asm__ volatile("rdtsc"
					 : "=A"(x));
	return x;
}

#elif defined(__x86_64__) && !defined(_WIN32)

static __inline__ unsigned long long GetTickCount(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc"
						 : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#endif

// clang-format off
u8 eeprom[] = {
	//0x6D, 0x76, 0x63, 0x61, 0x31, 0x30, 0x08, 0x01,
	0x76, 0x6D, 0x61, 0x63, 0x30, 0x31, 0x07, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

#ifdef _WIN32
HANDLE hEeprom;
HANDLE mapping;
#else
int hEeprom;
int mapping;
#endif

bool isRunning = false;

std::string GetHDDPath()
{
	//GHC uses UTF8 on all platforms
	std::string hddPath(EmuConfig.DEV9.HddFile);

	if (hddPath.empty())
		EmuConfig.DEV9.HddEnable = false;

	if (!Path::IsAbsolute(hddPath))
		hddPath = Path::Combine(EmuFolders::Settings, hddPath);

	return hddPath;
}

s32 DEV9init()
{
	DevCon.WriteLn("DEV9: DEV9init");

	memset(&dev9, 0, sizeof(dev9));
	dev9.ata = new ATA();
	DevCon.WriteLn("DEV9: DEV9init2");

	DevCon.WriteLn("DEV9: DEV9init3");

	FLASHinit();

#ifdef _WIN32
	hEeprom = CreateFile(
		L"eeprom.dat",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_WRITE_THROUGH,
		NULL);

	if (hEeprom == INVALID_HANDLE_VALUE)
	{
		dev9.eeprom = (u16*)eeprom;
	}
	else
	{
		mapping = CreateFileMapping(hEeprom, NULL, PAGE_READWRITE, 0, 0, NULL);
		if (mapping == INVALID_HANDLE_VALUE)
		{
			CloseHandle(hEeprom);
			dev9.eeprom = (u16*)eeprom;
		}
		else
		{
			dev9.eeprom = (u16*)MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, 0);

			if (dev9.eeprom == NULL)
			{
				CloseHandle(mapping);
				CloseHandle(hEeprom);
				dev9.eeprom = (u16*)eeprom;
			}
		}
	}
#else
	hEeprom = open("eeprom.dat", O_RDWR, 0);

	if (-1 == hEeprom)
	{
		dev9.eeprom = (u16*)eeprom;
	}
	else
	{
		dev9.eeprom = (u16*)mmap(NULL, 64, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, hEeprom, 0);

		if (dev9.eeprom == NULL)
		{
			close(hEeprom);
			dev9.eeprom = (u16*)eeprom;
		}
	}
#endif

	for (int rxbi = 0; rxbi < (SMAP_BD_SIZE / 8); rxbi++)
	{
		smap_bd_t* pbd = (smap_bd_t*)&dev9.dev9R[SMAP_BD_RX_BASE & 0xffff];
		pbd = &pbd[rxbi];

		pbd->ctrl_stat = SMAP_BD_RX_EMPTY;
		pbd->length = 0;
	}

	DevCon.WriteLn("DEV9: DEV9init ok");

	return 0;
}

void DEV9shutdown()
{
	DevCon.WriteLn("DEV9: DEV9shutdown");
	delete dev9.ata;
}

s32 DEV9open()
{
	DevCon.WriteLn("DEV9: DEV9open");

	std::string hddPath(GetHDDPath());

	if (EmuConfig.DEV9.HddEnable)
	{
		if (dev9.ata->Open(hddPath) != 0)
			EmuConfig.DEV9.HddEnable = false;
	}

	if (EmuConfig.DEV9.EthEnable)
		InitNet();

	isRunning = true;
	return 0;
}

void DEV9close()
{
	DevCon.WriteLn("DEV9: DEV9close");

	dev9.dma_iop_ptr = nullptr;
	dev9.ata->Close();
	TermNet();
	isRunning = false;
}

int DEV9irqHandler(void)
{
	//dev9Ru16(SPD_R_INTR_STAT)|= dev9.irqcause;
	//DevCon.WriteLn("DEV9: DEV9irqHandler %x, %x", dev9.irqcause, dev9.irqmask);
	if (dev9.irqcause & dev9.irqmask)
		return 1;
	return 0;
}

void _DEV9irq(int cause, int cycles)
{
	//DevCon.WriteLn("DEV9: _DEV9irq %x, %x", cause, dev9.irqmask);

	dev9.irqcause |= cause;

	if (cycles < 1)
		dev9Irq(1);
	else
		dev9Irq(cycles);
}

// SPEED <-> HDD FIFO
void HDDWriteFIFO()
{
	pxAssert(dev9.ata->dmaReady && (dev9.if_ctrl & SPD_IF_ATA_DMAEN));
	pxAssert((dev9.if_ctrl & SPD_IF_READ));

	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);
	const int space = (SPD_DBUF_AVAIL_MAX * 512 - unread);
	const int base = dev9.fifo_bytes_write % (SPD_DBUF_AVAIL_MAX * 512);

	pxAssert(unread <= SPD_DBUF_AVAIL_MAX * 512);

	int read;
	if (base + space > SPD_DBUF_AVAIL_MAX * 512)
	{
		const int was = SPD_DBUF_AVAIL_MAX * 512 - base;
		read = dev9.ata->ReadDMAToFIFO(dev9.fifo + base, was);
		if (read == was)
			read += dev9.ata->ReadDMAToFIFO(dev9.fifo, space - was);
	}
	else
	{
		read = dev9.ata->ReadDMAToFIFO(dev9.fifo + base, space);
	}

	dev9.fifo_bytes_write += read;
}
void HDDReadFIFO()
{
	pxAssert(dev9.ata->dmaReady && (dev9.if_ctrl & SPD_IF_ATA_DMAEN));
	pxAssert((dev9.if_ctrl & SPD_IF_READ) == 0);

	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);
	const int base = dev9.fifo_bytes_read % (SPD_DBUF_AVAIL_MAX * 512);

	pxAssert(unread <= SPD_DBUF_AVAIL_MAX * 512);

	int write;
	if (base + unread > SPD_DBUF_AVAIL_MAX * 512)
	{
		const int was = SPD_DBUF_AVAIL_MAX * 512 - base;
		write = dev9.ata->WriteDMAFromFIFO(dev9.fifo + base, was);
		if (write == was)
			write += dev9.ata->WriteDMAFromFIFO(dev9.fifo, unread - was);
	}
	else
	{
		write = dev9.ata->WriteDMAFromFIFO(dev9.fifo + base, unread);
	}

	dev9.fifo_bytes_read += write;
}
void IOPReadFIFO()
{
	pxAssert((dev9.dma_iop_ptr != nullptr) && (dev9.xfr_ctrl & SPD_XFR_DMAEN));
	pxAssert((dev9.xfr_ctrl & SPD_XFR_WRITE) == 0);

	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);
	const int base = dev9.fifo_bytes_read % (SPD_DBUF_AVAIL_MAX * 512);

	const int remain = dev9.dma_iop_size - dev9.dma_iop_transfered;
	const int read = std::min(remain, unread);

	pxAssert(unread <= SPD_DBUF_AVAIL_MAX * 512);

	if (read == 0)
		return;

	if (base + read > SPD_DBUF_AVAIL_MAX * 512)
	{
		const int was = SPD_DBUF_AVAIL_MAX * 512 - base;

		std::memcpy(dev9.dma_iop_ptr + dev9.dma_iop_transfered, dev9.fifo + base, was);
		std::memcpy(dev9.dma_iop_ptr + dev9.dma_iop_transfered + was, dev9.fifo, read - was);
	}
	else
	{
		std::memcpy(dev9.dma_iop_ptr + dev9.dma_iop_transfered, dev9.fifo + base, read);
	}

	dev9.dma_iop_transfered += read;
	dev9.fifo_bytes_read += read;
	if (dev9.fifo_bytes_read > dev9.fifo_bytes_write)
		Console.Error("DEV9: UNDERFLOW BY IOP");
}
void IOPWriteFIFO()
{
	pxAssert((dev9.dma_iop_ptr != nullptr) && (dev9.xfr_ctrl & SPD_XFR_DMAEN));
	pxAssert(dev9.xfr_ctrl & SPD_XFR_WRITE);

	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);
	const int space = (SPD_DBUF_AVAIL_MAX * 512 - unread);
	const int base = dev9.fifo_bytes_write % (SPD_DBUF_AVAIL_MAX * 512);

	const int remain = dev9.dma_iop_size - dev9.dma_iop_transfered;
	const int write = std::min(remain, space);

	pxAssert(unread <= SPD_DBUF_AVAIL_MAX * 512);

	if (write == 0)
		return;

	if (base + write > SPD_DBUF_AVAIL_MAX * 512)
	{
		const int was = SPD_DBUF_AVAIL_MAX * 512 - base;

		std::memcpy(dev9.fifo + base, dev9.dma_iop_ptr + dev9.dma_iop_transfered, was);
		std::memcpy(dev9.fifo + base, dev9.dma_iop_ptr + dev9.dma_iop_transfered + was, write - was);
	}
	else
	{
		std::memcpy(dev9.fifo + base, dev9.dma_iop_ptr + dev9.dma_iop_transfered, write);
	}

	dev9.dma_iop_transfered += write;
	dev9.fifo_bytes_write += write;
	if ((dev9.fifo_bytes_write - dev9.fifo_bytes_read) > SPD_DBUF_AVAIL_MAX * 512)
		Console.Error("DEV9: OVERFLOW BY IOP");
}
void FIFOIntr()
{
	//FIFO Buffer Full/Empty
	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);

	if (unread == 0)
	{
		dev9.irqcause &= ~SPD_INTR_ATA_FIFO_DATA;
		if ((dev9.irqcause & SPD_INTR_ATA_FIFO_EMPTY) == 0)
			_DEV9irq(SPD_INTR_ATA_FIFO_EMPTY, 1);
	}
	else
	{
		dev9.irqcause &= ~SPD_INTR_ATA_FIFO_EMPTY;
		if ((dev9.irqcause & SPD_INTR_ATA_FIFO_DATA) == 0)
			_DEV9irq(SPD_INTR_ATA_FIFO_DATA, 1);
	}

	if (unread == SPD_DBUF_AVAIL_MAX * 512)
	{
		if ((dev9.irqcause & SPD_INTR_ATA_FIFO_FULL) == 0)
			_DEV9irq(SPD_INTR_ATA_FIFO_FULL, 1);
	}
	else
	{
		dev9.irqcause &= ~SPD_INTR_ATA_FIFO_FULL;
	}

	// is DMA finished
	if ((dev9.dma_iop_ptr != nullptr) &&
		(dev9.dma_iop_transfered == dev9.dma_iop_size))
	{
		dev9.dma_iop_ptr = nullptr;
		psxDMA8Interrupt();
	}
}

// FIFO counters operate based on the direction set in SPD_R_XFR_CTRL
// Both might have to set to the same direction for (SPEED <-> HDD) to work
void DEV9runFIFO()
{
	const bool iopWrite = dev9.xfr_ctrl & SPD_XFR_WRITE; // IOP writes to FIFO
	const bool hddRead = dev9.if_ctrl & SPD_IF_READ; // HDD writes to FIFO

	const bool iopXfer = (dev9.dma_iop_ptr != nullptr) && (dev9.xfr_ctrl & SPD_XFR_DMAEN);
	const bool hddXfer = dev9.ata->dmaReady && (dev9.if_ctrl & SPD_IF_ATA_DMAEN);

	// Order operations based on iopWrite to ensure DMA has data/space to work with.
	if (iopWrite)
	{
		// Perform DMA from IOP.
		if (iopXfer)
			IOPWriteFIFO();

		// Drain the FIFO
		if (hddXfer && !hddRead)
		{
			HDDReadFIFO();
		}
	}
	else
	{
		// Ensure FIFO has data.
		if (hddXfer && hddRead)
		{
			HDDWriteFIFO();
		}

		if (iopXfer)
		{
			// Perform DMA to IOP.
			IOPReadFIFO();

			// Refill FIFO after DMA.
			// Need to recheck dmaReady incase prior
			// HDDWriteFIFO competed the transfer from HDD
			if (hddXfer && hddRead && dev9.ata->dmaReady)
			{
				HDDWriteFIFO();
			}
		}
	}

	FIFOIntr();
}

u16 SpeedRead(u32 addr, int width)
{
	u16 hard = 0;
	switch (addr)
	{
		case 0x10000020:
			//DevCon.WriteLn("DEV9: SPD_R_20 %dbit read %x", width, 1);
			return 1;
		case SPD_R_INTR_STAT:
			//DevCon.WriteLn("DEV9: SPD_R_INTR_STAT %dbit read %x", width, dev9.irqcause);
			return dev9.irqcause;

		case SPD_R_INTR_MASK:
			//DevCon.WriteLn("DEV9: SPD_R_INTR_MASK %dbit read %x", width, dev9.irqmask);
			return dev9.irqmask;

		case SPD_R_PIO_DATA:

			/*if(dev9.eeprom_dir!=1)
			{
				hard=0;
				break;
			}*/

			if (dev9.eeprom_state == EEPROM_TDATA)
			{
				if (dev9.eeprom_command == 2) // read
				{
					if (dev9.eeprom_bit != 0xFF)
						hard = ((dev9.eeprom[dev9.eeprom_address] << dev9.eeprom_bit) & 0x8000) >> 11;
					dev9.eeprom_bit++;
					if (dev9.eeprom_bit == 16)
					{
						dev9.eeprom_address++;
						dev9.eeprom_bit = 0;
					}
				}
			}
			//DevCon.WriteLn("DEV9: SPD_R_PIO_DATA %dbit read %x", width, hard);
			return hard;

		case SPD_R_REV_1:
			//DevCon.WriteLn("DEV9: SPD_R_REV_1 %dbit read %x", width, 0);
			return 0;

		case SPD_R_REV_2:
			hard = 0x11;
			//DevCon.WriteLn("DEV9: STD_R_REV_2 %dbit read %x", width, hard);
			return hard;

		case SPD_R_REV_3:
			// The Expansion bay always says HDD and Ethernet are supported, we need to keep HDD enabled and we handle it elsewhere.
			// Ethernet we will turn off as not sure on what that would do right now, but no known game cares if it's off.
			if (EmuConfig.DEV9.EthEnable)
				hard |= SPD_CAPS_SMAP;

			// TODO: Do we need flash? my 50003 model doesn't report this, but it does report DVR capable aka (1<<4), was that intended?
			hard |= SPD_CAPS_ATA | SPD_CAPS_FLASH;
			//DevCon.WriteLn("DEV9: SPD_R_REV_3 %dbit read %x", width, hard);
			return hard;

		case SPD_R_0e:
			hard = 0x0002; // Have HDD module inserted
			DevCon.WriteLn("DEV9: SPD_R_0e %dbit read %x", width, hard);
			return hard;
		case SPD_R_XFR_CTRL:
			DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL %dbit read %x", width, dev9.xfr_ctrl);
			return dev9.xfr_ctrl;
		case SPD_R_DBUF_STAT:
		{
			const u8 count = static_cast<u8>((dev9.fifo_bytes_write - dev9.fifo_bytes_read) / 512);
			if (dev9.xfr_ctrl & SPD_XFR_WRITE)
			{
				hard = static_cast<u8>(SPD_DBUF_AVAIL_MAX - count);
				hard |= (count == 0) ? SPD_DBUF_STAT_1 : static_cast<u16>(0);
				hard |= (count > 0) ? SPD_DBUF_STAT_2 : static_cast<u16>(0);
			}
			else
			{
				hard = count;
				hard |= (count < SPD_DBUF_AVAIL_MAX) ? SPD_DBUF_STAT_1 : static_cast<u16>(0);
				hard |= (count == 0) ? SPD_DBUF_STAT_2 : static_cast<u16>(0);
				// If overflow (HDD->SPEED), set both SPD_DBUF_STAT_2 & SPD_DBUF_STAT_FULL
				// and overflow INTR set
			}

			if (count == SPD_DBUF_AVAIL_MAX)
			{
				hard |= SPD_DBUF_STAT_FULL;
			}

			//DevCon.WriteLn("DEV9: SPD_R_DBUF_STAT %dbit read %x", width, hard);
			return hard;
		}
		case SPD_R_IF_CTRL:
			//DevCon.WriteLn("DEV9: SPD_R_IF_CTRL %dbit read %x", width,, dev9.if_ctrl);
			return dev9.if_ctrl;
		default:
			hard = dev9Ru16(addr);
			Console.Error("DEV9: Unknown %dbit read at address %lx value %x", width, addr, hard);
			return hard;
	}
}

void SpeedWrite(u32 addr, u16 value, int width)
{
	switch (addr)
	{
		case 0x10000020:
			// DevCon.WriteLn("DEV9: SPD_R_20 %dbit write %x", wisth, value);
			return;
		case SPD_R_INTR_STAT:
			Console.Error("DEV9: SPD_R_INTR_STAT %dbit write, WTF? %x", width, value);
			dev9.irqcause = value;
			return;
		case SPD_R_INTR_MASK: // 8bit writes affect whole reg
			//DevCon.WriteLn("DEV9: SPD_R_INTR_MASK %dbit write %x", checking for masked/unmasked interrupts", width, value);
			if ((dev9.irqmask != value) && ((dev9.irqmask | value) & dev9.irqcause))
			{
				//DevCon.WriteLn("DEV9: SPD_R_INTR_MASK firing unmasked interrupts");
				dev9Irq(1);
			}
			dev9.irqmask = value;
			return;

		case SPD_R_PIO_DIR:
			DevCon.WriteLn("DEV9: SPD_R_PIO_DIR %dbit write %x", width, value);

			if ((value & 0xc0) != 0xc0)
				return;

			if ((value & 0x30) == 0x20)
			{
				dev9.eeprom_state = 0;
			}
			dev9.eeprom_dir = (value >> 4) & 3;
			return;

		case SPD_R_PIO_DATA:
			//DevCon.WriteLn("DEV9: SPD_R_PIO_DATA %dbit write %x", width, value);

			if ((value & 0xc0) != 0xc0)
				return;

			switch (dev9.eeprom_state)
			{
				case EEPROM_READY:
					dev9.eeprom_command = 0;
					dev9.eeprom_state++;
					break;
				case EEPROM_OPCD0:
					dev9.eeprom_command = (value >> 4) & 2;
					dev9.eeprom_state++;
					dev9.eeprom_bit = 0xFF;
					break;
				case EEPROM_OPCD1:
					dev9.eeprom_command |= (value >> 5) & 1;
					dev9.eeprom_state++;
					break;
				case EEPROM_ADDR0:
				case EEPROM_ADDR1:
				case EEPROM_ADDR2:
				case EEPROM_ADDR3:
				case EEPROM_ADDR4:
				case EEPROM_ADDR5:
					dev9.eeprom_address =
						(dev9.eeprom_address & (63 ^ (1 << (dev9.eeprom_state - EEPROM_ADDR0)))) |
						((value >> (dev9.eeprom_state - EEPROM_ADDR0)) & (0x20 >> (dev9.eeprom_state - EEPROM_ADDR0)));
					dev9.eeprom_state++;
					break;
				case EEPROM_TDATA:
				{
					if (dev9.eeprom_command == 1) // write
					{
						dev9.eeprom[dev9.eeprom_address] =
							(dev9.eeprom[dev9.eeprom_address] & (63 ^ (1 << dev9.eeprom_bit))) |
							((value >> dev9.eeprom_bit) & (0x8000 >> dev9.eeprom_bit));
						dev9.eeprom_bit++;
						if (dev9.eeprom_bit == 16)
						{
							dev9.eeprom_address++;
							dev9.eeprom_bit = 0;
						}
					}
					break;
				}
				default:
					Console.Error("DEV9: Unknown EEPROM COMMAND");
					break;
			}
			return;

		case SPD_R_DMA_CTRL:
			//DevCon.WriteLn("DEV9: SPD_R_IF_CTRL %dbit write %x", width, value);
			dev9.dma_ctrl = value;

			//if (value & SPD_DMA_TO_SMAP)
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL DMA For SMAP");
			//else
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL DMA For ATA");

			//if ((value & SPD_DMA_FASTEST) != 0)
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL Fastest DMA Mode");
			//else
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL Slower DMA Mode");

			//if ((value & SPD_DMA_WIDE) != 0)
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL Wide(32bit) DMA Mode Set");
			//else
			//	DevCon.WriteLn("DEV9: SPD_R_DMA_CTRL 16bit DMA Mode");

			if ((value & SPD_DMA_PAUSE) != 0)
				Console.Error("DEV9: SPD_R_DMA_CTRL Pause DMA Not Implemented");

			if ((value & 0b1111111111101000) != 0)
				Console.Error("DEV9: SPD_R_DMA_CTRL Unknown value written %x", value);

			break;
		case SPD_R_XFR_CTRL:
		{
			//DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL %dbit write %x", width, value);
			const u16 oldValue = dev9.xfr_ctrl;
			dev9.xfr_ctrl = value;

			if ((value & SPD_XFR_WRITE) != (oldValue & SPD_XFR_WRITE))
				DEV9runFIFO();

			//if (value & SPD_XFR_WRITE)
			//	DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL Set Write");
			//else
			//	DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL Set Read");

			//if ((value & (1 << 1)) != 0)
			//	DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL Unknown Bit 1");

			//if ((value & (1 << 2)) != 0)
			//	DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL Unknown Bit 2");

			if (value & SPD_XFR_DMAEN)
			{
				//DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL For DMA Enabled");
				DEV9runFIFO();
			}
			//else
			//	DevCon.WriteLn("DEV9: SPD_R_XFR_CTRL For DMA Disabled");

			if ((value & 0b1111111101111000) != 0)
				Console.Error("DEV9: SPD_R_XFR_CTRL Unknown value written %x", value);

			break;
		}
		case SPD_R_DBUF_STAT:
			//DevCon.WriteLn("DEV9: SPD_R_DBUF_STAT %dbit write %x", width, value);

			if ((value & SPD_DBUF_RESET_READ_CNT) != 0)
			{
				//DevCon.WriteLn("DEV9: SPD_R_DBUF_STAT Reset read counter");
				dev9.fifo_bytes_read = 0;
			}
			if ((value & SPD_DBUF_RESET_WRITE_CNT) != 0)
			{
				//DevCon.WriteLn("DEV9: SPD_R_DBUF_STAT Reset write counter");
				dev9.fifo_bytes_write = 0;
			}

			if (value != 0)
				FIFOIntr();

			if (value != 3)
				Console.Error("DEV9: SPD_R_DBUF_STAT 16bit write %x Which != 3!!!", value);
			break;

		case SPD_R_IF_CTRL:
		{
			//DevCon.WriteLn("DEV9: SPD_R_IF_CTRL %dbit write %x", width, value);
			const u16 oldValue = dev9.if_ctrl;
			dev9.if_ctrl = value;

			//if (value & SPD_IF_UDMA)
			//	DevCon.WriteLn("DEV9: IF_CTRL UDMA Enabled");
			//else
			//	DevCon.WriteLn("DEV9: IF_CTRL UDMA Disabled");

			if ((value & SPD_IF_READ) != (oldValue & SPD_IF_READ))
				DEV9runFIFO();

			//if (value & SPD_IF_READ)
			//	DevCon.WriteLn("DEV9: IF_CTRL DMA Is ATA Read");
			//else
			//	DevCon.WriteLn("DEV9: IF_CTRL DMA Is ATA Write");

			if (value & SPD_IF_ATA_DMAEN)
			{
				//DevCon.WriteLn("DEV9: IF_CTRL ATA DMA Enabled");
				DEV9runFIFO();
			}
			//else
			//	DevCon.WriteLn("DEV9: IF_CTRL ATA DMA Disabled");

			/* During a HDD DMA transfer, the ATA regs are inacessable.
			 * The SPEED will cache register writes and wait until the end of the DMA block to write them.
			 * Bit 3 controls what happens when a read is performed while the ATA regs are inacessable.
			 * When set, the SPEED will asserts /WAIT to the IOP and wait for the HDD to end the DMA block, before reading the reg.
			 * When cleared, the read will fail if mid DMA. bit 1 in reg 0x62 indicates if the last read failed.
			 * Our DMA transfers are instant, so we can ignore this bit.
			 */
			//if (value & (1 << 3))
			//	DevCon.WriteLn("DEV9: IF_CTRL Wait for ATA register read Enabled");
			//else
			//	DevCon.WriteLn("DEV9: IF_CTRL Wait for ATA register read Disabled");

			if (value & (1 << 4))
				Console.Error("DEV9: IF_CTRL Unknown Bit 4 Set");
			if (value & (1 << 5))
				Console.Error("DEV9: IF_CTRL Unknown Bit 5 Set");

			if ((value & SPD_IF_HDD_RESET) == 0) //Maybe?
			{
				//DevCon.WriteLn("DEV9: IF_CTRL HDD Hard Reset");
				dev9.ata->ATA_HardReset();
			}
			if ((value & SPD_IF_ATA_RESET) != 0)
			{
				DevCon.WriteLn("DEV9: IF_CTRL ATA Reset");
				//0x62        0x0020
				dev9.if_ctrl = 0x001A;
				//0x66        0x0001
				dev9.pio_mode = 0x24;
				dev9.mdma_mode = 0x45;
				dev9.udma_mode = 0x83;
				//0x76    0x4ABA (And consequently 0x78 = 0x4ABA.)
			}

			if ((value & 0xFF00) > 0)
				Console.Error("DEV9: IF_CTRL Unknown Bit(s) %x", (value & 0xFF00));

			break;
		}
		case SPD_R_PIO_MODE: //ATA only? or includes EEPROM?
			//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 16bit %dbit write %x", width, value);
			dev9.pio_mode = value;

			switch (value)
			{
				case 0x92:
					//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 0");
					break;
				case 0x72:
					//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 1");
					break;
				case 0x32:
					//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 2");
					break;
				case 0x24:
					//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 3");
					break;
				case 0x23:
					//DevCon.WriteLn("DEV9: SPD_R_PIO_MODE 4");
					break;

				default:
					Console.Error("DEV9: SPD_R_PIO_MODE UNKNOWN MODE %x", value);
					break;
			}
			break;
		case SPD_R_MDMA_MODE:
			DevCon.WriteLn("DEV9: SPD_R_MDMA_MODE 16bit write %dbit write %x", width, value);
			dev9.mdma_mode = value;

			switch (value)
			{
				case 0xFF:
					DevCon.WriteLn("DEV9: SPD_R_MDMA_MODE 0");
					break;
				case 0x45:
					DevCon.WriteLn("DEV9: SPD_R_MDMA_MODE 1");
					break;
				case 0x24:
					DevCon.WriteLn("DEV9: SPD_R_MDMA_MODE 2");
					break;
				default:
					Console.Error("DEV9: SPD_R_MDMA_MODE UNKNOWN MODE %x", value);
					break;
			}

			break;
		case SPD_R_UDMA_MODE:
			DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 16bit write %dbit write %x", width, value);
			dev9.udma_mode = value;

			switch (value)
			{
				case 0xa7:
					DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 0");
					break;
				case 0x85:
					DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 1");
					break;
				case 0x63:
					DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 2");
					break;
				case 0x62:
					DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 3");
					break;
				case 0x61:
					DevCon.WriteLn("DEV9: SPD_R_UDMA_MODE 4");
					break;
				default:
					Console.Error("DEV9: SPD_R_UDMA_MODE UNKNOWN MODE %x", value);
					break;
			}
			break;

		default:
			dev9Ru8(addr) = value;
			Console.Error("DEV9: Unknown %dbit write at address %lx value %x", width, addr, value);
			return;
	}
}

u8 DEV9read8(u32 addr)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return 0;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		return dev9.ata->Read(addr, 8);
	}
	// Note, ATA regs within range of addresses used by Speed
	if (addr >= SPD_REGBASE && addr < SMAP_REGBASE)
	{
		// speed
		return SpeedRead(addr, 8);
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		// smap
		return smap_read8(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return static_cast<u8>(FLASHread32(addr, 1));
	}

	u8 hard = 0;
	switch (addr)
	{
		case DEV9_R_REV:
			hard = 0x32; // expansion bay
			//DevCon.WriteLn("DEV9: DEV9_R_REV 8bit read %x", hard);
			return hard;

		default:
			hard = dev9Ru8(addr);
			Console.Error("DEV9: Unknown 8bit read at address %lx value %x", addr, hard);
			return hard;
	}
}

u16 DEV9read16(u32 addr)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return 0;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		return dev9.ata->Read(addr, 16);
	}
	// Note, ATA regs within range of addresses used by Speed
	if (addr >= SPD_REGBASE && addr < SMAP_REGBASE)
	{
		// speed
		return SpeedRead(addr, 16);
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		// smap
		return smap_read16(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return static_cast<u16>(FLASHread32(addr, 2));
	}

	u16 hard = 0;
	switch (addr)
	{
		case DEV9_R_REV:
			//hard = 0x0030; // expansion bay
			//DevCon.WriteLn("DEV9: DEV9_R_REV 16bit read %x", dev9.irqmask);
			hard = 0x0032;
			return hard;

		default:
			hard = dev9Ru16(addr);
			Console.Error("DEV9: Unknown 16bit read at address %lx value %x", addr, hard);
			return hard;
	}
}

u32 DEV9read32(u32 addr)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return 0;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		Console.Error("DEV9: ATA does not support 32bit reads %lx", addr);
		return 0;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		return smap_read32(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return static_cast<u32>(FLASHread32(addr, 4));
	}

	const u32 hard = dev9Ru32(addr);
	Console.Error("DEV9: Unknown 32bit read at address %lx value %x", addr, hard);
	return hard;
}

void DEV9write8(u32 addr, u8 value)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		dev9.ata->Write(addr, value, 8);
		return;
	}
	// Note, ATA regs within range of addresses used by Speed
	if (addr >= SPD_REGBASE && addr < SMAP_REGBASE)
	{
		// speed
		SpeedWrite(addr, value, 8);
		return;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		// smap
		smap_write8(addr, value);
		return;
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		FLASHwrite32(addr, static_cast<u32>(value), 1);
		return;
	}

	Console.Error("DEV9: Unknown 8bit write at address %lx value %x", addr, value);
	return;
}

void DEV9write16(u32 addr, u16 value)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		dev9.ata->Write(addr, value, 16);
		return;
	}
	// Note, ATA regs within range of addresses used by Speed
	if (addr >= SPD_REGBASE && addr < SMAP_REGBASE)
	{
		// speed
		SpeedWrite(addr, value, 16);
		return;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		// smap
		smap_write16(addr, value);
		return;
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		FLASHwrite32(addr, static_cast<u32>(value), 2);
		return;
	}

	dev9Ru16(addr) = value;
	Console.Error("DEV9: *Unknown 16bit write at address %lx value %x", addr, value);
	return;
}

void DEV9write32(u32 addr, u32 value)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		ata_write<4>(addr, value);
#endif
		return;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		smap_write32(addr, value);
		return;
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		FLASHwrite32(addr, static_cast<u32>(value), 4);
		return;
	}

	switch (addr)
	{
		case SPD_R_INTR_MASK:
			Console.Error("DEV9: SPD_R_INTR_MASK, WTFH ?");
			break;
		default:
			dev9Ru32(addr) = value;
			Console.Error("DEV9: Unknown 32bit write at address %lx write %x", addr, value);
			return;
	}
}

void DEV9readDMA8Mem(u32* pMem, int size)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return;

	size >>= 1;

	DevCon.WriteLn("DEV9: *DEV9readDMA8Mem: size %x", size);

	if (dev9.dma_ctrl & SPD_DMA_TO_SMAP)
	{
		smap_readDMA8Mem(pMem, size);
		psxDMA8Interrupt();
	}
	else
	{
		if (!(dev9.xfr_ctrl & SPD_XFR_WRITE))
		{
			pxAssert(size <= SPD_DBUF_AVAIL_MAX * 512);
			dev9.dma_iop_ptr = reinterpret_cast<u8*>(pMem);
			dev9.dma_iop_size = size;
			dev9.dma_iop_transfered = 0;

			DEV9runFIFO();
		}
	}

	//TODO, track if read was successful
}

void DEV9writeDMA8Mem(u32* pMem, int size)
{
	if (!EmuConfig.DEV9.EthEnable && !EmuConfig.DEV9.HddEnable)
		return;

	size >>= 1;

	DevCon.WriteLn("DEV9: *DEV9writeDMA8Mem: size %x", size);

	if (dev9.dma_ctrl & SPD_DMA_TO_SMAP)
	{
		smap_writeDMA8Mem(pMem, size);
		psxDMA8Interrupt();
	}
	else
	{
		if (dev9.xfr_ctrl & SPD_XFR_WRITE)
		{
			pxAssert(size <= SPD_DBUF_AVAIL_MAX * 512);
			dev9.dma_iop_ptr = reinterpret_cast<u8*>(pMem);
			dev9.dma_iop_size = size;
			dev9.dma_iop_transfered = 0;

			DEV9runFIFO();
		}
	}
}

void DEV9async(u32 cycles)
{
	smap_async(cycles);
	dev9.ata->Async(cycles);
}

void DEV9CheckChanges(const Pcsx2Config& old_config)
{
	if (!isRunning)
		return;

	//Eth
	ReconfigureLiveNet(old_config);

	//Hdd
	//Hdd Validate Path
	std::string hddPath(GetHDDPath());

	//Hdd Compare with old config
	if (EmuConfig.DEV9.HddEnable)
	{
		if (old_config.DEV9.HddEnable)
		{
			//ATA::Open/Close dosn't set any regs
			//So we can close/open to apply settings
			if (EmuConfig.DEV9.HddFile != old_config.DEV9.HddFile)
			{
				dev9.ata->Close();
				if (dev9.ata->Open(hddPath) != 0)
					EmuConfig.DEV9.HddEnable = false;
			}
		}
		else if (dev9.ata->Open(hddPath) != 0)
			EmuConfig.DEV9.HddEnable = false;
	}
	else if (old_config.DEV9.HddEnable)
		dev9.ata->Close();
}
