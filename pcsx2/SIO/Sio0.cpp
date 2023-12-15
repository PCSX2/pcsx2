/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "Common.h"
#include "IopDma.h"
#include "IopHw.h"
#include "R3000A.h"
#include "SIO/Memcard/MemoryCardProtocol.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadBase.h"
#include "SIO/Sio.h"
#include "SIO/Sio0.h"
#include "StateWrapper.h"

#define SIO0LOG_ENABLE 0
#define Sio0Log if (SIO0LOG_ENABLE) DevCon

Sio0 g_Sio0;

void Sio0::ClearStatAcknowledge()
{
	stat &= ~(SIO0_STAT::ACK);
}

Sio0::Sio0() = default;
Sio0::~Sio0() = default;

bool Sio0::Initialize()
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
	return true;
}

bool Sio0::Shutdown()
{
	return true;
}

void Sio0::SoftReset()
{
	padStarted = false;
	sioMode = SioMode::NOT_SET;
	sioCommand = 0;
	sioStage = SioStage::IDLE;
	g_MemoryCardProtocol.ResetPS1State();
}

// Simulates the ACK line on the bus. Peripherals are expected to send an ACK signal
// over this line to tell the PS1 "keep sending me things I'm not done yet". The PS1
// then uses this after it receives the peripheral's response to decide what to do.
void Sio0::SetAcknowledge(bool ack)
{
	if (ack)
	{
		stat |= SIO0_STAT::ACK;
	}
	else
	{
		stat &= ~(SIO0_STAT::ACK);
	}
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
	Sio0Log.WriteLn("%s()\tSIO0 TX_DATA Read\t(%02X)", __FUNCTION__, txData);
	return txData;
}

u8 Sio0::GetRxData()
{
	Sio0Log.WriteLn("%s()\tSIO0 RX_DATA Read\t(%02X)", __FUNCTION__, rxData);
	stat |= (SIO0_STAT::TX_READY | SIO0_STAT::TX_EMPTY);
	stat &= ~(SIO0_STAT::RX_FIFO_NOT_EMPTY);
	return rxData;
}

u32 Sio0::GetStat()
{
	Sio0Log.WriteLn("%s()\tSIO0 STAT Read\t(%08X)", __FUNCTION__, stat);
	const u32 ret = stat;
	Interrupt(Sio0Interrupt::STAT_READ);
	return ret;
}

u16 Sio0::GetMode()
{
	Sio0Log.WriteLn("%s()\tSIO0 MODE Read\t(%08X)", __FUNCTION__, mode);
	return mode;
}

u16 Sio0::GetCtrl()
{
	Sio0Log.WriteLn("%s()\tSIO0 CTRL Read\t(%08X)", __FUNCTION__, ctrl);
	return ctrl;
}

u16 Sio0::GetBaud()
{
	Sio0Log.WriteLn("%s()\tSIO0 BAUD Read\t(%08X)", __FUNCTION__, baud);
	return baud;
}

void Sio0::SetTxData(u8 cmd)
{
	Sio0Log.WriteLn("%s()\tSIO0 TX_DATA Write\t(%02X)", __FUNCTION__, cmd);

	stat |= SIO0_STAT::TX_READY | SIO0_STAT::TX_EMPTY;
	stat |= (SIO0_STAT::RX_FIFO_NOT_EMPTY);

	if (!(ctrl & SIO0_CTRL::TX_ENABLE))
	{
		Console.Warning("%s(%02X) CTRL in illegal state, exiting instantly", __FUNCTION__, cmd);
		return;
	}

	txData = cmd;
	u8 data = 0;
	PadBase* currentPad = nullptr;

	switch (sioMode)
	{
		case SioMode::NOT_SET:
			sioMode = cmd;
			currentPad = Pad::GetPad(port, slot);
			currentPad->SoftReset();
			mcd = &mcds[port][slot];
			SetAcknowledge(true);
			break;
		case SioMode::PAD:
			currentPad = Pad::GetPad(port, slot);
			pxAssertMsg(currentPad != nullptr, "Got nullptr when looking up pad");
			// Set ACK in advance of sending the command to the pad.
			// The pad will, if the command is done, set ACK to false.
			SetAcknowledge(true);
			data = currentPad->SendCommandByte(cmd);
			SetRxData(data);
			break;
		case SioMode::MEMCARD:
			if (this->sioCommand == MemcardCommand::NOT_SET)
			{
				if (IsMemcardCommand(cmd) && mcd->IsPresent() && mcd->IsPSX())
				{
					this->sioCommand = cmd;	
					SetAcknowledge(true);
					SetRxData(this->flag);
				}
				else
				{
					SetAcknowledge(false);
					SetRxData(0x00);
				}
			}
			else
			{
				SetRxData(Memcard(cmd));
			}
			break;
		default:
			SetRxData(0xff);
			SetAcknowledge(false);
			break;
	}

	// If the peripheral did not ACK, the command is done. Time for a soft reset.
	if (!(this->stat & SIO0_STAT::ACK))
	{
		this->SoftReset();
	}

	Interrupt(Sio0Interrupt::TX_DATA_WRITE);
}

void Sio0::SetRxData(u8 value)
{
	Sio0Log.WriteLn("%s()\tSIO0 RX_DATA Write\t(%02X)", __FUNCTION__, value);
	rxData = value;
}

void Sio0::SetStat(u32 value)
{
	Sio0Log.Error("%s()\tSIO0 STAT Write\t(%08X)", __FUNCTION__, value);
}

void Sio0::SetMode(u16 value)
{
	Sio0Log.WriteLn("%s()\tSIO0 MODE Write\t(%04X)", __FUNCTION__, value);
	mode = value;
}

void Sio0::SetCtrl(u16 value)
{
	Sio0Log.WriteLn("%s()\tSIO0 CTRL Write\t(%04X)", __FUNCTION__, value);
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
	Sio0Log.WriteLn("%s()\tSIO0 BAUD Write\t(%04X)", __FUNCTION__, value);
	baud = value;
}

bool Sio0::IsPadCommand(u8 command)
{
	return command >= static_cast<u8>(Pad::Command::MYSTERY) && command <= static_cast<u8>(Pad::Command::RESPONSE_BYTES);
}

bool Sio0::IsMemcardCommand(u8 command)
{
	return command == MemcardCommand::PS1_READ || command == MemcardCommand::PS1_STATE || command == MemcardCommand::PS1_WRITE;
}

bool Sio0::IsPocketstationCommand(u8 command)
{
	return command == MemcardCommand::PS1_POCKETSTATION;
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

bool Sio0::DoState(StateWrapper& sw)
{
	if (!sw.DoMarker("Sio0"))
		return false;

	sw.Do(&txData);
	sw.Do(&rxData);
	sw.Do(&stat);
	sw.Do(&mode);
	sw.Do(&ctrl);
	sw.Do(&baud);
	sw.Do(&flag);
	sw.Do(&sioStage);
	sw.Do(&sioMode);
	sw.Do(&sioCommand);
	sw.Do(&padStarted);
	sw.Do(&rxDataSet);
	sw.Do(&port);
	sw.Do(&slot);

	return sw.IsGood();
}
