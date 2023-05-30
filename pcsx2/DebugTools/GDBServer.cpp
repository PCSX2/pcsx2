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

std::mutex CPUTransaction;

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

template<typename T>
inline
std::string
ValueToHexString(T value)
{
	std::string outString;
	outString.reserve(sizeof(T) * 2);

	BIG_ENDIFY_IT(value);
	for (size_t i = 0; i < sizeof(T); i++)
	{
		const u8* data = reinterpret_cast<u8*>(&value) + i;
		outString.push_back(ValueToASCII((*data) >> 4));
		outString.push_back(ValueToASCII((*data) & 0xf));
	}
	
	return outString;
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
		*output++ = (ASCIIToValue(input[i]) << 4) | ASCIIToValue(input[i + 1]);
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
	for (size_t i = 0; i < size; i++)
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

// really hacky way to detect endianess
volatile inline bool IsBigEndian()
{
	union {
		uint32_t i;
		char c[4];
    } betest;

	betest.i = 0x01020304;
    return betest.c[0] == 1;
}

template<typename T>
T reverse_bytes(T value)
{
	const u8* raw_value = reinterpret_cast<u8*>(&value);
	if constexpr (sizeof(T) == 8)
	{
		return 	(raw_value[0]) | 
				(raw_value[1] << 8) | 
				(raw_value[2] << 16) | 
				(raw_value[3] << 24) |
				(raw_value[4] << 32) |
				(raw_value[5] << 40) |
				(raw_value[6] << 48) |
				(raw_value[7] << 56);
	} 
	else if constexpr (sizeof(T) == 4) 
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

#define BIG_ENDIFY_IT(x) do { if (!IsBigEndian()) { x = reverse_bytes(x); } } while(false);

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

/*
  <reg name="lr0" bitsize="64" regnum="38"/>
  <reg name="lr1" bitsize="64"/>
  <reg name="lr2" bitsize="64"/>
  <reg name="lr3" bitsize="64"/>
  <reg name="lr4" bitsize="64"/>
  <reg name="lr5" bitsize="64"/>
  <reg name="lr6" bitsize="64"/>
  <reg name="lr7" bitsize="64"/>
  <reg name="lr8" bitsize="64"/>
  <reg name="lr9" bitsize="64"/>
  <reg name="lr10" bitsize="64"/>
  <reg name="lr11" bitsize="64"/>
  <reg name="lr12" bitsize="64"/>
  <reg name="lr13" bitsize="64"/>
  <reg name="lr14" bitsize="64"/>
  <reg name="lr15" bitsize="64"/>
  <reg name="lr16" bitsize="64"/>
  <reg name="lr17" bitsize="64"/>
  <reg name="lr18" bitsize="64"/>
  <reg name="lr19" bitsize="64"/>
  <reg name="lr20" bitsize="64"/>
  <reg name="lr21" bitsize="64"/>
  <reg name="lr22" bitsize="64"/>
  <reg name="lr23" bitsize="64"/>
  <reg name="lr24" bitsize="64"/>
  <reg name="lr25" bitsize="64"/>
  <reg name="lr26" bitsize="64"/>
  <reg name="lr27" bitsize="64"/>
  <reg name="lr28" bitsize="64"/>
  <reg name="lr29" bitsize="64"/>
  <reg name="lr30" bitsize="64"/>
  <reg name="lr31" bitsize="64"/>

  <reg name="hr0" bitsize="64" />
  <reg name="hr1" bitsize="64"/>
  <reg name="hr2" bitsize="64"/>
  <reg name="hr3" bitsize="64"/>
  <reg name="hr4" bitsize="64"/>
  <reg name="hr5" bitsize="64"/>
  <reg name="hr6" bitsize="64"/>
  <reg name="hr7" bitsize="64"/>
  <reg name="hr8" bitsize="64"/>
  <reg name="hr9" bitsize="64"/>
  <reg name="hr10" bitsize="64"/>
  <reg name="hr11" bitsize="64"/>
  <reg name="hr12" bitsize="64"/>
  <reg name="hr13" bitsize="64"/>
  <reg name="hr14" bitsize="64"/>
  <reg name="hr15" bitsize="64"/>
  <reg name="hr16" bitsize="64"/>
  <reg name="hr17" bitsize="64"/>
  <reg name="hr18" bitsize="64"/>
  <reg name="hr19" bitsize="64"/>
  <reg name="hr20" bitsize="64"/>
  <reg name="hr21" bitsize="64"/>
  <reg name="hr22" bitsize="64"/>
  <reg name="hr23" bitsize="64"/>
  <reg name="hr24" bitsize="64"/>
  <reg name="hr25" bitsize="64"/>
  <reg name="hr26" bitsize="64"/>
  <reg name="hr27" bitsize="64"/>
  <reg name="hr28" bitsize="64"/>
  <reg name="hr29" bitsize="64"/>
  <reg name="hr30" bitsize="64"/>
  <reg name="hr31" bitsize="64"/>
*/

template<typename T>
void
ExecuteCPUTask(DebugInterface* cpuInterface, T&& func)
{
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
	if (m_debugInterface->isAlive() && m_debugInterface->isCpuPaused())
		m_debugInterface->resumeCpu();
}

void 
GDBServer::stopExecution()
{
	if (m_debugInterface->isAlive() && !m_debugInterface->isCpuPaused())
		m_debugInterface->pauseCpu();
}

void 
GDBServer::singleStep()
{
	ExecuteCPUTask(m_debugInterface, [this]() {
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
	});
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

std::size_t 
GDBServer::processPacket(
	const char* inData, 
	std::size_t inSize,
	void* outData, 
	std::size_t& outSize
)
{
	std::size_t offset = 0;
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

	auto writePacketData = [this, outData, &outSize](const char* data, std::size_t size)->bool {
		if (outSize + size >= MAX_DEBUG_PACKET_SIZE)
		{
			return false;
		}

		std::memcpy((char*)outData + outSize, data, size);
		outSize += size;
		return true;
	};

	auto writePacketBegin = [this, outData, &outSize]() -> bool {
		if (outSize + (m_dontReplyAck ? 1 : 2) >= MAX_DEBUG_PACKET_SIZE)
		{
			return false;
		}

		char* writeData = reinterpret_cast<char*>(outData) + outSize;
		if (!m_dontReplyAck)
		{
			writeData[outSize++] = '+';
		}

		writeData[outSize++] = '$';
		return true;
	};

	auto writePacketEnd = [this, outData, &outSize]() -> bool {
		if (outSize + 3 >= MAX_DEBUG_PACKET_SIZE)
		{
			return false;
		}

		char* data = reinterpret_cast<char*>(outData);
		const u8 checksum = CalculateChecksum(data + (m_dontReplyAck ? 1 : 2), outSize - (m_dontReplyAck ? 1 : 2));

		data += outSize;
		*data++ = '#';
		*data++ = ValueToASCII((checksum >> 4) & 0xf);
		*data++ = ValueToASCII((checksum)&0xf);
		outSize += 3;

		return true;
	};

	auto writeBaseResponse = [this, outData, &outSize, &writePacketBegin, &writePacketData, &writePacketEnd](std::string_view stringValue) -> bool {
		if (!writePacketBegin())
			return false;

		if (!writePacketData(stringValue.data(), stringValue.size()))
			return false;

		if (!writePacketEnd())
			return false;

		return true;
	};

	auto writeThreadId = [this, outData, &outSize, &writePacketData](int threadId, int processId = 1) -> bool {
		char threadIdString[16] = {};
		int charsWritten = 0;

		if (m_multiprocess)
			charsWritten = snprintf(threadIdString, 16, "%x.%x", processId, threadId);
		else
			charsWritten = snprintf(threadIdString, 16, "%x", threadId);

		if (charsWritten < 1)
			return false;

		return writePacketData(threadIdString, charsWritten);
	};

	/*
	auto processCPUThreadsPacket = [this, outData, &outSize]()
	{
		bool paused = m_debugInterface->isCpuPaused();
		std::string threadsString;
		threadsString += "<?target version=\"1.0\"?>\n";
		threadsString += "<threads>\n";

		if (!paused)
		{
			m_debugInterface->pauseCpu();
			while (m_debugInterface->isCpuPaused())
			{
				Threading::Sleep(1);
			}
		}	

		for (const auto& threadHandle : m_debugInterface->getThreadList())
		{
			threadsString += "<thread id=\"" + std::to_string(threadHandle->TID()) + "\"";
			threadsString += " name=\"" + std::to_string(threadHandle->EntryPoint()) + "\"";	
		}
		
		if (!paused)
		{
			m_debugInterface->resumeCpu();
		}

		threadsString += "</threads>\n";
		return threadsString;
	};
	*/

	auto processXferPacket = [this, outData, &outSize, &writeBaseResponse, &writePacketBegin, &writePacketData, &writePacketEnd, makeStringView](std::string_view data) -> bool {
		std::size_t localOffset = 0;
		const auto verbString = makeStringView(data, localOffset, ':', ':');
		const auto sentenceString = std::string_view(data.data(), localOffset - verbString.size() - 1);
		const auto annexString = makeStringView(data, localOffset, ':', ':');
		const auto offsetString = makeStringView(data, localOffset, ':', ',');
		const auto lengthString = makeStringView(data, localOffset, ',', '\0');
		if (verbString.empty() || annexString.empty() || offsetString.empty() || lengthString.empty())
		{
			Console.Warning("GDB: one of the arguments for Xfer command was empty.");
			writeBaseResponse("E01");
			return false;
		}

		std::size_t offset = 0;
		std::size_t length = 0;
		if (std::from_chars(offsetString.data(), offsetString.data() + offsetString.size(), offset, 16).ec != std::errc())
		{
			Console.Warning("GDB: failed to convert offset ot integer.");
			writeBaseResponse("E01");
			return false;
		}	
		
		if (std::from_chars(lengthString.data(), lengthString.data() + lengthString.size(), length, 16).ec != std::errc())
		{
			Console.Warning("GDB: failed to convert length ot integer.");
			writeBaseResponse("E01");
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

			if (!success)
			{
				outSize = 0;
				writeBaseResponse("E01");
				Console.Warning("GDB: failed to write features info .");
			}

			DEBUG_WRITE("GDB: features request");
			return false;
		}
		
		if (IsSameString(sentenceString, "threads"))
		{
			DEBUG_WRITE("GDB: threads request");
		}

		// we don't support other 
		Console.Warning("GDB: unsupported Xfer packet [%s].", data.data());
		writeBaseResponse("");
		return false;
	};

	// true - continue packets processing
	// false - stop and send packet as is
	auto processQueryPacket = [this, outData, &outSize, &offset, &writePacketBegin, &writePacketData, &writePacketEnd, &writeBaseResponse, &writeThreadId, &processXferPacket](std::string_view data) -> bool {
		auto writeThreadInfo = [this, outData, &outSize, &writeBaseResponse, &writePacketBegin, &writePacketData, &writePacketEnd, &writeThreadId]() -> bool {
			if (m_stateThreadCounter == -1)
			{
				return false;
			}

			if (m_stateThreads.size() <= static_cast<std::size_t>(m_stateThreadCounter))
			{
				writeBaseResponse("l");
				DEBUG_WRITE("GDB: thread info enumerate end");
				return true;
			}

			bool success = writePacketBegin();
			success |= writePacketData("m", 1);
			success |= writeThreadId(m_stateThreads.at(m_stateThreadCounter)->TID());
			//success = WriteHexValue(reinterpret_cast<char*>(outData), outSize, m_stateThreads.at(m_stateThreadCounter)->TID());
			success |= writePacketEnd();
			if (!success)
			{
				outSize = 0;
				return false;
			}
							
			DEBUG_WRITE("GDB: thread info enumerating");		
			m_stateThreadCounter++;
			return true;
		};
		
		if (data.empty())
		{
			writeBaseResponse("E01");
			return false;
		}

		auto writeTraceStatus = [this, outData, &outSize, &writeBaseResponse, &writePacketBegin, &writePacketData, &writePacketEnd]() -> bool {
			bool success = writePacketBegin();
			success |= writePacketData(m_debugInterface->isAlive() ? "T1" : "T0", 2);
			// TODO: optional fields here
			success |= writePacketEnd();
			if (!success)
			{
				outSize = 0;
				return false;
			}

			return true;
		};

		switch (data[1])
		{
			case 'A':
				if (IsSameString(data, "qAttached"))
				{
					writeBaseResponse("1");
					return false;
				}
				break;
			case 'f':
				if (IsSameString(&data[2], "ThreadInfo"))
				{
					m_stateThreadCounter = 0;

					DEBUG_WRITE("GDB: thread info enumerate begin");
					updateThreadList();
					if (!writeThreadInfo())
					{
						Console.Warning("GDB: failed to get thread info.");
						writeBaseResponse("E01");
					}

					return false;
				}
				break;
			case 's':
				if (IsSameString(&data[2], "ThreadInfo"))
				{ 
					if (!writeThreadInfo())
					{
						Console.Warning("GDB: failed to get thread info.");
						writeBaseResponse("E01");
					}

					return false;
				}

				break;

			case 'C': { // get current thread 
				const auto currentThread = m_debugInterface->getCurrentThread();
				if (currentThread == nullptr)
				{
					writeBaseResponse("E01");
					return false;
				}

				bool success = writePacketBegin();
				success |= writePacketData("QC", 2);
				success |= writeThreadId(currentThread->TID());
				success |= writePacketEnd();
				if (!success)
				{
					outSize = 0;
					writeBaseResponse("E01");
				}

				return false;
			}
			break;

			case 'S':
				if (IsSameString(data, "qSymbol:"))
				{
					DEBUG_WRITE("GDB: symbol request");
					writeBaseResponse("OK");
					return false;
				}

				if (IsSameString(data, "qSupported"))
				{
					DEBUG_WRITE("GDB: supported features");
					writeBaseResponse(GDB_FEATURES);
					return false;
				}
				break;
			case 'T':
				if (IsSameString(data, "qTStatus"))
				{
					DEBUG_WRITE("GDB: qTStatus");
					if (!writeTraceStatus()) 
					{
						Console.Warning("GDB: failed to write thread status.");
						writeBaseResponse("E01");
					}
					
					return false;
				}

				if (IsSameString(data, "qThreadExtraInfo"))
				{
					writeBaseResponse("00");
					return false;
				}
				break;
			case 'X':
				if (IsSameString(data, "qXfer:"))
				{
					if (data.size() < 7)
					{
						Console.Warning("GDB: invalid \"qXfer\" packet.");
						writeBaseResponse("E01");
						return false;
					}

					return processXferPacket(std::string_view(data.data() + 6, data.size() - 6));
				}
				break;

			default:
				break;
		}

		// we don't support this command rn
		Console.Warning("GDB: unknown query operation [%s]", data.data());
		writeBaseResponse("");
		return false;
	};
	
	auto processGeneralPacket = [this,&writeBaseResponse](std::string_view data) -> bool {
		const std::string_view threadEventsString = "QThreadEvents:";
		if (IsSameString(data, threadEventsString))
		{
			const char* eventsEnableString = data.data() + threadEventsString.size();
			if (*eventsEnableString == '1')
				m_eventsEnabled = true;
			else if (*eventsEnableString == '0')
				m_eventsEnabled = false;
			else
			{
				Console.Warning("GDB: invalid \"QThreadEvents\" packet.");
				writeBaseResponse("E01");
				return false;
			}

			DEBUG_WRITE("GDB: thread events");
			writeBaseResponse("OK");
			return false;
		}

		Console.Warning("GDB: unknown general operation [%s]", data.data());
		writeBaseResponse("");
		return false;
	};

	auto processMultiLetterPacket = [this, &writeBaseResponse](std::string_view data) -> bool {
		if (IsSameString(data, "vMustReplyEmpty"))
		{
			DEBUG_WRITE("GDB: must reply empty");
			writeBaseResponse("");
			return false;
		}

		if (IsSameString(data, "vCtrlC"))
		{
			DEBUG_WRITE("GDB: ctrl+c interrupt");
			writeBaseResponse("OK");
			return false;
		}

		if (IsSameString(data, "vCont"))
		{
			if (data[5] == '?')
			{
				DEBUG_WRITE("GDB: vCont support");
				writeBaseResponse("vCont;c;C;s;S;t");
				return false;
			}

			if (data[5] == ';')
			{
				if (!m_debugInterface->isAlive())
				{
					writeBaseResponse("E01");
					return false;
				}

				// #TODO: 
				DEBUG_WRITE("GDB: vCont [%s]", data.data());
				switch (data[6])
				{
					case 'c':
						writeBaseResponse("E01");
						return false;
					case 's':
						writeBaseResponse("E01");
						return false;
					case 't':
						writeBaseResponse("E01");
						return false;
					case 'r':
						writeBaseResponse("E01");
						return false;
					default:
						writeBaseResponse("E01");
						return false;
				}
			}

			// invalid packet, don't process it
			Console.Warning("GDB: invalid \"vCont\" packet.");
			writeBaseResponse("E01");
			return false;
		}

		// we don't support this command rn
		Console.Warning("GDB: unknown \"vCont\" operation [%s]", data.data());
		writeBaseResponse("");
		return false;
	};

	auto processThreadOperations = [this, &writeBaseResponse](std::string_view data) -> bool {
		switch (data[1])
		{
			case 'c':
				if (IsSameString(data.data() + 2, 2, "-1", 2))
				{
					if (!m_debugInterface->isAlive() || !m_debugInterface->isCpuPaused())	
					{
						Console.Warning("GDB: trying to continue cpu execution when it's impossible.");
						writeBaseResponse("OK");	
						return false;	
					}

					DEBUG_WRITE("GDB: resume cpu");
					m_debugInterface->resumeCpu();
					writeBaseResponse("OK");
					return false;
				}
				
				Console.Warning("GDB: NOT IMPLEMENTED: continue for thread.");
				writeBaseResponse("E01");	
				break;
			default:
				break;
		}

		Console.Warning("GDB: unknown thread operation [%s]", data.data());
		writeBaseResponse("");
		return false;
	};

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
	
	switch (inData[offset])
	{
		case '!':
			writeBaseResponse("OK");
			return packetEnd;

		case 'Q': // general set
			if (!processGeneralPacket(data))
				return packetEnd;
			break;
		case 'q': // general query
			if (!processQueryPacket(data))
				return packetEnd;
			break;

		case 'v': // multi-letter named packet
			if (!processMultiLetterPacket(data))
				return packetEnd;
			break;

		case 'z': // remove watchpoint (may be breakpoint or memory breal)
			break;
		case 'Z': // insert watchpoint (may be breakpoint or memory breal)
			break;

		case 'H':
			if (!processThreadOperations(data))
				return packetEnd;
			break;

		case 'D':
			writeBaseResponse("OK");
			return packetEnd;	
		case 'R':
			break;
		case 'C': // continue with signal
			if (!m_debugInterface->isAlive() || !m_debugInterface->isCpuPaused())	
			{
				Console.Warning("GDB: trying to continue cpu execution when it's impossible.");
				writeBaseResponse("OK");	
				return packetEnd;	
			}

			DEBUG_WRITE("GDB: resume cpu");
			m_debugInterface->resumeCpu();
			writeBaseResponse("OK");
			return packetEnd;	
		case 'c': // continue
			if (!m_debugInterface->isAlive() || !m_debugInterface->isCpuPaused())	
			{
				Console.Warning("GDB: trying to continue cpu execution when it's impossible.");
				writeBaseResponse("OK");	
				return packetEnd;	
			}

			DEBUG_WRITE("GDB: resume cpu");
			m_debugInterface->resumeCpu();
			writeBaseResponse("OK");
			return packetEnd;	
		case 's': // step-out
			break;
		case '?': // signal
			writeBaseResponse(m_debugInterface->isCpuPaused() ? "S05" : "S00");	
			return packetEnd;	
			break;

		case 'g': // read registers
			break;
		case 'G': // write registers
			break;
		case 'p': // read register
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
			break;
	}

	// we can't process this packet so we just push "end of the packet".
	Console.Warning("GDB: unknown GDB packet [%s].", inData);
	writeBaseResponse("");
	return packetEnd;
}
