LZ4 Block Format Description
============================
Last revised: 2022-07-31 .
Author : Yann Collet


This specification is intended for developers willing to
produce or read LZ4 compressed data blocks
using any programming language of their choice.

LZ4 is an LZ77-type compressor with a fixed byte-oriented encoding format.
There is no entropy encoder back-end nor framing layer.
The latter is assumed to be handled by other parts of the system
(see [LZ4 Frame format]).
This design is assumed to favor simplicity and speed.

This document describes only the Block Format,
not how the compressor nor decompressor actually work.
For more details on such topics, see later section "Implementation Notes".

[LZ4 Frame format]: lz4_Frame_format.md



Compressed block format
-----------------------
An LZ4 compressed block is composed of sequences.
A sequence is a suite of literals (not-compressed bytes),
followed by a match copy operation.

Each sequence starts with a `token`.
The `token` is a one byte value, separated into two 4-bits fields.
Therefore each field ranges from 0 to 15.


The first field uses the 4 high-bits of the token.
It provides the length of literals to follow.

If the field value is smaller than 15,
then it represents the total nb of literals present in the sequence,
including 0, in which case there is no literal.

The value 15 is a special case: more bytes are required to indicate the full length.
Each additional byte then represents a value from 0 to 255,
which is added to the previous value to produce a total length.
When the byte value is 255, another byte must be read and added, and so on.
There can be any number of bytes of value `255` following `token`.
The Block Format does not define any "size limit",
though real implementations may feature some practical limits
(see more details in later chapter "Implementation Notes").

Note : this format explains why a non-compressible input block is expanded by 0.4%.

Example 1 : A literal length of 48 will be represented as :

  - 15 : value for the 4-bits High field
  - 33 : (=48-15) remaining length to reach 48

Example 2 : A literal length of 280 will be represented as :

  - 15  : value for the 4-bits High field
  - 255 : following byte is maxed, since 280-15 >= 255
  - 10  : (=280 - 15 - 255) remaining length to reach 280

Example 3 : A literal length of 15 will be represented as :

  - 15 : value for the 4-bits High field
  - 0  : (=15-15) yes, the zero must be output

Following `token` and optional length bytes, are the literals themselves.
They are exactly as numerous as just decoded (length of literals).
Reminder: it's possible that there are zero literals.


Following the literals is the match copy operation.

It starts by the `offset` value.
This is a 2 bytes value, in little endian format
(the 1st byte is the "low" byte, the 2nd one is the "high" byte).

The `offset` represents the position of the match to be copied from the past.
For example, 1 means "current position - 1 byte".
The maximum `offset` value is 65535. 65536 and beyond cannot be coded.
Note that 0 is an invalid `offset` value.
The presence of a 0 `offset` value denotes an invalid (corrupted) block.

Then the `matchlength` can be extracted.
For this, we use the second `token` field, the low 4-bits.
Such a value, obviously, ranges from 0 to 15.
However here, 0 means that the copy operation is minimal.
The minimum length of a match, called `minmatch`, is 4.
As a consequence, a 0 value means 4 bytes.
Similarly to literal length, any value smaller than 15 represents a length,
to which 4 (`minmatch`) must be added, thus ranging from 4 to 18.
A value of 15 is special, meaning 19+ bytes,
to which one must read additional bytes, one at a time,
with each byte value ranging from 0 to 255.
They are added to total to provide the final match length.
A 255 value means there is another byte to read and add.
There is no limit to the number of optional `255` bytes that can be present,
and therefore no limit to representable match length,
though real-life implementations are likely going to enforce limits for practical reasons (see more details in "Implementation Notes" section below).

Note: this format has a maximum achievable compression ratio of about ~250.

Decoding the `matchlength` reaches the end of current sequence.
Next byte will be the start of another sequence, and therefore a new `token`.


End of block conditions
-------------------------
There are specific restrictions required to terminate an LZ4 block.

1. The last sequence contains only literals.
   The block ends right after the literals (no `offset` field).
2. The last 5 bytes of input are always literals.
   Therefore, the last sequence contains at least 5 bytes.
   - Special : if input is smaller than 5 bytes,
     there is only one sequence, it contains the whole input as literals.
     Even empty input can be represented, using a zero byte,
     interpreted as a final token without literal and without a match.
3. The last match must start at least 12 bytes before the end of block.
   The last match is part of the _penultimate_ sequence.
   It is followed by the last sequence, which contains _only_ literals.
   - Note that, as a consequence,
     blocks < 12 bytes cannot be compressed.
     And as an extension, _independent_ blocks < 13 bytes cannot be compressed,
     because they must start by at least one literal,
     that the match can then copy afterwards.

When a block does not respect these end conditions,
a conformant decoder is allowed to reject the block as incorrect.

These rules are in place to ensure compatibility with
a wide range of historical decoders
which rely on these conditions for their speed-oriented design.

Implementation notes
-----------------------
The LZ4 Block Format only defines the compressed format,
it does not tell how to create a decoder or an encoder,
which design is left free to the imagination of the implementer.

However, thanks to experience, there are a number of typical topics that
most implementations will have to consider.
This section tries to provide a few guidelines.

#### Metadata

An LZ4-compressed Block requires additional metadata for proper decoding.
Typically, a decoder will require the compressed block's size,
and an upper bound of decompressed size.
Other variants exist, such as knowing the decompressed size,
and having an upper bound of the input size.
The Block Format does not specify how to transmit such information,
which is considered an out-of-band information channel.
That's because in many cases, the information is present in the environment.
For example, databases must store the size of their compressed block for indexing,
and know that their decompressed block can't be larger than a certain threshold.

If you need a format which is "self-contained",
and also transports the necessary metadata for proper decoding on any platform,
consider employing the [LZ4 Frame format] instead.

#### Large lengths

While the Block Format does not define any maximum value for length fields,
in practice, most implementations will feature some form of limit,
since it's expected for such values to be stored into registers of fixed bit width.

If length fields use 64-bit registers,
then it can be assumed that there is no practical limit,
as it would require a single continuous block of multiple petabytes to reach it,
which is unreasonable by today's standard.

If length fields use 32-bit registers, then it can be overflowed,
but requires a compressed block of size > 16 MB.
Therefore, implementations that do not deal with compressed blocks > 16 MB are safe.
However, if such a case is allowed,
then it's recommended to check that no large length overflows the register.

If length fields use 16-bit registers,
then it's definitely possible to overflow such register,
with less than < 300 bytes of compressed data.

A conformant decoder should be able to detect length overflows when it's possible,
and simply error out when that happens.
The input block might not be invalid,
it's just not decodable by the local decoder implementation.

Note that, in order to be compatible with the larger LZ4 ecosystem,
it's recommended to be able to read and represent lengths of up to 4 MB,
and to accept blocks of size up to 4 MB.
Such limits are compatible with 32-bit length registers,
and prevent overflow of 32-bit registers.

#### Safe decoding

If a decoder receives compressed data from any external source,
it is recommended to ensure that the decoder is resilient to corrupted input,
and made safe from buffer overflow manipulations.
Always ensure that read and write operations
remain within the limits of provided buffers.

Of particular importance, ensure that the nb of bytes instructed to copy
does not overflow neither the input nor the output buffers.
Ensure also, when reading an offset value, that the resulting position to copy
does not reach beyond the beginning of the buffer.
Such a situation can happen during the first 64 KB of decoded data.

For more safety, test the decoder with fuzzers
to ensure it's resilient to improbable sequences of conditions.
Combine them with sanitizers, in order to catch overflows (asan)
or initialization issues (msan).

Pay some attention to offset 0 scenario, which is invalid,
and therefore must not be blindly decoded:
a naive implementation could preserve destination buffer content,
which could then result in information disclosure
if such buffer was uninitialized and still containing private data.
For reference, in such a scenario, the reference LZ4 decoder
clears the match segment with `0` bytes,
though other solutions are certainly possible.

Finally, pay attention to the "overlap match" scenario,
when `matchlength` is larger than `offset`.
In which case, since `match_pos + matchlength > current_pos`,
some of the later bytes to copy do not exist yet,
and will be generated during the early stage of match copy operation.
Such scenario must be handled with special care.
A common case is an offset of 1,
meaning the last byte is repeated `matchlength` times.

#### Compression techniques

The core of a LZ4 compressor is to detect duplicated data across past 64 KB.
The format makes no assumption nor limits to the way a compressor
searches and selects matches within the source data block.
For example, an upper compression limit can be reached,
using a technique called "full optimal parsing", at high cpu and memory cost.
But multiple other techniques can be considered,
featuring distinct time / performance trade-offs.
As long as the specified format is respected,
the result will be compatible with and decodable by any compliant decoder.
