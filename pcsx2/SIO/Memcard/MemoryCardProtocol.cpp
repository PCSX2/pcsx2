/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "SIO/Memcard/MemoryCardProtocol.h"

#include "SIO/Sio.h"
#include "SIO/Sio2.h"
#include "SIO/Sio0.h"

#define MC_LOG_ENABLE 0
#define MC_LOG if (MC_LOG_ENABLE) DevCon

#define PS1_FAIL() if (this->PS1Fail()) return;

MemoryCardProtocol g_MemoryCardProtocol;

// Check if the memcard is for PS1, and if we are working on a command sent over SIO2.
// If so, return dead air.
bool MemoryCardProtocol::PS1Fail()
{
	if (mcd->IsPSX() && g_Sio2.commandLength > 0)
	{
		while (g_Sio2FifoOut.size() < g_Sio2.commandLength)
		{
			g_Sio2FifoOut.push_back(0x00);
		}

		return true;
	}

	return false;
}

// A repeated pattern in memcard commands is to pad with zero bytes,
// then end with 0x2b and terminator bytes. This function is a shortcut for that.
void MemoryCardProtocol::The2bTerminator(size_t length)
{
	while (g_Sio2FifoOut.size() < length - 2)
	{
		g_Sio2FifoOut.push_back(0x00);
	}

	g_Sio2FifoOut.push_back(0x2b);
	g_Sio2FifoOut.push_back(mcd->term);
}

// After one read or write, the memcard is almost certainly going to be issued a new read or write
// for the next segment of the same sector. Bump the transferAddr to where that segment begins.
// If it is the end and a new sector is being accessed, the SetSector function will deal with
// both sectorAddr and transferAddr.
void MemoryCardProtocol::ReadWriteIncrement(size_t length)
{
	mcd->transferAddr += length;
}

void MemoryCardProtocol::RecalculatePS1Addr()
{
	mcd->sectorAddr = ((ps1McState.sectorAddrMSB << 8) | ps1McState.sectorAddrLSB);
	mcd->goodSector = (mcd->sectorAddr <= 0x03ff);
	mcd->transferAddr = 128 * mcd->sectorAddr;
}

void MemoryCardProtocol::ResetPS1State()
{
	ps1McState.currentByte = 2;
	ps1McState.sectorAddrMSB = 0;
	ps1McState.sectorAddrLSB = 0;
	ps1McState.checksum = 0;
	ps1McState.expectedChecksum = 0;
	memset(ps1McState.buf.data(), 0, ps1McState.buf.size());
}

void MemoryCardProtocol::Probe()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(4);
}

void MemoryCardProtocol::UnknownWriteDeleteEnd()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(4);
}

void MemoryCardProtocol::SetSector()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 sectorLSB = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	const u8 sector2nd = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	const u8 sector3rd = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	const u8 sectorMSB = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	const u8 expectedChecksum = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();

	u8 computedChecksum = sectorLSB ^ sector2nd ^ sector3rd ^ sectorMSB;
	mcd->goodSector = (computedChecksum == expectedChecksum);

	if (!mcd->goodSector)
	{
		Console.Warning("%s() Warning! Memcard sector checksum failed! (Expected %02X != Actual %02X) Please report to the PCSX2 team!", __FUNCTION__, expectedChecksum, computedChecksum);
	}

	u32 newSector = sectorLSB | (sector2nd << 8) | (sector3rd << 16) | (sectorMSB << 24);
	mcd->sectorAddr = newSector;

	McdSizeInfo info;
	mcd->GetSizeInfo(info);
	mcd->transferAddr = (info.SectorSize + 16) * mcd->sectorAddr;

	The2bTerminator(9);
}

void MemoryCardProtocol::GetSpecs()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	//u8 checksum = 0x00;
	McdSizeInfo info;
	mcd->GetSizeInfo(info);
	g_Sio2FifoOut.push_back(0x2b);
	
	const u8 sectorSizeLSB = (info.SectorSize & 0xff);
	//checksum ^= sectorSizeLSB;
	g_Sio2FifoOut.push_back(sectorSizeLSB);

	const u8 sectorSizeMSB = (info.SectorSize >> 8);
	//checksum ^= sectorSizeMSB;
	g_Sio2FifoOut.push_back(sectorSizeMSB);

	const u8 eraseBlockSizeLSB = (info.EraseBlockSizeInSectors & 0xff);
	//checksum ^= eraseBlockSizeLSB;
	g_Sio2FifoOut.push_back(eraseBlockSizeLSB);

	const u8 eraseBlockSizeMSB = (info.EraseBlockSizeInSectors >> 8);
	//checksum ^= eraseBlockSizeMSB;
	g_Sio2FifoOut.push_back(eraseBlockSizeMSB);

	const u8 sectorCountLSB = (info.McdSizeInSectors & 0xff);
	//checksum ^= sectorCountLSB;
	g_Sio2FifoOut.push_back(sectorCountLSB);

	const u8 sectorCount2nd = (info.McdSizeInSectors >> 8);
	//checksum ^= sectorCount2nd;
	g_Sio2FifoOut.push_back(sectorCount2nd);

	const u8 sectorCount3rd = (info.McdSizeInSectors >> 16);
	//checksum ^= sectorCount3rd;
	g_Sio2FifoOut.push_back(sectorCount3rd);

	const u8 sectorCountMSB = (info.McdSizeInSectors >> 24);
	//checksum ^= sectorCountMSB;
	g_Sio2FifoOut.push_back(sectorCountMSB);
	
	g_Sio2FifoOut.push_back(info.Xor);
	g_Sio2FifoOut.push_back(mcd->term);
}

void MemoryCardProtocol::SetTerminator()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 newTerminator = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	const u8 oldTerminator = mcd->term;
	mcd->term = newTerminator;
	g_Sio2FifoOut.push_back(0x00);
	g_Sio2FifoOut.push_back(0x2b);
	g_Sio2FifoOut.push_back(oldTerminator);
}

void MemoryCardProtocol::GetTerminator()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	g_Sio2FifoOut.push_back(0x2b);
	g_Sio2FifoOut.push_back(mcd->term);
	g_Sio2FifoOut.push_back(static_cast<u8>(Terminator::DEFAULT));
}

void MemoryCardProtocol::WriteData()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	g_Sio2FifoOut.push_back(0x00);
	g_Sio2FifoOut.push_back(0x2b);
	const u8 writeLength = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	u8 checksum = 0x00;
	std::vector<u8> buf;

	for (size_t writeCounter = 0; writeCounter < writeLength; writeCounter++)
	{
		const u8 writeByte = g_Sio2FifoIn.front();
		g_Sio2FifoIn.pop_front();
		checksum ^= writeByte;
		buf.push_back(writeByte);
		g_Sio2FifoOut.push_back(0x00);
	}

	mcd->Write(buf.data(), buf.size());
	g_Sio2FifoOut.push_back(checksum);
	g_Sio2FifoOut.push_back(mcd->term);

	ReadWriteIncrement(writeLength);
}

void MemoryCardProtocol::ReadData()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 readLength = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();
	g_Sio2FifoOut.push_back(0x00);
	g_Sio2FifoOut.push_back(0x2b);
	std::vector<u8> buf;
	buf.resize(readLength);
	mcd->Read(buf.data(), buf.size());
	u8 checksum = 0x00;

	for (const u8 readByte : buf)
	{
		checksum ^= readByte;
		g_Sio2FifoOut.push_back(readByte);
	}

	g_Sio2FifoOut.push_back(checksum);
	g_Sio2FifoOut.push_back(mcd->term);

	ReadWriteIncrement(readLength);
}

u8 MemoryCardProtocol::PS1Read(u8 data)
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	bool sendAck = true;
	u8 ret = 0;

	switch (ps1McState.currentByte)
	{
		case 2:
			ret = 0x5a;
			break;
		case 3:
			ret = 0x5d;
			break;
		case 4:
			ps1McState.sectorAddrMSB = data;
			ret = 0x00;
			break;
		case 5:
			ps1McState.sectorAddrLSB = data;
			ret = 0x00;
			RecalculatePS1Addr();
			break;
		case 6:
			ret = 0x5c;
			break;
		case 7:
			ret = 0x5d;
			break;
		case 8:
			ret = ps1McState.sectorAddrMSB;
			break;
		case 9:
			ret = ps1McState.sectorAddrLSB;
			break;
		case 138:
			ret = ps1McState.checksum;
			break;
		case 139:
			ret = 0x47;
			sendAck = false;
			break;
		case 10:
			ps1McState.checksum = ps1McState.sectorAddrMSB ^ ps1McState.sectorAddrLSB;
			mcd->Read(ps1McState.buf.data(), ps1McState.buf.size());
			[[fallthrough]];
		default:
			ret = ps1McState.buf[ps1McState.currentByte - 10];
			ps1McState.checksum ^= ret;
			break;
	}

	g_Sio0.SetAcknowledge(sendAck);

	ps1McState.currentByte++;
	return ret;
}

u8 MemoryCardProtocol::PS1State(u8 data)
{
	Console.Error("%s(%02X) I do not exist, please change that ASAP.", __FUNCTION__, data);
	assert(false);
	return 0x00;
}

u8 MemoryCardProtocol::PS1Write(u8 data)
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	bool sendAck = true;
	u8 ret = 0;

	switch (ps1McState.currentByte)
	{
		case 2:
			ret = 0x5a;
			break;
		case 3:
			ret = 0x5d;
			break;
		case 4:
			ps1McState.sectorAddrMSB = data;
			ret = 0x00;
			break;
		case 5:
			ps1McState.sectorAddrLSB = data;
			ret = 0x00;
			RecalculatePS1Addr();
			break;
		case 134:
			ps1McState.expectedChecksum = data;
			ret = 0;
			break;
		case 135:
			ret = 0x5c;
			break;
		case 136:
			ret = 0x5d;
			break;
		case 137:
			if (!mcd->goodSector)
			{
				ret = 0xff;
			}
			else if (ps1McState.expectedChecksum != ps1McState.checksum)
			{
				ret = 0x4e;
			}
			else
			{
				mcd->Write(ps1McState.buf.data(), ps1McState.buf.size());
				ret = 0x47;
				// Clear the "directory unread" bit of the flag byte. Per no$psx, this is cleared
				// on writes, not reads.
				mcd->FLAG &= 0x07;
			}

			sendAck = false;
			break;
		case 6:
			ps1McState.checksum = ps1McState.sectorAddrMSB ^ ps1McState.sectorAddrLSB;
			[[fallthrough]];
		default:
			ps1McState.buf[ps1McState.currentByte - 6] = data;
			ps1McState.checksum ^= data;
			ret = 0x00;
			break;
	}

	g_Sio0.SetAcknowledge(sendAck);

	ps1McState.currentByte++;
	return ret;
}

u8 MemoryCardProtocol::PS1Pocketstation(u8 data)
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	g_Sio0.SetAcknowledge(false);
	return 0x00;
}

void MemoryCardProtocol::ReadWriteEnd()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(4);
}

void MemoryCardProtocol::EraseBlock()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	mcd->EraseBlock();
	The2bTerminator(4);
}

void MemoryCardProtocol::UnknownBoot()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(5);
}

void MemoryCardProtocol::AuthXor()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 modeByte = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();

	switch (modeByte)
	{
		// When encountered, the command length in RECV3 is guaranteed to be 14,
		// and the PS2 is expecting us to XOR the data it is about to send.
		case 0x01:
		case 0x02:
		case 0x04:
		case 0x0f:
		case 0x11:
		case 0x13:
		{
			// Long + XOR
			g_Sio2FifoOut.push_back(0x00);
			g_Sio2FifoOut.push_back(0x2b);
			u8 xorResult = 0x00;

			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = g_Sio2FifoIn.front();
				g_Sio2FifoIn.pop_front();
				xorResult ^= toXOR;
				g_Sio2FifoOut.push_back(0x00);
			}

			g_Sio2FifoOut.push_back(xorResult);
			g_Sio2FifoOut.push_back(mcd->term);
			break;
		}
		// When encountered, the command length in RECV3 is guaranteed to be 5,
		// and there is no attempt to XOR anything.
		case 0x00:
		case 0x03:
		case 0x05:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x10:
		case 0x12:
		case 0x14:
		{
			// Short + No XOR
			The2bTerminator(5);
			break;
		}
		// When encountered, the command length in RECV3 is guaranteed to be 14,
		// and the PS2 is about to send us data, BUT the PS2 does NOT want us
		// to send the XOR, it wants us to send the 0x2b and terminator as the
		// last two bytes.
		case 0x06:
		case 0x07:
		case 0x0b:
		{
			// Long + No XOR
			The2bTerminator(14);
			break;
		}
		default:
			Console.Warning("%s(queue) Unexpected modeByte (%02X), please report to the PCSX2 team", __FUNCTION__, modeByte);
			break;
	}
}

void MemoryCardProtocol::AuthF3()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(5);
}

void MemoryCardProtocol::AuthF7()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	The2bTerminator(5);
}
