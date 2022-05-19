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
#include "SPU2/Global.h"
#include "Dialogs.h"
#include "Config.h"
#include "pcsx2/Config.h"
#include "gui/StringHelpers.h"
#include "gui/wxDirName.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "common/Path.h"

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

// this is set true if PCSX2 invokes the SetLogDir callback, which tells SPU2 to use that over
// the configured crap in the ini file.
static bool LogLocationSetByPcsx2 = false;

static wxDirName LogsFolder;
static wxDirName DumpsFolder;

std::string AccessLogFileName;
std::string WaveLogFileName;
std::string DMA4LogFileName;
std::string DMA7LogFileName;

std::string CoresDumpFileName;
std::string MemDumpFileName;
std::string RegDumpFileName;

void CfgSetLogDir(const char* dir)
{
	LogsFolder = (dir == nullptr) ? wxString(L"logs") : wxString(dir, wxConvFile);
	DumpsFolder = (dir == nullptr) ? wxString(L"logs") : wxString(dir, wxConvFile);
	LogLocationSetByPcsx2 = (dir != nullptr);
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
	static const wchar_t* Section = L"DEBUG";

	static void set_default_filenames()
	{
		AccessLogFileName = "SPU2Log.txt";
		WaveLogFileName = "SPU2log.wav";
		DMA4LogFileName = "SPU2dma4.dat";
		DMA7LogFileName = "SPU2dma7.dat";

		CoresDumpFileName = "SPU2Cores.txt";
		MemDumpFileName = "SPU2mem.dat";
		RegDumpFileName = "SPU2regs.dat";
	}

	void ReadSettings()
	{
		DebugEnabled = CfgReadBool(Section, L"Global_Enable", 0);
		_MsgToConsole = CfgReadBool(Section, L"Show_Messages", 0);
		_MsgKeyOnOff = CfgReadBool(Section, L"Show_Messages_Key_On_Off", 0);
		_MsgVoiceOff = CfgReadBool(Section, L"Show_Messages_Voice_Off", 0);
		_MsgDMA = CfgReadBool(Section, L"Show_Messages_DMA_Transfer", 0);
		_MsgAutoDMA = CfgReadBool(Section, L"Show_Messages_AutoDMA", 0);
		_MsgOverruns = CfgReadBool(Section, L"Show_Messages_Overruns", 0);
		_MsgCache = CfgReadBool(Section, L"Show_Messages_CacheStats", 0);

		_AccessLog = CfgReadBool(Section, L"Log_Register_Access", 0);
		_DMALog = CfgReadBool(Section, L"Log_DMA_Transfers", 0);
		_WaveLog = CfgReadBool(Section, L"Log_WAVE_Output", 0);

		_CoresDump = CfgReadBool(Section, L"Dump_Info", 0);
		_MemDump = CfgReadBool(Section, L"Dump_Memory", 0);
		_RegDump = CfgReadBool(Section, L"Dump_Regs", 0);

		set_default_filenames();

		wxString wxAccessLogFileName;
		wxString wxWaveLogFileName;
		wxString wxDMA4LogFileName;
		wxString wxDMA7LogFileName;
		wxString wxCoresDumpFileName;
		wxString wxMemDumpFileName;
		wxString wxRegDumpFileName;
		CfgReadStr(Section, L"Access_Log_Filename", wxAccessLogFileName, L"SPU2Log.txt");
		CfgReadStr(Section, L"DMA4Log_Filename", wxDMA4LogFileName, L"SPU2dma4.dat");
		CfgReadStr(Section, L"WaveLog_Filename", wxWaveLogFileName, L"logs/SPU2log.wav");
		CfgReadStr(Section, L"DMA7Log_Filename", wxDMA7LogFileName, L"SPU2dma7.dat");

		CfgReadStr(Section, L"Info_Dump_Filename", wxCoresDumpFileName, L"SPU2Cores.txt");
		CfgReadStr(Section, L"Mem_Dump_Filename", wxMemDumpFileName, L"SPU2mem.dat");
		CfgReadStr(Section, L"Reg_Dump_Filename", wxRegDumpFileName, L"SPU2regs.dat");

		AccessLogFileName = StringUtil::wxStringToUTF8String(wxAccessLogFileName);
		WaveLogFileName  = StringUtil::wxStringToUTF8String(wxAccessLogFileName);
		DMA4LogFileName = StringUtil::wxStringToUTF8String(wxDMA4LogFileName);
		DMA7LogFileName = StringUtil::wxStringToUTF8String(wxDMA7LogFileName);
		CoresDumpFileName = StringUtil::wxStringToUTF8String(wxCoresDumpFileName);
		MemDumpFileName = StringUtil::wxStringToUTF8String(wxMemDumpFileName);
		RegDumpFileName = StringUtil::wxStringToUTF8String(wxRegDumpFileName);
	}


	void WriteSettings()
	{
		CfgWriteBool(Section, L"Global_Enable", DebugEnabled);

		CfgWriteBool(Section, L"Show_Messages", _MsgToConsole);
		CfgWriteBool(Section, L"Show_Messages_Key_On_Off", _MsgKeyOnOff);
		CfgWriteBool(Section, L"Show_Messages_Voice_Off", _MsgVoiceOff);
		CfgWriteBool(Section, L"Show_Messages_DMA_Transfer", _MsgDMA);
		CfgWriteBool(Section, L"Show_Messages_AutoDMA", _MsgAutoDMA);
		CfgWriteBool(Section, L"Show_Messages_Overruns", _MsgOverruns);
		CfgWriteBool(Section, L"Show_Messages_CacheStats", _MsgCache);

		CfgWriteBool(Section, L"Log_Register_Access", _AccessLog);
		CfgWriteBool(Section, L"Log_DMA_Transfers", _DMALog);
		CfgWriteBool(Section, L"Log_WAVE_Output", _WaveLog);

		CfgWriteBool(Section, L"Dump_Info", _CoresDump);
		CfgWriteBool(Section, L"Dump_Memory", _MemDump);
		CfgWriteBool(Section, L"Dump_Regs", _RegDump);

		set_default_filenames();
		CfgWriteStr(Section, L"Access_Log_Filename", StringUtil::UTF8StringToWxString(AccessLogFileName));
		CfgWriteStr(Section, L"WaveLog_Filename", StringUtil::UTF8StringToWxString(WaveLogFileName));
		CfgWriteStr(Section, L"DMA4Log_Filename", StringUtil::UTF8StringToWxString(DMA4LogFileName));
		CfgWriteStr(Section, L"DMA7Log_Filename", StringUtil::UTF8StringToWxString(DMA7LogFileName));

		CfgWriteStr(Section, L"Info_Dump_Filename", StringUtil::UTF8StringToWxString(CoresDumpFileName));
		CfgWriteStr(Section, L"Mem_Dump_Filename", StringUtil::UTF8StringToWxString(MemDumpFileName));
		CfgWriteStr(Section, L"Reg_Dump_Filename", StringUtil::UTF8StringToWxString(RegDumpFileName));
	}

} // namespace DebugConfig
