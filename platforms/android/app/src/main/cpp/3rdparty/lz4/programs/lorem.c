/*
    lorem.c - lorem ipsum generator
    Copyright (c) Yann Collet. All rights reserved.

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
   - LZ4 source repository : https://github.com/lz4/lz4
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* Implementation notes:
 *
 * This is a very simple lorem ipsum generator
 * which features a static list of words
 * and print them one after another randomly
 * with a fake sentence / paragraph structure.
 *
 * The goal is to generate a printable text
 * that can be used to fake a text compression scenario.
 * The resulting compression / ratio curve of the lorem ipsum generator
 * is more satisfying than the previous statistical generator,
 * which was initially designed for entropy compression,
 * and lacks a regularity more representative of text.
 *
 * The compression ratio achievable on the generated lorem ipsum
 * is still a bit too good, presumably because the dictionary is a bit too
 * small. It would be possible to create some more complex scheme, notably by
 * enlarging the dictionary with a word generator, and adding grammatical rules
 * (composition) and syntax rules. But that's probably overkill for the intended
 * goal.
 */

#include "lorem.h"
#include <assert.h>
#include <limits.h> /* INT_MAX */
#include <stdlib.h> /* malloc, abort */
#include <string.h> /* memcpy */

/* Define the word pool
 * Note: all words must have a len <= 16 */
static const char* kWords[] = {
    "lorem",        "ipsum",      "dolor",       "sit",          "amet",
    "consectetur",  "adipiscing", "elit",        "sed",          "do",
    "eiusmod",      "tempor",     "incididunt",  "ut",           "labore",
    "et",           "dolore",     "magna",       "aliqua",       "dis",
    "lectus",       "vestibulum", "mattis",      "ullamcorper",  "velit",
    "commodo",      "a",          "lacus",       "arcu",         "magnis",
    "parturient",   "montes",     "nascetur",    "ridiculus",    "mus",
    "mauris",       "nulla",      "malesuada",   "pellentesque", "eget",
    "gravida",      "in",         "dictum",      "non",          "erat",
    "nam",          "voluptat",   "maecenas",    "blandit",      "aliquam",
    "etiam",        "enim",       "lobortis",    "scelerisque",  "fermentum",
    "dui",          "faucibus",   "ornare",      "at",           "elementum",
    "eu",           "facilisis",  "odio",        "morbi",        "quis",
    "eros",         "donec",      "ac",          "orci",         "purus",
    "turpis",       "cursus",     "leo",         "vel",          "porta",
    "consequat",    "interdum",   "varius",      "vulputate",    "aliquet",
    "pharetra",     "nunc",       "auctor",      "urna",         "id",
    "metus",        "viverra",    "nibh",        "cras",         "mi",
    "unde",         "omnis",      "iste",        "natus",        "error",
    "perspiciatis", "voluptatem", "accusantium", "doloremque",   "laudantium",
    "totam",        "rem",        "aperiam",     "eaque",        "ipsa",
    "quae",         "ab",         "illo",        "inventore",    "veritatis",
    "quasi",        "architecto", "beatae",      "vitae",        "dicta",
    "sunt",         "explicabo",  "nemo",        "ipsam",        "quia",
    "voluptas",     "aspernatur", "aut",         "odit",         "fugit",
    "consequuntur", "magni",      "dolores",     "eos",          "qui",
    "ratione",      "sequi",      "nesciunt",    "neque",        "porro",
    "quisquam",     "est",        "dolorem",     "adipisci",     "numquam",
    "eius",         "modi",       "tempora",     "incidunt",     "magnam",
    "quaerat",      "ad",         "minima",      "veniam",       "nostrum",
    "ullam",        "corporis",   "suscipit",    "laboriosam",   "nisi",
    "aliquid",      "ex",         "ea",          "commodi",      "consequatur",
    "autem",        "eum",        "iure",        "voluptate",    "esse",
    "quam",         "nihil",      "molestiae",   "illum",        "fugiat",
    "quo",          "pariatur",   "vero",        "accusamus",    "iusto",
    "dignissimos",  "ducimus",    "blanditiis",  "praesentium",  "voluptatum",
    "deleniti",     "atque",      "corrupti",    "quos",         "quas",
    "molestias",    "excepturi",  "sint",        "occaecati",    "cupiditate",
    "provident",    "similique",  "culpa",       "officia",      "deserunt",
    "mollitia",     "animi",      "laborum",     "dolorum",      "fuga",
    "harum",        "quidem",     "rerum",       "facilis",      "expedita",
    "distinctio",   "libero",     "tempore",     "cum",          "soluta",
    "nobis",        "eligendi",   "optio",       "cumque",       "impedit",
    "minus",        "quod",       "maxime",      "placeat",      "facere",
    "possimus",     "assumenda",  "repellendus", "temporibus",   "quibusdam",
    "officiis",     "debitis",    "saepe",       "eveniet",      "voluptates",
    "repudiandae",  "recusandae", "itaque",      "earum",        "hic",
    "tenetur",      "sapiente",   "delectus",    "reiciendis",   "cillum",
    "maiores",      "alias",      "perferendis", "doloribus",    "asperiores",
    "repellat",     "minim",      "nostrud",     "exercitation", "ullamco",
    "laboris",      "aliquip",    "duis",        "aute",         "irure",
};
#define KNBWORDS (sizeof(kWords) / sizeof(kWords[0]))
static const unsigned kNbWords = KNBWORDS;

static const char* g_words[KNBWORDS] = { NULL };
static unsigned g_wordLen[KNBWORDS] = {0};
static char* g_wordBuffer = NULL;

/* simple 1-dimension distribution, based on word's length, favors small words
 */
static const int kWeights[]      = { 0, 8, 6, 4, 3, 2 };
static const unsigned kNbWeights = sizeof(kWeights) / sizeof(kWeights[0]);

#define DISTRIB_SIZE_MAX 650
static int g_distrib[DISTRIB_SIZE_MAX] = { 0 };
static unsigned g_distribCount         = 0;

static void countFreqs(
        const unsigned wordLen[],
        size_t nbWords,
        const int* weights,
        unsigned long nbWeights)
{
    unsigned total = 0;
    size_t w;
    for (w = 0; w < nbWords; w++) {
        size_t len = wordLen[w];
        int lmax;
        if (len >= nbWeights)
            len = nbWeights - 1;
        lmax = weights[len];
        total += (unsigned)lmax;
    }
    g_distribCount = total;
    assert(g_distribCount <= DISTRIB_SIZE_MAX);
}

static void init_word_len(
        const char* words[],
        size_t nbWords)
{
    size_t n;
    assert(words != NULL);
    for (n=0; n<nbWords; n++) {
        assert(words[n] != NULL);
        assert(strlen(words[n]) < 256);
        g_wordLen[n] = (unsigned char)strlen(words[n]);
    }

}

static size_t sumLen(const unsigned* sizes, size_t s)
{
    size_t total = 0;
    size_t n;
    assert(sizes != NULL);
    for (n=0; n<s; n++) {
        total += sizes[n];
    }
    return total;
}

static void init_word_buffer(void)
{
    size_t n;
    size_t const bufSize = sumLen(g_wordLen, kNbWords) + 16;
    char* ptr;
    assert(g_wordBuffer == NULL);
    g_wordBuffer = (char*)calloc(1, bufSize);
    if (g_wordBuffer == NULL) abort();
    ptr = g_wordBuffer;
    for (n=0; n<kNbWords; n++) {
        memcpy(ptr, kWords[n], g_wordLen[n]);
        g_words[n] = ptr;
        ptr += g_wordLen[n];
    }
}

static void init_word_distrib(
        const unsigned wordLen[],
        size_t nbWords,
        const int* weights,
        unsigned long nbWeights)
{
    size_t w, d = 0;
    countFreqs(wordLen, nbWords, weights, nbWeights);
    for (w = 0; w < nbWords; w++) {
        size_t len = wordLen[w];
        int l, lmax;
        if (len >= nbWeights)
            len = nbWeights - 1;
        lmax = weights[len];
        for (l = 0; l < lmax; l++) {
            g_distrib[d++] = (int)w;
        }
    }
}

/* Note: this unit only works when invoked sequentially.
 * No concurrent access is allowed */
static char* g_ptr         = NULL;
static size_t g_nbChars    = 0;
static size_t g_maxChars   = 10000000;
static unsigned g_randRoot = 0;

#define RDG_rotl32(x, r) ((x << r) | (x >> (32 - r)))
static unsigned LOREM_rand(unsigned range)
{
    static const unsigned prime1 = 2654435761U;
    static const unsigned prime2 = 2246822519U;
    unsigned rand32              = g_randRoot;
    rand32 *= prime1;
    rand32 ^= prime2;
    rand32     = RDG_rotl32(rand32, 13);
    g_randRoot = rand32;
    return (unsigned)(((unsigned long long)rand32 * range) >> 32);
}

static void writeLastCharacters(void)
{
    size_t lastChars = g_maxChars - g_nbChars;
    assert(g_maxChars >= g_nbChars);
    if (lastChars == 0)
        return;
    g_ptr[g_nbChars++] = '.';
    if (lastChars > 2) {
        memset(g_ptr + g_nbChars, ' ', lastChars - 2);
    }
    if (lastChars > 1) {
        g_ptr[g_maxChars - 1] = '\n';
    }
    g_nbChars = g_maxChars;
}

static void generateLastWord(const char* word, size_t wordLen, int upCase)
{
    if (g_nbChars + wordLen + 2 > g_maxChars) {
        writeLastCharacters();
        return;
    }
    memcpy(g_ptr + g_nbChars, word, wordLen);
    if (upCase) {
        static const char toUp = 'A' - 'a';
        g_ptr[g_nbChars]       = (char)(g_ptr[g_nbChars] + toUp);
    }
    g_nbChars += wordLen;
    writeLastCharacters();
}

#define MAX(a,b)  ((a)<(b)?(b):(a))
static void generateWord(const char* word, size_t wordLen, const char* separator, size_t sepLen, int upCase)
{
    size_t const wlen = MAX(16, wordLen + 2);
    if (g_nbChars + wlen > g_maxChars) {
        generateLastWord(word, wordLen, upCase);
        return;
    }
    assert(wordLen <= 16);
    memcpy(g_ptr + g_nbChars, word, 16);
    if (upCase) {
        static const char toUp = 'A' - 'a';
        g_ptr[g_nbChars]       = (char)(g_ptr[g_nbChars] + toUp);
    }
    g_nbChars += wordLen;
    assert(sepLen <= 2);
    memcpy(g_ptr + g_nbChars, separator, 2);
    g_nbChars += sepLen;
}

static int about(unsigned target)
{
    return (int)(LOREM_rand(target) + LOREM_rand(target) + 1);
}

/* Function to generate a random sentence */
static void generateSentence(int nbWords)
{
    int commaPos       = about(9);
    int comma2         = commaPos + about(7);
    int qmark          = (LOREM_rand(11) == 7);
    const char* endSep = qmark ? "? " : ". ";
    int i;
    for (i = 0; i < nbWords; i++) {
        int const wordID       = g_distrib[LOREM_rand(g_distribCount)];
        const char* sep        = " ";
        size_t sepLen = 1;
        if (i == commaPos)
            sep = ", ", sepLen=2;
        if (i == comma2)
            sep = ", ", sepLen=2;
        if (i == nbWords - 1)
            sep = endSep, sepLen=2;
        generateWord(g_words[wordID], g_wordLen[wordID], sep, sepLen, i == 0);
    }
}

static void generateParagraph(int nbSentences)
{
    int i;
    for (i = 0; i < nbSentences; i++) {
        int wordsPerSentence = about(11);
        generateSentence(wordsPerSentence);
    }
    if (g_nbChars < g_maxChars) {
        g_ptr[g_nbChars++] = '\n';
    }
    if (g_nbChars < g_maxChars) {
        g_ptr[g_nbChars++] = '\n';
    }
}

/* It's "common" for lorem ipsum generators to start with the same first
 * pre-defined sentence */
static void generateFirstSentence(void)
{
    int i;
    for (i = 0; i < 18; i++) {
        const char* separator = " ";
        size_t sepLen = 1;
        if (i == 4)
            separator = ", ", sepLen=2;
        if (i == 7)
            separator = ", ", sepLen=2;
        generateWord(g_words[i], g_wordLen[i], separator, sepLen, i == 0);
    }
    generateWord(g_words[18], g_wordLen[18], ". ", 2, 0);
}

size_t
LOREM_genBlock(void* buffer, size_t size, unsigned seed, int first, int fill)
{
    g_ptr = (char*)buffer;
    assert(size < INT_MAX);
    g_maxChars = size;
    g_nbChars  = 0;
    g_randRoot = seed;
    if (g_distribCount == 0) {
        init_word_len(kWords, kNbWords);
        init_word_buffer();
        init_word_distrib(g_wordLen, kNbWords, kWeights, kNbWeights);
    }

    if (first) {
        generateFirstSentence();
    }
    while (g_nbChars < g_maxChars) {
        int sentencePerParagraph = about(7);
        generateParagraph(sentencePerParagraph);
        if (!fill)
            break; /* only generate one paragraph in not-fill mode */
    }
    g_ptr = NULL;
    return g_nbChars;
}

void LOREM_genBuffer(void* buffer, size_t size, unsigned seed)
{
    LOREM_genBlock(buffer, size, seed, 1, 1);
}
