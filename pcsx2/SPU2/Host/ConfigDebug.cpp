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

#include "pcsx2/Config.h"

#include "SPU2/Global.h"
#include "SPU2/Host/Dialogs.h"
#include "SPU2/Host/Config.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "HostSettings.h"

bool DebugEnabled = false;
bool _MsgToConsole = false;
bool _MsgKeyOnOff = false;
bool _MsgVoiceOff = false;
bool _MsgDMA = false;
bool _MsgAutoDMA = false;
bool _MsgOverruns = false;
bool _MsgCache = false;

bool _AccessLog = false;
bool _DMALog = false;
bool _WaveLog = false;

bool _CoresDump = false;
bool _MemDump = false;
bool _RegDump = false;

std::string AccessLogFileName;
std::string WaveLogFileName;
std::string DMA4LogFileName;
std::string DMA7LogFileName;

std::string CoresDumpFileName;
std::string MemDumpFileName;
std::string RegDumpFileName;

void CfgSetSettingsDir(const char* dir)
{
}

void CfgSetLogDir(const char* dir)
{
}

FILE* OpenBinaryLog(const char* logfile)
{
	return FileSystem::OpenCFile(Path::Combine(EmuFolders::Logs, logfile).c_str(), "wb");
}

FILE* OpenLog(const char* logfile)
{
	return FileSystem::OpenCFile(Path::Combine(EmuFolders::Logs, logfile).c_str(), "w");
}

FILE* OpenDump(const char* logfile)
{
	return FileSystem::OpenCFile(Path::Combine(EmuFolders::Logs, logfile).c_str(), "w");
}

namespace DebugConfig
{
	static const char* Section = "SPU/Debug";

	void ReadSettings()
	{
		DebugEnabled = Host::GetBoolSettingValue(Section, "Global_Enable", 0);
		_MsgToConsole = Host::GetBoolSettingValue(Section, "Show_Messages", 0);
		_MsgKeyOnOff = Host::GetBoolSettingValue(Section, "Show_Messages_Key_On_Off", 0);
		_MsgVoiceOff = Host::GetBoolSettingValue(Section, "Show_Messages_Voice_Off", 0);
		_MsgDMA = Host::GetBoolSettingValue(Section, "Show_Messages_DMA_Transfer", 0);
		_MsgAutoDMA = Host::GetBoolSettingValue(Section, "Show_Messages_AutoDMA", 0);
		_MsgOverruns = Host::GetBoolSettingValue(Section, "Show_Messages_Overruns", 0);
		_MsgCache = Host::GetBoolSettingValue(Section, "Show_Messages_CacheStats", 0);

		_AccessLog = Host::GetBoolSettingValue(Section, "Log_Register_Access", 0);
		_DMALog = Host::GetBoolSettingValue(Section, "Log_DMA_Transfers", 0);
		_WaveLog = Host::GetBoolSettingValue(Section, "Log_WAVE_Output", 0);

		_CoresDump = Host::GetBoolSettingValue(Section, "Dump_Info", 0);
		_MemDump = Host::GetBoolSettingValue(Section, "Dump_Memory", 0);
		_RegDump = Host::GetBoolSettingValue(Section, "Dump_Regs", 0);

		AccessLogFileName = Host::GetStringSettingValue(Section, "Access_Log_Filename", "SPU2Log.txt");
		WaveLogFileName = Host::GetStringSettingValue(Section, "WaveLog_Filename", "SPU2log.wav");
		DMA4LogFileName = Host::GetStringSettingValue(Section, "DMA4Log_Filename", "SPU2dma4.dat");
		DMA7LogFileName = Host::GetStringSettingValue(Section, "DMA7Log_Filename", "SPU2dma7.dat");

		CoresDumpFileName = Host::GetStringSettingValue(Section, "Info_Dump_Filename", "SPU2Cores.txt");
		MemDumpFileName = Host::GetStringSettingValue(Section, "Mem_Dump_Filename", "SPU2mem.dat");
		RegDumpFileName = Host::GetStringSettingValue(Section, "Reg_Dump_Filename", "SPU2regs.dat");
	}
} // namespace DebugConfig
