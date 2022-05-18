/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2016  PCSX2 Dev Team
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
#include "Common.h"
#include "R3000A.h"
#include "IopMem.h"

#include "fmt/core.h"

static std::string psxout_buf;

// This filtering should almost certainly be done in the console classes instead
static std::string psxout_last;
static unsigned psxout_repeat;

static void flush_stdout(bool closing = false)
{
    size_t linelen;

    while (!psxout_buf.empty()) {
        linelen = psxout_buf.find_first_of("\n\0", 0, 2);
        if (linelen == std::string::npos) {
            if (!closing)
                return;
        } else
            psxout_buf[linelen++] = '\n';
        if (linelen != 1) {
            if (!psxout_buf.compare(0, linelen, psxout_last))
                psxout_repeat++;
            else {
                if (psxout_repeat) {
                    iopConLog(fmt::format("[{} more]\n", psxout_repeat));
                    psxout_repeat = 0;
                }
                psxout_last = psxout_buf.substr(0, linelen);
                iopConLog(ShiftJIS_ConvertString(psxout_last.data()));
            }
        }
        psxout_buf.erase(0, linelen);
    }
    if (closing && psxout_repeat) {
        iopConLog(fmt::format("[{} more]\n", psxout_repeat));
        psxout_repeat = 0;
    }
}

void psxBiosReset()
{
    flush_stdout(true);
}

// Called for PlayStation BIOS calls at 0xA0, 0xB0 and 0xC0 in kernel reserved memory (seemingly by actually calling those addresses)
// Returns true if we internally process the call, not that we're likely to do any such thing
bool psxBiosCall()
{
    // TODO: Tracing
    // TODO (maybe, psx is hardly a priority): HLE framework

    switch ((psxRegs.pc & 0x1FFFFFFFU) << 4 & 0xf00 | psxRegs.GPR.n.t1 & 0xff) {
        case 0xa03:
        case 0xb35:
            // write(fd, data, size)
            {
                int fd = psxRegs.GPR.n.a0;
                if (fd != 1)
                    return false;

                u32 data = psxRegs.GPR.n.a1;
                u32 size = psxRegs.GPR.n.a2;
                while (size--)
                    psxout_buf.push_back(iopMemRead8(data++));
                flush_stdout(false);
                return false;
            }
        case 0xa09:
        case 0xb3b:
            // putc(c, fd)
            if (psxRegs.GPR.n.a1 != 1)
                return false;
            [[fallthrough]];
        // fd=1, fall through to putchar
        case 0xa3c:
        case 0xb3d:
            // putchar(c)
            psxout_buf.push_back((char)psxRegs.GPR.n.a0);
            flush_stdout(false);
            return false;
        case 0xa3e:
        case 0xb3f:
            // puts(s)
            {
                u32 str = psxRegs.GPR.n.a0;
                while (char c = iopMemRead8(str++))
                    psxout_buf.push_back(c);
                psxout_buf.push_back('\n');
                flush_stdout(false);
                return false;
            }
    }

    return false;
}
