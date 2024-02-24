#include "rc_hash.h"

#include "../rc_compat.h"

#include "md5.h"

#include <stdio.h>
#include <ctype.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* arbitrary limit to prevent allocating and hashing large files */
#define MAX_BUFFER_SIZE 64 * 1024 * 1024

const char* rc_path_get_filename(const char* path);
static int rc_hash_whole_file(char hash[33], const char* path);

/* ===================================================== */

static rc_hash_message_callback error_message_callback = NULL;
rc_hash_message_callback verbose_message_callback = NULL;

void rc_hash_init_error_message_callback(rc_hash_message_callback callback)
{
  error_message_callback = callback;
}

int rc_hash_error(const char* message)
{
  if (error_message_callback)
    error_message_callback(message);

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback)
{
  verbose_message_callback = callback;
}

static void rc_hash_verbose(const char* message)
{
  if (verbose_message_callback)
    verbose_message_callback(message);
}

/* ===================================================== */

static struct rc_hash_filereader filereader_funcs;
static struct rc_hash_filereader* filereader = NULL;

static void* filereader_open(const char* path)
{
#if defined(WINVER) && WINVER >= 0x0500
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

  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_length) == 0)
  {
    free(wpath);
    return NULL;
  }
#if defined(__STDC_WANT_SECURE_LIB__)
  _wfopen_s(&fp, wpath, L"rb");
#else
  fp = _wfopen(wpath, L"rb");
#endif
  free(wpath);
  return fp;
#elif defined(__STDC_WANT_SECURE_LIB__)
  FILE* fp;
  fopen_s(&fp, path, "rb");
  return fp;
#else
  return fopen(path, "rb");
#endif
}

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
  filereader = NULL;
}

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  /* initialize with defaults first */
  filereader_funcs.open = filereader_open;
  filereader_funcs.seek = filereader_seek;
  filereader_funcs.tell = filereader_tell;
  filereader_funcs.read = filereader_read;
  filereader_funcs.close = filereader_close;

  /* hook up any provided custom handlers */
  if (reader) {
    if (reader->open)
      filereader_funcs.open = reader->open;

    if (reader->seek)
      filereader_funcs.seek = reader->seek;

    if (reader->tell)
      filereader_funcs.tell = reader->tell;

    if (reader->read)
      filereader_funcs.read = reader->read;

    if (reader->close)
      filereader_funcs.close = reader->close;
  }

  filereader = &filereader_funcs;
}

void* rc_file_open(const char* path)
{
  void* handle;

  if (!filereader)
  {
    rc_hash_init_custom_filereader(NULL);
    if (!filereader)
      return NULL;
  }

  handle = filereader->open(path);
  if (handle && verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Opened %s", rc_path_get_filename(path));
    verbose_message_callback(message);
  }

  return handle;
}

void rc_file_seek(void* file_handle, int64_t offset, int origin)
{
  if (filereader)
    filereader->seek(file_handle, offset, origin);
}

int64_t rc_file_tell(void* file_handle)
{
  return (filereader) ? filereader->tell(file_handle) : 0;
}

size_t rc_file_read(void* file_handle, void* buffer, int requested_bytes)
{
  return (filereader) ? filereader->read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(void* file_handle)
{
  if (filereader)
    filereader->close(file_handle);
}

/* ===================================================== */

static struct rc_hash_cdreader cdreader_funcs;
struct rc_hash_cdreader* cdreader = NULL;

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  if (reader)
  {
    memcpy(&cdreader_funcs, reader, sizeof(cdreader_funcs));
    cdreader = &cdreader_funcs;
  }
  else
  {
    cdreader = NULL;
  }
}

static void* rc_cd_open_track(const char* path, uint32_t track)
{
  if (cdreader && cdreader->open_track)
    return cdreader->open_track(path, track);

  rc_hash_error("no hook registered for cdreader_open_track");
  return NULL;
}

static size_t rc_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  if (cdreader && cdreader->read_sector)
    return cdreader->read_sector(track_handle, sector, buffer, requested_bytes);

  rc_hash_error("no hook registered for cdreader_read_sector");
  return 0;
}

static uint32_t rc_cd_first_track_sector(void* track_handle)
{
  if (cdreader && cdreader->first_track_sector)
    return cdreader->first_track_sector(track_handle);

  rc_hash_error("no hook registered for cdreader_first_track_sector");
  return 0;
}

static void rc_cd_close_track(void* track_handle)
{
  if (cdreader && cdreader->close_track)
  {
    cdreader->close_track(track_handle);
    return;
  }

  rc_hash_error("no hook registered for cdreader_close_track");
}

static uint32_t rc_cd_find_file_sector(void* track_handle, const char* path, uint32_t* size)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  uint32_t num_sectors = 0;
  size_t filename_length;
  const char* slash;

  if (!track_handle)
    return 0;

  /* we start at the root. don't need to explicitly find it */
  if (*path == '\\')
    ++path;

  filename_length = strlen(path);
  slash = strrchr(path, '\\');
  if (slash)
  {
    /* find the directory record for the first part of the path */
    memcpy(buffer, path, slash - path);
    buffer[slash - path] = '\0';

    sector = rc_cd_find_file_sector(track_handle, (const char *)buffer, NULL);
    if (!sector)
      return 0;

    ++slash;
    filename_length -= (slash - path);
    path = slash;
  }
  else
  {
    uint32_t logical_block_size;

    /* find the cd information */
    if (!rc_cd_read_sector(track_handle, rc_cd_first_track_sector(track_handle) + 16, buffer, 256))
      return 0;

    /* the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
     * https://www.cdroller.com/htm/readdata.html
     */
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);

    /* if the table of contents spans more than one sector, it's length of section will exceed it's logical block size */
    logical_block_size = (buffer[128] | (buffer[128 + 1] << 8)); /* logical block size */
    if (logical_block_size == 0) {
      num_sectors = 1;
    } else {
      num_sectors = (buffer[156 + 10] | (buffer[156 + 11] << 8) | (buffer[156 + 12] << 16) | (buffer[156 + 13] << 24)); /* length of section */
      num_sectors /= logical_block_size;
    }
  }

  /* fetch and process the directory record */
  if (!rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)))
    return 0;

  tmp = buffer;
  do
  {
    if (tmp >= buffer + sizeof(buffer) || !*tmp)
    {
      /* end of this path table block. if the path table spans multiple sectors, keep scanning */
      if (num_sectors > 1)
      {
        --num_sectors;
        if (rc_cd_read_sector(track_handle, ++sector, buffer, sizeof(buffer)))
        {
          tmp = buffer;
          continue;
        }
      }
      break;
    }

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[32] == filename_length || tmp[33 + filename_length] == ';') &&
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0)
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);

      if (verbose_message_callback)
      {
        char message[128];
        snprintf(message, sizeof(message), "Found %s at sector %d", path, sector);
        verbose_message_callback(message);
      }

      if (size)
        *size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

      return sector;
    }

    /* the first byte of the record is the length of the record */
    tmp += *tmp;
  } while (1);

  return 0;
}

/* ===================================================== */

const char* rc_path_get_filename(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
    if (ptr[-1] == '/' || ptr[-1] == '\\')
      break;

    --ptr;
  } while (ptr > path);

  return ptr;
}

static const char* rc_path_get_extension(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
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

  do
  {
    if (tolower(*ptr) != *ext)
      return 0;

    ++ext;
    ++ptr;
  } while (*ptr);

  return 1;
}

/* ===================================================== */

static void rc_hash_byteswap16(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF00FF00) >> 8 |
           (temp & 0x00FF00FF) << 8;
    *ptr++ = temp;
  }
}

static void rc_hash_byteswap32(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF000000) >> 24 |
           (temp & 0x00FF0000) >> 8 |
           (temp & 0x0000FF00) << 8 |
           (temp & 0x000000FF) << 24;
    *ptr++ = temp;
  }
}

static int rc_hash_finalize(md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  if (verbose_message_callback)
  {
    char message[128];
    snprintf(message, sizeof(message), "Generated hash %s", hash);
    verbose_message_callback(message);
  }

  return 1;
}

static int rc_hash_buffer(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  md5_state_t md5;
  md5_init(&md5);

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_append(&md5, buffer, (int)buffer_size);

  if (verbose_message_callback)
  {
    char message[128];
    snprintf(message, sizeof(message), "Hashing %u byte buffer", (unsigned)buffer_size);
    verbose_message_callback(message);
  }

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_cd_file(md5_state_t* md5, void* track_handle, uint32_t sector, const char* name, uint32_t size, const char* description)
{
  uint8_t buffer[2048];
  size_t num_read;

  if ((num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer))) < sizeof(buffer))
  {
    char message[128];
    snprintf(message, sizeof(message), "Could not read %s", description);
    return rc_hash_error(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  if (verbose_message_callback)
  {
    char message[128];
    if (name)
      snprintf(message, sizeof(message), "Hashing %s title (%u bytes) and contents (%u bytes) ", name, (unsigned)strlen(name), size);
    else
      snprintf(message, sizeof(message), "Hashing %s contents (%u bytes @ sector %u)", description, size, sector);

    verbose_message_callback(message);
  }

  if (size < (unsigned)num_read) /* we read a whole sector - only hash the part containing file data */
    num_read = (size_t)size;

  do
  {
    md5_append(md5, buffer, (int)num_read);

    if (size <= (unsigned)num_read)
      break;
    size -= (unsigned)num_read;

    ++sector;
    if (size >= sizeof(buffer))
      num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
    else
      num_read = rc_cd_read_sector(track_handle, sector, buffer, size);
  } while (num_read > 0);

  return 1;
}

static int rc_hash_3do(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  const uint8_t operafs_identifier[7] = { 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01 };
  void* track_handle;
  md5_state_t md5;
  int sector;
  int block_size, block_location;
  int offset, stop;
  size_t size = 0;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the Opera filesystem stores the volume information in the first 132 bytes of sector 0
   * https://github.com/barbeque/3dodump/blob/master/OperaFS-Format.md
   */
  rc_cd_read_sector(track_handle, 0, buffer, 132);

  if (memcmp(buffer, operafs_identifier, sizeof(operafs_identifier)) == 0)
  {
    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Found 3DO CD, title=%.32s", &buffer[0x28]);
      verbose_message_callback(message);
    }

    /* include the volume header in the hash */
    md5_init(&md5);
    md5_append(&md5, buffer, 132);

    /* the block size is at offset 0x4C (assume 0x4C is always 0) */
    block_size = buffer[0x4D] * 65536 + buffer[0x4E] * 256 + buffer[0x4F];

    /* the root directory block location is at offset 0x64 (and duplicated several
     * times, but we just look at the primary record) (assume 0x64 is always 0)*/
    block_location = buffer[0x65] * 65536 + buffer[0x66] * 256 + buffer[0x67];

    /* multiply the block index by the block size to get the real address */
    block_location *= block_size;

    /* convert that to a sector and read it */
    sector = block_location / 2048;

    do
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));

      /* offset to start of entries is at offset 0x10 (assume 0x10 and 0x11 are always 0) */
      offset = buffer[0x12] * 256 + buffer[0x13];

      /* offset to end of entries is at offset 0x0C (assume 0x0C is always 0) */
      stop = buffer[0x0D] * 65536 + buffer[0x0E] * 256 + buffer[0x0F];

      while (offset < stop)
      {
        if (buffer[offset + 0x03] == 0x02) /* file */
        {
          if (strcasecmp((const char*)&buffer[offset + 0x20], "LaunchMe") == 0)
          {
            /* the block size is at offset 0x0C (assume 0x0C is always 0) */
            block_size = buffer[offset + 0x0D] * 65536 + buffer[offset + 0x0E] * 256 + buffer[offset + 0x0F];

            /* the block location is at offset 0x44 (assume 0x44 is always 0) */
            block_location = buffer[offset + 0x45] * 65536 + buffer[offset + 0x46] * 256 + buffer[offset + 0x47];
            block_location *= block_size;

            /* the file size is at offset 0x10 (assume 0x10 is always 0) */
            size = (size_t)buffer[offset + 0x11] * 65536 + buffer[offset + 0x12] * 256 + buffer[offset + 0x13];

            if (verbose_message_callback)
            {
              char message[128];
              snprintf(message, sizeof(message), "Hashing header (%u bytes) and %.32s (%u bytes) ", 132, &buffer[offset + 0x20], (unsigned)size);
              verbose_message_callback(message);
            }

            break;
          }
        }

        /* the number of extra copies of the file is at offset 0x40 (assume 0x40-0x42 are always 0) */
        offset += 0x48 + buffer[offset + 0x43] * 4;
      }

      if (size != 0)
        break;

      /* did not find the file, see if the directory listing is continued in another sector */
      offset = buffer[0x02] * 256 + buffer[0x03];

      /* no more sectors to search*/
      if (offset == 0xFFFF)
        break;

      /* get next sector */
      offset *= block_size;
      sector = (block_location + offset) / 2048;
    } while (1);

    if (size == 0)
    {
      rc_cd_close_track(track_handle);
      return rc_hash_error("Could not find LaunchMe");
    }

    sector = block_location / 2048;

    while (size > 2048)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= 2048;
    }

    rc_cd_read_sector(track_handle, sector, buffer, size);
    md5_append(&md5, buffer, (int)size);
  }
  else
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a 3DO CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_7800(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&buffer[1], "ATARI7800", 9) == 0)
  {
    rc_hash_verbose("Ignoring 7800 header");

    buffer += 128;
    buffer_size -= 128;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

struct rc_hash_zip_idx
{
  size_t length;
  uint8_t* data;
};

static int rc_hash_zip_idx_sort(const void* a, const void* b)
{
  struct rc_hash_zip_idx *A = (struct rc_hash_zip_idx*)a, *B = (struct rc_hash_zip_idx*)b;
  size_t len = (A->length < B->length ? A->length : B->length);
  return memcmp(A->data, B->data, len);
}

static int rc_hash_zip_file(md5_state_t* md5, void* file_handle)
{
  uint8_t buf[2048], *alloc_buf, *cdir_start, *cdir_max, *cdir, *hashdata, eocdirhdr_size, cdirhdr_size;
  uint32_t cdir_entry_len;
  size_t sizeof_idx, indices_offset, alloc_size;
  int64_t i_file, archive_size, ecdh_ofs, total_files, cdir_size, cdir_ofs;
  struct rc_hash_zip_idx* hashindices, *hashindex;

  rc_file_seek(file_handle, 0, SEEK_END);
  archive_size = rc_file_tell(file_handle);

  /* Basic sanity checks - reject files which are too small */
  eocdirhdr_size = 22; /* the 'end of central directory header' is 22 bytes */
  if (archive_size < eocdirhdr_size)
    return rc_hash_error("ZIP is too small");

  /* Macros used for reading ZIP and writing to a buffer for hashing (undefined again at the end of the function) */
  #define RC_ZIP_READ_LE16(p) ((uint16_t)(((const uint8_t*)(p))[0]) | ((uint16_t)(((const uint8_t*)(p))[1]) << 8U))
  #define RC_ZIP_READ_LE32(p) ((uint32_t)(((const uint8_t*)(p))[0]) | ((uint32_t)(((const uint8_t*)(p))[1]) << 8U) | ((uint32_t)(((const uint8_t*)(p))[2]) << 16U) | ((uint32_t)(((const uint8_t*)(p))[3]) << 24U))
  #define RC_ZIP_READ_LE64(p) ((uint64_t)(((const uint8_t*)(p))[0]) | ((uint64_t)(((const uint8_t*)(p))[1]) << 8U) | ((uint64_t)(((const uint8_t*)(p))[2]) << 16U) | ((uint64_t)(((const uint8_t*)(p))[3]) << 24U) | ((uint64_t)(((const uint8_t*)(p))[4]) << 32U) | ((uint64_t)(((const uint8_t*)(p))[5]) << 40U) | ((uint64_t)(((const uint8_t*)(p))[6]) << 48U) | ((uint64_t)(((const uint8_t*)(p))[7]) << 56U))
  #define RC_ZIP_WRITE_LE32(p,v) { ((uint8_t*)(p))[0] = (uint8_t)((uint32_t)(v) & 0xFF); ((uint8_t*)(p))[1] = (uint8_t)(((uint32_t)(v) >> 8) & 0xFF); ((uint8_t*)(p))[2] = (uint8_t)(((uint32_t)(v) >> 16) & 0xFF); ((uint8_t*)(p))[3] = (uint8_t)((uint32_t)(v) >> 24); }
  #define RC_ZIP_WRITE_LE64(p,v) { ((uint8_t*)(p))[0] = (uint8_t)((uint64_t)(v) & 0xFF); ((uint8_t*)(p))[1] = (uint8_t)(((uint64_t)(v) >> 8) & 0xFF); ((uint8_t*)(p))[2] = (uint8_t)(((uint64_t)(v) >> 16) & 0xFF); ((uint8_t*)(p))[3] = (uint8_t)(((uint64_t)(v) >> 24) & 0xFF); ((uint8_t*)(p))[4] = (uint8_t)(((uint64_t)(v) >> 32) & 0xFF); ((uint8_t*)(p))[5] = (uint8_t)(((uint64_t)(v) >> 40) & 0xFF); ((uint8_t*)(p))[6] = (uint8_t)(((uint64_t)(v) >> 48) & 0xFF); ((uint8_t*)(p))[7] = (uint8_t)((uint64_t)(v) >> 56); }

  /* Find the end of central directory record by scanning the file from the end towards the beginning */
  for (ecdh_ofs = archive_size - sizeof(buf); ; ecdh_ofs -= (sizeof(buf) - 3))
  {
    int i, n = sizeof(buf);
    if (ecdh_ofs < 0)
      ecdh_ofs = 0;
    if (n > archive_size)
      n = (int)archive_size;
    rc_file_seek(file_handle, ecdh_ofs, SEEK_SET);
    if (rc_file_read(file_handle, buf, n) != (size_t)n)
      return rc_hash_error("ZIP read error");
    for (i = n - 4; i >= 0; --i)
      if (RC_ZIP_READ_LE32(buf + i) == 0x06054b50) /* end of central directory header signature */
        break;
    if (i >= 0)
    {
      ecdh_ofs += i;
      break;
    }
    if (!ecdh_ofs || (archive_size - ecdh_ofs) >= (0xFFFF + eocdirhdr_size))
      return rc_hash_error("Failed to find ZIP central directory");
  }

  /* Read and verify the end of central directory record. */
  rc_file_seek(file_handle, ecdh_ofs, SEEK_SET);
  if (rc_file_read(file_handle, buf, eocdirhdr_size) != eocdirhdr_size)
    return rc_hash_error("Failed to read ZIP central directory");

  /* Read central dir information from end of central directory header */
  total_files = RC_ZIP_READ_LE16(buf + 0x0A);
  cdir_size   = RC_ZIP_READ_LE32(buf + 0x0C);
  cdir_ofs    = RC_ZIP_READ_LE32(buf + 0x10);

  /* Check if this is a Zip64 file. In the block of code below:
   * - 20 is the size of the ZIP64 end of central directory locator
   * - 56 is the size of the ZIP64 end of central directory header
   */
  if ((cdir_ofs == 0xFFFFFFFF || cdir_size == 0xFFFFFFFF || total_files == 0xFFFF) && ecdh_ofs >= (20 + 56))
  {
    /* Read the ZIP64 end of central directory locator if it actually exists */
    rc_file_seek(file_handle, ecdh_ofs - 20, SEEK_SET);
    if (rc_file_read(file_handle, buf, 20) == 20 && RC_ZIP_READ_LE32(buf) == 0x07064b50) /* locator signature */
    {
      /* Found the locator, now read the actual ZIP64 end of central directory header */ 
      int64_t ecdh64_ofs = (int64_t)RC_ZIP_READ_LE64(buf + 0x08);
      if (ecdh64_ofs <= (archive_size - 56))
      {
        rc_file_seek(file_handle, ecdh64_ofs, SEEK_SET);
        if (rc_file_read(file_handle, buf, 56) == 56 && RC_ZIP_READ_LE32(buf) == 0x06064b50) /* header signature */
        {
          total_files = RC_ZIP_READ_LE64(buf + 0x20);
          cdir_size   = RC_ZIP_READ_LE64(buf + 0x28);
          cdir_ofs    = RC_ZIP_READ_LE64(buf + 0x30);
        }
      }
    }
  }

  /* Basic verificaton of central directory (limit to a 256MB content directory) */
  cdirhdr_size = 46; /* the 'central directory header' is 46 bytes */
  if ((cdir_size >= 0x10000000) || (cdir_size < total_files * cdirhdr_size) || ((cdir_ofs + cdir_size) > archive_size))
    return rc_hash_error("Central directory of ZIP file is invalid");

  /* Allocate once for both directory and our temporary sort index (memory aligned to sizeof(rc_hash_zip_idx)) */
  sizeof_idx = sizeof(struct rc_hash_zip_idx);
  indices_offset = (size_t)((cdir_size + sizeof_idx - 1) / sizeof_idx * sizeof_idx);
  alloc_size = (size_t)(indices_offset + total_files * sizeof_idx);
  alloc_buf = (uint8_t*)malloc(alloc_size);

  /* Read entire central directory to a buffer */
  if (!alloc_buf)
    return rc_hash_error("Could not allocate temporary buffer");
  rc_file_seek(file_handle, cdir_ofs, SEEK_SET);
  if ((int64_t)rc_file_read(file_handle, alloc_buf, (int)cdir_size) != cdir_size)
  {
    free(alloc_buf);
    return rc_hash_error("Failed to read central directory of ZIP file");
  }

  cdir_start = alloc_buf;
  cdir_max = cdir_start + cdir_size - cdirhdr_size;
  cdir = cdir_start;

  /* Write our temporary hash data to the same buffer we read the central directory from.
   * We can do that because the amount of data we keep for each file is guaranteed to be less than the file record.
   */
  hashdata = alloc_buf;
  hashindices = (struct rc_hash_zip_idx*)(alloc_buf + indices_offset);
  hashindex = hashindices;

  /* Now process the central directory file records */
  for (i_file = 0, cdir = cdir_start; i_file < total_files && cdir >= cdir_start && cdir <= cdir_max; i_file++, cdir += cdir_entry_len)
  {
    const uint8_t *name, *name_end;
    uint32_t signature     = RC_ZIP_READ_LE32(cdir + 0x00);
    uint32_t method        = RC_ZIP_READ_LE16(cdir + 0x0A);
    uint32_t crc32         = RC_ZIP_READ_LE32(cdir + 0x10);
    uint64_t comp_size     = RC_ZIP_READ_LE32(cdir + 0x14);
    uint64_t decomp_size   = RC_ZIP_READ_LE32(cdir + 0x18);
    uint32_t filename_len  = RC_ZIP_READ_LE16(cdir + 0x1C);
    int32_t  extra_len     = RC_ZIP_READ_LE16(cdir + 0x1E);
    int32_t  comment_len   = RC_ZIP_READ_LE16(cdir + 0x20);
    uint64_t local_hdr_ofs = RC_ZIP_READ_LE32(cdir + 0x2A);
    cdir_entry_len = cdirhdr_size + filename_len + extra_len + comment_len;

    if (signature != 0x02014b50) /* expected central directory entry signature */
      break;

    /* Handle Zip64 fields */
    if (decomp_size == 0xFFFFFFFF || comp_size == 0xFFFFFFFF || local_hdr_ofs == 0xFFFFFFFF)
    {
      int invalid = 0;
      const uint8_t *x = cdir + cdirhdr_size + filename_len, *xEnd, *field, *fieldEnd;
      for (xEnd = x + extra_len; (x + (sizeof(uint16_t) * 2)) < xEnd; x = fieldEnd)
      {
        field = x + (sizeof(uint16_t) * 2);
        fieldEnd = field + RC_ZIP_READ_LE16(x + 2);
        if (RC_ZIP_READ_LE16(x) != 0x0001 || fieldEnd > xEnd)
          continue; /* Not the Zip64 extended information extra field */

        if (decomp_size == 0xFFFFFFFF)
        {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) { invalid = 1; break; }
          decomp_size = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }
        if (comp_size == 0xFFFFFFFF)
        {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) { invalid = 1; break; }
          comp_size = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }
        if (local_hdr_ofs == 0xFFFFFFFF)
        {
          if ((unsigned)(fieldEnd - field) < sizeof(uint64_t)) { invalid = 1; break; }
          local_hdr_ofs = RC_ZIP_READ_LE64(field);
          field += sizeof(uint64_t);
        }
        break;
      }
      if (invalid)
      {
        free(alloc_buf);
        return rc_hash_error("Encountered invalid Zip64 file");
      }
    }

    /* Basic sanity check on file record */
    /* 30 is the length of the local directory header preceeding the compressed data */
    if ((!method && decomp_size != comp_size) || (decomp_size && !comp_size) || ((local_hdr_ofs + 30 + comp_size) > (uint64_t)archive_size))
    {
      free(alloc_buf);
      return rc_hash_error("Encountered invalid entry in ZIP central directory");
    }

    /* Write the pointer and length of the data we record about this file */
    hashindex->data = hashdata;
    hashindex->length = filename_len + 1 + 4 + 8;
    hashindex++;

    /* Convert and store the file name in the hash data buffer */
    for (name = (cdir + cdirhdr_size), name_end = name + filename_len; name != name_end; name++)
    {
      *(hashdata++) =
        (*name == '\\' ? '/' : /* convert back-slashes to regular slashes */
        (*name >= 'A' && *name <= 'Z') ? (*name | 0x20) : /* convert upper case letters to lower case */
        *name); /* else use the byte as-is */
    }

    /* Add zero terminator, CRC32 and decompressed size to the hash data buffer */
    *(hashdata++) = '\0';
    RC_ZIP_WRITE_LE32(hashdata, crc32);
    hashdata += 4;
    RC_ZIP_WRITE_LE64(hashdata, decomp_size);
    hashdata += 8;

    if (verbose_message_callback)
    {
      char message[1024];
      snprintf(message, sizeof(message), "File in ZIP: %.*s (%u bytes, CRC32 = %08X)", filename_len, (const char*)(cdir + cdirhdr_size), (unsigned)decomp_size, crc32);
      verbose_message_callback(message);
    }
  }

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Hashing %u files in ZIP archive", (unsigned)(hashindex - hashindices));
    verbose_message_callback(message);
  }

  /* Sort the file list indices */
  qsort(hashindices, (hashindex - hashindices), sizeof(struct rc_hash_zip_idx), rc_hash_zip_idx_sort);

  /* Hash the data in the order of the now sorted indices */
  for (; hashindices != hashindex; hashindices++)
    md5_append(md5, hashindices->data, (int)hashindices->length);

  free(alloc_buf);
  return 1;

  #undef RC_ZIP_READ_LE16
  #undef RC_ZIP_READ_LE32
  #undef RC_ZIP_READ_LE64
  #undef RC_ZIP_WRITE_LE32
  #undef RC_ZIP_WRITE_LE64
}

static int rc_hash_ms_dos(char hash[33], const char* path)
{
  md5_state_t md5;
  size_t path_len;
  int res;

  void* file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  /* hash the main content zip file first */
  md5_init(&md5);
  res = rc_hash_zip_file(&md5, file_handle);
  rc_file_close(file_handle);

  if (!res)
    return 0;

  /* if this is a .dosz file, check if an associated .dosc file exists */
  path_len = strlen(path);
  if (path[path_len-1] == 'z' || path[path_len-1] == 'Z')
  {
    char *dosc_path = strdup(path);
    if (!dosc_path)
        return rc_hash_error("Could not allocate temporary buffer");

    /* swap the z to c and use the same capitalization, hash the file if it exists*/
    dosc_path[path_len-1] = (path[path_len-1] == 'z' ? 'c' : 'C');
    file_handle = rc_file_open(dosc_path);
    free((void*)dosc_path);
    if (file_handle)
    {
      res = rc_hash_zip_file(&md5, file_handle);
      rc_file_close(file_handle);

      if (!res)
        return 0;
    }
  }

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_arcade(char hash[33], const char* path)
{
  /* arcade hash is just the hash of the filename (no extension) - the cores are pretty stringent about having the right ROM data */
  const char* filename = rc_path_get_filename(path);
  const char* ext = rc_path_get_extension(filename);
  size_t filename_length = ext - filename - 1;

  /* fbneo supports loading subsystems by using specific folder names.
   * if one is found, include it in the hash.
   * https://github.com/libretro/FBNeo/blob/master/src/burner/libretro/README.md#emulating-consoles
   */
  if (filename > path + 1)
  {
    int include_folder = 0;
    const char* folder = filename - 1;
    size_t parent_folder_length = 0;

    do
    {
      if (folder[-1] == '/' || folder[-1] == '\\')
        break;

      --folder;
    } while (folder > path);

    parent_folder_length = filename - folder - 1;
    switch (parent_folder_length)
    {
      case 3:
        if (memcmp(folder, "nes", 3) == 0 ||
            memcmp(folder, "fds", 3) == 0 ||
            memcmp(folder, "sms", 3) == 0 ||
            memcmp(folder, "msx", 3) == 0 ||
            memcmp(folder, "ngp", 3) == 0 ||
            memcmp(folder, "pce", 3) == 0 ||
            memcmp(folder, "sgx", 3) == 0)
          include_folder = 1;
        break;
      case 4:
        if (memcmp(folder, "tg16", 4) == 0)
          include_folder = 1;
        break;
      case 6:
        if (memcmp(folder, "coleco", 6) == 0 ||
            memcmp(folder, "sg1000", 6) == 0)
          include_folder = 1;
        break;
      case 8:
        if (memcmp(folder, "gamegear", 8) == 0 ||
            memcmp(folder, "megadriv", 8) == 0 ||
            memcmp(folder, "spectrum", 8) == 0)
          include_folder = 1;
        break;
      default:
        break;
    }

    if (include_folder)
    {
      char buffer[128]; /* realistically, this should never need more than ~20 characters */
      if (parent_folder_length + filename_length + 1 < sizeof(buffer))
      {
        memcpy(&buffer[0], folder, parent_folder_length);
        buffer[parent_folder_length] = '_';
        memcpy(&buffer[parent_folder_length + 1], filename, filename_length);
        return rc_hash_buffer(hash, (uint8_t*)&buffer[0], parent_folder_length + filename_length + 1);
      }
    }
  }

  return rc_hash_buffer(hash, (uint8_t*)filename, filename_length);
}

static int rc_hash_text(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  md5_state_t md5;
  const uint8_t* scan = buffer;
  const uint8_t* stop = buffer + buffer_size;

  md5_init(&md5);

  do {
    /* find end of line */
    while (scan < stop && *scan != '\r' && *scan != '\n')
      ++scan;

    md5_append(&md5, buffer, (int)(scan - buffer));

    /* include a normalized line ending */
    /* NOTE: this causes a line ending to be hashed at the end of the file, even if one was not present */
    md5_append(&md5, (const uint8_t*)"\n", 1);

    /* skip newline */
    if (scan < stop && *scan == '\r')
      ++scan;
    if (scan < stop && *scan == '\n')
      ++scan;

    buffer = scan;
  } while (scan < stop);

  return rc_hash_finalize(&md5, hash);
}

/* helper variable only used for testing */
const char* _rc_hash_jaguar_cd_homebrew_hash = NULL;

static int rc_hash_jaguar_cd(char hash[33], const char* path)
{
  uint8_t buffer[2352];
  char message[128];
  void* track_handle;
  md5_state_t md5;
  int byteswapped = 0;
  uint32_t size = 0;
  uint32_t offset = 0;
  uint32_t sector = 0;
  uint32_t remaining;
  uint32_t i;

  /* Jaguar CD header is in the first sector of the first data track OF THE SECOND SESSION.
   * The first track must be an audio track, but may be a warning message or actual game audio */
  track_handle = rc_cd_open_track(path, RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* The header is an unspecified distance into the first sector, but usually two bytes in.
   * It consists of 64 bytes of "TAIR" or "ATRI" repeating, depending on whether or not the data 
   * is byteswapped. Then another 32 byte that reads "ATARI APPROVED DATA HEADER ATRI "
   * (possibly byteswapped). Then a big-endian 32-bit value for the address where the boot code
   * should be loaded, and a second big-endian 32-bit value for the size of the boot code. */ 
  sector = rc_cd_first_track_sector(track_handle);
  rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));

  for (i = 64; i < sizeof(buffer) - 32 - 4 * 3; i++)
  {
    if (memcmp(&buffer[i], "TARA IPARPVODED TA AEHDAREA RT I", 32) == 0)
    {
      byteswapped = 1;
      offset = i + 32 + 4;
      size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
      break;
    }
    else if (memcmp(&buffer[i], "ATARI APPROVED DATA HEADER ATRI ", 32) == 0)
    {
      byteswapped = 0;
      offset = i + 32 + 4;
      size = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | (buffer[offset + 2] << 8) | (buffer[offset + 3]);
      break;
    }
  }

  if (size == 0) /* did not see ATARI APPROVED DATA HEADER */
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a Jaguar CD");
  }

  i = 0; /* only loop once */
  do
  {
    md5_init(&md5);

    offset += 4;

    if (verbose_message_callback)
    {
      snprintf(message, sizeof(message), "Hashing boot executable (%u bytes starting at %u bytes into sector %u)", size, offset, sector);
      rc_hash_verbose(message);
    }

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    do
    {
      if (byteswapped)
        rc_hash_byteswap16(buffer, &buffer[sizeof(buffer)]);

      remaining = sizeof(buffer) - offset;
      if (remaining >= size)
      {
        md5_append(&md5, &buffer[offset], size);
        size = 0;
        break;
      }

      md5_append(&md5, &buffer[offset], remaining);
      size -= remaining;
      offset = 0;
    } while (rc_cd_read_sector(track_handle, ++sector, buffer, sizeof(buffer)) == sizeof(buffer));

    rc_cd_close_track(track_handle);

    if (size > 0)
      return rc_hash_error("Not enough data");

    rc_hash_finalize(&md5, hash);

    /* homebrew games all seem to have the same boot executable and store the actual game code in track 2.
     * if we generated something other than the homebrew hash, return it. assume all homebrews are byteswapped. */
    if (strcmp(hash, "254487b59ab21bc005338e85cbf9fd2f") != 0 || !byteswapped) {
      if (_rc_hash_jaguar_cd_homebrew_hash == NULL || strcmp(hash, _rc_hash_jaguar_cd_homebrew_hash) != 0)
        return 1;
    }

    /* if we've already been through the loop a second time, just return the hash */
    if (i == 1)
      return 1;
    ++i;

    if (verbose_message_callback)
    {
      snprintf(message, sizeof(message), "Potential homebrew at sector %u, checking for KART data in track 2", sector);
      rc_hash_verbose(message);
    }

    track_handle = rc_cd_open_track(path, 2);
    if (!track_handle)
      return rc_hash_error("Could not open track");

    /* track 2 of the homebrew code has the 64 bytes or ATRI followed by 32 bytes of "ATARI APPROVED DATA HEADER ATRI!",
     * then 64 bytes of KART repeating. */
    sector = rc_cd_first_track_sector(track_handle);
    rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
    if (memcmp(&buffer[0x5E], "RT!IRTKA", 8) != 0)
      return rc_hash_error("Homebrew executable not found in track 2");

    /* found KART data*/
    if (verbose_message_callback)
    {
      snprintf(message, sizeof(message), "Found KART data in track 2");
      rc_hash_verbose(message);
    }

    offset = 0xA6;
    size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
  } while (1);
}

static int rc_hash_lynx(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'L' && buffer[1] == 'Y' && buffer[2] == 'N' && buffer[3] == 'X' && buffer[4] == 0)
  {
    rc_hash_verbose("Ignoring LYNX header");

    buffer += 64;
    buffer_size -= 64;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_neogeo_cd(char hash[33], const char* path)
{
  char buffer[1024], *ptr;
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* https://wiki.neogeodev.org/index.php?title=IPL_file, https://wiki.neogeodev.org/index.php?title=PRG_file
   * IPL file specifies data to be loaded before the game starts. PRG files are the executable code
   */
  sector = rc_cd_find_file_sector(track_handle, "IPL.TXT", &size);
  if (!sector)
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a NeoGeo CD game disc");
  }

  if (rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)) == 0)
  {
    rc_cd_close_track(track_handle);
    return 0;
  }

  md5_init(&md5);

  buffer[sizeof(buffer) - 1] = '\0';
  ptr = &buffer[0];
  do
  {
    char* start = ptr;
    while (*ptr && *ptr != '.')
      ++ptr;

    if (strncasecmp(ptr, ".PRG", 4) == 0)
    {
      ptr += 4;
      *ptr++ = '\0';

      sector = rc_cd_find_file_sector(track_handle, start, &size);
      if (!sector || !rc_hash_cd_file(&md5, track_handle, sector, NULL, size, start))
      {
        char error[128];
        rc_cd_close_track(track_handle);
        snprintf(error, sizeof(error), "Could not read %.16s", start);
        return rc_hash_error(error);
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
    if (*ptr != '\n')
      break;
    ++ptr;
  } while (*ptr != '\0' && *ptr != '\x1a');

  rc_cd_close_track(track_handle);
  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_nes(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A)
  {
    rc_hash_verbose("Ignoring NES header");

    buffer += 16;
    buffer_size -= 16;
  }
  else if (buffer[0] == 'F' && buffer[1] == 'D' && buffer[2] == 'S' && buffer[3] == 0x1A)
  {
      rc_hash_verbose("Ignoring FDS header");

      buffer += 16;
      buffer_size -= 16;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_n64(char hash[33], const char* path)
{
  uint8_t* buffer;
  uint8_t* stop;
  const size_t buffer_size = 65536;
  md5_state_t md5;
  size_t remaining;
  void* file_handle;
  int is_v64 = 0;
  int is_n64 = 0;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  buffer = (uint8_t*)malloc(buffer_size);
  if (!buffer)
  {
    rc_file_close(file_handle);
    return rc_hash_error("Could not allocate temporary buffer");
  }
  stop = buffer + buffer_size;

  /* read first byte so we can detect endianness */
  rc_file_seek(file_handle, 0, SEEK_SET);
  rc_file_read(file_handle, buffer, 1);

  if (buffer[0] == 0x80) /* z64 format (big endian [native]) */
  {
  }
  else if (buffer[0] == 0x37) /* v64 format (byteswapped) */
  {
    rc_hash_verbose("converting v64 to z64");
    is_v64 = 1;
  }
  else if (buffer[0] == 0x40) /* n64 format (little endian) */
  {
    rc_hash_verbose("converting n64 to z64");
    is_n64 = 1;
  }
  else if (buffer[0] == 0xE8 || buffer[0] == 0x22) /* ndd format (don't byteswap) */
  {
  }
  else
  {
    free(buffer);
    rc_file_close(file_handle);

    rc_hash_verbose("Not a Nintendo 64 ROM");
    return 0;
  }

  /* calculate total file size */
  rc_file_seek(file_handle, 0, SEEK_END);
  remaining = (size_t)rc_file_tell(file_handle);
  if (remaining > MAX_BUFFER_SIZE)
    remaining = MAX_BUFFER_SIZE;

  if (verbose_message_callback)
  {
    char message[64];
    snprintf(message, sizeof(message), "Hashing %u bytes", (unsigned)remaining);
    verbose_message_callback(message);
  }

  /* begin hashing */
  md5_init(&md5);

  rc_file_seek(file_handle, 0, SEEK_SET);
  while (remaining >= buffer_size)
  {
    rc_file_read(file_handle, buffer, (int)buffer_size);

    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)buffer_size);
    remaining -= buffer_size;
  }

  if (remaining > 0)
  {
    rc_file_read(file_handle, buffer, (int)remaining);

    stop = buffer + remaining;
    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)remaining);
  }

  /* cleanup */
  rc_file_close(file_handle);
  free(buffer);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_nintendo_ds(char hash[33], const char* path)
{
  uint8_t header[512];
  uint8_t* hash_buffer;
  uint32_t hash_size, arm9_size, arm9_addr, arm7_size, arm7_addr, icon_addr;
  size_t num_read;
  int64_t offset = 0;
  md5_state_t md5;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_SET);
  if (rc_file_read(file_handle, header, sizeof(header)) != 512)
    return rc_hash_error("Failed to read header");

  if (header[0] == 0x2E && header[1] == 0x00 && header[2] == 0x00 && header[3] == 0xEA &&
    header[0xB0] == 0x44 && header[0xB1] == 0x46 && header[0xB2] == 0x96 && header[0xB3] == 0)
  {
    /* SuperCard header detected, ignore it */
    rc_hash_verbose("Ignoring SuperCard header");

    offset = 512;
    rc_file_seek(file_handle, offset, SEEK_SET);
    rc_file_read(file_handle, header, sizeof(header));
  }

  arm9_addr = header[0x20] | (header[0x21] << 8) | (header[0x22] << 16) | (header[0x23] << 24);
  arm9_size = header[0x2C] | (header[0x2D] << 8) | (header[0x2E] << 16) | (header[0x2F] << 24);
  arm7_addr = header[0x30] | (header[0x31] << 8) | (header[0x32] << 16) | (header[0x33] << 24);
  arm7_size = header[0x3C] | (header[0x3D] << 8) | (header[0x3E] << 16) | (header[0x3F] << 24);
  icon_addr = header[0x68] | (header[0x69] << 8) | (header[0x6A] << 16) | (header[0x6B] << 24);

  if (arm9_size + arm7_size > 16 * 1024 * 1024)
  {
    /* sanity check - code blocks are typically less than 1MB each - assume not a DS ROM */
    snprintf((char*)header, sizeof(header), "arm9 code size (%u) + arm7 code size (%u) exceeds 16MB", arm9_size, arm7_size);
    return rc_hash_error((const char*)header);
  }

  hash_size = 0xA00;
  if (arm9_size > hash_size)
    hash_size = arm9_size;
  if (arm7_size > hash_size)
    hash_size = arm7_size;

  hash_buffer = (uint8_t*)malloc(hash_size);
  if (!hash_buffer)
  {
    rc_file_close(file_handle);

    snprintf((char*)header, sizeof(header), "Failed to allocate %u bytes", hash_size);
    return rc_hash_error((const char*)header);
  }

  md5_init(&md5);

  rc_hash_verbose("Hashing 352 byte header");
  md5_append(&md5, header, 0x160);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm9 code (at %08X)", arm9_size, arm9_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm9_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm9_size);
  md5_append(&md5, hash_buffer, arm9_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm7 code (at %08X)", arm7_size, arm7_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm7_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm7_size);
  md5_append(&md5, hash_buffer, arm7_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing 2560 byte icon and labels data (at %08X)", icon_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, icon_addr + offset, SEEK_SET);
  num_read = rc_file_read(file_handle, hash_buffer, 0xA00);
  if (num_read < 0xA00)
  {
    /* some homebrew games don't provide a full icon block, and no data after the icon block.
     * if we didn't get a full icon block, fill the remaining portion with 0s
     */
    if (verbose_message_callback)
    {
      snprintf((char*)header, sizeof(header), "Warning: only got %u bytes for icon and labels data, 0-padding to 2560 bytes", (unsigned)num_read);
      verbose_message_callback((const char*)header);
    }

    memset(&hash_buffer[num_read], 0, 0xA00 - num_read);
  }
  md5_append(&md5, hash_buffer, 0xA00);

  free(hash_buffer);
  rc_file_close(file_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_gamecube(char hash[33], const char* path)
{
  md5_state_t md5;
  void* file_handle;
  const uint32_t BASE_HEADER_SIZE = 0x2440;
  const uint32_t MAX_HEADER_SIZE = 1024 * 1024;
  uint32_t apploader_header_size, apploader_body_size, apploader_trailer_size, header_size;
  uint8_t quad_buffer[4];
  uint8_t addr_buffer[0xD8];
  uint8_t* buffer;
  uint32_t dol_offset;
  uint32_t dol_offsets[18];
  uint32_t dol_sizes[18];
  uint32_t dol_buf_size = 0;
  uint32_t ix;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  /* Verify Gamecube */
  rc_file_seek(file_handle, 0x1c, SEEK_SET);
  rc_file_read(file_handle, quad_buffer, 4);
  if (quad_buffer[0] != 0xC2|| quad_buffer[1] != 0x33 || quad_buffer[2] != 0x9F || quad_buffer[3] != 0x3D)
  {
    rc_file_close(file_handle);
    return rc_hash_error("Not a Gamecube disc");
  }

  /* GetApploaderSize */
  rc_file_seek(file_handle, BASE_HEADER_SIZE + 0x14, SEEK_SET);
  apploader_header_size = 0x20;
  rc_file_read(file_handle, quad_buffer, 4);
  apploader_body_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  rc_file_read(file_handle, quad_buffer, 4);
  apploader_trailer_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  header_size = BASE_HEADER_SIZE + apploader_header_size + apploader_body_size + apploader_trailer_size;
  if (header_size > MAX_HEADER_SIZE) header_size = MAX_HEADER_SIZE;

  /* Hash headers */
  buffer = (uint8_t*)malloc(header_size);
  if (!buffer)
  {
    rc_file_close(file_handle);
    return rc_hash_error("Could not allocate temporary buffer");
  }
  rc_file_seek(file_handle, 0, SEEK_SET);
  rc_file_read(file_handle, buffer, header_size);
  md5_init(&md5);
  if (verbose_message_callback)
  {
    char message[128];
    snprintf(message, sizeof(message), "Hashing %u byte header", header_size);
    verbose_message_callback(message);
  }
  md5_append(&md5, buffer, header_size);

  /* GetBootDOLOffset
   * Base header size is guaranteed larger than 0x423 therefore buffer contains dol_offset right now
   */
  dol_offset = (buffer[0x420] << 24) | (buffer[0x421] << 16) | (buffer[0x422] << 8) | buffer[0x423];
  free(buffer);

  /* Find offsetsand sizes for the 7 main.dol code segments and 11 main.dol data segments */
  rc_file_seek(file_handle, dol_offset, SEEK_SET);
  rc_file_read(file_handle, addr_buffer, 0xD8);
  for (ix = 0; ix < 18; ix++)
  {
    dol_offsets[ix] =
      (addr_buffer[0x0 + ix * 4] << 24) |
      (addr_buffer[0x1 + ix * 4] << 16) |
      (addr_buffer[0x2 + ix * 4] << 8) |
      addr_buffer[0x3 + ix * 4];
    dol_sizes[ix] =
      (addr_buffer[0x90 + ix * 4] << 24) |
      (addr_buffer[0x91 + ix * 4] << 16) |
      (addr_buffer[0x92 + ix * 4] << 8) |
      addr_buffer[0x93 + ix * 4];
    dol_buf_size = (dol_sizes[ix] > dol_buf_size) ? dol_sizes[ix] : dol_buf_size;
  }

  /* Iterate through the 18 main.dol segments and hash each */
  buffer = (uint8_t*)malloc(dol_buf_size);
  if (!buffer)
  {
    rc_file_close(file_handle);
    return rc_hash_error("Could not allocate temporary buffer");
  }
  for (ix = 0; ix < 18; ix++)
  {
    if (dol_sizes[ix] == 0)
      continue;
    rc_file_seek(file_handle, dol_offsets[ix], SEEK_SET);
    rc_file_read(file_handle, buffer, dol_sizes[ix]);
    if (verbose_message_callback)
    {
      char message[128];
      if (ix < 7)
        snprintf(message, sizeof(message), "Hashing %u byte main.dol code segment %u", dol_sizes[ix], ix);
      else
        snprintf(message, sizeof(message), "Hashing %u byte main.dol data segment %u", dol_sizes[ix], ix - 7);
      verbose_message_callback(message);
    }
    md5_append(&md5, buffer, dol_sizes[ix]);
  }

  /* Finalize */
  rc_file_close(file_handle);
  free(buffer);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_pce(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it (expect ROM data to be multiple of 128KB) */
  uint32_t calc_size = ((uint32_t)buffer_size / 0x20000) * 0x20000;
  if (buffer_size - calc_size == 512)
  {
    rc_hash_verbose("Ignoring PCE header");

    buffer += 512;
    buffer_size -= 512;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_pce_track(char hash[33], void* track_handle)
{
  uint8_t buffer[2048];
  md5_state_t md5;
  uint32_t sector, num_sectors;
  uint32_t size;

  /* the PC-Engine uses the second sector to specify boot information and program name.
   * the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
   * http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
   */
  if (rc_cd_read_sector(track_handle, rc_cd_first_track_sector(track_handle) + 1, buffer, 128) < 128)
  {
    return rc_hash_error("Not a PC Engine CD");
  }

  /* normal PC Engine CD will have a header block in sector 1 */
  if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
  {
    /* the title of the disc is the last 22 bytes of the header */
    md5_init(&md5);
    md5_append(&md5, &buffer[106], 22);

    if (verbose_message_callback)
    {
      char message[128];
      buffer[128] = '\0';
      snprintf(message, sizeof(message), "Found PC Engine CD, title=%.22s", &buffer[106]);
      verbose_message_callback(message);
    }

    /* the first three bytes specify the sector of the program data, and the fourth byte
     * is the number of sectors.
     */
    sector = (buffer[0] << 16) + (buffer[1] << 8) + buffer[2];
    num_sectors = buffer[3];

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %d sectors starting at sector %d", num_sectors, sector);
      verbose_message_callback(message);
    }

    sector += rc_cd_first_track_sector(track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE)
  {
    md5_init(&md5);
    while (size > sizeof(buffer))
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else
  {
    return rc_hash_error("Not a PC Engine CD");
  }

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_pce_cd(char hash[33], const char* path)
{
  int result;
  void* track_handle = rc_cd_open_track(path, RC_HASH_CDTRACK_FIRST_DATA);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  result = rc_hash_pce_track(hash, track_handle);

  rc_cd_close_track(track_handle);

  return result;
}

static int rc_hash_pcfx_cd(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;

  /* PC-FX executable can be in any track. Assume it's in the largest data track and check there first */
  track_handle = rc_cd_open_track(path, RC_HASH_CDTRACK_LARGEST);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* PC-FX CD will have a header marker in sector 0 */
  sector = rc_cd_first_track_sector(track_handle);
  rc_cd_read_sector(track_handle, sector, buffer, 32);
  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) != 0)
  {
    rc_cd_close_track(track_handle);

    /* not found in the largest data track, check track 2 */
    track_handle = rc_cd_open_track(path, 2);
    if (!track_handle)
      return rc_hash_error("Could not open track");

    sector = rc_cd_first_track_sector(track_handle);
    rc_cd_read_sector(track_handle, sector, buffer, 32);
  }

  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) == 0)
  {
    /* PC-FX boot header fills the first two sectors of the disc
     * https://bitbucket.org/trap15/pcfxtools/src/master/pcfx-cdlink.c
     * the important stuff is the first 128 bytes of the second sector (title being the first 32) */
    rc_cd_read_sector(track_handle, sector + 1, buffer, 128);

    md5_init(&md5);
    md5_append(&md5, buffer, 128);

    if (verbose_message_callback)
    {
      char message[128];
      buffer[128] = '\0';
      snprintf(message, sizeof(message), "Found PC-FX CD, title=%.32s", &buffer[0]);
      verbose_message_callback(message);
    }

    /* the program sector is in bytes 33-36 (assume byte 36 is 0) */
    sector = (buffer[34] << 16) + (buffer[33] << 8) + buffer[32];

    /* the number of sectors the program occupies is in bytes 37-40 (assume byte 40 is 0) */
    num_sectors = (buffer[38] << 16) + (buffer[37] << 8) + buffer[36];

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %d sectors starting at sector %d", num_sectors, sector);
      verbose_message_callback(message);
    }

    sector += rc_cd_first_track_sector(track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  else
  {
    int result = 0;
    rc_cd_read_sector(track_handle, sector + 1, buffer, 128);

    /* some PC-FX CDs still identify as PCE CDs */
    if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
      result = rc_hash_pce_track(hash, track_handle);

    rc_cd_close_track(track_handle);
    if (result)
      return result;

    return rc_hash_error("Not a PC-FX CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_dreamcast(char hash[33], const char* path)
{
  uint8_t buffer[256] = "";
  void* track_handle;
  char exe_file[32] = "";
  uint32_t size;
  uint32_t sector;
  int result = 0;
  md5_state_t md5;
  int i = 0;

  /* track 03 is the data track that contains the TOC and IP.BIN */
  track_handle = rc_cd_open_track(path, 3);
  if (track_handle)
  {
    /* first 256 bytes from first sector should have IP.BIN structure that stores game meta information
     * https://mc.pp.se/dc/ip.bin.html */
    rc_cd_read_sector(track_handle, rc_cd_first_track_sector(track_handle), buffer, sizeof(buffer));
  }

  if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
  {
    if (track_handle)
      rc_cd_close_track(track_handle);

    /* not a gd-rom dreamcast file. check for mil-cd by looking for the marker in the first data track */
    track_handle = rc_cd_open_track(path, RC_HASH_CDTRACK_FIRST_DATA);
    if (!track_handle)
      return rc_hash_error("Could not open track");

    rc_cd_read_sector(track_handle, rc_cd_first_track_sector(track_handle), buffer, sizeof(buffer));
    if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
    {
      /* did not find marker on track 3 or first data track */
      rc_cd_close_track(track_handle);
      return rc_hash_error("Not a Dreamcast CD");
    }
  }

  /* start the hash with the game meta information */
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)buffer, 256);

  if (verbose_message_callback)
  {
    char message[256];
    uint8_t* ptr = &buffer[0xFF];
    while (ptr > &buffer[0x80] && ptr[-1] == ' ')
      --ptr;
    *ptr = '\0';

    snprintf(message, sizeof(message), "Found Dreamcast CD: %.128s (%.16s)", (const char*)&buffer[0x80], (const char*)&buffer[0x40]);
    verbose_message_callback(message);
  }

  /* the boot filename is 96 bytes into the meta information (https://mc.pp.se/dc/ip0000.bin.html) */
  /* remove whitespace from bootfile */
  i = 0;
  while (!isspace((unsigned char)buffer[96 + i]) && i < 16)
    ++i;

  /* sometimes boot file isn't present on meta information.
   * nothing can be done, as even the core doesn't run the game in this case. */
  if (i == 0)
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Boot executable not specified on IP.BIN");
  }

  memcpy(exe_file, &buffer[96], i);
  exe_file[i] = '\0';

  sector = rc_cd_find_file_sector(track_handle, exe_file, &size);
  if (sector == 0)
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Could not locate boot executable");
  }

  if (rc_cd_read_sector(track_handle, sector, buffer, 1))
  {
    /* the boot executable is in the primary data track */
  }
  else
  {
    rc_cd_close_track(track_handle);

    /* the boot executable is normally in the last track */
    track_handle = rc_cd_open_track(path, RC_HASH_CDTRACK_LAST);
  }

  result = rc_hash_cd_file(&md5, track_handle, sector, NULL, size, "boot executable");
  rc_cd_close_track(track_handle);

  rc_hash_finalize(&md5, hash);
  return result;
}

static int rc_hash_find_playstation_executable(void* track_handle, const char* boot_key, const char* cdrom_prefix, 
                                               char exe_name[], uint32_t exe_name_size, uint32_t* exe_size)
{
  uint8_t buffer[2048];
  uint32_t size;
  char* ptr;
  char* start;
  const size_t boot_key_len = strlen(boot_key);
  const size_t cdrom_prefix_len = strlen(cdrom_prefix);
  int sector;

  sector = rc_cd_find_file_sector(track_handle, "SYSTEM.CNF", NULL);
  if (!sector)
    return 0;

  size = (uint32_t)rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer) - 1);
  buffer[size] = '\0';

  sector = 0;
  for (ptr = (char*)buffer; *ptr; ++ptr)
  {
    if (strncmp(ptr, boot_key, boot_key_len) == 0)
    {
      ptr += boot_key_len;
      while (isspace((unsigned char)*ptr))
        ++ptr;

      if (*ptr == '=')
      {
        ++ptr;
        while (isspace((unsigned char)*ptr))
          ++ptr;

        if (strncmp(ptr, cdrom_prefix, cdrom_prefix_len) == 0)
          ptr += cdrom_prefix_len;
        while (*ptr == '\\')
          ++ptr;

        start = ptr;
        while (!isspace((unsigned char)*ptr) && *ptr != ';')
          ++ptr;

        size = (uint32_t)(ptr - start);
        if (size >= exe_name_size)
          size = exe_name_size - 1;

        memcpy(exe_name, start, size);
        exe_name[size] = '\0';

        if (verbose_message_callback)
        {
          snprintf((char*)buffer, sizeof(buffer), "Looking for boot executable: %s", exe_name);
          verbose_message_callback((const char*)buffer);
        }

        sector = rc_cd_find_file_sector(track_handle, exe_name, exe_size);
        break;
      }
    }

    /* advance to end of line */
    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return sector;
}

static int rc_hash_psx(char hash[33], const char* path)
{
  uint8_t buffer[32];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  sector = rc_hash_find_playstation_executable(track_handle, "BOOT", "cdrom:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    sector = rc_cd_find_file_sector(track_handle, "PSX.EXE", &size);
    if (sector)
      memcpy(exe_name, "PSX.EXE", 8);
  }

  if (!sector)
  {
    rc_hash_error("Could not locate primary executable");
  }
  else if (rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_error("Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "PS-X EXE", 7) != 0)
    {
      if (verbose_message_callback)
      {
        char message[128];
        snprintf(message, sizeof(message), "%s did not contain PS-X EXE marker", exe_name);
        verbose_message_callback(message);
      }
    }
    else
    {
      /* the PS-X EXE header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
       * include the header itself. We want to include the header in the hash, so append another 2048 to that value.
       */
      size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(&md5, hash);
  }

  rc_cd_close_track(track_handle);

  return result;
}

static int rc_hash_ps2(char hash[33], const char* path)
{
  uint8_t buffer[4];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  sector = rc_hash_find_playstation_executable(track_handle, "BOOT2", "cdrom0:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    rc_hash_error("Could not locate primary executable");
  }
  else if (rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_error("Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "\x7f\x45\x4c\x46", 4) != 0)
    {
      if (verbose_message_callback)
      {
        char message[128];
        snprintf(message, sizeof(message), "%s did not contain ELF marker", exe_name);
        verbose_message_callback(message);
      }
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(&md5, hash);
  }

  rc_cd_close_track(track_handle);

  return result;
}

static int rc_hash_psp(char hash[33], const char* path)
{
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* http://www.romhacking.net/forum/index.php?topic=30899.0
   * PSP_GAME/PARAM.SFO contains key/value pairs identifying the game for the system (i.e. serial number,
   * name, version). PSP_GAME/SYSDIR/EBOOT.BIN is the encrypted primary executable.
   */
  sector = rc_cd_find_file_sector(track_handle, "PSP_GAME\\PARAM.SFO", &size);
  if (!sector)
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a PSP game disc");
  }

  md5_init(&md5);
  if (!rc_hash_cd_file(&md5, track_handle, sector, NULL, size, "PSP_GAME\\PARAM.SFO"))
  {
    rc_cd_close_track(track_handle);
    return 0;
  }

  sector = rc_cd_find_file_sector(track_handle, "PSP_GAME\\SYSDIR\\EBOOT.BIN", &size);
  if (!sector)
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Could not find primary executable");
  }

  if (!rc_hash_cd_file(&md5, track_handle, sector, NULL, size, "PSP_GAME\\SYSDIR\\EBOOT.BIN"))
  {
    rc_cd_close_track(track_handle);
    return 0;
  }

  rc_cd_close_track(track_handle);
  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_sega_cd(char hash[33], const char* path)
{
  uint8_t buffer[512];
  void* track_handle;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
   * After that is an arbitrary amount of code that ensures the game is being run in the correct region.
   * Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
   * primary executable is loaded. In many cases, a single game will have multiple executables, so even
   * if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
   * hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
   * that our players aren't modifying anything else on the disc.
   */
  rc_cd_read_sector(track_handle, 0, buffer, sizeof(buffer));
  rc_cd_close_track(track_handle);

  if (memcmp(buffer, "SEGADISCSYSTEM  ", 16) != 0 && /* Sega CD */
      memcmp(buffer, "SEGA SEGASATURN ", 16) != 0)   /* Sega Saturn */
  {
    return rc_hash_error("Not a Sega CD");
  }

  return rc_hash_buffer(hash, buffer, sizeof(buffer));
}

static int rc_hash_scv(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  /* https://gitlab.com/MaaaX-EmuSCV/libretro-emuscv/-/blob/master/readme.txt#L211 */
  if (memcmp(buffer, "EmuSCV", 6) == 0)
  {
    rc_hash_verbose("Ignoring SCV header");

    buffer += 32;
    buffer_size -= 32;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_snes(char hash[33], const uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  uint32_t calc_size = ((uint32_t)buffer_size / 0x2000) * 0x2000;
  if (buffer_size - calc_size == 512)
  {
    rc_hash_verbose("Ignoring SNES header");

    buffer += 512;
    buffer_size -= 512;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
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

void rc_file_seek_buffered_file(void* file_handle, int64_t offset, int origin)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  switch (origin)
  {
    case SEEK_SET: buffered_file->read_ptr = buffered_file->data + offset; break;
    case SEEK_CUR: buffered_file->read_ptr += offset; break;
    case SEEK_END: buffered_file->read_ptr = buffered_file->data + buffered_file->data_size + offset; break;
  }

  if (buffered_file->read_ptr < buffered_file->data)
    buffered_file->read_ptr = buffered_file->data;
  else if (buffered_file->read_ptr > buffered_file->data + buffered_file->data_size)
    buffered_file->read_ptr = buffered_file->data + buffered_file->data_size;
}

int64_t rc_file_tell_buffered_file(void* file_handle)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  return (buffered_file->read_ptr - buffered_file->data);
}

size_t rc_file_read_buffered_file(void* file_handle, void* buffer, size_t requested_bytes)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  const int64_t remaining = buffered_file->data_size - (buffered_file->read_ptr - buffered_file->data);
  if ((int)requested_bytes > remaining)
     requested_bytes = (int)remaining;

  memcpy(buffer, buffered_file->read_ptr, requested_bytes);
  buffered_file->read_ptr += requested_bytes;
  return requested_bytes;
}

void rc_file_close_buffered_file(void* file_handle)
{
  free(file_handle);
}

static int rc_hash_file_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size)
{
  struct rc_hash_filereader buffered_filereader_funcs;
  struct rc_hash_filereader* old_filereader = filereader;
  int result;

  memset(&buffered_filereader_funcs, 0, sizeof(buffered_filereader_funcs));
  buffered_filereader_funcs.open = rc_file_open_buffered_file;
  buffered_filereader_funcs.close = rc_file_close_buffered_file;
  buffered_filereader_funcs.read = rc_file_read_buffered_file;
  buffered_filereader_funcs.seek = rc_file_seek_buffered_file;
  buffered_filereader_funcs.tell = rc_file_tell_buffered_file;
  filereader = &buffered_filereader_funcs;

  rc_buffered_file.data = rc_buffered_file.read_ptr = buffer;
  rc_buffered_file.data_size = buffer_size;

  result = rc_hash_generate_from_file(hash, console_id, "[buffered file]");

  filereader = old_filereader;
  return result;
}

int rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size)
{
  switch (console_id)
  {
    default:
    {
      char message[128];
      snprintf(message, sizeof(message), "Unsupported console for buffer hash: %d", console_id);
      return rc_hash_error(message);
    }

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
      return rc_hash_buffer(hash, buffer, buffer_size);

    case RC_CONSOLE_ARDUBOY:
      /* https://en.wikipedia.org/wiki/Intel_HEX */
      return rc_hash_text(hash, buffer, buffer_size);

    case RC_CONSOLE_ATARI_7800:
      return rc_hash_7800(hash, buffer, buffer_size);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, buffer, buffer_size);

    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, buffer, buffer_size);

    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
      return rc_hash_pce(hash, buffer, buffer_size);

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return rc_hash_scv(hash, buffer, buffer_size);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, buffer, buffer_size);

    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_NINTENDO_3DS:
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_file_from_buffer(hash, console_id, buffer, buffer_size);
  }
}

static int rc_hash_whole_file(char hash[33], const char* path)
{
  md5_state_t md5;
  uint8_t* buffer;
  int64_t size;
  const size_t buffer_size = 65536;
  void* file_handle;
  size_t remaining;
  int result = 0;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Hashing first %u bytes (of %u bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Hashing %s (%u bytes)", rc_path_get_filename(path), (unsigned)size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    remaining = MAX_BUFFER_SIZE;
  else
    remaining = (size_t)size;

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    while (remaining >= buffer_size)
    {
      rc_file_read(file_handle, buffer, (int)buffer_size);
      md5_append(&md5, buffer, (int)buffer_size);
      remaining -= buffer_size;
    }

    if (remaining > 0)
    {
      rc_file_read(file_handle, buffer, (int)remaining);
      md5_append(&md5, buffer, (int)remaining);
    }

    free(buffer);
    result = rc_hash_finalize(&md5, hash);
  }

  rc_file_close(file_handle);
  return result;
}

static int rc_hash_buffered_file(char hash[33], uint32_t console_id, const char* path)
{
  uint8_t* buffer;
  int64_t size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Buffering first %u bytes (of %d bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Buffering %s (%d bytes)", rc_path_get_filename(path), (unsigned)size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  buffer = (uint8_t*)malloc((size_t)size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    rc_file_read(file_handle, buffer, (int)size);

    result = rc_hash_generate_from_buffer(hash, console_id, buffer, (size_t)size);

    free(buffer);
  }

  rc_file_close(file_handle);
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
  while (*path)
  {
    if (path[0] == ':' && path[1] == '/')
      return 1;

    ++path;
  }

  return 0;
}

static const char* rc_hash_get_first_item_from_playlist(const char* path)
{
  char buffer[1024];
  char* disc_path;
  char* ptr, *start, *next;
  size_t num_read, path_len, file_len;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
  {
    rc_hash_error("Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(file_handle);

  ptr = start = buffer;
  do
  {
    /* ignore empty and commented lines */
    while (*ptr == '#' || *ptr == '\r' || *ptr == '\n')
    {
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

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Extracted %.*s from playlist", (int)file_len, start);
    verbose_message_callback(message);
  }

  start[file_len++] = '\0';
  if (rc_hash_path_is_absolute(start))
    path_len = 0;
  else
    path_len = rc_path_get_filename(path) - path;

  disc_path = (char*)malloc(path_len + file_len + 1);
  if (!disc_path)
    return NULL;

  if (path_len)
    memcpy(disc_path, path, path_len);

  memcpy(&disc_path[path_len], start, file_len);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], uint32_t console_id, const char* path)
{
  int result;
  const char* disc_path;

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Processing playlist: %s", rc_path_get_filename(path));
    verbose_message_callback(message);
  }

  disc_path = rc_hash_get_first_item_from_playlist(path);
  if (!disc_path)
    return rc_hash_error("Failed to get first item from playlist");

  result = rc_hash_generate_from_file(hash, console_id, disc_path);

  free((void*)disc_path);
  return result;
}

int rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path)
{
  switch (console_id)
  {
    default:
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "Unsupported console for file hash: %d", console_id);
      return rc_hash_error(buffer);
    }

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
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, path);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_whole_file(hash, path);

    case RC_CONSOLE_ARDUBOY:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_SUPER_CASSETTEVISION:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, path);

    case RC_CONSOLE_3DO:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_3do(hash, path);

    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, path);

    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return rc_hash_jaguar_cd(hash, path);

    case RC_CONSOLE_DREAMCAST:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_dreamcast(hash, path);

    case RC_CONSOLE_GAMECUBE:
      return rc_hash_gamecube(hash, path);

    case RC_CONSOLE_MS_DOS:
      return rc_hash_ms_dos(hash, path);

    case RC_CONSOLE_NEO_GEO_CD:
      return rc_hash_neogeo_cd(hash, path);

    case RC_CONSOLE_NINTENDO_64:
      return rc_hash_n64(hash, path);

    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_nintendo_ds(hash, path);

    case RC_CONSOLE_PC_ENGINE_CD:
      if (rc_path_compare_extension(path, "cue") || rc_path_compare_extension(path, "chd"))
        return rc_hash_pce_cd(hash, path);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_buffered_file(hash, console_id, path);

    case RC_CONSOLE_PCFX:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_pcfx_cd(hash, path);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_psx(hash, path);

    case RC_CONSOLE_PLAYSTATION_2:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_ps2(hash, path);

    case RC_CONSOLE_PSP:
      return rc_hash_psp(hash, path);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_sega_cd(hash, path);
  }
}

static void rc_hash_iterator_append_console(struct rc_hash_iterator* iterator, uint8_t console_id)
{
  int i = 0;
  while (iterator->consoles[i] != 0)
  {
    if (iterator->consoles[i] == console_id)
      return;

    ++i;
  }

  iterator->consoles[i] = console_id;
}

static void rc_hash_initialize_dsk_iterator(struct rc_hash_iterator* iterator, const char* path)
{
  size_t size = iterator->buffer_size;
  if (size == 0)
  {
    /* attempt to use disk size to determine system */
    void* file = rc_file_open(path);
    if (file)
    {
      rc_file_seek(file, 0, SEEK_END);
      size = (size_t)rc_file_tell(file);
      rc_file_close(file);
    }
  }

  if (size == 512 * 9 * 80) /* 360KB */
  {
    /* FAT-12 3.5" DD (512 byte sectors, 9 sectors per track, 80 tracks per side */
    /* FAT-12 5.25" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 80 * 2) /* 720KB */
  {
    /* FAT-12 3.5" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 40) /* 180KB */
  {
    /* FAT-12 5.25" DD (512 byte sectors, 9 sectors per track, 40 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;

    /* AMSDOS 3" - 40 tracks */
    iterator->consoles[1] = RC_CONSOLE_AMSTRAD_PC;
  }
  else if (size == 256 * 16 * 35) /* 140KB */
  {
    /* Apple II new format - 256 byte sectors, 16 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }
  else if (size == 256 * 13 * 35) /* 113.75KB */
  {
    /* Apple II old format - 256 byte sectors, 13 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }

  /* once a best guess has been identified, make sure the others are added as fallbacks */

  /* check MSX first, as Apple II isn't supported by RetroArch, and RAppleWin won't use the iterator */
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_MSX);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_AMSTRAD_PC);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_APPLE_II);
}

void rc_hash_initialize_iterator(struct rc_hash_iterator* iterator, const char* path, const uint8_t* buffer, size_t buffer_size)
{
  int need_path = !buffer;

  memset(iterator, 0, sizeof(*iterator));
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  iterator->consoles[0] = 0;

  do
  {
    const char* ext = rc_path_get_extension(path);
    switch (tolower(*ext))
    {
      case '2':
        if (rc_path_compare_extension(ext, "2d"))
        {
          iterator->consoles[0] = RC_CONSOLE_SHARPX1;
        }
        break;

      case '3':
        if (rc_path_compare_extension(ext, "3ds") ||
            rc_path_compare_extension(ext, "3dsx"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_3DS;
        }
        break;

      case '7':
        if (rc_path_compare_extension(ext, "7z"))
        {
          /* decompressing zip file not supported */
          iterator->consoles[0] = RC_CONSOLE_ARCADE;
          need_path = 1;
        }
        break;

      case '8':
        /* http://tibasicdev.wikidot.com/file-extensions */
        if (rc_path_compare_extension(ext, "83g") ||
            rc_path_compare_extension(ext, "83p"))
        {
          iterator->consoles[0] = RC_CONSOLE_TI83;
        }
        break;

      case 'a':
        if (rc_path_compare_extension(ext, "a78"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_7800;
        }
        else if (rc_path_compare_extension(ext, "app") ||
                 rc_path_compare_extension(ext, "axf"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_3DS;
        }
        break;

      case 'b':
        if (rc_path_compare_extension(ext, "bin"))
        {
           if (buffer_size == 0)
           {
              /* raw bin file may be a CD track. if it's more than 32MB, try a CD hash. */
              void* file = rc_file_open(path);
              if (file)
              {
                 int64_t size;

                 rc_file_seek(file, 0, SEEK_END);
                 size = rc_file_tell(file);
                 rc_file_close(file);

                 if (size > 32 * 1024 * 1024)
                 {
                    iterator->consoles[0] = RC_CONSOLE_3DO; /* 4DO supports directly opening the bin file */
                    iterator->consoles[1] = RC_CONSOLE_PLAYSTATION; /* PCSX ReARMed supports directly opening the bin file*/
                    iterator->consoles[2] = RC_CONSOLE_PLAYSTATION_2; /* PCSX2 supports directly opening the bin file*/
                    iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* Genesis Plus GX supports directly opening the bin file*/

                    /* fallback to megadrive which just does a full hash. */
                    iterator->consoles[4] = RC_CONSOLE_MEGA_DRIVE;
                    break;
                 }
              }
           }

          /* bin is associated with MegaDrive, Sega32X, Atari 2600, Watara Supervision, MegaDuck,
           * Fairchild Channel F, Arcadia 2001, Interton VC 4000, and Super Cassette Vision.
           * Since they all use the same hashing algorithm, only specify one of them */
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        else if (rc_path_compare_extension(ext, "bs"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        break;

      case 'c':
        if (rc_path_compare_extension(ext, "cue"))
        {
          iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
          iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
          iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
          iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
          iterator->consoles[4] = RC_CONSOLE_PC_ENGINE_CD;
          iterator->consoles[5] = RC_CONSOLE_3DO;
          iterator->consoles[6] = RC_CONSOLE_PCFX;
          iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
          iterator->consoles[8] = RC_CONSOLE_ATARI_JAGUAR_CD;
          need_path = 1;
        }
        else if (rc_path_compare_extension(ext, "chd"))
        {
          iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
          iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
          iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
          iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
          iterator->consoles[4] = RC_CONSOLE_PSP;
          iterator->consoles[5] = RC_CONSOLE_PC_ENGINE_CD;
          iterator->consoles[6] = RC_CONSOLE_3DO;
          iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
          iterator->consoles[8] = RC_CONSOLE_PCFX;
          need_path = 1;
        }
        else if (rc_path_compare_extension(ext, "col"))
        {
          iterator->consoles[0] = RC_CONSOLE_COLECOVISION;
        }
        else if (rc_path_compare_extension(ext, "cas"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        else if (rc_path_compare_extension(ext, "chf"))
        {
          iterator->consoles[0] = RC_CONSOLE_FAIRCHILD_CHANNEL_F;
        }
        else if (rc_path_compare_extension(ext, "cart"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_CASSETTEVISION;
        }
        else if (rc_path_compare_extension(ext, "cci") ||
                 rc_path_compare_extension(ext, "cia") ||
                 rc_path_compare_extension(ext, "cxi"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_3DS;
        }
        break;

      case 'd':
        if (rc_path_compare_extension(ext, "dsk"))
        {
          rc_hash_initialize_dsk_iterator(iterator, path);
        }
        else if (rc_path_compare_extension(ext, "d64"))
        {
          iterator->consoles[0] = RC_CONSOLE_COMMODORE_64;
        }
        else if (rc_path_compare_extension(ext, "d88"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC8800;
          iterator->consoles[1] = RC_CONSOLE_SHARPX1;
        }
        else if (rc_path_compare_extension(ext, "dosz"))
        {
            iterator->consoles[0] = RC_CONSOLE_MS_DOS;
        }
        break;

      case 'e':
        if (rc_path_compare_extension(ext, "elf"))
        {
          /* This should probably apply to more consoles in the future */
          /* Although in any case this just hashes the entire file */
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_3DS;
        }
        break;

      case 'f':
        if (rc_path_compare_extension(ext, "fig"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "fds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "fd"))
        {
          iterator->consoles[0] = RC_CONSOLE_THOMSONTO8; /* disk */
        }
        break;

      case 'g':
        if (rc_path_compare_extension(ext, "gba"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_ADVANCE;
        }
        else if (rc_path_compare_extension(ext, "gbc"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_COLOR;
        }
        else if (rc_path_compare_extension(ext, "gb"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
        }
        else if (rc_path_compare_extension(ext, "gg"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAME_GEAR;
        }
        else if (rc_path_compare_extension(ext, "gdi"))
        {
          iterator->consoles[0] = RC_CONSOLE_DREAMCAST;
        }
        break;

      case 'h':
        if (rc_path_compare_extension(ext, "hex"))
        {
          iterator->consoles[0] = RC_CONSOLE_ARDUBOY;
        }
        break;

      case 'i':
        if (rc_path_compare_extension(ext, "iso"))
        {
          iterator->consoles[0] = RC_CONSOLE_PLAYSTATION_2;
          iterator->consoles[1] = RC_CONSOLE_PSP;
          iterator->consoles[2] = RC_CONSOLE_3DO;
          iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
          need_path = 1;
        }
        break;

      case 'j':
        if (rc_path_compare_extension(ext, "jag"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_JAGUAR;
        }
        break;

      case 'k':
        if (rc_path_compare_extension(ext, "k7"))
        {
          iterator->consoles[0] = RC_CONSOLE_THOMSONTO8; /* tape */
        }
        break;

      case 'l':
        if (rc_path_compare_extension(ext, "lnx"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_LYNX;
        }
        break;

      case 'm':
        if (rc_path_compare_extension(ext, "m3u"))
        {
          const char* disc_path = rc_hash_get_first_item_from_playlist(path);
          if (!disc_path) /* did not find a disc */
            return;

          iterator->buffer = NULL; /* ignore buffer; assume it's the m3u contents */

          path = iterator->path = disc_path;
          continue; /* retry with disc_path */
        }
        else if (rc_path_compare_extension(ext, "md"))
        {
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        else if (rc_path_compare_extension(ext, "min"))
        {
          iterator->consoles[0] = RC_CONSOLE_POKEMON_MINI;
        }
        else if (rc_path_compare_extension(ext, "mx1"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        else if (rc_path_compare_extension(ext, "mx2"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        else if (rc_path_compare_extension(ext, "m5"))
        {
          iterator->consoles[0] = RC_CONSOLE_THOMSONTO8; /* cartridge */
        }
        else if (rc_path_compare_extension(ext, "m7"))
        {
          iterator->consoles[0] = RC_CONSOLE_THOMSONTO8; /* cartridge */
        }
        break;

      case 'n':
        if (rc_path_compare_extension(ext, "nes"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "nds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_DS; /* ASSERT: handles both DS and DSi */
        }
        else if (rc_path_compare_extension(ext, "n64") ||
                 rc_path_compare_extension(ext, "ndd"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_64;
        }
        else if (rc_path_compare_extension(ext, "ngc"))
        {
          iterator->consoles[0] = RC_CONSOLE_NEOGEO_POCKET;
        }
        else if (rc_path_compare_extension(ext, "nib"))
        {
            /* also Apple II, but both are full-file hashes */
            iterator->consoles[0] = RC_CONSOLE_COMMODORE_64;
        }
        break;

      case 'p':
        if (rc_path_compare_extension(ext, "pce"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        else if (rc_path_compare_extension(ext, "pgm"))
        {
          iterator->consoles[0] = RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER;
        }
        break;

      case 'r':
        if (rc_path_compare_extension(ext, "rom"))
        {
          /* rom is associated with MSX, Thomson TO-8, and Fairchild Channel F.
           * Since they all use the same hashing algorithm, only specify one of them */
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        if (rc_path_compare_extension(ext, "ri"))
        {
          iterator->consoles[0] = RC_CONSOLE_MSX;
        }
        break;

      case 's':
        if (rc_path_compare_extension(ext, "smc") ||
            rc_path_compare_extension(ext, "sfc") ||
            rc_path_compare_extension(ext, "swc"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "sg"))
        {
          iterator->consoles[0] = RC_CONSOLE_SG1000;
        }
        else if (rc_path_compare_extension(ext, "sgx"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        else if (rc_path_compare_extension(ext, "sv"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPERVISION;
        }
        else if (rc_path_compare_extension(ext, "sap"))
        {
          iterator->consoles[0] = RC_CONSOLE_THOMSONTO8; /* disk */
        }
        break;

      case 't':
        if (rc_path_compare_extension(ext, "tap"))
        {
          iterator->consoles[0] = RC_CONSOLE_ORIC;
        }
        else if (rc_path_compare_extension(ext, "tic"))
        {
          iterator->consoles[0] = RC_CONSOLE_TIC80;
        }
        else if (rc_path_compare_extension(ext, "tvc"))
        {
          iterator->consoles[0] = RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER;
        }
        break;

      case 'u':
        if (rc_path_compare_extension(ext, "uze"))
        {
          iterator->consoles[0] = RC_CONSOLE_UZEBOX;
        }
        break;

      case 'v':
        if (rc_path_compare_extension(ext, "vb"))
        {
          iterator->consoles[0] = RC_CONSOLE_VIRTUAL_BOY;
        }
        else if (rc_path_compare_extension(ext, "v64"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_64;
        }
        break;

      case 'w':
        if (rc_path_compare_extension(ext, "wsc"))
        {
          iterator->consoles[0] = RC_CONSOLE_WONDERSWAN;
        }
        else if (rc_path_compare_extension(ext, "wasm"))
        {
          iterator->consoles[0] = RC_CONSOLE_WASM4;
        }
        else if (rc_path_compare_extension(ext, "woz"))
        {
          iterator->consoles[0] = RC_CONSOLE_APPLE_II;
        }
        break;

      case 'z':
        if (rc_path_compare_extension(ext, "zip"))
        {
          /* decompressing zip file not supported */
          iterator->consoles[0] = RC_CONSOLE_ARCADE;
          need_path = 1;
        }
        else if (rc_path_compare_extension(ext, "z64"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_64;
        }
        break;
    }

    if (verbose_message_callback)
    {
      char message[256];
      int count = 0;
      while (iterator->consoles[count])
        ++count;

      snprintf(message, sizeof(message), "Found %d potential consoles for %s file extension", count, ext);
      verbose_message_callback(message);
    }

    /* loop is only for specific cases that redirect to another file - like m3u */
    break;
  } while (1);

  if (need_path && !iterator->path)
    iterator->path = strdup(path);

  /* if we didn't match the extension, default to something that does a whole file hash */
  if (!iterator->consoles[0])
    iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
}

void rc_hash_destroy_iterator(struct rc_hash_iterator* iterator)
{
  if (iterator->path)
  {
    free((void*)iterator->path);
    iterator->path = NULL;
  }
}

int rc_hash_iterate(char hash[33], struct rc_hash_iterator* iterator)
{
  int next_console;
  int result = 0;

  do
  {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0)
    {
      hash[0] = '\0';
      break;
    }

    ++iterator->index;

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Trying console %d", next_console);
      verbose_message_callback(message);
    }

    if (iterator->buffer)
      result = rc_hash_generate_from_buffer(hash, next_console, iterator->buffer, iterator->buffer_size);
    else
      result = rc_hash_generate_from_file(hash, next_console, iterator->path);

  } while (!result);

  return result;
}
