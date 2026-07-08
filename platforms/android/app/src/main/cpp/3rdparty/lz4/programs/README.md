Command Line Interface for LZ4 library
============================================

### Build
The `lz4` Command Line Interface (CLI) is generated
using the `make` command, no additional parameter required.

The CLI generates and decodes [LZ4-compressed frames](../doc/lz4_Frame_format.md).

For more control over the build process,
the `Makefile` script supports all [standard conventions](https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html),
including standard targets (`all`, `install`, `clean`, etc.)
and standard variables (`CC`, `CFLAGS`, `CPPFLAGS`, etc.).

The makefile offer several targets for various use cases:
- `lz4` : default CLI, with a command line syntax similar to gzip
- `lz4c` : supports legacy lz4 commands (incompatible with gzip)
- `lz4c32` : Same as `lz4c`, but generates a 32-bits executable
- `unlz4`, `lz4cat` : symlinks to `lz4`, default to decompression and `cat` compressed files
- `man` : generates the man page, from `lz4.1.md` markdown source

#### Makefile Build variables
- `HAVE_MULTITHREAD` : build with multithreading support. Detection is generally automatic, but can be forced to `0` or `1` if needed. This is for example useful when cross-compiling for Windows from Linux.
- `HAVE_PTHREAD` : determines presence of `<pthread>` support. Detection is automatic, but can be forced to `0` or `1` if needed. This is in turn used by `make` to automatically trigger multithreading support.

#### C Preprocessor Build variables
These variables are read by the preprocessor at compilation time. They influence executable behavior, such as default starting values, and are exposed from `programs/lz4conf.h`. These variables can manipulated by any build system.
Assignment methods vary depending on environments.
On a typical `posix` + `gcc` + `make` setup, they can be defined with `CPPFLAGS=-DVARIABLE=value` assignment.
- `LZ4_CLEVEL_DEFAULT`: default compression level when none provided. Default is `1`.
- `LZ4IO_MULTITHREAD`: enable multithreading support. Default is disabled.
- `LZ4_NBWORKERS_DEFAULT`: default nb of worker threads used in multithreading mode (can be overridden with command `-T#`).
   Default is `0`, which means "auto-determine" based on local cpu.
- `LZ4_NBWORKERS_MAX`: absolute maximum nb of workers that can be requested at runtime.
   Currently set to 200 by default.
   This is mostly meant to protect the system against unreasonable and likely bogus requests, such as a million threads.
- `LZ4_BLOCKSIZEID_DEFAULT`: default `lz4` block size code. Valid values are [4-7], corresponding to 64 KB, 256 KB, 1 MB and 4 MB. At the time of this writing, default is 7, corresponding to 4 MB block size.

#### Environment Variables
It's possible to pass some parameters to `lz4` via environment variables.
This can be useful in situations where `lz4` is known to be invoked (from within a script for example) but there is no way to pass `lz4` parameters to influence the compression session.
The environment variable has higher priority than binary default, but lower priority than corresponding runtime command.
When set as global environment variables, it can enforce personalized defaults different from the binary set ones.

`LZ4_CLEVEL` can be used to specify a default compression level that `lz4` employs for compression when no other compression level is specified on command line. Executable default is generally `1`.

`LZ4_NBWORKERS` can be used to specify a default number of threads that `lz4` will employ for compression. Executable default is generally `0`, which means auto-determined based on local cpu. This functionality is only relevant when `lz4` is compiled with multithreading support. The maximum number of workers is capped at `LZ4_NBWORKERS_MAX` (`200` by default).

### Aggregation of parameters
The `lz4` CLI supports aggregation for short commands. For example, `-d`, `-q`, and `-f` can be joined into `-dqf`.
Aggregation doesn't work for `--long-commands`, which **must** be separated.


### Benchmark in Command Line Interface
`lz4` CLI includes an in-memory compression benchmark module, triggered by command `-b#`, with `#` representing the compression level.
The benchmark is conducted on a provided list of filenames.
The files are then read entirely into memory, to eliminate I/O overhead.
When multiple files are provided, they are bundled into the same benchmark session (though each file is a separate compression / decompression). Using `-S` command separates them (one session per file).
When no file is provided, uses an internal Lorem Ipsum generator instead.

The benchmark measures compressed size, compression factor, compression and decompression speed.
One can select multiple compression levels starting from `-b` and ending with `-e` (ascending).
The `-i` parameter selects a number of seconds used for each session.


### Usage of Command Line Interface
The full list of commands can be obtained with `-h` or `-H` parameter:
```
Usage :
      lz4 [arg] [input] [output]

input   : a filename
          with no FILE, or when FILE is - or stdin, read standard input
Arguments :
 -1     : Fast compression (default)
 -9     : High compression
 -d     : decompression (default for .lz4 extension)
 -z     : force compression
 -D FILE: use FILE as dictionary
 -f     : overwrite output without prompting
 -k     : preserve source files(s)  (default)
--rm    : remove source file(s) after successful de/compression
 -h/-H  : display help/long help and exit

Advanced arguments :
 -V     : display Version number and exit
 -v     : verbose mode
 -q     : suppress warnings; specify twice to suppress errors too
 -c     : force write to standard output, even if it is the console
 -t     : test compressed file integrity
 -m     : multiple input files (implies automatic output filenames)
 -r     : operate recursively on directories (sets also -m)
 -l     : compress using Legacy format (Linux kernel compression)
 -B#    : cut file into blocks of size # bytes [32+]
                     or predefined block size [4-7] (default: 7)
 -BD    : Block dependency (improve compression ratio)
 -BX    : enable block checksum (default:disabled)
--no-frame-crc : disable stream checksum (default:enabled)
--content-size : compressed frame includes original size (default:not present)
--[no-]sparse  : sparse mode (default:enabled on file, disabled on stdout)
--favor-decSpeed: compressed files decompress faster, but are less compressed
--fast[=#]: switch to ultra fast compression level (default: 1)

Benchmark arguments :
 -b#    : benchmark file(s), using # compression level (default : 1)
 -e#    : test all compression levels from -bX to # (default : 1)
 -i#    : minimum evaluation time in seconds (default : 3s)```
```

#### License

All files in this directory are licensed under GPL-v2.
See [COPYING](COPYING) for details.
The text of the license is also included at the top of each source file.
