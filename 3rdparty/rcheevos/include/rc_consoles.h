#ifndef RC_CONSOLES_H
#define RC_CONSOLES_H

#include "rc_export.h"

#include <stdint.h>

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Console identifiers                                                         |
\*****************************************************************************/

enum {
  RC_CONSOLE_UNKNOWN = 0,
  RC_CONSOLE_MEGA_DRIVE = 1,
  RC_CONSOLE_NINTENDO_64 = 2,
  RC_CONSOLE_SUPER_NINTENDO = 3,
  RC_CONSOLE_GAMEBOY = 4,
  RC_CONSOLE_GAMEBOY_ADVANCE = 5,
  RC_CONSOLE_GAMEBOY_COLOR = 6,
  RC_CONSOLE_NINTENDO = 7,
  RC_CONSOLE_PC_ENGINE = 8,
  RC_CONSOLE_SEGA_CD = 9,
  RC_CONSOLE_SEGA_32X = 10,
  RC_CONSOLE_MASTER_SYSTEM = 11,
  RC_CONSOLE_PLAYSTATION = 12,
  RC_CONSOLE_ATARI_LYNX = 13,
  RC_CONSOLE_NEOGEO_POCKET = 14,
  RC_CONSOLE_GAME_GEAR = 15,
  RC_CONSOLE_GAMECUBE = 16,
  RC_CONSOLE_ATARI_JAGUAR = 17,
  RC_CONSOLE_NINTENDO_DS = 18,
  RC_CONSOLE_WII = 19,
  RC_CONSOLE_WII_U = 20,
  RC_CONSOLE_PLAYSTATION_2 = 21,
  RC_CONSOLE_XBOX = 22,
  RC_CONSOLE_MAGNAVOX_ODYSSEY2 = 23,
  RC_CONSOLE_POKEMON_MINI = 24,
  RC_CONSOLE_ATARI_2600 = 25,
  RC_CONSOLE_MS_DOS = 26,
  RC_CONSOLE_ARCADE = 27,
  RC_CONSOLE_VIRTUAL_BOY = 28,
  RC_CONSOLE_MSX = 29,
  RC_CONSOLE_COMMODORE_64 = 30,
  RC_CONSOLE_ZX81 = 31,
  RC_CONSOLE_ORIC = 32,
  RC_CONSOLE_SG1000 = 33,
  RC_CONSOLE_VIC20 = 34,
  RC_CONSOLE_AMIGA = 35,
  RC_CONSOLE_ATARI_ST = 36,
  RC_CONSOLE_AMSTRAD_PC = 37,
  RC_CONSOLE_APPLE_II = 38,
  RC_CONSOLE_SATURN = 39,
  RC_CONSOLE_DREAMCAST = 40,
  RC_CONSOLE_PSP = 41,
  RC_CONSOLE_CDI = 42,
  RC_CONSOLE_3DO = 43,
  RC_CONSOLE_COLECOVISION = 44,
  RC_CONSOLE_INTELLIVISION = 45,
  RC_CONSOLE_VECTREX = 46,
  RC_CONSOLE_PC8800 = 47,
  RC_CONSOLE_PC9800 = 48,
  RC_CONSOLE_PCFX = 49,
  RC_CONSOLE_ATARI_5200 = 50,
  RC_CONSOLE_ATARI_7800 = 51,
  RC_CONSOLE_X68K = 52,
  RC_CONSOLE_WONDERSWAN = 53,
  RC_CONSOLE_CASSETTEVISION = 54,
  RC_CONSOLE_SUPER_CASSETTEVISION = 55,
  RC_CONSOLE_NEO_GEO_CD = 56,
  RC_CONSOLE_FAIRCHILD_CHANNEL_F = 57,
  RC_CONSOLE_FM_TOWNS = 58,
  RC_CONSOLE_ZX_SPECTRUM = 59,
  RC_CONSOLE_GAME_AND_WATCH = 60,
  RC_CONSOLE_NOKIA_NGAGE = 61,
  RC_CONSOLE_NINTENDO_3DS = 62,
  RC_CONSOLE_SUPERVISION = 63,
  RC_CONSOLE_SHARPX1 = 64,
  RC_CONSOLE_TIC80 = 65,
  RC_CONSOLE_THOMSONTO8 = 66,
  RC_CONSOLE_PC6000 = 67,
  RC_CONSOLE_PICO = 68,
  RC_CONSOLE_MEGADUCK = 69,
  RC_CONSOLE_ZEEBO = 70,
  RC_CONSOLE_ARDUBOY = 71,
  RC_CONSOLE_WASM4 = 72,
  RC_CONSOLE_ARCADIA_2001 = 73,
  RC_CONSOLE_INTERTON_VC_4000 = 74,
  RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER = 75,
  RC_CONSOLE_PC_ENGINE_CD = 76,
  RC_CONSOLE_ATARI_JAGUAR_CD = 77,
  RC_CONSOLE_NINTENDO_DSI = 78,
  RC_CONSOLE_TI83 = 79,
  RC_CONSOLE_UZEBOX = 80,

  RC_CONSOLE_HUBS = 100,
  RC_CONSOLE_EVENTS = 101,
  RC_CONSOLE_STANDALONE = 102
};

RC_EXPORT const char* RC_CCONV rc_console_name(uint32_t console_id);

/*****************************************************************************\
| Memory mapping                                                              |
\*****************************************************************************/

enum {
  RC_MEMORY_TYPE_SYSTEM_RAM,          /* normal system memory */
  RC_MEMORY_TYPE_SAVE_RAM,            /* memory that persists between sessions */
  RC_MEMORY_TYPE_VIDEO_RAM,           /* memory reserved for graphical processing */
  RC_MEMORY_TYPE_READONLY,            /* memory that maps to read only data */
  RC_MEMORY_TYPE_HARDWARE_CONTROLLER, /* memory for interacting with system components */
  RC_MEMORY_TYPE_VIRTUAL_RAM,         /* secondary address space that maps to real memory in system RAM */
  RC_MEMORY_TYPE_UNUSED               /* these addresses don't really exist */
};

typedef struct rc_memory_region_t {
  uint32_t start_address;             /* first address of block as queried by RetroAchievements */
  uint32_t end_address;               /* last address of block as queried by RetroAchievements */
  uint32_t real_address;              /* real address for first address of block */
  uint8_t type;                       /* RC_MEMORY_TYPE_ for block */
  const char* description;            /* short description of block */
}
rc_memory_region_t;

typedef struct rc_memory_regions_t {
  const rc_memory_region_t* region;
  uint32_t num_regions;
}
rc_memory_regions_t;

RC_EXPORT const rc_memory_regions_t* RC_CCONV rc_console_memory_regions(uint32_t console_id);

RC_END_C_DECLS

#endif /* RC_CONSOLES_H */
