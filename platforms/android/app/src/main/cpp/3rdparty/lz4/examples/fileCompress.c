/* LZ4file API example : compress a file
 * Modified from an example code by anjiahao
 *
 * This example will demonstrate how
 * to manipulate lz4 compressed files like
 * normal files */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <lz4file.h>


#define CHUNK_SIZE (16*1024)

static size_t get_file_size(char *filename)
{
    struct stat statbuf;

    if (filename == NULL) {
        return 0;
    }

    if(stat(filename,&statbuf)) {
        return 0;
    }

    return (size_t)statbuf.st_size;
}

static int compress_file(FILE* f_in, FILE* f_out)
{
    assert(f_in != NULL); assert(f_out != NULL);

    LZ4F_errorCode_t ret = LZ4F_OK_NoError;
    LZ4_writeFile_t* lz4fWrite;
    void* const buf = malloc(CHUNK_SIZE);
    if (!buf) {
        printf("error: memory allocation failed \n");
        return 1;
    }

    /* Of course, you can also use prefsPtr to
     * set the parameters of the compressed file
     * NULL is use default
     */
    ret = LZ4F_writeOpen(&lz4fWrite, f_out, NULL);
    if (LZ4F_isError(ret)) {
        printf("LZ4F_writeOpen error: %s\n", LZ4F_getErrorName(ret));
        free(buf);
        return 1;
    }

    while (1) {
        size_t len = fread(buf, 1, CHUNK_SIZE, f_in);

        if (ferror(f_in)) {
            printf("fread error\n");
            goto out;
        }

        /* nothing to read */
        if (len == 0) {
            break;
        }

        ret = LZ4F_write(lz4fWrite, buf, len);
        if (LZ4F_isError(ret)) {
            printf("LZ4F_write: %s\n", LZ4F_getErrorName(ret));
            goto out;
        }
    }

out:
    free(buf);
    if (LZ4F_isError(LZ4F_writeClose(lz4fWrite))) {
        printf("LZ4F_writeClose: %s\n", LZ4F_getErrorName(ret));
        return 1;
    }

    return 0;
}

static int decompress_file(FILE* f_in, FILE* f_out)
{
    assert(f_in != NULL); assert(f_out != NULL);

    LZ4F_errorCode_t ret = LZ4F_OK_NoError;
    LZ4_readFile_t* lz4fRead;
    void* const buf= malloc(CHUNK_SIZE);
    if (!buf) {
        printf("error: memory allocation failed \n");
        return 1;
    }

    ret = LZ4F_readOpen(&lz4fRead, f_in);
    if (LZ4F_isError(ret)) {
        printf("LZ4F_readOpen error: %s\n", LZ4F_getErrorName(ret));
        free(buf);
        return 1;
    }

    while (1) {
        ret = LZ4F_read(lz4fRead, buf, CHUNK_SIZE);
        if (LZ4F_isError(ret)) {
            printf("LZ4F_read error: %s\n", LZ4F_getErrorName(ret));
            goto out;
        }

        /* nothing to read */
        if (ret == 0) {
            break;
        }

        if(fwrite(buf, 1, ret, f_out) != ret) {
            printf("write error!\n");
            goto out;
        }
    }

out:
    free(buf);
    if (LZ4F_isError(LZ4F_readClose(lz4fRead))) {
        printf("LZ4F_readClose: %s\n", LZ4F_getErrorName(ret));
        return 1;
    }

    if (ret) {
        return 1;
    }

    return 0;
}

/* @return 0 if both FILE* have identical content */
int compareFiles(FILE* fp0, FILE* fp1)
{
    int result = 0;
    assert(fp0 != NULL); assert(fp1 != NULL);

    while (result==0) {
        char b0[1024];
        char b1[1024];
        size_t const r0 = fread(b0, 1, sizeof(b0), fp0);
        size_t const r1 = fread(b1, 1, sizeof(b1), fp1);

        result = (r0 != r1);
        if (!r0 || !r1) break;
        if (r0 == r1) result = memcmp(b0, b1, r0);
    }

    return result;
}

int main(int argc, const char **argv) {
    char inpFilename[256] = { 0 };
    char lz4Filename[256] = { 0 };
    char decFilename[256] = { 0 };

    if (argc < 2) {
        printf("Please specify input filename\n");
        return 0;
    }

    snprintf(inpFilename, 256, "%s", argv[1]);
    snprintf(lz4Filename, 256, "%s.lz4", argv[1]);
    snprintf(decFilename, 256, "%s.lz4.dec", argv[1]);

    printf("inp = [%s]\n", inpFilename);
    printf("lz4 = [%s]\n", lz4Filename);
    printf("dec = [%s]\n", decFilename);

    /* compress */
    {   FILE* const inpFp = fopen(inpFilename, "rb");
        FILE* const outFp = fopen(lz4Filename, "wb");
        printf("compress : %s -> %s\n", inpFilename, lz4Filename);
        int ret = compress_file(inpFp, outFp);
        fclose(inpFp);
        fclose(outFp);

        if (ret) {
            printf("compression error: %s\n", LZ4F_getErrorName(ret));
            return 1;
        }

        printf("%s: %zu → %zu bytes, %.1f%%\n",
            inpFilename,
            get_file_size(inpFilename),
            get_file_size(lz4Filename), /* might overflow is size_t is 32 bits and size_{in,out} > 4 GB */
            (double)get_file_size(lz4Filename) / (double)get_file_size(inpFilename) * 100);

        printf("compress : done\n");
    }

    /* decompress */
    {
        FILE* const inpFp = fopen(lz4Filename, "rb");
        FILE* const outFp = fopen(decFilename, "wb");

        if (inpFp == NULL) exit(1);
        if (outFp == NULL) exit(1);

        printf("decompress : %s -> %s\n", lz4Filename, decFilename);
        int ret = decompress_file(inpFp, outFp);

        fclose(outFp);
        fclose(inpFp);

        if (ret) {
            printf("compression error: %s\n", LZ4F_getErrorName(ret));
            return 1;
        }

        printf("decompress : done\n");
    }

    /* verify */
    {   FILE* const inpFp = fopen(inpFilename, "rb");
        FILE* const decFp = fopen(decFilename, "rb");

        if (inpFp == NULL) exit(1);
        if (decFp == NULL) exit(1);

        printf("verify : %s <-> %s\n", inpFilename, decFilename);
        int const cmp = compareFiles(inpFp, decFp);

        fclose(decFp);
        fclose(inpFp);

        if (cmp) {
            printf("corruption detected : decompressed file differs from original\n");
            return cmp;
        }

        printf("verify : OK\n");
    }

}
