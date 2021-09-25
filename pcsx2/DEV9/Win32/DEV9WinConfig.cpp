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
#include <stdlib.h>

#include <fstream>

#include "DEV9/DEV9.h"
#include "DEV9/Config.h"
#include "Config.h"

#include "ws2tcpip.h"

BOOL WritePrivateProfileInt(LPCWSTR lpAppName, LPCWSTR lpKeyName, int intvar, LPCWSTR lpFileName)
{
	return WritePrivateProfileString(lpAppName, lpKeyName, std::to_wstring(intvar).c_str(), lpFileName);
}
bool FileExists(std::wstring szPath)
{
	DWORD dwAttrib = GetFileAttributes(szPath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SaveConf()
{
	const std::wstring file(EmuFolders::Settings.Combine(wxString("DEV9.cfg")).GetFullPath());
	DeleteFile(file.c_str());

	//Create file with UT16 BOM to allow PrivateProfile to save unicode data
	int bom = 0xFEFF;
	std::fstream nfile = std::fstream(file, std::ios::out | std::ios::binary);
	nfile.write((char*)&bom, 2);
	//Write header to avoid empty line
	nfile.write((char*)L"[DEV9]", 14);
	nfile.close();

	wchar_t addrBuff[INET_ADDRSTRLEN] = {0};
	wchar_t wEth[sizeof(config.Eth)] = {0};
	mbstowcs(wEth, config.Eth, sizeof(config.Eth) - 1);
	WritePrivateProfileString(L"DEV9", L"Eth", wEth, file.c_str());

	WritePrivateProfileInt(L"DEV9", L"EthApi", (int)config.EthApi, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"InterceptDHCP", config.InterceptDHCP, file.c_str());

	InetNtop(AF_INET, &config.PS2IP, addrBuff, INET_ADDRSTRLEN);
	WritePrivateProfileString(L"DEV9", L"PS2IP", addrBuff, file.c_str());

	InetNtop(AF_INET, &config.Mask, addrBuff, INET_ADDRSTRLEN);
	WritePrivateProfileString(L"DEV9", L"Subnet", addrBuff, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"AutoSubnet", config.AutoMask, file.c_str());

	InetNtop(AF_INET, &config.Gateway, addrBuff, INET_ADDRSTRLEN);
	WritePrivateProfileString(L"DEV9", L"Gateway", addrBuff, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"AutoGateway", config.AutoGateway, file.c_str());

	InetNtop(AF_INET, &config.DNS1, addrBuff, INET_ADDRSTRLEN);
	WritePrivateProfileString(L"DEV9", L"DNS1", addrBuff, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"AutoDNS1", config.AutoDNS1, file.c_str());

	InetNtop(AF_INET, &config.DNS2, addrBuff, INET_ADDRSTRLEN);
	WritePrivateProfileString(L"DEV9", L"DNS2", addrBuff, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"AutoDNS2", config.AutoDNS2, file.c_str());

	WritePrivateProfileInt(L"DEV9", L"EthLogDNS", config.EthLogDNS, file.c_str());

	WritePrivateProfileString(L"DEV9", L"Hdd", config.Hdd, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"HddSize", config.HddSize, file.c_str());

	WritePrivateProfileInt(L"DEV9", L"ethEnable", config.ethEnable, file.c_str());
	WritePrivateProfileInt(L"DEV9", L"hddEnable", config.hddEnable, file.c_str());

	SaveDnsHosts();
}

void LoadConf()
{
	const std::wstring file(EmuFolders::Settings.Combine(wxString("DEV9.cfg")).GetFullPath());
	if (FileExists(file.c_str()) == false)
	{
		LoadDnsHosts();
		return;
	}

	wchar_t addrBuff[INET_ADDRSTRLEN] = {0};
	wchar_t wEth[sizeof(config.Eth)] = {0};
	mbstowcs(wEth, ETH_DEF, sizeof(config.Eth) - 1);
	GetPrivateProfileString(L"DEV9", L"Eth", wEth, wEth, sizeof(config.Eth), file.c_str());
	wcstombs(config.Eth, wEth, sizeof(config.Eth) - 1);

	config.EthApi = (NetApi)GetPrivateProfileInt(L"DEV9", L"EthApi", (int)NetApi::TAP, file.c_str());
	config.InterceptDHCP = GetPrivateProfileInt(L"DEV9", L"InterceptDHCP", config.InterceptDHCP, file.c_str());

	GetPrivateProfileString(L"DEV9", L"PS2IP", L"0.0.0.0", addrBuff, INET_ADDRSTRLEN, file.c_str());
	InetPton(AF_INET, addrBuff, &config.PS2IP);

	GetPrivateProfileString(L"DEV9", L"Subnet", L"0.0.0.0", addrBuff, INET_ADDRSTRLEN, file.c_str());
	InetPton(AF_INET, addrBuff, &config.Mask);
	config.AutoMask = GetPrivateProfileInt(L"DEV9", L"AutoSubnet", config.AutoMask, file.c_str());

	GetPrivateProfileString(L"DEV9", L"Gateway", L"0.0.0.0", addrBuff, INET_ADDRSTRLEN, file.c_str());
	InetPton(AF_INET, addrBuff, &config.Gateway);
	config.AutoGateway = GetPrivateProfileInt(L"DEV9", L"AutoGateway", config.AutoGateway, file.c_str());

	GetPrivateProfileString(L"DEV9", L"DNS1", L"0.0.0.0", addrBuff, INET_ADDRSTRLEN, file.c_str());
	InetPton(AF_INET, addrBuff, &config.DNS1);
	config.AutoDNS1 = GetPrivateProfileInt(L"DEV9", L"AutoDNS1", config.AutoDNS1, file.c_str());

	GetPrivateProfileString(L"DEV9", L"DNS2", L"0.0.0.0", addrBuff, INET_ADDRSTRLEN, file.c_str());
	InetPton(AF_INET, addrBuff, &config.DNS2);
	config.AutoDNS2 = GetPrivateProfileInt(L"DEV9", L"AutoDNS2", config.AutoDNS2, file.c_str());

	config.EthLogDNS = GetPrivateProfileInt(L"DEV9", L"EthLogDNS", config.EthLogDNS, file.c_str());

	GetPrivateProfileString(L"DEV9", L"Hdd", HDD_DEF, config.Hdd, sizeof(config.Hdd), file.c_str());
	config.HddSize = GetPrivateProfileInt(L"DEV9", L"HddSize", config.HddSize, file.c_str());

	config.ethEnable = GetPrivateProfileInt(L"DEV9", L"ethEnable", config.ethEnable, file.c_str());
	config.hddEnable = GetPrivateProfileInt(L"DEV9", L"hddEnable", config.hddEnable, file.c_str());

	LoadDnsHosts();
}
