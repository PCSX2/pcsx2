/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "GDBServer.h"
#include "Breakpoints.h"
#include "VMManager.h"
#include "MIPSAnalyst.h"
#undef min
#undef max
#include <charconv>
#include <filesystem>
#include <fstream>

#define DEBUG_WRITE(...) if (IsDebugBuild) { Console.WriteLn(Color_Gray, __VA_ARGS__); }
/*";QStartNoAckMode+" \*/
#define GDB_FEATURES \
	"PacketSize=47ff"               /* required by GDB stub*/ \
	";Qbtrace:off-"                 /* required by GDB stub*/ \
	";Qbtrace:bts-"                 /* required by GDB stub*/ \
	";Qbtrace:pt-"                  /* required by GDB stub*/ \
	";Qbtrace-conf:bts:size-"       /* required by GDB stub*/ \
	";Qbtrace-conf:pt:size-"        /* required by GDB stub*/ \
	";QCatchSyscalls-" \
	";QPassSignals-" \
	";qXfer:features:read+"\
	";qXfer:threads:read-"\
	";qXfer:libraries:read-"\
	";qXfer:memory-map:read-"\
	";qXfer:sdata:read-"\
	";qXfer:siginfo:read-"\
	";qXfer:traceframe-info:read-"\
	";hwbreak+" \
	";swbreak+" \
	";BreakpointCommands+" \
	";vContSupported+" \
	";QThreadEvents-" \
	";memory-tagging-" \
	";tracenz-" \
	";ConditionalBreakpoints+" \
	";ConditionalTracepoints-" \
	";TracepointSource-" \
	";EnableDisableTracepoints-" 

static const std::string targetIOPXML;
static const std::string targetEEXML = R"(<?xml version="1.0"?>
<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<target version="1.0">

<!-- Helping GDB -->
<architecture>mips:5900</architecture>
<osabi>none</osabi>

<!-- Mapping ought to be flexible, but there seems to be some
     hardcoded parts in gdb, so let's use the same mapping. -->
<feature name="org.gnu.gdb.mips.cpu">
  <reg name="r0" bitsize="32" regnum="0"/>
  <reg name="r1" bitsize="32"/>
  <reg name="r2" bitsize="32"/>
  <reg name="r3" bitsize="32"/>
  <reg name="r4" bitsize="32"/>
  <reg name="r5" bitsize="32"/>
  <reg name="r6" bitsize="32"/>
  <reg name="r7" bitsize="32"/>
  <reg name="r8" bitsize="32"/>
  <reg name="r9" bitsize="32"/>
  <reg name="r10" bitsize="32"/>
  <reg name="r11" bitsize="32"/>
  <reg name="r12" bitsize="32"/>
  <reg name="r13" bitsize="32"/>
  <reg name="r14" bitsize="32"/>
  <reg name="r15" bitsize="32"/>
  <reg name="r16" bitsize="32"/>
  <reg name="r17" bitsize="32"/>
  <reg name="r18" bitsize="32"/>
  <reg name="r19" bitsize="32"/>
  <reg name="r20" bitsize="32"/>
  <reg name="r21" bitsize="32"/>
  <reg name="r22" bitsize="32"/>
  <reg name="r23" bitsize="32"/>
  <reg name="r24" bitsize="32"/>
  <reg name="r25" bitsize="32"/>
  <reg name="r26" bitsize="32"/>
  <reg name="r27" bitsize="32"/>
  <reg name="r28" bitsize="32"/>
  <reg name="r29" bitsize="32"/>
  <reg name="r30" bitsize="32"/>
  <reg name="r31" bitsize="32"/>

  <reg name="lo" bitsize="32" regnum="33"/>
  <reg name="hi" bitsize="32" regnum="34"/>
  <reg name="pc" bitsize="32" regnum="37"/>
</feature>

<feature name="org.gnu.gdb.mips.cp0">
  <reg name="status" bitsize="32" regnum="32"/>
  <reg name="badvaddr" bitsize="32" regnum="35"/>
  <reg name="cause" bitsize="32" regnum="36"/>
</feature>

<!-- We don't have an FPU, but gdb hardcodes one, and will choke
     if this section isn't present. -->
<feature name="org.gnu.gdb.mips.fpu">
  <reg name="f0" bitsize="32" type="ieee_single" regnum="38"/>
  <reg name="f1" bitsize="32" type="ieee_single"/>
  <reg name="f2" bitsize="32" type="ieee_single"/>
  <reg name="f3" bitsize="32" type="ieee_single"/>
  <reg name="f4" bitsize="32" type="ieee_single"/>
  <reg name="f5" bitsize="32" type="ieee_single"/>
  <reg name="f6" bitsize="32" type="ieee_single"/>
  <reg name="f7" bitsize="32" type="ieee_single"/>
  <reg name="f8" bitsize="32" type="ieee_single"/>
  <reg name="f9" bitsize="32" type="ieee_single"/>
  <reg name="f10" bitsize="32" type="ieee_single"/>
  <reg name="f11" bitsize="32" type="ieee_single"/>
  <reg name="f12" bitsize="32" type="ieee_single"/>
  <reg name="f13" bitsize="32" type="ieee_single"/>
  <reg name="f14" bitsize="32" type="ieee_single"/>
  <reg name="f15" bitsize="32" type="ieee_single"/>
  <reg name="f16" bitsize="32" type="ieee_single"/>
  <reg name="f17" bitsize="32" type="ieee_single"/>
  <reg name="f18" bitsize="32" type="ieee_single"/>
  <reg name="f19" bitsize="32" type="ieee_single"/>
  <reg name="f20" bitsize="32" type="ieee_single"/>
  <reg name="f21" bitsize="32" type="ieee_single"/>
  <reg name="f22" bitsize="32" type="ieee_single"/>
  <reg name="f23" bitsize="32" type="ieee_single"/>
  <reg name="f24" bitsize="32" type="ieee_single"/>
  <reg name="f25" bitsize="32" type="ieee_single"/>
  <reg name="f26" bitsize="32" type="ieee_single"/>
  <reg name="f27" bitsize="32" type="ieee_single"/>
  <reg name="f28" bitsize="32" type="ieee_single"/>
  <reg name="f29" bitsize="32" type="ieee_single"/>
  <reg name="f30" bitsize="32" type="ieee_single"/>
  <reg name="f31" bitsize="32" type="ieee_single"/>

  <reg name="fcsr" bitsize="32" group="float"/>
  <reg name="fir" bitsize="32" group="float"/>
</feature>
</target>
)";

std::mutex CPUTransaction;

inline
bool 
ObtainCategoryAndIndex(int id, int& cat, int& idx)
{
	if (id < 32)
	{
		cat = EECAT_GPR;
		idx = id;
		return true;
	}

	if (id == 32) // status
	{
		cat = EECAT_CP0;
		idx = 12;
		return true;
	}

	if (id == 33) // lo
	{
		cat = EECAT_GPR;
		idx = 34;
		return true;
	}

	if (id == 34) // hi
	{
		cat = EECAT_GPR;
		idx = 33;
		return true;
	}

	if (id == 35) // badvaddr
	{
		cat = EECAT_CP0;
		idx = 8;
		return true;
	}

	if (id == 36) // cause
	{
		cat = EECAT_CP0;
		idx = 13;
		return true;
	}

	if (id == 37) // pc
	{
		cat = EECAT_GPR;
		idx = 32;
		return true;
	}

	if (id >= 38 && id < 70)
	{
		cat = EECAT_FPR;
		idx = id - 38;
		return true;
	}

	if (id == 70)
	{
		cat = EECAT_FCR;
		idx = 31;
		return true;
	}

	return false;
}

template<typename T>
T reverse_bytes(T value)
{
	const u8* raw_value = reinterpret_cast<u8*>(&value);
	if constexpr (sizeof(T) == 4) 
	{
		return 	(raw_value[0]) | 
				(raw_value[1] << 8) | 
				(raw_value[2] << 16) | 
				(raw_value[3] << 24);
	} 
	else if constexpr (sizeof(T) == 2)
	{
		return 	(raw_value[0]) | 
				(raw_value[1] << 8);
	} 

	return value;
}

// really hacky way to detect endianess
inline bool IsBigEndian()
{
	volatile union {
		uint32_t i;
		char c[4];
    } betest;

	betest.i = 0x01020304;
    return betest.c[0] == 1;
}

#define BIG_ENDIFY_IT(x) do { if (!IsBigEndian()) { x = reverse_bytes(x); } } while(false);

constexpr
u8
ASCIIToValue(char symbol)
{
	if ((symbol >= '0') && (symbol <= '9'))
	{
		return (symbol - '0');
	}
	else if ((symbol >= 'a') && (symbol <= 'f'))
	{
		return (symbol - 'a' + 0xa);
	}
	else if ((symbol >= 'A') && (symbol <= 'F'))
	{
		return (symbol - 'A' + 0xa);
	}

	return 0;
}

constexpr 
int
ValueToASCII(u8 value)
{
	constexpr const char* digitsLookup = "0123456789abcdef";
	return digitsLookup[std::clamp(value, (u8)0, (u8)16)];
}

template<typename T>
inline
bool
WriteHexValue(char* string, std::size_t& outSize, T value)
{
	if (outSize + sizeof(T) * 2 > MAX_DEBUG_PACKET_SIZE)
		return false;
	
	BIG_ENDIFY_IT(value);
	for (size_t i = 0; i < sizeof(T); i++)
	{
		const u8* data = reinterpret_cast<u8*>(&value) + i;
		string[outSize++] = ValueToASCII((*data) >> 4);
		string[outSize++] = ValueToASCII((*data) & 0xf);
	}
	
	return true;
}

constexpr 
void
EncodeHex(
	const u8* input,
	std::size_t inputSize,
	char* output,
	std::size_t outputSize
)
{
	for (size_t i = 0; i < inputSize; i++)
	{
		*output++ = ValueToASCII((input[i]) >> 4);
		*output++ = ValueToASCII((input[i]) & 0xf);
	}
}

constexpr 
void
DecodeHex(
	const char* input,
	std::size_t inputSize,
	u8* output,
	std::size_t outputSize
)
{
	for (size_t i = 0; i < inputSize; i += 2)
	{
		u8 value = ASCIIToValue(input[i]) << 4;
		if (i + 1 < inputSize)
			value |= ASCIIToValue(input[i + 1]);
	}
}

constexpr
u8
CalculateChecksum(
	const char* data,
	std::size_t size
)
{
	u8 checksum = 0;
	for (std::size_t i = 0; i < size; i++)
		checksum += data[i];

	return checksum;
}

constexpr 
u8
CalculateChecksum(std::string_view data)
{
	return CalculateChecksum(data.data(), data.size());
}

constexpr
bool
IsSameString(
	const char* source, 
	std::size_t sourceLength, 
	const char* compare,
	std::size_t compareLength
)
{
	for (size_t i = 0; i < std::min(sourceLength, compareLength); i++)
	{
		if (source[i] != compare[i])
		{
			return false;
		}
	}

	return true;
}

constexpr
bool
IsSameString(
	std::string_view source,
	std::string_view compare
)
{
	return IsSameString(source.data(), source.size(), compare.data(), compare.size());
}

template<typename T>
void
ExecuteCPUTask(DebugInterface* cpuInterface, T&& func)
{
	std::scoped_lock<std::mutex> sc(CPUTransaction);
	if (!cpuInterface->isAlive())
		return;		// just don't execute this

	const bool beenPaused = cpuInterface->isCpuPaused();
	if (!beenPaused)
		cpuInterface->pauseCpu();

	while (!cpuInterface->isCpuPaused())
		Threading::Sleep(1);

	func();
	if (!beenPaused)
		cpuInterface->resumeCpu();
}

GDBServer::GDBServer(DebugInterface* debugInterface)
{
	m_debugInterface = debugInterface;
}

GDBServer::~GDBServer()
{
}

void 
GDBServer::resumeExecution()
{
	std::scoped_lock<std::mutex> sc(CPUTransaction);
	if (m_debugInterface->isAlive() && m_debugInterface->isCpuPaused())
		m_debugInterface->resumeCpu();
}

void 
GDBServer::stopExecution()
{
	std::scoped_lock<std::mutex> sc(CPUTransaction);
	if (m_debugInterface->isAlive() && !m_debugInterface->isCpuPaused())
		m_debugInterface->pauseCpu();
}

void 
GDBServer::singleStep()
{
	if (!m_debugInterface->isAlive() || !m_debugInterface->isCpuPaused())
	{
		Console.Warning("GDB: trying to single step when cpu is not alive or not paused.");
		return;
	}

	// Allow the cpu to skip this pc if it is a breakpoint
	CBreakPoints::SetSkipFirst(m_debugInterface->getCpuType(), m_debugInterface->getPC());
	const u32 pc = m_debugInterface->getPC();
	const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(m_debugInterface, pc);

	u32 breakAddress = pc + 0x4;
	if (info.isBranch)
		breakAddress = (info.isConditional ? (info.conditionMet ? info.branchTarget : pc + (2 * 4)) : info.branchTarget);

	if (info.isSyscall)
		breakAddress = info.branchTarget; // Syscalls are always taken

	CBreakPoints::AddBreakPoint(m_debugInterface->getCpuType(), breakAddress, true);
	m_debugInterface->resumeCpu();
}

bool 
GDBServer::addBreakpoint(u32 address)
{
	if (!m_debugInterface->isValidAddress(address))
		return false;

	ExecuteCPUTask(m_debugInterface, [this, address]() { CBreakPoints::AddBreakPoint(m_debugInterface->getCpuType(), address); });
	return true;
}

bool 
GDBServer::removeBreakpoint(u32 address)
{
	if (!m_debugInterface->isValidAddress(address))
		return false;

	ExecuteCPUTask(m_debugInterface, [this, address]() { CBreakPoints::RemoveBreakPoint(m_debugInterface->getCpuType(), address); });
	return true;
}

void 
GDBServer::updateThreadList()
{
	ExecuteCPUTask(m_debugInterface, [this]() { m_stateThreads = m_debugInterface->getThreadList(); });
}

u32 
GDBServer::getRegisterSize(int id)
{
	return 32;
}

bool 
GDBServer::readRegister(int threadId, int id, u32& value)
{
	if (!m_debugInterface->isAlive() || m_debugInterface->isCpuPaused())
		return false;

	int cat = 0;
	int idx = 0;
	if (!ObtainCategoryAndIndex(id, cat, idx))
		return false;

	u128 regValue = m_debugInterface->getRegister(cat, idx);
	value = regValue._u32[0];
	return true;
}

bool 
GDBServer::writeRegister(int threadId, int id, u32 value)
{
	if (!m_debugInterface->isAlive() || m_debugInterface->isCpuPaused())
		return false;

	int cat = 0;
	int idx = 0;
	if (!ObtainCategoryAndIndex(id, cat, idx))
		return false;

	m_debugInterface->setRegister(cat, idx, u128::From32(value));
	return true;
}

bool 
GDBServer::writePacketBegin()
{
	std::size_t& outSize = *m_outSize;
	if (outSize + (m_dontReplyAck ? 1 : 2) >= MAX_DEBUG_PACKET_SIZE)
		return false;

	char* writeData = reinterpret_cast<char*>(m_outData) + outSize;
	if (!m_dontReplyAck)
		writeData[outSize++] = '+';

	writeData[outSize++] = '$';
	return true;
}

bool 
GDBServer::writePacketEnd()
{
	std::size_t& outSize = *m_outSize;
	if (outSize + 3 >= MAX_DEBUG_PACKET_SIZE)
		return false;

	char* data = reinterpret_cast<char*>(m_outData);
	const u8 checksum = CalculateChecksum(data + (m_dontReplyAck ? 1 : 2), outSize - (m_dontReplyAck ? 1 : 2));

	data += outSize;
	*data++ = '#';
	*data++ = ValueToASCII((checksum >> 4) 	& 0xf);
	*data++ = ValueToASCII((checksum) 		& 0xf);
	outSize += 3;

	return true;
}

bool 
GDBServer::writePacketData(const char* data, std::size_t size)
{
	std::size_t& outSize = *m_outSize;
	if (outSize + size >= MAX_DEBUG_PACKET_SIZE)
		return false;

	std::memcpy((char*)m_outData + outSize, data, size);
	outSize += size;
	return true;
}

bool 
GDBServer::writeBaseResponse(std::string_view data)
{
	if (!writePacketBegin())
		return false;

	if (!writePacketData(data.data(), data.size()))
		return false;

	if (!writePacketEnd())
		return false;

	return true;
}

bool 
GDBServer::writeThreadId(int threadId, int processId)
{
	char threadIdString[16] = {};
	int charsWritten = 0;

	if (m_multiprocess)
		charsWritten = snprintf(threadIdString, 16, "%x.%x", processId, threadId);
	else
		charsWritten = snprintf(threadIdString, 16, "%x", threadId);

	if (charsWritten < 1)
		return false;

	return writePacketData(threadIdString, charsWritten);
}

bool 
GDBServer::writeRegisterValue(int threadId, int registerNumber)
{
	std::size_t& outSize = *m_outSize;
	const u32 registerSize = getRegisterSize(registerNumber);
	const std::size_t finalSize = outSize + registerSize;
	if (outSize + registerSize >= MAX_DEBUG_PACKET_SIZE)
		return false;

	u32 value = 0;
	char* buffer = reinterpret_cast<char*>(m_outData);
	if (readRegister(threadId, registerNumber, value))
		while (outSize < finalSize)
			buffer[outSize++] = 'x';
	else
		if (!WriteHexValue(buffer, outSize, value))
			return false;

	return true;
}

bool 
GDBServer::writeAllRegisterValues(int threadId)
{
	for	(int i = 0; i < 72; i++)
	{
		if (!writeRegisterValue(threadId, i))
		{
			return false;
		}
	}	

	return true;
}

// true - continue packets processing
// false - stop and send packet as is
bool 
GDBServer::processXferPacket(std::string_view data)
{
	auto makeStringView = [](std::string_view data, std::size_t& offset, char symbol, char nextSymbol, bool notEqual = false) -> std::string_view {
		const std::size_t beginIndex = data.find_first_of(symbol, offset);
		const std::size_t endIndex = nextSymbol == '\0' ? data.size() : (
			notEqual ? 
			data.find_first_not_of(nextSymbol, beginIndex + 1) : 
			data.find_first_of(nextSymbol, beginIndex + 1)
		);

		if (beginIndex == std::size_t(-1) || endIndex == std::size_t(-1))
		{
			return {};
		}

		offset = endIndex;
		return std::string_view(data.data() + beginIndex + 1, endIndex - beginIndex - 1);
	};
		
	std::size_t localOffset = 0;
	const auto verbString = makeStringView(data, localOffset, ':', ':');
	const auto sentenceString = std::string_view(data.data(), localOffset - verbString.size() - 1);
	const auto annexString = makeStringView(data, localOffset, ':', ':');
	const auto offsetString = makeStringView(data, localOffset, ':', ',');
	const auto lengthString = makeStringView(data, localOffset, ',', '\0');
	if (verbString.empty() || annexString.empty() || offsetString.empty() || lengthString.empty())
	{
		Console.Warning("GDB: one of the arguments for Xfer command was empty.");
		return false;
	}

	std::size_t offset = 0;
	std::size_t length = 0;
	if (std::from_chars(offsetString.data(), offsetString.data() + offsetString.size(), offset, 16).ec != std::errc())
	{
		Console.Warning("GDB: failed to convert offset ot integer.");
		return false;
	}	
		
	if (std::from_chars(lengthString.data(), lengthString.data() + lengthString.size(), length, 16).ec != std::errc())
	{
		Console.Warning("GDB: failed to convert length ot integer.");
		return false;
	}

	if (IsSameString(sentenceString, "features"))
	{
		char firstSymbol = 'm';
		const std::string& targetXML = (m_debugInterface->getCpuType() == BREAKPOINT_EE) ? targetEEXML : targetIOPXML;
		if (targetXML.size() - offset < length)
		{
			offset = std::min(targetXML.size() - 1, offset);
			length = targetXML.size() - offset;
			firstSymbol = 'l';
		}

		bool success = writePacketBegin();
		success |= writePacketData(&firstSymbol, 1);
		success |= writePacketData(targetXML.data() + offset, length);
		success |= writePacketEnd();

		DEBUG_WRITE("        features request");
		return success;
	}
		
	if (IsSameString(sentenceString, "threads"))
	{
		DEBUG_WRITE("        threads request");
		return false;
	}

	// we don't support other 
	Console.Warning("GDB: unsupported Xfer packet [%s].", data.data());
	writeBaseResponse("");
	return true;
}

bool 
GDBServer::processQueryPacket(std::string_view data)
{
	auto writeThreadInfo = [this]() -> bool {
		if (m_stateThreadCounter == -1)
			return false;

		if (m_stateThreads.size() <= static_cast<std::size_t>(m_stateThreadCounter))
		{
			writeBaseResponse("l");
			DEBUG_WRITE("         thread info end");
			return true;
		}

		bool success = writePacketBegin();
		success |= writePacketData("m", 1);
		success |= writeThreadId(m_stateThreads.at(m_stateThreadCounter)->TID());
		success |= writePacketEnd();
		if (!success)
			return false;

		DEBUG_WRITE("         thread info %i", m_stateThreadCounter);
		m_stateThreadCounter++;
		return true;
	};

	auto writeTraceStatus = [this]() -> bool {
		bool success = writePacketBegin();
		success |= writePacketData(m_debugInterface->isAlive() ? "T1" : "T0", 2);
		success |= writePacketEnd();
		return success;
	};

	DEBUG_WRITE("GDB: processing query packet...");
	if (data.empty())
		return false;

	switch (data[1])
	{
		case 'A':
			if (IsSameString(data, "qAttached"))
			{
				DEBUG_WRITE("    attached");
				writeBaseResponse("1");
				return true;
			}
			break;
		case 'f':
			if (IsSameString(&data[2], "ThreadInfo"))
			{
				DEBUG_WRITE("    query thread info");
				m_stateThreadCounter = 0;
				updateThreadList();

				DEBUG_WRITE("        thread info begin");
				return writeThreadInfo();
			}
			break;
		case 's':
			if (IsSameString(&data[2], "ThreadInfo"))
			{
				return writeThreadInfo();
			}

			break;

		case 'C': // get current thread
		{ 
			DEBUG_WRITE("    get current thread");
			const auto currentThread = m_debugInterface->getCurrentThread();
			if (currentThread == nullptr)
				return false;

			bool success = writePacketBegin();
			success |= writePacketData("QC", 2);
			success |= writeThreadId(currentThread->TID());
			success |= writePacketEnd();
			return success;
		}
		break;

		case 'S':
			if (IsSameString(data, "qSymbol:"))
			{
				DEBUG_WRITE("    symbol request");
				writeBaseResponse("OK");
				return true;
			}

			if (IsSameString(data, "qSupported"))
			{
				DEBUG_WRITE("    supported features request");
				writeBaseResponse(GDB_FEATURES);
				return true;
			}
			break;
		case 'T':
			if (IsSameString(data, "qTStatus"))
			{
				DEBUG_WRITE("    trace status request");
				return writeTraceStatus();
			}

			if (IsSameString(data, "qThreadExtraInfo"))
			{
				DEBUG_WRITE("    extra thread info request");
				writeBaseResponse("00");
				return true;
			}
			break;
		case 'X':
			if (IsSameString(data, "qXfer:"))
			{
				DEBUG_WRITE("    Xfer request");
				if (data.size() < 7)
					return false;

				processXferPacket(std::string_view(data.data() + 6, data.size() - 6));
				return true;
			}
			break;

		default:
			break;
	}

	// we don't support this command rn
	Console.Warning("GDB: unknown query operation [%s]", data.data());
	writeBaseResponse("");
	return true;
}

bool 
GDBServer::processGeneralQueryPacket(std::string_view data)
{
	DEBUG_WRITE("GDB: processing general query packet...");
	const std::string_view threadEventsString = "QThreadEvents:";
	if (IsSameString(data, threadEventsString))
	{
		DEBUG_WRITE("     processing thread events...");
		const char* eventsEnableString = data.data() + threadEventsString.size();
		if (*eventsEnableString == '1')
			m_eventsEnabled = true;
		else if (*eventsEnableString == '0')
			m_eventsEnabled = false;
		else
		{
			return false;
		}

		DEBUG_WRITE("    events %s", m_eventsEnabled ? "enabled" : "disabled");
		writeBaseResponse("OK");
		return true;
	}

	Console.Warning("GDB: unknown general operation [%s]", data.data());
	writeBaseResponse("");
	return true;
}

bool 
GDBServer::processMultiletterPacket(std::string_view data)
{
	DEBUG_WRITE("GDB: processing multiletter packet...");
	if (IsSameString(data, "vMustReplyEmpty"))
	{
		DEBUG_WRITE("    must reply empty");
		writeBaseResponse("");
		return true; 
	}

	if (IsSameString(data, "vCtrlC"))
	{
		DEBUG_WRITE("    ctrl+c interrupt");
		writeBaseResponse("OK");
		return true;
	}

	if (IsSameString(data, "vCont"))
	{
		DEBUG_WRITE("    processing vCont packet...");
		if (data[5] == '?')
		{
			DEBUG_WRITE("        vCont supported features reply");
			writeBaseResponse("vCont;c;C;s;S;t");
			return true;
		}

		if (data[5] == ';')
		{
			DEBUG_WRITE("        vCont apply operation [%s]", data.data());
			if (!m_debugInterface->isAlive())
				return false;

			return false;
		}

		// invalid packet, don't process it
		return false;
	}

	// we don't support this command rn
	Console.Warning("GDB: unknown \"vCont\" operation [%s]", data.data());
	writeBaseResponse("");
	return true;
}

bool 
GDBServer::processThreadPacket(std::string_view data)
{
	DEBUG_WRITE("GDB: processing thread packet...");
	if (data[1] == 'c' || data[1] == 'g')
	{
		return writeBaseResponse("OK");
	}

	/*
	switch (data[1])
	{
		case 'c':
			DEBUG_WRITE("    thread resume");
			if (IsSameString(data.data() + 2, 2, "-1", 2))
			{
				resumeExecution();
				return writeBaseResponse("OK");
			}

			Console.Warning("GDB: NOT IMPLEMENTED: continue for thread.");
			return false;
		case 'g':
			DEBUG_WRITE("    thread read registers");

			// TODO: make reading registers for other threads
			bool success = writePacketBegin();
			success |= writeAllRegisterValues(0);
			success |= writePacketEnd();
			return success;

		default:
			break;
	}
	*/

	Console.Warning("GDB: unknown thread operation [%s]", data.data());
	writeBaseResponse("");
	return true;
}

/*
[    3.3770] GDB: processing query packet...
[    3.3770]      supported features request
[    4.9174] GDB: processing multiletter packet...
[    4.9174]      must reply empty
[    5.5354] GDB: processing thread packet...
[    5.5354] GDB: unknown thread operation [Hg0#df]
[    7.1434] GDB: processing query packet...
[    7.1434]      Xfer request
[    7.1434]      features request
[    7.1445] GDB: processing query packet...
[    7.1445]      trace status request
[    7.1456] GDB: processing query packet...
[    7.1456] GDB: unknown query operation [qTfV#81]
[    7.1477] GDB: processing query packet...
[    7.1477]      query thread info
[    7.1477]          thread info begin
[    7.1477]          thread info end
[    7.1488] GDB: processing thread packet...
[    7.1488]     thread resume
[    7.1498] GDB: processing query packet...
[    7.1498]      get current thread
[    7.1498] GDB: failed to process GDB packet [+$qC#b4].
[    7.1509] GDB: processing query packet...
[    7.1509]      attached
[    7.1520] GDB: failed to process GDB packet [+$g#67].
*/

bool 
GDBServer::replyPacket(void* outData, std::size_t& outSize)
{
	if (CBreakPoints::GetBreakpointTriggered())
	{
		CBreakPoints::ClearTemporaryBreakPoints();
		CBreakPoints::SetBreakpointTriggered(false);

		// Our current PC is on a breakpoint.
		// When we run the core again, we want to skip this breakpoint and run
		CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
		CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());

		m_outSize = &outSize;
		m_outData = outData;
		return writeBaseResponse("T05");	
	}

	outSize = 0;
	return true;
}

std::size_t 
GDBServer::processPacket(const char* inData, std::size_t inSize, void* outData, std::size_t& outSize)
{
	std::size_t offset = 0;

	if (inSize == 1 && (*inData == '+' || *inData == '-'))
	{
		return 0;
	}

	while (offset < inSize)
	{
		if (inData[offset++] == '$')
			break;
	}

	if (offset == inSize)
	{
		// Maybe invalid data or something went wrong, don't care at this case
		Console.Warning("GDB: invalid GDB packet (no '$' was received).");
		outSize = 0;
		return std::size_t(-1);
	}

	std::size_t endOffset = offset;
	while (endOffset < inSize)
	{
		char sym = inData[endOffset++];
		if (sym == '#')	
			break;
	}

	if (endOffset + 2 > inSize)
	{
		// Final offset + 2 can't be bigger than size so this packet might 
		// be invalid or doesn't contain any of checksum.
		Console.Warning("GDB: invalid GDB packet (no '#' was received).");
		return std::size_t(-1);
	}

	const std::string_view data = std::string_view(&inData[offset], endOffset - 1 - offset);
	const std::size_t packetEnd = endOffset + 2;
	const u8 calculatedChecksum = CalculateChecksum(data);
	const u8 readedChecksum = (ASCIIToValue(inData[endOffset]) << 4) | ASCIIToValue(inData[endOffset + 1]);
	if (readedChecksum != calculatedChecksum)
	{
		// checksum verification failure, maybe invalid GDB protocol or invalid client
		// (or maybe it's just another bug in this code, who knows).
		Console.Warning("GDB: invalid GDB checksum (%u != %u).", calculatedChecksum, readedChecksum);
		return std::size_t(-1);
	}
	
	m_outSize = &outSize;
	m_outData = outData;
	bool success = false;
	switch (inData[offset])
	{
		case '!':
			success = writeBaseResponse("OK");
			break;

		case 'Q': // general set
			success = processGeneralQueryPacket(data);
			break;

		case 'q': // general query
			success = processQueryPacket(data);
			break;

		case 'v': // multi-letter named packet
			success = processMultiletterPacket(data);
			break;

		case 'z': // remove watchpoint (may be breakpoint or memory breal)
			break;
		case 'Z': // insert watchpoint (may be breakpoint or memory breal)
			break;

		case 'H':
			success = processThreadPacket(data);
			break;

		case 'D':
			success = writeBaseResponse("OK");
			break;
			
		case 'C': 
			DEBUG_WRITE("GDB: resume cpu");
			resumeExecution();
			success = true;
			return 0;

		case 'c': // continue
			DEBUG_WRITE("GDB: resume cpu");
			resumeExecution();
			success = true;
			return 0;

		case 's': // step-out
			singleStep();
			m_waitingForTrap = true;
			return 0;

		case '?': // signal
			success = writeBaseResponse(m_debugInterface->isCpuPaused() ? "S05" : "S00");	
			break;

		case 'g': // read registers
			success = writePacketBegin();
			success |= writeAllRegisterValues(0);
			success |= writePacketEnd();
			break;
		case 'G': // write registers
			break;
		case 'p': // read register
		{
			if (data.size() < 3)
			{
				success = false;
				break;
			}

			u8 registedIdx = (ASCIIToValue(data[1]) << 4) | ASCIIToValue(data[2]);	
			success = writePacketBegin();
			success |= writeRegisterValue(0, registedIdx);
			success |= writePacketEnd();
		}
		break;

		case 'P': // write register
			break;
		case 'm': // read memory
			break;
		case 'M': // write memory
			break;
		case 'X': // write binary memory
			break;
		default:		
			success = writeBaseResponse("");
			break;
	}

	if (!success)
	{
		Console.Error("GDB: failed to process GDB packet [%s].", inData);
		*m_outSize = 0;
		writeBaseResponse("E00");
	}

	return packetEnd;
}
