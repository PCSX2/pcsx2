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
#include "common/StringUtil.h"

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
	";qXfer:threads:read+"\
	";qXfer:libraries:read-"\
	";qXfer:memory-map:read+"\
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
	";ConditionalBreakpoints-" \
	";ConditionalTracepoints-" \
	";TracepointSource-" \
	";EnableDisableTracepoints-" 

static const std::string_view targetIOPXML = R"(<?xml version="1.0"?>
<!DOCTYPE feature SYSTEM "gdb-target.dtd">
<target version="1.0">

<!-- Helping GDB -->
<architecture>mips:3000</architecture>
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

static const std::string_view targetEEXML = R"(<?xml version="1.0"?>
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
	00000000h  32 MB    Main RAM (first 1 MB reserved for kernel)
	20000000h  32 MB    Main RAM, uncached
	30100000h  31 MB    Main RAM, uncached and accelerated
	10000000h  64 KB    I/O registers
	11000000h  4 KB     VU0 code memory
	11004000h  4 KB     VU0 data memory
	11008000h  16 KB    VU1 code memory
	1100C000h  16 KB    VU1 data memory
	12000000h  8 KB     GS privileged registers
	1C000000h  2 MB     IOP RAM
	1FC00000h  4 MB     BIOS, uncached (rom0)
	9FC00000h  4 MB     BIOS, cached (rom09)
	BFC00000h  4 MB     BIOS, uncached (rom0b)
	70000000h  16 KB    Scratchpad RAM (only accessible via virtual addressing)
*/
static const std::string_view EEMemoryMap = R"(<?xml version="1.0"?>
<memory-map>
    <!-- Main memory block -->
    <memory type="ram" start="0x0000000000000000" length="0x2000000"/>
    <memory type="ram" start="0x0000000020000000" length="0x2000000"/>
    <memory type="ram" start="0x0000000030100000" length="0x1f00000"/>

    <!-- I/O registers -->
    <memory type="ram" start="0x0000000010000000" length="0x0010000"/>

  	<!-- VU memory -->
    <memory type="ram" start="0x0000000011000000" length="0x0001000"/>
    <memory type="ram" start="0x0000000011004000" length="0x0001000"/>
    <memory type="ram" start="0x0000000011008000" length="0x0004000"/>
    <memory type="ram" start="0x000000001100c000" length="0x0004000"/>

    <!-- GS memory -->
    <memory type="ram" start="0x0000000012000000" length="0x0002000"/>
    
    <!-- IOP memory -->
	<memory type="ram" start="0x000000001c000000" length="0x0200000"/>

  	<!-- BIOS -->
	<memory type="rom" start="0x000000001fc00000" length="0x0400000"/>
	<memory type="rom" start="0x000000009fc00000" length="0x0400000"/>
	<memory type="rom" start="0x00000000bfc00000" length="0x0400000"/>

    <!-- Scratchpad -->
	<memory type="ram" start="0x0000000070000000" length="0x0200000"/>
</memory-map>
)";

/*
	KUSEG: 00000000h-7FFFFFFFh User segment
	KSEG0: 80000000h-9FFFFFFFh Kernel segment 0
	KSEG1: A0000000h-BFFFFFFFh Kernel segment 1
	
	Physical
	00000000h  2 MB     Main RAM (same as on PSX)
	1D000000h           SIF registers
	1F800000h  64 KB    Various I/O registers
	1F900000h  1 KB     SPU2 registers
	1FC00000h  4 MB     BIOS (rom0) - Same as EE BIOS
	
	FFFE0000h (KSEG2)   Cache control
*/
static const std::string_view IOPMemoryMap = R"(<?xml version="1.0"?>
<memory-map>
    <!-- Main memory block -->
	<memory type="ram" start="0x0000000000000000" length="0x0200000"/>
	<memory type="ram" start="00000000001d000000" length="0x0200000"/>
	<memory type="ram" start="00000000001f800000" length="0x0010000"/>
	<memory type="ram" start="00000000001f900000" length="0x0004000"/>

  	<!-- BIOS -->
	<memory type="rom" start="00000000001fc00000" length="0x0400000"/>
</memory-map>
)";

std::mutex CPUTransaction;

inline bool ObtainCategoryAndIndex(int id, int& cat, int& idx)
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
T ReverseBytes(T value)
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

#define BIG_ENDIFY_IT(x) do { if (!IsBigEndian()) { x = ReverseBytes(x); } } while(false);

constexpr u8 ASCIIToValue(char symbol)
{
	if ((symbol >= '0') && (symbol <= '9'))
		return (symbol - '0');
	else if ((symbol >= 'a') && (symbol <= 'f'))
		return (symbol - 'a' + 0xa);
	else if ((symbol >= 'A') && (symbol <= 'F'))
		return (symbol - 'A' + 0xa);

	return 0;
}

constexpr int ValueToASCII(u8 value)
{
	constexpr const char* digitsLookup = "0123456789abcdef";
	return digitsLookup[std::clamp(value, (u8)0, (u8)16)];
}

template<typename T>
bool WriteHexValue(char* string, std::size_t& outSize, T value)
{
	if (outSize + sizeof(T) * 2 > MAX_DEBUG_PACKET_SIZE)
		return false;
	
	// All hex values in GDB format is big-endian
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
T ReadHexValue(const char* string, std::size_t stringSize)
{
	T value = 0;

	for (size_t i = 0; i < stringSize / 2; i++)
	{
		u8* writePtr = reinterpret_cast<u8*>(&value) + i;
		u8 rawValue = ASCIIToValue(string[i * 2]) << 4;
		if (i * 2 + 1 < stringSize)
			rawValue |= ASCIIToValue(string[i * 2 + 1]);

		*writePtr = rawValue;
	}

	// All hex values in GDB format is big-endian
	BIG_ENDIFY_IT(value);
	return value;
}

template <typename T>
T ReadHexValue(std::string_view string)
{
	return ReadHexValue<T>(string.data(), string.size());
}

constexpr u8 CalculateChecksum(
	const char* data,
	std::size_t size
)
{
	u8 checksum = 0;
	for (std::size_t i = 0; i < size; i++)
		checksum += data[i];

	return checksum;
}

constexpr u8 CalculateChecksum(std::string_view data)
{
	return CalculateChecksum(data.data(), data.size());
}

constexpr bool IsSameString(const char* source, std::size_t sourceLength, const char* compare, std::size_t compareLength)
{
	for (size_t i = 0; i < std::min(sourceLength, compareLength); i++)
		if (source[i] != compare[i])
			return false;

	return true;
}

constexpr bool IsSameString(std::string_view source, std::string_view compare)
{
	return IsSameString(source.data(), source.size(), compare.data(), compare.size());
}

static std::string_view MakeStringView(std::string_view data, std::size_t& offset, char symbol, char nextSymbol, bool notEqual = false)
{
	const std::size_t beginIndex = (symbol == '\0' ?
		offset : 
		data.find_first_of(symbol, offset)
	);

	const std::size_t endIndex = (nextSymbol == '\0' ? 
		data.size() : 
		(notEqual ? 
			data.find_first_not_of(nextSymbol, beginIndex + 1) : 
			data.find_first_of(nextSymbol, beginIndex + 1)
		)
	);

	if (beginIndex == std::size_t(-1) || endIndex == std::size_t(-1))
		return {};

	offset = endIndex;
	return std::string_view(data.data() + beginIndex + 1, endIndex - beginIndex - 1);
};

// Executes code only if CPU is paused
template<typename T>
void ExecuteCPUTask(DebugInterface* cpuInterface, T&& func)
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

void GDBServer::resumeExecution()
{
	std::scoped_lock<std::mutex> sc(CPUTransaction);
	if (m_debugInterface->isAlive() && m_debugInterface->isCpuPaused())
		m_debugInterface->resumeCpu();
}

void GDBServer::stopExecution()
{
	std::scoped_lock<std::mutex> sc(CPUTransaction);
	if (m_debugInterface->isAlive() && !m_debugInterface->isCpuPaused())
		m_debugInterface->pauseCpu();
}

void GDBServer::singleStep()
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

bool GDBServer::addBreakpoint(u32 address)
{
	if (!m_debugInterface->isValidAddress(address))
		return false;

	ExecuteCPUTask(m_debugInterface, [this, address]() { CBreakPoints::AddBreakPoint(m_debugInterface->getCpuType(), address); });
	return true;
}

bool GDBServer::removeBreakpoint(u32 address)
{
	if (!m_debugInterface->isValidAddress(address))
		return false;

	ExecuteCPUTask(m_debugInterface, [this, address]() { CBreakPoints::RemoveBreakPoint(m_debugInterface->getCpuType(), address); });
	return true;
}

void GDBServer::updateThreadList()
{
	ExecuteCPUTask(m_debugInterface, [this]() { m_stateThreads = m_debugInterface->getThreadList(); });
}

void GDBServer::generateThreadListString()
{
	updateThreadList();

	m_threadListString = "<?target version=\"1.0\"?>\n";
	m_threadListString += "<threads>\n";
	for (const auto& thread : m_stateThreads)
	{
		char tempString[64] = {};
		if (m_multiprocess)
			snprintf(tempString, 64, "<thread id=\"%x.%x\" />\n", 1, thread->TID() + 1);
		else
			snprintf(tempString, 64, "<thread id=\"%x\" />\n", thread->TID() + 1);

		m_threadListString += tempString;
	}

	m_threadListString += "</threads>\n";
}

u32 GDBServer::getRegisterSize(int id)
{
	return 32;
}

bool GDBServer::readRegister(int threadId, int id, u32& value)
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

bool GDBServer::writeRegister(int threadId, int id, u32 value)
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

bool GDBServer::readMemory(u8* data, u32 address, u32 length)
{
	if (!m_debugInterface->isAlive() || m_debugInterface->isCpuPaused())
		return false;

	for (size_t i = 0; i < length; i++)
		data[i] = m_debugInterface->read8(address + i);

	return true;
}

bool GDBServer::writeMemory(const u8* data, u32 address, u32 length)
{
	if (!m_debugInterface->isAlive() || m_debugInterface->isCpuPaused())
		return false;

	for (size_t i = 0; i < length; i++)
		m_debugInterface->write8(address + i, data[i]);

	return true;
}

bool GDBServer::writePacketBegin()
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

bool GDBServer::writePacketEnd()
{
	std::size_t& outSize = *m_outSize;
	if (outSize + 3 >= MAX_DEBUG_PACKET_SIZE)
		return false;

	char* data = reinterpret_cast<char*>(m_outData);
	const u8 checksum = CalculateChecksum(data + (m_dontReplyAck ? 1 : 2), outSize - (m_dontReplyAck ? 1 : 2));

	// Every packet in GDB protocol must be ended with '#' symbol and checksum in hex format.
	data += outSize;
	*data++ = '#';
	*data++ = ValueToASCII((checksum >> 4) 	& 0xf);
	*data++ = ValueToASCII((checksum) 		& 0xf);
	outSize += 3;

	return true;
}

bool GDBServer::writePacketData(const char* data, std::size_t size)
{
	std::size_t& outSize = *m_outSize;
	if (outSize + size >= MAX_DEBUG_PACKET_SIZE)
		return false;

	std::memcpy((char*)m_outData + outSize, data, size);
	outSize += size;
	return true;
}

bool GDBServer::writePacketBaseResponse(std::string_view data)
{
	if (!writePacketBegin())
		return false;

	if (!writePacketData(data.data(), data.size()))
		return false;

	if (!writePacketEnd())
		return false;

	return true;
}

bool GDBServer::writePacketThreadId(int threadId, int processId)
{
	char threadIdString[16] = {};
	int charsWritten = 0;

	// Thread id in GDB format must be constructed in "AA.BB" format for connection with 
	// "multiprocess" extension or in "BB" format for other connections.
	if (m_multiprocess)
		charsWritten = snprintf(threadIdString, 16, "%x.%x", processId, threadId);
	else
		charsWritten = snprintf(threadIdString, 16, "%x", threadId);

	if (charsWritten < 1)
		return false;

	return writePacketData(threadIdString, charsWritten);
}

bool GDBServer::writePacketMemoryReadValues(u32 address, u32 length)
{
	std::size_t& outSize = *m_outSize;
	char* buffer = reinterpret_cast<char*>(m_outData);
	if (outSize + length * 2 >= MAX_DEBUG_PACKET_SIZE)
		return false;

	if (!m_debugInterface->isAlive() || m_debugInterface->isCpuPaused())
		return false;

	for (size_t i = 0; i < length; i++)
	{
		u8 value = m_debugInterface->read8(address + i);
		buffer[outSize++] = ValueToASCII((value) >> 4);
		buffer[outSize++] = ValueToASCII((value) & 0xf);
	}

	return true;
}

bool GDBServer::writePacketRegisterValue(int threadId, int registerNumber)
{
	std::size_t& outSize = *m_outSize;
	const u32 registerSize = getRegisterSize(registerNumber) / 4;
	const std::size_t finalSize = outSize + registerSize;
	if (finalSize >= MAX_DEBUG_PACKET_SIZE)
		return false;

	u32 value = 0;
	char* buffer = reinterpret_cast<char*>(m_outData);
	if (readRegister(threadId, registerNumber, value))
	{
		if (!WriteHexValue(buffer, outSize, value))
			return false;
	}
	else
	{
		while (outSize < finalSize)
			buffer[outSize++] = '0';
	}

	return true;
}

bool GDBServer::writePacketAllRegisterValues(int threadId)
{
	for	(int i = 0; i < 72; i++)
		if (!writePacketRegisterValue(threadId, i))
			return false;

	return true;
}

bool GDBServer::writePacketPaged(std::size_t offset, std::size_t length, const std::string_view& string)
{
	const char* firstSymbol = "m";
	if (string.size() - offset < length)
	{
		offset = std::min(string.size() - 1, offset);
		length = string.size() - offset;
		firstSymbol = "l";
	}

	bool success = writePacketBegin();
	success |= writePacketData(firstSymbol, 1);
	success |= writePacketData(string.data() + offset, length);
	success |= writePacketEnd();
	return success;
}

// true - continue packets processing
// false - stop and send packet as is
bool GDBServer::processXferPacket(std::string_view data)
{
	constexpr u8 featuresChecksum = CalculateChecksum("features");
	constexpr u8 threadsChecksum = CalculateChecksum("threads");
	constexpr u8 memoryMapChecksum = CalculateChecksum("memory-map");
		
	std::size_t localOffset = 0;
	const auto verbString = MakeStringView(data, localOffset, ':', ':');
	const auto sentenceString = std::string_view(data.data(), localOffset - verbString.size() - 1);
	const auto annexString = MakeStringView(data, localOffset, ':', ':');
	const auto offsetString = MakeStringView(data, localOffset, ':', ',');
	const auto lengthString = MakeStringView(data, localOffset, ',', '\0');
	(void)annexString;
	if (verbString.empty() || offsetString.empty() || lengthString.empty())
	{
		Console.Warning("GDB: one of the arguments for Xfer command was empty.");
		return false;
	}

	if (!IsSameString(verbString.data(), verbString.size(), "read", 4))
	{
		Console.Warning("GDB: only \"read\" operations are supported.");
		return false;
	}

	const bool isEE = (m_debugInterface->getCpuType() == BREAKPOINT_EE);
	const u8 sentenceChecksum = CalculateChecksum(sentenceString.data(), sentenceString.size());
	const std::size_t offset = ReadHexValue<std::size_t>(offsetString.data(), offsetString.size());
	const std::size_t length = ReadHexValue<std::size_t>(lengthString.data(), lengthString.size());
	switch (sentenceChecksum)
	{
		case featuresChecksum:
			DEBUG_WRITE("        features request");
			return writePacketPaged(offset, length, isEE ? targetEEXML : targetIOPXML);

		case threadsChecksum:
			DEBUG_WRITE("        threads request");
			if (m_threadListString.empty())
				generateThreadListString();

			return writePacketPaged(offset, length, std::string_view(m_threadListString.data(), m_threadListString.size()));

		case memoryMapChecksum:
			DEBUG_WRITE("        memory map request");
			return writePacketPaged(offset, length, isEE ? EEMemoryMap : IOPMemoryMap);

		default:
			break;
	}

	// we don't support other 
	Console.Warning("GDB: unsupported Xfer packet [%s].", data.data());
	writePacketBaseResponse("");
	return true;
}

bool GDBServer::processQueryPacket(std::string_view data)
{
	auto writeThreadInfo = [this]() -> bool {
		if (m_stateThreadCounter == -1)
			return false;

		if (m_stateThreads.size() <= static_cast<std::size_t>(m_stateThreadCounter))
		{
			writePacketBaseResponse("l");
			DEBUG_WRITE("         thread info end");
			return true;
		}

		bool success = writePacketBegin();
		success |= writePacketData("m", 1);
		success |= writePacketThreadId(m_stateThreads.at(m_stateThreadCounter)->TID());
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
				writePacketBaseResponse("1");
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
				return writePacketBaseResponse(m_multiprocess ? "QCp1.t1" : "QCt1");

			bool success = writePacketBegin();
			success |= writePacketData("QC", 2);
			success |= writePacketThreadId(currentThread->TID());
			success |= writePacketEnd();
			return success;
		}
		break;

		case 'S':
			if (IsSameString(data, "qSymbol:"))
			{
				DEBUG_WRITE("    symbol request");
				writePacketBaseResponse("OK");
				return true;
			}

			if (IsSameString(data, "qSupported"))
			{
				DEBUG_WRITE("    supported features request");
				writePacketBaseResponse(GDB_FEATURES);
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
				writePacketBaseResponse("00");
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
	writePacketBaseResponse("");
	return true;
}

bool GDBServer::processGeneralQueryPacket(std::string_view data)
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
			return false;

		DEBUG_WRITE("    events %s", m_eventsEnabled ? "enabled" : "disabled");
		writePacketBaseResponse("OK");
		return true;
	}

	Console.Warning("GDB: unknown general operation [%s]", data.data());
	writePacketBaseResponse("");
	return true;
}

bool GDBServer::processMultiletterPacket(std::string_view data)
{
	DEBUG_WRITE("GDB: processing multiletter packet...");
	if (IsSameString(data, "vMustReplyEmpty"))
	{
		DEBUG_WRITE("    must reply empty");
		writePacketBaseResponse("");
		return true; 
	}

	if (IsSameString(data, "vCtrlC"))
	{
		DEBUG_WRITE("    ctrl+c interrupt");
		writePacketBaseResponse("OK");
		return true;
	}

	if (IsSameString(data, "vCont"))
	{
		DEBUG_WRITE("    processing vCont packet...");
		if (data[5] == '?')
		{
			DEBUG_WRITE("        vCont supported features reply");
			writePacketBaseResponse("vCont;c;C;s;S;t");
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
	writePacketBaseResponse("");
	return true;
}

bool GDBServer::processThreadPacket(std::string_view data)
{
	DEBUG_WRITE("GDB: processing thread packet...");
	if (data[1] == 'c' || data[1] == 'g')
	{
		// Deprecated or unsupported right now by stub.
		return writePacketBaseResponse("OK");
	}

	Console.Warning("GDB: unknown thread operation [%s]", data.data());
	writePacketBaseResponse("");
	return true;
}

bool GDBServer::processReadRegisterPacket(std::string_view data)
{
	if (data.size() < 3)
		return false;

	u8 registedIdx = (ASCIIToValue(data[1]) << 4) | ASCIIToValue(data[2]);	
	bool success = writePacketBegin();
	success |= writePacketRegisterValue(0, registedIdx);
	success |= writePacketEnd();
	return success;
}

bool GDBServer::processWriteRegisterPacket(std::string_view data)
{
	return false;
}

bool GDBServer::processReadAllRegistersPacket(std::string_view data)
{
	bool success = writePacketBegin();
	success |= writePacketAllRegisterValues(0);
	success |= writePacketEnd();
	return success;
}

bool GDBServer::processWriteAllRegistersPacket(std::string_view data)
{
	return false;
}

bool GDBServer::processReadMemoryPacket(std::string_view data)
{
	std::size_t offset = 1;
	const auto addressString = MakeStringView(data, offset, '\0', ',');
	const auto lengthString = MakeStringView(data, offset, ',', ':');
	if (addressString.empty() || lengthString.empty())
	{
		Console.Warning("GDB: one of the fields in read memory packet are invalid.");
		return false;
	}

	const u32 address = ReadHexValue<u32>(addressString);
	const u32 length = ReadHexValue<u32>(lengthString);
	if (!m_debugInterface->isValidAddress(address))
	{
		Console.Warning("GDB: input address in write memory packet is invalid.");
		return false;
	}

	if (!m_debugInterface->isValidAddress(address + length - 1))
	{
		Console.Warning("GDB: input end address in write memory packet is invalid.");
		return false;
	}

	return writePacketMemoryReadValues(address, length);
}

bool GDBServer::processWriteMemoryPacket(std::string_view data, bool binary)
{
	std::size_t offset = 1;
	const auto addressString = MakeStringView(data, offset, '\0', ',');
	const auto lengthString = MakeStringView(data, offset, ',', ':');
	const auto dataString = MakeStringView(data, offset, ':', '\0');
	if (addressString.empty() || lengthString.empty() || dataString.empty())
	{
		Console.Warning("GDB: one of the fields in write memory packet are invalid.");
		return false;
	}

	const u32 address = ReadHexValue<u32>(addressString);
	const u32 length = ReadHexValue<u32>(lengthString);
	if (!m_debugInterface->isValidAddress(address))
	{
		Console.Warning("GDB: input address in write memory packet is invalid.");
		return false;
	}

	if (!m_debugInterface->isValidAddress(address + length - 1))
	{
		Console.Warning("GDB: input end address in write memory packet is invalid.");
		return false;
	}

	if (dataString.size() / 2 != length)
	{
		Console.Warning("GDB: data size in write memory packet is invalid (%i, expected %i).", dataString.size() / 2, length);
		return false;
	}
	
	const auto decodedData = StringUtil::DecodeHex(dataString);
	if (!decodedData.has_value())
	{
		Console.Warning("GDB: unnable to decode data string in write memory packet.");
		return false;
	}

	if (!writeMemory(decodedData->data(), address, decodedData->size()))
	{
		Console.Warning("GDB: unnable to write data in write memory packet.");
		return false;
	}

	return writePacketBaseResponse("OK");
}

void GDBServer::clearState()
{
	m_stateThreads.clear();
	m_threadListString.clear();
	m_waitingForTrap = false;
	m_multiprocess = false;
	m_eventsEnabled = false;
	m_dontReplyAck = false;
	m_wantsShutdown = false;
}

bool GDBServer::replyPacket(void* outData, std::size_t& outSize, bool& wantsShutdown)
{
	m_outSize = &outSize;
	m_outData = outData;

	const bool breakpointTriggered = CBreakPoints::GetBreakpointTriggered();
	if (breakpointTriggered)
	{
		CBreakPoints::ClearTemporaryBreakPoints();
		CBreakPoints::SetBreakpointTriggered(false);

		// Our current PC is on a breakpoint.
		// When we run the core again, we want to skip this breakpoint and run
		CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
		CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());
	}

	if (m_wantsShutdown)
	{
		wantsShutdown = true;

		// We want to continue execution of CPU when GDB is detached
		if (breakpointTriggered)
			resumeExecution();
	
		return writePacketBaseResponse("OK");
	}
	
	wantsShutdown = false;
	if (breakpointTriggered)
	{
		// Update thread list on every breakpoint signal
		updateThreadList();
		return writePacketBaseResponse("T05");	
	}

	outSize = 0;
	return true;
}

std::size_t GDBServer::processPacket(const char* inData, std::size_t inSize, void* outData, std::size_t& outSize)
{
	std::size_t offset = 0;

	// Ignore all ACK packets from GDB client
	if (inSize == 1 && (*inData == '+' || *inData == '-'))
	{
		return 0;
	}

	// Skip all symbols until we will not reach the beginning of packet 
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

	// Find end of the current packet
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
		/*
		case 'z': // remove watchpoint (may be breakpoint or memory breal)
			break;
		case 'Z': // insert watchpoint (may be breakpoint or memory breal)
			break;
		*/
	
		case '!': // advanced mode
			success = writePacketBaseResponse("OK");
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

		case 'H': // thread operations
			success = processThreadPacket(data);
			break;

		case 'D': // detach
			m_wantsShutdown = true;
			break;
			
		case 'C': // continue execution with signal
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
			success = writePacketBaseResponse(m_debugInterface->isCpuPaused() ? "S05" : "S00");	
			break;

		case 'g': // read registers
			success = processReadAllRegistersPacket(data);
			break;
		case 'G': // write registers
			success = processWriteAllRegistersPacket(data);
			break;
		case 'p': // read register
			success = processReadRegisterPacket(data);
			break;
		case 'P': // write register
			success = processWriteRegisterPacket(data);
			break;

		case 'm': // read memory
			success = processReadMemoryPacket(data);
			break;
		case 'M': // write memory
			success = processWriteMemoryPacket(data, false);
			break;
		case 'X': // write binary memory
			success = processWriteMemoryPacket(data, true);
			break;

		default:		
			Console.Warning("GDB: unknown packet \"%s\" was passed", inData);
			success = writePacketBaseResponse("");
			break;
	}

	if (!success)
	{
		Console.Error("GDB: failed to process GDB packet [%s].", inData);
		*m_outSize = 0;
		writePacketBaseResponse("E00");
	}

	return packetEnd;
}
