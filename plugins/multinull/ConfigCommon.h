/*  Multinull
 *  Copyright (C) 2004-2015  PCSX2 Dev Team
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

#include "PS2Eext.h"

struct ConfigLogCombination
{
	PluginConf Config;
	PluginLog Log;
	int LogEnabled;
	std::string IniPath;
	std::string LogPath;
	std::string Name;

	void SetName(std::string nameToSetTo) { Name = nameToSetTo; }

	void SetLoggingState()
	{
		if (LogEnabled)
		{
			Log.WriteToConsole = true;
			Log.WriteToFile = true;
		}
		else
		{
			Log.WriteToConsole = false;
			Log.WriteToFile = false;
		}
	}

	void InitLog()
	{
		if (LogPath == "")
		{
			SetLoggingFolder("logs");
		}
		LoadConfig();
		SetLoggingState();
		Log.Open(LogPath + "/" + Name + ".log");
	}

	void ReloadLog()
	{
		Log.Close();
		InitLog();
	}

	void SetConfigFolder(const char *configFolder) { IniPath = (configFolder == NULL) ? "inis" : configFolder; }

	void SetLoggingFolder(const char *logFolder) { LogPath = (logFolder == NULL) ? "logs" : logFolder; }

	void LoadConfig()
	{
		if (IniPath == "")
		{
			SetConfigFolder("inis");
		}
		std::string IniFile = LogPath + "/" + Name + ".ini";
		if (!Config.Open(IniFile, READ_FILE))
		{
			Log.WriteLn("Failed to open %s", IniFile.c_str());
			SaveConfig();
			return;
		}

		LogEnabled = Config.ReadInt("logging", 0);
		Config.Close();
	}

	void SaveConfig()
	{
		if (IniPath == "")
		{
			SetConfigFolder("inis");
		}
		std::string IniFile = LogPath + "/" + Name + ".ini";
		if (!Config.Open(IniFile, WRITE_FILE))
		{
			Log.WriteLn("Failed to open %s", IniFile.c_str());
			return;
		}

		Config.WriteInt("logging", LogEnabled);
		Config.Close();
	}

	void ConfigureGUI()
	{
		LoadConfig();
		PluginNullConfigure("MultiNull logging", LogEnabled);
		SaveConfig();
	}

	void AboutGUI() { SysMessage("MultiNull"); }
};
