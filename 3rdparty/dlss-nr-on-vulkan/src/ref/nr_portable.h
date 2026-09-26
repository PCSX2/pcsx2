/*
 * nr_portable.h — the handful of things the C here needs that C11 does not give the same
 * way on Linux, macOS and Windows: half-precision conversion, a thread-local, dynamic
 * loading, a clock, and the string and file odds and ends MSVC spells differently.
 *
 * The half conversions are the one place correctness lives. `_Float16` is used where the
 * compiler has it (GCC and Clang, which is every build this project has measured); the
 * fallback is a software round-to-nearest-even conversion for MSVC and for the RISC-V
 * targets that cannot hold a half in a vector (below), checked against the type
 * exhaustively — every one of the 65 536 halves widens to the same float, and every one
 * of the 2^32 floats narrows to the same half, NaN payloads aside (notes/phase69).
 * `NR_NO_FLOAT16` forces the fallback where the type exists, which is how that was run.
 */
#ifndef NR_PORTABLE_H
#define NR_PORTABLE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#define __USE_GNU
#include <dlfcn.h>
#endif

/* -- half precision ------------------------------------------------------ */

/* Clang defines __FLT16_MANT_DIG__ for every riscv64 target, Android's included, and that
 * one has the V extension but neither Zfh nor Zvfh: no half in a scalar FP register and
 * none in a vector. The loop vectorizer still turns the conversions below into
 * <vscale x N x half> fptrunc/fpext, which the backend can only lower by scalarizing --
 * and scalarizing a scalable vector is unimplemented, so the compile dies outright with
 * "Scalarization of scalable vectors is not supported" (`nr_compose` in nr_image.c, and
 * the C frame test). Without Zfh the type is a libcall on that target anyway, so the
 * software path costs it nothing. Vectors that can hold a half keep `_Float16`.
 */
#if defined(__riscv_vector) && !defined(__riscv_zvfh) && !defined(NR_NO_FLOAT16)
#define NR_NO_FLOAT16 1
#endif

#if !defined(NR_NO_FLOAT16) && (defined(__FLT16_MANT_DIG__) || (defined(__clang__) && defined(__aarch64__)))
#define NR_HAVE_FLOAT16 1
#endif

static inline float nr_half_to_float(uint16_t h)
{
#ifdef NR_HAVE_FLOAT16
    _Float16 v;
    memcpy(&v, &h, 2);
    return (float)v;
#else
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exponent = (h >> 10) & 0x1Fu, mantissa = h & 0x3FFu, bits;
    if (exponent == 0) {
        if (mantissa == 0) bits = sign;
        else {                                  /* subnormal: normalise */
            int shift = 0;
            while (!(mantissa & 0x400u)) { mantissa <<= 1; shift++; }
            mantissa &= 0x3FFu;
            bits = sign | ((uint32_t)(127 - 15 - shift + 1) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float f;
    memcpy(&f, &bits, 4);
    return f;
#endif
}

static inline uint16_t nr_float_to_half(float f)
{
#ifdef NR_HAVE_FLOAT16
    _Float16 v = (_Float16)f;                   /* round to nearest even, overflow to infinity */
    uint16_t h;
    memcpy(&h, &v, 2);
    return h;
#else
    uint32_t bits;
    memcpy(&bits, &f, 4);
    uint16_t sign = (uint16_t)((bits >> 16) & 0x8000u);
    uint32_t abs = bits & 0x7FFFFFFFu;
    if (abs >= 0x7F800000u)                     /* inf or nan */
        return (uint16_t)(sign | 0x7C00u | (abs > 0x7F800000u ? 0x200u : 0u));
    if (abs >= 0x477FF000u)                     /* rounds to or past the largest half */
        return (uint16_t)(sign | 0x7C00u);
    if (abs < 0x33000000u) return sign;         /* below half the smallest subnormal */
    int32_t exponent = (int32_t)(abs >> 23) - 127 + 15;
    uint32_t mantissa = abs & 0x7FFFFFu;
    uint32_t half_bits, shift;
    if (exponent <= 0) {                        /* subnormal in half */
        mantissa |= 0x800000u;
        shift = (uint32_t)(14 - exponent);
        half_bits = mantissa >> shift;
        uint32_t remainder = mantissa & ((1u << shift) - 1u), halfway = 1u << (shift - 1);
        if (remainder > halfway || (remainder == halfway && (half_bits & 1u))) half_bits++;
        return (uint16_t)(sign | half_bits);
    }
    half_bits = ((uint32_t)exponent << 10) | (mantissa >> 13);
    uint32_t remainder = mantissa & 0x1FFFu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (half_bits & 1u))) half_bits++;
    return (uint16_t)(sign | half_bits);
#endif
}

static inline float nr_half_round(float f) { return nr_half_to_float(nr_float_to_half(f)); }

/* -- thread-local, strings, files ---------------------------------------- */

#ifdef _MSC_VER
#define NR_THREAD_LOCAL __declspec(thread)
#define nr_strdup _strdup
#define nr_strtok_r strtok_s
#else
#define NR_THREAD_LOCAL _Thread_local
#define nr_strdup strdup
#define nr_strtok_r strtok_r
#endif

/* -- a clock ------------------------------------------------------------- */

static inline double nr_now(void)
{
#ifdef _WIN32
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
#endif
}

/* -- dynamic loading ------------------------------------------------------- */

#ifdef _WIN32
#define NR_SHARED_SUFFIX ".dll"
#elif defined(__APPLE__)
#define NR_SHARED_SUFFIX ".dylib"
#else
#define NR_SHARED_SUFFIX ".so"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
typedef HMODULE nr_dl;
static inline nr_dl nr_dl_open(const char *path) { return LoadLibraryA(path); }
static inline void *nr_dl_sym(nr_dl handle, const char *name) { return (void *)GetProcAddress(handle, name); }
static inline const char *nr_dl_error(void)
{
    static NR_THREAD_LOCAL char text[64];

#if __STDC_WANT_SECURE_LIB__
    sprintf_s(text, sizeof text, "error %lu", (unsigned long)GetLastError());
#else
    snprintf(text, sizeof text, "error %lu", (unsigned long)GetLastError());
#endif

    return text;
}
/* The directory holding the module that contains `symbol`. */
static inline int nr_dl_self_dir(const void *symbol, char *out, size_t cap)
{
    HMODULE module = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)symbol, &module)) return -1;
    if (!GetModuleFileNameA(module, out, (DWORD)cap)) return -1;
    char *slash = strrchr(out, '\\'), *fwd = strrchr(out, '/');
    if (fwd > slash) slash = fwd;
    if (slash) *slash = 0; else snprintf(out, cap, ".");
    return 0;
}
#else
#include <dlfcn.h>
typedef void *nr_dl;
static inline nr_dl nr_dl_open(const char *path) { return dlopen(path, RTLD_NOW | RTLD_GLOBAL); }
static inline void *nr_dl_sym(nr_dl handle, const char *name) { return dlsym(handle, name); }
static inline const char *nr_dl_error(void) { const char *e = dlerror(); return e ? e : "unknown error"; }
static inline int nr_dl_self_dir(const void *symbol, char *out, size_t cap)
{
    Dl_info info;
    if (!dladdr((void *)symbol, &info) || !info.dli_fname) return -1;
    snprintf(out, cap, "%s", info.dli_fname);
    char *slash = strrchr(out, '/');
    if (slash) *slash = 0; else snprintf(out, cap, ".");
    return 0;
}
#endif

/* -- environment and scratch files ---------------------------------------- */

static inline void nr_setenv_default(const char *name, const char *value)
{
    if (getenv(name)) return;
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 0);
#endif
}

/* Somewhere writable for a scratch file: TMPDIR, TEMP or TMP if set, else /tmp or `.` --
 * and on Android, which has no /tmp, the directory adb shell can write to. */
static inline const char *nr_temp_dir(void)
{
    const char *names[] = { "TMPDIR", "TEMP", "TMP" };
    for (size_t i = 0; i < 3; i++) { const char *v = getenv(names[i]); if (v && *v) return v; }
#ifdef _WIN32
    return ".";
#elif defined(__ANDROID__)
    return "/data/local/tmp";
#else
    return "/tmp";
#endif
}

#ifdef _WIN32
#include <process.h>
#define nr_getpid _getpid
#else
#include <unistd.h>
#define nr_getpid getpid
#endif

#endif
