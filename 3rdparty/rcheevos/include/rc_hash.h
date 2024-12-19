#ifndef RC_HASH_H
#define RC_HASH_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "rc_consoles.h"

RC_BEGIN_C_DECLS

  /* ===================================================== */

  /* generates a hash from a block of memory.
   * returns non-zero on success, or zero on failure.
   */
  RC_EXPORT int RC_CCONV rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size);

  /* generates a hash from a file.
   * returns non-zero on success, or zero on failure.
   */
  RC_EXPORT int RC_CCONV rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path);

  /* ===================================================== */

  /* data for rc_hash_iterate
   */
  typedef struct rc_hash_iterator
  {
    const uint8_t* buffer;
    size_t buffer_size;
    uint8_t consoles[12];
    int index;
    const char* path;
  } rc_hash_iterator_t;

  /* initializes a rc_hash_iterator
   * - path must be provided
   * - if buffer and buffer_size are provided, path may be a filename (i.e. for something extracted from a zip file)
   */
  RC_EXPORT void RC_CCONV rc_hash_initialize_iterator(struct rc_hash_iterator* iterator, const char* path, const uint8_t* buffer, size_t buffer_size);

  /* releases resources associated to a rc_hash_iterator
   */
  RC_EXPORT void RC_CCONV rc_hash_destroy_iterator(struct rc_hash_iterator* iterator);

  /* generates the next hash for the data in the rc_hash_iterator.
   * returns non-zero if a hash was generated, or zero if no more hashes can be generated for the data.
   */
  RC_EXPORT int RC_CCONV rc_hash_iterate(char hash[33], struct rc_hash_iterator* iterator);

  /* ===================================================== */

  /* specifies a function to call when an error occurs to display the error message */
  typedef void (RC_CCONV *rc_hash_message_callback)(const char*);
  RC_EXPORT void RC_CCONV rc_hash_init_error_message_callback(rc_hash_message_callback callback);

  /* specifies a function to call for verbose logging */
  RC_EXPORT void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback);

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

  struct rc_hash_filereader
  {
    rc_hash_filereader_open_file_handler      open;
    rc_hash_filereader_seek_handler           seek;
    rc_hash_filereader_tell_handler           tell;
    rc_hash_filereader_read_handler           read;
    rc_hash_filereader_close_file_handler     close;
  };

  RC_EXPORT void RC_CCONV rc_hash_init_custom_filereader(struct rc_hash_filereader* reader);

  /* ===================================================== */

  #define RC_HASH_CDTRACK_FIRST_DATA ((uint32_t)-1) /* the first data track (skip audio tracks) */
  #define RC_HASH_CDTRACK_LAST ((uint32_t)-2) /* the last data/audio track */
  #define RC_HASH_CDTRACK_LARGEST ((uint32_t)-3) /* the largest data/audio track */
  #define RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION ((uint32_t)-4) /* the first data/audio track of the second session */

  /* opens a track from the specified file. see the RC_HASH_CDTRACK_ defines for special tracks.
   * returns a handle to be passed to the other functions, or NULL if the track could not be opened.
   */
  typedef void* (RC_CCONV *rc_hash_cdreader_open_track_handler)(const char* path, uint32_t track);

  /* attempts to read the specified number of bytes from the file starting at the specified absolute sector.
   * returns the number of bytes actually read.
   */
  typedef size_t (RC_CCONV *rc_hash_cdreader_read_sector_handler)(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes);

  /* closes the track handle */
  typedef void (RC_CCONV *rc_hash_cdreader_close_track_handler)(void* track_handle);

  /* gets the absolute sector index for the first sector of a track */
  typedef uint32_t(RC_CCONV *rc_hash_cdreader_first_track_sector_handler)(void* track_handle);

  struct rc_hash_cdreader
  {
    rc_hash_cdreader_open_track_handler              open_track;
    rc_hash_cdreader_read_sector_handler             read_sector;
    rc_hash_cdreader_close_track_handler             close_track;
    rc_hash_cdreader_first_track_sector_handler      first_track_sector;
  };

  RC_EXPORT void RC_CCONV rc_hash_get_default_cdreader(struct rc_hash_cdreader* cdreader);
  RC_EXPORT void RC_CCONV rc_hash_init_default_cdreader(void);
  RC_EXPORT void RC_CCONV rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader);

  /* ===================================================== */

RC_END_C_DECLS

#endif /* RC_HASH_H */
