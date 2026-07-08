#include "rc_hash.h"

#include "rc_hash_internal.h"

#include "../rc_compat.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <share.h>
#endif

#include <ctype.h>
#include <stdarg.h>

const char* rc_path_get_filename(const char* path);
static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);

/* ===================================================== */

static rc_hash_message_callback_deprecated g_error_message_callback = NULL;
static rc_hash_message_callback_deprecated g_verbose_message_callback = NULL;

static void rc_hash_call_g_error_message_callback(const char* message, const rc_hash_iterator_t* iterator)
{
  (void)iterator;
  g_error_message_callback(message);
}

static void rc_hash_call_g_verbose_message_callback(const char* message, const rc_hash_iterator_t* iterator)
{
  (void)iterator;
  g_verbose_message_callback(message);
}

static void rc_hash_dispatch_message_va(const rc_hash_message_callback_func callback,
  const rc_hash_iterator_t* iterator, const char* format, va_list args)
{
  char buffer[1024];

#ifdef __STDC_SECURE_LIB__
  vsprintf_s(buffer, sizeof(buffer), format, args);
#elif __STDC_VERSION__ >= 199901L /* vsnprintf requires c99 */
  vsnprintf(buffer, sizeof(buffer), format, args);
#else /* c89 doesn't have a size-limited vsprintf function - assume the buffer is large enough */
  vsprintf(buffer, format, args);
#endif

  callback(buffer, iterator);
}

void rc_hash_init_error_message_callback(rc_hash_message_callback_deprecated callback)
{
  g_error_message_callback = callback;
}

static rc_hash_message_callback_func rc_hash_get_error_message_callback(const rc_hash_callbacks_t* callbacks)
{
  if (callbacks && callbacks->error_message)
    return callbacks->error_message;

  if (g_error_message_callback)
    return rc_hash_call_g_error_message_callback;

  if (callbacks && callbacks->verbose_message)
    return callbacks->verbose_message;

  if (g_verbose_message_callback)
    return rc_hash_call_g_verbose_message_callback;

  return NULL;
}

int rc_hash_iterator_error(const rc_hash_iterator_t* iterator, const char* message)
{
  rc_hash_message_callback_func message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback)
    message_callback(message, iterator);

  return 0;
}

int rc_hash_iterator_error_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  rc_hash_message_callback_func message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(message_callback, iterator, format, args);
    va_end(args);
  }

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback_deprecated callback)
{
  g_verbose_message_callback = callback;
}

void rc_hash_iterator_verbose(const rc_hash_iterator_t* iterator, const char* message)
{
  if (iterator->callbacks.verbose_message)
    iterator->callbacks.verbose_message(message, iterator);
  else if (g_verbose_message_callback)
    g_verbose_message_callback(message);
}

void rc_hash_iterator_verbose_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  if (iterator->callbacks.verbose_message) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(iterator->callbacks.verbose_message, iterator, format, args);
    va_end(args);
  }
  else if (g_verbose_message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(rc_hash_call_g_verbose_message_callback, iterator, format, args);
    va_end(args);
  }
}

/* ===================================================== */

static struct rc_hash_filereader g_filereader_funcs;
static struct rc_hash_filereader* g_filereader = NULL;

#if defined(WINVER) && WINVER >= 0x0500
static void* filereader_open(const char* path)
{
  /* Windows requires using wchar APIs for Unicode paths */
  /* Note that MultiByteToWideChar will only be defined for >= Windows 2000 */
  wchar_t* wpath;
  int wpath_length;
  FILE* fp;

  /* Calculate wpath length from path */
  wpath_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (wpath_length == 0) /* 0 indicates error (this is likely from invalid UTF-8) */
    return NULL;

  wpath = (wchar_t*)malloc(wpath_length * sizeof(wchar_t));
  if (!wpath)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_length) == 0) {
    free(wpath);
    return NULL;
  }

 #if defined(__STDC_SECURE_LIB__)
  /* have to use _SH_DENYNO because some cores lock the file while its loaded */
  fp = _wfsopen(wpath, L"rb", _SH_DENYNO);
 #else
  fp = _wfopen(wpath, L"rb");
 #endif

  free(wpath);
  return fp;
}
#else /* !WINVER >= 0x0500 */
static void* filereader_open(const char* path)
{
 #if defined(__STDC_SECURE_LIB__)
  #if defined(WINVER)
   /* have to use _SH_DENYNO because some cores lock the file while its loaded */
   return _fsopen(path, "rb", _SH_DENYNO);
  #else /* !WINVER */
   FILE *fp;
   fopen_s(&fp, path, "rb");
   return fp;
  #endif
 #else /* !__STDC_SECURE_LIB__ */
  return fopen(path, "rb");
 #endif
}
#endif /* WINVER >= 0x0500 */

static void filereader_seek(void* file_handle, int64_t offset, int origin)
{
#if defined(_WIN32)
  _fseeki64((FILE*)file_handle, offset, origin);
#elif defined(_LARGEFILE64_SOURCE)
  fseeko64((FILE*)file_handle, offset, origin);
#else
  fseek((FILE*)file_handle, offset, origin);
#endif
}

static int64_t filereader_tell(void* file_handle)
{
#if defined(_WIN32)
  return _ftelli64((FILE*)file_handle);
#elif defined(_LARGEFILE64_SOURCE)
  return ftello64((FILE*)file_handle);
#else
  return ftell((FILE*)file_handle);
#endif
}

static size_t filereader_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void filereader_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

/* for unit tests - normally would call rc_hash_init_custom_filereader(NULL) */
void rc_hash_reset_filereader(void)
{
  g_filereader = NULL;
}

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  /* initialize with defaults first */
  g_filereader_funcs.open = filereader_open;
  g_filereader_funcs.seek = filereader_seek;
  g_filereader_funcs.tell = filereader_tell;
  g_filereader_funcs.read = filereader_read;
  g_filereader_funcs.close = filereader_close;

  /* hook up any provided custom handlers */
  if (reader) {
    if (reader->open)
      g_filereader_funcs.open = reader->open;

    if (reader->seek)
      g_filereader_funcs.seek = reader->seek;

    if (reader->tell)
      g_filereader_funcs.tell = reader->tell;

    if (reader->read)
      g_filereader_funcs.read = reader->read;

    if (reader->close)
      g_filereader_funcs.close = reader->close;
  }

  g_filereader = &g_filereader_funcs;
}

void* rc_file_open(const rc_hash_iterator_t* iterator, const char* path)
{
  void* handle = NULL;

  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    handle = iterator->callbacks.filereader.open(path);
    if (handle)
      rc_hash_iterator_verbose_formatted(iterator, "Opened %s", rc_path_get_filename(path));
  }

  return handle;
}

void rc_file_seek(const rc_hash_iterator_t* iterator, void* file_handle, int64_t offset, int origin)
{
  if (iterator->callbacks.filereader.seek)
    iterator->callbacks.filereader.seek(file_handle, offset, origin);
}

int64_t rc_file_tell(const rc_hash_iterator_t* iterator, void* file_handle)
{
  return iterator->callbacks.filereader.tell ? iterator->callbacks.filereader.tell(file_handle) : 0;
}

size_t rc_file_read(const rc_hash_iterator_t* iterator, void* file_handle, void* buffer, int requested_bytes)
{
  return iterator->callbacks.filereader.read ? iterator->callbacks.filereader.read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(const rc_hash_iterator_t* iterator, void* file_handle)
{
  if (iterator->callbacks.filereader.close)
    iterator->callbacks.filereader.close(file_handle);
}

int64_t rc_file_size(const rc_hash_iterator_t* iterator, const char* path)
{
  int64_t size = 0;

  /* don't use rc_file_open to avoid log statements */
  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    void* handle = iterator->callbacks.filereader.open(path);
    if (handle) {
      rc_file_seek(iterator, handle, 0, SEEK_END);
      size = rc_file_tell(iterator, handle);
      rc_file_close(iterator, handle);
    }
  }

  return size;
}

/* ===================================================== */

const char* rc_path_get_filename(const char* path)
{
  const char* ptr = path + strlen(path);
  do {
    if (ptr[-1] == '/' || ptr[-1] == '\\')
      break;

    --ptr;
  } while (ptr > path);

  return ptr;
}

const char* rc_path_get_extension(const char* path)
{
  const char* ptr = path + strlen(path);
  do {
    if (ptr[-1] == '.')
      return ptr;

    --ptr;
  } while (ptr > path);

  return path + strlen(path);
}

int rc_path_compare_extension(const char* path, const char* ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);
  const char* ptr = path + path_len - ext_len;
  if (ptr[-1] != '.')
    return 0;

  if (memcmp(ptr, ext, ext_len) == 0)
    return 1;

  do {
    if (tolower(*ptr) != *ext)
      return 0;

    ++ext;
    ++ptr;
  } while (*ptr);

  return 1;
}

/* ===================================================== */

void rc_hash_byteswap16(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32) {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF00FF00) >> 8 |
           (temp & 0x00FF00FF) << 8;
    *ptr++ = temp;
  }
}

void rc_hash_byteswap32(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32) {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF000000) >> 24 |
           (temp & 0x00FF0000) >> 8 |
           (temp & 0x0000FF00) << 8 |
           (temp & 0x000000FF) << 24;
    *ptr++ = temp;
  }
}

int rc_hash_finalize(const rc_hash_iterator_t* iterator, md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  rc_hash_iterator_verbose_formatted(iterator, "Generated hash %s", hash);

  return 1;
}

int rc_hash_buffer(char hash[33], const uint8_t* buffer, size_t buffer_size, const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_init(&md5);

  md5_append(&md5, buffer, (int)buffer_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte buffer", (unsigned)buffer_size);

  return rc_hash_finalize(iterator, &md5, hash);
}

struct rc_buffered_file
{
  const uint8_t* read_ptr;
  const uint8_t* data;
  size_t data_size;
};

static struct rc_buffered_file rc_buffered_file;

static void* rc_file_open_buffered_file(const char* path)
{
  struct rc_buffered_file* handle = (struct rc_buffered_file*)malloc(sizeof(struct rc_buffered_file));
  (void)path;

  if (handle)
    memcpy(handle, &rc_buffered_file, sizeof(rc_buffered_file));

  return handle;
}

static void rc_file_seek_buffered_file(void* file_handle, int64_t offset, int origin)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  switch (origin) {
    case SEEK_SET: buffered_file->read_ptr = buffered_file->data + offset; break;
    case SEEK_CUR: buffered_file->read_ptr += offset; break;
    case SEEK_END: buffered_file->read_ptr = buffered_file->data + buffered_file->data_size + offset; break;
  }

  if (buffered_file->read_ptr < buffered_file->data)
    buffered_file->read_ptr = buffered_file->data;
  else if (buffered_file->read_ptr > buffered_file->data + buffered_file->data_size)
    buffered_file->read_ptr = buffered_file->data + buffered_file->data_size;
}

static int64_t rc_file_tell_buffered_file(void* file_handle)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  return (buffered_file->read_ptr - buffered_file->data);
}

static size_t rc_file_read_buffered_file(void* file_handle, void* buffer, size_t requested_bytes)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  const int64_t remaining = buffered_file->data_size - (buffered_file->read_ptr - buffered_file->data);
  if ((int)requested_bytes > remaining)
     requested_bytes = (int)remaining;

  memcpy(buffer, buffered_file->read_ptr, requested_bytes);
  buffered_file->read_ptr += requested_bytes;
  return requested_bytes;
}

static void rc_file_close_buffered_file(void* file_handle)
{
  free(file_handle);
}

static int rc_hash_file_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  int result;

  rc_hash_iterator_t buffered_file_iterator;
  memset(&buffered_file_iterator, 0, sizeof(buffered_file_iterator));
  memcpy(&buffered_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
  buffered_file_iterator.userdata = iterator->userdata;

  buffered_file_iterator.callbacks.filereader.open = rc_file_open_buffered_file;
  buffered_file_iterator.callbacks.filereader.close = rc_file_close_buffered_file;
  buffered_file_iterator.callbacks.filereader.read = rc_file_read_buffered_file;
  buffered_file_iterator.callbacks.filereader.seek = rc_file_seek_buffered_file;
  buffered_file_iterator.callbacks.filereader.tell = rc_file_tell_buffered_file;
  buffered_file_iterator.path = "memory stream";

  rc_buffered_file.data = rc_buffered_file.read_ptr = iterator->buffer;
  rc_buffered_file.data_size = iterator->buffer_size;

  result = rc_hash_from_file(hash, console_id, &buffered_file_iterator);

  buffered_file_iterator.path = NULL;
  rc_hash_destroy_iterator(&buffered_file_iterator);
  return result;
}

static int rc_hash_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  switch (console_id) {
    default:
      return rc_hash_iterator_error_formatted(iterator, "Unsupported console for buffer hash: %d", console_id);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      return rc_hash_buffer(hash, iterator->buffer, iterator->buffer_size, iterator);

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_ARDUBOY:
      return rc_hash_arduboy(hash, iterator);

    case RC_CONSOLE_ATARI_7800:
      return rc_hash_7800(hash, iterator);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, iterator);

    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, iterator);

    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
      return rc_hash_pce(hash, iterator);

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return rc_hash_scv(hash, iterator);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, iterator);
#endif

    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_NINTENDO_3DS:
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_file_from_buffer(hash, console_id, iterator);
  }
}

int rc_hash_whole_file(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  uint8_t* buffer;
  int64_t size;
  const size_t buffer_size = 65536;
  void* file_handle;
  size_t remaining;
  int result = 0;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing first %u bytes (of %u bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    remaining = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s (%u bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
    remaining = (size_t)size;
  }

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer) {
    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    while (remaining >= buffer_size) {
      rc_file_read(iterator, file_handle, buffer, (int)buffer_size);
      md5_append(&md5, buffer, (int)buffer_size);
      remaining -= buffer_size;
    }

    if (remaining > 0) {
      rc_file_read(iterator, file_handle, buffer, (int)remaining);
      md5_append(&md5, buffer, (int)remaining);
    }

    free(buffer);
    result = rc_hash_finalize(iterator, &md5, hash);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

int rc_hash_buffered_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  uint8_t* buffer;
  int64_t size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering first %u bytes (of %d bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    size = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering %s (%d bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
  }

  buffer = (uint8_t*)malloc((size_t)size);
  if (buffer) {
    rc_hash_iterator_t buffer_iterator;
    memset(&buffer_iterator, 0, sizeof(buffer_iterator));
    memcpy(&buffer_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
    buffer_iterator.userdata = iterator->userdata;
    buffer_iterator.path = iterator->path;
    buffer_iterator.buffer = buffer;
    buffer_iterator.buffer_size = (size_t)size;

    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, (int)size);

    result = rc_hash_from_buffer(hash, console_id, &buffer_iterator);

    free(buffer);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

static int rc_hash_path_is_absolute(const char* path)
{
  if (!path[0])
    return 0;

  /* "/path/to/file" or "\path\to\file" */
  if (path[0] == '/' || path[0] == '\\')
    return 1;

  /* "C:\path\to\file" */
  if (path[1] == ':' && path[2] == '\\')
    return 1;

  /* "scheme:/path/to/file" */
  while (*path) {
    if (path[0] == ':' && path[1] == '/')
      return 1;

    ++path;
  }

  return 0;
}

static const char* rc_hash_get_first_item_from_playlist(const rc_hash_iterator_t* iterator) {
  char buffer[1024];
  char* disc_path;
  char* ptr, *start, *next;
  size_t num_read, path_len, file_len;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle) {
    rc_hash_iterator_error(iterator, "Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(iterator, file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(iterator, file_handle);

  ptr = start = buffer;
  do {
    /* ignore empty and commented lines */
    while (*ptr == '#' || *ptr == '\r' || *ptr == '\n') {
      while (*ptr && *ptr != '\n')
        ++ptr;
      if (*ptr)
        ++ptr;
    }

    /* find and extract the current line */
    start = ptr;
    while (*ptr && *ptr != '\n')
      ++ptr;
    next = ptr;

    /* remove trailing whitespace - especially '\r' */
    while (ptr > start && isspace((unsigned char)ptr[-1]))
      --ptr;

    /* if we found a non-empty line, break out of the loop to process it */
    file_len = ptr - start;
    if (file_len)
      break;

    /* did we reach the end of the file? */
    if (!*next)
      return NULL;

    /* if the line only contained whitespace, keep searching */
    ptr = next + 1;
  } while (1);

  rc_hash_iterator_verbose_formatted(iterator, "Extracted %.*s from playlist", (int)file_len, start);

  start[file_len++] = '\0';
  if (rc_hash_path_is_absolute(start))
    path_len = 0;
  else
    path_len = rc_path_get_filename(iterator->path) - iterator->path;

  disc_path = (char*)malloc(path_len + file_len + 1);
  if (!disc_path)
    return NULL;

  if (path_len)
    memcpy(disc_path, iterator->path, path_len);

  memcpy(&disc_path[path_len], start, file_len);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  rc_hash_iterator_t first_file_iterator;
  const char* disc_path;
  int result;

  rc_hash_iterator_verbose_formatted(iterator, "Processing playlist: %s", rc_path_get_filename(iterator->path));

  disc_path = rc_hash_get_first_item_from_playlist(iterator);
  if (!disc_path)
    return rc_hash_iterator_error(iterator, "Failed to get first item from playlist");

  memset(&first_file_iterator, 0, sizeof(first_file_iterator));
  memcpy(&first_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
  first_file_iterator.userdata = iterator->userdata;
  first_file_iterator.path = disc_path; /* rc_hash_destory_iterator will free */

  result = rc_hash_from_file(hash, console_id, &first_file_iterator);

  rc_hash_destroy_iterator(&first_file_iterator);
  return result;
}

static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  const char* path = iterator->path;

  switch (console_id) {
    default:
      return rc_hash_iterator_error_formatted(iterator, "Unsupported console for file hash: %d", console_id);

    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_MEGA_DRIVE:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_SUPER_CASSETTEVISION:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_whole_file(hash, iterator);

#ifndef RC_HASH_NO_DISC
    case RC_CONSOLE_3DO:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_3do(hash, iterator);
#endif

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, iterator);

    case RC_CONSOLE_ARDUBOY:
      return rc_hash_arduboy(hash, iterator);
#endif

#ifndef RC_HASH_NO_DISC
    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return rc_hash_jaguar_cd(hash, iterator);

    case RC_CONSOLE_DREAMCAST:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_dreamcast(hash, iterator);

    case RC_CONSOLE_GAMECUBE:
      return rc_hash_gamecube(hash, iterator);
#endif

#ifndef RC_HASH_NO_ZIP
    case RC_CONSOLE_MS_DOS:
      return rc_hash_ms_dos(hash, iterator);
#endif

#ifndef RC_HASH_NO_DISC
    case RC_CONSOLE_NEO_GEO_CD:
      return rc_hash_neogeo_cd(hash, iterator);
#endif

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_NINTENDO_64:
      return rc_hash_n64(hash, iterator);
#endif

#ifndef RC_HASH_NO_ENCRYPTED
    case RC_CONSOLE_NINTENDO_3DS:
      return rc_hash_nintendo_3ds(hash, iterator);
#endif

#ifndef RC_HASH_NO_ROM
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_nintendo_ds(hash, iterator);
#endif

#ifndef RC_HASH_NO_DISC
    case RC_CONSOLE_PC_ENGINE_CD:
      if (rc_path_compare_extension(path, "cue") || rc_path_compare_extension(path, "chd"))
        return rc_hash_pce_cd(hash, iterator);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_PCFX:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_pcfx_cd(hash, iterator);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_psx(hash, iterator);

    case RC_CONSOLE_PLAYSTATION_2:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_ps2(hash, iterator);

    case RC_CONSOLE_PSP:
      return rc_hash_psp(hash, iterator);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_sega_cd(hash, iterator);

    case RC_CONSOLE_WII:
      return rc_hash_wii(hash, iterator);
#endif
  }
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path);

static void rc_hash_iterator_append_console(struct rc_hash_iterator* iterator, uint8_t console_id) {
  int i = 0;
  while (iterator->consoles[i] != 0) {
    if (iterator->consoles[i] == console_id)
      return;

    ++i;
  }

  iterator->consoles[i] = console_id;
}

void rc_hash_merge_callbacks(rc_hash_iterator_t* iterator, const rc_hash_callbacks_t* callbacks)
{
  if (callbacks->verbose_message)
    iterator->callbacks.verbose_message = callbacks->verbose_message;
  if (callbacks->error_message)
    iterator->callbacks.verbose_message = callbacks->error_message;

  if (callbacks->filereader.open)
    memcpy(&iterator->callbacks.filereader, &callbacks->filereader, sizeof(callbacks->filereader));

#ifndef RC_HASH_NO_DISC
  if (callbacks->cdreader.open_track)
    memcpy(&iterator->callbacks.cdreader, &callbacks->cdreader, sizeof(callbacks->cdreader));
#endif

#ifndef RC_HASH_NO_ENCRYPTED
  if (callbacks->encryption.get_3ds_cia_normal_key)
    iterator->callbacks.encryption.get_3ds_cia_normal_key = callbacks->encryption.get_3ds_cia_normal_key;
  if (callbacks->encryption.get_3ds_ncch_normal_keys)
    iterator->callbacks.encryption.get_3ds_ncch_normal_keys = callbacks->encryption.get_3ds_ncch_normal_keys;
#endif
}


static void rc_hash_reset_iterator(rc_hash_iterator_t* iterator) {
  memset(iterator, 0, sizeof(*iterator));
  iterator->index = -1;

  if (g_verbose_message_callback)
    iterator->callbacks.verbose_message = rc_hash_call_g_verbose_message_callback;
  if (g_error_message_callback)
    iterator->callbacks.error_message = rc_hash_call_g_error_message_callback;

  if (g_filereader) {
    memcpy(&iterator->callbacks.filereader, g_filereader, sizeof(*g_filereader));
  } else if (!iterator->callbacks.filereader.open) {
    iterator->callbacks.filereader.open = filereader_open;
    iterator->callbacks.filereader.close = filereader_close;
    iterator->callbacks.filereader.seek = filereader_seek;
    iterator->callbacks.filereader.tell = filereader_tell;
    iterator->callbacks.filereader.read = filereader_read;
  }

#ifndef RC_HASH_NO_DISC
  rc_hash_reset_iterator_disc(iterator);
#endif

#ifndef RC_HASH_NO_ENCRYPTED
  rc_hash_reset_iterator_encrypted(iterator);
#endif
}

static void rc_hash_initialize_iterator_single(rc_hash_iterator_t* iterator, int data) {
  iterator->consoles[0] = (uint8_t)data;
}

static void rc_hash_initialize_iterator_bin(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  if (iterator->buffer_size == 0) {
    /* raw bin file may be a CD track. if it's more than 32MB, try a CD hash. */
    const int64_t size = rc_file_size(iterator, iterator->path);
    if (size > 32 * 1024 * 1024) {
      iterator->consoles[0] = RC_CONSOLE_3DO; /* 4DO supports directly opening the bin file */
      iterator->consoles[1] = RC_CONSOLE_PLAYSTATION; /* PCSX ReARMed supports directly opening the bin file*/
      iterator->consoles[2] = RC_CONSOLE_PLAYSTATION_2; /* PCSX2 supports directly opening the bin file*/
      iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* Genesis Plus GX supports directly opening the bin file*/

      /* fallback to megadrive which just does a full hash. */
      iterator->consoles[4] = RC_CONSOLE_MEGA_DRIVE;
      return;
    }
  }

  /* bin is associated with MegaDrive, Sega32X, Atari 2600, Watara Supervision, MegaDuck,
   * Fairchild Channel F, Arcadia 2001, Interton VC 4000, and Super Cassette Vision.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
}

static void rc_hash_initialize_iterator_chd(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PSP;
  iterator->consoles[5] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[6] = RC_CONSOLE_3DO;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_PCFX;
}

static void rc_hash_initialize_iterator_cue(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[5] = RC_CONSOLE_3DO;
  iterator->consoles[6] = RC_CONSOLE_PCFX;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_ATARI_JAGUAR_CD;
}

static void rc_hash_initialize_iterator_d88(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PC8800;
  iterator->consoles[1] = RC_CONSOLE_SHARPX1;
}

static void rc_hash_initialize_iterator_dsk(rc_hash_iterator_t* iterator, int data) {
  size_t size = iterator->buffer_size;
  if (size == 0)
    size = (size_t)rc_file_size(iterator, iterator->path);

  (void)data;

  if (size == 512 * 9 * 80) { /* 360KB */
    /* FAT-12 3.5" DD (512 byte sectors, 9 sectors per track, 80 tracks per side */
    /* FAT-12 5.25" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 80 * 2) { /* 720KB */
    /* FAT-12 3.5" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 40) { /* 180KB */
    /* FAT-12 5.25" DD (512 byte sectors, 9 sectors per track, 40 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;

    /* AMSDOS 3" - 40 tracks */
    iterator->consoles[1] = RC_CONSOLE_AMSTRAD_PC;
  }
  else if (size == 256 * 16 * 35) { /* 140KB */
    /* Apple II new format - 256 byte sectors, 16 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }
  else if (size == 256 * 13 * 35) { /* 113.75KB */
    /* Apple II old format - 256 byte sectors, 13 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }

  /* once a best guess has been identified, make sure the others are added as fallbacks */

  /* check MSX first, as Apple II isn't supported by RetroArch, and RAppleWin won't use the iterator */
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_MSX);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_AMSTRAD_PC);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_ZX_SPECTRUM);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_APPLE_II);
}

static void rc_hash_initialize_iterator_iso(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[1] = RC_CONSOLE_PSP;
  iterator->consoles[2] = RC_CONSOLE_3DO;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_GAMECUBE;
  iterator->consoles[5] = RC_CONSOLE_WII;
}

static void rc_hash_initialize_iterator_m3u(rc_hash_iterator_t* iterator, int data) {
  const char* first_file_path;

  (void)data;

  /* temporarily set the iterator path to the m3u file so we can extract the
   * path of the first disc. rc_hash_get_first_item_from_playlist will return
   * an allocated string or NULL, so rc_hash_destroy_iterator won't get tripped
   * up by the non-allocted value we're about to assign.
   */
  first_file_path = rc_hash_get_first_item_from_playlist(iterator);
  if (!first_file_path) /* did not find a disc */
    return;

  /* release the m3u path and replace with the first file path */
  free((void*)iterator->path);
  iterator->path = first_file_path; /* assert: already malloc'd; don't need to strdup */

  iterator->buffer = NULL; /* ignore buffer; assume it's the m3u contents */

  rc_hash_initialize_iterator_from_path(iterator, iterator->path);
}

static void rc_hash_initialize_iterator_nib(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  iterator->consoles[1] = RC_CONSOLE_COMMODORE_64;
}

static void rc_hash_initialize_iterator_rom(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  /* rom is associated with MSX, Thomson TO-8, and Fairchild Channel F.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MSX;
}

static void rc_hash_initialize_iterator_tap(rc_hash_iterator_t* iterator, int data) {
  (void)data;

  /* also Oric and ZX Spectrum, but all are full file hashes */
  iterator->consoles[0] = RC_CONSOLE_COMMODORE_64;
}

static const rc_hash_iterator_ext_handler_entry_t rc_hash_iterator_ext_handlers[] = {
  { "2d", rc_hash_initialize_iterator_single, RC_CONSOLE_SHARPX1 },
  { "3ds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "3dsx", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "7z", rc_hash_initialize_iterator_single, RC_CONSOLE_ARCADE },
  { "83g", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 }, /* http://tibasicdev.wikidot.com/file-extensions */
  { "83p", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 },
  { "a26", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_2600 },
  { "a78", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_7800 },
  { "app", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "arduboy", rc_hash_initialize_iterator_single, RC_CONSOLE_ARDUBOY },
  { "axf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "bin", rc_hash_initialize_iterator_bin, 0 },
  { "bs", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "cart", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_CASSETTEVISION },
  { "cas", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "cci", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "chd", rc_hash_initialize_iterator_chd, 0 },
  { "chf", rc_hash_initialize_iterator_single, RC_CONSOLE_FAIRCHILD_CHANNEL_F },
  { "cia", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "col", rc_hash_initialize_iterator_single, RC_CONSOLE_COLECOVISION },
  { "csw", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "cue", rc_hash_initialize_iterator_cue, 0 },
  { "cxi", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "d64", rc_hash_initialize_iterator_single, RC_CONSOLE_COMMODORE_64 },
  { "d88", rc_hash_initialize_iterator_d88, 0 },
  { "dosz", rc_hash_initialize_iterator_single, RC_CONSOLE_MS_DOS },
  { "dsk", rc_hash_initialize_iterator_dsk, 0 },
  { "elf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "fd", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 },
  { "fds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "fig", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "gb", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY },
  { "gba", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_ADVANCE },
  { "gbc", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_COLOR },
  { "gdi", rc_hash_initialize_iterator_single, RC_CONSOLE_DREAMCAST },
  { "gg", rc_hash_initialize_iterator_single, RC_CONSOLE_GAME_GEAR },
  { "hex", rc_hash_initialize_iterator_single, RC_CONSOLE_ARDUBOY },
  { "iso", rc_hash_initialize_iterator_iso, 0 },
  { "jag", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_JAGUAR },
  { "k7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* tape */
  { "lnx", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_LYNX },
  { "m3u", rc_hash_initialize_iterator_m3u, 0 },
  { "m5", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "m7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "md", rc_hash_initialize_iterator_single, RC_CONSOLE_MEGA_DRIVE },
  { "min", rc_hash_initialize_iterator_single, RC_CONSOLE_POKEMON_MINI },
  { "mx1", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "mx2", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "n64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "ndd", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "nds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_DS }, /* handles both DS and DSi */
  { "nes", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "ngc", rc_hash_initialize_iterator_single, RC_CONSOLE_NEOGEO_POCKET },
  { "nib", rc_hash_initialize_iterator_nib, 0 },
  { "pbp", rc_hash_initialize_iterator_single, RC_CONSOLE_PSP },
  { "pce", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "pgm", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "pzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "ri", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "rom", rc_hash_initialize_iterator_rom, 0 },
  { "sap", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* disk */
  { "scl", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "sfc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sg", rc_hash_initialize_iterator_single, RC_CONSOLE_SG1000 },
  { "sgx", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "smc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sv", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPERVISION },
  { "swc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "tap", rc_hash_initialize_iterator_tap, 0 },
  { "tic", rc_hash_initialize_iterator_single, RC_CONSOLE_TIC80 },
  { "trd", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "tvc", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "tzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "uze", rc_hash_initialize_iterator_single, RC_CONSOLE_UZEBOX },
  { "v64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "vb", rc_hash_initialize_iterator_single, RC_CONSOLE_VIRTUAL_BOY },
  { "wad", rc_hash_initialize_iterator_single, RC_CONSOLE_WII },
  { "wasm", rc_hash_initialize_iterator_single, RC_CONSOLE_WASM4 },
  { "woz", rc_hash_initialize_iterator_single, RC_CONSOLE_APPLE_II },
  { "wsc", rc_hash_initialize_iterator_single, RC_CONSOLE_WONDERSWAN },
  { "z64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "zip", rc_hash_initialize_iterator_single, RC_CONSOLE_ARCADE }
};

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers) {
  *num_handlers = sizeof(rc_hash_iterator_ext_handlers) / sizeof(rc_hash_iterator_ext_handlers[0]);
  return rc_hash_iterator_ext_handlers;
}

static int rc_hash_iterator_find_handler(const void* left, const void* right) {
  const rc_hash_iterator_ext_handler_entry_t* left_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)left;
  const rc_hash_iterator_ext_handler_entry_t* right_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)right;

  return strcmp(left_handler->ext, right_handler->ext);
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path) {
  size_t num_handlers;
  const rc_hash_iterator_ext_handler_entry_t* handlers = rc_hash_get_iterator_ext_handlers(&num_handlers);
  const rc_hash_iterator_ext_handler_entry_t* handler;
  rc_hash_iterator_ext_handler_entry_t search;
  const char* ext = rc_path_get_extension(path);
  size_t index;

  /* lowercase the extension as we copy it into the search object */
  memset(&search, 0, sizeof(search));
  for (index = 0; index < sizeof(search.ext) - 1; ++index) {
    const int c = (int)ext[index];
    if (!c)
      break;

    search.ext[index] = tolower(c);
  }

  /* find the handler for the extension */
  handler = (const rc_hash_iterator_ext_handler_entry_t*)
    bsearch(&search, handlers, num_handlers, sizeof(*handler), rc_hash_iterator_find_handler);
  if (handler) {
    handler->handler(iterator, handler->data);

    if (iterator->callbacks.verbose_message) {
      int count = 0;
      while (iterator->consoles[count])
        ++count;

      rc_hash_iterator_verbose_formatted(iterator, "Found %d potential consoles for %s file extension", count, ext);
    }
  }
  else {
    rc_hash_iterator_error_formatted(iterator, "No console mapping specified for %s file extension - trying full file hash", ext);

    /* if we didn't match the extension, default to something that does a whole file hash */
    if (!iterator->consoles[0])
      iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
  }
}

void rc_hash_initialize_iterator(rc_hash_iterator_t* iterator, const char* path, const uint8_t* buffer, size_t buffer_size)
{
  rc_hash_reset_iterator(iterator);
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  if (path)
    iterator->path = strdup(path);
}

void rc_hash_destroy_iterator(rc_hash_iterator_t* iterator) {
  if (iterator->path) {
    free((void*)iterator->path);
    iterator->path = NULL;
  }

  iterator->buffer = NULL;
}

int rc_hash_iterate(char hash[33], rc_hash_iterator_t* iterator) {
  int next_console;
  int result = 0;

  if (iterator->index == -1) {
    rc_hash_initialize_iterator_from_path(iterator, iterator->path);
    iterator->index = 0;
  }

  do {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0) {
      hash[0] = '\0';
      break;
    }

    ++iterator->index;

    rc_hash_iterator_verbose_formatted(iterator, "Trying console %d", next_console);

    result = rc_hash_generate(hash, next_console, iterator);
  } while (!result);

  return result;
}

int rc_hash_generate(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  if (iterator->buffer)
    return rc_hash_from_buffer(hash, console_id, iterator);

  return rc_hash_from_file(hash, console_id, iterator);
}

int rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size) {
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.buffer = buffer;
  iterator.buffer_size = buffer_size;

  result = rc_hash_from_buffer(hash, console_id, &iterator);

  rc_hash_destroy_iterator(&iterator);

  return result;
}

int rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path){
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.path = path;

  result = rc_hash_from_file(hash, console_id, &iterator);

  iterator.path = NULL; /* prevent free. we didn't strdup */

  rc_hash_destroy_iterator(&iterator);

  return result;
}
