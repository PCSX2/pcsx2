/***************************************************************************************************

  Zyan Core Library (Zycore-C)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

/**
 * @file    Provides cross-platform terminal helper functions.
 * @brief
 */

#ifndef ZYCORE_API_TERMINAL_H
#define ZYCORE_API_TERMINAL_H

#include <Zycore/LibC.h>
#include <Zycore/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZYAN_NO_LIBC

/* ============================================================================================== */
/* VT100 CSI SGR sequences                                                                        */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* General                                                                                        */
/* ---------------------------------------------------------------------------------------------- */

#define ZYAN_VT100SGR_RESET             "\033[0m"

/* ---------------------------------------------------------------------------------------------- */
/* Foreground colors                                                                              */
/* ---------------------------------------------------------------------------------------------- */

#define ZYAN_VT100SGR_FG_DEFAULT        "\033[39m"

#define ZYAN_VT100SGR_FG_BLACK          "\033[30m"
#define ZYAN_VT100SGR_FG_RED            "\033[31m"
#define ZYAN_VT100SGR_FG_GREEN          "\033[32m"
#define ZYAN_VT100SGR_FG_YELLOW         "\033[33m"
#define ZYAN_VT100SGR_FG_BLUE           "\033[34m"
#define ZYAN_VT100SGR_FG_MAGENTA        "\033[35m"
#define ZYAN_VT100SGR_FG_CYAN           "\033[36m"
#define ZYAN_VT100SGR_FG_WHITE          "\033[37m"
#define ZYAN_VT100SGR_FG_BRIGHT_BLACK   "\033[90m"
#define ZYAN_VT100SGR_FG_BRIGHT_RED     "\033[91m"
#define ZYAN_VT100SGR_FG_BRIGHT_GREEN   "\033[92m"
#define ZYAN_VT100SGR_FG_BRIGHT_YELLOW  "\033[93m"
#define ZYAN_VT100SGR_FG_BRIGHT_BLUE    "\033[94m"
#define ZYAN_VT100SGR_FG_BRIGHT_MAGENTA "\033[95m"
#define ZYAN_VT100SGR_FG_BRIGHT_CYAN    "\033[96m"
#define ZYAN_VT100SGR_FG_BRIGHT_WHITE   "\033[97m"

/* ---------------------------------------------------------------------------------------------- */
/* Background color                                                                               */
/* ---------------------------------------------------------------------------------------------- */

#define ZYAN_VT100SGR_BG_DEFAULT        "\033[49m"

#define ZYAN_VT100SGR_BG_BLACK          "\033[40m"
#define ZYAN_VT100SGR_BG_RED            "\033[41m"
#define ZYAN_VT100SGR_BG_GREEN          "\033[42m"
#define ZYAN_VT100SGR_BG_YELLOW         "\033[43m"
#define ZYAN_VT100SGR_BG_BLUE           "\033[44m"
#define ZYAN_VT100SGR_BG_MAGENTA        "\033[45m"
#define ZYAN_VT100SGR_BG_CYAN           "\033[46m"
#define ZYAN_VT100SGR_BG_WHITE          "\033[47m"
#define ZYAN_VT100SGR_BG_BRIGHT_BLACK   "\033[100m"
#define ZYAN_VT100SGR_BG_BRIGHT_RED     "\033[101m"
#define ZYAN_VT100SGR_BG_BRIGHT_GREEN   "\033[102m"
#define ZYAN_VT100SGR_BG_BRIGHT_YELLOW  "\033[103m"
#define ZYAN_VT100SGR_BG_BRIGHT_BLUE    "\033[104m"
#define ZYAN_VT100SGR_BG_BRIGHT_MAGENTA "\033[105m"
#define ZYAN_VT100SGR_BG_BRIGHT_CYAN    "\033[106m"
#define ZYAN_VT100SGR_BG_BRIGHT_WHITE   "\033[107m"

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * Declares the `ZyanStandardStream` enum.
 */
typedef enum ZyanStandardStream_
{
    /**
     * The default input stream.
     */
    ZYAN_STDSTREAM_IN,
    /**
     * The default output stream.
     */
    ZYAN_STDSTREAM_OUT,
    /**
     * The default error stream.
     */
    ZYAN_STDSTREAM_ERR
} ZyanStandardStream;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Enables VT100 ansi escape codes for the given stream.
 *
 * @param   stream  Either `ZYAN_STDSTREAM_OUT` or `ZYAN_STDSTREAM_ERR`.
 *
 * @return  A zyan status code.
 *
 * This functions returns `ZYAN_STATUS_SUCCESS` on all non-Windows systems without performing any
 * operations, assuming that VT100 is supported by default.
 *
 * On Windows systems, VT100 functionality is only supported on Windows 10 build 1607 (anniversary
 * update) and later.
 */
ZYCORE_EXPORT ZyanStatus ZyanTerminalEnableVT100(ZyanStandardStream stream);

/**
 * Checks, if the given standard stream reads from or writes to a terminal.
 *
 * @param   stream  The standard stream to check.
 *
 * @return  `ZYAN_STATUS_TRUE`, if the stream is bound to a terminal, `ZYAN_STATUS_FALSE` if not,
 *          or another zyan status code if an error occured.
 */
ZYCORE_EXPORT ZyanStatus ZyanTerminalIsTTY(ZyanStandardStream stream);

/* ============================================================================================== */

#endif // ZYAN_NO_LIBC

#ifdef __cplusplus
}
#endif

#endif /* ZYCORE_API_TERMINAL_H */
