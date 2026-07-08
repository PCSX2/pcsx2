// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// PCAPAdapter stubs for Android (no libpcap available).

#include "PrecompiledHeader.h"
#include "DEV9/pcap_io.h"

PCAPAdapter::PCAPAdapter() {}
PCAPAdapter::~PCAPAdapter() {}
bool PCAPAdapter::blocks() { return false; }
bool PCAPAdapter::isInitialised() { return false; }
bool PCAPAdapter::recv(NetPacket* pkt) { return false; }
bool PCAPAdapter::send(NetPacket* pkt) { return false; }
void PCAPAdapter::reloadSettings() {}
std::vector<AdapterEntry> PCAPAdapter::GetAdapters() { return {}; }
AdapterOptions PCAPAdapter::GetAdapterOptions() { return AdapterOptions::None; }

bool PCAPAdapter::InitPCAP(const std::string& adapter, bool promiscuous) { return false; }
bool PCAPAdapter::SetMACSwitchedFilter(PacketReader::MAC_Address mac) { return false; }
void PCAPAdapter::SetMACBridgedRecv(NetPacket* pkt) {}
void PCAPAdapter::SetMACBridgedSend(NetPacket* pkt) {}
void PCAPAdapter::HandleFrameCheckSequence(NetPacket* pkt) {}
bool PCAPAdapter::ValidateEtherFrame(NetPacket* pkt) { return false; }
