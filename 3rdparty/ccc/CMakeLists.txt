cmake_minimum_required(VERSION 3.14)
project(ccc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ccc STATIC
	src/ccc/ast.cpp
	src/ccc/ast.h
	src/ccc/elf.cpp
	src/ccc/elf.h
	src/ccc/elf_symtab.cpp
	src/ccc/elf_symtab.h
	src/ccc/importer_flags.cpp
	src/ccc/importer_flags.h
	src/ccc/mdebug_analysis.cpp
	src/ccc/mdebug_analysis.h
	src/ccc/mdebug_importer.cpp
	src/ccc/mdebug_importer.h
	src/ccc/mdebug_section.cpp
	src/ccc/mdebug_section.h
	src/ccc/mdebug_symbols.cpp
	src/ccc/mdebug_symbols.h
	src/ccc/sndll.cpp
	src/ccc/sndll.h
	src/ccc/stabs.cpp
	src/ccc/stabs.h
	src/ccc/stabs_to_ast.cpp
	src/ccc/stabs_to_ast.h
	src/ccc/symbol_database.cpp
	src/ccc/symbol_database.h
	src/ccc/symbol_file.cpp
	src/ccc/symbol_file.h
	src/ccc/symbol_table.cpp
	src/ccc/symbol_table.h
	src/ccc/util.cpp
	src/ccc/util.h
)

target_include_directories(ccc PUBLIC src)
