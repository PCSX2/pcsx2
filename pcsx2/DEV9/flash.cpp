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

// The code has been designed for 64Mb flash and uses as file support the second memory card
#include <stdio.h>
//#include <winsock2.h>
#include "DEV9.h"

#define PAGE_SIZE_BITS 9
#define PAGE_SIZE (1 << PAGE_SIZE_BITS)
#define ECC_SIZE (16)
#define PAGE_SIZE_ECC (PAGE_SIZE + ECC_SIZE)
#define BLOCK_SIZE (16 * PAGE_SIZE)
#define BLOCK_SIZE_ECC (16 * PAGE_SIZE_ECC)
#define CARD_SIZE (1024 * BLOCK_SIZE)
#define CARD_SIZE_ECC (1024 * BLOCK_SIZE_ECC)


static volatile u32 ctrl, cmd = (u32)-1, address, id, counter, addrbyte;
static u8 data[PAGE_SIZE_ECC], file[CARD_SIZE_ECC];

static void xfromman_call20_calculateXors(unsigned char buffer[128], unsigned char blah[4]);

static void calculateECC(u8 page[PAGE_SIZE_ECC])
{
	memset(page + PAGE_SIZE, 0x00, ECC_SIZE);
	xfromman_call20_calculateXors(page + 0 * (PAGE_SIZE >> 2), page + PAGE_SIZE + 0 * 3); //(ECC_SIZE>>2));
	xfromman_call20_calculateXors(page + 1 * (PAGE_SIZE >> 2), page + PAGE_SIZE + 1 * 3); //(ECC_SIZE>>2));
	xfromman_call20_calculateXors(page + 2 * (PAGE_SIZE >> 2), page + PAGE_SIZE + 2 * 3); //(ECC_SIZE>>2));
	xfromman_call20_calculateXors(page + 3 * (PAGE_SIZE >> 2), page + PAGE_SIZE + 3 * 3); //(ECC_SIZE>>2));
}

static const char* getCmdName(u32 cmd)
{
	switch (cmd)
	{
		case SM_CMD_READ1:
			return "READ1";
		case SM_CMD_READ2:
			return "READ2";
		case SM_CMD_READ3:
			return "READ3";
		case SM_CMD_RESET:
			return "RESET";
		case SM_CMD_WRITEDATA:
			return "WRITEDATA";
		case SM_CMD_PROGRAMPAGE:
			return "PROGRAMPAGE";
		case SM_CMD_ERASEBLOCK:
			return "ERASEBLOCK";
		case SM_CMD_ERASECONFIRM:
			return "ERASECONFIRM";
		case SM_CMD_GETSTATUS:
			return "GETSTATUS";
		case SM_CMD_READID:
			return "READID";
		default:
			return "unknown";
	}
}

void FLASHinit()
{
	FILE* fd;

	id = FLASH_ID_64MBIT;
	counter = 0;
	addrbyte = 0;

	address = 0;
	memset(data, 0xFF, PAGE_SIZE);
	calculateECC(data);
	ctrl = FLASH_PP_READY;

	fd = fopen("flash.dat", "rb");
	if (fd != NULL)
	{
		size_t ret;

		ret = fread(file, 1, CARD_SIZE_ECC, fd);
		if (ret != CARD_SIZE_ECC)
		{
			DevCon.WriteLn("DEV9: Reading error.");
		}

		fclose(fd);
	}
	else
		memset(file, 0xFF, CARD_SIZE_ECC);
}

u32 FLASHread32(u32 addr, int size)
{
	u32 value, refill = 0;

	switch (addr)
	{
		case FLASH_R_DATA:
			memcpy(&value, &data[counter], size);
			counter += size;
			DevCon.WriteLn("DEV9: *FLASH DATA %dbit read 0x%08lX %s", size * 8, value, (ctrl & FLASH_PP_READ) ? "READ_ENABLE" : "READ_DISABLE");
			if (cmd == SM_CMD_READ3)
			{
				if (counter >= PAGE_SIZE_ECC)
				{
					counter = PAGE_SIZE;
					refill = 1;
				}
			}
			else
			{
				if ((ctrl & FLASH_PP_NOECC) && (counter >= PAGE_SIZE))
				{
					counter %= PAGE_SIZE;
					refill = 1;
				}
				else if (!(ctrl & FLASH_PP_NOECC) && (counter >= PAGE_SIZE_ECC))
				{
					counter %= PAGE_SIZE_ECC;
					refill = 1;
				}
			}

			if (refill)
			{
				ctrl &= ~FLASH_PP_READY;
				address += PAGE_SIZE;
				address %= CARD_SIZE;
				memcpy(data, file + (address >> PAGE_SIZE_BITS) * PAGE_SIZE_ECC, PAGE_SIZE);
				calculateECC(data); // calculate ECC; should be in the file already
				ctrl |= FLASH_PP_READY;
			}

			return value;

		case FLASH_R_CMD:
			DevCon.WriteLn("DEV9: *FLASH CMD %dbit read %s DENIED", size * 8, getCmdName(cmd));
			return cmd;

		case FLASH_R_ADDR:
			DevCon.WriteLn("DEV9: *FLASH ADDR %dbit read DENIED", size * 8);
			return 0;

		case FLASH_R_CTRL:
			DevCon.WriteLn("DEV9: *FLASH CTRL %dbit read 0x%08lX", size * 8, ctrl);
			return ctrl;

		case FLASH_R_ID:
			if (cmd == SM_CMD_READID)
			{
				DevCon.WriteLn("DEV9: *FLASH ID %dbit read 0x%08lX", size * 8, id);
				return id; //0x98=Toshiba/0xEC=Samsung maker code should be returned first
			}
			else if (cmd == SM_CMD_GETSTATUS)
			{
				value = 0x80 | ((ctrl & 1) << 6); // 0:0=pass, 6:ready/busy, 7:1=not protected
				DevCon.WriteLn("DEV9: *FLASH STATUS %dbit read 0x%08lX", size * 8, value);
				return value;
			} //else fall off
			return 0;

		default:
			DevCon.WriteLn("DEV9: *FLASH Unknown %dbit read at address %lx", size * 8, addr);
			return 0;
	}
}

void FLASHwrite32(u32 addr, u32 value, int size)
{

	switch (addr & 0x1FFFFFFF)
	{
		case FLASH_R_DATA:

			DevCon.WriteLn("DEV9: *FLASH DATA %dbit write 0x%08lX %s", size * 8, value, (ctrl & FLASH_PP_WRITE) ? "WRITE_ENABLE" : "WRITE_DISABLE");
			memcpy(&data[counter], &value, size);
			counter += size;
			counter %= PAGE_SIZE_ECC; //should not get past the last byte, but at the end
			break;

		case FLASH_R_CMD:
			if (!(ctrl & FLASH_PP_READY))
			{
				if ((value != SM_CMD_GETSTATUS) && (value != SM_CMD_RESET))
				{
					DevCon.WriteLn("DEV9: *FLASH CMD %dbit write %s ILLEGAL in busy mode - IGNORED", size * 8, getCmdName(value));
					break;
				}
			}
			if (cmd == SM_CMD_WRITEDATA)
			{
				if ((value != SM_CMD_PROGRAMPAGE) && (value != SM_CMD_RESET))
				{
					DevCon.WriteLn("DEV9: *FLASH CMD %dbit write %s ILLEGAL after WRITEDATA cmd - IGNORED", size * 8, getCmdName(value));
					ctrl &= ~FLASH_PP_READY; //go busy, reset is needed
					break;
				}
			}
			DevCon.WriteLn("DEV9: *FLASH CMD %dbit write %s", size * 8, getCmdName(value));
			switch (value)
			{ // A8 bit is encoded in READ cmd;)
				case SM_CMD_READ1:
					counter = 0;
					if (cmd != SM_CMD_GETSTATUS)
						address = counter;
					addrbyte = 0;
					break;
				case SM_CMD_READ2:
					counter = PAGE_SIZE / 2;
					if (cmd != SM_CMD_GETSTATUS)
						address = counter;
					addrbyte = 0;
					break;
				case SM_CMD_READ3:
					counter = PAGE_SIZE;
					if (cmd != SM_CMD_GETSTATUS)
						address = counter;
					addrbyte = 0;
					break;
				case SM_CMD_RESET:
					FLASHinit();
					break;
				case SM_CMD_WRITEDATA:
					counter = 0;
					address = counter;
					addrbyte = 0;
					break;
				case SM_CMD_ERASEBLOCK:
					counter = 0;
					memset(data, 0xFF, PAGE_SIZE);
					address = counter;
					addrbyte = 1;
					break;
				case SM_CMD_PROGRAMPAGE: //fall
				case SM_CMD_ERASECONFIRM:
					ctrl &= ~FLASH_PP_READY;
					calculateECC(data);
					memcpy(file + (address / PAGE_SIZE) * PAGE_SIZE_ECC, data, PAGE_SIZE_ECC);
					/*write2file*/
					ctrl |= FLASH_PP_READY;
					break;
				case SM_CMD_GETSTATUS:
					break;
				case SM_CMD_READID:
					counter = 0;
					address = counter;
					addrbyte = 0;
					break;
				default:
					ctrl &= ~FLASH_PP_READY;
					return; //ignore any other command; go busy, reset is needed
			}
			cmd = value;
			break;

		case FLASH_R_ADDR:
			DevCon.WriteLn("DEV9: *FLASH ADDR %dbit write 0x%08lX", size * 8, value);
			address |= (value & 0xFF) << (addrbyte == 0 ? 0 : (1 + 8 * addrbyte));
			addrbyte++;
			DevCon.WriteLn("DEV9: *FLASH ADDR = 0x%08lX (addrbyte=%d)", address, addrbyte);
			if (!(value & 0x100))
			{ // address is complete
				if ((cmd == SM_CMD_READ1) || (cmd == SM_CMD_READ2) || (cmd == SM_CMD_READ3))
				{
					ctrl &= ~FLASH_PP_READY;
					memcpy(data, file + (address >> PAGE_SIZE_BITS) * PAGE_SIZE_ECC, PAGE_SIZE);
					calculateECC(data); // calculate ECC; should be in the file already
					ctrl |= FLASH_PP_READY;
				}
				addrbyte = 0; // address reset
				{
					const u32 blocks = address / BLOCK_SIZE;
					u32 pages = address - (blocks * BLOCK_SIZE);
					[[maybe_unused]]const u32 bytes = pages % PAGE_SIZE;
					pages = pages / PAGE_SIZE;
					DevCon.WriteLn("DEV9: *FLASH ADDR = 0x%08lX (%d:%d:%d) (addrbyte=%d) FINAL", address, blocks, pages, bytes, addrbyte);
				}
			}
			break;

		case FLASH_R_CTRL:
			DevCon.WriteLn("DEV9: *FLASH CTRL %dbit write 0x%08lX", size * 8, value);
			ctrl = (ctrl & FLASH_PP_READY) | (value & ~FLASH_PP_READY);
			break;

		case FLASH_R_ID:
			DevCon.WriteLn("DEV9: *FLASH ID %dbit write 0x%08lX DENIED :P", size * 8, value);
			break;

		default:
			DevCon.WriteLn("DEV9: *FLASH Unkwnown %dbit write at address 0x%08lX= 0x%08lX IGNORED", size * 8, addr, value);
			break;
	}
}

static unsigned char xor_table[256] = {
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4, 0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77, 0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66, 0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5, 0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55, 0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96, 0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87, 0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44, 0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0xF0, 0x77, 0x66, 0xE1, 0x55, 0xD2, 0xC3, 0x44, 0x44, 0xC3, 0xD2, 0x55, 0xE1, 0x66, 0x77, 0xF0,
	0x33, 0xB4, 0xA5, 0x22, 0x96, 0x11, 0x00, 0x87, 0x87, 0x00, 0x11, 0x96, 0x22, 0xA5, 0xB4, 0x33,
	0x22, 0xA5, 0xB4, 0x33, 0x87, 0x00, 0x11, 0x96, 0x96, 0x11, 0x00, 0x87, 0x33, 0xB4, 0xA5, 0x22,
	0xE1, 0x66, 0x77, 0xF0, 0x44, 0xC3, 0xD2, 0x55, 0x55, 0xD2, 0xC3, 0x44, 0xF0, 0x77, 0x66, 0xE1,
	0x11, 0x96, 0x87, 0x00, 0xB4, 0x33, 0x22, 0xA5, 0xA5, 0x22, 0x33, 0xB4, 0x00, 0x87, 0x96, 0x11,
	0xD2, 0x55, 0x44, 0xC3, 0x77, 0xF0, 0xE1, 0x66, 0x66, 0xE1, 0xF0, 0x77, 0xC3, 0x44, 0x55, 0xD2,
	0xC3, 0x44, 0x55, 0xD2, 0x66, 0xE1, 0xF0, 0x77, 0x77, 0xF0, 0xE1, 0x66, 0xD2, 0x55, 0x44, 0xC3,
	0x00, 0x87, 0x96, 0x11, 0xA5, 0x22, 0x33, 0xB4, 0xB4, 0x33, 0x22, 0xA5, 0x11, 0x96, 0x87, 0x00};

static void xfromman_call20_calculateXors(unsigned char buffer[128], unsigned char blah[4])
{
	unsigned char a = 0, b = 0, c = 0, i;

	for (i = 0; i < 128; i++)
	{
		a ^= xor_table[buffer[i]];
		if (xor_table[buffer[i]] & 0x80)
		{
			b ^= ~i;
			c ^= i;
		}
	}

	blah[0] = (~a) & 0x77;
	blah[1] = (~b) & 0x7F;
	blah[2] = (~c) & 0x7F;
}
