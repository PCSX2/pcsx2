
#pragma once

typedef unsigned char uint8;
typedef signed char int8;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned int uint32;
typedef signed int int32;
typedef unsigned long long uint64;
typedef signed long long int64;

#ifndef RESTRICT

#ifdef __INTEL_COMPILER

#define RESTRICT restrict

#elif defined(_MSC_VER)

#define RESTRICT __restrict

#elif defined(__GNUC__)

#define RESTRICT __restrict__

#else

#define RESTRICT

#endif

#endif
