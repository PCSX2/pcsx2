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

#pragma once

#include "SIO/SioTypes.h"

class StateWrapper;

class Sio0
{
private:
	u32 txData; // 0x1f801040
	u32 rxData; // 0x1f801040
	u32 stat; // 0x1f801044
	u16 mode; // 0x1f801048
	u16 ctrl; // 0x1f80104a
	u16 baud; // 0x1f80104e

	void ClearStatAcknowledge();

public:
	u8 flag = 0;

	SioStage sioStage = SioStage::IDLE;
	u8 sioMode = SioMode::NOT_SET;
	u8 sioCommand = 0;
	bool padStarted = false;
	bool rxDataSet = false;

	u8 port = 0;
	u8 slot = 0;

	Sio0();
	~Sio0();

	bool Initialize();
	bool Shutdown();

	void SoftReset();
	bool DoState(StateWrapper& sw);

	void SetAcknowledge(bool ack);
	void Interrupt(Sio0Interrupt sio0Interrupt);

	u8 GetTxData();
	u8 GetRxData();
	u32 GetStat();
	u16 GetMode();
	u16 GetCtrl();
	u16 GetBaud();

	void SetTxData(u8 value);
	void SetRxData(u8 value);
	void SetStat(u32 value);
	void SetMode(u16 value);
	void SetCtrl(u16 value);
	void SetBaud(u16 value);

	bool IsPadCommand(u8 command);
	bool IsMemcardCommand(u8 command);
	bool IsPocketstationCommand(u8 command);

	u8 Memcard(u8 value);
};

extern Sio0 g_Sio0;
