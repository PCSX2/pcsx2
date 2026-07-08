# LZ4 Streaming API Example: Double Buffer
by *Takayuki Matsuoka*

[`blockStreaming_doubleBuffer.c`](blockStreaming_doubleBuffer.c) is an example
of the LZ4 Streaming API where we implement double buffer (de)compression.

Please note:

- Firstly, read ["LZ4 Streaming API Basics"](streaming_api_basics.md).
- This is a relatively advanced application example.
- The output file is not compatible with lz4frame and is platform dependent.


## What's the point of this example?

The LZ4 Streaming API can be used to handle compressing a huge file in a small
amount of memory. This example shows how to use a "Double Buffer" to compress
blocks (i.e. chunks) of a uniform size in a stream. The Streaming API used in
this way *always* yields a better compression ratio than the regular Block API.

For an example with non-uniform blocks, see ["LZ4 Streaming API Example: Line by
Line Text Compression"](blockStreaming_lineByLine.md).

## How compression works

Firstly, allocate "Double Buffer" for input and "compressed data buffer" for
output. Double buffer has two pages, the "first" page (`Page#1`) and the "second"
page (`Page#2`).

```
        Double Buffer

      Page#1    Page#2
    +---------+---------+
    | Block#1 |         |
    +----+----+---------+
         |
         v
      {Out#1}
```

Next, read the first block to the double buffer's first page. Compress it with
`LZ4_compress_continue()`. On the first compression, LZ4 doesn't have any
previous dependencies, so it just compresses the block without dependencies and
writes the compressed block `{Out#1}` to the compressed data buffer. After that,
write `{Out#1}` to the file.

```
      Prefix Dependency

         |         |
         v         |
    +---------+----+----+
    | Block#1 | Block#2 |
    +---------+----+----+
                   |
                   v
                {Out#2}
```

Next, read the second block to the double buffer's second page and compress it.
This time, LZ4 can use the dependency on `Block#1` to improve the compression
ratio. This dependency is called "Prefix mode".

```
   External Dictionary Mode
         +---------+
         |         |
         |         v
    +----+----+---------+
    | Block#3 | Block#2 |
    +----+----+---------+
         |
         v
      {Out#3}
```

Next, read the third block to the double buffer's *first* page and compress it.
This time LZ4 can use dependency on Block#2. This dependency is called "External
Dictionary mode".

```
      Prefix Dependency
         +---------+
         |         |
         v         |
    +---------+----+----+
    | Block#3 | Block#4 |
    +---------+----+----+
                   |
                   v
                {Out#4}
```

Continue this procedure till the end of the file.


## How decompression works

Decompression follows the reverse order:

- Read the first compressed block.
- Decompress it to the first page and write that page to the file.
- Read the second compressed block.
- Decompress it to the second page and write that page to the file.
- Read the third compressed block.
- Decompress it to the *first* page and write that page to the file.

Continue this procedure till the end of the compressed file.
