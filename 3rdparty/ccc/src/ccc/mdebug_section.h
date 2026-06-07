// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "util.h"

namespace ccc::mdebug {

struct SymbolicHeader;

enum class SymbolType : u32 {
	NIL = 0,
	GLOBAL = 1,
	STATIC = 2,
	PARAM = 3,
	LOCAL = 4,
	LABEL = 5,
	PROC = 6,
	BLOCK = 7,
	END = 8,
	MEMBER = 9,
	TYPEDEF = 10,
	FILE_SYMBOL = 11,
	STATICPROC = 14,
	CONSTANT = 15
};

enum class SymbolClass : u32 {
	NIL = 0,
	TEXT = 1,
	DATA = 2,
	BSS = 3,
	REGISTER = 4,
	ABS = 5,
	UNDEFINED = 6,
	LOCAL = 7,
	BITS = 8,
	DBX = 9,
	REG_IMAGE = 10,
	INFO = 11,
	USER_STRUCT = 12,
	SDATA = 13,
	SBSS = 14,
	RDATA = 15,
	VAR = 16,
	COMMON = 17,
	SCOMMON = 18,
	VAR_REGISTER = 19,
	VARIANT = 20,
	SUNDEFINED = 21,
	INIT = 22,
	BASED_VAR = 23,
	XDATA = 24,
	PDATA = 25,
	FINI = 26,
	NONGP = 27
};

// See stab.def from gcc for documentation on what all these are.
enum StabsCode {
	STAB = 0x00,
	N_GSYM = 0x20,
	N_FNAME = 0x22,
	N_FUN = 0x24,
	N_STSYM = 0x26,
	N_LCSYM = 0x28,
	N_MAIN = 0x2a,
	N_PC = 0x30,
	N_NSYMS = 0x32,
	N_NOMAP = 0x34,
	N_OBJ = 0x38,
	N_OPT = 0x3c,
	N_RSYM = 0x40,
	N_M2C = 0x42,
	N_SLINE = 0x44,
	N_DSLINE = 0x46,
	N_BSLINE = 0x48,
	N_EFD = 0x4a,
	N_EHDECL = 0x50,
	N_CATCH = 0x54,
	N_SSYM = 0x60,
	N_SO = 0x64,
	N_LSYM = 0x80,
	N_BINCL = 0x82,
	N_SOL = 0x84,
	N_PSYM = 0xa0,
	N_EINCL = 0xa2,
	N_ENTRY = 0xa4,
	N_LBRAC = 0xc0,
	N_EXCL = 0xc2,
	N_SCOPE = 0xc4,
	N_RBRAC = 0xe0,
	N_BCOMM = 0xe2,
	N_ECOMM = 0xe4,
	N_ECOML = 0xe8,
	N_NBTEXT = 0xf0,
	N_NBDATA = 0xf2,
	N_NBBSS = 0xf4,
	N_NBSTS = 0xf6,
	N_NBLCS = 0xf8,
	N_LENG = 0xfe
};

CCC_PACKED_STRUCT(ProcedureDescriptor,
	/* 0x00 */ u32 address;
	/* 0x04 */ u32 symbol_index;
	/* 0x08 */ s32 line_number_entry_index;
	/* 0x0c */ s32 saved_register_mask;
	/* 0x10 */ s32 saved_register_offset;
	/* 0x14 */ s32 optimization_entry_index;
	/* 0x18 */ s32 saved_float_register_mask;
	/* 0x1c */ s32 saved_float_register_offset;
	/* 0x20 */ s32 frame_size;
	/* 0x24 */ s16 frame_pointer_register;
	/* 0x26 */ s16 return_pc_register;
	/* 0x28 */ s32 line_number_low;
	/* 0x2c */ s32 line_number_high;
	/* 0x30 */ u32 line_number_offset;
)
static_assert(sizeof(ProcedureDescriptor) == 0x34);

struct Symbol {
	u32 value;
	SymbolType symbol_type;
	SymbolClass symbol_class;
	u32 index;
	const char* string;
	const ProcedureDescriptor* procedure_descriptor = nullptr;
	
	bool is_stabs() const {
		return (index & 0xfff00) == 0x8f300;
	}
	
	StabsCode code() const {
		return (StabsCode) (index - 0x8f300);
	}
};

struct File {
	std::vector<Symbol> symbols;
	u32 address = 0;
	std::string working_dir; // The working directory of gcc.
	std::string command_line_path; // The source file path passed on the command line to gcc.
	std::string full_path; // The full combined path.
};

class SymbolTableReader {
public:
	Result<void> init(std::span<const u8> elf, s32 section_offset);
	
	s32 file_count() const;
	Result<File> parse_file(s32 index) const;
	Result<std::vector<Symbol>> parse_external_symbols() const;
	
	void print_header(FILE* out) const;
	Result<void> print_symbols(FILE* out, bool print_locals, bool print_procedure_descriptors, bool print_externals) const;

protected:
	bool m_ready = false;
	
	std::span<const u8> m_elf;
	s32 m_section_offset;
	
	// If the .mdebug section was moved without updating its contents all the
	// absolute file offsets stored within will be incorrect by a fixed amount.
	s32 m_fudge_offset;
	
	const SymbolicHeader* m_hdrr;
};

const char* symbol_type(SymbolType type);
const char* symbol_class(SymbolClass symbol_class);
const char* stabs_code_to_string(StabsCode code);

}
