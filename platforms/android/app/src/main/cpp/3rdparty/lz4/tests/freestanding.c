// Basic test for LZ4_FREESTANDING

// $ gcc -ffreestanding -nostdlib freestanding.c && ./a.out || echo $?

// $ strace ./a.out
// execve("./a.out", ["./a.out"], 0x7fffaf5fa580 /* 22 vars */) = 0
// brk(NULL)                               = 0x56536f4fe000
// arch_prctl(0x3001 /* ARCH_??? */, 0x7fffc9e74950) = -1 EINVAL (Invalid argument)
// mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fd5c9c2b000
// access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
// arch_prctl(ARCH_SET_FS, 0x7fd5c9c2bc40) = 0
// set_tid_address(0x7fd5c9c2bf10)         = 381
// set_robust_list(0x7fd5c9c2bf20, 24)     = 0
// rseq(0x7fd5c9c2c5e0, 0x20, 0, 0x53053053) = 0
// mprotect(0x56536ea63000, 4096, PROT_READ) = 0
// exit(0)                                 = ?
// +++ exited with 0 +++

// $ ltrace ./a.out
// +++ exited (status 0) +++

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#  define EXTERN_C extern "C"
#else
#  define EXTERN_C
#endif


#if !defined(__x86_64__) || !defined(__linux__)
int main(int argc, char** argv) { return 0; }
#else

static void MY_exit(int exitCode);
static void MY_abort(void);
EXTERN_C void *memmove(void *dst, const void *src, size_t n);
EXTERN_C void *memcpy(void * __restrict__ dst, const void * __restrict__ src, size_t n);
EXTERN_C void *memset(void *s, int c, size_t n);
EXTERN_C int memcmp(const void *s1, const void *s2, size_t n);

// LZ4/HC basic freestanding setup
#define LZ4_FREESTANDING 1
#define LZ4_memmove(dst, src, size) memmove((dst),(src),(size))
#define LZ4_memcpy(dst, src, size)  memcpy((dst),(src),(size))
#define LZ4_memset(p,v,s)           memset((p),(v),(s))

#include "../lib/lz4.c"
#include "../lib/lz4hc.c"

// Test for LZ4
static void test_lz4(const uint8_t* srcData, int srcSize) {
    // Compress
    static uint8_t compressBuffer[1024 * 1024];
    const int compressedSize = LZ4_compress_default(
        (const char*) srcData,
        (char*) compressBuffer,
        srcSize,
        sizeof(compressBuffer)
    );
    if (compressedSize <= 0) {
        MY_exit(__LINE__);
    }

    // Decompress
    static uint8_t decompressBuffer[1024 * 1024];
    const int decompressedSize = LZ4_decompress_safe(
        (const char*) compressBuffer,
        (char*) decompressBuffer,
        compressedSize,
        sizeof(decompressBuffer)
    );
    if (decompressedSize <= 0) {
        MY_exit(__LINE__);
    }

    // Verify
    if (decompressedSize != srcSize) {
        MY_exit(__LINE__);
    }
    if (memcmp(srcData, decompressBuffer, srcSize) != 0) {
        MY_exit(__LINE__);
    }
}


// Test for LZ4HC
static void test_lz4hc(const uint8_t* srcData, int srcSize) {
    // Compress
    static uint8_t compressBuffer[1024 * 1024];
    const int compressedSize = LZ4_compress_HC(
        (const char*) srcData,
        (char*) compressBuffer,
        srcSize,
        sizeof(compressBuffer),
        LZ4HC_CLEVEL_DEFAULT
    );
    if (compressedSize <= 0) {
        MY_exit(__LINE__);
    }

    // Decompress
    static uint8_t decompressBuffer[1024 * 1024];
    const int decompressedSize = LZ4_decompress_safe(
        (const char*) compressBuffer,
        (char*) decompressBuffer,
        compressedSize,
        sizeof(decompressBuffer)
    );
    if (decompressedSize <= 0) {
        MY_exit(__LINE__);
    }

    // Verify
    if (decompressedSize != srcSize) {
        MY_exit(__LINE__);
    }
    if (memcmp(srcData, decompressBuffer, srcSize) != 0) {
        MY_exit(__LINE__);
    }
}


static void test(void) {
    // First 256 bytes of lz4/README.md
    static const uint8_t README_md[] = {
        0x4c, 0x5a, 0x34, 0x20, 0x2d, 0x20, 0x45, 0x78, 0x74, 0x72, 0x65, 0x6d, 0x65, 0x6c, 0x79, 0x20,
        0x66, 0x61, 0x73, 0x74, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e,
        0x0a, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
        0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d, 0x3d,
        0x3d, 0x0a, 0x0a, 0x4c, 0x5a, 0x34, 0x20, 0x69, 0x73, 0x20, 0x6c, 0x6f, 0x73, 0x73, 0x6c, 0x65,
        0x73, 0x73, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x20, 0x61,
        0x6c, 0x67, 0x6f, 0x72, 0x69, 0x74, 0x68, 0x6d, 0x2c, 0x0a, 0x70, 0x72, 0x6f, 0x76, 0x69, 0x64,
        0x69, 0x6e, 0x67, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x72, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x20,
        0x73, 0x70, 0x65, 0x65, 0x64, 0x20, 0x3e, 0x20, 0x35, 0x30, 0x30, 0x20, 0x4d, 0x42, 0x2f, 0x73,
        0x20, 0x70, 0x65, 0x72, 0x20, 0x63, 0x6f, 0x72, 0x65, 0x2c, 0x0a, 0x73, 0x63, 0x61, 0x6c, 0x61,
        0x62, 0x6c, 0x65, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x6d, 0x75, 0x6c, 0x74, 0x69, 0x2d, 0x63,
        0x6f, 0x72, 0x65, 0x73, 0x20, 0x43, 0x50, 0x55, 0x2e, 0x0a, 0x49, 0x74, 0x20, 0x66, 0x65, 0x61,
        0x74, 0x75, 0x72, 0x65, 0x73, 0x20, 0x61, 0x6e, 0x20, 0x65, 0x78, 0x74, 0x72, 0x65, 0x6d, 0x65,
        0x6c, 0x79, 0x20, 0x66, 0x61, 0x73, 0x74, 0x20, 0x64, 0x65, 0x63, 0x6f, 0x64, 0x65, 0x72, 0x2c,
        0x0a, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x70, 0x65, 0x65, 0x64, 0x20, 0x69, 0x6e, 0x20, 0x6d,
        0x75, 0x6c, 0x74, 0x69, 0x70, 0x6c, 0x65, 0x20, 0x47, 0x42, 0x2f, 0x73, 0x20, 0x70, 0x65, 0x72,
    };

    static const uint8_t* srcData = README_md;
    static const int      srcSize = (int) sizeof(README_md);
    test_lz4  (srcData, srcSize);
    test_lz4hc(srcData, srcSize);
}


// low level syscall
#define SYS_exit (60)

static __inline long os_syscall1(long n, long a1) {
    register long rax __asm__ ("rax") = n;
    register long rdi __asm__ ("rdi") = a1;
    __asm__ __volatile__ ("syscall" : "+r"(rax) : "r"(rdi) : "rcx", "r11", "memory");
    return rax;
}

static void MY_exit(int exitCode) {
    (void) os_syscall1(SYS_exit, exitCode);
    __builtin_unreachable();  // suppress "warning: 'noreturn' function does return"
}

static void MY_abort(void) {
    MY_exit(-1);
}

// https://refspecs.linuxbase.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/baselib---assert-fail-1.html
void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    MY_abort();
}


// GCC requires memcpy, memmove, memset and memcmp.
// https://gcc.gnu.org/onlinedocs/gcc/Standards.html
// > GCC requires the freestanding environment provide memcpy, memmove, memset and memcmp.
EXTERN_C void *memmove(void *dst, const void *src, size_t n) {
    uint8_t* d = (uint8_t*) dst;
    const uint8_t* s = (const uint8_t*) src;

    if (d > s) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    } else {
        while (n--) {
            *d++ = *s++;
        }
    }
    return dst;
}

EXTERN_C void *memcpy(void * __restrict__ dst, const void * __restrict__ src, size_t n) {
    return memmove(dst, src, n);
}

EXTERN_C void *memset(void *s, int c, size_t n) {
    uint8_t* p = (uint8_t*) s;
    while (n--) {
        *p++ = (uint8_t) c;
    }
    return s;
}

EXTERN_C int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*) s1;
    const uint8_t* p2 = (const uint8_t*) s2;
    while (n--) {
        const uint8_t c1 = *p1++;
        const uint8_t c2 = *p2++;
        if (c1 < c2) {
            return -1;
        } else if (c1 > c2) {
            return 1;
        }
    }
    return 0;
}


//
EXTERN_C void __attribute__((force_align_arg_pointer)) _start(void) {
    test();
    MY_exit(0);
}

int main(int argc, char** argv) {
    test();
    MY_exit(0);
    return 0;
}
#endif
