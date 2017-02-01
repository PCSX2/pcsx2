/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

// Note: std::vsnprintf requires C++11 (and the function must return the total
// number of characters which would have been written even if a truncation occured)
// Note2: Code is only used in debugger (perf is not important)
static void vssappendf(std::string &dest, const char *format, va_list args)
{
    char first_try[128]; // this function is called 99% (100%?) of the times for small string
    va_list args_copy;
    va_copy(args_copy, args);

    s32 size = std::vsnprintf(first_try, 128, format, args_copy) + 1;

    va_end(args_copy);

    if (size < 0)
        return;
    if (size < 128) {
        dest += first_try;
        return;
    }

    std::vector<char> output;
    output.resize(size + 1);
    std::vsnprintf(output.data(), size, format, args);

    dest += output.data();
}

void ssappendf(std::string &dest, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vssappendf(dest, format, args);
    va_end(args);
}
