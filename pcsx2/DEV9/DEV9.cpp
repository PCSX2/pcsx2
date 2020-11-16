/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#ifdef _WIN32
//#include <winsock2.h>
#include <Winioctl.h>
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#endif

#include "ghc/filesystem.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#define EXTERN
#include "DEV9.h"
#undef EXTERN
#include "Config.h"
#include "AppConfig.h"
#include "smap.h"


#ifdef _WIN32
#pragma warning(disable : 4244)

HINSTANCE hInst = NULL;
#endif

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

std::string s_strIniPath = "inis";
std::string s_strLogPath = "logs";
// Warning: The below log function is SLOW. Better fix it before attempting to use it.
#ifdef _DEBUG
int logFile = 1;
#else
int logFile = 0;
#endif

void __Log(int level, const char* fmt, ...)
{
	static char buffer[1024];

	if (level < DEV9_LOG_LEVEL)
		return;

	va_list list1;
	va_list list2;

	static int ticks = -1;
	int nticks = GetTickCount();

	if (ticks == -1)
		ticks = nticks;

	if (logFile)
		DEV9Log.Write("[%10d + %4d] ", nticks, nticks - ticks);
	ticks = nticks;

	if (logFile)
	{
		va_start(list1, fmt);
		//PSELog has no vargs method
		//use tmp buffer
		vsnprintf(buffer, 1024, fmt, list1);
		DEV9Log.Write(buffer);
		va_end(list1);
	}

	va_start(list2, fmt);
	emu_vprintf(fmt, list2);
	va_end(list2);
}

void LogInit()
{
	const char* logName = "dev9Log.txt";

	//GHC uses UTF8 on all platforms
	ghc::filesystem::path path(GetLogFolder().ToUTF8().data());
	path /= logName;
	std::string strPath = path.u8string();

	DEV9Log.WriteToFile = true;
	DEV9Log.Open(strPath.c_str());
}

s32 DEV9init()
{
	LogInit();
	DEV9_LOG("DEV9init\n");

	memset(&dev9, 0, sizeof(dev9));
	dev9.ata = new ATA();
	DEV9_LOG("DEV9init2\n");

	DEV9_LOG("DEV9init3\n");

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

	int rxbi;

	for (rxbi = 0; rxbi < (SMAP_BD_SIZE / 8); rxbi++)
	{
		smap_bd_t* pbd = (smap_bd_t*)&dev9.dev9R[SMAP_BD_RX_BASE & 0xffff];
		pbd = &pbd[rxbi];

		pbd->ctrl_stat = SMAP_BD_RX_EMPTY;
		pbd->length = 0;
	}

	DEV9_LOG("DEV9init ok\n");

	return 0;
}

void DEV9shutdown()
{
	DEV9_LOG("DEV9shutdown\n");
	delete dev9.ata;
	if (logFile)
		DEV9Log.Close();
}

s32 DEV9open(void* pDsp)
{
	DEV9_LOG("DEV9open\n");
	LoadConf();
#ifdef _WIN32
	//Convert to utf8
	char mbHdd[sizeof(config.Hdd)] = {0};
	WideCharToMultiByte(CP_UTF8, 0, config.Hdd, -1, mbHdd, sizeof(mbHdd) - 1, nullptr, nullptr);
	DEV9_LOG("open r+: %s\n", mbHdd);
#else
	DEV9_LOG("open r+: %s\n", config.Hdd);
#endif

#ifdef _WIN32
	ghc::filesystem::path hddPath(std::wstring(config.Hdd));
#else
	ghc::filesystem::path hddPath(config.Hdd);
#endif

	if (hddPath.empty())
		config.hddEnable = false;

	if (hddPath.is_relative())
	{
		//GHC uses UTF8 on all platforms
		ghc::filesystem::path path(GetSettingsFolder().ToUTF8().data());
		hddPath = path / hddPath;
	}
	
	if (config.hddEnable)
	{
		if (dev9.ata->Open(hddPath) != 0)
			config.hddEnable = false;
	}

	return _DEV9open();
}

void DEV9close()
{
	DEV9_LOG("DEV9close\n");

	dev9.ata->Close();
	_DEV9close();
}

int DEV9irqHandler(void)
{
	//dev9Ru16(SPD_R_INTR_STAT)|= dev9.irqcause;
	DEV9_LOG("_DEV9irqHandler %x, %x\n", dev9.irqcause, dev9.irqmask);
	if (dev9.irqcause & dev9.irqmask)
		return 1;
	return 0;
}

void _DEV9irq(int cause, int cycles)
{
	DEV9_LOG("_DEV9irq %x, %x\n", cause, dev9.irqmask);

	dev9.irqcause |= cause;

	if (cycles < 1)
		dev9Irq(1);
	else
		dev9Irq(cycles);
}

//Fakes SPEED FIFO
void HDDWriteFIFO()
{
	if (dev9.ata->dmaReady && (dev9.if_ctrl & SPD_IF_ATA_DMAEN))
	{
		const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);
		const int spaceSectors = (SPD_DBUF_AVAIL_MAX * 512 - unread) / 512;
		if (spaceSectors < 0)
		{
			DEV9_LOG_ERROR("No Space on SPEED FIFO");
			pxAssert(false);
			abort();
		}

		const int readSectors = dev9.ata->nsectorLeft < spaceSectors ? dev9.ata->nsectorLeft : spaceSectors;
		dev9.fifo_bytes_write += readSectors * 512;
		dev9.ata->nsectorLeft -= readSectors;
	}
	//FIFOIntr();
}
void HDDReadFIFO()
{
	if (dev9.ata->dmaReady && (dev9.if_ctrl & SPD_IF_ATA_DMAEN))
	{
		const int writeSectors = (dev9.fifo_bytes_write - dev9.fifo_bytes_read) / 512;
		dev9.fifo_bytes_read += writeSectors * 512;
		dev9.ata->nsectorLeft -= writeSectors;
	}
	//FIFOIntr();
}
void IOPReadFIFO(int bytes)
{
	dev9.fifo_bytes_read += bytes;
	if (dev9.fifo_bytes_read > dev9.fifo_bytes_write)
		DEV9_LOG_ERROR("UNDERFLOW BY IOP\n");
	//FIFOIntr();
}
void IOPWriteFIFO(int bytes)
{
	dev9.fifo_bytes_write += bytes;
	if (dev9.fifo_bytes_write - SPD_DBUF_AVAIL_MAX * 512 > dev9.fifo_bytes_read)
		DEV9_LOG_ERROR("OVERFLOW BY IOP\n");
	//FIFOIntr();
}
void FIFOIntr()
{
	//FIFO Buffer Full/Empty
	const int unread = (dev9.fifo_bytes_write - dev9.fifo_bytes_read);

	if (unread == 0)
	{
		if ((dev9.irqcause & SPD_INTR_ATA_FIFO_EMPTY) == 0)
			_DEV9irq(SPD_INTR_ATA_FIFO_EMPTY, 1);
	}
	if (unread == SPD_DBUF_AVAIL_MAX * 512)
	{
		//Log_Error("FIFO Full");
		//INTR Full?
	}
}

u8 DEV9read8(u32 addr)
{
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u8 hard;
	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		DEV9_LOG_ERROR("ATA does not support 8bit reads %lx\n", addr);
		return 0;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		return smap_read8(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return (u8)FLASHread32(addr, 1);
	}

	switch (addr)
	{
		case SPD_R_PIO_DATA:

			/*if(dev9.eeprom_dir!=1)
			{
				hard=0;
				break;
			}*/

			if (dev9.eeprom_state == EEPROM_TDATA)
			{
				if (dev9.eeprom_command == 2) //read
				{
					if (dev9.eeprom_bit == 0xFF)
						hard = 0;
					else
						hard = ((dev9.eeprom[dev9.eeprom_address] << dev9.eeprom_bit) & 0x8000) >> 11;
					dev9.eeprom_bit++;
					if (dev9.eeprom_bit == 16)
					{
						dev9.eeprom_address++;
						dev9.eeprom_bit = 0;
					}
				}
				else
					hard = 0;
			}
			else
				hard = 0;
			DEV9_LOG_VERB("SPD_R_PIO_DATA 8bit read %x\n", hard);
			return hard;

		case DEV9_R_REV:
			hard = 0x32; // expansion bay
			DEV9_LOG_VERB("DEV9_R_REV 8bit read %x\n", hard);
			return hard;

		default:
			hard = dev9Ru8(addr);
			DEV9_LOG_ERROR("*Unknown 8bit read at address %lx value %x\n", addr, hard);
			return hard;
	}
}

u16 DEV9read16(u32 addr)
{
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u16 hard;
	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		return dev9.ata->Read16(addr);
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		return smap_read16(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return (u16)FLASHread32(addr, 2);
	}

	switch (addr)
	{
		case SPD_R_INTR_STAT:
			DEV9_LOG_VERB("SPD_R_INTR_STAT 16bit read %x\n", dev9.irqcause);
			return dev9.irqcause;

		case SPD_R_INTR_MASK:
			DEV9_LOG("SPD_R_INTR_MASK 16bit read %x\n", dev9.irqmask);
			return dev9.irqmask;

		case SPD_R_PIO_DATA:

			/*if(dev9.eeprom_dir!=1)
			{
				hard=0;
				break;
			}*/

			if (dev9.eeprom_state == EEPROM_TDATA)
			{
				if (dev9.eeprom_command == 2) //read
				{
					if (dev9.eeprom_bit == 0xFF)
						hard = 0;
					else
						hard = ((dev9.eeprom[dev9.eeprom_address] << dev9.eeprom_bit) & 0x8000) >> 11;
					dev9.eeprom_bit++;
					if (dev9.eeprom_bit == 16)
					{
						dev9.eeprom_address++;
						dev9.eeprom_bit = 0;
					}
				}
				else
					hard = 0;
			}
			else
				hard = 0;
			DEV9_LOG_VERB("SPD_R_PIO_DATA 16bit read %x\n", hard);
			return hard;

		case DEV9_R_REV:
			//hard = 0x0030; // expansion bay
			DEV9_LOG_VERB("DEV9_R_REV 16bit read %x\n", dev9.irqmask);
			hard = 0x0032;
			return hard;

		case SPD_R_REV_1:
			DEV9_LOG_VERB("SPD_R_REV_1 16bit read %x\n", 0);
			return 0;

		case SPD_R_REV_2:
			hard = 0x0011;
			DEV9_LOG_VERB("STD_R_REV_1 16bit read %x\n", hard);
			return hard;

		case SPD_R_REV_3:
			hard = 0;
			if (config.hddEnable)
				hard |= SPD_CAPS_ATA;
			if (config.ethEnable)
				hard |= SPD_CAPS_SMAP;
			hard |= SPD_CAPS_FLASH;
			DEV9_LOG_VERB("SPD_R_REV_3 16bit read %x\n", hard);
			return hard;

		case SPD_R_0e:
			hard = 0x0002; //Have HDD inserted
			DEV9_LOG_VERB("SPD_R_0e 16bit read %x\n", hard);
			return hard;
		case SPD_R_XFR_CTRL:
			DEV9_LOG_VERB("SPD_R_XFR_CTRL 16bit read %x\n", dev9.xfr_ctrl);
			return dev9.xfr_ctrl;
		case SPD_R_DBUF_STAT:
		{
			hard = 0;
			if (dev9.if_ctrl & SPD_IF_READ) //Semi async
			{
				HDDWriteFIFO(); //Yes this is not a typo
			}
			else
			{
				HDDReadFIFO();
			}
			FIFOIntr();

			const u8 count = (u8)((dev9.fifo_bytes_write - dev9.fifo_bytes_read) / 512);
			if (dev9.xfr_ctrl & SPD_XFR_WRITE) //or ifRead?
			{
				hard = (u8)(SPD_DBUF_AVAIL_MAX - count);
				hard |= (count == 0) ? SPD_DBUF_STAT_1 : (u16)0;
				hard |= (count > 0) ? SPD_DBUF_STAT_2 : (u16)0;
			}
			else
			{
				hard = count;
				hard |= (count < SPD_DBUF_AVAIL_MAX) ? SPD_DBUF_STAT_1 : (u16)0;
				hard |= (count == 0) ? SPD_DBUF_STAT_2 : (u16)0;
				//If overflow (HDD->SPEED), set both SPD_DBUF_STAT_2 & SPD_DBUF_STAT_FULL
				//and overflow INTR set
			}

			if (count == SPD_DBUF_AVAIL_MAX)
			{
				hard |= SPD_DBUF_STAT_FULL;
			}

			DEV9_LOG_VERB("SPD_R_DBUF_STAT 16bit read %x\n", hard);
			return hard;
		}
		case SPD_R_IF_CTRL:
			DEV9_LOG_VERB("SPD_R_IF_CTRL 16bit read %x\n", dev9.if_ctrl);
			return dev9.if_ctrl;
		default:
			hard = dev9Ru16(addr);
			DEV9_LOG_ERROR("*Unknown 16bit read at address %lx value %x\n", addr, hard);
			return hard;
	}
}

u32 DEV9read32(u32 addr)
{
	if (!config.ethEnable & !config.hddEnable)
		return 0;

	u32 hard;
	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		DEV9_LOG_ERROR("ATA does not support 32bit reads %lx\n", addr);
		return 0;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		return smap_read32(addr);
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		return (u32)FLASHread32(addr, 4);
	}

	hard = dev9Ru32(addr);
	DEV9_LOG_ERROR("*Unknown 32bit read at address %lx value %x\n", addr, hard);
	return hard;
}

void DEV9write8(u32 addr, u8 value)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
#ifdef ENABLE_ATA
		ata_write<1>(addr, value);
#endif
		return;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		smap_write8(addr, value);
		return;
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		FLASHwrite32(addr, (u32)value, 1);
		return;
	}

	switch (addr)
	{
		case 0x10000020:
			DEV9_LOG_ERROR("SPD_R_INTR_CAUSE, WTFH ?\n");
			dev9.irqcause = 0xff;
			break;
		case SPD_R_INTR_STAT:
			DEV9_LOG_ERROR("SPD_R_INTR_STAT,  WTFH ?\n");
			dev9.irqcause = value;
			return;
		case SPD_R_INTR_MASK:
			DEV9_LOG_ERROR("SPD_R_INTR_MASK8, WTFH ?\n");
			break;

		case SPD_R_PIO_DIR:
			DEV9_LOG_VERB("SPD_R_PIO_DIR 8bit write %x\n", value);

			if ((value & 0xc0) != 0xc0)
				return;

			if ((value & 0x30) == 0x20)
			{
				dev9.eeprom_state = 0;
			}
			dev9.eeprom_dir = (value >> 4) & 3;

			return;

		case SPD_R_PIO_DATA:
			DEV9_LOG_VERB("SPD_R_PIO_DATA 8bit write %x\n", value);

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
					if (dev9.eeprom_command == 1) //write
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
				}
				break;
				default:
					DEV9_LOG_ERROR("Unkown EEPROM COMMAND\n");
					break;
			}
			return;
		default:
			dev9Ru8(addr) = value;
			DEV9_LOG_ERROR("*Unknown 8bit write at address %lx value %x\n", addr, value);
			return;
	}
}

void DEV9write16(u32 addr, u16 value)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	if (addr >= ATA_DEV9_HDD_BASE && addr < ATA_DEV9_HDD_END)
	{
		dev9.ata->Write16(addr, value);
		return;
	}
	if (addr >= SMAP_REGBASE && addr < FLASH_REGBASE)
	{
		//smap
		smap_write16(addr, value);
		return;
	}
	if ((addr >= FLASH_REGBASE) && (addr < (FLASH_REGBASE + FLASH_REGSIZE)))
	{
		FLASHwrite32(addr, (u32)value, 2);
		return;
	}

	switch (addr)
	{
		case SPD_R_INTR_MASK:
			DEV9_LOG_VERB("SPD_R_INTR_MASK 16bit write %x	, checking for masked/unmasked interrupts\n", value);
			if ((dev9.irqmask != value) && ((dev9.irqmask | value) & dev9.irqcause))
			{
				DEV9_LOG_VERB("SPD_R_INTR_MASK16 firing unmasked interrupts\n");
				dev9Irq(1);
			}
			dev9.irqmask = value;
			break;

		case SPD_R_PIO_DIR:
			DEV9_LOG_VERB("SPD_R_PIO_DIR 16bit write %x\n", value);

			if ((value & 0xc0) != 0xc0)
				return;

			if ((value & 0x30) == 0x20)
			{
				dev9.eeprom_state = 0;
			}
			dev9.eeprom_dir = (value >> 4) & 3;

			return;

		case SPD_R_PIO_DATA:
			DEV9_LOG_VERB("SPD_R_PIO_DATA 16bit write %x\n", value);

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
					if (dev9.eeprom_command == 1) //write
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
				}
				break;
				default:
					DEV9_LOG_ERROR("Unkown EEPROM COMMAND\n");
					break;
			}
			return;

		case SPD_R_DMA_CTRL:
			DEV9_LOG_VERB("SPD_R_IF_CTRL 16bit write %x\n", value);
			dev9.dma_ctrl = value;

			if (value & SPD_DMA_TO_SMAP)
				DEV9_LOG_VERB("SPD_R_DMA_CTRL DMA For SMAP\n");
			else
				DEV9_LOG_VERB("SPD_R_DMA_CTRL DMA For ATA\n");

			if ((value & SPD_DMA_FASTEST) != 0)
				DEV9_LOG_VERB("SPD_R_DMA_CTRL Fastest DMA Mode\n");
			else
				DEV9_LOG_VERB("SPD_R_DMA_CTRL Slower DMA Mode\n");

			if ((value & SPD_DMA_WIDE) != 0)
				DEV9_LOG_VERB("SPD_R_DMA_CTRL Wide(32bit) DMA Mode Set\n");
			else
				DEV9_LOG_VERB("SPD_R_DMA_CTRL 16bit DMA Mode\n");

			if ((value & SPD_DMA_PAUSE) != 0)
				DEV9_LOG_ERROR("SPD_R_DMA_CTRL Pause DMA\n");

			if ((value & 0b1111111111101000) != 0)
				DEV9_LOG_ERROR("SPD_R_DMA_CTRL Unkown value written %x\n", value);

			break;
		case SPD_R_XFR_CTRL:
			DEV9_LOG_VERB("SPD_R_IF_CTRL 16bit write %x\n", value);
			dev9.xfr_ctrl = value;

			if (value & SPD_XFR_WRITE)
				DEV9_LOG_VERB("SPD_R_XFR_CTRL Set Write\n");
			else
				DEV9_LOG_VERB("SPD_R_XFR_CTRL Set Read\n");

			if ((value & (1 << 1)) != 0)
				DEV9_LOG_VERB("SPD_R_XFR_CTRL Unkown Bit 1\n");

			if ((value & (1 << 2)) != 0)
				DEV9_LOG_VERB("SPD_R_XFR_CTRL Unkown Bit 2\n");

			if (value & SPD_XFR_DMAEN)
				DEV9_LOG_VERB("SPD_R_XFR_CTRL For DMA Enabled\n");
			else
				DEV9_LOG_VERB("SPD_R_XFR_CTRL For DMA Disabled\n");

			if ((value & 0b1111111101111000) != 0)
			{
				DEV9_LOG_ERROR("SPD_R_XFR_CTRL Unkown value written %x\n", value);
			}

			break;
		case SPD_R_DBUF_STAT:
			DEV9_LOG_VERB("SPD_R_DBUF_STAT 16bit write %x\n", value);

			if ((value & SPD_DBUF_RESET_FIFO) != 0)
			{
				DEV9_LOG_VERB("SPD_R_XFR_CTRL Reset FIFO\n");
				dev9.fifo_bytes_write = 0;
				dev9.fifo_bytes_read = 0;
				dev9.xfr_ctrl &= ~SPD_XFR_WRITE; //?
				dev9.if_ctrl |= SPD_IF_READ; //?

				FIFOIntr();
			}

			if (value != 3)
				DEV9_LOG_ERROR("SPD_R_38 16bit write %x Which != 3!!!", value);
			break;

		case SPD_R_IF_CTRL:
			DEV9_LOG_VERB("SPD_R_IF_CTRL 16bit write %x\n", value);
			dev9.if_ctrl = value;

			if (value & SPD_IF_UDMA)
				DEV9_LOG_VERB("IF_CTRL UDMA Enabled\n");
			else
				DEV9_LOG_VERB("IF_CTRL UDMA Disabled\n");
			if (value & SPD_IF_READ)
				DEV9_LOG_VERB("IF_CTRL DMA Is ATA Read\n");
			else
				DEV9_LOG_VERB("IF_CTRL DMA Is ATA Write\n");

			if (value & SPD_IF_ATA_DMAEN)
			{
				DEV9_LOG_VERB("IF_CTRL ATA DMA Enabled\n");
				if (value & SPD_IF_READ) //Semi async
				{
					HDDWriteFIFO(); //Yes this is not a typo
				}
				else
				{
					HDDReadFIFO();
				}
				FIFOIntr();
			}
			else
				DEV9_LOG_VERB("IF_CTRL ATA DMA Disabled\n");

			if (value & (1 << 3))
				DEV9_LOG_VERB("IF_CTRL Unkown Bit 3 Set\n");

			if (value & (1 << 4))
				DEV9_LOG_ERROR("IF_CTRL Unkown Bit 4 Set\n");
			if (value & (1 << 5))
				DEV9_LOG_ERROR("IF_CTRL Unkown Bit 5 Set\n");

			if ((value & SPD_IF_HDD_RESET) == 0) //Maybe?
			{
				DEV9_LOG_INFO("IF_CTRL HDD Hard Reset\n");
				dev9.ata->ATA_HardReset();
			}
			if ((value & SPD_IF_ATA_RESET) != 0)
			{
				DEV9_LOG_INFO("IF_CTRL ATA Reset\n");
				//0x62        0x0020
				dev9.if_ctrl = 0x001A;
				//0x66        0x0001
				dev9.pio_mode = 0x24;
				dev9.mdma_mode = 0x45;
				dev9.udma_mode = 0x83;
				//0x76    0x4ABA (And consequently 0x78 = 0x4ABA.)
			}

			if ((value & 0xFF00) > 0)
				DEV9_LOG_ERROR("IF_CTRL Unkown Bit(s) %x\n", (value & 0xFF00));

			break;
		case SPD_R_PIO_MODE: //ATA only? or includes EEPROM?
			DEV9_LOG_VERB("SPD_R_PIO_MODE 16bit write %x\n", value);
			dev9.pio_mode = value;

			switch (value)
			{
				case 0x92:
					DEV9_LOG_INFO("SPD_R_PIO_MODE 0\n");
					break;
				case 0x72:
					DEV9_LOG_INFO("SPD_R_PIO_MODE 1\n");
					break;
				case 0x32:
					DEV9_LOG_INFO("SPD_R_PIO_MODE 2\v");
					break;
				case 0x24:
					DEV9_LOG_INFO("SPD_R_PIO_MODE 3\n");
					break;
				case 0x23:
					DEV9_LOG_INFO("SPD_R_PIO_MODE 4\n");
					break;

				default:
					DEV9_LOG_ERROR("SPD_R_PIO_MODE UNKOWN MODE %x\n", value);
					break;
			}
			break;
		case SPD_R_MDMA_MODE: //ATA only? or includes EEPROM?
			DEV9_LOG_VERB("SPD_R_MDMA_MODE 16bit write %x\n", value);
			dev9.mdma_mode = value;

			switch (value)
			{
				case 0xFF:
					DEV9_LOG_INFO("SPD_R_MDMA_MODE 0\n");
					break;
				case 0x45:
					DEV9_LOG_INFO("SPD_R_MDMA_MODE 1\n");
					break;
				case 0x24:
					DEV9_LOG_INFO("SPD_R_MDMA_MODE 2\n");
					break;
				default:
					DEV9_LOG_ERROR("SPD_R_MDMA_MODE UNKOWN MODE %x\n", value);
					break;
			}

			break;
		case SPD_R_UDMA_MODE: //ATA only?
			DEV9_LOG_VERB("SPD_R_UDMA_MODE 16bit write %x\n", value);
			dev9.udma_mode = value;

			switch (value)
			{
				case 0xa7:
					DEV9_LOG_VERB("SPD_R_UDMA_MODE 0\n");
					break;
				case 0x85:
					DEV9_LOG_VERB("SPD_R_UDMA_MODE 1\n");
					break;
				case 0x63:
					DEV9_LOG_VERB("SPD_R_UDMA_MODE 2\n");
					break;
				case 0x62:
					DEV9_LOG_VERB("SPD_R_UDMA_MODE 3\n");
					break;
				case 0x61:
					DEV9_LOG_VERB("SPD_R_UDMA_MODE 4\n");
					break;
				default:
					DEV9_LOG_ERROR("SPD_R_UDMA_MODE UNKOWN MODE %x\n", value);
					break;
			}
			break;

		default:
			dev9Ru16(addr) = value;
			DEV9_LOG_ERROR("*Unknown 16bit write at address %lx value %x\n", addr, value);
			return;
	}
}

void DEV9write32(u32 addr, u32 value)
{
	if (!config.ethEnable & !config.hddEnable)
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
		FLASHwrite32(addr, (u32)value, 4);
		return;
	}

	switch (addr)
	{
		case SPD_R_INTR_MASK:
			DEV9_LOG_ERROR("SPD_R_INTR_MASK	, WTFH ?\n");
			break;
		default:
			dev9Ru32(addr) = value;
			DEV9_LOG_ERROR("*Unknown 32bit write at address %lx write %x\n", addr, value);
			return;
	}
}

void DEV9readDMA8Mem(u32* pMem, int size)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	size >>= 1;

	DEV9_LOG_VERB("*DEV9readDMA8Mem: size %x\n", size);
	DEV9_LOG_INFO("rDMA\n");

	if (dev9.dma_ctrl & SPD_DMA_TO_SMAP)
		smap_readDMA8Mem(pMem, size);
	else
	{
		if (dev9.xfr_ctrl & SPD_XFR_DMAEN &&
			!(dev9.xfr_ctrl & SPD_XFR_WRITE))
		{
			HDDWriteFIFO();
			IOPReadFIFO(size);
			dev9.ata->ATAreadDMA8Mem((u8*)pMem, size);
			FIFOIntr();
		}
	}

	//TODO, track if read was successful
}

void DEV9writeDMA8Mem(u32* pMem, int size)
{
	if (!config.ethEnable & !config.hddEnable)
		return;

	size >>= 1;

	DEV9_LOG_VERB("*DEV9writeDMA8Mem: size %x\n", size);
	DEV9_LOG_INFO("wDMA\n");

	if (dev9.dma_ctrl & SPD_DMA_TO_SMAP)
		smap_writeDMA8Mem(pMem, size);
	else
	{
		if (dev9.xfr_ctrl & SPD_XFR_DMAEN &&
			dev9.xfr_ctrl & SPD_XFR_WRITE)
		{
			IOPWriteFIFO(size);
			HDDReadFIFO();
			dev9.ata->ATAwriteDMA8Mem((u8*)pMem, size);
			FIFOIntr();
		}
	}

	//TODO, track if write was successful
}

void DEV9async(u32 cycles)
{
	smap_async(cycles);
	dev9.ata->Async(cycles);
}

// extended funcs

void DEV9setSettingsDir(const char* dir)
{
	// Grab the ini directory.
	// TODO: Use
	s_strIniPath = (dir == NULL) ? "inis" : dir;
}

void DEV9setLogDir(const char* dir)
{
	// Get the path to the log directory.
	s_strLogPath = (dir == NULL) ? "logs" : dir;

	// Reload the log file after updated the path
	// Currently dosn't change winPcap log directories post DEV9open()
	DEV9Log.Close();
	LogInit();
}

int emu_printf(const char* fmt, ...)
{
	va_list vl;
	int ret;
	va_start(vl, fmt);
	ret = vfprintf(stderr, fmt, vl);
	va_end(vl);
	fflush(stderr);
	return ret;
}

int emu_vprintf(const char* fmt, va_list vl)
{
	int ret;
	ret = vfprintf(stderr, fmt, vl);
	fflush(stderr);
	return ret;
}
