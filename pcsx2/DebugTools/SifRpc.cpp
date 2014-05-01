/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include "Common.h"
#include "IopCommon.h"
#include "DebugTools/SifRpc.h"

namespace sif_tracer {
	u32 ee_receiver_cmd_buffer;

	static u32 read(const u32 sif, const u32 src) {
		if (sif == 1)
			return psMu32(src);
		else
			return iopMemRead32(src);
	}

	static const u8* ptr(const u32 sif, u32 src) {
		if (sif == 1)
			return (const u8*)PSM(src);
		else
			return (const u8*)iopVirtMemR<u32>(src);
	}

	static const char* cmd_to_string(u32 cmd) {
		switch (cmd) {
			case SIF_CMD_CH_SADDR:	return "CMD_CH_SADDR";
			case SIF_CMD_SREG:		return "CMD_SREG";
			case SIF_CMD_INIT:		return "CMD_INIT";
			case SIF_CMD_RESET:		return "CMD_RESET";
			case SIF_CMD_RSP:		return "CMD_RSP";
			case SIF_CMD_BIND:		return "CMD_BIND";
			case SIF_CMD_CALL:		return "CMD_CALL";
			case SIF_CMD_OTHERDATA: return "CMD_O_DATA";
			default:				return "CMD_???";
		}
	}

	static void print_bind_info(const u32 sif, SifBind* bind) {
		// In case we want to print more info of the client data structure
		DevCon.WriteLn("\t\t???:%x. Packet src:0x%x. Packet ID:%d. ClientData:0x%x. ServerId:0x%x\n", 
				bind->unknown1, bind->p_src, bind->p_id, bind->client_data_ptr, bind->server_id);
	}

	static void print_rsp_info(const u32 sif, SifRsp* rsp) {
		// In case we want to print more info of the server/client data structure
		DevCon.WriteLn("\t\t???:%x. Req packet src:0x%x. (Packet id)???:%x, Req ClientData:0x%x, Req cmd:%s, ServerData:0x%x\n",
				rsp->unknown1, rsp->p_src, rsp->p_id, rsp->client_data_ptr, cmd_to_string(rsp->req_cmd), rsp->server_data_ptr);
	}

	static void print_call_info(const u32 sif, SifCall* call) {
		// In case we want to print more info of the server/client data structure
		DevCon.WriteLn("\t\t???:%x. Packet src:0x%x. Packet ID:%d. ClientData:0x%x. rpc service:0x%x.", 
				call->unknown1, call->p_src, call->p_id, call->client_data_ptr, call->fno);
		DevCon.WriteLn("\t\tArg Size:%d B. Ret Add:0x%x. Ret Size:%d B. Ret Mode:%x. ServerData:0x%x\n",
				call->size, call->rsp_add, call->rsp_size, call->rsp_mode, call->server_data_ptr);
	}

	static void analyze_rpc_command(const u32 sif, u32 src) {
		SifCmdHdr* h = (SifCmdHdr*)ptr(sif, src);
		u32 h_size   = h->h_size;
		u32 d_size   = h->d_size;
		u32 d_addr   = h->d_addr;
		u32 cmd      = h->fcode;

		if (d_size)
			DevCon.WriteLn("\tSIF%d rpc cmd: %s. H:%d B, With D:%d B from 0x%x", sif, cmd_to_string(cmd), h_size, d_size, d_addr);
		else
			DevCon.WriteLn("\tSIF%d rpc cmd: %s. H:%d B", sif, cmd_to_string(cmd), h_size);

		if (h_size <= 16) {
			DevCon.WriteLn("\n");
			return;
		}

		switch (cmd) {
			case SIF_CMD_INIT: 
				DevCon.WriteLn("\t\treceiver address: 0x%x\n", read(sif, src+16));
				return;
			case SIF_CMD_SREG:
				DevCon.WriteLn("\t\tset register %d <= 0x%x (or the reverse)\n", read(sif, src+16), read(sif, src+20));
				return;
			case SIF_CMD_RESET:
				DevCon.WriteLn("\t\tReboot with flag %x file => %s\n", read(sif, src+20), ptr(sif, src+24));
				return;
			case SIF_CMD_BIND: print_bind_info(sif, (SifBind*)ptr(sif, src)); return;
			case SIF_CMD_RSP:  print_rsp_info(sif, (SifRsp*)ptr(sif, src)); return;
			case SIF_CMD_CALL: print_call_info(sif, (SifCall*)ptr(sif, src)); return;
			default: break;
		}

		// Unknown command format just print the data
		char *buf = new char[32];
		std::string header("\textra cmd data:");

		for (u32 i = 16; i < h_size; i+=4) {
			std::sprintf(buf, "0x%x ", read(sif, src+i));
			header += std::string(buf);
		}
		delete[] buf;

		DevCon.WriteLn("%s\n", header.c_str());
	}

	void analyze_dma_transfer(const u32 sif, u32 transfers, u32 count)
	{
		if (sif > 1) {
			DevCon.Error("analyze_dma_transfer sif %d not implemented", sif);
			return;
		}
		if (!SysTraceActive(EE.Sif) || !SysTraceActive(IOP.Sif))
			return;

		SifDma* trans = (SifDma*)ptr(sif, transfers);

		std::string origin = (sif == 0) ? "IOP" : "EE";
		for (u32 c = 0; c < count; c++) {
			u32 src  = trans[c].src;
			u32 dst  = trans[c].dest;
			u32 size = trans[c].size;
			DevCon.WriteLn("SIF%d (%s) transfer (%d/%d) of %d B from 0x%x to 0x%x", sif, origin.c_str(), c, count-1, size, src, dst);

			// Note F210 is set by sifman.027
			if (sif == 1 && dst == psHu32(SBUS_F210)) {
				// EE-IOP command detected
				if (psMu32(src+8) == SIF_CMD_INIT && psMu32(src) == 20) {
					ee_receiver_cmd_buffer = psMu32(src+16);
				}
				analyze_rpc_command(sif, src);
			}
			if (sif == 0 && dst == ee_receiver_cmd_buffer) {
				// IOP-EE command detected
				analyze_rpc_command(sif, src);
			}
			// Else just a big fat blob of data
		}
	}

}
