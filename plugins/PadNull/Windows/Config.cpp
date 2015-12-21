/*  PadNull
 *  Copyright (C) 2004-2010  PCSX2 Dev Team
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

#include "../Pad.h"

extern std::string s_strIniPath;

void SaveConfig()
{
	const std::string iniFile = s_strIniPath + "/Padnull.ini";

	PluginConf ini;
	if (!ini.Open(iniFile, READ_FILE))
	{
		printf("failed to open %s\n", iniFile.c_str());
		SaveConfig();//save and return
		return;
	}
	conf.Log = ini.ReadInt("logging", 0);
	ini.Close();
}

void LoadConfig()
{
	const std::string iniFile(s_strIniPath + "/Padnull.ini");

	PluginConf ini;
	if (!ini.Open(iniFile, WRITE_FILE))
	{
		printf("failed to open %s\n", iniFile.c_str());
		return;
	}
	ini.WriteInt("logging", conf.Log);
	ini.Close();
}
