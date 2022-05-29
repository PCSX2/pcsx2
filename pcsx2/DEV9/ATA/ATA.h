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

#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>

#include "ghc/filesystem.h"

#include "common/Path.h"
#include "DEV9/SimpleQueue.h"

class ATA
{
public:
	//Transfer
	bool dmaReady = false;
	int nsector = 0;     //sector count
	int nsectorLeft = 0; //sectors left to transfer
private:
	const bool lba48Supported = false;

	std::fstream hddImage;
	u64 hddImageSize;

	int pioMode;
	int sdmaMode;
	int mdmaMode;
	int udmaMode;

	//Info
	u8 curHeads = 16;
	u8 curSectors = 63;
	u16 curCylinders = 0;

	u8 curMultipleSectorsSetting = 128;

	u8 identifyData[512] = {0};

	//LBA48 in use?
	bool lba48 = false;

	//Enable/disable features
	bool fetSmartEnabled = true;
	bool fetSecurityEnabled = false;
	bool fetWriteCacheEnabled = true;
	bool fetHostProtectedAreaEnabled = false;

	//Regs
	u16 regCommand; //WriteOnly, Only to be written BSY and DRQ are cleared, DMACK is not set and device is not sleeping, except for DEVICE RESET
	//PIO Read/Write, Only to be written DMACK is not set and DRQ is 1
	//COMMAND REG (WriteOnly) Only to be written DMACK is not set
	//Bit 0 = 0
	bool regControlEnableIRQ = false; //Bit 1 = 1 Disable Interrupt
	//Bit 2 = 1 Software Reset
	bool regControlHOBRead = false; //Bit 7 = HOB (cleared by any write to RegCommand, Sets if Low order or High order bytes are read in ATAread16)
	//End COMMAND REG
	u8 regError; //ReadOnly

	//DEVICE REG (Read/Write)
	u8 regSelect;
	//Bit 0-3: LBA Bits 24-27 (Unused in 48bit) or Command Dependent
	//Bit 4: Selected Device
	//Bit 5: Obsolete (All?)
	//Bit 6: Command Dependent
	//Bit 7: Obsolete (All?)
	//End COMMAND REG
	u8 regFeature; //WriteOnly, Only to be written BSY and DRQ are cleared and DMACK is not set
	u8 regFeatureHOB;

	//Following regs are Read/Write, Only to be written BSY and DRQ are cleared and DMACK is not set
	u8 regSector; //Sector Number or LBA Low
	u8 regSectorHOB;
	u8 regLcyl; //LBA Mid
	u8 regLcylHOB;
	u8 regHcyl; //LBA High
	u8 regHcylHOB;
	//TODO handle nsector code
	u8 regNsector;
	u8 regNsectorHOB;

	u8 regStatus; //ReadOnly. When read via AlternateStatus pending interrupts are not cleared

	//Transfer
	//Write Buffer(s)
	bool awaitFlush = false;
	u8* currentWrite; //array
	u32 currentWriteLength;
	u64 currentWriteSectors;

	struct WriteQueueEntry
	{
		u8* data;
		u32 length;
		u64 sector;
	};
	SimpleQueue<WriteQueueEntry> writeQueue;

	std::thread ioThread;
	bool ioRunning = false;
	std::mutex ioMutex;

	std::condition_variable ioThreadIdle_cv;
	bool ioThreadIdle_bool = false;

	std::condition_variable ioReady;
	std::atomic_bool ioClose{false};
	bool ioWrite;
	bool ioRead;
	void (ATA::*waitingCmd)() = nullptr;
	//Write Buffer(s)

	//Read Buffer
	int rdTransferred = 0;
	int wrTransferred = 0;
	//Max tranfer on 24bit is 256*512 = 128KB
	//Max tranfer on 48bit is 65536*512 = 32MB
	int readBufferLen;
	u8* readBuffer = nullptr;
	//Read Buffer

	//PIO Buffer
	int pioPtr;
	int pioEnd;
	u8 pioBuffer[512];

	int sectorsPerInterrupt;
	void (ATA::*pioDRQEndTransferFunc)() = nullptr;
	//PIO Buffer

	//Smart
	bool smartAutosave = true;
	bool smartErrors = false;
	u8 smartSelfTestCount = 0;
	//Smart

	u8 sceSec[256 * 2] = {0};

public:
	ATA();

	int Open(ghc::filesystem::path hddPath);
	void Close();

	void ATA_HardReset();

	u16 Read16(u32 addr);
	void Write16(u32 addr, u16 value);

	void Async(u32 cycles);

	void ATAreadDMA8Mem(u8* pMem, int size);
	void ATAwriteDMA8Mem(u8* pMem, int size);

	u16 ATAreadPIO();
	//ATAwritePIO;

private:
	//Info
	void CreateHDDinfo(u64 sizeSectors);
	void CreateHDDinfoCsum();

	//State
	void ResetBegin();
	void ResetEnd(bool hard);

	u8 GetSelectedDevice()
	{
		return (regSelect >> 4) & 1;
	}
	void SetSelectedDevice(u8 value)
	{
		if (value == 1)
			regSelect |= (1 << 4);
		else
			regSelect &= ~(1 << 4);
	}

	s64 HDD_GetLBA();
	void HDD_SetLBA(s64 sectorNum);

	bool HDD_CanSeek();
	bool HDD_CanAccess(int* sectors);

	void ClearHOB();

	//Transfer
	void IO_Thread();
	void IO_Read();
	bool IO_Write();
	void HDD_ReadAsync(void (ATA::*drqCMD)());
	void HDD_ReadSync(void (ATA::*drqCMD)());
	bool HDD_CanAssessOrSetError();
	void HDD_SetErrorAtTransferEnd();

	//Commands
	void IDE_ExecCmd(u16 value);

	bool PreCmd();
	void HDD_Unk();

	void IDE_CmdLBA48Transform(bool islba48);

	void DRQCmdDMADataToHost();
	void PostCmdDMADataToHost();
	void DRQCmdDMADataFromHost();
	void PostCmdDMADataFromHost();
	void HDD_ReadDMA(bool isLBA48);
	void HDD_WriteDMA(bool isLBA48);

	void PreCmdExecuteDeviceDiag();
	void PostCmdExecuteDeviceDiag();
	void HDD_ExecuteDeviceDiag();

	void PostCmdNoData();
	void CmdNoDataAbort();
	void HDD_FlushCache();
	void HDD_InitDevParameters();
	void HDD_ReadVerifySectors(bool isLBA48);
	void HDD_SeekCmd();
	void HDD_SetFeatures();
	void HDD_SetMultipleMode();
	void HDD_Nop();
	void HDD_Idle();
	void HDD_IdleImmediate();

	void DRQCmdPIODataToHost(u8* buff, int buffLen, int buffIndex, int size, bool sendIRQ);
	void PostCmdPIODataToHost();
	void HDD_IdentifyDevice();

	void HDD_ReadMultiple(bool isLBA48);
	void HDD_ReadSectors(bool isLBA48);
	void HDD_ReadPIO(bool isLBA48);
	void HDD_ReadPIOS2();
	void HDD_ReadPIOEndBlock();
	//HDD_Write*

	void HDD_Smart();
	void SMART_SetAutoSaveAttribute();
	void SMART_ExecuteOfflineImmediate();
	void SMART_EnableOps(bool enable);
	void SMART_ReturnStatus();

	void HDD_SCE();
	void SCE_IDENTIFY_DRIVE();

	//In here temporally
	static void WriteUInt16(u8* data, int* index, u16 value);
	static void WriteUInt32(u8* data, int* index, u32 value);
	static void WriteUInt64(u8* data, int* index, u64 value);
	static void WritePaddedString(u8* data, int* index, std::string value, u32 len);
};
