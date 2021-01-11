cmake_minimum_required(VERSION 3.12)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

if(APPLE)
    add_compile_options(-dOBJ_FORMAT_macho)
elseif(WIN32)
    #add_compile_options(-d OBJ_FORMAT_win32)
    # FIXME the command above doesn't seem to work on Windows
    set(CMAKE_ASM_NASM_FLAGS -dOBJ_FORMAT_win32)
else()
    add_compile_options(-dOBJ_FORMAT_elf)
endif()

add_library(FLAC-asm OBJECT
    cpu_asm.nasm
    fixed_asm.nasm
    lpc_asm.nasm)
