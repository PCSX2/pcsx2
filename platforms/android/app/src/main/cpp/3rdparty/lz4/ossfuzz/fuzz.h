/**
 * Fuzz target interface.
 * Fuzz targets have some common parameters passed as macros during compilation.
 * Check the documentation for each individual fuzzer for more parameters.
 *
 * @param FUZZ_RNG_SEED_SIZE:
 *        The number of bytes of the source to look at when constructing a seed
 *        for the deterministic RNG. These bytes are discarded before passing
 *        the data to lz4 functions. Every fuzzer initializes the RNG exactly
 *        once before doing anything else, even if it is unused.
 *        Default: 4.
 * @param LZ4_DEBUG:
 *        This is a parameter for the lz4 library. Defining `LZ4_DEBUG=1`
 *        enables assert() statements in the lz4 library. Higher levels enable
 *        logging, so aren't recommended. Defining `LZ4_DEBUG=1` is
 *        recommended.
 * @param LZ4_FORCE_MEMORY_ACCESS:
 *        This flag controls how the zstd library accesses unaligned memory.
 *        It can be undefined, or 0 through 2. If it is undefined, it selects
 *        the method to use based on the compiler. If testing with UBSAN set
 *        MEM_FORCE_MEMORY_ACCESS=0 to use the standard compliant method.
 * @param FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
 *        This is the canonical flag to enable deterministic builds for fuzzing.
 *        Changes to zstd for fuzzing are gated behind this define.
 *        It is recommended to define this when building zstd for fuzzing.
 */

#ifndef FUZZ_H
#define FUZZ_H

#ifndef FUZZ_RNG_SEED_SIZE
#  define FUZZ_RNG_SEED_SIZE 4
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int LLVMFuzzerTestOneInput(const uint8_t *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif
