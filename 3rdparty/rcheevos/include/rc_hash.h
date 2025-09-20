#ifndef RC_HASH_H
#define RC_HASH_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "rc_consoles.h"

RC_BEGIN_C_DECLS

  struct rc_hash_iterator;

  /* ===================================================== */

  typedef void (RC_CCONV *rc_hash_message_callback_deprecated)(const char*);

  /* specifies a function to call when an error occurs to display the error message */
  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_error_message_callback(rc_hash_message_callback_deprecated callback);

  /* specifies a function to call for verbose logging */
  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void rc_hash_init_verbose_message_callback(rc_hash_message_callback_deprecated callback);

  /* ===================================================== */

  /* opens a file */
  typedef void* (RC_CCONV *rc_hash_filereader_open_file_handler)(const char* path_utf8);

  /* moves the file pointer - standard fseek parameters */
  typedef void (RC_CCONV *rc_hash_filereader_seek_handler)(void* file_handle, int64_t offset, int origin);

  /* locates the file pointer */
  typedef int64_t (RC_CCONV *rc_hash_filereader_tell_handler)(void* file_handle);

  /* reads the specified number of bytes from the file starting at the read pointer.
   * returns the number of bytes actually read.
   */
  typedef size_t (RC_CCONV *rc_hash_filereader_read_handler)(void* file_handle, void* buffer, size_t requested_bytes);

  /* closes the file */
  typedef void (RC_CCONV *rc_hash_filereader_close_file_handler)(void* file_handle);

  typedef struct rc_hash_filereader
  {
    rc_hash_filereader_open_file_handler      open;
    rc_hash_filereader_seek_handler           seek;
    rc_hash_filereader_tell_handler           tell;
    rc_hash_filereader_read_handler           read;
    rc_hash_filereader_close_file_handler     close;
  } rc_hash_filereader_t;

  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_custom_filereader(struct rc_hash_filereader* reader);

  /* ===================================================== */

#ifndef RC_HASH_NO_DISC

  #define RC_HASH_CDTRACK_FIRST_DATA ((uint32_t)-1) /* the first data track (skip audio tracks) */
  #define RC_HASH_CDTRACK_LAST ((uint32_t)-2) /* the last data/audio track */
  #define RC_HASH_CDTRACK_LARGEST ((uint32_t)-3) /* the largest data/audio track */
  #define RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION ((uint32_t)-4) /* the first data/audio track of the second session */

  /* opens a track from the specified file. see the RC_HASH_CDTRACK_ defines for special tracks.
   * returns a handle to be passed to the other functions, or NULL if the track could not be opened.
   */
  typedef void* (RC_CCONV *rc_hash_cdreader_open_track_handler)(const char* path, uint32_t track);
  typedef void* (RC_CCONV* rc_hash_cdreader_open_track_iterator_handler)(const char* path, uint32_t track, const struct rc_hash_iterator* iterator);

  /* attempts to read the specified number of bytes from the file starting at the specified absolute sector.
   * returns the number of bytes actually read.
   */
  typedef size_t (RC_CCONV *rc_hash_cdreader_read_sector_handler)(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes);

  /* closes the track handle */
  typedef void (RC_CCONV *rc_hash_cdreader_close_track_handler)(void* track_handle);

  /* gets the absolute sector index for the first sector of a track */
  typedef uint32_t(RC_CCONV *rc_hash_cdreader_first_track_sector_handler)(void* track_handle);

  typedef struct rc_hash_cdreader
  {
    rc_hash_cdreader_open_track_handler              open_track;
    rc_hash_cdreader_read_sector_handler             read_sector;
    rc_hash_cdreader_close_track_handler             close_track;
    rc_hash_cdreader_first_track_sector_handler      first_track_sector;
    rc_hash_cdreader_open_track_iterator_handler     open_track_iterator;
  } rc_hash_cdreader_t;

  RC_EXPORT void RC_CCONV rc_hash_get_default_cdreader(struct rc_hash_cdreader* cdreader);
  /* [deprecated] don't set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_default_cdreader(void);
  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader);

#endif /* RC_HASH_NO_DISC */

#ifndef RC_HASH_NO_ENCRYPTED

  /* specifies a function called to obtain a 3DS CIA decryption normal key.
   * this key would be derived from slot0x3DKeyX and the common key specified by the passed index.
   * the normal key should be written in big endian format
   * returns non-zero on success, or zero on failure.
   */
  typedef int (RC_CCONV *rc_hash_3ds_get_cia_normal_key_func)(uint8_t common_key_index, uint8_t out_normal_key[16]);
  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_3ds_get_cia_normal_key_func(rc_hash_3ds_get_cia_normal_key_func func);

  /* specifies a function called to obtain 3DS NCCH decryption normal keys.
   * the primary key will always use slot0x2CKeyX and the passed primary KeyY.
   * the secondary key will use the KeyX slot passed
   * the secondary KeyY will be identical to the primary keyY if the passed program id is NULL
   * if the program id is not null, then the secondary KeyY will be obtained with "seed crypto"
   * with "seed crypto" the 8 byte program id can be used to obtain a 16 byte "seed" within the seeddb.bin firmware file
   * the primary KeyY then the seed will then be hashed with SHA256, and the upper 16 bytes of the digest will be the secondary KeyY used
   * the normal keys should be written in big endian format
   * returns non-zero on success, or zero on failure.
   */
  typedef int (RC_CCONV *rc_hash_3ds_get_ncch_normal_keys_func)(uint8_t primary_key_y[16], uint8_t secondary_key_x_slot, uint8_t* optional_program_id,
                                                                uint8_t out_primary_key[16], uint8_t out_secondary_key[16]);
  /* [deprecated] set callbacks in rc_hash_iterator_t */
  RC_EXPORT void RC_CCONV rc_hash_init_3ds_get_ncch_normal_keys_func(rc_hash_3ds_get_ncch_normal_keys_func func);

#endif /* RC_HASH_NO_ENCRYPTED */

/* ===================================================== */

typedef void (RC_CCONV* rc_hash_message_callback_func)(const char*, const struct rc_hash_iterator* iterator);

typedef struct rc_hash_callbacks {
  rc_hash_message_callback_func verbose_message;
  rc_hash_message_callback_func error_message;

  rc_hash_filereader_t filereader;
#ifndef RC_HASH_NO_DISC
  rc_hash_cdreader_t cdreader;
#endif

#ifndef RC_HASH_NO_ENCRYPTED
  struct rc_hash_encryption_callbacks {
    rc_hash_3ds_get_cia_normal_key_func get_3ds_cia_normal_key;
    rc_hash_3ds_get_ncch_normal_keys_func get_3ds_ncch_normal_keys;
  } encryption;
#endif
} rc_hash_callbacks_t;

/* data for rc_hash_iterate
 */
typedef struct rc_hash_iterator {
  const uint8_t* buffer;
  size_t buffer_size;
  uint8_t consoles[12];
  int index;
  const char* path;
  void* userdata;

  rc_hash_callbacks_t callbacks;
} rc_hash_iterator_t;

/* initializes a rc_hash_iterator
 * - path must be provided
 * - if buffer and buffer_size are provided, path may be a filename (i.e. for something extracted from a zip file)
 */
RC_EXPORT void RC_CCONV rc_hash_initialize_iterator(rc_hash_iterator_t* iterator, const char* path, const uint8_t* buffer, size_t buffer_size);

/* releases resources associated to a rc_hash_iterator
 */
RC_EXPORT void RC_CCONV rc_hash_destroy_iterator(rc_hash_iterator_t* iterator);

/* generates the next hash for the data in the rc_hash_iterator.
 * returns non-zero if a hash was generated, or zero if no more hashes can be generated for the data.
 */
RC_EXPORT int RC_CCONV rc_hash_iterate(char hash[33], rc_hash_iterator_t* iterator);

/* generates a hash for the data in the rc_hash_iterator.
 * returns non-zero if a hash was generated.
 */
RC_EXPORT int RC_CCONV rc_hash_generate(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);

/* ===================================================== */

/* generates a hash from a block of memory.
 * returns non-zero on success, or zero on failure.
 */
/* [deprecated] use rc_hash_generate instead */
RC_EXPORT int RC_CCONV rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size);

/* generates a hash from a file.
 * returns non-zero on success, or zero on failure.
 */
/* [deprecated] use rc_hash_generate instead */
RC_EXPORT int RC_CCONV rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path);

/* ===================================================== */

RC_END_C_DECLS

#endif /* RC_HASH_H */
