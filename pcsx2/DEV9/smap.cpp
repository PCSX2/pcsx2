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

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <Winioctl.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <mutex>

#include "smap.h"
#include "net.h"
#include "pcap_io.h"

bool has_link = true;
volatile bool fireIntR = false;
std::mutex frame_counter_mutex;
std::mutex reset_mutex;
/*
#define	SMAP_BASE			0xb0000000
#define	SMAP_REG8(Offset)		(*(u8 volatile*)(SMAP_BASE+(Offset)))
#define	SMAP_REG16(Offset)	(*(u16 volatile*)(SMAP_BASE+(Offset)))
#define	SMAP_REG32(Offset)	(*(u32 volatile*)(SMAP_BASE+(Offset)))

u32 EMAC3REG_READ(u32 u32Offset)
{
        u32	hi=SMAP_REG16(u32Offset);
        u32	lo=SMAP_REG16(u32Offset+2);
        return	(hi<<16)|lo;
}


void EMAC3REG_WRITE(u32 u32Offset,u32 u32V)
{
        SMAP_REG16(u32Offset)=((u32V>>16)&0xFFFF);
        SMAP_REG16(u32Offset+2)=(u32V&0xFFFF);
}
#define	SMAP_EMAC3_BASE	0x2000
#define	SMAP_EMAC3_STA_CTRL		(SMAP_EMAC3_BASE+0x5C)
void test()
{
	printf ("EMAC3R 0x%08X raw read 0x%08X\n",EMAC3REG_READ(SMAP_EMAC3_STA_CTRL),SMAP_REG32(SMAP_EMAC3_STA_CTRL));
}*/

//this can return a false positive, but its not problem since it may say it cant recv while it can (no harm done, just delay on packets)
bool rx_fifo_can_rx()
{
	//check if RX is on & stuff like that here

	//Check if there is space on RXBD
	if (dev9Ru8(SMAP_R_RXFIFO_FRAME_CNT) == 64)
		return false;

	//Check if there is space on fifo
	int rd_ptr = dev9Ru32(SMAP_R_RXFIFO_RD_PTR);
	int space = sizeof(dev9.rxfifo) -
				((dev9.rxfifo_wr_ptr - rd_ptr) & 16383);


	if (space == 0)
		space = sizeof(dev9.rxfifo);

	if (space < 1514)
		return false;

	//we can recv a packet !
	return true;
}

void rx_process(NetPacket* pk)
{
	smap_bd_t* pbd = ((smap_bd_t*)&dev9.dev9R[SMAP_BD_RX_BASE & 0xffff]) + dev9.rxbdi;

	int bytes = (pk->size + 3) & (~3);

	if (!(pbd->ctrl_stat & SMAP_BD_RX_EMPTY))
	{
		Console.Error("DEV9: ERROR : Discarding %d bytes (RX%d not ready)", bytes, dev9.rxbdi);
		return;
	}

	int pstart = (dev9.rxfifo_wr_ptr) & 16383;
	int i = 0;
	while (i < bytes)
	{
		dev9_rxfifo_write(pk->buffer[i++]);
		dev9.rxfifo_wr_ptr &= 16383;
	}

	//increase RXBD
	std::unique_lock<std::mutex> reset_lock(reset_mutex);
	dev9.rxbdi++;
	dev9.rxbdi &= (SMAP_BD_SIZE / 8) - 1;

	//Fill the BD with info !
	pbd->length = pk->size;
	pbd->pointer = 0x4000 + pstart;
	pbd->ctrl_stat &= ~SMAP_BD_RX_EMPTY;

	//increase frame count
	std::unique_lock<std::mutex> counter_lock(frame_counter_mutex);
	dev9Ru8(SMAP_R_RXFIFO_FRAME_CNT)++;
	counter_lock.unlock();
	reset_lock.unlock();
	//spams// emu_printf("Got packet, %d bytes (%d fifo)\n", pk->size,bytes);
	fireIntR = true;
	//_DEV9irq(SMAP_INTR_RXEND,0);//now ? or when the fifo is full ? i guess now atm
	//note that this _is_ wrong since the IOP interrupt system is not thread safe.. but nothing i can do about that
}

u32 wswap(u32 d)
{
	return (d >> 16) | (d << 16);
}

void tx_process()
{
	//we loop based on count ? or just *use* it ?
	u32 cnt = dev9Ru8(SMAP_R_TXFIFO_FRAME_CNT);
	//spams// printf("tx_process : %u cnt frames !\n",cnt);

	NetPacket pk;
	u32 fc = 0;
	for (fc = 0; fc < cnt; fc++)
	{
		smap_bd_t* pbd = ((smap_bd_t*)&dev9.dev9R[SMAP_BD_TX_BASE & 0xffff]) + dev9.txbdi;

		if (!(pbd->ctrl_stat & SMAP_BD_TX_READY))
		{
			Console.Error("DEV9: SMAP: ERROR : !pbd->ctrl_stat&SMAP_BD_TX_READY");
			break;
		}
		if (pbd->length & 3)
		{
			//spams// emu_printf("WARN : pbd->length not aligned %u\n",pbd->length);
		}

		if (pbd->length > 1514)
		{
			Console.Error("DEV9: SMAP: ERROR : Trying to send packet too big.");
		}
		else
		{
			u32 base = (pbd->pointer - 0x1000) & 16383;
			DevCon.WriteLn("DEV9: Sending Packet from base %x, size %d", base, pbd->length);

			pk.size = pbd->length;

			if (!(pbd->pointer >= 0x1000))
			{
				Console.Error("DEV9: SMAP: ERROR: odd , !pbd->pointer>0x1000 | 0x%X %u", pbd->pointer, pbd->length);
			}
			//increase fifo pointer(s)
			//uh does that even exist on real h/w ?
			/*
			if(dev9.txfifo_rd_ptr+pbd->length >= 16383)
			{
				//warp around !
				//first part
				u32 was=16384-dev9.txfifo_rd_ptr;
				memcpy(pk.buffer,dev9.txfifo+dev9.txfifo_rd_ptr,was);
				//warp
				dev9.txfifo_rd_ptr+=pbd->length;
				dev9.txfifo_rd_ptr&=16383;
				if (pbd->length!=was+dev9.txfifo_rd_ptr)
				{
					emu_printf("ERROR ON TX FIFO HANDLING, %x\n", dev9.txfifo_rd_ptr);
				}
				//second part
				memcpy(pk.buffer+was,dev9.txfifo,pbd->length-was);
			}
			else
			{	//no warp or 'perfect' warp (reads end, resets to start
				memcpy(pk.buffer,dev9.txfifo+dev9.txfifo_rd_ptr,pbd->length);
				dev9.txfifo_rd_ptr+=pbd->length;
				if (dev9.txfifo_rd_ptr==16384)
					dev9.txfifo_rd_ptr=0;
			}
			
			

			if (dev9.txfifo_rd_ptr&(~16383))
			{
				emu_printf("ERROR ON TX FIFO HANDLING, %x\n", dev9.txfifo_rd_ptr);
			}
			*/

			if (base + pbd->length > 16384)
			{
				u32 was = 16384 - base;
				memcpy(pk.buffer, dev9.txfifo + base, was);
				memcpy(pk.buffer + was, dev9.txfifo, pbd->length - was);
				DevCon.WriteLn("DEV9: Warped read, was=%u, sz=%u, sz-was=%u", was, pbd->length, pbd->length - was);
			}
			else
			{
				memcpy(pk.buffer, dev9.txfifo + base, pbd->length);
			}
			tx_put(&pk);
		}


		pbd->ctrl_stat &= ~SMAP_BD_TX_READY;

		//increase TXBD
		dev9.txbdi++;
		dev9.txbdi &= (SMAP_BD_SIZE / 8) - 1;

		//decrease frame count -- this is not thread safe
		dev9Ru8(SMAP_R_TXFIFO_FRAME_CNT)--;
	}

	//spams// emu_printf("processed %u frames, %u count, cnt = %u\n",fc,dev9Ru8(SMAP_R_TXFIFO_FRAME_CNT),cnt);
	//if some error/early exit signal TXDNV
	if (fc != cnt || cnt == 0)
	{
		Console.Error("DEV9: SMAP: WARN : (fc!=cnt || cnt==0) but packet send request was made oO..");
		_DEV9irq(SMAP_INTR_TXDNV, 0);
	}
	//if we actualy send something send TXEND
	if (fc != 0)
		_DEV9irq(SMAP_INTR_TXEND, 100); //now ? or when the fifo is empty ? i guess now atm
}


void emac3_write(u32 addr)
{
	u32 value = wswap(dev9Ru32(addr));
	switch (addr)
	{
		case SMAP_R_EMAC3_MODE0_L:
			//DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_MODE0 write %x", value);
			value = (value & (~SMAP_E3_SOFT_RESET)) | SMAP_E3_TXMAC_IDLE | SMAP_E3_RXMAC_IDLE;
			dev9Ru16(SMAP_R_EMAC3_STA_CTRL_H) |= SMAP_E3_PHY_OP_COMP;
			break;
		case SMAP_R_EMAC3_TxMODE0_L:
			//DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_TxMODE0_L write %x", value);
			//Process TX  here ?
			if (!(value & SMAP_E3_TX_GNP_0))
				Console.Error("DEV9: SMAP_R_EMAC3_TxMODE0_L: SMAP_E3_TX_GNP_0 not set");

			tx_process();
			value = value & ~SMAP_E3_TX_GNP_0;
			if (value)
				Console.Error("DEV9: SMAP_R_EMAC3_TxMODE0_L: extra bits set !");
			break;
		case SMAP_R_EMAC3_TxMODE1_L:
			//DevCon.WriteLn("DEV9: SMAP_R_EMAC3_TxMODE1_L 32bit write %x", value);
			if (value == 0x380f0000)
			{
				Console.WriteLn("DEV9: Adapter Detection Hack - Resetting RX/TX");
				ad_reset();
				_DEV9irq(SMAP_INTR_RXEND | SMAP_INTR_TXEND | SMAP_INTR_TXDNV, 5);
			}
			break;
		case SMAP_R_EMAC3_STA_CTRL_L:
			//DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_STA_CTRL write %x", value);
			{
				if (value & (SMAP_E3_PHY_READ))
				{
					value |= SMAP_E3_PHY_OP_COMP;
					int reg = value & (SMAP_E3_PHY_REG_ADDR_MSK);
					u16 val = dev9.phyregs[reg];
					switch (reg)
					{
						case SMAP_DsPHYTER_BMSR:
							if (has_link)
								val |= SMAP_PHY_BMSR_LINK | SMAP_PHY_BMSR_ANCP;
							break;
						case SMAP_DsPHYTER_PHYSTS:
							if (has_link)
								val |= SMAP_PHY_STS_LINK | SMAP_PHY_STS_100M | SMAP_PHY_STS_FDX | SMAP_PHY_STS_ANCP;
							break;
					}
					//DevCon.WriteLn("DEV9: phy_read %d: %x", reg, val);
					value = (value & 0xFFFF) | (val << 16);
				}
				if (value & (SMAP_E3_PHY_WRITE))
				{
					value |= SMAP_E3_PHY_OP_COMP;
					int reg = value & (SMAP_E3_PHY_REG_ADDR_MSK);
					u16 val = value >> 16;
					switch (reg)
					{
						case SMAP_DsPHYTER_BMCR:
							val &= ~SMAP_PHY_BMCR_RST;
							val |= 0x1;
							break;
					}
					//DevCon.WriteLn("DEV9: phy_write %d: %x", reg, val);
					dev9.phyregs[reg] = val;
				}
			}
			break;
		default:
			DevCon.WriteLn("DEV9: SMAP: emac3 write  %x=%x", addr, value);
			break;
	}
	dev9Ru32(addr) = wswap(value);
}

u8 smap_read8(u32 addr)
{
	switch (addr)
	{
		case SMAP_R_TXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_FRAME_CNT read 8");
			break;
		case SMAP_R_RXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_FRAME_CNT read 8");
			break;

		case SMAP_R_BD_MODE:
			return dev9.bd_swap;

		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 8 bit read @ %X,v=%X", addr, dev9Ru8(addr));
			return dev9Ru8(addr);
	}

	DevCon.WriteLn("DEV9: SMAP : error , 8 bit read @ %X,v=%X", addr, dev9Ru8(addr));
	return dev9Ru8(addr);
}

u16 smap_read16(u32 addr)
{
	int rv = dev9Ru16(addr);
	if (addr >= SMAP_BD_TX_BASE && addr < (SMAP_BD_TX_BASE + SMAP_BD_SIZE))
	{
		if (dev9.bd_swap)
			return (rv << 8) | (rv >> 8);
		return rv;
		/*
		switch (addr & 0x7)
		{
		case 0: // ctrl_stat
			hard = dev9Ru16(addr);
			//DevCon.WriteLn("DEV9: TX_CTRL_STAT[%d]: read %x", (addr - SMAP_BD_TX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 2: // unknown
			hard = dev9Ru16(addr);
			//DevCon.WriteLn("DEV9: TX_UNKNOWN[%d]: read %x", (addr - SMAP_BD_TX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 4: // length
			hard = dev9Ru16(addr);
			DevCon.WriteLn("DEV9: TX_LENGTH[%d]: read %x", (addr - SMAP_BD_TX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 6: // pointer
			hard = dev9Ru16(addr);
			DevCon.WriteLn("DEV9: TX_POINTER[%d]: read %x", (addr - SMAP_BD_TX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		}
		*/
	}
	else if (addr >= SMAP_BD_RX_BASE && addr < (SMAP_BD_RX_BASE + SMAP_BD_SIZE))
	{
		if (dev9.bd_swap)
			return (rv << 8) | (rv >> 8);
		return rv;
		/*
		switch (addr & 0x7)
		{
		case 0: // ctrl_stat
			hard = dev9Ru16(addr);
			//DevCon.WriteLn("DEV9: RX_CTRL_STAT[%d]: read %x", (addr - SMAP_BD_RX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 2: // unknown
			hard = dev9Ru16(addr);
			//DevCon.WriteLn("DEV9: RX_UNKNOWN[%d]: read %x", (addr - SMAP_BD_RX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 4: // length
			hard = dev9Ru16(addr);
			DevCon.WriteLn("DEV9: RX_LENGTH[%d]: read %x", (addr - SMAP_BD_RX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		case 6: // pointer
			hard = dev9Ru16(addr);
			DevCon.WriteLn("DEV9: RX_POINTER[%d]: read %x", (addr - SMAP_BD_RX_BASE) / 8, hard);
			if(dev9.bd_swap)
				return (hard<<8)|(hard>>8);
			return hard;
		}
		*/
	}
#if (0)
	switch (addr)
	{
		case SMAP_R_TXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_FRAME_CNT read 16");
			return dev9Ru16(addr);
		case SMAP_R_RXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_FRAME_CNT read 16");
			return dev9Ru16(addr);
		case SMAP_R_EMAC3_MODE0_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_MODE0_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_MODE0_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_MODE0_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_MODE1_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_MODE1_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_MODE1_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_MODE1_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_RxMODE_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_RxMODE_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_RxMODE_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_RxMODE_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_INTR_STAT_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_INTR_STAT_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_INTR_STAT_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_INTR_STAT_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_INTR_ENABLE_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_INTR_ENABLE_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_INTR_ENABLE_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_INTR_ENABLE_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_TxMODE0_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_TxMODE0_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_TxMODE0_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_TxMODE0_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_TxMODE1_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_TxMODE1_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_TxMODE1_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_TxMODE1_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_STA_CTRL_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_STA_CTRL_L 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);

		case SMAP_R_EMAC3_STA_CTRL_H:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_STA_CTRL_H 16bit read %x", dev9Ru16(addr));
			return dev9Ru16(addr);
		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 16 bit read @ %X,v=%X", addr, dev9Ru16(addr));
			return dev9Ru16(addr);
	}
#endif
	return rv;
}

u32 smap_read32(u32 addr)
{
	if (addr >= SMAP_EMAC3_REGBASE && addr < SMAP_EMAC3_REGEND)
	{
		u32 hi = smap_read16(addr);
		u32 lo = smap_read16(addr + 2) << 16;
		return hi | lo;
	}
	switch (addr)
	{
		case SMAP_R_TXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_FRAME_CNT read 32");
			return dev9Ru32(addr);
		case SMAP_R_RXFIFO_FRAME_CNT:
			DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_FRAME_CNT read 32");
			return dev9Ru32(addr);
		case SMAP_R_EMAC3_STA_CTRL_L:
			DevCon.WriteLn("DEV9: SMAP_R_EMAC3_STA_CTRL_L 32bit read value %x", dev9Ru32(addr));
			return dev9Ru32(addr);

		case SMAP_R_RXFIFO_DATA:
		{
			int rd_ptr = dev9Ru32(SMAP_R_RXFIFO_RD_PTR) & 16383;

			int rv = *((u32*)(dev9.rxfifo + rd_ptr));

			dev9Ru32(SMAP_R_RXFIFO_RD_PTR) = ((rd_ptr + 4) & 16383);

			//DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_DATA 32bit read %x", rv);
			return rv;
		}
		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 32 bit read @ %X,v=%X", addr, dev9Ru32(addr));
			return dev9Ru32(addr);
	}
}

void smap_write8(u32 addr, u8 value)
{
	std::unique_lock<std::mutex> reset_lock(reset_mutex, std::defer_lock);
	std::unique_lock<std::mutex> counter_lock(frame_counter_mutex, std::defer_lock);
	switch (addr)
	{
		case SMAP_R_TXFIFO_FRAME_INC:
			//DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_FRAME_INC 8bit write %x", value);
			{
				dev9Ru8(SMAP_R_TXFIFO_FRAME_CNT)++;
			}
			return;

		case SMAP_R_RXFIFO_FRAME_DEC:
			//DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_FRAME_DEC 8bit write %x", value);
			counter_lock.lock();
			dev9Ru8(addr) = value;
			{
				dev9Ru8(SMAP_R_RXFIFO_FRAME_CNT)--;
			}
			counter_lock.unlock();
			return;

		case SMAP_R_TXFIFO_CTRL:
			//DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_CTRL 8bit write %x", value);
			if (value & SMAP_TXFIFO_RESET)
			{
				dev9.txbdi = 0;
				dev9.txfifo_rd_ptr = 0;
				dev9Ru8(SMAP_R_TXFIFO_FRAME_CNT) = 0; //this actualy needs to be atomic (lock mov ...)
				dev9Ru32(SMAP_R_TXFIFO_WR_PTR) = 0;
				dev9Ru32(SMAP_R_TXFIFO_SIZE) = 16384;
			}
			value &= ~SMAP_TXFIFO_RESET;
			dev9Ru8(addr) = value;
			return;

		case SMAP_R_RXFIFO_CTRL:
			//DevCon.WriteLn("DEV9: SMAP_R_RXFIFO_CTRL 8bit write %x", value);
			if (value & SMAP_RXFIFO_RESET)
			{
				reset_lock.lock(); //lock reset mutex 1st
				counter_lock.lock();
				dev9.rxbdi = 0;
				dev9.rxfifo_wr_ptr = 0;
				dev9Ru8(SMAP_R_RXFIFO_FRAME_CNT) = 0;
				dev9Ru32(SMAP_R_RXFIFO_RD_PTR) = 0;
				dev9Ru32(SMAP_R_RXFIFO_SIZE) = 16384;
				reset_lock.unlock();
				counter_lock.unlock();
			}
			value &= ~SMAP_RXFIFO_RESET;
			dev9Ru8(addr) = value;
			return;

		case SMAP_R_BD_MODE:
			if (value & SMAP_BD_SWAP)
			{
				DevCon.WriteLn("DEV9: SMAP_R_BD_MODE: Byteswapping enabled.");
				dev9.bd_swap = 1;
			}
			else
			{
				DevCon.WriteLn("DEV9: SMAP_R_BD_MODE: Byteswapping disabled.");
				dev9.bd_swap = 0;
			}
			return;
		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 8 bit write @ %X,v=%X", addr, value);
			dev9Ru8(addr) = value;
			return;
	}
}

void smap_write16(u32 addr, u16 value)
{
	if (addr >= SMAP_BD_TX_BASE && addr < (SMAP_BD_TX_BASE + SMAP_BD_SIZE))
	{
		if (dev9.bd_swap)
			value = (value >> 8) | (value << 8);
		dev9Ru16(addr) = value;
		/*
		switch (addr & 0x7) 
		{
		case 0: // ctrl_stat
			DevCon.WriteLn("DEV9: TX_CTRL_STAT[%d]: write %x", (addr - SMAP_BD_TX_BASE) / 8, value);
			//hacky
			dev9Ru16(addr) = value;
			return;
		case 2: // unknown
			//DevCon.WriteLn("DEV9: TX_UNKNOWN[%d]: write %x", (addr - SMAP_BD_TX_BASE) / 8, value);
			dev9Ru16(addr) = value;
			return;
		case 4: // length
			DevCon.WriteLn("DEV9: TX_LENGTH[%d]: write %x", (addr - SMAP_BD_TX_BASE) / 8, value);
			dev9Ru16(addr) = value;
			return;
		case 6: // pointer
			DevCon.WriteLn("DEV9: TX_POINTER[%d]: write %x", (addr - SMAP_BD_TX_BASE) / 8, value);
			dev9Ru16(addr) = value;
			return;
		}
		*/
		return;
	}
	else if (addr >= SMAP_BD_RX_BASE && addr < (SMAP_BD_RX_BASE + SMAP_BD_SIZE))
	{
		//int rx_index=(addr - SMAP_BD_RX_BASE)>>3;
		if (dev9.bd_swap)
			value = (value >> 8) | (value << 8);
		dev9Ru16(addr) = value;
		/*
		switch (addr & 0x7) 
		{
		case 0: // ctrl_stat
			DevCon.WriteLn("DEV9: RX_CTRL_STAT[%d]: write %x", rx_index, value);
			dev9Ru16(addr) = value;
			if(value&0x8000)
			{
				DevCon.WriteLn("DEV9:  * * PACKET READ COMPLETE:   rd_ptr=%d, wr_ptr=%d", dev9Ru32(SMAP_R_RXFIFO_RD_PTR), dev9.rxfifo_wr_ptr);
			}
			return;
		case 2: // unknown
			//DevCon.WriteLn("DEV9: RX_UNKNOWN[%d]: write %x", rx_index, value);
			dev9Ru16(addr) = value;
			return;
		case 4: // length
			DevCon.WriteLn("DEV9: RX_LENGTH[%d]: write %x", rx_index, value);
			dev9Ru16(addr) = value;
			return;
		case 6: // pointer
			DevCon.WriteLn("DEV9: RX_POINTER[%d]: write %x", rx_index, value);
			dev9Ru16(addr) = value;
			return;
		}
		*/
		return;
	}

	switch (addr)
	{
		case SMAP_R_INTR_CLR:
			//DevCon.WriteLn("DEV9: SMAP: SMAP_R_INTR_CLR 16bit write %x", value);
			dev9.irqcause &= ~value;
			return;

		case SMAP_R_TXFIFO_WR_PTR:
			DevCon.WriteLn("DEV9: SMAP: SMAP_R_TXFIFO_WR_PTR 16bit write %x", value);
			dev9Ru16(addr) = value;
			return;
#define EMAC3_L_WRITE(name)                                   \
	case name:                                                \
		/* DevCon.WriteLn("DEV9: SMAP: " #name " 16 bit write %x", value);*/ \
		dev9Ru16(addr) = value;                               \
		return;
	// clang-format off
	//handle L writes
	EMAC3_L_WRITE(SMAP_R_EMAC3_MODE0_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_MODE1_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_TxMODE0_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_TxMODE1_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_RxMODE_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INTR_STAT_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INTR_ENABLE_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_ADDR_HI_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_ADDR_LO_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_VLAN_TPID)
	EMAC3_L_WRITE(SMAP_R_EMAC3_PAUSE_TIMER_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INDIVID_HASH1)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INDIVID_HASH2)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INDIVID_HASH3)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INDIVID_HASH4)
	EMAC3_L_WRITE(SMAP_R_EMAC3_GROUP_HASH1)
	EMAC3_L_WRITE(SMAP_R_EMAC3_GROUP_HASH2)
	EMAC3_L_WRITE(SMAP_R_EMAC3_GROUP_HASH3)
	EMAC3_L_WRITE(SMAP_R_EMAC3_GROUP_HASH4)

	EMAC3_L_WRITE(SMAP_R_EMAC3_LAST_SA_HI)
	EMAC3_L_WRITE(SMAP_R_EMAC3_LAST_SA_LO)
	EMAC3_L_WRITE(SMAP_R_EMAC3_INTER_FRAME_GAP_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_STA_CTRL_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_TX_THRESHOLD_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_RX_WATERMARK_L)
	EMAC3_L_WRITE(SMAP_R_EMAC3_TX_OCTETS)
	EMAC3_L_WRITE(SMAP_R_EMAC3_RX_OCTETS)
	// clang-format on

#define EMAC3_H_WRITE(name)                                   \
	case name:                                                \
		/* DevCon.WriteLn("DEV9: SMAP: " #name " 16 bit write %x", value);*/ \
		dev9Ru16(addr) = value;                               \
		emac3_write(addr - 2);                                \
		return;
	// clang-format off
	//handle H writes
	EMAC3_H_WRITE(SMAP_R_EMAC3_MODE0_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_MODE1_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_TxMODE0_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_TxMODE1_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_RxMODE_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INTR_STAT_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INTR_ENABLE_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_ADDR_HI_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_ADDR_LO_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_VLAN_TPID + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_PAUSE_TIMER_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INDIVID_HASH1 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INDIVID_HASH2 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INDIVID_HASH3 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INDIVID_HASH4 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_GROUP_HASH1 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_GROUP_HASH2 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_GROUP_HASH3 + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_GROUP_HASH4 + 2)

	EMAC3_H_WRITE(SMAP_R_EMAC3_LAST_SA_HI + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_LAST_SA_LO + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_INTER_FRAME_GAP_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_STA_CTRL_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_TX_THRESHOLD_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_RX_WATERMARK_H)
	EMAC3_H_WRITE(SMAP_R_EMAC3_TX_OCTETS + 2)
	EMAC3_H_WRITE(SMAP_R_EMAC3_RX_OCTETS + 2)
	// clang-format on
			/*
	case SMAP_R_EMAC3_MODE0_L:
		DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_MODE0 write %x", value);
		dev9Ru16(addr) = value;
		return;
	case SMAP_R_EMAC3_TxMODE0_L:
		DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_TxMODE0_L 16bit write %x", value);
		dev9Ru16(addr) = value;
		return;
	case SMAP_R_EMAC3_TxMODE1_L:
		emu_printf("SMAP: SMAP_R_EMAC3_TxMODE1_L 16bit write %x\n", value);
		dev9Ru16(addr) = value;
		return;

	case SMAP_R_EMAC3_TxMODE0_H:
		emu_printf("SMAP: SMAP_R_EMAC3_TxMODE0_H 16bit write %x\n", value);
		dev9Ru16(addr) = value;
		return;
	
	case SMAP_R_EMAC3_TxMODE1_H:
		emu_printf("SMAP: SMAP_R_EMAC3_TxMODE1_H 16bit write %x\n", value);
		dev9Ru16(addr) = value;
		return;
	case SMAP_R_EMAC3_STA_CTRL_H:
		DevCon.WriteLn("DEV9: SMAP: SMAP_R_EMAC3_STA_CTRL_H 16bit write %x", value);
		dev9Ru16(addr) = value;
		return;
		*/

		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 16 bit write @ %X,v=%X", addr, value);
			dev9Ru16(addr) = value;
			return;
	}
}

void smap_write32(u32 addr, u32 value)
{
	if (addr >= SMAP_EMAC3_REGBASE && addr < SMAP_EMAC3_REGEND)
	{
		smap_write16(addr, value & 0xFFFF);
		smap_write16(addr + 2, value >> 16);
		return;
	}
	switch (addr)
	{
		case SMAP_R_TXFIFO_DATA:
			//DevCon.WriteLn("DEV9: SMAP_R_TXFIFO_DATA 32bit write %x", value);
			*((u32*)(dev9.txfifo + dev9Ru32(SMAP_R_TXFIFO_WR_PTR))) = value;
			dev9Ru32(SMAP_R_TXFIFO_WR_PTR) = (dev9Ru32(SMAP_R_TXFIFO_WR_PTR) + 4) & 16383;
			return;
		default:
			DevCon.WriteLn("DEV9: SMAP : Unknown 32 bit write @ %X,v=%X", addr, value);
			dev9Ru32(addr) = value;
			return;
	}
}

void smap_readDMA8Mem(u32* pMem, int size)
{
	if (dev9Ru16(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN)
	{
		dev9Ru32(SMAP_R_RXFIFO_RD_PTR) &= 16383;

		DevCon.WriteLn("DEV9:  * * SMAP DMA READ START: rd_ptr=%d, wr_ptr=%d", dev9Ru32(SMAP_R_RXFIFO_RD_PTR), dev9.rxfifo_wr_ptr);
		while (size > 0)
		{
			*pMem = *((u32*)(dev9.rxfifo + dev9Ru32(SMAP_R_RXFIFO_RD_PTR)));
			pMem++;
			dev9Ru32(SMAP_R_RXFIFO_RD_PTR) = (dev9Ru32(SMAP_R_RXFIFO_RD_PTR) + 4) & 16383;

			size -= 4;
		}
		DevCon.WriteLn("DEV9:  * * SMAP DMA READ END:   rd_ptr=%d, wr_ptr=%d", dev9Ru32(SMAP_R_RXFIFO_RD_PTR), dev9.rxfifo_wr_ptr);

		dev9Ru16(SMAP_R_RXFIFO_CTRL) &= ~SMAP_RXFIFO_DMAEN;
	}
}

void smap_writeDMA8Mem(u32* pMem, int size)
{
	if (dev9Ru16(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_DMAEN)
	{
		dev9Ru32(SMAP_R_TXFIFO_WR_PTR) &= 16383;

		DevCon.WriteLn("DEV9:  * * SMAP DMA WRITE START: wr_ptr=%d, rd_ptr=%d", dev9Ru32(SMAP_R_TXFIFO_WR_PTR), dev9.txfifo_rd_ptr);
		while (size > 0)
		{
			int value = *pMem;
			//	value=(value<<24)|(value>>24)|((value>>8)&0xFF00)|((value<<8)&0xFF0000);
			pMem++;

			*((u32*)(dev9.txfifo + dev9Ru32(SMAP_R_TXFIFO_WR_PTR))) = value;
			dev9Ru32(SMAP_R_TXFIFO_WR_PTR) = (dev9Ru32(SMAP_R_TXFIFO_WR_PTR) + 4) & 16383;
			size -= 4;
		}
		DevCon.WriteLn("DEV9:  * * SMAP DMA WRITE END:   wr_ptr=%d, rd_ptr=%d", dev9Ru32(SMAP_R_TXFIFO_WR_PTR), dev9.txfifo_rd_ptr);

		dev9Ru16(SMAP_R_TXFIFO_CTRL) &= ~SMAP_TXFIFO_DMAEN;
	}
}

void smap_async(u32 cycles)
{
	if (fireIntR)
	{
		fireIntR = false;
		//Is this used to signal each individual packet, or just when there are packets in the RX fifo?
		//I think it just signals when there are packets in the RX fifo
		_DEV9irq(SMAP_INTR_RXEND, 0); //Make the call to _DEV9irq in a thread safe way
	}
}
