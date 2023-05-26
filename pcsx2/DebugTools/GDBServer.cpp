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
#undef min
#undef max
#include <charconv>

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
	";qXfer:memory-map:read-"\
	";qXfer:sdata:read-"\
	";qXfer:siginfo:read-"\
	";qXfer:traceframe-info:read-"\
	";hwbreak+" \
	";swbreak+" \
	";BreakpointCommands+" \
	";vContSupported+" \
	";QThreadEvents+" \
	";memory-tagging-" \
	";tracenz-" \
	";ConditionalBreakpoints+" \
	";ConditionalTracepoints-" \
	";TracepointSource-" \
	";EnableDisableTracepoints-" 

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

inline
std::size_t
GetRegisterNumber(DebugInterface* cpuInterface, std::size_t cat, std::size_t idx)
{
	for (size_t i = 0; i < cat; i++)
	{
		idx += cpuInterface->getRegisterCount(i);
	}

	return idx;
}

inline
bool
GetRegisterCategoryAndIndex(DebugInterface* cpuInterface, std::size_t number, std::size_t& cat, std::size_t& idx)
{
	std::size_t acc = 0;
	for (; cat < cpuInterface->getRegisterCategoryCount(); cat++)
	{
		if (number >= acc && number < acc + cpuInterface->getRegisterCount(cat)) 
		{
			break;	
		}

		acc += cpuInterface->getRegisterCount(cat);
	}

	if (cat == cpuInterface->getRegisterCategoryCount())
	{
		return false;
	}

	idx = number - acc;
	return true;
}

#define NEW_LINE "\n"

inline
std::string
GetFeatureString(DebugInterface* cpuInterface)
{
	std::string featureString;

	auto getRegisterType = [](bool isFloat, std::size_t size) {
		if (isFloat)
		{
			switch (size)
			{
				case 8:
					return " type=\"uint8\"";
				case 16:
					return " type=\"uint16\"";
				case 32:
					return isFloat ? " type=\"ieee_single\"" : " type=\"uint32\"";
				case 64:
					return isFloat ? " type=\"ieee_double\"" : " type=\"uint64\"";
				case 128:
					return isFloat ? " type=\"vec128\"" : " type=\"uint128\"";
				default:
					return "";
					break;
			}
		}
		return "";
	};

	featureString += "<?xml version=\"1.0\"?>" NEW_LINE;
	featureString += "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">" NEW_LINE;
	featureString += "<target version=\"1.0\">" NEW_LINE;
	featureString += NEW_LINE;
	if (cpuInterface->getCpuType() == BREAKPOINT_VU0 || cpuInterface->getCpuType() == BREAKPOINT_VU1)
	{
		featureString += "<architecture>sonyvu</architecture>" NEW_LINE;
		featureString += "<feature name=\"pcsx2.vu\">" NEW_LINE;
	}
	else
	{
		featureString += "<architecture>mips:sony</architecture>" NEW_LINE;
		featureString += "<feature name=\"org.gnu.gdb.mips.cpu\">" NEW_LINE;
	}

	// adding support for 128-bit registers
	featureString += "<vector id=\"v4f\" type=\"ieee_single\" count=\"4\"/>" NEW_LINE;
	featureString += "<vector id=\"v2d\" type=\"ieee_double\" count=\"2\"/>" NEW_LINE;
	featureString += "<vector id=\"v16i8\" type=\"int8\" count=\"16\"/>" NEW_LINE;
	featureString += "<vector id=\"v8i16\" type=\"int16\" count=\"8\"/>" NEW_LINE;
	featureString += "<vector id=\"v4i32\" type=\"int32\" count=\"4\"/>" NEW_LINE;
	featureString += "<vector id=\"v2i64\" type=\"int64\" count=\"2\"/>" NEW_LINE;
	featureString += NEW_LINE;

	featureString += "<union id=\"vec128\">" NEW_LINE;
	featureString += "    <field name=\"v4_float\" type=\"v4f\"/>" NEW_LINE;
	featureString += "    <field name=\"v2_double\" type=\"v2d\"/>" NEW_LINE;
	featureString += "    <field name=\"v16_int8\" type=\"v16i8\"/>" NEW_LINE;
	featureString += "    <field name=\"v8_int16\" type=\"v8i16\"/>" NEW_LINE;
	featureString += "    <field name=\"v4_int32\" type=\"v4i32\"/>" NEW_LINE;
	featureString += "    <field name=\"v2_int64\" type=\"v2i64\"/>" NEW_LINE;
	featureString += "    <field name=\"uint128\" type=\"uint128\"/>" NEW_LINE;
	featureString += "</union>" NEW_LINE;
	featureString += NEW_LINE;

	for (std::size_t cat = 0; cat < cpuInterface->getRegisterCategoryCount(); cat++)
	{
		if (cpuInterface->getCpuType() == BREAKPOINT_EE)
		{
			switch (cat)
			{
				case EECAT_GPR:
					featureString += "<feature name=\"org.gnu.gdb.mips.cpu\">" NEW_LINE;
					break;
				case EECAT_CP0:
					featureString += "<feature name=\"org.gnu.gdb.mips.cp0\">" NEW_LINE;
					break;
				case EECAT_FPR:
					featureString += "<feature name=\"org.gnu.gdb.mips.fpu\">" NEW_LINE;
					break;
				default:
					continue;
			} 

			std::string group = cpuInterface->getRegisterCategoryName(cat);
			for (std::size_t i = 0; i < cpuInterface->getRegisterCount(cat); i++)
			{
				std::string name = cpuInterface->getRegisterName(cat, i);
				std::string bitsize = std::to_string(cpuInterface->getRegisterSize(cat));
				std::string regnum = std::to_string(GetRegisterNumber(cpuInterface, cat, i));
				featureString +=
					"    <reg name=\"" + name +
					"\" bitsize=\"" + bitsize +
					"\" regnum=\"" + regnum +
					"\" group=\"" + group + "\"" +
					getRegisterType((cat == EECAT_VU0F || cat == EECAT_FPR) ? true : false, cpuInterface->getRegisterSize(cat)) +
					" />" NEW_LINE;
			}

			featureString += "</feature>" NEW_LINE;
			featureString += NEW_LINE;	
		}
		else
		{
		
		}
	}

	featureString += "</target>" NEW_LINE;	

	return featureString;
}

inline
std::string
GetCPUThreads(DebugInterface* cpuInterface)
{
	bool paused = cpuInterface->isCpuPaused();
	std::string threadsString;
	threadsString += "<?target version=\"1.0\"?>\n";
	threadsString += "<threads>\n";

	if (!paused)
	{
		cpuInterface->pauseCpu();
		while (cpuInterface->isCpuPaused())
		{
			Threading::Sleep(1);
		}
	}	

	for (const auto& threadHandle : cpuInterface->GetThreadList())
	{
		threadsString += "<thread id=\"" + std::to_string(threadHandle->TID()) + "\"";
		threadsString += " name=\"" + std::to_string(threadHandle->EntryPoint()) + "\"";	
	}
	
	if (!paused)
	{
		cpuInterface->resumeCpu();
	}

	threadsString += "</threads>\n";
	return threadsString;
}

GDBServer::GDBServer(DebugInterface* debugInterface)
{
	m_debugInterface = debugInterface;
	m_featuresString = GetFeatureString(m_debugInterface);
}

GDBServer::~GDBServer()
{
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
		if (outSize + (m_connected ? 1 : 2) >= MAX_DEBUG_PACKET_SIZE)
		{
			return false;
		}

		char* writeData = reinterpret_cast<char*>(outData) + outSize;
		if (!m_connected)
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
		const u8 checksum = CalculateChecksum(data + (m_connected ? 1 : 2), outSize - (m_connected ? 1 : 2));

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

	if (inSize == 1 && (*inData == '+' || *inData == '-'))
	{
		char* writeData = reinterpret_cast<char*>(outData);
		writeData[outSize++] = *inData;
		return inSize;
	}

	while (offset < inSize)
	{
		if (inData[offset++] == '$')
			break;
	}

	if (offset == inSize)
	{
		// Maybe invalid data or something went wrong, don't care at this case
		Console.WriteLn(Color_StrongOrange, "DebugNetworkServer: invalid GDB packet (no '$' was received).");
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
		Console.WriteLn(Color_StrongOrange, "DebugNetworkServer: invalid GDB packet (no '#' was received).");
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
		Console.WriteLn(Color_StrongOrange, "DebugNetworkServer: invalid GDB checksum (%u != %u).", calculatedChecksum, readedChecksum);
		return std::size_t(-1);
	}
	auto processXferPacket = [this, outData, &outSize, &writeBaseResponse, &writePacketBegin, &writePacketData, &writePacketEnd, makeStringView](std::string_view data) -> bool {
		std::size_t localOffset = 0;
		const auto verbString = makeStringView(data, localOffset, ':', ':');
		const auto sentenceString = std::string_view(data.data(), localOffset - verbString.size() - 1);
		const auto annexString = makeStringView(data, localOffset, ':', ':');
		const auto offsetString = makeStringView(data, localOffset, ':', ',');
		const auto lengthString = makeStringView(data, localOffset, ',', '\0');
		if (verbString.empty() || annexString.empty() || offsetString.empty() || lengthString.empty())
		{
			writeBaseResponse("E01");
			return false;
		}

		std::size_t offset = 0;
		std::size_t length = 0;
		if (std::from_chars(offsetString.data(), offsetString.data() + offsetString.size(), offset, 16).ec != std::errc())
		{
			writeBaseResponse("E01");
			return false;
		}	
		
		if (std::from_chars(lengthString.data(), lengthString.data() + lengthString.size(), length, 16).ec != std::errc())
		{
			writeBaseResponse("E01");
			return false;
		}

		if (IsSameString(sentenceString, "features"))
		{
			char firstSymbol = 'm';
			if (m_featuresString.size() - offset < length)
			{
				offset = std::min(m_featuresString.size() - 1, offset);
				length = m_featuresString.size() - offset;
				firstSymbol = 'l';
			}

			bool success = writePacketBegin();
			success |= writePacketData(&firstSymbol, 1);
			success |= writePacketData(m_featuresString.data() + offset, length);
			success |= writePacketEnd();

			if (!success)
			{
				outSize = 0;
				writeBaseResponse("E01");
			}

			return false;
		}
		
		if (IsSameString(sentenceString, "threads"))
		{
		}

		/*
		if (IsSameString(data, featuresRequestString))
		{
			std::string featuresString = GetFeatureString(m_debugInterface);
			writeBaseResponse(featuresString.c_str());
			return false;
		}		
		
		if (IsSameString(data, "threads"))
		{
			std::string threadsString = GetCPUThreads(m_debugInterface);
			writeBaseResponse(threadsString.c_str());
			return false;
		}
		*/

		// we don't support other 
		writeBaseResponse("");
		return false;
	};

	// true - continue packets processing
	// false - stop and send packet as is
	auto processQueryPacket = [outData, &outSize, &offset, &writeBaseResponse, &processXferPacket](std::string_view data) -> bool {
		if (data.empty())
		{
			writeBaseResponse("E01");
			return false;
		}

		switch (data[1])
		{
			case 'C': // get current thread
				break;
			case 'S':
				if (IsSameString(data, "qSymbol:"))
				{
					writeBaseResponse("OK");
					return false;
				}

				if (IsSameString(data, "qSupported"))
				{
					writeBaseResponse(GDB_FEATURES);
					return false;
				}
				break;
			case 'X':
				if (IsSameString(data, "qXfer:"))
				{
					if (data.size() < 7)
					{
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
				writeBaseResponse("E01");
				return false;
			}

			writeBaseResponse("OK");
			return false;
		}

		writeBaseResponse("");
		return false;
	};

	auto processMultiLetterPacket = [this, &writeBaseResponse](std::string_view data) -> bool {
		if (IsSameString(data, "vMustReplyEmpty"))
		{
			writeBaseResponse("");
			return false;
		}

		if (IsSameString(data, "vCtrlC"))
		{
			writeBaseResponse("OK");
			return false;
		}

		if (IsSameString(data, "vCont"))
		{
			if (data[5] == '?')
			{
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
			writeBaseResponse("E01");
			return false;
		}

		// we don't support this command rn
		writeBaseResponse("");
		return false;
	};

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

		case 'R':
			break;
		case 'c': // continue
			break;
		case 's': // step-out
			break;
		case '?': // signal
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
	writeBaseResponse("");
	return packetEnd;
}
