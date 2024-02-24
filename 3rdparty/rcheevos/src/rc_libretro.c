/* This file provides a series of functions for integrating RetroAchievements with libretro.
 * These functions will be called by a libretro frontend to validate certain expected behaviors
 * and simplify mapping core data to the RAIntegration DLL.
 * 
 * Originally designed to be shared between RALibretro and RetroArch, but will simplify
 * integrating with any other frontends.
 */

#include "rc_libretro.h"

#include "rc_consoles.h"
#include "rc_compat.h"

#include <ctype.h>
#include <string.h>

/* internal helper functions in hash.c */
extern void* rc_file_open(const char* path);
extern void rc_file_seek(void* file_handle, int64_t offset, int origin);
extern int64_t rc_file_tell(void* file_handle);
extern size_t rc_file_read(void* file_handle, void* buffer, int requested_bytes);
extern void rc_file_close(void* file_handle);
extern int rc_path_compare_extension(const char* path, const char* ext);
extern int rc_hash_error(const char* message);


static rc_libretro_message_callback rc_libretro_verbose_message_callback = NULL;

/* a value that starts with a comma is a CSV.
 * if it starts with an exclamation point, it's everything but the provided value.
 * if it starts with an exclamntion point followed by a comma, it's everything but the CSV values.
 * values are case-insensitive */
typedef struct rc_disallowed_core_settings_t
{
  const char* library_name;
  const rc_disallowed_setting_t* disallowed_settings;
} rc_disallowed_core_settings_t;

static const rc_disallowed_setting_t _rc_disallowed_bsnes_settings[] = {
  { "bsnes_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_cap32_settings[] = {
  { "cap32_autorun", "disabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_dolphin_settings[] = {
  { "dolphin_cheats_enabled", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_dosbox_pure_settings[] = {
  { "dosbox_pure_strict_mode", "false" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_duckstation_settings[] = {
  { "duckstation_CDROM.LoadImagePatches", "true" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ecwolf_settings[] = {
  { "ecwolf-invulnerability", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fbneo_settings[] = {
  { "fbneo-allow-patched-romsets", "enabled" },
  { "fbneo-cheat-*", "!,Disabled,0 - Disabled" },
  { "fbneo-dipswitch-*", "Universe BIOS*" },
  { "fbneo-neogeo-mode", "UNIBIOS" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_fceumm_settings[] = {
  { "fceumm_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_settings[] = {
  { "genesis_plus_gx_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_gpgx_wide_settings[] = {
  { "genesis_plus_gx_wide_lock_on", ",action replay (pro),game genie" },
  { "genesis_plus_gx_wide_region_detect", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_settings[] = {
  { "mesen_region", ",PAL,Dendy" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_mesen_s_settings[] = {
  { "mesen-s_region", "PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_neocd_settings[] = {
  { "neocd_bios", "uni-bios*" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_pcsx_rearmed_settings[] = {
  { "pcsx_rearmed_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_picodrive_settings[] = {
  { "picodrive_region", ",Europe,Japan PAL" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_ppsspp_settings[] = {
  { "ppsspp_cheats", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_quasi88_settings[] = {
  { "q88_cpu_clock", ",1,2" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_smsplus_settings[] = {
  { "smsplus_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_snes9x_settings[] = {
  { "snes9x_gfx_clip", "disabled" },
  { "snes9x_gfx_transp", "disabled" },
  { "snes9x_layer_*", "disabled" },
  { "snes9x_region", "pal" },
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_vice_settings[] = {
  { "vice_autostart", "disabled" }, /* autostart dictates initial load and reset from menu */
  { "vice_reset", "!autostart" }, /* reset dictates behavior when pressing reset button (END) */
  { NULL, NULL }
};

static const rc_disallowed_setting_t _rc_disallowed_virtual_jaguar_settings[] = {
  { "virtualjaguar_pal", "enabled" },
  { NULL, NULL }
};

static const rc_disallowed_core_settings_t rc_disallowed_core_settings[] = {
  { "bsnes-mercury", _rc_disallowed_bsnes_settings },
  { "cap32", _rc_disallowed_cap32_settings },
  { "dolphin-emu", _rc_disallowed_dolphin_settings },
  { "DOSBox-pure", _rc_disallowed_dosbox_pure_settings },
  { "DuckStation", _rc_disallowed_duckstation_settings },
  { "ecwolf", _rc_disallowed_ecwolf_settings },
  { "FCEUmm", _rc_disallowed_fceumm_settings },
  { "FinalBurn Neo", _rc_disallowed_fbneo_settings },
  { "Genesis Plus GX", _rc_disallowed_gpgx_settings },
  { "Genesis Plus GX Wide", _rc_disallowed_gpgx_wide_settings },
  { "Mesen", _rc_disallowed_mesen_settings },
  { "Mesen-S", _rc_disallowed_mesen_s_settings },
  { "NeoCD", _rc_disallowed_neocd_settings },
  { "PPSSPP", _rc_disallowed_ppsspp_settings },
  { "PCSX-ReARMed", _rc_disallowed_pcsx_rearmed_settings },
  { "PicoDrive", _rc_disallowed_picodrive_settings },
  { "QUASI88", _rc_disallowed_quasi88_settings },
  { "SMS Plus GX", _rc_disallowed_smsplus_settings },
  { "Snes9x", _rc_disallowed_snes9x_settings },
  { "VICE x64", _rc_disallowed_vice_settings },
  { "Virtual Jaguar", _rc_disallowed_virtual_jaguar_settings },
  { NULL, NULL }
};

static int rc_libretro_string_equal_nocase_wildcard(const char* test, const char* value) {
  char c1, c2;
  while ((c1 = *test++)) {
    if (tolower(c1) != tolower(c2 = *value++))
      return (c2 == '*');
  }

  return (*value == '\0');
}

static int rc_libretro_match_value(const char* val, const char* match) {
  /* if value starts with a comma, it's a CSV list of potential matches */
  if (*match == ',') {
    do {
      const char* ptr = ++match;
      size_t size;

      while (*match && *match != ',')
        ++match;

      size = match - ptr;
      if (val[size] == '\0') {
        if (memcmp(ptr, val, size) == 0) {
          return 1;
        }
        else {
          char buffer[128];
          memcpy(buffer, ptr, size);
          buffer[size] = '\0';
          if (rc_libretro_string_equal_nocase_wildcard(buffer, val))
            return 1;
        }
      }
    } while (*match == ',');

    return 0;
  }

  /* a leading exclamation point means the provided value(s) are not forbidden (are allowed) */
  if (*match == '!')
    return !rc_libretro_match_value(val, &match[1]);

  /* just a single value, attempt to match it */
  return rc_libretro_string_equal_nocase_wildcard(val, match);
}

int rc_libretro_is_setting_allowed(const rc_disallowed_setting_t* disallowed_settings, const char* setting, const char* value) {
  const char* key;
  size_t key_len;

  for (; disallowed_settings->setting; ++disallowed_settings) {
    key = disallowed_settings->setting;
    key_len = strlen(key);

    if (key[key_len - 1] == '*') {
      if (memcmp(setting, key, key_len - 1) == 0) {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
    else {
      if (memcmp(setting, key, key_len + 1) == 0) {
        if (rc_libretro_match_value(value, disallowed_settings->value))
          return 0;
      }
    }
  }

  return 1;
}

const rc_disallowed_setting_t* rc_libretro_get_disallowed_settings(const char* library_name) {
  const rc_disallowed_core_settings_t* core_filter = rc_disallowed_core_settings;
  size_t library_name_length;

  if (!library_name || !library_name[0])
    return NULL;

  library_name_length = strlen(library_name) + 1;
  while (core_filter->library_name) {
    if (memcmp(core_filter->library_name, library_name, library_name_length) == 0)
      return core_filter->disallowed_settings;

    ++core_filter;
  }

  return NULL;
}

typedef struct rc_disallowed_core_systems_t
{
    const char* library_name;
    const uint32_t disallowed_consoles[4];
} rc_disallowed_core_systems_t;

static const rc_disallowed_core_systems_t rc_disallowed_core_systems[] = {
  /* https://github.com/libretro/Mesen-S/issues/8 */
  { "Mesen-S", { RC_CONSOLE_GAMEBOY, RC_CONSOLE_GAMEBOY_COLOR, 0 }},
  { NULL, { 0 } }
};

int rc_libretro_is_system_allowed(const char* library_name, uint32_t console_id) {
  const rc_disallowed_core_systems_t* core_filter = rc_disallowed_core_systems;
  size_t library_name_length;
  size_t i;

  if (!library_name || !library_name[0])
    return 1;

  library_name_length = strlen(library_name) + 1;
  while (core_filter->library_name) {
    if (memcmp(core_filter->library_name, library_name, library_name_length) == 0) {
      for (i = 0; i < sizeof(core_filter->disallowed_consoles) / sizeof(core_filter->disallowed_consoles[0]); ++i) {
        if (core_filter->disallowed_consoles[i] == console_id)
          return 0;
      }
      break;
    }

    ++core_filter;
  }

  return 1;
}

uint8_t* rc_libretro_memory_find_avail(const rc_libretro_memory_regions_t* regions, uint32_t address, uint32_t* avail) {
  uint32_t i;

  for (i = 0; i < regions->count; ++i) {
    const size_t size = regions->size[i];
    if (address < size) {
      if (regions->data[i] == NULL)
        break;

      if (avail)
        *avail = (uint32_t)(size - address);

      return &regions->data[i][address];
    }

    address -= (uint32_t)size;
  }

  if (avail)
    *avail = 0;

  return NULL;
}

uint8_t* rc_libretro_memory_find(const rc_libretro_memory_regions_t* regions, uint32_t address) {
  return rc_libretro_memory_find_avail(regions, address, NULL);
}

uint32_t rc_libretro_memory_read(const rc_libretro_memory_regions_t* regions, uint32_t address,
      uint8_t* buffer, uint32_t num_bytes) {
  uint32_t bytes_read = 0;
  uint32_t avail;
  uint32_t i;

  for (i = 0; i < regions->count; ++i) {
    const size_t size = regions->size[i];
    if (address >= size) {
      /* address is not in this block, adjust and look at next block */
      address -= (unsigned)size;
      continue;
    }

    if (regions->data[i] == NULL) /* no memory associated to this block. abort */
      break;

    avail = (unsigned)(size - address);
    if (avail >= num_bytes) {
      /* requested memory is fully within this block, copy and return it */
      memcpy(buffer, &regions->data[i][address], num_bytes);
      bytes_read += num_bytes;
      return bytes_read;
    }

    /* copy whatever is available in this block, and adjust for the next block */
    memcpy(buffer, &regions->data[i][address], avail);
    buffer += avail;
    bytes_read += avail;
    num_bytes -= avail;
    address = 0;
  }

  return bytes_read;
}

void rc_libretro_init_verbose_message_callback(rc_libretro_message_callback callback) {
  rc_libretro_verbose_message_callback = callback;
}

static void rc_libretro_verbose(const char* message) {
  if (rc_libretro_verbose_message_callback)
    rc_libretro_verbose_message_callback(message);
}

static const char* rc_memory_type_str(int type) {
  switch (type)
  {
    case RC_MEMORY_TYPE_SAVE_RAM:
      return "SRAM";
    case RC_MEMORY_TYPE_VIDEO_RAM:
      return "VRAM";
    case RC_MEMORY_TYPE_UNUSED:
      return "UNUSED";
    default:
      break;
  }

  return "SYSTEM RAM";
}

static void rc_libretro_memory_register_region(rc_libretro_memory_regions_t* regions, int type,
                                               uint8_t* data, size_t size, const char* description) {
  if (size == 0)
    return;

  if (regions->count == (sizeof(regions->size) / sizeof(regions->size[0]))) {
    rc_libretro_verbose("Too many memory memory regions to register");
    return;
  }

  if (!data && regions->count > 0 && !regions->data[regions->count - 1]) {
    /* extend null region */
    regions->size[regions->count - 1] += size;
  }
  else if (data && regions->count > 0 &&
           data == (regions->data[regions->count - 1] + regions->size[regions->count - 1])) {
    /* extend non-null region */
    regions->size[regions->count - 1] += size;
  }
  else {
    /* create new region */
    regions->data[regions->count] = data;
    regions->size[regions->count] = size;
    ++regions->count;
  }

  regions->total_size += size;

  if (rc_libretro_verbose_message_callback) {
    char message[128];
    snprintf(message, sizeof(message), "Registered 0x%04X bytes of %s at $%06X (%s)", (unsigned)size,
             rc_memory_type_str(type), (unsigned)(regions->total_size - size), description);
    rc_libretro_verbose_message_callback(message);
  }
}

static void rc_libretro_memory_init_without_regions(rc_libretro_memory_regions_t* regions,
                                                    rc_libretro_get_core_memory_info_func get_core_memory_info) {
  /* no regions specified, assume system RAM followed by save RAM */
  char description[64];
  rc_libretro_core_memory_info_t info;

  snprintf(description, sizeof(description), "offset 0x%06x", 0);

  get_core_memory_info(RETRO_MEMORY_SYSTEM_RAM, &info);
  if (info.size)
    rc_libretro_memory_register_region(regions, RC_MEMORY_TYPE_SYSTEM_RAM, info.data, info.size, description);

  get_core_memory_info(RETRO_MEMORY_SAVE_RAM, &info);
  if (info.size)
    rc_libretro_memory_register_region(regions, RC_MEMORY_TYPE_SAVE_RAM, info.data, info.size, description);
}

static const struct retro_memory_descriptor* rc_libretro_memory_get_descriptor(const struct retro_memory_map* mmap, uint32_t real_address, size_t* offset)
{
  const struct retro_memory_descriptor* desc = mmap->descriptors;
  const struct retro_memory_descriptor* end = desc + mmap->num_descriptors;

  for (; desc < end; desc++) {
    if (desc->select == 0) {
      /* if select is 0, attempt to explcitly match the address */
      if (real_address >= desc->start && real_address < desc->start + desc->len) {
        *offset = real_address - desc->start;
        return desc;
      }
    }
    else {
      /* otherwise, attempt to match the address by matching the select bits */
      /* address is in the block if (addr & select) == (start & select) */
      if (((desc->start ^ real_address) & desc->select) == 0) {
        /* get the relative offset of the address from the start of the memory block */
        uint32_t reduced_address = real_address - (unsigned)desc->start;

        /* remove any bits from the reduced_address that correspond to the bits in the disconnect
         * mask and collapse the remaining bits. this code was copied from the mmap_reduce function
         * in RetroArch. i'm not exactly sure how it works, but it does. */
        uint32_t disconnect_mask = (unsigned)desc->disconnect;
        while (disconnect_mask) {
          const uint32_t tmp = (disconnect_mask - 1) & ~disconnect_mask;
          reduced_address = (reduced_address & tmp) | ((reduced_address >> 1) & ~tmp);
          disconnect_mask = (disconnect_mask & (disconnect_mask - 1)) >> 1;
        }

        /* calculate the offset within the descriptor */
        *offset = reduced_address;

        /* sanity check - make sure the descriptor is large enough to hold the target address */
        if (reduced_address < desc->len)
          return desc;
      }
    }
  }

  *offset = 0;
  return NULL;
}

static void rc_libretro_memory_init_from_memory_map(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                                                    const rc_memory_regions_t* console_regions) {
  char description[64];
  uint32_t i;
  uint8_t* region_start;
  uint8_t* desc_start;
  size_t desc_size;
  size_t offset;

  for (i = 0; i < console_regions->num_regions; ++i) {
    const rc_memory_region_t* console_region = &console_regions->region[i];
    size_t console_region_size = console_region->end_address - console_region->start_address + 1;
    uint32_t real_address = console_region->real_address;
    uint32_t disconnect_size = 0;

    while (console_region_size > 0) {
      const struct retro_memory_descriptor* desc = rc_libretro_memory_get_descriptor(mmap, real_address, &offset);
      if (!desc) {
        if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
          snprintf(description, sizeof(description), "Could not map region starting at $%06X",
                   (unsigned)(real_address - console_region->real_address + console_region->start_address));
          rc_libretro_verbose(description);
        }

        if (disconnect_size && console_region_size > disconnect_size) {
          rc_libretro_memory_register_region(regions, console_region->type, NULL, disconnect_size, "null filler");
          console_region_size -= disconnect_size;
          real_address += disconnect_size;
          disconnect_size = 0;
          continue;
        }

        rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
        break;
      }

      snprintf(description, sizeof(description), "descriptor %u, offset 0x%06X%s",
               (unsigned)(desc - mmap->descriptors) + 1, (int)offset, desc->ptr ? "" : " [no pointer]");

      if (desc->ptr) {
        desc_start = (uint8_t*)desc->ptr + desc->offset;
        region_start = desc_start + offset;
      }
      else {
        region_start = NULL;
      }

      desc_size = desc->len - offset;
      if (desc->disconnect && desc_size > desc->disconnect) {
        /* if we need to extract a disconnect bit, the largest block we can read is up to
         * the next time that bit flips */
        /* https://stackoverflow.com/questions/12247186/find-the-lowest-set-bit */
        disconnect_size = (desc->disconnect & -((int)desc->disconnect));
        desc_size = disconnect_size - (real_address & (disconnect_size - 1));
      }

      if (console_region_size > desc_size) {
        if (desc_size == 0) {
          if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
            snprintf(description, sizeof(description), "Could not map region starting at $%06X",
                     (unsigned)(real_address - console_region->real_address + console_region->start_address));
            rc_libretro_verbose(description);
          }

          rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size, "null filler");
          console_region_size = 0;
        }
        else {
          rc_libretro_memory_register_region(regions, console_region->type, region_start, desc_size, description);
          console_region_size -= desc_size;
          real_address += (unsigned)desc_size;
        }
      }
      else {
        rc_libretro_memory_register_region(regions, console_region->type, region_start, console_region_size, description);
        console_region_size = 0;
      }
    }
  }
}

static uint32_t rc_libretro_memory_console_region_to_ram_type(uint8_t region_type) {
  switch (region_type)
  {
    case RC_MEMORY_TYPE_SAVE_RAM:
      return RETRO_MEMORY_SAVE_RAM;
    case RC_MEMORY_TYPE_VIDEO_RAM:
      return RETRO_MEMORY_VIDEO_RAM;
    default:
      break;
  }

  return RETRO_MEMORY_SYSTEM_RAM;
}

static void rc_libretro_memory_init_from_unmapped_memory(rc_libretro_memory_regions_t* regions,
    rc_libretro_get_core_memory_info_func get_core_memory_info, const rc_memory_regions_t* console_regions) {
  char description[64];
  uint32_t i, j;
  rc_libretro_core_memory_info_t info;
  size_t offset;

  for (i = 0; i < console_regions->num_regions; ++i) {
    const rc_memory_region_t* console_region = &console_regions->region[i];
    const size_t console_region_size = console_region->end_address - console_region->start_address + 1;
    const uint32_t type = rc_libretro_memory_console_region_to_ram_type(console_region->type);
    uint32_t base_address = 0;

    for (j = 0; j <= i; ++j) {
      const rc_memory_region_t* console_region2 = &console_regions->region[j];
      if (rc_libretro_memory_console_region_to_ram_type(console_region2->type) == type) {
        base_address = console_region2->start_address;
        break;
      }
    }
    offset = console_region->start_address - base_address;

    get_core_memory_info(type, &info);

    if (offset < info.size) {
      info.size -= offset;

      if (info.data) {
        snprintf(description, sizeof(description), "offset 0x%06X", (int)offset);
        info.data += offset;
      }
      else {
        snprintf(description, sizeof(description), "null filler");
      }
    }
    else {
      if (rc_libretro_verbose_message_callback && console_region->type != RC_MEMORY_TYPE_UNUSED) {
        snprintf(description, sizeof(description), "Could not map region starting at $%06X", (unsigned)console_region->start_address);
        rc_libretro_verbose(description);
      }

      info.data = NULL;
      info.size = 0;
    }

    if (console_region_size > info.size) {
      /* want more than what is available, take what we can and null fill the rest */
      rc_libretro_memory_register_region(regions, console_region->type, info.data, info.size, description);
      rc_libretro_memory_register_region(regions, console_region->type, NULL, console_region_size - info.size, "null filler");
    }
    else {
      /* only take as much as we need */
      rc_libretro_memory_register_region(regions, console_region->type, info.data, console_region_size, description);
    }
  }
}

int rc_libretro_memory_init(rc_libretro_memory_regions_t* regions, const struct retro_memory_map* mmap,
                            rc_libretro_get_core_memory_info_func get_core_memory_info, uint32_t console_id) {
  const rc_memory_regions_t* console_regions = rc_console_memory_regions(console_id);
  rc_libretro_memory_regions_t new_regions;
  int has_valid_region = 0;
  uint32_t i;

  if (!regions)
    return 0;

  memset(&new_regions, 0, sizeof(new_regions));

  if (console_regions == NULL || console_regions->num_regions == 0)
    rc_libretro_memory_init_without_regions(&new_regions, get_core_memory_info);
  else if (mmap && mmap->num_descriptors != 0)
    rc_libretro_memory_init_from_memory_map(&new_regions, mmap, console_regions);
  else
    rc_libretro_memory_init_from_unmapped_memory(&new_regions, get_core_memory_info, console_regions);

  /* determine if any valid regions were found */
  for (i = 0; i < new_regions.count; i++) {
    if (new_regions.data[i]) {
      has_valid_region = 1;
      break;
    }
  }

  memcpy(regions, &new_regions, sizeof(*regions));
  return has_valid_region;
}

void rc_libretro_memory_destroy(rc_libretro_memory_regions_t* regions) {
  memset(regions, 0, sizeof(*regions));
}

void rc_libretro_hash_set_init(struct rc_libretro_hash_set_t* hash_set,
                               const char* m3u_path, rc_libretro_get_image_path_func get_image_path) {
  char image_path[1024];
  char* m3u_contents;
  char* ptr;
  int64_t file_len;
  void* file_handle;
  int index = 0;

  memset(hash_set, 0, sizeof(*hash_set));

  if (!rc_path_compare_extension(m3u_path, "m3u"))
    return;

  file_handle = rc_file_open(m3u_path);
  if (!file_handle) {
    rc_hash_error("Could not open playlist");
    return;
  }

  rc_file_seek(file_handle, 0, SEEK_END);
  file_len = rc_file_tell(file_handle);
  rc_file_seek(file_handle, 0, SEEK_SET);

  m3u_contents = (char*)malloc((size_t)file_len + 1);
  if (m3u_contents) {
    rc_file_read(file_handle, m3u_contents, (int)file_len);
    m3u_contents[file_len] = '\0';

    rc_file_close(file_handle);

    ptr = m3u_contents;
    do
    {
      /* ignore whitespace */
      while (isspace((int)*ptr))
        ++ptr;

      if (*ptr == '#') {
        /* ignore comment unless it's the special SAVEDISK extension */
        if (memcmp(ptr, "#SAVEDISK:", 10) == 0) {
          /* get the path to the save disk from the frontend, assign it a bogus hash so
           * it doesn't get hashed later */
          if (get_image_path(index, image_path, sizeof(image_path))) {
            const char save_disk_hash[33] = "[SAVE DISK]";
            rc_libretro_hash_set_add(hash_set, image_path, -1, save_disk_hash);
            ++index;
          }
        }
      }
      else {
        /* non-empty line, tally a file */
        ++index;
      }

      /* find the end of the line */
      while (*ptr && *ptr != '\n')
        ++ptr;

    } while (*ptr);

    free(m3u_contents);
  }

  if (hash_set->entries_count > 0) {
    /* at least one save disk was found. make sure the core supports the #SAVEDISK: extension by
     * asking for the last expected disk. if it's not found, assume no #SAVEDISK: support */
    if (!get_image_path(index - 1, image_path, sizeof(image_path)))
      hash_set->entries_count = 0;
  }
}

void rc_libretro_hash_set_destroy(struct rc_libretro_hash_set_t* hash_set) {
  if (hash_set->entries)
    free(hash_set->entries);
  memset(hash_set, 0, sizeof(*hash_set));
}

static uint32_t rc_libretro_djb2(const char* input)
{
  uint32_t result = 5381;
  char c;

  while ((c = *input++) != '\0')
    result = ((result << 5) + result) + c; /* result = result * 33 + c */

  return result;
}

void rc_libretro_hash_set_add(struct rc_libretro_hash_set_t* hash_set,
                              const char* path, uint32_t game_id, const char hash[33]) {
  const uint32_t path_djb2 = (path != NULL) ? rc_libretro_djb2(path) : 0;
  struct rc_libretro_hash_entry_t* entry = NULL;
  struct rc_libretro_hash_entry_t* scan;
  struct rc_libretro_hash_entry_t* stop = hash_set->entries + hash_set->entries_count;;

  if (path_djb2) {
    /* attempt to match the path */
    for (scan = hash_set->entries; scan < stop; ++scan) {
      if (scan->path_djb2 == path_djb2) {
        entry = scan;
        break;
      }
    }
  }

  if (!entry)
  {
    /* entry not found, allocate a new one */
    if (hash_set->entries_size == 0) {
      hash_set->entries_size = 4;
      hash_set->entries = (struct rc_libretro_hash_entry_t*)
          malloc(hash_set->entries_size * sizeof(struct rc_libretro_hash_entry_t));
    }
    else if (hash_set->entries_count == hash_set->entries_size) {
      hash_set->entries_size += 4;
      hash_set->entries = (struct rc_libretro_hash_entry_t*)realloc(hash_set->entries,
          hash_set->entries_size * sizeof(struct rc_libretro_hash_entry_t));
    }

    if (hash_set->entries == NULL) /* unexpected, but better than crashing */
      return;

    entry = hash_set->entries + hash_set->entries_count++;
  }

  /* update the entry */
  entry->path_djb2 = path_djb2;
  entry->game_id = game_id;
  memcpy(entry->hash, hash, sizeof(entry->hash));
}

const char* rc_libretro_hash_set_get_hash(const struct rc_libretro_hash_set_t* hash_set, const char* path)
{
  const uint32_t path_djb2 = rc_libretro_djb2(path);
  struct rc_libretro_hash_entry_t* scan = hash_set->entries;
  struct rc_libretro_hash_entry_t* stop = scan + hash_set->entries_count;
  for (; scan < stop; ++scan) {
    if (scan->path_djb2 == path_djb2)
      return scan->hash;
  }

  return NULL;
}

int rc_libretro_hash_set_get_game_id(const struct rc_libretro_hash_set_t* hash_set, const char* hash)
{
  struct rc_libretro_hash_entry_t* scan = hash_set->entries;
  struct rc_libretro_hash_entry_t* stop = scan + hash_set->entries_count;
  for (; scan < stop; ++scan) {
    if (memcmp(scan->hash, hash, sizeof(scan->hash)) == 0)
      return scan->game_id;
  }

  return 0;
}
