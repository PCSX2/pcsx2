#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "lz4.h"

const char source[] =
  "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod\n"
  "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim\n"
  "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea\n"
  "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate\n"
  "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat\n"
  "cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id\n"
  "est laborum.\n"
  "\n"
  "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium\n"
  "doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore\n"
  "veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim\n"
  "ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia\n"
  "consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque\n"
  "porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur,\n"
  "adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore\n"
  "et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis\n"
  "nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid\n"
  "ex ea commodi consequatur? Quis autem vel eum iure reprehenderit qui in ea\n"
  "voluptate velit esse quam nihil molestiae consequatur, vel illum qui\n"
  "dolorem eum fugiat quo voluptas nulla pariatur?\n";

#define BUFFER_SIZE 2048

int main(void)
{
  int srcLen = (int)strlen(source);
  size_t const smallSize = 1024;
  size_t const largeSize = 64 * 1024 - 1;
  char cmpBuffer[BUFFER_SIZE];
  char* const buffer = (char*)malloc(BUFFER_SIZE + largeSize);
  char* outBuffer = buffer + largeSize;
  char* const dict = (char*)malloc(largeSize);
  char* const largeDict = dict;
  char* const smallDict = dict + largeSize - smallSize;
  int i;
  int cmpSize;

  printf("starting test decompress-partial-usingDict : \n");
  assert(buffer != NULL);
  assert(dict != NULL);

  cmpSize = LZ4_compress_default(source, cmpBuffer, srcLen, BUFFER_SIZE);

  for (i = cmpSize; i < cmpSize + 10; ++i) {
    int result = LZ4_decompress_safe_partial_usingDict(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE, NULL, 0);
    if ( (result < 0)
      || (result != srcLen)
      || memcmp(source, outBuffer, (size_t)srcLen) ) {
      printf("test decompress-partial-usingDict with no dict error \n");
      return -1;
    }
  }

  for (i = cmpSize; i < cmpSize + 10; ++i) {
    int result = LZ4_decompress_safe_partial_usingDict(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE, outBuffer - smallSize, smallSize);
    if ( (result < 0)
      || (result != srcLen)
      || memcmp(source, outBuffer, (size_t)srcLen) ) {
      printf("test decompress-partial-usingDict with small prefix error \n");
      return -1;
    }
  }

  for (i = cmpSize; i < cmpSize + 10; ++i) {
    int result = LZ4_decompress_safe_partial_usingDict(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE, buffer, largeSize);
    if ( (result < 0)
      || (result != srcLen)
      || memcmp(source, outBuffer, (size_t)srcLen) ) {
      printf("test decompress-partial-usingDict with large prefix error \n");
      return -1;
    }
  }

  for (i = cmpSize; i < cmpSize + 10; ++i) {
    int result = LZ4_decompress_safe_partial_usingDict(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE, smallDict, smallSize);
    if ( (result < 0)
      || (result != srcLen)
      || memcmp(source, outBuffer, (size_t)srcLen) ) {
      printf("test decompress-partial-usingDict with small external dict error \n");
      return -1;
    }
  }

  for (i = cmpSize; i < cmpSize + 10; ++i) {
    int result = LZ4_decompress_safe_partial_usingDict(cmpBuffer, outBuffer, i, srcLen, BUFFER_SIZE, largeDict, largeSize);
    if ( (result < 0)
      || (result != srcLen)
      || memcmp(source, outBuffer, (size_t)srcLen) ) {
      printf("test decompress-partial-usingDict with large external dict error \n");
      return -1;
    }
  }

  printf("test decompress-partial-usingDict OK \n");
  return 0;
}
