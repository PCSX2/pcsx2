/*
 * LZ4 file library
 * Copyright (c) Yann Collet and LZ4 contributors. All rights reserved.
 *
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at :
 * - LZ4 homepage : http://www.lz4.org
 * - LZ4 source repository : https://github.com/lz4/lz4
 */
#include <stdlib.h>  /* malloc, free */
#include <string.h>
#include <assert.h>
#include "lz4.h"
#include "lz4file.h"

/* =====   Error Handling   ===== */
static LZ4F_errorCode_t returnErrorCode(LZ4F_errorCodes code)
{
    return (LZ4F_errorCode_t)-(ptrdiff_t)code;
}

#undef RETURN_ERROR
#define RETURN_ERROR(e) return returnErrorCode(LZ4F_ERROR_ ## e)

/* =====    Read API Implementation    ===== */

struct LZ4_readFile_s {
  LZ4F_dctx* dctxPtr;
  FILE* fp;
  LZ4_byte* srcBuf;
  size_t srcBufNext;
  size_t srcBufSize;
  size_t srcBufMaxSize;
};

static void freeReadFileResources(LZ4_readFile_t* lz4fRead)
{
  if (lz4fRead==NULL) return;
  LZ4F_freeDecompressionContext(lz4fRead->dctxPtr);
  free(lz4fRead->srcBuf);
  free(lz4fRead);
}

static void freeAndNullReadFile(LZ4_readFile_t** statePtr)
{
  assert(statePtr != NULL);
  freeReadFileResources(*statePtr);
  *statePtr = NULL;
}

static LZ4F_errorCode_t readAndParseHeader(LZ4_readFile_t* readFile, FILE* fp)
{
    char headerBuf[LZ4F_HEADER_SIZE_MAX];
    LZ4F_frameInfo_t frameInfo;
    size_t consumedSize;

    /* Read the header from file */
    const size_t bytesRead = fread(headerBuf, 1, sizeof(headerBuf), fp);
    if (bytesRead < LZ4F_HEADER_SIZE_MIN + LZ4F_ENDMARK_SIZE) {
        RETURN_ERROR(io_read);
    }

    /* Parse frame information */
    consumedSize = bytesRead;
    { const LZ4F_errorCode_t result = LZ4F_getFrameInfo(readFile->dctxPtr, &frameInfo, headerBuf, &consumedSize);
      if (LZ4F_isError(result)) {
          return result;
    } }

    /* Determine buffer size based on block size */
    { const size_t blockSize = LZ4F_getBlockSize(frameInfo.blockSizeID);
      if (blockSize == 0) {
          RETURN_ERROR(maxBlockSize_invalid);
      }
      readFile->srcBufMaxSize = blockSize;
    }

    /* Allocate source buffer */
    assert(readFile->srcBuf == NULL); /* Should be NULL from calloc */
    readFile->srcBuf = (LZ4_byte*)malloc(readFile->srcBufMaxSize);
    if (readFile->srcBuf == NULL) {
        RETURN_ERROR(allocation_failed);
    }

    /* Store remaining header data in buffer */
    readFile->srcBufSize = bytesRead - consumedSize;
    if (readFile->srcBufSize > 0) {
        memcpy(readFile->srcBuf, headerBuf + consumedSize, readFile->srcBufSize);
    }
    readFile->srcBufNext = 0;

    return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4F_readOpen(LZ4_readFile_t** lz4fRead, FILE* fp)
{
    LZ4_readFile_t* readFile;

    /* Validate parameters */
    if (fp == NULL || lz4fRead == NULL) {
        RETURN_ERROR(parameter_null);
    }

    /* Allocate read file structure */
    readFile = (LZ4_readFile_t*)calloc(1, sizeof(LZ4_readFile_t));
    if (readFile == NULL) {
        RETURN_ERROR(allocation_failed);
    }

    readFile->fp = fp;

    /* Initialize decompression context */
    { LZ4F_errorCode_t const result = LZ4F_createDecompressionContext(&readFile->dctxPtr, LZ4F_VERSION);
      if (LZ4F_isError(result)) {
          freeAndNullReadFile(&readFile);
          return result;
    } }

    /* Read and parse the header */
    { LZ4F_errorCode_t const result = readAndParseHeader(readFile, fp);
      if (LZ4F_isError(result)) {
          freeAndNullReadFile(&readFile);
          return result;
    } }

    *lz4fRead = readFile;
    return LZ4F_OK_NoError;
}

size_t LZ4F_read(LZ4_readFile_t* lz4fRead, void* buf, size_t size)
{
  LZ4_byte* outPtr = (LZ4_byte*)buf;
  size_t totalBytesRead = 0;

  if (lz4fRead == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (totalBytesRead < size) {
    size_t srcBytes = lz4fRead->srcBufSize - lz4fRead->srcBufNext;
    size_t dstBytes = size - totalBytesRead;

    if (srcBytes == 0) {
      size_t const bytesRead = fread(lz4fRead->srcBuf, 1, lz4fRead->srcBufMaxSize, lz4fRead->fp);
      if (bytesRead == 0) {
        if (ferror(lz4fRead->fp)) {
          RETURN_ERROR(io_read);
        }
        break; /* end of input reached */
      }
      /* success: ret > 0*/
      lz4fRead->srcBufSize = bytesRead;
      srcBytes = lz4fRead->srcBufSize;
      lz4fRead->srcBufNext = 0;
    }

    { size_t const decStatus = LZ4F_decompress(
                          lz4fRead->dctxPtr,
                          outPtr, &dstBytes,
                          lz4fRead->srcBuf + lz4fRead->srcBufNext,
                          &srcBytes,
                          NULL);
      if (LZ4F_isError(decStatus)) {
          return decStatus;
    } }

    lz4fRead->srcBufNext += srcBytes;
    totalBytesRead += dstBytes;
    outPtr += dstBytes;
  }

  return totalBytesRead;
}

LZ4F_errorCode_t LZ4F_readClose(LZ4_readFile_t* lz4fRead)
{
  if (lz4fRead == NULL)
    RETURN_ERROR(parameter_null);
  freeReadFileResources(lz4fRead);
  return LZ4F_OK_NoError;
}

/* =====   write API   ===== */

struct LZ4_writeFile_s {
  LZ4F_cctx* cctxPtr;
  FILE* fp;
  LZ4_byte* dstBuf;
  size_t maxWriteSize;
  size_t dstBufMaxSize;
  LZ4F_errorCode_t errCode;
};

static void freeWriteFileResources(LZ4_writeFile_t* state)
{
  if (state == NULL) return;
  LZ4F_freeCompressionContext(state->cctxPtr);
  free(state->dstBuf);
  free(state);
}

static void freeAndNullWriteFile(LZ4_writeFile_t** statePtr)
{
  assert(statePtr != NULL);
  freeWriteFileResources(*statePtr);
  *statePtr = NULL;
}

static LZ4F_errorCode_t writeHeader(LZ4_writeFile_t* writeFile,
                  FILE* fp,
            const LZ4F_preferences_t* prefsPtr)
{
  LZ4_byte headerBuf[LZ4F_HEADER_SIZE_MAX];

  /* Generate header */
  LZ4F_errorCode_t const headerSize = LZ4F_compressBegin(writeFile->cctxPtr,
                                  headerBuf, LZ4F_HEADER_SIZE_MAX, prefsPtr);
  if (LZ4F_isError(headerSize)) {
    return headerSize;
  }

  /* Write header to file */
  if (headerSize != fwrite(headerBuf, 1, headerSize, fp)) {
    RETURN_ERROR(io_write);
  }

  return LZ4F_OK_NoError;
}

LZ4F_errorCode_t LZ4F_writeOpen(LZ4_writeFile_t** lz4fWrite, FILE* fp, const LZ4F_preferences_t* prefsPtr)
{
  LZ4_writeFile_t* writeFile;
  size_t blockSize;

  if (fp == NULL || lz4fWrite == NULL)
      RETURN_ERROR(parameter_null);

  /* Validate block size */
  { LZ4F_blockSizeID_t const blockSizeID = prefsPtr ? prefsPtr->frameInfo.blockSizeID : LZ4F_default;
    blockSize = LZ4F_getBlockSize(blockSizeID);
    if (blockSize == 0) {
        RETURN_ERROR(maxBlockSize_invalid);
  } }

  /* Allocate write file structure */
  writeFile = (LZ4_writeFile_t*)calloc(1, sizeof(LZ4_writeFile_t));
  if (writeFile == NULL) {
    RETURN_ERROR(allocation_failed);
  }
  writeFile->fp = fp;
  writeFile->errCode = LZ4F_OK_NoError;
  writeFile->maxWriteSize = blockSize;

  /* Calculate and allocate destination buffer */
  writeFile->dstBufMaxSize = LZ4F_compressBound(blockSize, prefsPtr);
  writeFile->dstBuf = (LZ4_byte*)malloc(writeFile->dstBufMaxSize);
  if (writeFile->dstBuf == NULL) {
    freeAndNullWriteFile(&writeFile);
    RETURN_ERROR(allocation_failed);
  }

  /* Initialize compression context */
  { LZ4F_errorCode_t const status = LZ4F_createCompressionContext(&writeFile->cctxPtr, LZ4F_VERSION);
    if (LZ4F_isError(status)) {
        freeAndNullWriteFile(lz4fWrite);
        return status;
  } }

    /* Write header to file */
  { LZ4F_errorCode_t const writeStatus = writeHeader(writeFile, fp, prefsPtr);
    if (LZ4F_isError(writeStatus)) {
        freeAndNullWriteFile(&writeFile);
        return writeStatus;
  } }

  *lz4fWrite = writeFile;
  return LZ4F_OK_NoError;
}

size_t LZ4F_write(LZ4_writeFile_t* lz4fWrite, const void* buf, size_t size)
{
  const LZ4_byte* p = (const LZ4_byte*)buf;
  size_t remainingBytes = size;

  /* Validate parameters */
  if (lz4fWrite == NULL || buf == NULL)
    RETURN_ERROR(parameter_null);

  while (remainingBytes) {
    size_t const chunkSize = (remainingBytes > lz4fWrite->maxWriteSize) ? lz4fWrite->maxWriteSize : remainingBytes;

    /* Compress and write chunk */
    size_t cSize = LZ4F_compressUpdate(lz4fWrite->cctxPtr,
                              lz4fWrite->dstBuf, lz4fWrite->dstBufMaxSize,
                              p, chunkSize,
                              NULL);
    if (LZ4F_isError(cSize)) {
      lz4fWrite->errCode = cSize;
      return cSize;
    }

    if (cSize != fwrite(lz4fWrite->dstBuf, 1, cSize, lz4fWrite->fp)) {
      lz4fWrite->errCode = returnErrorCode(LZ4F_ERROR_io_write);
      RETURN_ERROR(io_write);
    }

    /* Update positions */
    p += chunkSize;
    remainingBytes -= chunkSize;
  }

  return size;
}

LZ4F_errorCode_t LZ4F_writeClose(LZ4_writeFile_t* lz4fWrite)
{
  LZ4F_errorCode_t ret = LZ4F_OK_NoError;

  if (lz4fWrite == NULL) {
    RETURN_ERROR(parameter_null);
  }

  if (lz4fWrite->errCode == LZ4F_OK_NoError) {
    ret =  LZ4F_compressEnd(lz4fWrite->cctxPtr,
                            lz4fWrite->dstBuf, lz4fWrite->dstBufMaxSize,
                            NULL);
    if (LZ4F_isError(ret)) {
      goto cleanup;
    }

    if (ret != fwrite(lz4fWrite->dstBuf, 1, ret, lz4fWrite->fp)) {
      ret = returnErrorCode(LZ4F_ERROR_io_write);
    }
  }

cleanup:
  freeWriteFileResources(lz4fWrite);
  return ret;
}
