// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "mdebug_section.h"

namespace ccc::mdebug {

// MIPS debug symbol table headers.
// See include/coff/sym.h from GNU binutils for more information.

CCC_PACKED_STRUCT(SymbolicHeader,
	/* 0x00 */ s16 magic;
	/* 0x02 */ s16 version_stamp;
	/* 0x04 */ s32 line_number_count;
	/* 0x08 */ s32 line_numbers_size_bytes;
	/* 0x0c */ s32 line_numbers_offset;
	/* 0x10 */ s32 dense_numbers_count;
	/* 0x14 */ s32 dense_numbers_offset;
	/* 0x18 */ s32 procedure_descriptor_count;
	/* 0x1c */ s32 procedure_descriptors_offset;
	/* 0x20 */ s32 local_symbol_count;
	/* 0x24 */ s32 local_symbols_offset;
	/* 0x28 */ s32 optimization_symbols_count;
	/* 0x2c */ s32 optimization_symbols_offset;
	/* 0x30 */ s32 auxiliary_symbol_count;
	/* 0x34 */ s32 auxiliary_symbols_offset;
	/* 0x38 */ s32 local_strings_size_bytes;
	/* 0x3c */ s32 local_strings_offset;
	/* 0x40 */ s32 external_strings_size_bytes;
	/* 0x44 */ s32 external_strings_offset;
	/* 0x48 */ s32 file_descriptor_count;
	/* 0x4c */ s32 file_descriptors_offset;
	/* 0x50 */ s32 relative_file_descriptor_count;
	/* 0x54 */ s32 relative_file_descriptors_offset;
	/* 0x58 */ s32 external_symbols_count;
	/* 0x5c */ s32 external_symbols_offset;
)

CCC_PACKED_STRUCT(FileDescriptor,
	/* 0x00 */ u32 address;
	/* 0x04 */ s32 file_path_string_offset;
	/* 0x08 */ s32 strings_offset;
	/* 0x0c */ s32 cb_ss;
	/* 0x10 */ s32 isym_base;
	/* 0x14 */ s32 symbol_count;
	/* 0x18 */ s32 line_number_entry_index_base;
	/* 0x1c */ s32 cline;
	/* 0x20 */ s32 optimization_entry_index_base;
	/* 0x24 */ s32 copt;
	/* 0x28 */ u16 ipd_first;
	/* 0x2a */ u16 procedure_descriptor_count;
	/* 0x2c */ s32 iaux_base;
	/* 0x30 */ s32 caux;
	/* 0x34 */ s32 rfd_base;
	/* 0x38 */ s32 crfd;
	/* 0x3c */ u32 lang : 5;
	/* 0x3c */ u32 f_merge : 1;
	/* 0x3c */ u32 f_readin : 1;
	/* 0x3c */ u32 f_big_endian : 1;
	/* 0x3c */ u32 reserved_1 : 22;
	/* 0x40 */ s32 line_number_offset;
	/* 0x44 */ s32 cb_line;
)
static_assert(sizeof(FileDescriptor) == 0x48);

CCC_PACKED_STRUCT(SymbolHeader,
	/* 0x0 */ u32 iss;
	/* 0x4 */ u32 value;
	/* 0x8 */ u32 st : 6;
	/* 0x8 */ u32 sc : 5;
	/* 0x8 */ u32 reserved : 1;
	/* 0x8 */ u32 index : 20;
)
static_assert(sizeof(SymbolHeader) == 0xc);

CCC_PACKED_STRUCT(ExternalSymbolHeader,
	/* 0x0 */ u16 flags;
	/* 0x2 */ s16 ifd;
	/* 0x4 */ SymbolHeader symbol;
)
static_assert(sizeof(ExternalSymbolHeader) == 0x10);

static void print_symbol(FILE* out, const Symbol& symbol);
static void print_procedure_descriptor(FILE* out, const ProcedureDescriptor& procedure_descriptor);
static Result<s32> get_corruption_fixing_fudge_offset(s32 section_offset, const SymbolicHeader& hdrr);
static Result<Symbol> get_symbol(const SymbolHeader& header, std::span<const u8> elf, s32 strings_offset);

Result<void> SymbolTableReader::init(std::span<const u8> elf, s32 section_offset)
{
	m_elf = elf;
	m_section_offset = section_offset;
	
	m_hdrr = get_packed<SymbolicHeader>(m_elf, m_section_offset);
	CCC_CHECK(m_hdrr != nullptr, "MIPS debug section header out of bounds.");
	CCC_CHECK(m_hdrr->magic == 0x7009, "Invalid symbolic header.");
	
	Result<s32> fudge_offset = get_corruption_fixing_fudge_offset(m_section_offset, *m_hdrr);
	CCC_RETURN_IF_ERROR(fudge_offset);
	m_fudge_offset = *fudge_offset;
	
	m_ready = true;
	
	return Result<void>();
}
	
s32 SymbolTableReader::file_count() const
{
	CCC_ASSERT(m_ready);
	return m_hdrr->file_descriptor_count;
}

Result<File> SymbolTableReader::parse_file(s32 index) const
{
	CCC_ASSERT(m_ready);
	
	File file;
	
	u64 fd_offset = m_hdrr->file_descriptors_offset + index * sizeof(FileDescriptor);
	const FileDescriptor* fd_header = get_packed<FileDescriptor>(m_elf, fd_offset + m_fudge_offset);
	CCC_CHECK(fd_header != nullptr, "MIPS debug file descriptor out of bounds.");
	CCC_CHECK(fd_header->f_big_endian == 0, "Not little endian or bad file descriptor table.");
	
	file.address = fd_header->address;
	
	s32 rel_raw_path_offset = fd_header->strings_offset + fd_header->file_path_string_offset;
	s32 raw_path_offset = m_hdrr->local_strings_offset + rel_raw_path_offset + m_fudge_offset;
	const char* command_line_path = get_string(m_elf, raw_path_offset);
	if(command_line_path) {
		file.command_line_path = command_line_path;
	}
	
	// Parse local symbols.
	for(s64 j = 0; j < fd_header->symbol_count; j++) {
		u64 rel_symbol_offset = (fd_header->isym_base + j) * sizeof(SymbolHeader);
		u64 symbol_offset = m_hdrr->local_symbols_offset + rel_symbol_offset + m_fudge_offset;
		const SymbolHeader* symbol_header = get_packed<SymbolHeader>(m_elf, symbol_offset);
		CCC_CHECK(symbol_header != nullptr, "Symbol header out of bounds.");
		
		s32 strings_offset = m_hdrr->local_strings_offset + fd_header->strings_offset + m_fudge_offset;
		Result<Symbol> sym = get_symbol(*symbol_header, m_elf, strings_offset);
		CCC_RETURN_IF_ERROR(sym);
		
		bool string_offset_equal = (s32) symbol_header->iss == fd_header->file_path_string_offset;
		if(file.working_dir.empty() && string_offset_equal && sym->is_stabs() && sym->code() == N_SO && file.symbols.size() > 2) {
			const Symbol& working_dir = file.symbols.back();
			if(working_dir.is_stabs() && working_dir.code() == N_SO) {
				file.working_dir = working_dir.string;
			}
		}
		
		file.symbols.emplace_back(std::move(*sym));
	}
	
	// Parse procedure descriptors.
	for(s64 i = 0; i < fd_header->procedure_descriptor_count; i++) {
		u64 rel_procedure_offset = (fd_header->ipd_first + i) * sizeof(ProcedureDescriptor);
		u64 procedure_offset = m_hdrr->procedure_descriptors_offset + rel_procedure_offset + m_fudge_offset;
		const ProcedureDescriptor* procedure_descriptor = get_packed<ProcedureDescriptor>(m_elf, procedure_offset);
		CCC_CHECK(procedure_descriptor != nullptr, "Procedure descriptor out of bounds.");
		
		CCC_CHECK(procedure_descriptor->symbol_index < file.symbols.size(), "Symbol index out of bounds.");
		file.symbols[procedure_descriptor->symbol_index].procedure_descriptor = procedure_descriptor;
	}

	
	file.full_path = merge_paths(file.working_dir, file.command_line_path);
	
	return file;
}

Result<std::vector<Symbol>> SymbolTableReader::parse_external_symbols() const
{
	CCC_ASSERT(m_ready);
	
	std::vector<Symbol> external_symbols;
	for(s64 i = 0; i < m_hdrr->external_symbols_count; i++) {
		u64 sym_offset = m_hdrr->external_symbols_offset + i * sizeof(ExternalSymbolHeader);
		const ExternalSymbolHeader* external_header = get_packed<ExternalSymbolHeader>(m_elf, sym_offset + m_fudge_offset);
		CCC_CHECK(external_header != nullptr, "External header out of bounds.");
		
		Result<Symbol> sym = get_symbol(external_header->symbol, m_elf, m_hdrr->external_strings_offset + m_fudge_offset);
		CCC_RETURN_IF_ERROR(sym);
		external_symbols.emplace_back(std::move(*sym));
	}
	
	return external_symbols;
}

void SymbolTableReader::print_header(FILE* dest) const
{
	CCC_ASSERT(m_ready);
	
	fprintf(dest, "Symbolic Header, magic = %hx, vstamp = %hx:\n",
		(u16) m_hdrr->magic,
		(u16) m_hdrr->version_stamp);
	fprintf(dest, "\n");
	fprintf(dest, "                              Offset              Size (Bytes)        Count\n");
	fprintf(dest, "                              ------              ------------        -----\n");
	fprintf(dest, "  Line Numbers                0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->line_numbers_offset,
		(u32) m_hdrr->line_numbers_size_bytes,
		m_hdrr->line_number_count);
	fprintf(dest, "  Dense Numbers               0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->dense_numbers_offset,
		(u32) m_hdrr->dense_numbers_count * 8,
		m_hdrr->dense_numbers_count);
	fprintf(dest, "  Procedure Descriptors       0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->procedure_descriptors_offset,
		(u32) m_hdrr->procedure_descriptor_count * (u32) sizeof(ProcedureDescriptor),
		m_hdrr->procedure_descriptor_count);
	fprintf(dest, "  Local Symbols               0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->local_symbols_offset,
		(u32) m_hdrr->local_symbol_count * (u32) sizeof(SymbolHeader),
		m_hdrr->local_symbol_count);
	fprintf(dest, "  Optimization Symbols        0x%-8x          "  "-                   " "%-8d\n",
		(u32) m_hdrr->optimization_symbols_offset,
		m_hdrr->optimization_symbols_count);
	fprintf(dest, "  Auxiliary Symbols           0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->auxiliary_symbols_offset,
		(u32) m_hdrr->auxiliary_symbol_count * 4,
		m_hdrr->auxiliary_symbol_count);
	fprintf(dest, "  Local Strings               0x%-8x          "  "0x%-8x          "  "-\n",
		(u32) m_hdrr->local_strings_offset,
		(u32) m_hdrr->local_strings_size_bytes);
	fprintf(dest, "  External Strings            0x%-8x          "  "0x%-8x          "  "-\n",
		(u32) m_hdrr->external_strings_offset,
		(u32) m_hdrr->external_strings_size_bytes);
	fprintf(dest, "  File Descriptors            0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->file_descriptors_offset,
		(u32) m_hdrr->file_descriptor_count * (u32) sizeof(FileDescriptor),
		m_hdrr->file_descriptor_count);
	fprintf(dest, "  Relative File Descriptors   0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->relative_file_descriptors_offset,
		(u32) m_hdrr->relative_file_descriptor_count * 4,
		m_hdrr->relative_file_descriptor_count);
	fprintf(dest, "  External Symbols            0x%-8x          "  "0x%-8x          "  "%-8d\n",
		(u32) m_hdrr->external_symbols_offset,
		(u32) m_hdrr->external_symbols_count * 16,
		m_hdrr->external_symbols_count);
}

Result<void> SymbolTableReader::print_symbols(FILE* out, bool print_locals, bool print_procedure_descriptors, bool print_externals) const
{
	if(print_locals || print_procedure_descriptors) {
		s32 count = file_count();
		for(s32 i = 0; i < count; i++) {
			Result<File> file = parse_file(i);
			CCC_RETURN_IF_ERROR(file);
			
			fprintf(out, "FILE %s:\n", file->command_line_path.c_str());
			for(const Symbol& symbol : file->symbols) {
				if(print_locals || symbol.procedure_descriptor) {
					print_symbol(out, symbol);
				}
				if(print_procedure_descriptors && symbol.procedure_descriptor) {
					print_procedure_descriptor(out, *symbol.procedure_descriptor);
				}
			}
		}
	}
	
	if(print_externals) {
		fprintf(out, "EXTERNAL SYMBOLS:\n");
		Result<std::vector<Symbol>> external_symbols = parse_external_symbols();
		CCC_RETURN_IF_ERROR(external_symbols);
		for(const Symbol& symbol : *external_symbols) {
			print_symbol(out, symbol);
		}
	}
	
	return Result<void>();
}

static void print_symbol(FILE* out, const Symbol& symbol)
{
	fprintf(out, "    %8x ", symbol.value);
	
	const char* symbol_type_str = symbol_type(symbol.symbol_type);
	if(symbol_type_str) {
		fprintf(out, "%-11s ", symbol_type_str);
	} else {
		fprintf(out, "ST(%7u) ", (u32) symbol.symbol_type);
	}
	
	const char* symbol_class_str = symbol_class(symbol.symbol_class);
	if(symbol_class_str) {
		fprintf(out, "%-4s ", symbol_class_str);
	} else if ((u32) symbol.symbol_class == 0) {
		fprintf(out, "         ");
	} else {
		fprintf(out, "SC(%4u) ", (u32) symbol.symbol_class);
	}
	
	if(symbol.is_stabs()) {
		fprintf(out, "%-8s ", stabs_code_to_string(symbol.code()));
	} else {
		fprintf(out, "SI(%4u) ", symbol.index);
	}
	
	fprintf(out, "%s\n", symbol.string);
}

static void print_procedure_descriptor(FILE* out, const ProcedureDescriptor& procedure_descriptor)
{
	fprintf(out, "                    Address                       0x%08x\n", procedure_descriptor.address);
	fprintf(out, "                    Symbol Index                  %d\n", procedure_descriptor.symbol_index);
	fprintf(out, "                    Line Number Entry Index       %d\n", procedure_descriptor.line_number_entry_index);
	fprintf(out, "                    Saved Register Mask           0x%08x\n", procedure_descriptor.saved_register_mask);
	fprintf(out, "                    Saved Register Offset         %d\n", procedure_descriptor.saved_register_offset);
	fprintf(out, "                    Optimization Entry Index      %d\n", procedure_descriptor.optimization_entry_index);
	fprintf(out, "                    Saved Float Register Mask     0x%08x\n", procedure_descriptor.saved_float_register_mask);
	fprintf(out, "                    Saved Float Register Offset   %d\n", procedure_descriptor.saved_float_register_offset);
	fprintf(out, "                    Frame Size                    %d\n", procedure_descriptor.frame_size);
	fprintf(out, "                    Frame Pointer Register        %hd\n", procedure_descriptor.frame_pointer_register);
	fprintf(out, "                    Return PC Register            %hd\n", procedure_descriptor.return_pc_register);
	fprintf(out, "                    Line Number Low               %d\n", procedure_descriptor.line_number_low);
	fprintf(out, "                    Line Number High              %d\n", procedure_descriptor.line_number_high);
	fprintf(out, "                    Line Number Offset            %d\n", procedure_descriptor.line_number_offset);
}

static Result<s32> get_corruption_fixing_fudge_offset(s32 section_offset, const SymbolicHeader& hdrr)
{
	// GCC will always put the first part of the symbol table right after the
	// header, so if the header says it's somewhere else we know the section has
	// probably been moved without updating its contents.
	s32 right_after_header = INT32_MAX;
	if(hdrr.line_numbers_offset > 0) right_after_header = std::min(hdrr.line_numbers_offset, right_after_header);
	if(hdrr.dense_numbers_offset > 0) right_after_header = std::min(hdrr.dense_numbers_offset, right_after_header);
	if(hdrr.procedure_descriptors_offset > 0) right_after_header = std::min(hdrr.procedure_descriptors_offset, right_after_header);
	if(hdrr.local_symbols_offset > 0) right_after_header = std::min(hdrr.local_symbols_offset, right_after_header);
	if(hdrr.optimization_symbols_offset > 0) right_after_header = std::min(hdrr.optimization_symbols_offset, right_after_header);
	if(hdrr.auxiliary_symbols_offset > 0) right_after_header = std::min(hdrr.auxiliary_symbols_offset, right_after_header);
	if(hdrr.local_strings_offset > 0) right_after_header = std::min(hdrr.local_strings_offset, right_after_header);
	if(hdrr.external_strings_offset > 0) right_after_header = std::min(hdrr.external_strings_offset, right_after_header);
	if(hdrr.file_descriptors_offset > 0) right_after_header = std::min(hdrr.file_descriptors_offset, right_after_header);
	if(hdrr.relative_file_descriptors_offset > 0) right_after_header = std::min(hdrr.relative_file_descriptors_offset, right_after_header);
	if(hdrr.external_symbols_offset > 0) right_after_header = std::min(hdrr.external_symbols_offset, right_after_header);
	
	CCC_CHECK(right_after_header >= 0 && right_after_header < INT32_MAX, "Invalid symbolic header.");
	
	// Figure out how much we need to adjust all the file offsets by.
	s32 fudge_offset = section_offset - (right_after_header - sizeof(SymbolicHeader));
	if(fudge_offset != 0) {
		CCC_WARN("The .mdebug section was moved without updating its contents. Adjusting file offsets by %d bytes.", fudge_offset);
	}
	
	return fudge_offset;
}

static Result<Symbol> get_symbol(const SymbolHeader& header, std::span<const u8> elf, s32 strings_offset)
{
	Symbol symbol;
	
	const char* string = get_string(elf, strings_offset + header.iss);
	CCC_CHECK(string, "Symbol has invalid string.");
	symbol.string = string;
	
	symbol.value = header.value;
	symbol.symbol_type = (SymbolType) header.st;
	symbol.symbol_class = (SymbolClass) header.sc;
	symbol.index = header.index;
	
	if(symbol.is_stabs()) {
		CCC_CHECK(stabs_code_to_string(symbol.code()) != nullptr, "Bad stabs symbol code '%x'.", symbol.code());
	}
	
	return symbol;
}

const char* symbol_type(SymbolType type)
{
	switch(type) {
		case SymbolType::NIL: return "NIL";
		case SymbolType::GLOBAL: return "GLOBAL";
		case SymbolType::STATIC: return "STATIC";
		case SymbolType::PARAM: return "PARAM";
		case SymbolType::LOCAL: return "LOCAL";
		case SymbolType::LABEL: return "LABEL";
		case SymbolType::PROC: return "PROC";
		case SymbolType::BLOCK: return "BLOCK";
		case SymbolType::END: return "END";
		case SymbolType::MEMBER: return "MEMBER";
		case SymbolType::TYPEDEF: return "TYPEDEF";
		case SymbolType::FILE_SYMBOL: return "FILE";
		case SymbolType::STATICPROC: return "STATICPROC";
		case SymbolType::CONSTANT: return "CONSTANT";
	}
	return nullptr;
}

const char* symbol_class(SymbolClass symbol_class)
{
	switch(symbol_class) {
		case SymbolClass::NIL: return "NIL";
		case SymbolClass::TEXT: return "TEXT";
		case SymbolClass::DATA: return "DATA";
		case SymbolClass::BSS: return "BSS";
		case SymbolClass::REGISTER: return "REGISTER";
		case SymbolClass::ABS: return "ABS";
		case SymbolClass::UNDEFINED: return "UNDEFINED";
		case SymbolClass::LOCAL: return "LOCAL";
		case SymbolClass::BITS: return "BITS";
		case SymbolClass::DBX: return "DBX";
		case SymbolClass::REG_IMAGE: return "REG_IMAGE";
		case SymbolClass::INFO: return "INFO";
		case SymbolClass::USER_STRUCT: return "USER_STRUCT";
		case SymbolClass::SDATA: return "SDATA";
		case SymbolClass::SBSS: return "SBSS";
		case SymbolClass::RDATA: return "RDATA";
		case SymbolClass::VAR: return "VAR";
		case SymbolClass::COMMON: return "COMMON";
		case SymbolClass::SCOMMON: return "SCOMMON";
		case SymbolClass::VAR_REGISTER: return "VAR_REGISTER";
		case SymbolClass::VARIANT: return "VARIANT";
		case SymbolClass::SUNDEFINED: return "SUNDEFINED";
		case SymbolClass::INIT: return "INIT";
		case SymbolClass::BASED_VAR: return "BASED_VAR";
		case SymbolClass::XDATA: return "XDATA";
		case SymbolClass::PDATA: return "PDATA";
		case SymbolClass::FINI: return "FINI";
		case SymbolClass::NONGP: return "NONGP";
	}
	return nullptr;
}

const char* stabs_code_to_string(StabsCode code)
{
	switch(code) {
		case STAB: return "STAB";
		case N_GSYM: return "GSYM";
		case N_FNAME: return "FNAME";
		case N_FUN: return "FUN";
		case N_STSYM: return "STSYM";
		case N_LCSYM: return "LCSYM";
		case N_MAIN: return "MAIN";
		case N_PC: return "PC";
		case N_NSYMS: return "NSYMS";
		case N_NOMAP: return "NOMAP";
		case N_OBJ: return "OBJ";
		case N_OPT: return "OPT";
		case N_RSYM: return "RSYM";
		case N_M2C: return "M2C";
		case N_SLINE: return "SLINE";
		case N_DSLINE: return "DSLINE";
		case N_BSLINE: return "BSLINE";
		case N_EFD: return "EFD";
		case N_EHDECL: return "EHDECL";
		case N_CATCH: return "CATCH";
		case N_SSYM: return "SSYM";
		case N_SO: return "SO";
		case N_LSYM: return "LSYM";
		case N_BINCL: return "BINCL";
		case N_SOL: return "SOL";
		case N_PSYM: return "PSYM";
		case N_EINCL: return "EINCL";
		case N_ENTRY: return "ENTRY";
		case N_LBRAC: return "LBRAC";
		case N_EXCL: return "EXCL";
		case N_SCOPE: return "SCOPE";
		case N_RBRAC: return "RBRAC";
		case N_BCOMM: return "BCOMM";
		case N_ECOMM: return "ECOMM";
		case N_ECOML: return "ECOML";
		case N_NBTEXT: return "NBTEXT";
		case N_NBDATA: return "NBDATA";
		case N_NBBSS: return "NBBSS";
		case N_NBSTS: return "NBSTS";
		case N_NBLCS: return "NBLCS";
		case N_LENG: return "LENG";
	}
	return nullptr;
}

}
