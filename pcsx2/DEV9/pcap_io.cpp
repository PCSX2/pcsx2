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
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
//#include <ws2tcpip.h>
//#include <comdef.h>
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <net/if.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include "pcap_io.h"
#include "DEV9.h"
#include "net.h"
#ifndef PCAP_NETMASK_UNKNOWN
#define PCAP_NETMASK_UNKNOWN 0xffffffff
#endif

pcap_t* adhandle;
pcap_dumper_t* dump_pcap = nullptr;
char errbuf[PCAP_ERRBUF_SIZE];

int pcap_io_running = 0;
bool pcap_io_switched;
bool pcap_io_blocking;

extern u8 eeprom[];

char namebuff[256];

ip_address ps2_ip;
mac_address ps2_mac;
mac_address host_mac;

#ifdef _WIN32
//IP_ADAPTER_ADDRESSES is a structure that contains ptrs to data in other regions
//of the buffer, se we need to return both so the caller can free the buffer
//after it's finished reading the needed data from IP_ADAPTER_ADDRESSES
bool PCAPGetWin32Adapter(const char* name, PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer)
{
	const int guidindex = strlen("\\Device\\NPF_");

	int neededSize = 128;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	ULONG dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	PIP_ADAPTER_ADDRESSES pAdapterInfo;

	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
		NULL,
		AdapterInfo.get(),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: GetWin32Adapter() buffer too small, resizing");
		neededSize = dwBufLen / sizeof(IP_ADAPTER_ADDRESSES) + 1;
		AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
		dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
			NULL,
			AdapterInfo.get(),
			&dwBufLen);
	}
	if (dwStatus != ERROR_SUCCESS)
		return false;

	pAdapterInfo = AdapterInfo.get();

	do
	{
		if (0 == strcmp(pAdapterInfo->AdapterName, &name[guidindex]))
		{
			*adapter = *pAdapterInfo;
			buffer->swap(AdapterInfo);
			return true;
		}

		pAdapterInfo = pAdapterInfo->Next;
	} while (pAdapterInfo);
	return false;
}
#elif defined(__POSIX__)
//getifaddrs is not POSIX, but is supported on MAC & Linux
bool PCAPGetIfAdapter(char* name, ifaddrs* adapter, ifaddrs** buffer)
{
	//Note, we don't support "any" adapter, but that also fails in pcap_io_init()
	ifaddrs* adapterInfo;
	ifaddrs* pAdapter;

	int error = getifaddrs(&adapterInfo);
	if (error)
		return false;

	pAdapter = adapterInfo;

	do
	{
		if (pAdapter->ifa_addr->sa_family == AF_INET && strcmp(pAdapter->ifa_name, name) == 0)
			break;

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	if (pAdapter != nullptr)
	{
		*adapter = *pAdapter;
		*buffer = adapterInfo;
		return true;
	}

	freeifaddrs(adapterInfo);
	return false;
}
#endif

// Fetches the MAC address and prints it
int GetMACAddress(char* adapter, mac_address* addr)
{
	int retval = 0;
#ifdef _WIN32
	IP_ADAPTER_ADDRESSES adapterInfo;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;

	if (PCAPGetWin32Adapter(adapter, &adapterInfo, &buffer))
	{
		memcpy(addr, adapterInfo.PhysicalAddress, 6);
		retval = 1;
	}

#elif defined(__linux__)
	struct ifreq ifr;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, adapter);
	if (0 == ioctl(fd, SIOCGIFHWADDR, &ifr))
	{
		retval = 1;
		memcpy(addr, ifr.ifr_hwaddr.sa_data, 6);
	}
	else
	{
		Console.Error("Could not get MAC address for adapter: %s", adapter);
	}
	close(fd);
#else
	Console.Error("Could not get MAC address for adapter, OS not supported");
#endif
	return retval;
}

int pcap_io_init(char* adapter, bool switched, mac_address virtual_mac)
{
	struct bpf_program fp;
	char filter[1024] = "ether broadcast or ether dst ";
	int dlt;
	char* dlt_name;
	Console.WriteLn("DEV9: Opening adapter '%s'...", adapter);

	pcap_io_switched = switched;

	/* Open the adapter */
	if ((adhandle = pcap_open_live(adapter, // name of the device
			 65536, // portion of the packet to capture.
			 // 65536 grants that the whole packet will be captured on all the MACs.
			 switched ? 1 : 0,
			 1, // read timeout
			 errbuf // error buffer
			 )) == NULL)
	{
		Console.Error("DEV9: %s", errbuf);
		Console.Error("DEV9: Unable to open the adapter. %s is not supported by pcap", adapter);
		return -1;
	}
	if (switched)
	{
		char virtual_mac_str[18];
		sprintf(virtual_mac_str, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", virtual_mac.bytes[0], virtual_mac.bytes[1], virtual_mac.bytes[2], virtual_mac.bytes[3], virtual_mac.bytes[4], virtual_mac.bytes[5]);
		strcat(filter, virtual_mac_str);
		//	fprintf(stderr, "Trying pcap filter: %s\n", filter);

		if (pcap_compile(adhandle, &fp, filter, 1, PCAP_NETMASK_UNKNOWN) == -1)
		{
			Console.Error("DEV9: Error calling pcap_compile: %s", pcap_geterr(adhandle));
			return -1;
		}

		if (pcap_setfilter(adhandle, &fp) == -1)
		{
			Console.Error("DEV9: Error setting filter: %s", pcap_geterr(adhandle));
			return -1;
		}
	}

	if (pcap_setnonblock(adhandle, 1, errbuf) == -1)
	{
		Console.Error("DEV9: Error setting non-blocking: %s", pcap_geterr(adhandle));
		Console.Error("DEV9: Continuing in blocking mode");
		pcap_io_blocking = true;
	}
	else
		pcap_io_blocking = false;

	dlt = pcap_datalink(adhandle);
	dlt_name = (char*)pcap_datalink_val_to_name(dlt);

	Console.Error("DEV9: Device uses DLT %d: %s", dlt, dlt_name);
	switch (dlt)
	{
		case DLT_EN10MB:
			//case DLT_IEEE802_11:
			break;
		default:
			Console.Error("ERROR: Unsupported DataLink Type (%d): %s", dlt, dlt_name);
			pcap_close(adhandle);
			return -1;
	}

#ifdef DEBUG
	const std::string plfile(s_strLogPath + "/pkt_log.pcap");
	dump_pcap = pcap_dump_open(adhandle, plfile.c_str());
#endif

	pcap_io_running = 1;
	Console.WriteLn("DEV9: Adapter Ok.");
	return 0;
}

#ifdef _WIN32
int gettimeofday(struct timeval* tv, void* tz)
{
	unsigned __int64 ns100; /*time since 1 Jan 1601 in 100ns units */

	GetSystemTimeAsFileTime((LPFILETIME)&ns100);
	tv->tv_usec = (long)((ns100 / 10L) % 1000000L);
	tv->tv_sec = (long)((ns100 - 116444736000000000L) / 10000000L);
	return (0);
}
#endif

int pcap_io_send(void* packet, int plen)
{
	if (pcap_io_running <= 0)
		return -1;

	if (!pcap_io_switched)
	{
		if (((ethernet_header*)packet)->protocol == 0x0008) //IP
		{
			ps2_ip = ((ip_header*)((u8*)packet + sizeof(ethernet_header)))->src;
		}
		if (((ethernet_header*)packet)->protocol == 0x0608) //ARP
		{
			ps2_ip = ((arp_packet*)((u8*)packet + sizeof(ethernet_header)))->p_src;
			((arp_packet*)((u8*)packet + sizeof(ethernet_header)))->h_src = host_mac;
		}
		((ethernet_header*)packet)->src = host_mac;
	}

	if (dump_pcap)
	{
		static struct pcap_pkthdr ph;
		gettimeofday(&ph.ts, NULL);
		ph.caplen = plen;
		ph.len = plen;
		pcap_dump((u_char*)dump_pcap, &ph, (u_char*)packet);
	}

	return pcap_sendpacket(adhandle, (u_char*)packet, plen);
}

int pcap_io_recv(void* packet, int max_len)
{
	static struct pcap_pkthdr* header;
	static const u_char* pkt_data1;

	if (pcap_io_running <= 0)
		return -1;

	if ((pcap_next_ex(adhandle, &header, &pkt_data1)) > 0)
	{
		if ((int)header->len > max_len)
			return -1;

		memcpy(packet, pkt_data1, header->len);

		if (!pcap_io_switched)
		{
			{
				if (((ethernet_header*)packet)->protocol == 0x0008)
				{
					ip_header* iph = ((ip_header*)((u8*)packet + sizeof(ethernet_header)));
					if (ip_compare(iph->dst, ps2_ip) == 0)
					{
						((ethernet_header*)packet)->dst = ps2_mac;
					}
				}
				if (((ethernet_header*)packet)->protocol == 0x0608)
				{
					arp_packet* aph = ((arp_packet*)((u8*)packet + sizeof(ethernet_header)));
					if (ip_compare(aph->p_dst, ps2_ip) == 0)
					{
						((ethernet_header*)packet)->dst = ps2_mac;
						((arp_packet*)((u8*)packet + sizeof(ethernet_header)))->h_dst = ps2_mac;
					}
				}
			}
		}

		if (dump_pcap)
			pcap_dump((u_char*)dump_pcap, header, (u_char*)packet);

		return header->len;
	}

	return -1;
}

void pcap_io_close()
{
	if (dump_pcap)
		pcap_dump_close(dump_pcap);
	if (adhandle)
		pcap_close(adhandle);
	pcap_io_running = 0;
}


PCAPAdapter::PCAPAdapter()
	: NetAdapter()
{
	if (config.ethEnable == 0)
		return;

#ifdef _WIN32
	if (!load_pcap())
		return;
#endif

	mac_address hostMAC;
	mac_address newMAC;

	GetMACAddress(config.Eth, &hostMAC);
	memcpy(&newMAC, ps2MAC, 6);

	//Lets take the hosts last 2 bytes to make it unique on Xlink
	newMAC.bytes[5] = hostMAC.bytes[4];
	newMAC.bytes[4] = hostMAC.bytes[5];

	SetMACAddress((u8*)&newMAC);
	host_mac = hostMAC;
	ps2_mac = newMAC; //Needed outside of this class

	if (pcap_io_init(config.Eth, config.EthApi == NetApi::PCAP_Switched, newMAC) == -1)
	{
		Console.Error("DEV9: Can't open Device '%s'", config.Eth);
		return;
	}

#ifdef _WIN32
	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;
	if (PCAPGetWin32Adapter(config.Eth, &adapter, &buffer))
		InitInternalServer(&adapter);
	else
	{
		Console.Error("DEV9: Failed to get adapter information");
		InitInternalServer(nullptr);
	}
#elif defined(__POSIX__)
	ifaddrs adapter;
	ifaddrs* buffer;
	if (PCAPGetIfAdapter(config.Eth, &adapter, &buffer))
	{
		InitInternalServer(&adapter);
		freeifaddrs(buffer);
	}
	else
	{
		Console.Error("DEV9: Failed to get adapter information");
		InitInternalServer(nullptr);
	}
#endif
}
bool PCAPAdapter::blocks()
{
	pxAssert(pcap_io_running);
	return pcap_io_blocking;
}
bool PCAPAdapter::isInitialised()
{
	return !!pcap_io_running;
}
//gets a packet.rv :true success
bool PCAPAdapter::recv(NetPacket* pkt)
{
	if (!pcap_io_blocking && NetAdapter::recv(pkt))
		return true;

	int size = pcap_io_recv(pkt->buffer, sizeof(pkt->buffer));
	if (size > 0 && VerifyPkt(pkt, size))
	{
		InspectRecv(pkt);
		return true;
	}
	else
		return false;
}
//sends the packet .rv :true success
bool PCAPAdapter::send(NetPacket* pkt)
{
	InspectSend(pkt);
	if (NetAdapter::send(pkt))
		return true;

	if (pcap_io_send(pkt->buffer, pkt->size))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void PCAPAdapter::reloadSettings()
{
#ifdef _WIN32
	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;
	if (PCAPGetWin32Adapter(config.Eth, &adapter, &buffer))
		ReloadInternalServer(&adapter);
	else
		ReloadInternalServer(nullptr);
#elif defined(__POSIX__)
	ifaddrs adapter;
	ifaddrs* buffer;
	if (PCAPGetIfAdapter(config.Eth, &adapter, &buffer))
	{
		ReloadInternalServer(&adapter);
		freeifaddrs(buffer);
	}
	else
		ReloadInternalServer(nullptr);
#endif
}

PCAPAdapter::~PCAPAdapter()
{
	pcap_io_close();
}

std::vector<AdapterEntry> PCAPAdapter::GetAdapters()
{
	std::vector<AdapterEntry> nic;

#ifdef _WIN32
	if (!load_pcap())
		return nic;
#endif

	pcap_if_t* alldevs;
	pcap_if_t* d;

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		return nic;
	}

	d = alldevs;
	while (d != NULL)
	{
		AdapterEntry entry;
		entry.type = NetApi::PCAP_Switched;
#ifdef _WIN32
		//guid
		wchar_t wEth[sizeof(config.Eth)] = {0};
		mbstowcs(wEth, d->name, sizeof(config.Eth) - 1);
		entry.guid = std::wstring(wEth);

		IP_ADAPTER_ADDRESSES adapterInfo;
		std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;

		if (PCAPGetWin32Adapter(d->name, &adapterInfo, &buffer))
			entry.name = std::wstring(adapterInfo.FriendlyName);
		else
		{
			//have to use description
			//NPCAP 1.10 is using an version of pcap that dosn't
			//allow us to set it to use UTF8
			//see https://github.com/nmap/npcap/issues/276
			//But use MultiByteToWideChar so we can later switch to UTF8
			int len_desc = strlen(d->description) + 1;
			int len_buf = MultiByteToWideChar(CP_ACP, 0, d->description, len_desc, nullptr, 0);

			wchar_t* buf = new wchar_t[len_buf];
			MultiByteToWideChar(CP_ACP, 0, d->description, len_desc, buf, len_buf);
			entry.name = std::wstring(buf);
			delete[] buf;
		}
#else
		entry.name = std::string(d->name);
		entry.guid = std::string(d->name);
#endif

		nic.push_back(entry);
		entry.type = NetApi::PCAP_Bridged;
		nic.push_back(entry);
		d = d->next;
	}

	return nic;
}
