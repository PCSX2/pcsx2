// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MipsAssembler.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <string_view>

// just an empty class, so that it's not necessary to remove all the calls manually
// will make it easier to update if there are changes later on
class Logger
{
public:
	enum ErrorType { Warning, Error, FatalError, Notice };

	static void printError(ErrorType type, const wchar_t* text, ...)
	{
		//
	}

	static void queueError(ErrorType type, const wchar_t* text, ...)
	{
		//
	}

};

constexpr auto MipsRegisters = std::to_array<tMipsRegister>({
	{ "r0", 0},  { "zero", 0}, { "$0", 0 }, { "$zero", 0 },
	{ "at", 1},  { "r1", 1},   { "$1", 1 }, { "$at", 1 },
	{ "v0", 2},  { "r2", 2},   { "$v0", 2 },
	{ "v1", 3},  { "r3", 3},   { "$v1", 3 },
	{ "a0", 4},  { "r4", 4},   { "$a0", 4 },
	{ "a1", 5},  { "r5", 5},   { "$a1", 5 },
	{ "a2", 6},  { "r6", 6},   { "$a2", 6 },
	{ "a3", 7},  { "r7", 7},   { "$a3", 7 },
	{ "t0", 8},  { "r8", 8},   { "$t0", 8 },
	{ "t1", 9},  { "r9", 9},   { "$t1", 9 },
	{ "t2", 10}, { "r10", 10}, { "$t2", 10 },
	{ "t3", 11}, { "r11", 11}, { "$t3", 11 },
	{ "t4", 12}, { "r12", 12}, { "$t4", 12 },
	{ "t5", 13}, { "r13", 13}, { "$t5", 13 },
	{ "t6", 14}, { "r14", 14}, { "$t6", 14 },
	{ "t7", 15}, { "r15", 15}, { "$t7", 15 },
	{ "s0", 16}, { "r16", 16}, { "$s0", 16 },
	{ "s1", 17}, { "r17", 17}, { "$s1", 17 },
	{ "s2", 18}, { "r18", 18}, { "$s2", 18 },
	{ "s3", 19}, { "r19", 19}, { "$s3", 19 },
	{ "s4", 20}, { "r20", 20}, { "$s4", 20 },
	{ "s5", 21}, { "r21", 21}, { "$s5", 21 },
	{ "s6", 22}, { "r22", 22}, { "$s6", 22 },
	{ "s7", 23}, { "r23", 23}, { "$s7", 23 },
	{ "t8", 24}, { "r24", 24}, { "$t8", 24 },
	{ "t9", 25}, { "r25", 25}, { "$t9", 25 },
	{ "k0", 26}, { "r26", 26}, { "$k0", 26 },
	{ "k1", 27}, { "r27", 27}, { "$k1", 27 },
	{ "gp", 28}, { "r28", 28}, { "$gp", 28 },
	{ "sp", 29}, { "r29", 29}, { "$sp", 29 },
	{ "fp", 30}, { "r30", 30}, { "$fp", 30 },
	{ "ra", 31}, { "r31", 31}, { "$ra", 31 }
});

constexpr auto MipsFloatRegisters = std::to_array<tMipsRegister>({
	{ "f0", 0, },	{ "$f0", 0 },
	{ "f1", 1, },	{ "$f1", 1 },
	{ "f2", 2, },	{ "$f2", 2 },
	{ "f3", 3, },	{ "$f3", 3 },
	{ "f4", 4, },	{ "$f4", 4 },
	{ "f5", 5, },	{ "$f5", 5 },
	{ "f6", 6, },	{ "$f6", 6 },
	{ "f7", 7, },	{ "$f7", 7 },
	{ "f8", 8, },	{ "$f8", 8 },
	{ "f9", 9, },	{ "$f9", 9 },
	{ "f00", 0,},	{ "$f00", 0 },
	{ "f01", 1,},	{ "$f01", 1 },
	{ "f02", 2,},	{ "$f02", 2 },
	{ "f03", 3,},	{ "$f03", 3 },
	{ "f04", 4,},	{ "$f04", 4 },
	{ "f05", 5,},	{ "$f05", 5 },
	{ "f06", 6,},	{ "$f06", 6 },
	{ "f07", 7,},	{ "$f07", 7 },
	{ "f08", 8,},	{ "$f08", 8 },
	{ "f09", 9,},	{ "$f09", 9 },
	{ "f10", 10 },	{ "$f10", 10 },
	{ "f11", 11 },	{ "$f11", 11 },
	{ "f12", 12 },	{ "$f12", 12 },
	{ "f13", 13 },	{ "$f13", 13 },
	{ "f14", 14 },	{ "$f14", 14 },
	{ "f15", 15 },	{ "$f15", 15 },
	{ "f16", 16 },	{ "$f16", 16 },
	{ "f17", 17 },	{ "$f17", 17 },
	{ "f18", 18 },	{ "$f18", 18 },
	{ "f19", 19 },	{ "$f19", 19 },
	{ "f20", 20 },	{ "$f20", 20 },
	{ "f21", 21 },	{ "$f21", 21 },
	{ "f22", 22 },	{ "$f22", 22 },
	{ "f23", 23 },	{ "$f23", 23 },
	{ "f24", 24 },	{ "$f24", 24 },
	{ "f25", 25 },	{ "$f25", 25 },
	{ "f26", 26 },	{ "$f26", 26 },
	{ "f27", 27 },	{ "$f27", 27 },
	{ "f28", 28 },	{ "$f28", 28 },
	{ "f29", 29 },	{ "$f29", 29 },
	{ "f30", 30 },	{ "$f30", 30 },
	{ "f31", 31 },	{ "$f31", 31 }
});

constexpr auto MipsPs2Cop2FpRegisters = std::to_array<tMipsRegister>({
	{ "vf0", 0 },	{ "$vf0", 0 },
	{ "vf1", 1 },	{ "$vf1", 1 },
	{ "vf2", 2 },	{ "$vf2", 2 },
	{ "vf3", 3 },	{ "$vf3", 3 },
	{ "vf4", 4 },	{ "$vf4", 4 },
	{ "vf5", 5 },	{ "$vf5", 5 },
	{ "vf6", 6 },	{ "$vf6", 6 },
	{ "vf7", 7 },	{ "$vf7", 7 },
	{ "vf8", 8 },	{ "$vf8", 8 },
	{ "vf9", 9 },	{ "$vf9", 9 },
	{ "vf00", 0 },	{ "$vf00", 0 },
	{ "vf01", 1 },	{ "$vf01", 1 },
	{ "vf02", 2 },	{ "$vf02", 2 },
	{ "vf03", 3 },	{ "$vf03", 3 },
	{ "vf04", 4 },	{ "$vf04", 4 },
	{ "vf05", 5 },	{ "$vf05", 5 },
	{ "vf06", 6 },	{ "$vf06", 6 },
	{ "vf07", 7 },	{ "$vf07", 7 },
	{ "vf08", 8 },	{ "$vf08", 8 },
	{ "vf09", 9 },	{ "$vf09", 9 },
	{ "vf10", 10 },	{ "$vf10", 10 },
	{ "vf11", 11 },	{ "$vf11", 11 },
	{ "vf12", 12 },	{ "$vf12", 12 },
	{ "vf13", 13 },	{ "$vf13", 13 },
	{ "vf14", 14 },	{ "$vf14", 14 },
	{ "vf15", 15 },	{ "$vf15", 15 },
	{ "vf16", 16 },	{ "$vf16", 16 },
	{ "vf17", 17 },	{ "$vf17", 17 },
	{ "vf18", 18 },	{ "$vf18", 18 },
	{ "vf19", 19 },	{ "$vf19", 19 },
	{ "vf20", 20 },	{ "$vf20", 20 },
	{ "vf21", 21 },	{ "$vf21", 21 },
	{ "vf22", 22 },	{ "$vf22", 22 },
	{ "vf23", 23 },	{ "$vf23", 23 },
	{ "vf24", 24 },	{ "$vf24", 24 },
	{ "vf25", 25 },	{ "$vf25", 25 },
	{ "vf26", 26 },	{ "$vf26", 26 },
	{ "vf27", 27 },	{ "$vf27", 27 },
	{ "vf28", 28 },	{ "$vf28", 28 },
	{ "vf29", 29 },	{ "$vf29", 29 },
	{ "vf30", 30 },	{ "$vf30", 30 },
	{ "vf31", 31 },	{ "$vf31", 31 }
});

constexpr auto MipsPs2Cop2IRegisters = std::to_array<tMipsRegister>({
	{ "vi0", 0 },	{ "$vi0", 0 },
	{ "vi1", 1 },	{ "$vi1", 1 },
	{ "vi2", 2 },	{ "$vi2", 2 },
	{ "vi3", 3 },	{ "$vi3", 3 },
	{ "vi4", 4 },	{ "$vi4", 4 },
	{ "vi5", 5 },	{ "$vi5", 5 },
	{ "vi6", 6 },	{ "$vi6", 6 },
	{ "vi7", 7 },	{ "$vi7", 7 },
	{ "vi8", 8 },	{ "$vi8", 8 },
	{ "vi9", 9 },	{ "$vi9", 9 },
	{ "vi00", 0 },	{ "$vi00", 0 },
	{ "vi01", 1 },	{ "$vi01", 1 },
	{ "vi02", 2 },	{ "$vi02", 2 },
	{ "vi03", 3 },	{ "$vi03", 3 },
	{ "vi04", 4 },	{ "$vi04", 4 },
	{ "vi05", 5 },	{ "$vi05", 5 },
	{ "vi06", 6 },	{ "$vi06", 6 },
	{ "vi07", 7 },	{ "$vi07", 7 },
	{ "vi08", 8 },	{ "$vi08", 8 },
	{ "vi09", 9 },	{ "$vi09", 9 },
	{ "vi10", 10 },	{ "$vi10", 10 },
	{ "vi11", 11 },	{ "$vi11", 11 },
	{ "vi12", 12 },	{ "$vi12", 12 },
	{ "vi13", 13 },	{ "$vi13", 13 },
	{ "vi14", 14 },	{ "$vi14", 14 },
	{ "vi15", 15 },	{ "$vi15", 15 }
});

bool charEquals(char a, char b)
{
	return std::tolower(a) == std::tolower(b);
}

bool strEquals(std::string_view lhs, std::string_view rhs)
{
	const size_t compare_to = std::min(lhs.size(), rhs.size());
	return std::ranges::equal(lhs.begin(), lhs.begin() +  compare_to, rhs.begin(), rhs.begin() + compare_to, charEquals);
}

bool isValidRegisterTrail(std::string_view source, size_t idx)
{
	return source.size() <= idx || source[idx] == ',' || source[idx] == '\n'  || source[idx] == 0
	|| source[idx] == ')'  || source[idx] == '(' || source[idx] == '-';
}

void SplitLine(const char* Line, char* Name, char* Arguments)
{
	while (*Line == ' ' || *Line == '\t') Line++;
	while (*Line != ' ' && *Line != '\t')
	{
		if (*Line  == 0)
		{
			*Name = 0;
			*Arguments = 0;
			return;
		}
		*Name++ = *Line++;
	}
	*Name = 0;

	while (*Line == ' ' || *Line == '\t') Line++;

	while (*Line != 0)
	{
		*Arguments++ = *Line++;
	}
	*Arguments = 0;
}

bool MipsAssembleOpcode(const char* line, DebugInterface* cpu, u32 address, u32& dest, std::string& errorText)
{
	char name[64],args[256];
	SplitLine(line,name,args);

	CMipsInstruction opcode(cpu);
	if (cpu == NULL || !opcode.Load(name,args,(int)address))
	{
		errorText = opcode.getErrorMessage();
		return false;
	}

	if (!opcode.Validate())
	{
		errorText = "Parameter failure.";
		return false;
	}

	opcode.Encode();
	dest = opcode.getEncoding();

	return true;
}

template<std::size_t SIZE>
bool MipsGetRegister(const char* source, int& RetLen, MipsRegisterInfo& Result, const std::array<tMipsRegister, SIZE>& RegisterList)
{
	for (const auto& reg : RegisterList) {
		if(strEquals(reg.name, source) && isValidRegisterTrail(source, strlen(reg.name)))
		{
			std::strncpy(Result.name, source, strlen(reg.name));
			Result.num = reg.num;
			RetLen = static_cast<int>(strlen(reg.name)); // grrr... should be unsigned!!
			return true;
		}
	}
	return false;
}

template<std::size_t SIZE>
int MipsGetRegister(const char* source, int& RetLen, const std::array<tMipsRegister, SIZE>& RegisterList)
{
	for (const auto& reg : RegisterList) {
		if(strEquals(reg.name, source) && isValidRegisterTrail(source, strlen(reg.name)))
		{
			RetLen = static_cast<int>(strlen(reg.name)); // grrr... should be unsigned!!
			return reg.num;
		}
	}
	return -1;
}


bool MipsCheckImmediate(const char* Source, DebugInterface* cpu, int& dest, int& RetLen)
{
	char Buffer[512];
	int BufferPos = 0;
	int l;

	if (MipsGetRegister(Source,l, MipsRegisters) != -1)	// error
	{
		return false;
	}

	int SourceLen = 0;

	while (true)
	{
		if (*Source == '\'' && *(Source+2) == '\'')
		{
			Buffer[BufferPos++] = *Source++;
			Buffer[BufferPos++] = *Source++;
			Buffer[BufferPos++] = *Source++;
			SourceLen+=3;
			continue;
		}

		if (*Source == 0 || *Source == '\n' || *Source == ',')
		{
			Buffer[BufferPos] = 0;
			break;
		}
		if ( *Source == ' ' || *Source == '\t')
		{
			Source++;
			SourceLen++;
			continue;
		}


		if (*Source == '(')	// could be part of the opcode
		{
			if (MipsGetRegister(Source+1,l, MipsRegisters) != -1)	// end
			{
				Buffer[BufferPos] = 0;
				break;
			}
		}
		Buffer[BufferPos++] = *Source++;
		SourceLen++;
	}

	if (BufferPos == 0) return false;
	RetLen = SourceLen;

	PostfixExpression postfix;
	std::string error;
	if (!cpu->initExpression(Buffer,postfix,error))
		return false;

	u64 value;
	if (!cpu->parseExpression(postfix,value,error))
		return false;

	dest = (int) value;
	return true;
}

CMipsInstruction::CMipsInstruction(DebugInterface* cpu) :
	Opcode(), NoCheckError(false), Loaded(false), RamPos(0),
	registers(), immediateType(MIPS_NOIMMEDIATE), immediate(),
	vfpuSize(0), encoding(0), error()
{
	this->cpu = cpu;
}

bool CMipsInstruction::Load(const char* Name, const char* Params, int RamPos)
{
	bool paramfail = false;
	NoCheckError = false;
	this->RamPos = RamPos;

	const MipsArchDefinition& arch = mipsArchs[MARCH_PS2];
	for (int z = 0; MipsOpcodes[z].name != NULL; z++)
	{
		if ((MipsOpcodes[z].archs & arch.supportSets) == 0)
			continue;
		if ((MipsOpcodes[z].archs & arch.excludeMask) != 0)
			continue;

		if ((MipsOpcodes[z].flags & MO_64BIT) && !(arch.flags & MO_64BIT))
			continue;
		if ((MipsOpcodes[z].flags & MO_FPU) && !(arch.flags & MO_FPU))
			continue;

		if (parseOpcode(MipsOpcodes[z],Name))
		{
			if (LoadEncoding(MipsOpcodes[z],Params))
			{
				Loaded = true;
				return true;
			}
			paramfail = true;
		}
	}

	if (!NoCheckError)
	{
		if (paramfail)
		{
			error = "Parameter failure.";
		} else {
			error = "Invalid opcode.";
		}
	}
	return false;
}


bool CMipsInstruction::parseOpcode(const tMipsOpcode& SourceOpcode, const char* Line)
{
	vfpuSize = -1;

	const char* SourceEncoding = SourceOpcode.name;
	while (*SourceEncoding != 0)
	{
		if (*Line == 0) return false;

		switch (*SourceEncoding)
		{
		case 'S':	// vfpu size
			switch (*Line)
			{
			case 's':
				vfpuSize = 0;
				break;
			case 'p':
				vfpuSize = 1;
				break;
			case 't':
				vfpuSize = 2;
				break;
			case 'q':
				vfpuSize = 3;
				break;
			default:
				return false;
			}
			SourceEncoding++;
			Line++;
			break;
		default:
			if (*SourceEncoding++ != *Line++) return false;
			break;
		}
	}

	// there's something else, bad
	return (*Line == 0);
}

bool CMipsInstruction::LoadEncoding(const tMipsOpcode& SourceOpcode, const char* Line)
{
	immediateType = MIPS_NOIMMEDIATE;
	registers.reset();

	if (vfpuSize == -1)
	{
		if (SourceOpcode.flags & MO_VFPU_SINGLE)
			vfpuSize = 0;
		else if (SourceOpcode.flags & MO_VFPU_QUAD)
			vfpuSize = 3;
	}

	const char* SourceEncoding = SourceOpcode.encoding;

	while (*Line == ' ' || *Line == '\t') Line++;

	if (!(*SourceEncoding == 0 && *Line == 0))
	{
		int RetLen;
		while (*SourceEncoding != 0)
		{
			while (*Line == ' ' || *Line == '\t') Line++;
			if (*Line == 0) return false;

			switch (*SourceEncoding)
			{
			case 'T':	// float reg
				if (!MipsGetRegister(Line,RetLen,registers.frt, MipsFloatRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'D':	// float reg
				if (!MipsGetRegister(Line,RetLen,registers.frd, MipsFloatRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'S':	// float reg
				if (!MipsGetRegister(Line,RetLen,registers.frs, MipsFloatRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 't':
				if (!MipsGetRegister(Line,RetLen,registers.grt, MipsRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'd':
				if (!MipsGetRegister(Line,RetLen,registers.grd, MipsRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 's':
				if (!MipsGetRegister(Line,RetLen,registers.grs, MipsRegisters)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'V':	// ps2 vector registers
				switch (*(SourceEncoding+1))
				{
				case 's':
					if (!MipsGetRegister(Line,RetLen,registers.ps2vrs, MipsPs2Cop2FpRegisters)) return false;
					Line += RetLen;
					break;
				case 't':
					if (!MipsGetRegister(Line,RetLen,registers.ps2vrt, MipsPs2Cop2FpRegisters)) return false;
					Line += RetLen;
					break;
				case 'd':
					if (!MipsGetRegister(Line,RetLen,registers.ps2vrd, MipsPs2Cop2FpRegisters)) return false;
					Line += RetLen;
					break;
				case 'i':
					switch(*(SourceEncoding+2))
					{
						case 's':
							if (!MipsGetRegister(Line,RetLen,registers.ps2vrs, MipsPs2Cop2IRegisters)) return false;
							Line += RetLen;
							break;
						case 't':
							if (!MipsGetRegister(Line,RetLen,registers.ps2vrt, MipsPs2Cop2IRegisters)) return false;
							Line += RetLen;
							break;
						case 'd':
							if (!MipsGetRegister(Line,RetLen,registers.ps2vrd, MipsPs2Cop2IRegisters)) return false;
							Line += RetLen;
							break;
					}
					SourceEncoding += 1;
					break;
				default:
					return false;
				}
				SourceEncoding += 2;
				break;
			case 'a':	// 5 bit immediate
				if (!MipsCheckImmediate(Line,cpu,immediate.originalValue,RetLen)) return false;
				immediateType = MIPS_IMMEDIATE5;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'i':	// 16 bit immediate
				if (!MipsCheckImmediate(Line,cpu,immediate.originalValue,RetLen)) return false;
				immediateType = MIPS_IMMEDIATE16;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'b':	// 20 bit immediate
				if (!MipsCheckImmediate(Line,cpu,immediate.originalValue,RetLen)) return false;
				immediateType = MIPS_IMMEDIATE20;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'I':	// 32 bit immediate
				if (!MipsCheckImmediate(Line,cpu,immediate.originalValue,RetLen)) return false;
				immediateType = MIPS_IMMEDIATE26;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'r':	// forced register
				if (MipsGetRegister(Line,RetLen, MipsRegisters) != *(SourceEncoding+1)) return false;
				Line += RetLen;
				SourceEncoding += 2;
				break;
			case '/':	// forced letter
				SourceEncoding++;	// fallthrough
			default:	// everything else
				if (*SourceEncoding++ != *Line++) return false;
				break;
			}
		}
	}

	while (*Line == ' ' || *Line == '\t') Line++;
	if (*Line != 0)	return false;	// there's something else, bad

	// opcode is ok - now set all flags
	Opcode = SourceOpcode;
	immediate.value = immediate.originalValue;

	setOmittedRegisters();
	return true;
}

void CMipsInstruction::setOmittedRegisters()
{
	// copy over omitted registers
	if (Opcode.flags & MO_RSD)
		registers.grd = registers.grs;

	if (Opcode.flags & MO_RST)
		registers.grt = registers.grs;

	if (Opcode.flags & MO_RDT)
		registers.grt = registers.grd;

	if (Opcode.flags & MO_FRSD)
		registers.frd = registers.frs;
}

int getImmediateBits(MipsImmediateType type)
{
	switch (type)
	{
	case MIPS_IMMEDIATE5:
		return 5;
	case MIPS_IMMEDIATE16:
		return 16;
	case MIPS_IMMEDIATE20:
		return 20;
	case MIPS_IMMEDIATE26:
		return 26;
	default:
		return 0;
	}
}

bool CMipsInstruction::Validate()
{
	if (RamPos % 4)
	{
		Logger::queueError(Logger::Error,L"opcode not aligned to word boundary");
		return false;
	}

	// check immediates
	if (immediateType != MIPS_NOIMMEDIATE)
	{
		immediate.originalValue = immediate.value;

		if (Opcode.flags & MO_IMMALIGNED)	// immediate must be aligned
		{
			if (immediate.value % 4)
			{
				Logger::queueError(Logger::Error,L"Immediate must be word aligned",immediate.value);
				return false;
			}
		}

		if (Opcode.flags & MO_IPCA)	// absolute value >> 2)
		{
			immediate.value = (immediate.value >> 2) & 0x3FFFFFF;
		} else if (Opcode.flags & MO_IPCR)	// relative 16 bit value
		{
			const int num = (immediate.value-RamPos-4) >> 2;

			if (num > std::numeric_limits<short>::max() || num < std::numeric_limits<short>::min())
			{
				Logger::queueError(Logger::Error,L"Branch target %08X out of range",immediate.value);
				return false;
			}
			immediate.value = num;
		}

		int immediateBits = getImmediateBits(immediateType);
		unsigned int mask = (0xFFFFFFFF << (32-immediateBits)) >> (32-immediateBits);
		int digits = (immediateBits+3) / 4;

		if ((unsigned int)std::abs(immediate.value) > mask)
		{
			Logger::queueError(Logger::Error,L"Immediate value %0*X out of range",digits,immediate.value);
			return false;
		}

		immediate.value &= mask;
	}

	return true;
}

void CMipsInstruction::encodeNormal()
{
	encoding = Opcode.destencoding;

	if (registers.grs.num != -1) encoding |= MIPS_RS(registers.grs.num);	// source reg
	if (registers.grt.num != -1) encoding |= MIPS_RT(registers.grt.num);	// target reg
	if (registers.grd.num != -1) encoding |= MIPS_RD(registers.grd.num);	// dest reg

	if (registers.frt.num != -1) encoding |= MIPS_FT(registers.frt.num);	// float target reg
	if (registers.frs.num != -1) encoding |= MIPS_FS(registers.frs.num);	// float source reg
	if (registers.frd.num != -1) encoding |= MIPS_FD(registers.frd.num);	// float dest reg

	if (registers.ps2vrt.num != -1) encoding |= (registers.ps2vrt.num << 16);	// ps2 vector target reg
	if (registers.ps2vrs.num != -1) encoding |= (registers.ps2vrs.num << 11);	// ps2 vector source reg
	if (registers.ps2vrd.num != -1) encoding |= (registers.ps2vrd.num << 6);	// ps2 vector dest reg

	switch (immediateType)
	{
	case MIPS_NOIMMEDIATE:
		break;
	case MIPS_IMMEDIATE5:
	case MIPS_IMMEDIATE20:
		encoding |= immediate.value << 6;
		break;
	case MIPS_IMMEDIATE16:
	case MIPS_IMMEDIATE26:
		encoding |= immediate.value;
		break;
	}
}

void CMipsInstruction::Encode()
{
	encodeNormal();
}
