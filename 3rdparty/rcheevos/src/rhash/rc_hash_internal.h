#ifndef RC_HASH_INTERNAL_H
#define RC_HASH_INTERNAL_H

#include "rc_hash.h"
#include "md5.h"

RC_BEGIN_C_DECLS

/* hash.c */

void* rc_file_open(const rc_hash_iterator_t* iterator, const char* path);
void rc_file_seek(const rc_hash_iterator_t* iterator, void* file_handle, int64_t offset, int origin);
int64_t rc_file_tell(const rc_hash_iterator_t* iterator, void* file_handle);
size_t rc_file_read(const rc_hash_iterator_t* iterator, void* file_handle, void* buffer, int requested_bytes);
void rc_file_close(const rc_hash_iterator_t* iterator, void* file_handle);
int64_t rc_file_size(const rc_hash_iterator_t* iterator, const char* path);


void rc_hash_iterator_verbose(const rc_hash_iterator_t* iterator, const char* message);
void rc_hash_iterator_verbose_formatted(const rc_hash_iterator_t* iterator, const char* format, ...);
int rc_hash_iterator_error(const rc_hash_iterator_t* iterator, const char* message);
int rc_hash_iterator_error_formatted(const rc_hash_iterator_t* iterator, const char* format, ...);


/* arbitrary limit to prevent allocating and hashing large files */
#define MAX_BUFFER_SIZE 64 * 1024 * 1024

void rc_hash_merge_callbacks(rc_hash_iterator_t* iterator, const rc_hash_callbacks_t* callbacks);
int rc_hash_finalize(const rc_hash_iterator_t* iterator, md5_state_t* md5, char hash[33]);

int rc_hash_buffer(char hash[33], const uint8_t* buffer, size_t buffer_size, const rc_hash_iterator_t* iterator);
void rc_hash_byteswap16(uint8_t* buffer, const uint8_t* stop);
void rc_hash_byteswap32(uint8_t* buffer, const uint8_t* stop);


const char* rc_path_get_filename(const char* path);
const char* rc_path_get_extension(const char* path);
int rc_path_compare_extension(const char* path, const char* ext);


typedef void (RC_CCONV* rc_hash_iterator_ext_handler_t)(rc_hash_iterator_t* iterator, int data);
typedef struct rc_hash_iterator_ext_handler_entry_t {
  char ext[8];
  rc_hash_iterator_ext_handler_t handler;
  int data;
} rc_hash_iterator_ext_handler_entry_t;

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers);


typedef struct rc_hash_cdrom_track_t {
  void* file_handle;        /* the file handle for reading the track data */
  const rc_hash_filereader_t* file_reader; /* functions to perform raw file I/O */
  int64_t file_track_offset;/* the offset of the track data within the file */
  int sector_size;          /* the size of each sector in the track data */
  int sector_header_size;   /* the offset to the raw data within a sector block */
  int raw_data_size;        /* the amount of raw data within a sector block */
  int track_first_sector;   /* the first absolute sector associated to the track (includes pregap) */
  int track_pregap_sectors; /* the number of pregap sectors */
#ifndef NDEBUG
  uint32_t track_id;        /* the index of the track */
#endif
} rc_hash_cdrom_track_t;


int rc_hash_whole_file(char hash[33], const rc_hash_iterator_t* iterator);
int rc_hash_buffered_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);

#ifndef RC_HASH_NO_ROM
  /* hash_rom.c */
  int rc_hash_7800(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_arcade(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_arduboy(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_lynx(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_nes(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_n64(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_nintendo_ds(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_pce(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_scv(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_snes(char hash[33], const rc_hash_iterator_t* iterator);
#endif

#ifndef RC_HASH_NO_DISC
  /* hash_disc.c */
  void rc_hash_reset_iterator_disc(rc_hash_iterator_t* iterator);

  int rc_hash_3do(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_dreamcast(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_gamecube(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_jaguar_cd(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_neogeo_cd(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_pce_cd(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_pcfx_cd(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_psx(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_ps2(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_psp(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_sega_cd(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_wii(char hash[33], const rc_hash_iterator_t* iterator);
#endif

#ifndef RC_HASH_NO_ENCRYPTED
  /* hash_encrypted.c */
  void rc_hash_reset_iterator_encrypted(rc_hash_iterator_t* iterator);

  int rc_hash_nintendo_3ds(char hash[33], const rc_hash_iterator_t* iterator);
#endif

#ifndef RC_HASH_NO_ZIP
  /* hash_zip.c */
  int rc_hash_ms_dos(char hash[33], const rc_hash_iterator_t* iterator);
  int rc_hash_arduboyfx(char hash[33], const rc_hash_iterator_t* iterator);
#endif

RC_END_C_DECLS

#endif /* RC_HASH_INTERNAL_H */
