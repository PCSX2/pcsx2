#include "rc_consoles.h"

#include <ctype.h>

const char* rc_console_name(uint32_t console_id)
{
  switch (console_id)
  {
    case RC_CONSOLE_3DO:
      return "3DO";

    case RC_CONSOLE_AMIGA:
      return "Amiga";

    case RC_CONSOLE_AMSTRAD_PC:
      return "Amstrad CPC";

    case RC_CONSOLE_APPLE_II:
      return "Apple II";

    case RC_CONSOLE_ARCADE:
      return "Arcade";

    case RC_CONSOLE_ARCADIA_2001:
      return "Arcadia 2001";

    case RC_CONSOLE_ARDUBOY:
      return "Arduboy";

    case RC_CONSOLE_ATARI_2600:
      return "Atari 2600";

    case RC_CONSOLE_ATARI_5200:
      return "Atari 5200";

    case RC_CONSOLE_ATARI_7800:
      return "Atari 7800";

    case RC_CONSOLE_ATARI_JAGUAR:
      return "Atari Jaguar";

    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return "Atari Jaguar CD";

    case RC_CONSOLE_ATARI_LYNX:
      return "Atari Lynx";

    case RC_CONSOLE_ATARI_ST:
        return "Atari ST";

    case RC_CONSOLE_CASSETTEVISION:
      return "CassetteVision";

    case RC_CONSOLE_CDI:
      return "CD-I";

    case RC_CONSOLE_COLECOVISION:
      return "ColecoVision";

    case RC_CONSOLE_COMMODORE_64:
      return "Commodore 64";

    case RC_CONSOLE_DREAMCAST:
      return "Dreamcast";

    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
      return "Elektor TV Games Computer";

    case RC_CONSOLE_EVENTS:
      return "Events";

    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
      return "Fairchild Channel F";

    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
      return "Famicom Disk System";

    case RC_CONSOLE_FM_TOWNS:
      return "FM Towns";

    case RC_CONSOLE_GAME_AND_WATCH:
      return "Game & Watch";

    case RC_CONSOLE_GAMEBOY:
      return "GameBoy";

    case RC_CONSOLE_GAMEBOY_ADVANCE:
      return "GameBoy Advance";

    case RC_CONSOLE_GAMEBOY_COLOR:
      return "GameBoy Color";

    case RC_CONSOLE_GAMECUBE:
      return "GameCube";

    case RC_CONSOLE_GAME_GEAR:
      return "Game Gear";

    case RC_CONSOLE_HUBS:
      return "Hubs";

    case RC_CONSOLE_INTELLIVISION:
      return "Intellivision";

    case RC_CONSOLE_INTERTON_VC_4000:
      return "Interton VC 4000";

    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
      return "Magnavox Odyssey 2";

    case RC_CONSOLE_MASTER_SYSTEM:
      return "Master System";

    case RC_CONSOLE_MEGA_DRIVE:
      return "Sega Genesis";

    case RC_CONSOLE_MEGADUCK:
      return "Mega Duck";

    case RC_CONSOLE_MS_DOS:
      return "MS-DOS";

    case RC_CONSOLE_MSX:
      return "MSX";

    case RC_CONSOLE_NEO_GEO_CD:
      return "Neo Geo CD";

    case RC_CONSOLE_NEOGEO_POCKET:
      return "Neo Geo Pocket";

    case RC_CONSOLE_NINTENDO:
      return "Nintendo Entertainment System";

    case RC_CONSOLE_NINTENDO_64:
      return "Nintendo 64";

    case RC_CONSOLE_NINTENDO_DS:
      return "Nintendo DS";

    case RC_CONSOLE_NINTENDO_DSI:
      return "Nintendo DSi";

    case RC_CONSOLE_NINTENDO_3DS:
      return "Nintendo 3DS";

    case RC_CONSOLE_NOKIA_NGAGE:
      return "Nokia N-Gage";

    case RC_CONSOLE_ORIC:
      return "Oric";

    case RC_CONSOLE_PC6000:
      return "PC-6000";

    case RC_CONSOLE_PC8800:
      return "PC-8000/8800";

    case RC_CONSOLE_PC9800:
      return "PC-9800";

    case RC_CONSOLE_PCFX:
      return "PC-FX";

    case RC_CONSOLE_PC_ENGINE:
      return "PC Engine";

    case RC_CONSOLE_PC_ENGINE_CD:
      return "PC Engine CD";

    case RC_CONSOLE_PLAYSTATION:
      return "PlayStation";

    case RC_CONSOLE_PLAYSTATION_2:
      return "PlayStation 2";

    case RC_CONSOLE_PSP:
      return "PlayStation Portable";

    case RC_CONSOLE_POKEMON_MINI:
      return "Pokemon Mini";

    case RC_CONSOLE_SEGA_32X:
      return "Sega 32X";

    case RC_CONSOLE_SEGA_CD:
      return "Sega CD";
	  
    case RC_CONSOLE_PICO:
      return "Sega Pico";

    case RC_CONSOLE_SATURN:
      return "Sega Saturn";

    case RC_CONSOLE_SG1000:
      return "SG-1000";

    case RC_CONSOLE_SHARPX1:
      return "Sharp X1";

    case RC_CONSOLE_STANDALONE:
      return "Standalone";

    case RC_CONSOLE_SUPER_NINTENDO:
      return "Super Nintendo Entertainment System";

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return "Super CassetteVision";

    case RC_CONSOLE_SUPERVISION:
      return "Watara Supervision";

    case RC_CONSOLE_THOMSONTO8:
      return "Thomson TO8";

    case RC_CONSOLE_TI83:
      return "TI-83";

    case RC_CONSOLE_TIC80:
      return "TIC-80";

    case RC_CONSOLE_UZEBOX:
      return "Uzebox";

    case RC_CONSOLE_VECTREX:
      return "Vectrex";

    case RC_CONSOLE_VIC20:
      return "VIC-20";

    case RC_CONSOLE_VIRTUAL_BOY:
      return "Virtual Boy";

    case RC_CONSOLE_WASM4:
      return "WASM-4";

    case RC_CONSOLE_WII:
      return "Wii";

    case RC_CONSOLE_WII_U:
      return "Wii-U";

    case RC_CONSOLE_WONDERSWAN:
      return "WonderSwan";

    case RC_CONSOLE_X68K:
      return "X68K";

    case RC_CONSOLE_XBOX:
      return "XBOX";

    case RC_CONSOLE_ZEEBO:
      return "Zeebo";

    case RC_CONSOLE_ZX81:
      return "ZX-81";

    case RC_CONSOLE_ZX_SPECTRUM:
      return "ZX Spectrum";

    default:
      return "Unknown";
  }
}

/* ===== 3DO ===== */
/* http://www.arcaderestoration.com/memorymap/48/3DO+Bios.aspx */
/* NOTE: the Opera core attempts to expose the NVRAM as RETRO_SAVE_RAM, but the 3DO documentation
 * says that applications should only access NVRAM through API calls as it's shared across mulitple
 * games. This suggests that even if the core does expose it, it may change depending on which other
 * games the user has played - so ignore it.
 */
static const rc_memory_region_t _rc_memory_regions_3do[] = {
    { 0x000000U, 0x1FFFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
};
static const rc_memory_regions_t rc_memory_regions_3do = { _rc_memory_regions_3do, 1 };

/* ===== Amiga ===== */
/* http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node00D3.html */
static const rc_memory_region_t _rc_memory_regions_amiga[] = {
    { 0x000000U, 0x07FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" }, /* 512KB main RAM */
    { 0x080000U, 0x0FFFFFU, 0x080000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" }, /* 512KB extended RAM */
};
static const rc_memory_regions_t rc_memory_regions_amiga = { _rc_memory_regions_amiga, 2 };

/* ===== Amstrad CPC ===== */
/* http://www.cpcalive.com/docs/amstrad_cpc_6128_memory_map.html */
/* https://www.cpcwiki.eu/index.php/File:AWMG_page151.jpg */
/* The original CPC only had 64KB of memory, but the newer model has 128KB (expandable to 576KB) */
/* https://www.grimware.org/doku.php/documentations/devices/gatearraydo=export_xhtml#mmr */
static const rc_memory_region_t _rc_memory_regions_amstrad_pc[] = {
    { 0x000000U, 0x00003FU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Firmware" },
    { 0x000040U, 0x00B0FFU, 0x000040U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x00B100U, 0x00BFFFU, 0x00B100U, RC_MEMORY_TYPE_SYSTEM_RAM, "Stack and Firmware Data" },
    { 0x00C000U, 0x00FFFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Screen Memory" },
    { 0x010000U, 0x08FFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" },
};
static const rc_memory_regions_t rc_memory_regions_amstrad_pc = { _rc_memory_regions_amstrad_pc, 5 };

/* ===== Apple II ===== */
static const rc_memory_region_t _rc_memory_regions_appleii[] = {
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
    { 0x010000U, 0x01FFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Auxillary RAM" }
};
static const rc_memory_regions_t rc_memory_regions_appleii = { _rc_memory_regions_appleii, 2 };

/* ===== Arcadia 2001 ===== */
/* https://amigan.yatho.com/a-coding.txt */
/* RAM banks 1 and 2 only exist on some variant models - no game actually uses them */
static const rc_memory_region_t _rc_memory_regions_arcadia_2001[] = {
    { 0x000000U, 0x0000FFU, 0x001800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 3 */
    { 0x000100U, 0x0001FFU, 0x001900U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "I/O Area" },
    { 0x000200U, 0x0002FFU, 0x001A00U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 4 */
};
static const rc_memory_regions_t rc_memory_regions_arcadia_2001 = { _rc_memory_regions_arcadia_2001, 3 };

/* ===== Arduboy ===== */
/* https://scienceprog.com/avr-microcontroller-memory-map/ (Atmega32) */
static const rc_memory_region_t _rc_memory_regions_arduboy[] = {
    { 0x000000U, 0x0000FFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Registers" },
    /* https://www.dailydot.com/debug/arduboy-kickstarter/ 2.5KB of RAM */
    /* https://github.com/buserror/simavr/blob/1d227277b3d0039f9faef9ea62880ca3051b14f8/simavr/cores/avr/iom32u4.h#L1444-L1445 */
    { 0x000100U, 0x000AFFU, 0x00000100U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* 1KB of EEPROM https://github.com/libretro/arduous/blob/93e1a6289b42ef48de1fcfb96443981725955ad0/src/arduous/arduous.cpp#L453-L455
     * https://github.com/buserror/simavr/blob/1d227277b3d0039f9faef9ea62880ca3051b14f8/simavr/cores/avr/iom32u4.h#L1450 */
    /* EEPROM has it's own addressing scheme starting at $0000. I've chosen to virtualize the address
     * at $80000000 to avoid a conflict */
    { 0x000B00U, 0x000EFFU, 0x80000000U, RC_MEMORY_TYPE_SAVE_RAM, "EEPROM" }
};
static const rc_memory_regions_t rc_memory_regions_arduboy = { _rc_memory_regions_arduboy, 3 };

/* ===== Atari 2600 ===== */
static const rc_memory_region_t _rc_memory_regions_atari2600[] = {
    { 0x000000U, 0x00007FU, 0x000080U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_atari2600 = { _rc_memory_regions_atari2600, 1 };

/* ===== Atari 7800 ===== */
/* http://www.atarihq.com/danb/files/78map.txt */
/* http://pdf.textfiles.com/technical/7800_devkit.pdf */
static const rc_memory_region_t _rc_memory_regions_atari7800[] = {
    { 0x000000U, 0x0017FFU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware Interface" },
    { 0x001800U, 0x0027FFU, 0x001800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x002800U, 0x002FFFU, 0x002800U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x003000U, 0x0037FFU, 0x003000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x003800U, 0x003FFFU, 0x003800U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored RAM" },
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" },
    { 0x008000U, 0x00FFFFU, 0x008000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM" }
};
static const rc_memory_regions_t rc_memory_regions_atari7800 = { _rc_memory_regions_atari7800, 7 };

/* ===== Atari Jaguar ===== */
/* https://www.mulle-kybernetik.com/jagdox/memorymap.html */
static const rc_memory_region_t _rc_memory_regions_atari_jaguar[] = {
    { 0x000000U, 0x1FFFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_atari_jaguar = { _rc_memory_regions_atari_jaguar, 1 };

/* ===== Atari Lynx ===== */
/* http://www.retroisle.com/atari/lynx/Technical/Programming/lynxprgdumm.php */
static const rc_memory_region_t _rc_memory_regions_atari_lynx[] = {
    { 0x000000U, 0x0000FFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Zero Page" },
    { 0x000100U, 0x0001FFU, 0x000100U, RC_MEMORY_TYPE_SYSTEM_RAM, "Stack" },
    { 0x000200U, 0x00FBFFU, 0x000200U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x00FC00U, 0x00FCFFU, 0x00FC00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "SUZY hardware access" },
    { 0x00FD00U, 0x00FDFFU, 0x00FD00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "MIKEY hardware access" },
    { 0x00FE00U, 0x00FFF7U, 0x00FE00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Boot ROM" },
    { 0x00FFF8U, 0x00FFFFU, 0x00FFF8U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware vectors" }
};
static const rc_memory_regions_t rc_memory_regions_atari_lynx = { _rc_memory_regions_atari_lynx, 7 };

/* ===== ColecoVision ===== */
static const rc_memory_region_t _rc_memory_regions_colecovision[] = {
    /* "System RAM" refers to the main RAM at 0x6000-0x63FF. However, this RAM might not always be visible.
     * If the Super Game Module (SGM) is active, then it might overlay its own RAM at 0x0000-0x1FFF and 0x2000-0x7FFF.
     * These positions overlap the BIOS and System RAM, therefore we use virtual addresses for these memory spaces. */
    { 0x000000U, 0x0003FFU, 0x006000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x000400U, 0x0023FFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "SGM Low RAM" }, /* Normally situated at 0x0000-0x1FFF, which overlaps the BIOS */
    { 0x002400U, 0x0083FFU, 0x012000U, RC_MEMORY_TYPE_SYSTEM_RAM, "SGM High RAM" } /* Normally situated at 0x2000-0x7FFF, which overlaps System RAM */
};
static const rc_memory_regions_t rc_memory_regions_colecovision = { _rc_memory_regions_colecovision, 3 };

/* ===== Commodore 64 ===== */
/* https://www.c64-wiki.com/wiki/Memory_Map */
/* https://sta.c64.org/cbm64mem.html */
/* NOTE: Several blocks of C64 memory can be bank-switched for ROM data (see https://www.c64-wiki.com/wiki/Bank_Switching).
 *       Achievement triggers rely on values changing, so we don't really need to look at the ROM data.
 *       The achievement logic assumes the RAM data is always present in the bankable blocks. As such,
 *       clients providing memory to achievements should always return the RAM values at the queried address. */
static const rc_memory_region_t _rc_memory_regions_c64[] = {
    { 0x000000U, 0x0003FFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x000400U, 0x0007FFU, 0x000400U, RC_MEMORY_TYPE_VIDEO_RAM, "Screen RAM" },
    { 0x000800U, 0x009FFFU, 0x000800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* BASIC area. $8000-$9FFF can bank to cartridge ROM */
    { 0x00A000U, 0x00BFFFU, 0x00A000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* can bank to BASIC ROM or cartridge ROM */
    { 0x00C000U, 0x00CFFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x00D000U, 0x00DFFFU, 0x00D000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* can bank to I/O Area or character ROM */
    { 0x00E000U, 0x00FFFFU, 0x00E000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* can bank to kernel ROM */
};
static const rc_memory_regions_t rc_memory_regions_c64 = { _rc_memory_regions_c64, 7 };

/* ===== Dreamcast ===== */
/* http://archiv.sega-dc.de/munkeechuff/hardware/Memory.html */
static const rc_memory_region_t _rc_memory_regions_dreamcast[] = {
    { 0x00000000U, 0x00FFFFFFU, 0x0C000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_dreamcast = { _rc_memory_regions_dreamcast, 1 };

/* ===== Elektor TV Games Computer ===== */
/* https://amigan.yatho.com/e-coding.txt */
static const rc_memory_region_t _rc_memory_regions_elektor_tv_games[] = {
    { 0x000000U, 0x0013FFU, 0x000800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x001400U, 0x0014FFU, 0x001C00U, RC_MEMORY_TYPE_UNUSED, "Unused" }, /* mirror of $1D00-$1DFF */
    { 0x001500U, 0x0016FFU, 0x001D00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "I/O Area" }, /* two 256-byte I/O areas */
    { 0x001700U, 0x0017FFU, 0x001F00U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_elektor_tv_games = { _rc_memory_regions_elektor_tv_games, 4 };

/* ===== Fairchild Channel F ===== */
static const rc_memory_region_t _rc_memory_regions_fairchild_channel_f[] = {
    /* "System RAM" is actually just a bunch of registers internal to CPU so all carts have it.
     * "Video RAM" is part of the console so it's always available but it is write-only by the ROMs.
     * "Cartridge RAM" is the cart BUS. Most carts only have ROMs on this bus. Exception are
     *     German Schach and homebrew carts that have 2K of RAM there in addition to ROM.
     * "F2102 RAM" is used by Maze for 1K of RAM.
     * https://discord.com/channels/310192285306454017/645777658319208448/967001438087708714 */
    { 0x00000000U, 0x0000003FU, 0x00100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x00000040U, 0x0000083FU, 0x00300000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" },
    { 0x00000840U, 0x0001083FU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    { 0x00010840U, 0x00010C3FU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "F2102 RAM" }
};
static const rc_memory_regions_t rc_memory_regions_fairchild_channel_f = { _rc_memory_regions_fairchild_channel_f, 4 };

/* ===== Famicon Disk System ===== */
/* https://fms.komkon.org/EMUL8/NES.html */
static const rc_memory_region_t _rc_memory_regions_famicom_disk_system[] = {
    { 0x0000U, 0x07FFU, 0x0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x0800U, 0x0FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1000U, 0x17FFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1800U, 0x1FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x2000U, 0x2007U, 0x2000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "PPU Register" },
    { 0x2008U, 0x3FFFU, 0x2008U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored PPU Register" }, /* repeats every 8 bytes */
    { 0x4000U, 0x4017U, 0x4000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O register" },
    { 0x4018U, 0x401FU, 0x4018U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O test register" },
    { 0x4020U, 0x40FFU, 0x4020U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "FDS I/O registers"},
    { 0x4100U, 0x5FFFU, 0x4100U, RC_MEMORY_TYPE_READONLY, "Cartridge data"}, /* varies by mapper */
    { 0x6000U, 0xDFFFU, 0x6000U, RC_MEMORY_TYPE_SYSTEM_RAM, "FDS RAM"},
    { 0xE000U, 0xFFFFU, 0xE000U, RC_MEMORY_TYPE_READONLY, "FDS BIOS ROM"},
};
static const rc_memory_regions_t rc_memory_regions_famicom_disk_system = { _rc_memory_regions_famicom_disk_system, 12 };

/* ===== GameBoy / MegaDuck ===== */
static const rc_memory_region_t _rc_memory_regions_gameboy[] = {
    { 0x000000U, 0x0000FFU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt vector" },
    { 0x000100U, 0x00014FU, 0x000100U, RC_MEMORY_TYPE_READONLY, "Cartridge header" },
    { 0x000150U, 0x003FFFU, 0x000150U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (fixed)" }, /* bank 0 */
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (paged)" }, /* bank 1-XX (switchable) */
    { 0x008000U, 0x0097FFU, 0x008000U, RC_MEMORY_TYPE_VIDEO_RAM, "Tile RAM" },
    { 0x009800U, 0x009BFFU, 0x009800U, RC_MEMORY_TYPE_VIDEO_RAM, "BG1 map data" },
    { 0x009C00U, 0x009FFFU, 0x009C00U, RC_MEMORY_TYPE_VIDEO_RAM, "BG2 map data" },
    { 0x00A000U, 0x00BFFFU, 0x00A000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM (bank 0)"},
    { 0x00C000U, 0x00CFFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (fixed)" },
    { 0x00D000U, 0x00DFFFU, 0x00D000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (fixed)" },
    { 0x00E000U, 0x00FDFFU, 0x00C000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Echo RAM" },
    { 0x00FE00U, 0x00FE9FU, 0x00FE00U, RC_MEMORY_TYPE_VIDEO_RAM, "Sprite RAM"},
    { 0x00FEA0U, 0x00FEFFU, 0x00FEA0U, RC_MEMORY_TYPE_UNUSED, ""},
    { 0x00FF00U, 0x00FF7FU, 0x00FF00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware I/O"},
    { 0x00FF80U, 0x00FFFEU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "Quick RAM"},
    { 0x00FFFFU, 0x00FFFFU, 0x00FFFFU, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt enable"},

    /* GameBoy's cartridge RAM may have a total of up to 16 banks that can be paged through $A000-$BFFF.
     * It is desirable to always have access to these extra banks. We do this by expecting the extra banks
     * to be addressable at addresses not supported by the native system. 0x10000-0x16000 is reserved
     * for the extra banks of system memory that are exclusive to the GameBoy Color. */
    { 0x010000U, 0x015FFFU, 0x010000U, RC_MEMORY_TYPE_UNUSED, "Unused (GameBoy Color exclusive)" },
    { 0x016000U, 0x033FFFU, 0x016000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM (banks 1-15)" },
};
static const rc_memory_regions_t rc_memory_regions_megaduck = { _rc_memory_regions_gameboy, 16 };
static const rc_memory_regions_t rc_memory_regions_gameboy = { _rc_memory_regions_gameboy, 18 };

/* ===== GameBoy Color ===== */
static const rc_memory_region_t _rc_memory_regions_gameboy_color[] = {
    { 0x000000U, 0x0000FFU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt vector" },
    { 0x000100U, 0x00014FU, 0x000100U, RC_MEMORY_TYPE_READONLY, "Cartridge header" },
    { 0x000150U, 0x003FFFU, 0x000150U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (fixed)" }, /* bank 0 */
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM (paged)" }, /* bank 1-XX (switchable) */
    { 0x008000U, 0x0097FFU, 0x008000U, RC_MEMORY_TYPE_VIDEO_RAM, "Tile RAM" },
    { 0x009800U, 0x009BFFU, 0x009800U, RC_MEMORY_TYPE_VIDEO_RAM, "BG1 map data" },
    { 0x009C00U, 0x009FFFU, 0x009C00U, RC_MEMORY_TYPE_VIDEO_RAM, "BG2 map data" },
    { 0x00A000U, 0x00BFFFU, 0x00A000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM (bank 0)"},
    { 0x00C000U, 0x00CFFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (bank 0)" },
    { 0x00D000U, 0x00DFFFU, 0x00D000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (bank 1)" },
    { 0x00E000U, 0x00FDFFU, 0x00C000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Echo RAM" },
    { 0x00FE00U, 0x00FE9FU, 0x00FE00U, RC_MEMORY_TYPE_VIDEO_RAM, "Sprite RAM"},
    { 0x00FEA0U, 0x00FEFFU, 0x00FEA0U, RC_MEMORY_TYPE_UNUSED, ""},
    { 0x00FF00U, 0x00FF7FU, 0x00FF00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Hardware I/O"},
    { 0x00FF80U, 0x00FFFEU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "Quick RAM"},
    { 0x00FFFFU, 0x00FFFFU, 0x00FFFFU, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Interrupt enable"},

    /* GameBoy Color provides 6 extra banks of system memory that can be paged out through the $D000-$DFFF,
     * and the cartridge RAM may have a total of up to 16 banks page through $A000-$BFFF.
     * It is desirable to always have access to these extra banks. We do this by expecting the extra banks
     * to be addressable at addresses not supported by the native system. */
    { 0x010000U, 0x015FFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM (banks 2-7)" },
    { 0x016000U, 0x033FFFU, 0x016000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM (banks 1-15)" },
};
static const rc_memory_regions_t rc_memory_regions_gameboy_color = { _rc_memory_regions_gameboy_color, 18 };

/* ===== GameBoy Advance ===== */
/* http://problemkaputt.de/gbatek-gba-memory-map.htm */
static const rc_memory_region_t _rc_memory_regions_gameboy_advance[] = {
    { 0x000000U, 0x007FFFU, 0x03000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* 32KB  Internal Work RAM */
    { 0x008000U, 0x047FFFU, 0x02000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* 256KB External Work RAM */
    { 0x048000U, 0x057FFFU, 0x0E000000U, RC_MEMORY_TYPE_SAVE_RAM, "Save RAM" }      /* 64KB  Game Pak SRAM */
};
static const rc_memory_regions_t rc_memory_regions_gameboy_advance = { _rc_memory_regions_gameboy_advance, 3 };

/* ===== GameCube ===== */
/* https://wiibrew.org/wiki/Memory_map */
static const rc_memory_region_t _rc_memory_regions_gamecube[] = {
    { 0x00000000U, 0x017FFFFF, 0x80000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_gamecube = { _rc_memory_regions_gamecube, 1 };

/* ===== Game Gear ===== */
/* https://www.smspower.org/Development/MemoryMap */
/* https://www.smspower.org/Development/Mappers */
static const rc_memory_region_t _rc_memory_regions_game_gear[] = {
    { 0x000000U, 0x001FFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* GG/SMS have various possible mappings for cartridge memory depending on the mapper used.
     * However, these ultimately do not map all of their memory at once, typically requiring banking.
     * Thus, the "real address" used is just a virtual address mapping all cartridge memory in one contiguous block.
     * Note that this may possibly refer to non-battery backed "extended RAM" so this isn't strictly RC_MEMORY_TYPE_SAVE_RAM.
     * libretro cores expose "extended RAM" as RETRO_MEMORY_SAVE_RAM regardless however.
     */
    { 0x002000U, 0x009FFFU, 0x010000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_game_gear = { _rc_memory_regions_game_gear, 2 };

/* ===== Intellivision ===== */
/* http://wiki.intellivision.us/index.php/Memory_Map */
/* NOTE: Intellivision memory addresses point at 16-bit values. FreeIntv exposes them as little-endian
 *       32-bit values. As such, the addresses are off by a factor of 4 _and_ the data is only where we
 *       expect it on little-endian systems.
 */
static const rc_memory_region_t _rc_memory_regions_intellivision[] = {
    /* For backwards compatibility, register a 128-byte chunk of video RAM so the system memory
     * will start at $0080. $0000-$007F previously tried to map to the STIC video registers as
     * RETRO_MEMORY_VIDEO_RAM, and FreeIntv didn't expose any RETRO_MEMORY_VIDEO_RAM, so the first
     * byte of RETRO_MEMORY_SYSTEM_RAM was registered at $0080. The data at $0080 is actually the
     * STIC registers (4 bytes each), so we need to provide an arbitrary 128-byte padding that
     * claims to be video RAM to ensure the system RAM ends up at the right address.
     */
    { 0x000000U, 0x00007FU, 0xFFFFFFU, RC_MEMORY_TYPE_VIDEO_RAM, "" },

    /* RetroAchievements address = real address x4 + 0x80.
     * These all have to map to RETRO_MEMORY_SYSTEM_RAM (even the video-related fields) as the
     * entire block is exposed as a single entity by FreeIntv */

    /* $0000-$007F: STIC registers, $0040-$007F are readonly */
    { 0x000080U, 0x00027FU, 0x000000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "STIC Registers" },
    /* $0080-$00FF: unused */
    { 0x000280U, 0x00047FU, 0x000080U, RC_MEMORY_TYPE_UNUSED, "" },
    /* $0100-$035F: system RAM, $0100-$01EF is scratch memory and only 8-bits per address */
    { 0x000480U, 0x000DFFU, 0x000100U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* $0360-$03FF: unused */
    { 0x000E00U, 0x00107FU, 0x000360U, RC_MEMORY_TYPE_UNUSED, "" },
    /* $0400-$0FFF: cartridge RAM */
    { 0x001080U, 0x00407FU, 0x000400U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    /* $1000-$1FFF: unused */
    { 0x004080U, 0x00807FU, 0x001000U, RC_MEMORY_TYPE_UNUSED, "" },
    /* $2000-$2FFF: cartridge RAM */
    { 0x008080U, 0x00C07FU, 0x002000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    /* $3000-$3FFF: video RAM */
    { 0x00C080U, 0x01007FU, 0x003000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Video RAM" },
    /* $4000-$FFFF: cartridge RAM */
    { 0x010080U, 0x04007FU, 0x004000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
};
static const rc_memory_regions_t rc_memory_regions_intellivision = { _rc_memory_regions_intellivision, 10 };

/* ===== Interton VC 4000 ===== */
/* https://amigan.yatho.com/i-coding.txt */
/* Cartridge RAM is not persisted, it's just expanded storage */
static const rc_memory_region_t _rc_memory_regions_interton_vc_4000[] = {
    { 0x000000U, 0x0003FFU, 0x001800U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    { 0x000400U, 0x0004FFU, 0x001E00U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "I/O Area" },
    { 0x000500U, 0x0005FFU, 0x001F00U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, 
};
static const rc_memory_regions_t rc_memory_regions_interton_vc_4000 = { _rc_memory_regions_interton_vc_4000, 3 };

/* ===== Magnavox Odyssey 2 ===== */
/* https://sudonull.com/post/76885-Architecture-and-programming-Philips-Videopac-Magnavox-Odyssey-2 */
static const rc_memory_region_t _rc_memory_regions_magnavox_odyssey_2[] = {
    /* Internal and external RAMs are reachable using unique instructions.
     * The real addresses provided are virtual and for mapping purposes only. */
    { 0x000000U, 0x00003FU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Internal RAM" },
    { 0x000040U, 0x00013FU, 0x000040U, RC_MEMORY_TYPE_SYSTEM_RAM, "External RAM" }
};
static const rc_memory_regions_t rc_memory_regions_magnavox_odyssey_2 = { _rc_memory_regions_magnavox_odyssey_2, 2 };

/* ===== Master System ===== */
/* https://www.smspower.org/Development/MemoryMap */
/* https://www.smspower.org/Development/Mappers */
static const rc_memory_region_t _rc_memory_regions_master_system[] = {
    { 0x000000U, 0x001FFFU, 0x00C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* GG/SMS have various possible mappings for cartridge memory depending on the mapper used.
     * However, these ultimately do not map all of their memory at once, typically requiring banking.
     * Thus, the "real address" used is just a virtual address mapping all cartridge memory in one contiguous block.
     * Note that this may possibly refer to non-battery backed "extended RAM" so this isn't strictly RC_MEMORY_TYPE_SAVE_RAM.
     * libretro cores expose "extended RAM" as RETRO_MEMORY_SAVE_RAM regardless however.
     */
    { 0x002000U, 0x009FFFU, 0x010000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_master_system = { _rc_memory_regions_master_system, 2 };

/* ===== MegaDrive (Genesis) ===== */
/* https://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_megadrive[] = {
    { 0x000000U, 0x00FFFFU, 0xFF0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x010000U, 0x01FFFFU, 0x000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_megadrive = { _rc_memory_regions_megadrive, 2 };

/* ===== MegaDrive 32X (Genesis 32X) ===== */
/* http://devster.monkeeh.com/sega/32xguide1.txt */
static const rc_memory_region_t _rc_memory_regions_megadrive_32x[] = {
    { 0x000000U, 0x00FFFFU, 0x00FF0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* Main MegaDrive RAM */
    { 0x010000U, 0x04FFFFU, 0x06000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "32X RAM"},     /* Additional 32X RAM */
    { 0x050000U, 0x05FFFFU, 0x00000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_megadrive_32x = { _rc_memory_regions_megadrive_32x, 3 };

/* ===== MSX ===== */
/* https://www.msx.org/wiki/The_Memory */
/* MSX only has 64KB of addressable RAM, of which 32KB is reserved for the system/BIOS.
 * However, the system has up to 512KB of RAM, which is paged into the addressable RAM
 * We expect the raw RAM to be exposed, rather than force the devs to worry about the
 * paging system. The entire RAM is expected to appear starting at $10000, which is not
 * addressable by the system itself.
 */
static const rc_memory_region_t _rc_memory_regions_msx[] = {
    { 0x000000U, 0x07FFFFU, 0x010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_msx = { _rc_memory_regions_msx, 1 };

/* ===== MS DOS ===== */
static const rc_memory_region_t _rc_memory_regions_ms_dos[] = {
    /* DOS emulators split the 640 KB conventional memory into two regions.
     * First the part of the conventional memory given to the running game at $000000.
     * The part of the conventional memory containing DOS and BIOS controlled memory
     * is at $100000. The length of these can vary depending on the hardware
     * and DOS version (or emulated DOS shell).
     * These first two regions will only ever total to 640 KB but the regions map
     * to 1 MB bounds to make resulting memory addresses more readable.
     * When emulating a game not under DOS (so called 'PC Booter' games), the entirety
     * of the 640 KB conventional memory block will be at $000000.
     */
    { 0x00000000U, 0x0009FFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Game Conventional Memory" },
    { 0x000A0000U, 0x000FFFFFU, 0x000A0000U, RC_MEMORY_TYPE_UNUSED, "Padding to align OS Conventional Memory" },
    { 0x00100000U, 0x0019FFFFU, 0x00100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "OS Conventional Memory" },
    { 0x001A0000U, 0x001FFFFFU, 0x001A0000U, RC_MEMORY_TYPE_UNUSED, "Padding to align Expanded Memory" },
    /* Last is all the expanded memory which for now we map up to 64 MB which should be
     * enough for the games we want to cover. An emulator might emulate more than that.
     */
    { 0x00200000U, 0x041FFFFFU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Expanded Memory" }
};
static const rc_memory_regions_t rc_memory_regions_ms_dos = { _rc_memory_regions_ms_dos, 5 };

/* ===== Neo Geo Pocket ===== */
/* http://neopocott.emuunlim.com/docs/tech-11.txt */
static const rc_memory_region_t _rc_memory_regions_neo_geo_pocket[] = {
    /* The docs suggest there's Work RAM exposed from $0000-$6FFF, Sound RAM from $7000-$7FFF, and Video 
     * RAM from $8000-$BFFF, but both MednafenNGP and FBNeo only expose system RAM from $4000-$7FFF */
    { 0x000000U, 0x003FFFU, 0x004000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_neo_geo_pocket = { _rc_memory_regions_neo_geo_pocket, 1 };

/* ===== Neo Geo CD ===== */
/* https://wiki.neogeodev.org/index.php?title=68k_memory_map */
/* NeoCD exposes $000000-$1FFFFF as System RAM, but it seems like only the WORKRAM section is used.
 * This is consistent with http://www.hardmvs.fr/manuals/NeoGeoProgrammersGuide.pdf (page25), which says:
 *
 *   Furthermore, the NEO-GEO provides addresses 100000H-10FFFFH as a work area, out of  which the
 *   addresses 10F300H-10FFFFH are reserved exclusively for use by the system program. Therefore,
 *   every game is to use addresses 100000H-10F2FFH.
 *
 * Also note that PRG files (game ROM) can be loaded anywhere else in the $000000-$1FFFFF range.
 * AoF3 illustrates this pretty clearly: https://wiki.neogeodev.org/index.php?title=IPL_file
 *
 *   PROG_CD.PRG,0,0
 *   PROG_CDX.PRG,0,058000
 *   CNV_NM.PRG,0,0C0000
 *   FIX_DATA.PRG,0,0FD000
 *   OBJACTLK.PRG,0,130000
 *   SSEL_CNV.PRG,0,15A000
 *   SSEL_BAK.PRG,0,16F000
 *   HITMSG.PRG,0,170000
 *   SSEL_SPR.PRG,0,19D000
 */
static const rc_memory_region_t _rc_memory_regions_neo_geo_cd[] = {
    { 0x000000U, 0x00F2FFU, 0x00100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* NOTE: some BIOS settings are exposed through the reserved RAM: https://wiki.neogeodev.org/index.php?title=68k_ASM_defines */
    { 0x00F300U, 0x00FFFFU, 0x0010F300U, RC_MEMORY_TYPE_SYSTEM_RAM, "Reserved RAM" },
};
static const rc_memory_regions_t rc_memory_regions_neo_geo_cd = { _rc_memory_regions_neo_geo_cd, 2 };

/* ===== Nintendo Entertainment System ===== */
/* https://wiki.nesdev.com/w/index.php/CPU_memory_map */
static const rc_memory_region_t _rc_memory_regions_nes[] = {
    { 0x0000U, 0x07FFU, 0x0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x0800U, 0x0FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1000U, 0x17FFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x1800U, 0x1FFFU, 0x0000U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirror RAM" }, /* duplicates memory from $0000-$07FF */
    { 0x2000U, 0x2007U, 0x2000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "PPU Register" },
    { 0x2008U, 0x3FFFU, 0x2008U, RC_MEMORY_TYPE_VIRTUAL_RAM, "Mirrored PPU Register" }, /* repeats every 8 bytes */
    { 0x4000U, 0x4017U, 0x4000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O register" },
    { 0x4018U, 0x401FU, 0x4018U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "APU and I/O test register" },

    /* NOTE: these are for the original NES/Famicom */
    { 0x4020U, 0x5FFFU, 0x4020U, RC_MEMORY_TYPE_READONLY, "Cartridge data"}, /* varies by mapper */
    { 0x6000U, 0x7FFFU, 0x6000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM"},
    { 0x8000U, 0xFFFFU, 0x8000U, RC_MEMORY_TYPE_READONLY, "Cartridge ROM"},
};
static const rc_memory_regions_t rc_memory_regions_nes = { _rc_memory_regions_nes, 11 };

/* ===== Nintendo 64 ===== */
/* https://raw.githubusercontent.com/mikeryan/n64dev/master/docs/n64ops/n64ops%23h.txt */
/* https://n64brew.dev/wiki/Memory_map#Virtual_Memory_Map */
static const rc_memory_region_t _rc_memory_regions_n64[] = {
    { 0x000000U, 0x1FFFFFU, 0x80000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RDRAM 1 */
    { 0x200000U, 0x3FFFFFU, 0x80200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RDRAM 2 */
    { 0x400000U, 0x7FFFFFU, 0x80400000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }  /* expansion pak */
};
static const rc_memory_regions_t rc_memory_regions_n64 = { _rc_memory_regions_n64, 3 };

/* ===== Nintendo DS ===== */
/* https://www.akkit.org/info/gbatek.htm#dsmemorymaps */
static const rc_memory_region_t _rc_memory_regions_nintendo_ds[] = {
    { 0x0000000U, 0x03FFFFFU, 0x02000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* To keep DS/DSi memory maps aligned, padding is set here for the DSi's extra RAM */
    { 0x0400000U, 0x0FFFFFFU, 0x02400000U, RC_MEMORY_TYPE_UNUSED, "Unused (DSi exclusive)" },
    /* The DS/DSi have "tightly coupled memory": very fast memory directly connected to the CPU.
     * This memory has an instruction variant (ITCM) and a data variant (DTCM).
     * For achievement purposes it is useful to be able to access the data variant.
     * This memory does not have a fixed address on console, being able to be moved to any $0xxxx000 region.
     * While normally this kind of memory is addressed outside of the possible native addressing space, this is simply not possible,
     * as the DS/DSi's address space covers all possible uint32_t values.
     * $0E000000 is used here as a "pseudo-end," as this is nearly the end of all the memory actually mapped to addresses
     * This means that (with the exception of $FFFF0000 onwards, which has the ARM9 BIOS mapped) $0E000000 onwards has nothing mapped to it
     */
    { 0x1000000U, 0x1003FFFU, 0x0E000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Data TCM" }
};
static const rc_memory_regions_t rc_memory_regions_nintendo_ds = { _rc_memory_regions_nintendo_ds, 3 };

/* ===== Nintendo DSi ===== */
/* https://problemkaputt.de/gbatek.htm#dsiiomap */
static const rc_memory_region_t _rc_memory_regions_nintendo_dsi[] = {
    { 0x0000000U, 0x0FFFFFFU, 0x02000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x1000000U, 0x1003FFFU, 0x0E000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Data TCM" }
};
static const rc_memory_regions_t rc_memory_regions_nintendo_dsi = { _rc_memory_regions_nintendo_dsi, 2 };

/* ===== Oric ===== */
static const rc_memory_region_t _rc_memory_regions_oric[] = {
    /* actual size depends on machine type - up to 64KB */
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_oric = { _rc_memory_regions_oric, 1 };

/* ===== PC-8800 ===== */
static const rc_memory_region_t _rc_memory_regions_pc8800[] = {
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Main RAM" },
    { 0x010000U, 0x010FFFU, 0x010000U, RC_MEMORY_TYPE_VIDEO_RAM, "Text VRAM" } /* technically VRAM, but often used as system RAM */
};
static const rc_memory_regions_t rc_memory_regions_pc8800 = { _rc_memory_regions_pc8800, 2 };

/* ===== PC Engine ===== */
/* http://www.archaicpixels.com/Memory_Map */
static const rc_memory_region_t _rc_memory_regions_pc_engine[] = {
    { 0x000000U, 0x001FFFU, 0x1F0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_pc_engine = { _rc_memory_regions_pc_engine, 1 };

/* ===== PC Engine CD===== */
/* http://www.archaicpixels.com/Memory_Map */
static const rc_memory_region_t _rc_memory_regions_pc_engine_cd[] = {
    { 0x000000U, 0x001FFFU, 0x1F0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x002000U, 0x011FFFU, 0x100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "CD RAM" },
    { 0x012000U, 0x041FFFU, 0x0D0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Super System Card RAM" },
    { 0x042000U, 0x0427FFU, 0x1EE000U, RC_MEMORY_TYPE_SAVE_RAM,   "CD Battery-backed RAM" }
};
static const rc_memory_regions_t rc_memory_regions_pc_engine_cd = { _rc_memory_regions_pc_engine_cd, 4 };

/* ===== PC-FX ===== */
/* http://daifukkat.su/pcfx/data/memmap.html */
static const rc_memory_region_t _rc_memory_regions_pcfx[] = {
    { 0x000000U, 0x1FFFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x200000U, 0x207FFFU, 0xE0000000U, RC_MEMORY_TYPE_SAVE_RAM, "Internal Backup Memory" },
    { 0x208000U, 0x20FFFFU, 0xE8000000U, RC_MEMORY_TYPE_SAVE_RAM, "External Backup Memory" },
};
static const rc_memory_regions_t rc_memory_regions_pcfx = { _rc_memory_regions_pcfx, 3 };

/* ===== PlayStation ===== */
/* http://www.raphnet.net/electronique/psx_adaptor/Playstation.txt */
static const rc_memory_region_t _rc_memory_regions_playstation[] = {
    { 0x000000U, 0x00FFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x010000U, 0x1FFFFFU, 0x00010000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x200000U, 0x2003FFU, 0x1F800000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Scratchpad RAM" }
};
static const rc_memory_regions_t rc_memory_regions_playstation = { _rc_memory_regions_playstation, 3 };

/* ===== PlayStation 2 ===== */
/* https://psi-rockin.github.io/ps2tek/ */
static const rc_memory_region_t _rc_memory_regions_playstation2[] = {
    { 0x00000000U, 0x000FFFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x00100000U, 0x01FFFFFFU, 0x00100000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x02000000U, 0x02003FFFU, 0x70000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Scratchpad RAM" },
};
static const rc_memory_regions_t rc_memory_regions_playstation2 = { _rc_memory_regions_playstation2, 3 };

/* ===== PlayStation Portable ===== */
/* https://github.com/uofw/upspd/wiki/Memory-map */
static const rc_memory_region_t _rc_memory_regions_psp[] = {
    { 0x00000000U, 0x007FFFFFU, 0x08000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Kernel RAM" },
    { 0x00800000U, 0x01FFFFFFU, 0x08800000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_psp = { _rc_memory_regions_psp, 2 };

/* ===== Pokemon Mini ===== */
/* https://www.pokemon-mini.net/documentation/memory-map/ */
static const rc_memory_region_t _rc_memory_regions_pokemini[] = {
    { 0x000000U, 0x000FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "BIOS RAM" },
    { 0x001000U, 0x001FFFU, 0x001000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_pokemini = { _rc_memory_regions_pokemini, 2 };

/* ===== Sega CD ===== */
/* https://en.wikibooks.org/wiki/Genesis_Programming/68K_Memory_map/ */
static const rc_memory_region_t _rc_memory_regions_segacd[] = {
    { 0x000000U, 0x00FFFFU, 0x00FF0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "68000 RAM" },
    { 0x010000U, 0x08FFFFU, 0x80020000U, RC_MEMORY_TYPE_SYSTEM_RAM, "CD PRG RAM" }, /* normally banked into $020000-$03FFFF */
    { 0x090000U, 0x0AFFFFU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "CD WORD RAM" }
};
static const rc_memory_regions_t rc_memory_regions_segacd = { _rc_memory_regions_segacd, 3 };

/* ===== Sega Saturn ===== */
/* https://segaretro.org/Sega_Saturn_hardware_notes_(2004-04-27) */
static const rc_memory_region_t _rc_memory_regions_saturn[] = {
    { 0x000000U, 0x0FFFFFU, 0x00200000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Work RAM Low" },
    { 0x100000U, 0x1FFFFFU, 0x06000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Work RAM High" }
};
static const rc_memory_regions_t rc_memory_regions_saturn = { _rc_memory_regions_saturn, 2 };

/* ===== SG-1000 ===== */
/* https://www.smspower.org/Development/MemoryMap */
static const rc_memory_region_t _rc_memory_regions_sg1000[] = {
    { 0x000000U, 0x0003FFU, 0xC000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* https://github.com/libretro/FBNeo/blob/697801c6262be6ca91615cf905444d3e039bc06f/src/burn/drv/sg1000/d_sg1000.cpp#L210-L237 */
    /* Expansion mode B exposes 8KB at $C000. The first 2KB hides the System RAM, but since the address matches,
       we'll leverage that definition and expand it another 6KB */
    { 0x000400U, 0x001FFFU, 0xC400U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" },
    /* Expansion mode A exposes 8KB at $2000 */
    { 0x002000U, 0x003FFFU, 0x2000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" },
    /* Othello exposes 2KB at $8000, and The Castle exposes 8KB at $8000 */
    { 0x004000U, 0x005FFFU, 0x8000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Extended RAM" }
};
static const rc_memory_regions_t rc_memory_regions_sg1000 = { _rc_memory_regions_sg1000, 4 };

/* ===== Super Cassette Vision ===== */
/* https://github.com/mamedev/mame/blob/f32bb79e8541ba96d3a8144b220c48fb7536ba4b/src/mame/epoch/scv.cpp#L78-L86 */
/* SCV only has 128 bytes of system RAM, any additional memory is provided on the individual carts and is
 * not backed up by battery. */
/* http://www.videogameconsolelibrary.com/pg80-super_cass_vis.htm#page=specs */
static const rc_memory_region_t _rc_memory_regions_scv[] = {
    { 0x000000U, 0x000FFFU, 0x000000U, RC_MEMORY_TYPE_READONLY, "System ROM" }, /* BIOS */
    { 0x001000U, 0x001FFFU, 0x001000U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x002000U, 0x003FFFU, 0x002000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" }, /* only really goes to $33FF? */
    { 0x004000U, 0x007FFFU, 0x004000U, RC_MEMORY_TYPE_UNUSED, "" },
    { 0x008000U, 0x00FF7FU, 0x008000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Cartridge RAM" },
    { 0x00FF80U, 0x00FFFFU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_scv = { _rc_memory_regions_scv, 6 };

/* ===== Super Nintendo ===== */
/* https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_memory_map#LoROM */
static const rc_memory_region_t _rc_memory_regions_snes[] = {
    { 0x000000U, 0x01FFFFU, 0x07E0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* Cartridge RAM here could be in a variety of places in SNES memory, depending on the ROM type.
     * Due to this, we place Cartridge RAM outside of the possible native addressing space.
     * Note that this also covers SA-1 BW-RAM (which is exposed as RETRO_MEMORY_SAVE_RAM for libretro).
     */
    { 0x020000U, 0x09FFFFU, 0x1000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" },
    /* I-RAM on the SA-1 is normally at 0x003000. However, this address typically just has a mirror of System RAM for other ROM types.
     * To avoid conflicts, don't use 0x003000, instead map it outside of the possible native addressing space.
     */
    { 0x0A0000U, 0x0A07FFU, 0x1080000U, RC_MEMORY_TYPE_SYSTEM_RAM, "I-RAM (SA-1)" }
};
static const rc_memory_regions_t rc_memory_regions_snes = { _rc_memory_regions_snes, 3 };

/* ===== Thomson TO8 ===== */
/* https://github.com/mamedev/mame/blob/master/src/mame/drivers/thomson.cpp#L1617 */
static const rc_memory_region_t _rc_memory_regions_thomson_to8[] = {
    { 0x000000U, 0x07FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_thomson_to8 = { _rc_memory_regions_thomson_to8, 1 };

/* ===== TI-83 ===== */
/* https://tutorials.eeems.ca/ASMin28Days/lesson/day03.html#mem */
static const rc_memory_region_t _rc_memory_regions_ti83[] = {
    { 0x000000U, 0x007FFFU, 0x008000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
};
static const rc_memory_regions_t rc_memory_regions_ti83 = { _rc_memory_regions_ti83, 1 };

/* ===== TIC-80 ===== */
/* https://github.com/nesbox/TIC-80/wiki/RAM */
static const rc_memory_region_t _rc_memory_regions_tic80[] = {
    { 0x000000U, 0x003FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Video RAM" }, /* have to classify this as system RAM because the core exposes it as part of the RETRO_MEMORY_SYSTEM_RAM */
    { 0x004000U, 0x005FFFU, 0x004000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Tile RAM" },
    { 0x006000U, 0x007FFFU, 0x006000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Sprite RAM" },
    { 0x008000U, 0x00FF7FU, 0x008000U, RC_MEMORY_TYPE_SYSTEM_RAM, "MAP RAM" },
    { 0x00FF80U, 0x00FF8BU, 0x00FF80U, RC_MEMORY_TYPE_SYSTEM_RAM, "Input State" },
    { 0x00FF8CU, 0x014003U, 0x00FF8CU, RC_MEMORY_TYPE_SYSTEM_RAM, "Sound RAM" },
    { 0x014004U, 0x014403U, 0x014004U, RC_MEMORY_TYPE_SAVE_RAM, "Persistent Memory" }, /* this is also returned as part of RETRO_MEMORY_SYSTEM_RAM, but can be extrapolated correctly because the pointer starts at the first SYSTEM_RAM region */
    { 0x014404U, 0x014603U, 0x014404U, RC_MEMORY_TYPE_SYSTEM_RAM, "Sprite Flags" },
    { 0x014604U, 0x014E03U, 0x014604U, RC_MEMORY_TYPE_SYSTEM_RAM, "System Font" },
    { 0x014E04U, 0x017FFFU, 0x014E04U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM"}
};
static const rc_memory_regions_t rc_memory_regions_tic80 = { _rc_memory_regions_tic80, 10 };

/* ===== Uzebox ===== */
/* https://uzebox.org/index.php */
static const rc_memory_region_t _rc_memory_regions_uzebox[] = {
    { 0x000000U, 0x000FFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_uzebox = { _rc_memory_regions_uzebox, 1 };

/* ===== Vectrex ===== */
/* https://roadsidethoughts.com/vectrex/vectrex-memory-map.htm */
static const rc_memory_region_t _rc_memory_regions_vectrex[] = {
    { 0x000000U, 0x0003FFU, 0x00C800U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_vectrex = { _rc_memory_regions_vectrex, 1 };

/* ===== Virtual Boy ===== */
static const rc_memory_region_t _rc_memory_regions_virtualboy[] = {
    { 0x000000U, 0x00FFFFU, 0x05000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x010000U, 0x01FFFFU, 0x06000000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_virtualboy = { _rc_memory_regions_virtualboy, 2 };

/* ===== Watara Supervision ===== */
/* https://github.com/libretro/potator/blob/b5e5ba02914fcdf4a8128072dbc709da28e08832/common/memorymap.c#L231-L259 */
static const rc_memory_region_t _rc_memory_regions_watara_supervision[] = {
    { 0x0000U, 0x001FFFU, 0x0000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x2000U, 0x003FFFU, 0x2000U, RC_MEMORY_TYPE_HARDWARE_CONTROLLER, "Registers" },
    { 0x4000U, 0x005FFFU, 0x4000U, RC_MEMORY_TYPE_VIDEO_RAM, "Video RAM" }
};
static const rc_memory_regions_t rc_memory_regions_watara_supervision = { _rc_memory_regions_watara_supervision, 3 };

/* ===== WASM-4 ===== */
/* fantasy console that runs specifically designed WebAssembly games */
/* https://github.com/aduros/wasm4/blob/main/site/docs/intro.md#hardware-specs */
static const rc_memory_region_t _rc_memory_regions_wasm4[] = {
    { 0x000000U, 0x00FFFFU, 0x00000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* Persistent storage is not directly accessible from the game. It has to be loaded into System RAM first
    { 0x010000U, 0x0103FFU, 0x80000000U, RC_MEMORY_TYPE_SAVE_RAM, "Disk Storage"}
    */
};
static const rc_memory_regions_t rc_memory_regions_wasm4 = { _rc_memory_regions_wasm4, 1 };

/* ===== Wii ===== */
/* https://wiibrew.org/wiki/Memory_map */
static const rc_memory_region_t _rc_memory_regions_wii[] = {
    { 0x00000000U, 0x017FFFFF, 0x80000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    { 0x01800000U, 0x0FFFFFFF, 0x81800000U, RC_MEMORY_TYPE_UNUSED, "Unused" },
    { 0x10000000U, 0x13FFFFFF, 0x90000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }
};
static const rc_memory_regions_t rc_memory_regions_wii = { _rc_memory_regions_wii, 3 };

/* ===== WonderSwan ===== */
/* http://daifukkat.su/docs/wsman/#ovr_memmap */
static const rc_memory_region_t _rc_memory_regions_wonderswan[] = {
    /* RAM ends at 0x3FFF for WonderSwan, WonderSwan color uses all 64KB */
    { 0x000000U, 0x00FFFFU, 0x000000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" },
    /* Only 64KB of SRAM is accessible via the addressing scheme, but the cartridge
     * may have up to 512KB of SRAM. http://daifukkat.su/docs/wsman/#cart_meta
     * Since beetle_wswan exposes it as a contiguous block, assume its contiguous
     * even though the documentation says $20000-$FFFFF is ROM data. If this causes
     * a conflict in the future, we can revisit. A new region with a virtual address
     * could be added to pick up the additional SRAM data. As long as it immediately
     * follows the 64KB at $10000, all existing achievements should be unaffected.
     */
    { 0x010000U, 0x08FFFFU, 0x010000U, RC_MEMORY_TYPE_SAVE_RAM, "Cartridge RAM" }
};
static const rc_memory_regions_t rc_memory_regions_wonderswan = { _rc_memory_regions_wonderswan, 2 };

/* ===== ZX Spectrum ===== */
/* https://github.com/TASEmulators/BizHawk/blob/3a3b22c/src/BizHawk.Emulation.Cores/Computers/SinclairSpectrum/Machine/ZXSpectrum16K/ZX16.cs
 * https://github.com/TASEmulators/BizHawk/blob/3a3b22c/src/BizHawk.Emulation.Cores/Computers/SinclairSpectrum/Machine/ZXSpectrum48K/ZX48.Memory.cs
 * https://worldofspectrum.org/faq/reference/128kreference.htm */
static const rc_memory_region_t _rc_memory_regions_zx_spectrum[] = {
    /* ZX Spectrum is complicated as multiple models exist with varying amounts of memory.
     * In practice, this can be reduced to two categories: 16K/48K units, and 128K units.
     * 16K/48K units have RAM starting at $4000 onwards, 16K ending at $7FFF, 48K ending at $FFFF.
     * 128K units have banked memory, with $4000-$7FFF normally having RAM bank 5, and $8000-$BFFF normally having RAM bank 2.
     * $C000-$FFFF is normally reserved for banked RAM, having any of banks 0-7.
     * For the purposes of the RAM map, $C000-$FFFF is assumed to be bank 0, and $10000 onwards has the other banks in order (1, 3, 4, 6, 7)
     * Doing it this way always for 16K/48K games to have the same memory map on the 128K, and thus avoid issues due to the model selected.
     * Later 128K units also have a special banking mode that changes up banking completely, but for 16K/48K compatibility purposes this doesn't matter, and so is irrelevant.
     */
    { 0x00000U, 0x03FFFU, 0x04000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Screen RAM" }, /* RAM bank 5 on 128K units */
    { 0x04000U, 0x07FFFU, 0x08000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 2 on 128K units */
    { 0x08000U, 0x0BFFFU, 0x0C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 0-7 on 128K units, assumed to be bank 0 here */
    { 0x0C000U, 0x0FFFFU, 0x10000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 1 on 128K units */
    { 0x10000U, 0x13FFFU, 0x14000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 3 on 128K units */
    { 0x14000U, 0x17FFFU, 0x18000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 4 on 128K units */
    { 0x18000U, 0x1BFFFU, 0x1C000U, RC_MEMORY_TYPE_SYSTEM_RAM, "System RAM" }, /* RAM bank 6 on 128K units */
    { 0x1C000U, 0x1FFFFU, 0x20000U, RC_MEMORY_TYPE_SYSTEM_RAM, "Screen RAM" } /* RAM bank 7 on 128K units */
};
static const rc_memory_regions_t rc_memory_regions_zx_spectrum = { _rc_memory_regions_zx_spectrum, 8 };

/* ===== default ===== */
static const rc_memory_regions_t rc_memory_regions_none = { 0, 0 };

const rc_memory_regions_t* rc_console_memory_regions(uint32_t console_id)
{
  switch (console_id)
  {
    case RC_CONSOLE_3DO:
      return &rc_memory_regions_3do;

    case RC_CONSOLE_AMIGA:
      return &rc_memory_regions_amiga;

    case RC_CONSOLE_AMSTRAD_PC:
      return &rc_memory_regions_amstrad_pc;

    case RC_CONSOLE_APPLE_II:
      return &rc_memory_regions_appleii;

    case RC_CONSOLE_ARCADIA_2001:
      return &rc_memory_regions_arcadia_2001;

    case RC_CONSOLE_ARDUBOY:
      return &rc_memory_regions_arduboy;

    case RC_CONSOLE_ATARI_2600:
      return &rc_memory_regions_atari2600;

    case RC_CONSOLE_ATARI_7800:
      return &rc_memory_regions_atari7800;

    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return &rc_memory_regions_atari_jaguar;

    case RC_CONSOLE_ATARI_LYNX:
      return &rc_memory_regions_atari_lynx;

    case RC_CONSOLE_COLECOVISION:
      return &rc_memory_regions_colecovision;

    case RC_CONSOLE_COMMODORE_64:
      return &rc_memory_regions_c64;

    case RC_CONSOLE_DREAMCAST:
      return &rc_memory_regions_dreamcast;

    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
      return &rc_memory_regions_elektor_tv_games;

    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
      return &rc_memory_regions_fairchild_channel_f;

    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
      return &rc_memory_regions_famicom_disk_system;

    case RC_CONSOLE_GAMEBOY:
      return &rc_memory_regions_gameboy;

    case RC_CONSOLE_GAMEBOY_COLOR:
      return &rc_memory_regions_gameboy_color;

    case RC_CONSOLE_GAMEBOY_ADVANCE:
      return &rc_memory_regions_gameboy_advance;

    case RC_CONSOLE_GAMECUBE:
      return &rc_memory_regions_gamecube;

    case RC_CONSOLE_GAME_GEAR:
      return &rc_memory_regions_game_gear;

    case RC_CONSOLE_INTELLIVISION:
      return &rc_memory_regions_intellivision;

    case RC_CONSOLE_INTERTON_VC_4000:
      return &rc_memory_regions_interton_vc_4000;

    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
      return &rc_memory_regions_magnavox_odyssey_2;

    case RC_CONSOLE_MASTER_SYSTEM:
      return &rc_memory_regions_master_system;

    case RC_CONSOLE_MEGA_DRIVE:
      return &rc_memory_regions_megadrive;

    case RC_CONSOLE_MEGADUCK:
      return &rc_memory_regions_megaduck;
  
    case RC_CONSOLE_SEGA_32X:
      return &rc_memory_regions_megadrive_32x;

    case RC_CONSOLE_MSX:
      return &rc_memory_regions_msx;

    case RC_CONSOLE_MS_DOS:
      return &rc_memory_regions_ms_dos;

    case RC_CONSOLE_NEOGEO_POCKET:
      return &rc_memory_regions_neo_geo_pocket;

    case RC_CONSOLE_NEO_GEO_CD:
      return &rc_memory_regions_neo_geo_cd;

    case RC_CONSOLE_NINTENDO:
      return &rc_memory_regions_nes;

    case RC_CONSOLE_NINTENDO_64:
      return &rc_memory_regions_n64;

    case RC_CONSOLE_NINTENDO_DS:
      return &rc_memory_regions_nintendo_ds;

    case RC_CONSOLE_NINTENDO_DSI:
      return &rc_memory_regions_nintendo_dsi;

    case RC_CONSOLE_ORIC:
      return &rc_memory_regions_oric;

    case RC_CONSOLE_PC8800:
      return &rc_memory_regions_pc8800;

    case RC_CONSOLE_PC_ENGINE:
      return &rc_memory_regions_pc_engine;

    case RC_CONSOLE_PC_ENGINE_CD:
      return &rc_memory_regions_pc_engine_cd;

    case RC_CONSOLE_PCFX:
      return &rc_memory_regions_pcfx;

    case RC_CONSOLE_PLAYSTATION:
      return &rc_memory_regions_playstation;

    case RC_CONSOLE_PLAYSTATION_2:
      return &rc_memory_regions_playstation2;

    case RC_CONSOLE_PSP:
      return &rc_memory_regions_psp;

    case RC_CONSOLE_POKEMON_MINI:
      return &rc_memory_regions_pokemini;

    case RC_CONSOLE_SATURN:
      return &rc_memory_regions_saturn;

    case RC_CONSOLE_SEGA_CD:
      return &rc_memory_regions_segacd;

    case RC_CONSOLE_SG1000:
      return &rc_memory_regions_sg1000;

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return &rc_memory_regions_scv;

    case RC_CONSOLE_SUPER_NINTENDO:
      return &rc_memory_regions_snes;

    case RC_CONSOLE_SUPERVISION:
      return &rc_memory_regions_watara_supervision;

    case RC_CONSOLE_THOMSONTO8:
      return &rc_memory_regions_thomson_to8;

    case RC_CONSOLE_TI83:
      return &rc_memory_regions_ti83;

    case RC_CONSOLE_TIC80:
      return &rc_memory_regions_tic80;

    case RC_CONSOLE_UZEBOX:
      return &rc_memory_regions_uzebox;

    case RC_CONSOLE_VECTREX:
      return &rc_memory_regions_vectrex;

    case RC_CONSOLE_VIRTUAL_BOY:
      return &rc_memory_regions_virtualboy;

    case RC_CONSOLE_WASM4:
      return &rc_memory_regions_wasm4;

    case RC_CONSOLE_WII:
      return &rc_memory_regions_wii;

    case RC_CONSOLE_WONDERSWAN:
      return &rc_memory_regions_wonderswan;

    case RC_CONSOLE_ZX_SPECTRUM:
      return &rc_memory_regions_zx_spectrum;

    default:
      return &rc_memory_regions_none;
  }
}
