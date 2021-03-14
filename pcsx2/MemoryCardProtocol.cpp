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

#include "MemoryCardProtocol.h"
#include "Sio.h"
#include "des.h"

#define MC_LOG_ENABLE 0
#define MC_LOG if (MC_LOG_ENABLE) DevCon

#define PS1_FAIL() if (this->PS1Fail()) return;

MemoryCardProtocol g_MemoryCardProtocol;

// keysource and key are self generated values
uint8_t keysource[] = { 0xf5, 0x80, 0x95, 0x3c, 0x4c, 0x84, 0xa9, 0xc0 };
uint8_t dex_key[16] = { 0x17, 0x39, 0xd3, 0xbc, 0xd0, 0x2c, 0x18, 0x07, 0x4b, 0x17, 0xf0, 0xea, 0xc4, 0x66, 0x30, 0xf9 };
uint8_t cex_key[16] = { 0x06, 0x46, 0x7a, 0x6c, 0x5b, 0x9b, 0x82, 0x77, 0x39, 0x0f, 0x78, 0xb7, 0xf2, 0xc6, 0xa5, 0x20 };
uint8_t *key = dex_key;
uint8_t iv[8];
uint8_t seed[8];
uint8_t nonce[8];
uint8_t MechaChallenge1[8];
uint8_t MechaChallenge2[8];
uint8_t MechaChallenge3[8];
uint8_t MechaResponse1[8];
uint8_t MechaResponse2[8];
uint8_t MechaResponse3[8];

static void desEncrypt(void *key, void *data)
{
	DesContext dc;
	desInit(&dc, (uint8_t *) key, 8);
	desEncryptBlock(&dc, (uint8_t *) data, (uint8_t *) data);
}

static void desDecrypt(void *key, void *data)
{
	DesContext dc;
	desInit(&dc, (uint8_t *) key, 8);
	desDecryptBlock(&dc, (uint8_t *) data, (uint8_t *) data);
}

static void doubleDesEncrypt(void *key, void *data)
{
	desEncrypt(key, data);
	desDecrypt(&((uint8_t *) key)[8], data);
	desEncrypt(key, data);
}

static void doubleDesDecrypt(void *key, void *data)
{
	desDecrypt(key, data);
	desEncrypt(&((uint8_t *) key)[8], data);
	desDecrypt(key, data);
}

static void xor_bit(const void* a, const void* b, void* Result, size_t Length)
{
	size_t i;
	for (i = 0; i < Length; i++) {
		((uint8_t*)Result)[i] = ((uint8_t*)a)[i] ^ ((uint8_t*)b)[i];
	}
}

void generateIvSeedNonce()
{
	for (int i = 0; i < 8; i++)
	{
		iv[i] = rand();
		seed[i] = keysource[i] ^ iv[i];
		nonce[i] = rand();
	}
}

void generateResponse()
{
	uint8_t ChallengeIV[8] = { /* SHA256: e7b02f4f8d99a58b96dbca4db81c5d666ea7c46fbf6e1d5c045eaba0ee25416a */ };
	char filename[1024];
	snprintf(filename, sizeof(filename), "%s/%s", EmuFolders::Bios.c_str(), "civ.bin");
	FILE *f = fopen(filename, "rb");
	if (f)
	{
		fread(ChallengeIV, 1, sizeof(ChallengeIV), f);
		fclose(f);
	}

	doubleDesDecrypt(key, MechaChallenge1);
	uint8_t random[8];
	xor_bit(MechaChallenge1, ChallengeIV, random, 8);

	// MechaChallenge2 and MechaChallenge3 let's the card verify the console

	xor_bit(nonce, ChallengeIV, MechaResponse1, 8);
	doubleDesEncrypt(key, MechaResponse1);

	xor_bit(random, MechaResponse1, MechaResponse2, 8);
	doubleDesEncrypt(key, MechaResponse2);

	uint8_t CardKey[] = { 'M', 'e', 'c', 'h', 'a', 'P', 'w', 'n' };
	xor_bit(CardKey, MechaResponse2, MechaResponse3, 8);
	doubleDesEncrypt(key, MechaResponse3);
}

// Check if the memcard is for PS1, and if we are working on a command sent over SIO2.
// If so, return dead air.
bool MemoryCardProtocol::PS1Fail()
{
	if (mcd->IsPSX() && sio2.commandLength > 0)
	{
		while (fifoOut.size() < sio2.commandLength)
		{
			fifoOut.push_back(0x00);
		}

		return true;
	}

	return false;
}

// A repeated pattern in memcard commands is to pad with zero bytes,
// then end with 0x2b and terminator bytes. This function is a shortcut for that.
void MemoryCardProtocol::The2bTerminator(size_t length)
{
	while (fifoOut.size() < length - 2)
	{
		fifoOut.push_back(0x00);
	}

	fifoOut.push_back(0x2b);
	fifoOut.push_back(mcd->term);
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
	const u8 sectorLSB = fifoIn.front();
	fifoIn.pop_front();
	const u8 sector2nd = fifoIn.front();
	fifoIn.pop_front();
	const u8 sector3rd = fifoIn.front();
	fifoIn.pop_front();
	const u8 sectorMSB = fifoIn.front();
	fifoIn.pop_front();
	const u8 expectedChecksum = fifoIn.front();
	fifoIn.pop_front();

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
	fifoOut.push_back(0x2b);
	
	const u8 sectorSizeLSB = (info.SectorSize & 0xff);
	//checksum ^= sectorSizeLSB;
	fifoOut.push_back(sectorSizeLSB);

	const u8 sectorSizeMSB = (info.SectorSize >> 8);
	//checksum ^= sectorSizeMSB;
	fifoOut.push_back(sectorSizeMSB);

	const u8 eraseBlockSizeLSB = (info.EraseBlockSizeInSectors & 0xff);
	//checksum ^= eraseBlockSizeLSB;
	fifoOut.push_back(eraseBlockSizeLSB);

	const u8 eraseBlockSizeMSB = (info.EraseBlockSizeInSectors >> 8);
	//checksum ^= eraseBlockSizeMSB;
	fifoOut.push_back(eraseBlockSizeMSB);

	const u8 sectorCountLSB = (info.McdSizeInSectors & 0xff);
	//checksum ^= sectorCountLSB;
	fifoOut.push_back(sectorCountLSB);

	const u8 sectorCount2nd = (info.McdSizeInSectors >> 8);
	//checksum ^= sectorCount2nd;
	fifoOut.push_back(sectorCount2nd);

	const u8 sectorCount3rd = (info.McdSizeInSectors >> 16);
	//checksum ^= sectorCount3rd;
	fifoOut.push_back(sectorCount3rd);

	const u8 sectorCountMSB = (info.McdSizeInSectors >> 24);
	//checksum ^= sectorCountMSB;
	fifoOut.push_back(sectorCountMSB);
	
	fifoOut.push_back(info.Xor);
	fifoOut.push_back(mcd->term);
}

void MemoryCardProtocol::SetTerminator()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 newTerminator = fifoIn.front();
	fifoIn.pop_front();
	const u8 oldTerminator = mcd->term;
	mcd->term = newTerminator;
	fifoOut.push_back(0x00);
	fifoOut.push_back(0x2b);
	fifoOut.push_back(oldTerminator);
}

void MemoryCardProtocol::GetTerminator()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	fifoOut.push_back(0x2b);
	fifoOut.push_back(mcd->term);
	fifoOut.push_back(static_cast<u8>(Terminator::DEFAULT));
}

void MemoryCardProtocol::WriteData()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	fifoOut.push_back(0x00);
	fifoOut.push_back(0x2b);
	const u8 writeLength = fifoIn.front();
	fifoIn.pop_front();
	u8 checksum = 0x00;
	std::vector<u8> buf;

	for (size_t writeCounter = 0; writeCounter < writeLength; writeCounter++)
	{
		const u8 writeByte = fifoIn.front();
		fifoIn.pop_front();
		checksum ^= writeByte;
		buf.push_back(writeByte);
		fifoOut.push_back(0x00);
	}

	mcd->Write(buf.data(), buf.size());
	fifoOut.push_back(checksum);
	fifoOut.push_back(mcd->term);

	ReadWriteIncrement(writeLength);
}

void MemoryCardProtocol::ReadData()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 readLength = fifoIn.front();
	fifoIn.pop_front();
	fifoOut.push_back(0x00);
	fifoOut.push_back(0x2b);
	std::vector<u8> buf;
	buf.resize(readLength);
	mcd->Read(buf.data(), buf.size());
	u8 checksum = 0x00;

	for (const u8 readByte : buf)
	{
		checksum ^= readByte;
		fifoOut.push_back(readByte);
	}

	fifoOut.push_back(checksum);
	fifoOut.push_back(mcd->term);

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
			ret = ps1McState.buf.at(ps1McState.currentByte - 10);
			ps1McState.checksum ^= ret;
			break;
	}

	if (sendAck)
	{
		sio0.Acknowledge();
	}

	ps1McState.currentByte++;
	return ret;
}

u8 MemoryCardProtocol::PS1State(u8 data)
{
	DevCon.Error("%s(%02X) I do not exist, please change that ASAP.", __FUNCTION__, data);
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
			ps1McState.buf.at(ps1McState.currentByte - 6) = data;
			ps1McState.checksum ^= data;
			ret = 0x00;
			break;
	}

	if (sendAck)
	{
		sio0.Acknowledge();
	}

	ps1McState.currentByte++;
	return ret;
}

u8 MemoryCardProtocol::PS1Pocketstation(u8 data)
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	sio2.SetRecv1(Recv1::DISCONNECTED);
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
	const u8 modeByte = fifoIn.front();
	fifoIn.pop_front();

	switch (modeByte)
	{
		// When encountered, the command length in RECV3 is guaranteed to be 14,
		// and the PS2 is expecting us to XOR the data it is about to send.
		case 0x01: // get iv
		{
			generateIvSeedNonce();
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = iv[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
		case 0x02: // get seed
		{
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = seed[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
		case 0x04: // get nonce
		{
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = nonce[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
		case 0x06:
		{
			for (size_t i = 0; i < 8; i++)
			{
				const u8 val = fifoIn.front();
				fifoIn.pop_front();
				MechaChallenge3[7 - i] = val;
			}
			The2bTerminator(14);
			break;
		}
		case 0x07:
		{
			for (size_t i = 0; i < 8; i++)
			{
				const u8 val = fifoIn.front();
				fifoIn.pop_front();
				MechaChallenge2[7 - i] = val;
			}
			The2bTerminator(14);
			break;
		}
		case 0x0b:
		{
			for (size_t i = 0; i < 8; i++)
			{
				const u8 val = fifoIn.front();
				fifoIn.pop_front();
				MechaChallenge1[7 - i] = val;
			}
			The2bTerminator(14);
			break;
		}
		case 0x0f: // CardResponse1
		{
			generateResponse();
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = MechaResponse1[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
		case 0x11: // CardResponse2
		{
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = MechaResponse2[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
		case 0x13: // CardResponse3
		{
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			u8 xorResult = 0x00;
			for (size_t xorCounter = 0; xorCounter < 8; xorCounter++)
			{
				const u8 toXOR = MechaResponse3[7 - xorCounter];
				fifoIn.pop_front();
				xorResult ^= toXOR;
				fifoOut.push_back(toXOR);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		}
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
			The2bTerminator(5);
			break;
		}
		default:
			Console.Warning("%s(queue) Unexpected modeByte (%02X), please report to the PCSX2 team", __FUNCTION__, modeByte);
			break;
	}
}

void MemoryCardProtocol::AuthCrypt()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();
	const u8 modeByte = fifoIn.front();
	fifoIn.pop_front();

	static u8 xorResult = 0;
	static u8 buf[9];

	switch (modeByte)
	{
		case 0x40:
		case 0x50:
		case 0x42:
		case 0x52:
			The2bTerminator(5);
			break;
		case 0x41:
		case 0x51:
			xorResult = 0;
			for (size_t i = 0; i < 8; i++)
			{
				const u8 val = fifoIn.front();
				fifoIn.pop_front();
				xorResult ^= val;
				buf[i] = val;
			}
			The2bTerminator(14);
			break;
		case 0x43:
		case 0x53:
			fifoOut.push_back(0x00);
			fifoOut.push_back(0x2b);
			for (size_t i = 0; i < 8; i++)
			{
				fifoOut.push_back(buf[i]);
			}
			fifoOut.push_back(xorResult);
			fifoOut.push_back(mcd->term);
			break;
		default:
			Console.Warning("%s(queue) Unexpected modeByte (%02X), please report to the PCSX2 team", __FUNCTION__, modeByte);
			break;
	}
}

void MemoryCardProtocol::AuthReset()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();

	key = dex_key;
	The2bTerminator(5);
}

void MemoryCardProtocol::AuthKeySelect()
{
	MC_LOG.WriteLn("%s", __FUNCTION__);
	PS1_FAIL();

	const u8 data = fifoIn.front();
	fifoIn.pop_front();
	if (data == 1)
	{
		key = cex_key;
	}
	The2bTerminator(5);
}
