/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-2022  PCSX2 Dev Team
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
#include "MipsAssembler.h"

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


const tMipsRegister MipsRegister[] = {
	{ "r0", 0, 2 }, { "zero", 0, 4}, { "$0", 0, 2 }, { "$zero", 0, 5 },
	{ "at", 1, 2 }, { "r1", 1, 2 }, { "$1", 1, 2 }, { "$at", 1, 3 },
	{ "v0", 2, 2 }, { "r2", 2, 2 }, { "$v0", 2, 3 },
	{ "v1", 3, 2 }, { "r3", 3, 2 }, { "$v1", 3, 3 },
	{ "a0", 4, 2 }, { "r4", 4, 2 }, { "$a0", 4, 3 },
	{ "a1", 5, 2 }, { "r5", 5, 2 }, { "$a1", 5, 3 },
	{ "a2", 6, 2 }, { "r6", 6, 2 }, { "$a2", 6, 3 },
	{ "a3", 7, 2 }, { "r7", 7, 2 }, { "$a3", 7, 3 },
	{ "t0", 8, 2 }, { "r8", 8, 2 }, { "$t0", 8, 3 },
	{ "t1", 9, 2 }, { "r9", 9, 2 }, { "$t1", 9, 3 },
	{ "t2", 10, 2 }, { "r10", 10, 3 }, { "$t2", 10, 3 },
	{ "t3", 11, 2 }, { "r11", 11, 3 }, { "$t3", 11, 3 },
	{ "t4", 12, 2 }, { "r12", 12, 3 }, { "$t4", 12, 3 },
	{ "t5", 13, 2 }, { "r13", 13, 3 }, { "$t5", 13, 3 },
	{ "t6", 14, 2 }, { "r14", 14, 3 }, { "$t6", 14, 3 },
	{ "t7", 15, 2 }, { "r15", 15, 3 }, { "$t7", 15, 3 },
	{ "s0", 16, 2 }, { "r16", 16, 3 }, { "$s0", 16, 3 },
	{ "s1", 17, 2 }, { "r17", 17, 3 }, { "$s1", 17, 3 },
	{ "s2", 18, 2 }, { "r18", 18, 3 }, { "$s2", 18, 3 },
	{ "s3", 19, 2 }, { "r19", 19, 3 }, { "$s3", 19, 3 },
	{ "s4", 20, 2 }, { "r20", 20, 3 }, { "$s4", 20, 3 },
	{ "s5", 21, 2 }, { "r21", 21, 3 }, { "$s5", 21, 3 },
	{ "s6", 22, 2 }, { "r22", 22, 3 }, { "$s6", 22, 3 },
	{ "s7", 23, 2 }, { "r23", 23, 3 }, { "$s7", 23, 3 },
	{ "t8", 24, 2 }, { "r24", 24, 3 }, { "$t8", 24, 3 },
	{ "t9", 25, 2 }, { "r25", 25, 3 }, { "$t9", 25, 3 },
	{ "k0", 26, 2 }, { "r26", 26, 3 }, { "$k0", 26, 3 },
	{ "k1", 27, 2 }, { "r27", 27, 3 }, { "$k1", 27, 3 },
	{ "gp", 28, 2 }, { "r28", 28, 3 }, { "$gp", 28, 3 },
	{ "sp", 29, 2 }, { "r29", 29, 3 }, { "$sp", 29, 3 },
	{ "fp", 30, 2 }, { "r30", 30, 3 }, { "$fp", 30, 3 },
	{ "ra", 31, 2 }, { "r31", 31, 3 }, { "$ra", 31, 3 },
	{ NULL, -1, 0}
};


const tMipsRegister MipsFloatRegister[] = {
	{ "f0", 0, 2},		{ "$f0", 0, 3 },
	{ "f1", 1, 2},		{ "$f1", 1, 3 },
	{ "f2", 2, 2},		{ "$f2", 2, 3 },
	{ "f3", 3, 2},		{ "$f3", 3, 3 },
	{ "f4", 4, 2},		{ "$f4", 4, 3 },
	{ "f5", 5, 2},		{ "$f5", 5, 3 },
	{ "f6", 6, 2},		{ "$f6", 6, 3 },
	{ "f7", 7, 2},		{ "$f7", 7, 3 },
	{ "f8", 8, 2},		{ "$f8", 8, 3 },
	{ "f9", 9, 2},		{ "$f9", 9, 3 },
	{ "f00", 0, 3},		{ "$f00", 0, 4 },
	{ "f01", 1, 3},		{ "$f01", 1, 4 },
	{ "f02", 2, 3},		{ "$f02", 2, 4 },
	{ "f03", 3, 3},		{ "$f03", 3, 4 },
	{ "f04", 4, 3},		{ "$f04", 4, 4 },
	{ "f05", 5, 3},		{ "$f05", 5, 4 },
	{ "f06", 6, 3},		{ "$f06", 6, 4 },
	{ "f07", 7, 3},		{ "$f07", 7, 4 },
	{ "f08", 8, 3},		{ "$f08", 8, 4 },
	{ "f09", 9, 3},		{ "$f09", 9, 4 },
	{ "f10", 10, 3},	{ "$f10", 10, 4 },
	{ "f11", 11, 3},	{ "$f11", 11, 4 },
	{ "f12", 12, 3},	{ "$f12", 12, 4 },
	{ "f13", 13, 3},	{ "$f13", 13, 4 },
	{ "f14", 14, 3},	{ "$f14", 14, 4 },
	{ "f15", 15, 3},	{ "$f15", 15, 4 },
	{ "f16", 16, 3},	{ "$f16", 16, 4 },
	{ "f17", 17, 3},	{ "$f17", 17, 4 },
	{ "f18", 18, 3},	{ "$f18", 18, 4 },
	{ "f19", 19, 3},	{ "$f19", 19, 4 },
	{ "f20", 20, 3},	{ "$f20", 20, 4 },
	{ "f21", 21, 3},	{ "$f21", 21, 4 },
	{ "f22", 22, 3},	{ "$f22", 22, 4 },
	{ "f23", 23, 3},	{ "$f23", 23, 4 },
	{ "f24", 24, 3},	{ "$f24", 24, 4 },
	{ "f25", 25, 3},	{ "$f25", 25, 4 },
	{ "f26", 26, 3},	{ "$f26", 26, 4 },
	{ "f27", 27, 3},	{ "$f27", 27, 4 },
	{ "f28", 28, 3},	{ "$f28", 28, 4 },
	{ "f29", 29, 3},	{ "$f29", 29, 4 },
	{ "f30", 30, 3},	{ "$f30", 30, 4 },
	{ "f31", 31, 3},	{ "$f31", 31, 4 }
};

const tMipsRegister MipsPs2Cop2FpRegister[] = {
	{ "vf0", 0, 3},		{ "$vf0", 0, 4 },
	{ "vf1", 1, 3},		{ "$vf1", 1, 4 },
	{ "vf2", 2, 3},		{ "$vf2", 2, 4 },
	{ "vf3", 3, 3},		{ "$vf3", 3, 4 },
	{ "vf4", 4, 3},		{ "$vf4", 4, 4 },
	{ "vf5", 5, 3},		{ "$vf5", 5, 4 },
	{ "vf6", 6, 3},		{ "$vf6", 6, 4 },
	{ "vf7", 7, 3},		{ "$vf7", 7, 4 },
	{ "vf8", 8, 3},		{ "$vf8", 8, 4 },
	{ "vf9", 9, 3},		{ "$vf9", 9, 4 },
	{ "vf00", 0, 4},	{ "$vf00", 0, 5 },
	{ "vf01", 1, 4},	{ "$vf01", 1, 5 },
	{ "vf02", 2, 4},	{ "$vf02", 2, 5 },
	{ "vf03", 3, 4},	{ "$vf03", 3, 5 },
	{ "vf04", 4, 4},	{ "$vf04", 4, 5 },
	{ "vf05", 5, 4},	{ "$vf05", 5, 5 },
	{ "vf06", 6, 4},	{ "$vf06", 6, 5 },
	{ "vf07", 7, 4},	{ "$vf07", 7, 5 },
	{ "vf08", 8, 4},	{ "$vf08", 8, 5 },
	{ "vf09", 9, 4},	{ "$vf09", 9, 5 },
	{ "vf10", 10, 4},	{ "$vf10", 10, 5 },
	{ "vf11", 11, 4},	{ "$vf11", 11, 5 },
	{ "vf12", 12, 4},	{ "$vf12", 12, 5 },
	{ "vf13", 13, 4},	{ "$vf13", 13, 5 },
	{ "vf14", 14, 4},	{ "$vf14", 14, 5 },
	{ "vf15", 15, 4},	{ "$vf15", 15, 5 },
	{ "vf16", 16, 4},	{ "$vf16", 16, 5 },
	{ "vf17", 17, 4},	{ "$vf17", 17, 5 },
	{ "vf18", 18, 4},	{ "$vf18", 18, 5 },
	{ "vf19", 19, 4},	{ "$vf19", 19, 5 },
	{ "vf20", 20, 4},	{ "$vf20", 20, 5 },
	{ "vf21", 21, 4},	{ "$vf21", 21, 5 },
	{ "vf22", 22, 4},	{ "$vf22", 22, 5 },
	{ "vf23", 23, 4},	{ "$vf23", 23, 5 },
	{ "vf24", 24, 4},	{ "$vf24", 24, 5 },
	{ "vf25", 25, 4},	{ "$vf25", 25, 5 },
	{ "vf26", 26, 4},	{ "$vf26", 26, 5 },
	{ "vf27", 27, 4},	{ "$vf27", 27, 5 },
	{ "vf28", 28, 4},	{ "$vf28", 28, 5 },
	{ "vf29", 29, 4},	{ "$vf29", 29, 5 },
	{ "vf30", 30, 4},	{ "$vf30", 30, 5 },
	{ "vf31", 31, 4},	{ "$vf31", 31, 5 }
};

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


bool MipsGetRegister(const char* source, int& RetLen, MipsRegisterInfo& Result)
{
	for (int z = 0; MipsRegister[z].name != NULL; z++)
	{
		int len = MipsRegister[z].len;
		if (strncmp(MipsRegister[z].name,source,len) == 0)	// okay so far
		{
			if (source[len] == ',' || source[len] == '\n'  || source[len] == 0
				|| source[len] == ')'  || source[len] == '(' || source[len] == '-')	// one of these has to come after a register
			{
				memcpy(Result.name,source,len);
				Result.name[len] = 0;
				Result.num = MipsRegister[z].num;
				RetLen = len;
				return true;
			}
		}
	}
	return false;
}

int MipsGetRegister(const char* source, int& RetLen)
{
	for (int z = 0; MipsRegister[z].name != NULL; z++)
	{
		int len = MipsRegister[z].len;
		if (strncmp(MipsRegister[z].name,source,len) == 0)	// okay so far
		{
			if (source[len] == ',' || source[len] == '\n'  || source[len] == 0
				|| source[len] == ')'  || source[len] == '(' || source[len] == '-')	// one of these has to come after a register
			{
				RetLen = len;
				return MipsRegister[z].num;
			}
		}
	}
	return -1;
}


bool MipsGetFloatRegister(const char* source, int& RetLen, MipsRegisterInfo& Result)
{
	for (int z = 0; MipsFloatRegister[z].name != NULL; z++)
	{
		int len = MipsFloatRegister[z].len;
		if (strncmp(MipsFloatRegister[z].name,source,len) == 0)	// okay so far
		{
			if (source[len] == ',' || source[len] == '\n'  || source[len] == 0
				|| source[len] == ')'  || source[len] == '(' || source[len] == '-')	// one of these has to come after a register
			{
				memcpy(Result.name,source,len);
				Result.name[len] = 0;
				Result.num = MipsFloatRegister[z].num;
				RetLen = len;
				return true;
			}
		}
	}
	return false;
}

bool MipsGetPs2VectorRegister(const char* source, int& RetLen, MipsRegisterInfo& Result)
{
	for (int z = 0; MipsPs2Cop2FpRegister[z].name != NULL; z++)
	{
		int len = MipsPs2Cop2FpRegister[z].len;
		if (strncmp(MipsPs2Cop2FpRegister[z].name,source,len) == 0)	// okay so far
		{
			if (source[len] == ',' || source[len] == '\n'  || source[len] == 0
				|| source[len] == ')'  || source[len] == '(' || source[len] == '-')	// one of these has to come after a register
			{
				memcpy(Result.name,source,len);
				Result.name[len] = 0;
				Result.num = MipsPs2Cop2FpRegister[z].num;
				RetLen = len;
				return true;
			}
		}
	}
	return false;
}

int MipsGetFloatRegister(const char* source, int& RetLen)
{
	for (int z = 0; MipsFloatRegister[z].name != NULL; z++)
	{
		int len = MipsFloatRegister[z].len;
		if (strncmp(MipsFloatRegister[z].name,source,len) == 0)	// okay so far
		{
			if (source[len] == ',' || source[len] == '\n'  || source[len] == 0
				|| source[len] == ')'  || source[len] == '(' || source[len] == '-')	// one of these has to come after a register
			{
				RetLen = len;
				return MipsFloatRegister[z].num;
			}
		}
	}
	return -1;
}


bool MipsCheckImmediate(const char* Source, DebugInterface* cpu, int& dest, int& RetLen)
{
	char Buffer[512];
	int BufferPos = 0;
	int l;

	if (MipsGetRegister(Source,l) != -1)	// error
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
			if (MipsGetRegister(Source+1,l) != -1)	// end
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
	if (!cpu->initExpression(Buffer,postfix))
		return false;

	u64 value;
	if (!cpu->parseExpression(postfix,value))
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
				if (!MipsGetFloatRegister(Line,RetLen,registers.frt)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'D':	// float reg
				if (!MipsGetFloatRegister(Line,RetLen,registers.frd)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'S':	// float reg
				if (!MipsGetFloatRegister(Line,RetLen,registers.frs)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 't':
				if (!MipsGetRegister(Line,RetLen,registers.grt)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'd':
				if (!MipsGetRegister(Line,RetLen,registers.grd)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 's':
				if (!MipsGetRegister(Line,RetLen,registers.grs)) return false;
				Line += RetLen;
				SourceEncoding++;
				break;
			case 'V':	// ps2 vector registers
				switch (*(SourceEncoding+1))
				{
				case 's':
					if (!MipsGetPs2VectorRegister(Line,RetLen,registers.ps2vrs)) return false;
					Line += RetLen;
					break;
				case 't':
					if (!MipsGetPs2VectorRegister(Line,RetLen,registers.ps2vrt)) return false;
					Line += RetLen;
					break;
				case 'd':
					if (!MipsGetPs2VectorRegister(Line,RetLen,registers.ps2vrd)) return false;
					Line += RetLen;
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
				if (MipsGetRegister(Line,RetLen) != *(SourceEncoding+1)) return false;
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
			int num = (immediate.value-RamPos-4);
			
			if (num > 0x20000 || num < (-0x20000))
			{
				Logger::queueError(Logger::Error,L"Branch target %08X out of range",immediate.value);
				return false;
			}
			immediate.value = num >> 2;
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
	if (registers.ps2vrs.num != -1) encoding |= (registers.ps2vrs.num << 21);	// ps2 vector source reg
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
