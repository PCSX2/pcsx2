/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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
#include "IPU/IPU.h"
#include "IPU/IPUdma.h"
#include "IPU/IPU_MultiISA.h"

alignas(16) IPU_Fifo ipu_fifo;

void IPU_Fifo::init()
{
	out.readpos = 0;
	out.writepos = 0;
	in.readpos = 0;
	in.writepos = 0;
	std::memset(in.data, 0, sizeof(in.data));
	std::memset(out.data, 0, sizeof(out.data));
}

void IPU_Fifo_Input::clear()
{
	std::memset(data, 0, sizeof(data));
	g_BP.IFC = 0;
	ipuRegs.ctrl.IFC = 0;
	readpos = 0;
	writepos = 0;

	// Because the FIFO is drained it will request more data immediately
	IPU1Status.DataRequested = true;

	if (ipu1ch.chcr.STR && cpuRegs.eCycle[4] == 0x9999)
	{
		CPU_INT(DMAC_TO_IPU, 4);
	}
}

void IPU_Fifo_Output::clear()
{
	std::memset(data, 0, sizeof(data));
	ipuRegs.ctrl.OFC = 0;
	readpos = 0;
	writepos = 0;
}

void IPU_Fifo::clear()
{
	in.clear();
	out.clear();
}

std::string IPU_Fifo_Input::desc() const
{
	return StringUtil::StdStringFromFormat("IPU Fifo Input: readpos = 0x%x, writepos = 0x%x, data = 0x%x", readpos, writepos, data);
}

std::string IPU_Fifo_Output::desc() const
{
	return StringUtil::StdStringFromFormat("IPU Fifo Output: readpos = 0x%x, writepos = 0x%x, data = 0x%x", readpos, writepos, data);
}

int IPU_Fifo_Input::write(const u32* pMem, int size)
{
	const int transfer_size = std::min(size, 8 - (int)g_BP.IFC);
	if (!transfer_size) return 0;

	const int first_words = std::min((32 - writepos), transfer_size << 2);
	const int second_words = (transfer_size << 2) - first_words;

	memcpy(&data[writepos], pMem, first_words << 2);
	pMem += first_words;

	if(second_words)
		memcpy(&data[0], pMem, second_words << 2);

	writepos = (writepos + (transfer_size << 2)) & 31;

	g_BP.IFC += transfer_size;

	if (g_BP.IFC == 8)
		IPU1Status.DataRequested = false;

	CPU_INT(IPU_PROCESS, transfer_size * BIAS);

	return transfer_size;
}

int IPU_Fifo_Input::read(void *value)
{
	// wait until enough data to ensure proper streaming.
	if (g_BP.IFC <= 1)
	{
		// IPU FIFO is empty and DMA is waiting so lets tell the DMA we are ready to put data in the FIFO
		IPU1Status.DataRequested = true;

		if(ipu1ch.chcr.STR && cpuRegs.eCycle[4] == 0x9999)
		{
			CPU_INT( DMAC_TO_IPU, std::min(8U, ipu1ch.qwc));
		}

		if (g_BP.IFC == 0) return 0;
		pxAssert(g_BP.IFC > 0);
	}

	CopyQWC(value, &data[readpos]);

	readpos = (readpos + 4) & 31;
	g_BP.IFC--;
	return 1;
}

int IPU_Fifo_Output::write(const u32 *value, uint size)
{
	pxAssertMsg(size>0, "Invalid size==0 when calling IPU_Fifo_Output::write");

	const int transfer_size = std::min(size, 8 - (uint)ipuRegs.ctrl.OFC);
	if(!transfer_size) return 0;

	const int first_words = std::min((32 - writepos), transfer_size << 2);
	const int second_words = (transfer_size << 2) - first_words;

	memcpy(&data[writepos], value, first_words << 2);
	value += first_words;
	if (second_words)
		memcpy(&data[0], value, second_words << 2);

	writepos = (writepos + (transfer_size << 2)) & 31;

	ipuRegs.ctrl.OFC += transfer_size;

	if(ipu0ch.chcr.STR)
		IPU_INT_FROM(ipuRegs.ctrl.OFC * BIAS);

	return transfer_size;
}

void IPU_Fifo_Output::read(void *value, uint size)
{
	pxAssert(ipuRegs.ctrl.OFC >= size);
	ipuRegs.ctrl.OFC -= size;

	// Zeroing the read data is not needed, since the ringbuffer design will never read back
	// the zero'd data anyway. --air

	const int first_words = std::min((32 - readpos), static_cast<int>(size << 2));
	const int second_words = static_cast<int>(size << 2) - first_words;

	memcpy(value, &data[readpos], first_words << 2);
	value = static_cast<u32*>(value) + first_words;

	if (second_words)
		memcpy(value, &data[0], second_words << 2);

	readpos = (readpos + static_cast<int>(size << 2)) & 31;
}

void ReadFIFO_IPUout(mem128_t* out)
{
	if (!pxAssertDev( ipuRegs.ctrl.OFC > 0, "Attempted read from IPUout's FIFO, but the FIFO is empty!" )) return;
	ipu_fifo.out.read(out, 1);

	// Games should always check the fifo before reading from it -- so if the FIFO has no data
	// its either some glitchy game or a bug in pcsx2.
}

void WriteFIFO_IPUin(const mem128_t* value)
{
	IPU_LOG( "WriteFIFO/IPUin <- 0x%08X.%08X.%08X.%08X", value->_u32[0], value->_u32[1], value->_u32[2], value->_u32[3]);

	//committing every 16 bytes
	if( ipu_fifo.in.write(value->_u32, 1) == 0 )
	{
		if (ipuRegs.ctrl.BUSY && !CommandExecuteQueued)
		{
			CommandExecuteQueued = false;
			CPU_INT(IPU_PROCESS, 8);
		}
	}
}
