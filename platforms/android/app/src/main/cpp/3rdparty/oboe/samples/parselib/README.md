**parselib**
==========
Classes for parsing and loading audio data.

## Abstract
(Oboe) **parselib** contains facilities for reading and loading audio data from streams. Streams can be wrapped around either files or memory blocks.

**parselib** is written in C++ and is intended to be called from Android native code. It is implemented as a static library.

## Supported Encodings
* Microsoft WAV format

## **parselib** project structure
* stream
Contains classes related to reading audio data from a stream abstraction

* wav
Contains classes to read/load audio data in WAV format

## **stream** Classes
### InputStream
An abstract class that defines the `InputStream` interface.

### FileInputStream
A concrete implementation of `InputStream` that reads data from a file.

### MemInputStream
A concrete implementation of `InputStream` that reads data from a memory block.

## **wav** Classes
Contains classes to read/load audio data in WAV format. WAV format files are "Microsoft Resource Interchange File Format" (RIFF) files. WAV files contain a variety of RIFF "chunks", but only a few are required (see 'Chunk' classes below)

### Utility
#### AudioEncoding
Defines constants for various audio encodings

### WavTypes
Support for **RIFF** file types and managing FOURCC data.

### WAV Data I/O
#### WavStreamReader
Parses and loads WAV data from an InputStream.

### WAV Data
#### WavChunkHeader
Defines common fields and operations for all WAV format RIFF Chunks.

#### WavFmtChunkHeader
Defines fields and operations for RIFF '`fmt `' chunks

#### WavRIFFChunkHeader
Defines fields and operations for RIFF '`data`' chunks
