/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "R3000A.h"
#include "IopHw.h"
#include "IopDma.h"

#include "Common.h"
#include "Sio.h"
#include "sio_internal.h"
#include "PAD/Gamepad.h"

#include "common/Timer.h"
#include "Recording/InputRecording.h"


_sio sio;
_mcd mcds[2][4];
_mcd *mcd;

SIO_MODE siomode = SIO_START;
static void sioWrite8inl(u8 data);
#define SIO_WRITE void inline
#define SIO_FORCEINLINE __fi

// Magic psx values from nocash info
static const u8 memcard_psx[] = {0x5A, 0x5D, 0x5C, 0x5D, 0x04, 0x00, 0x00, 0x80};

// Memory Card Specs for standard Sony 8mb carts:
//    Flags (magic sio '+' thingie!), Sector size, eraseBlockSize (in pages), card size (in pages), xor checksum (superblock?), terminator (unused?).
// FIXME variable commented out since it's not used atm.
// static const mc_command_0x26_tag mc_sizeinfo_8mb= {'+', 512, 16, 0x4000, 0x52, 0x5A};

// Ejection timeout management belongs in MemoryCardFile
//
//Reinsert the card after auto-eject: after max tries or after min tries + XXX milliseconds, whichever comes first.
//E.g. if the game polls the card 100 times/sec and max tries=100, then after 1 second it will see the card as inserted (ms timeout not reached).
//E.g. if the game polls the card 1 time/sec, then it will see the card ejected 4 times, and on the 5th it will see it as inserted (4 secs from the initial access).
//(A 'try' in this context is the game accessing SIO)
static const int   FORCED_MCD_EJECTION_MIN_TRIES =2;
static const int   FORCED_MCD_EJECTION_MAX_TRIES =128;
static const float FORCED_MCD_EJECTION_MAX_MS_AFTER_MIN_TRIES =2800; 

//allow timeout also for the mcd manager panel
void SetForceMcdEjectTimeoutNow( uint port, uint slot )
{
	mcds[port][slot].ForceEjection_Timeout = FORCED_MCD_EJECTION_MAX_TRIES;
}
void SetForceMcdEjectTimeoutNow()
{
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			SetForceMcdEjectTimeoutNow( port, slot );
		}
	}
}

void ClearMcdEjectTimeoutNow( uint port, uint slot )
{
	mcds[port][slot].ForceEjection_Timeout = 0;
}
void ClearMcdEjectTimeoutNow()
{
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			ClearMcdEjectTimeoutNow( port, slot );
		}
	}
}

// Currently only check if pad wants mtap to be active.
// Could lets PCSX2 have its own options, if anyone ever
// wants to add support for using the extra memcard slots.
static bool IsMtapPresent( uint port )
{
	return EmuConfig.MultitapEnabled( port );
}

void sioInit()
{
	memzero(sio);

	sio.bufSize = 4;
	siomode = SIO_START;
	
	for(int i = 0; i < 2; i++)
	{
		for(int j = 0; j < 4; j++)
		{
			mcds[i][j].term = 0x55;
			mcds[i][j].port = i;
			mcds[i][j].slot = j;
			mcds[i][j].FLAG = 0x08;
			mcds[i][j].ForceEjection_Timeout = 0;
		}

		sio.slot[i] = 0;
	}

	sio.port = 0;
	mcd = &mcds[0][0];

	// Transfer(?) Ready and the Buffer is Empty
	sio.StatReg = TX_RDY | TX_EMPTY;
	sio.packetsize = 0;
}

bool isR3000ATest = false;

// Check the active game's type, and fire the matching interrupt.
// The 3rd bit of the HW_IFCG register lets us know if PSX mode is active. 1 = PSX, 0 = PS2
// Note that the R3000A's call to interrupts only calls the PS2 based (lack of) delays.
SIO_FORCEINLINE void sioInterrupt() {
	if ((psxHu32(HW_ICFG) & (1 << 3)) && !isR3000ATest) {
		if (!(psxRegs.interrupt & (1 << IopEvt_SIO)))
			PSX_INT(IopEvt_SIO, 64); // PSXCLK/250000);
	} else {
		PAD_LOG("Sio Interrupt");
		sio.StatReg |= IRQ;
		iopIntcIrq(7); //Should this be used instead of the one below?
		//psxHu32(0x1070)|=0x80;
	}

	isR3000ATest = false;
}

// An offhand way for the R3000A to access the sioInterrupt function.
// Following the design of the old preprocessor system, the R3000A should
// never call the PSX delays (oddly enough), and only the PS2. So we need
// an extra layer here to help control that.
__fi void sioInterruptR() {
	isR3000ATest = true;
	sioInterrupt();
}

SIO_WRITE sioWriteStart(u8 data)
{
	u32 sioreg = sio2.packet.sendArray3[sio2.cmdport ? (sio2.cmdport - 1) : 0];

	//u16 size1 = (sioreg >>  8) & 0x1FF;
	u16 size2 = (sioreg >> 18) & 0x1FF;

	//if(size1 != size2)
	//	DevCon.Warning("SIO: Bad command length [%02X] (%02X|%02X)", data, size1, size2);
	
	// On mismatch, sio2.cmdlength (size1) is smaller than what it should (Persona 3)
	// while size2 is the proper length. -KrossX
	sio.bufSize = size2; //std::max(size1, size2);

	if(sio.bufSize)
		sio.bufSize--;

	switch(data)
	{
	case 0x01: siomode = SIO_CONTROLLER; break;
	case 0x21: siomode = SIO_MULTITAP; break;
	case 0x61: siomode = SIO_INFRARED; break;
	case 0x81: siomode = SIO_MEMCARD; break;

	default:
		DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
		DEVICE_UNPLUGGED();
		siomode = SIO_DUMMY;
		break;
	}

	sioWrite8inl(data);
}

int byteCnt = 0;

//This is only for digital pad! .... a fast hacky fix for testing
//#define IS_LAST_BYTE_IN_PACKET ((byteCnt >= 3) ? 1 : 0)
//this should be done on the last byte, but it works like this fine for now... How does one know which number is the last byte anyway (or are there any more following it).
//maybe in the end a small LUT will be necessary that has the number of bytes for each command...
//On the real PS1, if the controller doesn't supply an /ACK signal after each byte but the last, the transmission falls through... but it seems it is actually the interrupt
//what 'continues' a transfer.
//On the real PS1, asserting /ACK after the last byte would cause the transfer to fail.
#define IS_LAST_BYTE_IN_PACKET ((sio.bufCount >= 3) ? 1 : 0)

SIO_WRITE sioWriteController(u8 data)
{
	//if (data == 0x01) byteCnt = 0;
	switch(sio.bufCount)
	{
	case 0:
		byteCnt = 0; //hope this gets only cleared on the first byte...
		SIO_STAT_READY();
		DEVICE_PLUGGED();
		sio.buf[0] = PADstartPoll(sio.port + 1);
		break;

	default:
		sio.buf[sio.bufCount] = PADpoll(data);
		if (EmuConfig.EnableRecordingTools)
		{
			// Only examine controllers 1 / 2
			if (sio.slot[sio.port] == 0 || sio.slot[sio.port] == 1)
			{
				g_InputRecording.ControllerInterrupt(data, sio.port, sio.bufCount, sio.buf);
			}
		}
		break;
	}
	//Console.WriteLn( "SIO: sent = %02X  From pad data =  %02X  bufCnt %08X ", data, sio.buf[sio.bufCount], sio.bufCount);
	sioInterrupt(); //Don't all commands(transfers) cause an interrupt?
}

SIO_WRITE sioWriteMultitap(u8 data)
{
	static u8 siocmd = 0;
	//sio.packetsize++;

	switch(sio.bufCount)
	{
	case 0:
		if(IsMtapPresent(sio.port))
		{
			SIO_STAT_READY();
			DEVICE_PLUGGED();
			sio.buf[0] = 0xFF;
			sio.buf[1] = 0x80;
			sio.buf[2] = 0x5A;
		}
		else
		{
			DEVICE_UNPLUGGED();
			sio.buf[0] = 0x00;
			siomode = SIO_DUMMY;
		}
		break;

	case 1:
		siocmd = data;
		switch(data)
		{
		case 0x12: // Pads supported      /// slots supported, port 0,1
		case 0x13: // Memcards supported //// slots supported, port 2,3
			sio.buf[3] = 0x04;
			sio.buf[4] = 0x00;
			sio.buf[5] = 0x5A; // 0x66 here, disables the thing.
			//sio.bufSize = 5;
			break;

		case 0x21: // Select pad
		case 0x22: // Select memcard
			sio.buf[3] = 0x00;
			sio.buf[4] = 0x00;
			sio.buf[5] = 0x00;
			sio.buf[6] = 0x5A;
			//sio.bufSize = 6;
			break;

		default:
			DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
			sio.buf[3] = 0x00;
			sio.buf[4] = 0x00;
			sio.buf[5] = 0x00;
			sio.buf[6] = 0x00;
			break;

		}
		break;

	case 2: // Respond to 0x21/0x22 with requested port
		switch(siocmd)
		{
		case 0x21:
			{
				sio.slot[sio.port] = data;

				u32 ret = PADsetSlot(sio.port+1, data+1);
				sio.buf[5] = ret? data : 0xFF;
				sio.buf[6] = ret? 0x5A : 0x66;
			}
			break;

		case 0x22:
			{
				sio.slot[sio.port] = data;
				sio.buf[5] = data;
			}
			break;
		}
		break;

	case 3: break;
	case 5: break;
	case 6: break;

	//default: sio.buf[sio.bufCount] = 0x00; break;
	}

	sioInterrupt();
}

SIO_WRITE MemcardResponse()
{
	if(sio.bufSize > 1)
	{
		sio.buf[sio.bufSize - 1] = 0x2B;
		sio.buf[sio.bufSize - 0] = mcd->term;
	}
}

SIO_WRITE memcardAuth(u8 data)
{
	static bool doXorCheck = false;
	static u8 xorResult = 0;

	if(sio.bufCount == 2)
	{
		switch(data)
		{
		case 0x01: case 0x02: case 0x04: 
		case 0x0F: case 0x11: case 0x13:
			doXorCheck = true;
			xorResult = 0;
			sio.buf[3] = 0x2B;
			sio.buf[sio.bufSize] = mcd->term;
			break;

		default:
			doXorCheck = false;
			MemcardResponse();
			break;
		}
	}
	else if(doXorCheck)
	{
		switch(sio.bufCount)
		{
		case 3: break;
		case 12: sio.buf[12] = xorResult; break;
		default: xorResult ^= data; break;
		};
	}
}

SIO_WRITE memcardErase(u8 data)
{
	switch(sio.bufCount)
	{
	case 0:
		if(data != 0x81) sio.bufCount = -1;
		break;

	case 1: 
		{
			u8 header[] = {0xFF, 0xFF, 0xFF, 0x2B, mcd->term};

			switch(data)
			{
			case 0x82: // Erase
				//siomode = SIO_DUMMY; // Nothing more to do here.
				memcpy(sio.buf, &header[1], 4);
				sio.bufSize = 3;
				mcd->EraseBlock();
				break;

			default:
				DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
				sio.bufCount = -1;
				//sio.bufSize = 3;
				//sio.bufCount = 4;
				break;
			}
		}
		sioInterrupt();
		break;

	default:
		if(sio.bufCount > sio.bufSize)
		{
			if(data == 0x81)
			{
				SIO_STAT_READY();
				sio.bufCount = 0x00;
			}
		}
		break;
	}
}

SIO_WRITE memcardWrite(u8 data)
{
	static u8 checksum_pos = 0;
	static u8 transfer_size = 0;
	static bool once = false;

	switch(sio.bufCount)
	{
	case 0:
		if(data != 0x81) sio.bufCount = -1;
		break;

	case 1: 
		{
			u8 header[] = {0xFF, 0xFF, 0xFF, 0x2B, mcd->term};

			switch(data)
			{
			case 0x42: // Write
				memcpy(sio.buf, header, 4);
				once = true;
				break;

			case 0x81: // Commit
				if(once)
				{
					siomode = SIO_DUMMY; // Nothing more to do here.
					memcpy(sio.buf, &header[1], 4);
					sio.bufSize = 3;

					sio2.packet.recvVal1 = 0x1600; // Writing
					sio2.packet.recvVal3 = 0x8C;

					once = false;
					break;
				}
				[[fallthrough]];

			default:
				DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
				sio.bufCount = -1;
				//sio.bufSize = 3;
				//sio.bufCount = 4;
				break;
			}

		}
		sioInterrupt();
		break;

	case 2:
		transfer_size = data;

		// Note: coverity wrongly detects a buffer overflow. Because data + 5 could be > 512...
		// So let's add a mask-nop to avoid various useless reports.
		sio.buf[(data & 0xFF) + 5] = mcd->term;
		sio.bufSize = data + 5;
		checksum_pos = data + 4;
		break;

	default:

		if(sio.bufCount < checksum_pos)
		{
			sio.buf[sio.bufCount+1] = data;
		}
		else if(sio.bufCount == checksum_pos)
		{
			u8 xor_check = mcd->DoXor(&sio.buf[4], checksum_pos - 4);
				
			if(xor_check != sio.buf[sio.bufCount])
				Console.Warning("MemWrite: Checksum invalid! XOR: %02X, IN: %02X\n", xor_check, sio.buf[sio.bufCount]);

			sio.buf[sio.bufCount] = xor_check;
			mcd->Write(&sio.buf[4], transfer_size);
			mcd->transferAddr += transfer_size;
		}

		if(sio.bufCount > sio.bufSize)
		{
			if(data == 0x81)
			{
				SIO_STAT_READY();
				sio.bufCount = 0x00;
			}
		}
		break;
	}
}

SIO_WRITE memcardRead(u8 data)
{
	/*static u8 checksum_pos = 0;*/
	// psxmode: check if memcard reads need checksum_pos as well as writes (function above!)
	static u8 transfer_size = 0;
	static bool once = false;

	switch(sio.bufCount)
	{
	case 0:
		if(data != 0x81) sio.bufCount = -1;
		break;

	case 1: 
		{
			u8 header[] = {0xFF, 0xFF, 0xFF, 0x2B, mcd->term};

			switch(data)
			{
			case 0x43: // Read
				memcpy(sio.buf, header, 4);
				once = true;
				break;

			case 0x81: // Commit
				if(once)
				{
					siomode = SIO_DUMMY; // Nothing more to do here.
					memcpy(sio.buf, &header[1], 4);
					sio.bufSize = 3;

					sio2.packet.recvVal1 = 0x1700; // Reading
					sio2.packet.recvVal3 = 0x8C;

					once = false;
					break;
				}
				[[fallthrough]];

			default:
				DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
				sio.bufCount = -1;
				//sio.bufSize = 3;
				//sio.bufCount = 4;
				break;
			}
		}
		sioInterrupt();
		break;

	case 2:
		transfer_size = data;

		mcd->Read(&sio.buf[4], transfer_size);
		mcd->transferAddr += transfer_size;

		sio.buf[transfer_size + 4] = mcd->DoXor(&sio.buf[4], transfer_size);
		sio.buf[transfer_size + 5] = mcd->term;
		sio.bufSize = transfer_size + 5;
		break;

	default:
		if(sio.bufCount > sio.bufSize)
		{
			if(data == 0x81)
			{
				SIO_STAT_READY();
				sio.bufCount = 0x00;
			}
		}
		break;
	}
}


SIO_WRITE memcardSector(u8 data)
{
	static u8 xor_check = 0;
	
	switch(sio.bufCount)
	{
		case 2: mcd->sectorAddr  = data <<  0; xor_check  = data; break;
		case 3: mcd->sectorAddr |= data <<  8; xor_check ^= data; break;
		case 4: mcd->sectorAddr |= data << 16; xor_check ^= data; break;
		case 5: mcd->sectorAddr |= data << 24; xor_check ^= data; break;
		case 6: mcd->goodSector = data == xor_check; break;
		case 8: mcd->transferAddr = (512+16) * mcd->sectorAddr; break;
		case 9:
			{
				switch(sio.cmd)
				{
				case 0x21: siomode = SIO_MEMCARD_ERASE; break;
				case 0x22: siomode = SIO_MEMCARD_WRITE; break;
				case 0x23: siomode = SIO_MEMCARD_READ; break;
				}

				memset8<0xFF>(sio.buf);
				sio.bufCount = -1;
			}
	}
}

SIO_WRITE memcardInit()
{
	mcd = &mcds[sio.GetPort()][sio.GetSlot()];

	// forced ejection logic.  Technically belongs in the McdIsPresent handler.

	bool forceEject = false;

	if(mcd->ForceEjection_Timeout)
	{
		if(mcd->ForceEjection_Timeout == FORCED_MCD_EJECTION_MAX_TRIES && mcd->IsPresent())
			Console.WriteLn( Color_Green,  "Auto-ejecting memcard [port:%d, slot:%d]", sio.GetPort(), sio.GetSlot());

		mcd->ForceEjection_Timeout--;
		forceEject = true;

		u32 numTimesAccessed = FORCED_MCD_EJECTION_MAX_TRIES - mcd->ForceEjection_Timeout;
		
		//minimum tries reached. start counting millisec timeout.
		if(numTimesAccessed == FORCED_MCD_EJECTION_MIN_TRIES)
			mcd->ForceEjection_Timestamp = Common::Timer::GetCurrentValue();

		if(numTimesAccessed > FORCED_MCD_EJECTION_MIN_TRIES)
		{
			if(Common::Timer::ConvertValueToMilliseconds(Common::Timer::GetCurrentValue() - mcd->ForceEjection_Timestamp) >= FORCED_MCD_EJECTION_MAX_MS_AFTER_MIN_TRIES)
			{
				DevCon.Warning( "Auto-eject: Timeout reached after mcd was accessed %d times [port:%d, slot:%d]", numTimesAccessed, sio.GetPort(), sio.GetSlot());
				mcd->ForceEjection_Timeout = 0;	//Done. on next sio access the card will be seen as inserted.
			}
		}

		if(mcd->ForceEjection_Timeout == 0 && mcd->IsPresent())
			Console.WriteLn( Color_Green,  "Re-inserting auto-ejected memcard [port:%d, slot:%d]", sio.GetPort(), sio.GetSlot());
	}
			
	if(!forceEject && mcd->IsPresent())
	{
		DEVICE_PLUGGED();
		siomode = mcd->IsPSX() ? SIO_MEMCARD_PSX : SIO_MEMCARD;
	}
	else
	{
		DEVICE_UNPLUGGED();
		siomode = SIO_DUMMY;
	}

	
}

SIO_WRITE sioWriteMemcard(u8 data)
{
	switch(sio.bufCount)
	{
	case 0:
		SIO_STAT_READY();
		memcardInit();
		sioInterrupt();
		break;

	case 1:
		sio.cmd = data;
		switch(data)
		{
		case 0x21: // SET_SECTOR_ERASE
		case 0x22: // SET_SECTOR_WRITE
		case 0x23: // SET_SECTOR_READ
			sio2.packet.recvVal3 = 0x8C;
			MemcardResponse();
			siomode = SIO_MEMCARD_SECTOR;
			break;

		case 0x26: // GET_SPECS ?
			{
				sio2.packet.recvVal3 = 0x83;

				mc_command_0x26_tag cmd;
				McdSizeInfo info;

				mcd->GetSizeInfo(info);

				cmd.field_151			= 0x2B;
				cmd.sectorSize			= info.SectorSize;
				cmd.eraseBlocks			= info.EraseBlockSizeInSectors;
				cmd.mcdSizeInSectors	= info.McdSizeInSectors;
				cmd.mc_xor				= info.Xor;
				cmd.Z					= mcd->term;

				memcpy(&sio.buf[2], &cmd, sizeof(mc_command_0x26_tag));
			}
			break;

		case 0x27: // SET_TERMINATOR
			sio2.packet.recvVal3 = 0x8B;
			break;

		case 0x28: // GET_TERMINATOR
			sio2.packet.recvVal3 = 0x8B;
			sio.buf[2] = 0x2B;
			sio.buf[3] = mcd->term;
			sio.buf[4] = 0x55; // 0x55 or 0xFF ?
			break;

		// If the PS2 commands fail, it falls back into PSX mode
		case 0x52: // PSX 'R'ead
		case 0x53: // PSX 'S'tate
		case 0x57: // PSX 'W'rite
		case 0x58: // PSX Pocketstation
			siomode = SIO_DUMMY;
			break;

		case 0xF0: // Auth stuff
			siomode = SIO_MEMCARD_AUTH;
			break;

		case 0x11: // On Boot/Probe
		case 0x12: // On Write/Delete/Recheck?
			sio2.packet.recvVal3 = 0x8C;
			[[fallthrough]];

		case 0x81: // Checked right after copy/delete
		case 0xBF: // Wtf?? On game booting?
		case 0xF3: // Reset?
		case 0xF7: // No idea
			MemcardResponse();
			siomode = SIO_DUMMY;
			break;

		default:
			DevCon.Warning("%s cmd: %02X??", __FUNCTION__, data);
			siomode = SIO_DUMMY;
			break;
		}
		sioInterrupt();
		break;

	case 2:
		switch(sio.cmd)
		{
		case 0x27: // SET_TERMINATOR
			mcd->term = data;
			MemcardResponse();
			break;
		}
		break;
	}
}

SIO_WRITE sioWriteMemcardPSX(u8 data)
{
	switch(sio.bufCount)
	{
	case 0: // Same init stuff...
		SIO_STAT_READY();
		memcardInit();
		sioInterrupt();
		break;

	case 1:
		sio.cmd = data;
		switch(data)
		{
		case 0x53: // PSX 'S'tate // haven't seen it happen yet
			sio.buf[1] = mcd->FLAG;
			memcpy(&sio.buf[2], memcard_psx, 8);
			siomode = SIO_DUMMY;
			break;

		case 0x52: // PSX 'R'ead / Probe
		case 0x57: // PSX 'W'rite
		case 0x58: // POCKETSTATION!! Grrrr // Lots of love to the PS2DEV/ps2sdk
			sio.buf[1] = 0x00; //mcd->FLAG;
			sio.buf[2] = 0x5A; // probe end, success "0x5A"
			sio.buf[3] = 0x5D;
			sio.buf[4] = 0x00;
			break;
		// Old handing for Pocketstation, effectively discarded the calls.
		// Keeping it around for reference.
		//case 0x58: // POCKETSTATION!! Grrrr // Lots of love to the PS2DEV/ps2sdk
		//	DEVICE_UNPLUGGED(); // Check is for 0x01000 on stat
		//	siomode = SIO_DUMMY;
		//	break;

		default:
			//printf("%s cmd: %02X??\n", __FUNCTION__, data);
			siomode = SIO_DUMMY;
			break;
		}
		sioInterrupt();
		break;

	case 2: break;
	case 3: break;

	case 4:
		sio.buf[5] = data;
		mcd->sectorAddr = data << 8;
		break;

	case 5:
		sio.buf[6] = data;
		mcd->sectorAddr |= data;
		mcd->goodSector = !(mcd->sectorAddr > 0x3FF);
		mcd->transferAddr = 128 * mcd->sectorAddr;
		break;

	case 6:
		if(sio.cmd == 0x52)
		{
			// READ

			if(!mcd->goodSector)
			{
				memset8<0xFF>(sio.buf);
				siomode = SIO_DUMMY;
			}
			else
			{
				sio.buf[8] = sio.buf[5];
				sio.buf[9] = sio.buf[6];
				sio.buf[6] = 0x5C;
				sio.buf[7] = 0x5D;

				mcd->Read(&sio.buf[10], 0x80);

				sio.buf[138] = mcd->DoXor(&sio.buf[8], 0x80 + 2);
				sio.buf[139] = 0x47;
				siomode = SIO_DUMMY;
			}
		}
		else
		{
			sio.buf[sio.bufCount+1] = data;
		}
		break;

	default:
		// WRITE

		sio.buf[sio.bufCount+1] = data;

		if(sio.bufCount == 134)
		{
			u8 xorcheck = mcd->DoXor(&sio.buf[5], 0x80+2);

			sio.buf[135] = 0x5C;
			sio.buf[136] = 0x5D;

			// (47h=Good, 4Eh=BadChecksum, FFh=BadSector)
			sio.buf[137] = data == xorcheck ? 0x47 : 0x4E;
			if(!mcd->goodSector) sio.buf[137] = 0xFF;
			else mcd->Write(&sio.buf[7], 0x80);
			siomode = SIO_DUMMY;
		}
		break;
	}
}

SIO_WRITE sioWriteInfraRed(u8 data)
{
	SIO_STAT_READY();
	DEVICE_PLUGGED();
	siomode = SIO_DUMMY;
	sioInterrupt();
}

//This bit-field in the STATUS register contains the (inveted) state of the /ACK linre from the Controller / MC.
//1 = /ACK_line_active_low
//Should go into Sio.h
#define ACK_INP 0x80

//This is named RESET_ERR in sio_internal.h.
#define CLR_INTR 0x0010
//Set the ammount of received bytes that triggers an interrupt.
//0=1, 1=2, 2=4, 3=8 receivedBytesIntTriger = 1<< ((ctrl & RX_BYTES_INT) >>8)
#define RX_BYTES_INT 0x0300
//Enable interrupt on TX ready and TX empty
#define TX_INT_EN 0x0400
//Trigger interrupt after receiving several (see above) bytes.
#define RX_INT_EN 0x0800
//Controll register: Enable the /ACK line trigerring the interrupt.
#define ACK_INT_EN 0x1000
//Selects slot 1 or 2
#define SLOT_NR 0x2000

void chkTriggerInt() {
	//Conditions for triggerring an interrupt.
	//this is not correct, but ... it can be fixed later
	 sioInterrupt(); return;
	if ((sio.StatReg & IRQ)) { sioInterrupt(); return; } //The interrupt flag in the main INTR_STAT reg should go active on multiple occasions. Set it here for now (hack), until the correct mechanism is made.
	if ((sio.CtrlReg & ACK_INT_EN) && ((sio.StatReg & TX_RDY) || (sio.StatReg & TX_EMPTY))) { sioInterrupt(); return; }
	if ((sio.CtrlReg & ACK_INT_EN) && (sio.StatReg & ACK_INP)) { sioInterrupt(); return; }
	//The following one may be incorrect.
	//if ((sio.CtrlReg & RX_INT_EN) && ((byteCnt >= (1<< ((sio.CtrlReg & RX_BYTES_INT) >>8))) ? 1:0) ) { sioInterrupt(); return; }
	return;
}


static void sioWrite8inl(u8 data)
{
//	Console.WriteLn( "SIO DATA write %02X  mode %08X " , data, siomode);
	switch(siomode)
	{
	case SIO_START: sioWriteStart(data); break;
	case SIO_CONTROLLER: sioWriteController(data); break;
	case SIO_MULTITAP: sioWriteMultitap(data); break;
	case SIO_INFRARED: sioWriteInfraRed(data); break;
	case SIO_MEMCARD: sioWriteMemcard(data); break;
	case SIO_MEMCARD_AUTH: memcardAuth(data); break;
	case SIO_MEMCARD_ERASE: memcardErase(data); break;
	case SIO_MEMCARD_WRITE: memcardWrite(data); break;
	case SIO_MEMCARD_READ: memcardRead(data); break;
	case SIO_MEMCARD_SECTOR: memcardSector(data); break;
	case SIO_MEMCARD_PSX: sioWriteMemcardPSX(data); break;
	case SIO_DUMMY: break;
	};
	sio.StatReg |= RX_RDY; //Why not set the byte-received flag, when for EVERY sent byte, one is received... it's just how SPI is...
	sio.StatReg |= TX_EMPTY; //The current byte *has* been sent, so it is empty.
	if (IS_LAST_BYTE_IN_PACKET != 1) //The following should be set after each byte transfer but the last one.
		sio.StatReg |= ACK_INP; //Signal that Controller (or MC) has brought the /ACK (Acknowledge) line active low.

	sioInterrupt();
	//chkTriggerInt();
	//Console.WriteLn( "SIO0 WR DATA COMMON %02X  INT_STAT= %08X  IOPpc= %08X " , data, psxHu32(0x1070), psxRegs.pc);
	byteCnt++;
}

int clrAckCnt =0;

void sioStatRead() {

if (clrAckCnt > 1) {  //This check can probably be removed...
	sio.StatReg &= ~ACK_INP; //clear (goes inactive) /ACK line.
	// sio.StatReg &= ~TX_RDY;
	// sio.StatReg &= ~0x200; //irq
	// if (byteCnt == 1)
	// 	sio.StatReg &= ~RX_RDY;
	clrAckCnt = 0;
}
	//The /ACK line should go active for >2us, in a time window between 12us and 100us after each byte is sent (received by the controller).
	//If that doesn't happen, the controller is considered missing.
	//The /ACK line must NOT go active after the last byte in the transmission! (Otherwise some err. may happen - tested.)
if ((sio.StatReg & ACK_INP)) clrAckCnt++;

	chkTriggerInt();
	return;
}

void sioWriteCtrl16(u16 value)
{
	static u8 tcount[2];

//	Console.WriteLn( "SIO0 WR CTRL %02X  IOPpc= %08X " , value, psxRegs.pc);

	tcount[sio.port] = sio.bufCount;
	sio.port = (value >> 13) & 1;

	//printf("RegCtrl: %04X, %d\n", value, sio.bufCount);

	sio.CtrlReg = value & ~RESET_ERR;
	if (value & RESET_ERR) sio.StatReg &= ~IRQ;

	if ((sio.CtrlReg & SIO_RESET) || (!sio.CtrlReg))
	{
		siomode = SIO_START;

		tcount[0] = 0;
		tcount[1] = 0;

		sio.StatReg = TX_RDY | TX_EMPTY;
		psxRegs.interrupt &= ~(1<<IopEvt_SIO);
	}

	if (sio.CtrlReg & CLR_INTR) sio.StatReg &= ~(IRQ | PARITY_ERR); //clear internal interrupt
	sio.bufCount = tcount[sio.port];
	//chkTriggerInt(); //necessary? - causes transfer to stup after the third byte
}

u8 sioRead8()
{
	u8 ret;

	if(sio.StatReg & RX_RDY)
	{
		ret = sio.buf[sio.bufCount];
		if(sio.bufCount == sio.bufSize) SIO_STAT_EMPTY();
		sio.bufCount++;
		SIO_STAT_EMPTY(); //this should depend on the counter above... but it seems wrong (buffer never empties).
		sio.StatReg &= ~TX_RDY; //all clear (transfer of byte ended)
	}
	else
	{
		ret = sio.ret;
	}
		//	sio.StatReg &= ~TX_RDY; //all clear (transfer of byte ended)
//	Console.WriteLn( "SIO DATA read %02X  stat= %08X " , ret, sio.StatReg);
	return ret;
}

void sioWrite8(u8 value)
{
	sioWrite8inl(value);
}

void SIODMAWrite(u8 value) //Why does the SIO2 FIFO handler call this function...??
{ //PS1 and PS2 modes are separate and the SIO0 and SIO2 aren't active at the sam time...
	sioWrite8inl(value);
}

void sioNextFrame() {
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			mcds[port][slot].NextFrame();
		}
	}
}

void sioSetGameSerial( const std::string& serial ) {
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			if ( mcds[port][slot].ReIndex( serial ) ) {
				SetForceMcdEjectTimeoutNow( port, slot );
			}
		}
	}
}

void SaveStateBase::sioFreeze()
{
	// CRCs for memory cards.
	u64 m_mcdCRCs[2][8];

	FreezeTag( "sio" );
	Freeze( sio );

	if( IsSaving() )
	{
		for( uint port=0; port<2; ++port )
			for( uint slot=0; slot<4; ++slot )
				m_mcdCRCs[port][slot] = mcds[port][slot].GetChecksum();
	}

	Freeze( m_mcdCRCs );

	if( IsLoading() && EmuConfig.McdEnableEjection )
	{
		// Notes on the ForceEjectionTimeout:
		//  * TOTA works with values as low as 20 here.
		//    It "times out" with values around 1800 (forces user to check the memcard
		//    twice to find it).  Other games could be different. :|
		//
		//  * At 64: Disgaea 1 and 2, and Grandia 2 end up displaying a quick "no memcard!"
		//    notice before finding the memorycard and re-enumerating it.  A very minor
		//    annoyance, but no breakages.

		//  * GuitarHero will break completely with almost any value here, by design, because
		//    it has a "rule" that the memcard should never be ejected during a song.  So by
		//    ejecting it, the game freezes (which is actually good emulation, but annoying!)

		for( u8 port=0; port<2; ++port )
			for( u8 slot=0; slot<4; ++slot )
		{
			u64 checksum = mcds[port][slot].GetChecksum();

			if( checksum != m_mcdCRCs[port][slot] )
				mcds[port][slot].ForceEjection_Timeout = FORCED_MCD_EJECTION_MAX_TRIES;
		}
	}
}
