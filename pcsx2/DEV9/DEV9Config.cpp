/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/StringUtil.h"
#include "ghc/filesystem.h"
#include <wx/fileconf.h>

#include "DEV9.h"

#ifndef PCSX2_CORE
#include "gui/AppConfig.h"
#include "gui/IniInterface.h"
#endif

#ifdef _WIN32
#include "ws2tcpip.h"
#elif defined(__POSIX__)
#include <arpa/inet.h>
#endif

#if defined(__FreeBSD__)
#include <sys/socket.h>
#endif

void SaveDnsHosts()
{
#ifndef PCSX2_CORE
	std::unique_ptr<wxFileConfig> hini(OpenFileConfig(StringUtil::UTF8StringToWxString(Path::Combine(EmuFolders::Settings, "DEV9Hosts.ini"))));
#else
	std::unique_ptr<wxFileConfig> hini(new wxFileConfig(wxEmptyString, wxEmptyString, EmuFolders::Settings.Combine(wxString("DEV9Hosts.ini")).GetFullPath(), wxEmptyString, wxCONFIG_USE_RELATIVE_PATH));
#endif
	IniSaver ini((wxConfigBase*)hini.get());

	for (size_t i = 0; i < config.EthHosts.size(); i++)
	{
		std::wstring groupName(L"Host" + std::to_wstring(i));
		ScopedIniGroup iniEntry(ini, groupName);
		ConfigHost entry = config.EthHosts[i];

		wxString url(fromUTF8(entry.Url));
		ini.Entry(L"Url", url);
		//TODO UTF8(?)
		wxString desc(fromUTF8(entry.Desc));
		ini.Entry(L"Desc", desc);

		char addrBuff[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, entry.Address, addrBuff, INET_ADDRSTRLEN);
		wxString address(addrBuff);
		ini.Entry(L"Address", address);

		ini.Entry(L"Enabled", entry.Enabled);
	}
	ini.Flush();
}

void LoadDnsHosts()
{
	wxFileName iniPath = StringUtil::UTF8StringToWxString(Path::Combine(EmuFolders::Settings, "DEV9Hosts.ini"));
	config.EthHosts.clear();
	//If no file exists, create one to provide an example config
	if (!iniPath.FileExists())
	{
		//Load Default settings
		ConfigHost exampleHost;
		exampleHost.Url = "www.example.com";
		exampleHost.Desc = "Set DNS to 192.0.2.1 to use this host list";
		memset(exampleHost.Address, 0, 4);
		exampleHost.Enabled = false;
		config.EthHosts.push_back(exampleHost);
		SaveDnsHosts();
		return;
	}

#ifndef PCSX2_CORE
	std::unique_ptr<wxFileConfig> hini(OpenFileConfig(iniPath.GetFullPath()));
#else
	std::unique_ptr<wxFileConfig> hini(new wxFileConfig(wxEmptyString, wxEmptyString, iniPath.GetFullPath(), wxEmptyString, wxCONFIG_USE_RELATIVE_PATH));
#endif
	IniLoader ini((wxConfigBase*)hini.get());

	int i = 0;
	while (true)
	{
		std::wstring groupName(L"Host" + std::to_wstring(i));
		ScopedIniGroup iniEntry(ini, groupName);
		wxString tmp = wxEmptyString;
		ini.Entry(L"Url", tmp, wxEmptyString);
		//An empty url means we tried to read beyond end of the host list
		if (tmp.IsEmpty())
			break;

		ConfigHost entry;
		entry.Url = tmp.ToUTF8();

		ini.Entry(L"Desc", tmp, wxEmptyString);
		entry.Desc = tmp.ToUTF8();

		ini.Entry(L"Address", tmp, L"0.0.0.0");

		int ret = inet_pton(AF_INET, tmp.ToUTF8(), entry.Address);
		//Only check Enabled if valid ip
		if (ret != 1)
		{
			memset(entry.Address, 0, 4);
			entry.Enabled = false;
		}
		else
			ini.Entry(L"Enabled", entry.Enabled, false);

		if (EmuConfig.DEV9.EthLogDNS && entry.Enabled)
			Console.WriteLn("DEV9: Host entry %i: url %s mapped to %s", i, entry.Url.c_str(), tmp.ToStdString().c_str());

		config.EthHosts.push_back(entry);
		i++;
	}
}
