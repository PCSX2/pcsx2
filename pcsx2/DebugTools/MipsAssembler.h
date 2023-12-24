// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "MipsAssemblerTables.h"
#include "DebugInterface.h"

enum MipsImmediateType { MIPS_NOIMMEDIATE, MIPS_IMMEDIATE5,
	MIPS_IMMEDIATE16, MIPS_IMMEDIATE20, MIPS_IMMEDIATE26 };

enum MipsArchType { MARCH_PSX = 0, MARCH_N64, MARCH_PS2, MARCH_PSP, MARCH_INVALID };

typedef struct {
	const char* name;
	short num;
	short len;
} tMipsRegister;

typedef struct {
	char name[5];
	short num;
} MipsRegisterInfo;

struct MipsImmediate
{
	int value;
	int originalValue;
};

struct MipsOpcodeRegisters {
	MipsRegisterInfo grs;			// general source reg
	MipsRegisterInfo grt;			// general target reg
	MipsRegisterInfo grd;			// general dest reg

	MipsRegisterInfo frs;			// float source reg
	MipsRegisterInfo frt;			// float target reg
	MipsRegisterInfo frd;			// float dest reg

	MipsRegisterInfo ps2vrs;		// ps2 vector source reg
	MipsRegisterInfo ps2vrt;		// ps2 vector target reg
	MipsRegisterInfo ps2vrd;		// ps2 vector dest reg

	void reset()
	{
		grs.num = grt.num = grd.num = -1;
		frs.num = frt.num = frd.num = -1;
		ps2vrs.num = ps2vrt.num = ps2vrd.num = -1;
	}
};


class CMipsInstruction
{
public:
	CMipsInstruction(DebugInterface* cpu);
	bool Load(const char* Name, const char* Params, int RamPos);
	virtual bool Validate();
	virtual void Encode();
	u32 getEncoding() { return encoding; };
	std::string getErrorMessage() { return error; };
private:
	void encodeNormal();
	bool parseOpcode(const tMipsOpcode& SourceOpcode, const char* Line);
	bool LoadEncoding(const tMipsOpcode& SourceOpcode, const char* Line);
	void setOmittedRegisters();

	tMipsOpcode Opcode;
	bool NoCheckError;
	bool Loaded;
	int RamPos;

	// opcode variables
	MipsOpcodeRegisters registers;
	MipsImmediateType immediateType;
	MipsImmediate immediate;
	int vfpuSize;

	DebugInterface* cpu;
	u32 encoding;
	std::string error;
};

bool MipsAssembleOpcode(const char* line, DebugInterface* cpu, u32 address, u32& dest, std::string& errorText);
