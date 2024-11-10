// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Assertions.h"
#include <algorithm>
#include <memory>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include "common/StringUtil.h"
#include <WinSock2.h>
#include <iphlpapi.h>
#endif

#include <stdio.h>
#include "pcap_io.h"
#include "DEV9.h"
#include "AdapterUtils.h"
#include "net.h"
#include "PacketReader/EthernetFrameEditor.h"
#include "PacketReader/ARP/ARP_Packet.h"
#include "PacketReader/ARP/ARP_PacketEditor.h"
#ifndef PCAP_NETMASK_UNKNOWN
#define PCAP_NETMASK_UNKNOWN 0xffffffff
#endif

#ifdef _WIN32
#define PCAPPREFIX "\\Device\\NPF_"
#endif

using namespace PacketReader;
using namespace PacketReader::ARP;
using namespace PacketReader::IP;

PCAPAdapter::PCAPAdapter()
	: NetAdapter()
{
	if (!EmuConfig.DEV9.EthEnable)
		return;
#ifdef _WIN32
	if (!load_pcap())
		return;
#endif

#ifdef _WIN32
	std::string pcapAdapter = PCAPPREFIX + EmuConfig.DEV9.EthDevice;
#else
	std::string pcapAdapter = EmuConfig.DEV9.EthDevice;
#endif

	switched = EmuConfig.DEV9.EthApi == Pcsx2Config::DEV9Options::NetApi::PCAP_Switched;

	if (!InitPCAP(pcapAdapter, switched))
	{
		Console.Error("DEV9: PCAP: Can't open Device '%s'", EmuConfig.DEV9.EthDevice.c_str());
		return;
	}

	AdapterUtils::Adapter adapter;
	AdapterUtils::AdapterBuffer buffer;
	std::optional<MAC_Address> adMAC = std::nullopt;
	const bool foundAdapter = AdapterUtils::GetAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);
	if (foundAdapter)
		adMAC = AdapterUtils::GetAdapterMAC(&adapter);
	else
		Console.Error("DEV9: PCAP: Failed to get adapter information");

	// DLT_RAW adapters may not have a MAC address
	// Just use the default MAC in such case
	// SetMACSwitchedFilter will also fail on such adapters
	if (!ipOnly)
	{
		if (adMAC.has_value())
		{
			hostMAC = adMAC.value();
			MAC_Address newMAC = ps2MAC;

			//Lets take the hosts last 2 bytes to make it unique on Xlink
			newMAC.bytes[5] = hostMAC.bytes[4];
			newMAC.bytes[4] = hostMAC.bytes[5];

			SetMACAddress(&newMAC);
		}
		else
		{
			Console.Error("DEV9: PCAP: Failed to get MAC address for adapter");
			pcap_close(hpcap);
			hpcap = nullptr;
			return;
		}

		if (switched && !SetMACSwitchedFilter(ps2MAC))
		{
			pcap_close(hpcap);
			hpcap = nullptr;
			Console.Error("DEV9: PCAP: Can't open Device '%s'", EmuConfig.DEV9.EthDevice.c_str());
			return;
		}
	}

	if (foundAdapter)
		InitInternalServer(&adapter);
	else
		InitInternalServer(nullptr);
}
AdapterOptions PCAPAdapter::GetAdapterOptions()
{
	return AdapterOptions::None;
}
bool PCAPAdapter::blocks()
{
	pxAssert(hpcap);
	return blocking;
}
bool PCAPAdapter::isInitialised()
{
	return hpcap != nullptr;
}
//gets a packet.rv :true success
bool PCAPAdapter::recv(NetPacket* pkt)
{
	pxAssert(hpcap);

	if (!blocking && NetAdapter::recv(pkt))
		return true;

	EthernetFrame* bFrame;
	if (vRecBuffer.Dequeue(&bFrame))
	{
		bFrame->WritePacket(pkt);
		InspectRecv(pkt);

		delete bFrame;
		return true;
	}

	pcap_pkthdr* header;
	const u_char* pkt_data;

	// pcap bridged will pick up packets not intended for us, returning false on those packets will incur a 1ms wait.
	// This delays getting packets we need, so instead loop untill a valid packet, or no packet, is returned from pcap_next_ex.
	while (pcap_next_ex(hpcap, &header, &pkt_data) > 0)
	{
		if (!ipOnly)
		{
			// 1518 is the largest Ethernet frame we can get using an MTU of 1500 (assuming no VLAN tagging).
			// This includes the FCS, which should be trimmed (PS2 SDK dosn't allow extra space for this).
			if (header->len > 1518)
			{
				Console.Error("DEV9: PCAP: Dropped jumbo frame of size: %u", header->len);
				continue;
			}

			pxAssert(header->len == header->caplen);

			memcpy(pkt->buffer, pkt_data, header->len);
			pkt->size = static_cast<int>(header->len);

			if (!switched)
				SetMACBridgedRecv(pkt);

			if (VerifyPkt(pkt, header->len))
			{
				HandleFrameCheckSequence(pkt);

				// FCS (if present) has been removed, apply correct limit
				if (pkt->size > 1514)
				{
					Console.Error("DEV9: PCAP: Dropped jumbo frame of size: %u", pkt->size);
					continue;
				}

				InspectRecv(pkt);
				return true;
			}
		}
		else
		{
			// MTU of 1500
			if (header->len > 1500)
			{
				Console.Error("DEV9: PCAP: Dropped jumbo IP packet of size: %u", header->len);
				continue;
			}

			// Ensure IPv4
			u8 ver = (pkt_data[0] & 0xF0) >> 4;
			if (ver != 4)
			{
				Console.Error("DEV9: PCAP: Dropped non IPv4 packet");
				continue;
			}

			// Avoid pcap looping packets by checking IP
			IP_Packet ipPkt(const_cast<u_char*>(pkt_data), header->len);
			if (ipPkt.sourceIP == ps2IP)
			{
				continue;
			}

			pxAssert(header->len == header->caplen);

			// Build EtherFrame using captured packet
			PayloadPtr* pl = new PayloadPtr(const_cast<u_char*>(pkt_data), header->len);
			EthernetFrame frame(pl);
			frame.sourceMAC = internalMAC;
			frame.destinationMAC = ps2MAC;
			frame.protocol = static_cast<u16>(EtherType::IPv4);
			frame.WritePacket(pkt);

			InspectRecv(pkt);
			return true;
		}
		// continue.
	}

	return false;
}
//sends the packet .rv :true success
bool PCAPAdapter::send(NetPacket* pkt)
{
	pxAssert(hpcap);

	InspectSend(pkt);
	if (NetAdapter::send(pkt))
		return true;

	// TODO: loopback broadcast packets to host pc in switched mode.
	if (!ipOnly)
	{
		if (!switched)
			SetMACBridgedSend(pkt);

		if (pcap_sendpacket(hpcap, (u_char*)pkt->buffer, pkt->size))
			return false;
		else
			return true;
	}
	else
	{
		EthernetFrameEditor frame(pkt);
		if (frame.GetProtocol() == static_cast<u16>(EtherType::IPv4))
		{
			PayloadPtr* payload = frame.GetPayload();
			IP_Packet pkt(payload->data, payload->GetLength());

			if (pkt.sourceIP != IP_Address{{{0, 0, 0, 0}}})
			{
				ps2IP = pkt.sourceIP;
			}

			if (pcap_sendpacket(hpcap, payload->data, pkt.GetLength()))
				return false;
			else
				return true;
		}
		if (frame.GetProtocol() == static_cast<u16>(EtherType::ARP))
		{
			// We will need to respond to ARP requests for all except the PS2 ip
			// However, we won't know the PS2 ip yet unless our dhcpServer is used
			PayloadPtr* payload = frame.GetPayload();
			ARP_Packet arpPkt(payload->data, payload->GetLength());
			if (arpPkt.protocol == static_cast<u16>(EtherType::IPv4))
			{
				/* This is untested */
				if (arpPkt.op == 1) //ARP request
				{
					if (*(IP_Address*)arpPkt.targetProtocolAddress.get() != dhcpServer.ps2IP)
					// it's trying to resolve the gateway's mac addr
					{
						Console.Error("DEV9: PCAP: ARP Request on DLT_RAW adapter, providing assumed response");
						ARP_Packet* arpRet = new ARP_Packet(6, 4);
						std::memcpy(arpRet->targetHardwareAddress.get(), arpPkt.senderHardwareAddress.get(), sizeof(MAC_Address));
						std::memcpy(arpRet->senderHardwareAddress.get(), &internalMAC, sizeof(MAC_Address));
						std::memcpy(arpRet->targetProtocolAddress.get(), arpPkt.senderProtocolAddress.get(), sizeof(IP_Address));
						std::memcpy(arpRet->senderProtocolAddress.get(), arpPkt.targetProtocolAddress.get(), sizeof(IP_Address));
						arpRet->op = 2,
						arpRet->protocol = arpPkt.protocol;
						arpRet->hardwareType = arpPkt.hardwareType;

						EthernetFrame* retARP = new EthernetFrame(arpRet);
						retARP->destinationMAC = ps2MAC;
						retARP->sourceMAC = internalMAC;
						retARP->protocol = static_cast<u16>(EtherType::ARP);

						vRecBuffer.Enqueue(retARP);
					}
				}
			}
			return true;
		}
		return false;
	}
}

void PCAPAdapter::reloadSettings()
{
	AdapterUtils::Adapter adapter;
	AdapterUtils::AdapterBuffer buffer;
	if (AdapterUtils::GetAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer))
		ReloadInternalServer(&adapter);
	else
		ReloadInternalServer(nullptr);
}

PCAPAdapter::~PCAPAdapter()
{
	if (hpcap)
	{
		pcap_close(hpcap);
		hpcap = nullptr;
	}

	//Clear out vRecBuffer
	while (!vRecBuffer.IsQueueEmpty())
	{
		EthernetFrame* retPay;
		if (!vRecBuffer.Dequeue(&retPay))
		{
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(1ms);
			continue;
		}

		delete retPay;
	}
}

std::vector<AdapterEntry> PCAPAdapter::GetAdapters()
{
	std::vector<AdapterEntry> nic;

#ifdef _WIN32
	if (!load_pcap())
		return nic;
#endif

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_if_t* alldevs;
	pcap_if_t* d;

	if (pcap_findalldevs(&alldevs, errbuf) == -1)
		return nic;

	d = alldevs;
	while (d != NULL)
	{
		AdapterEntry entry;
		entry.type = Pcsx2Config::DEV9Options::NetApi::PCAP_Switched;
#ifdef _WIN32
		//guid
		if (!std::string_view(d->name).starts_with(PCAPPREFIX))
		{
			Console.Error("DEV9: PCAP: Unexpected Device: ", d->name);
			d = d->next;
			continue;
		}

		entry.guid = std::string(&d->name[strlen(PCAPPREFIX)]);

		IP_ADAPTER_ADDRESSES adapterInfo;
		AdapterUtils::AdapterBuffer buffer;

		if (AdapterUtils::GetAdapter(entry.guid, &adapterInfo, &buffer))
			entry.name = StringUtil::WideStringToUTF8String(std::wstring(adapterInfo.FriendlyName));
		else
		{
			//have to use description
			//NPCAP 1.10 is using a version of pcap that doesn't
			//allow us to set it to use UTF8
			//see https://github.com/nmap/npcap/issues/276
			//We have to convert from ANSI to wstring, to then convert to UTF8
			const int len_desc = strlen(d->description) + 1;
			const int len_buf = MultiByteToWideChar(CP_ACP, 0, d->description, len_desc, nullptr, 0);

			std::unique_ptr<wchar_t[]> buf = std::make_unique<wchar_t[]>(len_buf);
			MultiByteToWideChar(CP_ACP, 0, d->description, len_desc, buf.get(), len_buf);

			entry.name = StringUtil::WideStringToUTF8String(std::wstring(buf.get()));
		}
#else
		entry.name = std::string(d->name);
		entry.guid = std::string(d->name);
#endif

		nic.push_back(entry);
		entry.type = Pcsx2Config::DEV9Options::NetApi::PCAP_Bridged;
		nic.push_back(entry);
		d = d->next;
	}

	return nic;
}

// Opens device for capture and sets non-blocking.
bool PCAPAdapter::InitPCAP(const std::string& adapter, bool promiscuous)
{
	char errbuf[PCAP_ERRBUF_SIZE];
	Console.WriteLn("DEV9: PCAP: Opening adapter '%s'...", adapter.c_str());

	// Open the adapter.
	if ((hpcap = pcap_open_live(adapter.c_str(), // Name of the device.
			 65536, // portion of the packet to capture.
			 // 65536 grants that the whole packet will be captured on all the MACs.
			 promiscuous ? 1 : 0,
			 1, // Read timeout.
			 errbuf // Error buffer.
			 )) == nullptr)
	{
		Console.Error("DEV9: PCAP: %s", errbuf);
		Console.Error("DEV9: PCAP: Unable to open the adapter. %s is not supported by pcap", adapter.c_str());
		return false;
	}

	if (pcap_setnonblock(hpcap, 1, errbuf) == -1)
	{
		Console.Error("DEV9: PCAP: Error setting non-blocking: %s", pcap_geterr(hpcap));
		Console.Error("DEV9: PCAP: Continuing in blocking mode");
		blocking = true;
	}
	else
		blocking = false;

	// Validate.
	const int dlt = pcap_datalink(hpcap);
	const char* dlt_name = pcap_datalink_val_to_name(dlt);

	Console.WriteLn("DEV9: PCAP: Device uses DLT %d: %s", dlt, dlt_name);
	switch (dlt)
	{
		case DLT_EN10MB:
			//case DLT_IEEE802_11:
			ipOnly = false;
			break;
		case DLT_RAW:
			ipOnly = true;
			break;
		default:
			Console.Error("DEV9: PCAP: Error, unsupported data link type (%d): %s", dlt, dlt_name);
			pcap_close(hpcap);
			hpcap = nullptr;
			return false;
	}

	Console.WriteLn("DEV9: PCAP: Adapter Ok.");
	return true;
}

bool PCAPAdapter::SetMACSwitchedFilter(MAC_Address mac)
{
	bpf_program fp;

	char filter[128];
	std::snprintf(filter, std::size(filter), "ether broadcast or ether dst %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
		mac.bytes[0], mac.bytes[1], mac.bytes[2], mac.bytes[3], mac.bytes[4], mac.bytes[5]);

	if (pcap_compile(hpcap, &fp, filter, 1, PCAP_NETMASK_UNKNOWN) == -1)
	{
		Console.Error("DEV9: PCAP: Error calling pcap_compile: %s", pcap_geterr(hpcap));
		return false;
	}

	int setFilterRet;
	if ((setFilterRet = pcap_setfilter(hpcap, &fp)) == -1)
		Console.Error("DEV9: PCAP: Error setting filter: %s", pcap_geterr(hpcap));

	pcap_freecode(&fp);
	return setFilterRet != -1;
}

void PCAPAdapter::SetMACBridgedRecv(NetPacket* pkt)
{
	EthernetFrameEditor frame(pkt);
	if (frame.GetProtocol() == static_cast<u16>(EtherType::IPv4)) // IP
	{
		// Compare DEST IP in IP with the PS2's IP, if they match, change DEST MAC to ps2MAC.
		PayloadPtr* payload = frame.GetPayload();
		IP_Packet ippkt(payload->data, payload->GetLength());
		if (ippkt.destinationIP == ps2IP)
			frame.SetDestinationMAC(ps2MAC);
	}
	if (frame.GetProtocol() == static_cast<u16>(EtherType::ARP)) // ARP
	{
		// Compare DEST IP in ARP with the PS2's IP, if they match, DEST MAC to ps2MAC on both ARP and ETH Packet headers.
		ARP_PacketEditor arpPkt(frame.GetPayload());
		if (*(IP_Address*)arpPkt.TargetProtocolAddress() == ps2IP)
		{
			frame.SetDestinationMAC(ps2MAC);
			*(MAC_Address*)arpPkt.TargetHardwareAddress() = ps2MAC;
		}
	}
}

void PCAPAdapter::SetMACBridgedSend(NetPacket* pkt)
{
	EthernetFrameEditor frame(pkt);
	if (frame.GetProtocol() == static_cast<u16>(EtherType::IPv4)) // IP
	{
		PayloadPtr* payload = frame.GetPayload();
		IP_Packet ippkt(payload->data, payload->GetLength());
		ps2IP = ippkt.sourceIP;
	}
	if (frame.GetProtocol() == static_cast<u16>(EtherType::ARP)) // ARP
	{
		ARP_PacketEditor arpPkt(frame.GetPayload());
		ps2IP = *(IP_Address*)arpPkt.SenderProtocolAddress();
		*(MAC_Address*)arpPkt.SenderHardwareAddress() = hostMAC;
	}
	frame.SetSourceMAC(hostMAC);
}

/*
 * Strips the Frame Check Sequence if we manage to capture it.
 * 
 * On Windows, (some?) Intel NICs can be configured to capture FCS.
 * 
 * Linux can be configure to capture FCS, using `ethtool -K <interface> rx-fcs on` on supported devices.
 * Support for capturing FCS can be checked with `ethtool -k <interface> | grep rx-fcs`.
 * if it's `off [Fixed]`, then the interface/driver dosn't support capturing FCS.
 * 
 * BSD based systems might capture FCS by default.
 * 
 * Packets sent by host won't have FCS, We identify these packets by checking the source MAC address.
 * Packets sent by another application via packet injection also won't have FCS and may not match the adapter MAC.
 */
void PCAPAdapter::HandleFrameCheckSequence(NetPacket* pkt)
{
	EthernetFrameEditor frame(pkt);
	if (frame.GetSourceMAC() == hostMAC)
		return;

	// There is a (very) low chance of the last 4 bytes of payload somehow acting as a valid checksum for the whole Ethernet frame.
	// For EtherTypes we already can parse, trim the Ethernet frame based on the payload length.

	int payloadSize = -1;
	if (frame.GetProtocol() == static_cast<u16>(EtherType::IPv4)) // IP
	{
		PayloadPtr* payload = frame.GetPayload();
		IP_Packet ippkt(payload->data, payload->GetLength());
		payloadSize = ippkt.GetLength();
	}
	if (frame.GetProtocol() == static_cast<u16>(EtherType::ARP)) // ARP
	{
		ARP_PacketEditor arpPkt(frame.GetPayload());
		payloadSize = arpPkt.GetLength();
	}

	if (payloadSize != -1)
	{
		// Minumum frame size is 60 + 4 byte FCS.
		// Virtual NICs may omit this padding, so check we arn't increasing pkt size.
		payloadSize = std::min(std::max(payloadSize, 60 - frame.headerLength), pkt->size);

		pkt->size = payloadSize + frame.headerLength;
		return;
	}

	// Ethertype unknown, rely on checking for a FCS.
	if (ValidateEtherFrame(pkt))
		pkt->size -= 4;
}

bool PCAPAdapter::ValidateEtherFrame(NetPacket* pkt)
{
	u32 crc = 0xFFFFFFFF;

	for (int i = 0; i < pkt->size; i++)
	{
		// Neads unsigned value
		crc = crc ^ static_cast<u8>(pkt->buffer[i]);
		for (int bit = 0; bit < 8; bit++)
		{
			if ((crc & 1) != 0)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc = (crc >> 1);
		}
	}

	crc = ~crc;

	return crc == 0x2144DF1C;
}
