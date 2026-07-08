// [P63] PNG stubs for macOS build (libpng not linked)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#include "Image.h"
#include <cstdio>
#include <vector>
bool PNGFileLoader(RGBA8Image* img, const char* fn, FILE* fp) { return false; }
bool PNGFileSaver(const RGBA8Image& img, const char* fn, FILE* fp, u8 q) { return false; }
bool PNGBufferLoader(RGBA8Image* img, const void* buf, size_t len) { return false; }
bool PNGBufferSaver(const RGBA8Image& img, std::vector<u8>* buf, u8 q) { return false; }
#endif
