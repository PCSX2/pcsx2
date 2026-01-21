#define SLL  0x00
#define SRL  0x01
#define SRA  0x02
#define SLLV 0x04
#define SRLV 0x06
#define SRAV 0x07
#define JR   0x08
#define JALR 0x09
#define MOVZ 0x0A
#define MOVN 0x0B

#define SYSCALL 0x0C
#define BREAK   0x0D
#define SYNC    0x0F

#define MTHI  0x10
#define MFHI  0x11
#define MTLO  0x12
#define MFLO  0x13
#define DSLLV 0x14
#define DSRLV 0x16
#define DSRAV 0x17
#define MULT  0x18
#define MULTU 0x19
#define DIV   0x1A
#define DIVU  0x1B

#define ADD   0x20
#define ADDU  0x21
#define SUB   0x22
#define SUBU  0x23
#define AND   0x24
#define OR    0x25
#define XOR   0x26
#define NOR   0x27
#define MFSA  0x28
#define MTSA  0x29
#define SLT   0x2A
#define SLTU  0x2B
#define DADD  0x2C
#define DADDU 0x2D
#define DSUB  0x2E
#define DSUBU 0x2F

#define	DSLL   0x38
#define DSRL   0x3A
#define DSRA   0x3B
#define DSLL32 0x3C
#define DSRL32 0x3E
#define DSRA32 0x3F

// Immediate operations.
#define BLTZ 0x41

#define J    0x42
#define JAL  0x43

#define BEQ  0x44
#define BNE  0x45
#define BLEZ 0x46
#define BGTZ 0x47

#define ADDI  0x48
#define ADDIU 0x49
#define SLTI  0x4A
#define SLTIU 0x4B
#define ANDI  0x4C
#define ORI   0x4D
#define XORI  0x4E
#define LUI   0x4F

#define MFC1 0x51
#define BEQL 0x54
#define BNEL 0x55
#define BLEZL 0x56
#define BGTZL 0x57

#define DADDI  0x58
#define DADDIU 0x59
#define LQ     0x5E
#define SQ     0x5F

#define LB     0x60
#define LH     0x61
#define LW     0x63
#define LBU    0x64
#define LHU    0x65
#define LWU    0x67

#define SB 0x68
#define SH 0x69
#define SW 0x6B

// Missing stuff here.
#define LWC1 0x71
#define LD 0x77
#define SWC1 0x79
// ...and here
#define SD 0x7F
// ...and probably here

#define V0 2
#define V1 3
#define A0 4
#define A1 5
#define A2 6
#define GP 28

#define READS_RS  0x0001
#define READS_RT  0x0002
#define READS_RD  0x0004
#define READS_FS  0x0008
#define READS_FT  0x0010
#define READS_FD  0x0020
#define WRITES_RS 0x0040
#define WRITES_RT 0x0080
#define WRITES_RD 0x0100
#define WRITES_FS 0x0200
#define WRITES_FT 0x0400
#define WRITES_FD 0x0800
#define REPLACES  0x1000

//#define SET_OPERATION(code, value) (code = (value) < 0x3F ? ((code & 0xFFFFFFC0) | (value)) : ((code & 0x03FFFFFF) | (((value) - 0x40) << 26))) // ?!
#define SET_OPERATION(code, value) _SET_OPERATION(&(code), value)
#define SET_IMMEDIATE(code, value) (code = (code & 0xFFFF0000) | (value))
#define SET_RS(code, value) (code = (code & 0xFC1FFFFF) | ((value) << 21))
#define SET_RT(code, value) (code = (code & 0xFFE0FFFF) | ((value) << 16))
#define SET_RD(code, value) (code = (code & 0xFFFF07FF) | ((value) << 11))
#define SET_FS(code, value) (code = (code & 0xFFFF07FF) | ((value) << 11))
#define SET_FT(code, value) (code = (code & 0xFFE0FFFF) | ((value) << 16))
#define SET_FD(code, value) (code = (code & 0xFFFFF83F) | ((value) << 6))
#define SET_SHIFT(code, value) (code = ((code & 0xFFFFF83F) | ((value) << 6)))
#define SET_ADDRESS(code, value) (code = ((code & 0xFC000000) | ((value) >> 2)))

/* OPCODE SECTIONS:
0-0x3F: Standard R-types
0x40-0x7F: Standard I-types
0x80-??: Main floating-point types
*/

#define GET_OFFSET(code) (((signed short) ((code) & 0xFFFF) + 1) * 4)
//#define GET_OPERATION(code) ((code) >> 26 ? (((code) >> 26) + 0x40) : ((code) & 0x0000003F))
#define GET_IMMEDIATE(code) ((code) & 0x0000FFFF)
#define GET_RS(code) (((code) >> 21) & 0x1F)
#define GET_RT(code) (((code) >> 16) & 0x1F)
#define GET_RD(code) (((code) >> 11) & 0x1F)
#define GET_FS(code) (((code) >> 11) & 0x1F)
#define GET_FT(code) (((code) >> 16) & 0x1F)
#define GET_FD(code) (((code) >> 6) & 0x1F)
#define GET_SHIFT(code) (((code) >> 6) & 0x1F)
#define GET_ADDRESS(code) (((code) & 0x03FFFFFF) << 2) // Not sure whether the & is entirely correct.

#define ISBRANCHOP(op) ((op) == BEQ || (op) == BNE || (op) == BLEZ || (op) == BLTZ || (op) == BLEZL || (op) == BGTZL || ((op) >= 0xA0 && (op) <= 0xA8))

extern char* operations    [0xFF];
extern char* sc_operations [0x7F]; // Shortcut operations of type 1. (Writing variable is the same as a main reading variable, eg + becomes +=.)
extern char* sc2_operations[0x7F]; // Shortcut operations of type 2. (Writing variables ultimately just set the variable to one of the values.)

extern unsigned short notes[0x7F];

extern char* asmdef[0xFF];
extern unsigned int asmMask[0xFF];
extern unsigned int asmCode[0xFF];

extern const char* registers[32];
extern const char f_registers[32][5];

int Get_Reads  (unsigned int code, char* registers);
int Get_Writes (unsigned int code, char* registers);

bool Reads  (unsigned int code, char reg);
bool Writes (unsigned int code, char reg);

bool CodeToASM(char* str, unsigned int address, const unsigned int code);
bool ASMToCode(unsigned int address, unsigned int* code, unsigned int* mask, const char* str);

inline int GET_OPERATION(unsigned int code)
{
	__asm
	{
		mov eax,dword ptr[code]
		shr eax,26
		test eax,eax
		jne SecondTest;
	};
	return code & 0x3F;
	SecondTest:
	__asm
	{
		cmp eax,11h
		je ThirdTest;
		cmp eax,1
		je ThirdTest;
	};
	return (code >> 26) + 0x40;
	ThirdTest:
	/*if (code >> 26 == 0)
		return code & 0x3F;
	else if (code >> 26 != 0x11 && code >> 26 != 0x01)
		return (code >> 26) + 0x40;*/
	/*
	if (code >> 26 == 0x11) // Floating-point
		return 0x80 + GET_RS(code);

	return (code >> 26) + 0x40;*/
	for (int i = 0x80; i < 0xB0; i ++)
	{
		/*if (! asmMask[i]) continue;

		if ((code & asmMask[i]) == asmCode[i])
			return i;*/
		__asm
		{
			mov eax,dword ptr[i]
			mov ebx,dword ptr asmMask[eax*4]
			test ebx,ebx
			je Continue

			mov edx,dword ptr[code]
			mov ecx,dword ptr asmCode[eax*4]
			and edx,ebx
			cmp edx,ecx
			jne Continue
		};
		return i;
		Continue:;
	}

	return 0;
}

inline void _SET_OPERATION(unsigned int* code, unsigned int op)
{
	if (op < 0x40)
		*code = (*code & 0xFFFFFFC0) | op;
	else if (op < 0x80)
		*code = (*code & 0x03FFFFFF) | ((op - 0x40) << 26);
	else
		*code = (*code & ~asmMask[op]) | asmCode[op];
}

inline bool GetOffset(int* offset, unsigned int address, unsigned int code)
{
	int op = GET_OPERATION(code);

	if (op == BEQ || op == BNE || op == BLTZ || op == BGTZ || op == BEQL || op == BNEL || op == BLEZ)
	{
		*offset = (signed short) GET_IMMEDIATE(code) + 1;
		return 1;
	}
	else if (op == J || op == JAL)
	{
		*offset = ((int) GET_ADDRESS(code) - (int) address) / 4;
		return 1;
	}

	return 0;
}