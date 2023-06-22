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
#include "R3000A.h"
#include "IopHw.h"
#include "IopDma.h"

#include "Common.h"
#include "Sio.h"
#include "MemoryCardProtocol.h"
#include "MultitapProtocol.h"
#include "Config.h"
#include "Host.h"
#include "PAD/Host/PAD.h"

#include "common/Timer.h"
#include "Recording/InputRecording.h"
#include "IconsFontAwesome5.h"

#define SIO0LOG_ENABLE 0
#define SIO2LOG_ENABLE 0

#define Sio0Log if (SIO0LOG_ENABLE) DevCon
#define Sio2Log if (SIO2LOG_ENABLE) DevCon

std::deque<u8> fifoIn;
std::deque<u8> fifoOut;

Sio0 sio0;
Sio2 sio2;

_mcd mcds[2][4];
_mcd *mcd;

// ============================================================================
// SIO0
// ============================================================================

void Sio0::ClearStatAcknowledge()
{
	stat &= ~(SIO0_STAT::ACK);
}

Sio0::Sio0()
{
	this->FullReset();
}

Sio0::~Sio0() = default;

void Sio0::SoftReset()
{
	padStarted = false;
	sioMode = SioMode::NOT_SET;
	sioCommand = 0;
	sioStage = SioStage::IDLE;
}

void Sio0::FullReset()
{
	SoftReset();

	port = 0;
	slot = 0;

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			mcds[i][j].term = 0x55;
			mcds[i][j].port = i;
			mcds[i][j].slot = j;
			mcds[i][j].FLAG = 0x08;
			mcds[i][j].autoEjectTicks = 0;
		}
	}

	mcd = &mcds[0][0];
}

// Simulates the ACK line on the bus. Peripherals are expected to send an ACK signal
// over this line to tell the PS1 "keep sending me things I'm not done yet". The PS1
// then uses this after it receives the peripheral's response to decide what to do.
void Sio0::Acknowledge()
{
	stat |= SIO0_STAT::ACK;
}

void Sio0::Interrupt(Sio0Interrupt sio0Interrupt)
{
	switch (sio0Interrupt)
	{
		case Sio0Interrupt::TEST_EVENT:
			iopIntcIrq(7);
			break;
		case Sio0Interrupt::STAT_READ:
			ClearStatAcknowledge();
			break;
		case Sio0Interrupt::TX_DATA_WRITE:
			break;
		default:
			Console.Error("%s(%d) Invalid parameter", __FUNCTION__, sio0Interrupt);
			assert(false);
			break;
	}
	
	if (!(psxRegs.interrupt & (1 << IopEvt_SIO)))
	{
		PSX_INT(IopEvt_SIO, PSXCLK / 250000); // PSXCLK/250000);
	}
}

u8 Sio0::GetTxData()
{
	Sio0Log.WriteLn("%s() SIO0 TX_DATA Read (%02X)", __FUNCTION__, txData);
	return txData;
}

u8 Sio0::GetRxData()
{
	Sio0Log.WriteLn("%s() SIO0 RX_DATA Read (%02X)", __FUNCTION__, rxData);
	stat |= (SIO0_STAT::TX_READY | SIO0_STAT::TX_EMPTY);
	stat &= ~(SIO0_STAT::RX_FIFO_NOT_EMPTY);
	return rxData;
}

u32 Sio0::GetStat()
{
	Sio0Log.WriteLn("%s() SIO0 STAT Read (%08X)", __FUNCTION__, stat);
	const u32 ret = stat;
	Interrupt(Sio0Interrupt::STAT_READ);
	return ret;
}

u16 Sio0::GetMode()
{
	Sio0Log.WriteLn("%s() SIO0 MODE Read (%08X)", __FUNCTION__, mode);
	return mode;
}

u16 Sio0::GetCtrl()
{
	Sio0Log.WriteLn("%s() SIO0 CTRL Read (%08X)", __FUNCTION__, ctrl);
	return ctrl;
}

u16 Sio0::GetBaud()
{
	Sio0Log.WriteLn("%s() SIO0 BAUD Read (%08X)", __FUNCTION__, baud);
	return baud;
}

void Sio0::SetTxData(u8 value)
{
	Sio0Log.WriteLn("%s(%02X) SIO0 TX_DATA Write", __FUNCTION__, value);

	stat |= SIO0_STAT::TX_READY | SIO0_STAT::TX_EMPTY;
	stat |= (SIO0_STAT::RX_FIFO_NOT_EMPTY);

	if (!(ctrl & SIO0_CTRL::TX_ENABLE))
	{
		Console.Warning("%s(%02X) CTRL in illegal state, exiting instantly", __FUNCTION__, value);
		return;
	}

	txData = value;
	u8 res = 0;

	switch (sioStage)
	{
		case SioStage::IDLE:
			sioMode = value;
			stat |= SIO0_STAT::TX_READY;

			switch (sioMode)
			{
				case SioMode::PAD:
					res = PADstartPoll(port, slot);

					if (res)
					{
						Acknowledge();
					}

					break;
				case SioMode::MEMCARD:
					mcd = &mcds[port][slot];

					// Check if auto ejection is active. If so, set RECV1 to DISCONNECTED,
					// and zero out the fifo to simulate dead air over the wire.
					if (mcd->autoEjectTicks)
					{
						SetRxData(0x00);
						mcd->autoEjectTicks--;

						if (mcd->autoEjectTicks == 0)
						{
							Host::AddKeyedOSDMessage(fmt::format("AutoEjectSlotClear{}{}", port, slot),
								fmt::format(TRANSLATE_SV("MemoryCard", "Memory Card in port %d / slot %d reinserted"),
									port + 1, slot + 1),
								Host::OSD_INFO_DURATION);
						}

						return;
					}
					
					// If memcard is missing, not PS1, or auto ejected, do not let SIO0 stage advance,
					// reply with dead air and no ACK.
					if (!mcd->IsPresent() || !mcd->IsPSX())
					{
						SetRxData(0x00);
						return;
					}

					Acknowledge();
					break;
			}

			SetRxData(res);
			sioStage = SioStage::WAITING_COMMAND;
			break;
		case SioStage::WAITING_COMMAND:
			stat &= ~(SIO0_STAT::TX_READY);

			if (IsPadCommand(value))
			{
				res = PADpoll(value);
				SetRxData(res);

				if (!PADcomplete())
				{
					Acknowledge();
				}

				sioStage = SioStage::WORKING;
			}
			else if (IsMemcardCommand(value))
			{
				SetRxData(flag);
				Acknowledge();
				sioCommand = value;
				sioStage = SioStage::WORKING;
			}
			else if (IsPocketstationCommand(value))
			{
				// Set the line low, no acknowledge.
				SetRxData(0x00);
				sioStage = SioStage::IDLE;
			}
			else
			{
				Console.Error("%s(%02X) Bad SIO command", __FUNCTION__, value);
				SetRxData(0xff);
				SoftReset();
			}

			break;
		case SioStage::WORKING:
			switch (sioMode)
			{
				case SioMode::PAD:
					res = PADpoll(value);
					SetRxData(res);

					if (!PADcomplete())
					{
						Acknowledge();
					}

					break;
				case SioMode::MEMCARD:
					SetRxData(Memcard(value));
					break;
				default:
					Console.Error("%s(%02X) Unhandled SioMode: %02X", __FUNCTION__, value, sioMode);
					SetRxData(0xff);
					SoftReset();
					break;
			}

			break;
		default:
			Console.Error("%s(%02X) Unhandled SioStage: %02X", __FUNCTION__, value, static_cast<u8>(sioStage));
			SetRxData(0xff);
			SoftReset();
			break;
	}

	Interrupt(Sio0Interrupt::TX_DATA_WRITE);
}

void Sio0::SetRxData(u8 value)
{
	Sio0Log.WriteLn("%s(%02X) SIO0 RX_DATA Write", __FUNCTION__, value);
	rxData = value;
}

void Sio0::SetStat(u32 value)
{
	Sio0Log.Error("%s(%08X) SIO0 STAT Write", __FUNCTION__, value);
}

void Sio0::SetMode(u16 value)
{
	Sio0Log.WriteLn("%s(%04X) SIO0 MODE Write", __FUNCTION__, value);
	mode = value;
}

void Sio0::SetCtrl(u16 value)
{
	Sio0Log.WriteLn("%s(%04X) SIO0 CTRL Write", __FUNCTION__, value);
	ctrl = value;
	port = (ctrl & SIO0_CTRL::PORT) > 0;

	// CTRL appears to be set to 0 between every "transaction".
	// Not documented anywhere, but we'll use this to "reset"
	// the SIO0 state, particularly during the annoying probes
	// to memcards that occur when a game boots.
	if (ctrl == 0)
	{
		g_MemoryCardProtocol.ResetPS1State();
		SoftReset();
	}

	// If CTRL acknowledge, reset STAT bits 3 and 9
	if (ctrl & SIO0_CTRL::ACK)
	{
		stat &= ~(SIO0_STAT::IRQ | SIO0_STAT::RX_PARITY_ERROR);
	}

	if (ctrl & SIO0_CTRL::RESET)
	{
		stat = 0;
		ctrl = 0;
		mode = 0;
		SoftReset();
	}
}

void Sio0::SetBaud(u16 value)
{
	Sio0Log.WriteLn("%s(%04X) SIO0 BAUD Write", __FUNCTION__, value);
	baud = value;
}

bool Sio0::IsPadCommand(u8 command)
{
	return command >= PadCommand::UNK_0 && command <= PadCommand::ANALOG;
}

bool Sio0::IsMemcardCommand(u8 command)
{
	return command == MemcardCommand::PS1_READ || command == MemcardCommand::PS1_STATE || command == MemcardCommand::PS1_WRITE;
}

bool Sio0::IsPocketstationCommand(u8 command)
{
	return command == MemcardCommand::PS1_POCKETSTATION;
}

u8 Sio0::Pad(u8 value)
{
	if (PADcomplete())
	{
		padStarted = false;
	}
	else if (!padStarted)
	{
		padStarted = true;
		PADstartPoll(port, slot);
		Acknowledge();
	}

	return PADpoll(value);
}

u8 Sio0::Memcard(u8 value)
{
	switch (sioCommand)
	{
		case MemcardCommand::PS1_READ:
			return g_MemoryCardProtocol.PS1Read(value);
		case MemcardCommand::PS1_STATE:
			return g_MemoryCardProtocol.PS1State(value);
		case MemcardCommand::PS1_WRITE:
			return g_MemoryCardProtocol.PS1Write(value);
		case MemcardCommand::PS1_POCKETSTATION:
			return g_MemoryCardProtocol.PS1Pocketstation(value);
		default:
			Console.Error("%s(%02X) Unhandled memcard command (%02X)", __FUNCTION__, value, sioCommand);
			SoftReset();
			break;
	}

	return 0xff;
}

// ============================================================================
// SIO2
// ============================================================================


Sio2::Sio2()
{
	this->FullReset();
}

Sio2::~Sio2() = default;

void Sio2::SoftReset()
{
	send3Read = false;
	send3Position = 0;
	commandLength = 0;
	processedLength = 0;
	// Clear dmaBlockSize, in case the next SIO2 command is not sent over DMA11.
	dmaBlockSize = 0;
	send3Complete = false;

	// Anything in fifoIn which was not necessary to consume should be cleared out prior to the next SIO2 cycle.
	while (!fifoIn.empty())
	{
		fifoIn.pop_front();
	}
}

void Sio2::FullReset()
{
	this->SoftReset();

	for (size_t i = 0; i < send3.size(); i++)
	{
		send3.at(i) = 0;
	}

	for (size_t i = 0; i < send1.size(); i++)
	{
		send1.at(i) = 0;
		send2.at(i) = 0;
	}

	dataIn = 0;
	dataOut = 0;
	SetCtrl(Sio2Ctrl::SIO2MAN_RESET);
	SetRecv1(Recv1::DISCONNECTED);
	recv2 = Recv2::DEFAULT;
	recv3 = Recv3::DEFAULT;
	unknown1 = 0;
	unknown2 = 0;
	iStat = 0;

	port = 0;
	slot = 0;

	while (!fifoOut.empty())
	{
		fifoOut.pop_front();
	}

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			mcds[i][j].term = 0x55;
			mcds[i][j].port = i;
			mcds[i][j].slot = j;
			mcds[i][j].FLAG = 0x08;
			mcds[i][j].autoEjectTicks = 0;
		}
	}

	mcd = &mcds[0][0];
}


void Sio2::Interrupt()
{
	iopIntcIrq(17);
}

void Sio2::SetCtrl(u32 value)
{
	this->ctrl = value;

	if (this->ctrl & Sio2Ctrl::START_TRANSFER)
	{
		Interrupt();
	}
}

void Sio2::SetSend3(size_t position, u32 value)
{
	this->send3.at(position) = value;

	if (position == 0)
	{
		SoftReset();
	}
}

void Sio2::SetRecv1(u32 value)
{
	this->recv1 = value;
}

void Sio2::Pad()
{
	// Send PAD our current port, and get back whatever it says the first response byte should be.
	const u8 firstResponseByte = PADstartPoll(port, slot);
	fifoOut.push_back(firstResponseByte);
	// Some games will refuse to read ALL pads, if RECV1 is not set to the CONNECTED value when ANY pad is polled,
	// REGARDLESS of whether that pad is truly connected or not.
	SetRecv1(Recv1::CONNECTED);

	// Then for every byte in fifoIn, pass to PAD and see what it kicks back to us.
	while (!fifoIn.empty())
	{
		const u8 commandByte = fifoIn.front();
		fifoIn.pop_front();
		const u8 responseByte = PADpoll(commandByte);
		fifoOut.push_back(responseByte);
	}
}

void Sio2::Multitap()
{
	fifoOut.push_back(0x00);

	const bool multitapEnabled = (port == 0 && EmuConfig.MultitapPort0_Enabled) || (port == 1 && EmuConfig.MultitapPort1_Enabled);
	SetRecv1(multitapEnabled ? Recv1::CONNECTED : Recv1::DISCONNECTED);

	if (multitapEnabled)
	{
		g_MultitapProtocol.SendToMultitap();
	}
	else 
	{
		while (fifoOut.size() < commandLength)
		{
			fifoOut.push_back(0x00);
		}
	}
}

void Sio2::Infrared()
{
	SetRecv1(Recv1::DISCONNECTED);

	fifoIn.pop_front();
	const u8 responseByte = 0xff;
	
	while (fifoOut.size() < commandLength)
	{
		fifoOut.push_back(responseByte);
	}
}

void Sio2::Memcard()
{
	mcd = &mcds[port][slot];

	// Check if auto ejection is active. If so, set RECV1 to DISCONNECTED,
	// and zero out the fifo to simulate dead air over the wire.
	if (mcd->autoEjectTicks)
	{
		SetRecv1(Recv1::DISCONNECTED);
		fifoOut.push_back(0x00); // Because Sio2::Write pops the first fifoIn member

		while (!fifoIn.empty())
		{
			fifoIn.pop_front();
			fifoOut.push_back(0x00);
		}

		mcd->autoEjectTicks--;

		if (mcd->autoEjectTicks == 0)
		{
			Host::AddKeyedOSDMessage(fmt::format("AutoEjectSlotClear{}{}", port, slot),
				fmt::format(
					TRANSLATE_SV("MemoryCard", "Memory Card in port {} / slot {} reinserted."), port + 1, slot + 1),
				Host::OSD_INFO_DURATION);
		}

		return;
	}

	SetRecv1(mcd->IsPresent() ? Recv1::CONNECTED : Recv1::DISCONNECTED);
	
	const u8 commandByte = fifoIn.front();
	fifoIn.pop_front();
	const u8 responseByte = mcd->IsPresent() ? 0x00 : 0xff;
	fifoOut.push_back(responseByte);
	// Technically, the FLAG byte is only for PS1 memcards. However,
	// since this response byte is still a dud on PS2 memcards, we can
	// basically just cheat and always make this our second response byte for memcards.
	fifoOut.push_back(mcd->FLAG);
	u8 ps1Input = 0;
	u8 ps1Output = 0;

	switch (commandByte)
	{
		case MemcardCommand::PROBE:
			g_MemoryCardProtocol.Probe();
			break;
		case MemcardCommand::UNKNOWN_WRITE_DELETE_END:
			g_MemoryCardProtocol.UnknownWriteDeleteEnd();
			break;
		case MemcardCommand::SET_ERASE_SECTOR:
		case MemcardCommand::SET_WRITE_SECTOR:
		case MemcardCommand::SET_READ_SECTOR:
			g_MemoryCardProtocol.SetSector();
			break;
		case MemcardCommand::GET_SPECS:
			g_MemoryCardProtocol.GetSpecs();
			break;
		case MemcardCommand::SET_TERMINATOR:
			g_MemoryCardProtocol.SetTerminator();
			break;
		case MemcardCommand::GET_TERMINATOR:
			g_MemoryCardProtocol.GetTerminator();
			break;
		case MemcardCommand::WRITE_DATA:
			g_MemoryCardProtocol.WriteData();
			break;
		case MemcardCommand::READ_DATA:
			g_MemoryCardProtocol.ReadData();
			break;
		case MemcardCommand::PS1_READ:
			g_MemoryCardProtocol.ResetPS1State();

			while (!fifoIn.empty())
			{
				ps1Input = fifoIn.front();
				ps1Output = g_MemoryCardProtocol.PS1Read(ps1Input);
				fifoIn.pop_front();
				fifoOut.push_back(ps1Output);
			}

			break;
		case MemcardCommand::PS1_STATE:
			g_MemoryCardProtocol.ResetPS1State();

			while (!fifoIn.empty())
			{
				ps1Input = fifoIn.front();
				ps1Output = g_MemoryCardProtocol.PS1State(ps1Input);
				fifoIn.pop_front();
				fifoOut.push_back(ps1Output);
			}

			break;
		case MemcardCommand::PS1_WRITE:
			g_MemoryCardProtocol.ResetPS1State();

			while (!fifoIn.empty())
			{
				ps1Input = fifoIn.front();
				ps1Output = g_MemoryCardProtocol.PS1Write(ps1Input);
				fifoIn.pop_front();
				fifoOut.push_back(ps1Output);
			}

			break;
		case MemcardCommand::PS1_POCKETSTATION:
			g_MemoryCardProtocol.ResetPS1State();

			while (!fifoIn.empty())
			{
				ps1Input = fifoIn.front();
				ps1Output = g_MemoryCardProtocol.PS1Pocketstation(ps1Input);
				fifoIn.pop_front();
				fifoOut.push_back(ps1Output);
			}

			break;
		case MemcardCommand::READ_WRITE_END:
			g_MemoryCardProtocol.ReadWriteEnd();
			break;
		case MemcardCommand::ERASE_BLOCK:
			g_MemoryCardProtocol.EraseBlock();
			break;
		case MemcardCommand::UNKNOWN_BOOT:
			g_MemoryCardProtocol.UnknownBoot();
			break;
		case MemcardCommand::AUTH_XOR:
			g_MemoryCardProtocol.AuthXor();
			break;
		case MemcardCommand::AUTH_F3:
			g_MemoryCardProtocol.AuthF3();
			break;
		case MemcardCommand::AUTH_F7:
			g_MemoryCardProtocol.AuthF7();
			break;
		default:
			Console.Warning("%s() Unhandled memcard command %02X, things are about to break!", __FUNCTION__, commandByte);
			break;
	}
}

void Sio2::Write(u8 data)
{
	Sio2Log.WriteLn("%s(%02X) SIO2 DATA Write", __FUNCTION__, data);

	if (!send3Read)
	{
		// No more SEND3 positions to access, but the game is still sending us SIO2 writes. Lets ignore them.
		if (send3Position > send3.size())
		{
			Console.Warning("%s(%02X) Received data after exhausting all SEND3 values!", __FUNCTION__, data);
			return;
		}

		const u32 currentSend3 = send3.at(send3Position);
		port = currentSend3 & Send3::PORT;
		commandLength = (currentSend3 >> 8) & Send3::COMMAND_LENGTH_MASK;
		send3Read = true;

		// The freshly read SEND3 position had a length of 0, so we are done handling SIO2 commands until
		// the next SEND3 writes.
		if (commandLength == 0)
		{
			send3Complete = true;
		}

		// If the prior command did not need to fully pop fifoIn, do so now,
		// so that the next command isn't trying to read the last command's leftovers.
		while (!fifoIn.empty())
		{
			fifoIn.pop_front();
		}
	}

	if (send3Complete)
	{
		return;
	}
	
	fifoIn.push_back(data);

	// We have received as many command bytes as we expect, and...
	//
	// ... These were from direct writes into IOP memory (DMA block size is zero when direct writes occur)
	// ... These were from SIO2 DMA (DMA block size is non-zero when SIO2 DMA occurs)
	if ((fifoIn.size() == sio2.commandLength && sio2.dmaBlockSize == 0) || fifoIn.size() == sio2.dmaBlockSize)
	{
		// Go ahead and prep so the next write triggers a load of the new SEND3 value.
		sio2.send3Read = false;
		sio2.send3Position++;

		// Check the SIO mode
		const u8 sioMode = fifoIn.front();
		fifoIn.pop_front();

		switch (sioMode)
		{
			case SioMode::PAD:
				this->Pad();
				break;
			case SioMode::MULTITAP:
				this->Multitap();
				break;
			case SioMode::INFRARED:
				this->Infrared();
				break;
			case SioMode::MEMCARD:
				this->Memcard();
				break;
			default:
				Console.Error("%s(%02X) Unhandled SIO mode %02X", __FUNCTION__, data, sioMode);
				fifoOut.push_back(0x00);
				SetRecv1(Recv1::DISCONNECTED);
				break;
		}

		// If command was sent over SIO2 DMA, align fifoOut to the block size
		if (sio2.dmaBlockSize > 0)
		{
			const size_t dmaDiff = fifoOut.size() % sio2.dmaBlockSize;

			if (dmaDiff > 0)
			{
				const size_t padding = sio2.dmaBlockSize - dmaDiff;

				for (size_t i = 0; i < padding; i++)
				{
					fifoOut.push_back(0x00);
				}
			}
		}
	}
}

u8 Sio2::Read()
{
	u8 ret = 0x00;
	
	if (!fifoOut.empty())
	{
		ret = fifoOut.front();
		fifoOut.pop_front();
	}
	else
	{
		Console.Warning("%s() fifoOut underflow! Returning 0x00.", __FUNCTION__);
	}
	
	Sio2Log.WriteLn("%s() SIO2 DATA Read (%02X)", __FUNCTION__, ret);
	return ret;
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
				AutoEject::Set( port, slot );
			}
		}
	}
}

void SaveStateBase::sio2Freeze()
{
	FreezeTag("sio2");

	Freeze(sio2);
	FreezeDeque(fifoIn);
	FreezeDeque(fifoOut);

	// CRCs for memory cards.
	// If the memory card hasn't changed when loading state, we can safely skip ejecting it.
	u64 mcdCrcs[SIO::PORTS][SIO::SLOTS];
	if (IsSaving())
	{
		for (u32 port = 0; port < SIO::PORTS; port++)
		{
			for (u32 slot = 0; slot < SIO::SLOTS; slot++)
				mcdCrcs[port][slot] = mcds[port][slot].GetChecksum();
		}
	}
	Freeze(mcdCrcs);

	if (IsLoading())
	{
		bool ejected = false;
		for (u32 port = 0; port < SIO::PORTS && !ejected; port++)
		{
			for (u32 slot = 0; slot < SIO::SLOTS; slot++)
			{
				if (mcdCrcs[port][slot] != mcds[port][slot].GetChecksum())
				{
					AutoEject::SetAll();
					ejected = true;
					break;
				}
			}
		}
	}
}

void SaveStateBase::sioFreeze()
{
	FreezeTag("sio0");
	Freeze(sio0);
}

std::tuple<u32, u32> sioConvertPadToPortAndSlot(u32 index)
{
	if (index > 4) // [5,6,7]
		return std::make_tuple(1, index - 4); // 2B,2C,2D
	else if (index > 1) // [2,3,4]
		return std::make_tuple(0, index - 1); // 1B,1C,1D
	else // [0,1]
		return std::make_tuple(index, 0); // 1A,2A
}

u32 sioConvertPortAndSlotToPad(u32 port, u32 slot)
{
	if (slot == 0)
		return port;
	else if (port == 0) // slot=[0,1]
		return slot + 1; // 2,3,4
	else
		return slot + 4; // 5,6,7
}

bool sioPadIsMultitapSlot(u32 index)
{
	return (index >= 2);
}

bool sioPortAndSlotIsMultitap(u32 port, u32 slot)
{
	return (slot != 0);
}

void AutoEject::Set(size_t port, size_t slot)
{
	if (EmuConfig.McdEnableEjection)
	{
		mcds[port][slot].autoEjectTicks = 60;
	}
}

void AutoEject::Clear(size_t port, size_t slot)
{
	mcds[port][slot].autoEjectTicks = 0;
}

void AutoEject::SetAll()
{
	Host::AddIconOSDMessage("AutoEjectAllSet", ICON_FA_SD_CARD,
		TRANSLATE_SV("MemoryCard", "Force ejecting all Memory Cards."), Host::OSD_INFO_DURATION);

	for (size_t port = 0; port < SIO::PORTS; port++)
	{
		for (size_t slot = 0; slot < SIO::SLOTS; slot++)
		{
			AutoEject::Set(port, slot);
		}
	}
}

void AutoEject::ClearAll()
{
	for (size_t port = 0; port < SIO::PORTS; port++)
	{
		for (size_t slot = 0; slot < SIO::SLOTS; slot++)
		{
			AutoEject::Clear(port, slot);
		}
	}
}