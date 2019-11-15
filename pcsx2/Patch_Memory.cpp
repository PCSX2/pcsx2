/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#define _PC_ // disables MIPS opcode macros.

#include "IopCommon.h"
#include "Patch.h"
#include <chrono>

u32 SkipCount = 0, IterationCount = 0;
u32 IterationIncrement = 0, ValueIncrement = 0;
u32 PrevCheatType = 0, PrevCheatAddr = 0, LastType = 0;

// Planning to make a stopwatch class to make this much easier and cleaner. Will work for now, however.
std::chrono::time_point<std::chrono::high_resolution_clock> StartTime, FinishTime;
bool isStarted = false;

void writeCheat()
{
    switch (LastType) {
        case 0x0:
            memWrite8(PrevCheatAddr, IterationIncrement & 0xFF);
            break;
        case 0x1:
            memWrite16(PrevCheatAddr, IterationIncrement & 0xFFFF);
            break;
        case 0x2:
            memWrite32(PrevCheatAddr, IterationIncrement);
            break;
        default:
            break;
    }
}

void handle_extended_t(IniPatch *p)
{
    if (SkipCount > 0)
        SkipCount--;

    else
        switch (PrevCheatType) {
            case 0x3040: {
                u32 mem = memRead32(PrevCheatAddr);
                memWrite32(PrevCheatAddr, mem + (p->addr));
                PrevCheatType = 0;
            } break;

            case 0x3050: {
                u32 mem = memRead32(PrevCheatAddr);
                memWrite32(PrevCheatAddr, mem - (p->addr));
                PrevCheatType = 0;
                break;
            }

            case 0x4000:
                for (u32 i = 0; i < IterationCount; i++)
                    memWrite32((u32)(PrevCheatAddr + (i * IterationIncrement)), (u32)(p->addr + ((u32)p->data * i)));

                PrevCheatType = 0;
                break;

            case 0x5000:
                for (u32 i = 0; i < IterationCount; i++) {
                    u8 mem = memRead8(PrevCheatAddr + i);
                    memWrite8((p->addr + i) & 0x0FFFFFFF, mem);
                }
                PrevCheatType = 0;
                break;

            case 0x6000: {
                if (((u32)p->addr & 0x0000FFFF) == 0)
                    IterationCount = 1;
                else
                    IterationCount = (u32)p->addr & 0x0000FFFF;

                LastType = ((u32)p->addr & 0x000F0000) >> 16;
                u32 mem = memRead32(PrevCheatAddr);

                PrevCheatAddr = mem + (u32)p->data;
                IterationCount--;

                if (IterationCount == 0) {
                    PrevCheatType = 0;
                    if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
                        writeCheat();
                } else {
                    if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) == 0)
                        PrevCheatType = 0;
                    else
                        PrevCheatType = 0x6001;
                }
            } break;

            case 0x6001: {
                // Read first pointer
                u32 mem = memRead32(PrevCheatAddr & 0x0FFFFFFF);

                PrevCheatAddr = mem + (u32)p->addr;
                IterationCount--;

                // Check if needed to read another pointer
                if (IterationCount == 0) {
                    PrevCheatType = 0;
                    if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
                        writeCheat();
                } else {
                    mem = memRead32(PrevCheatAddr);

                    PrevCheatAddr = mem + (u32)p->data;
                    IterationCount--;
                    if (IterationCount == 0) {
                        PrevCheatType = 0;
                        if (((mem & 0x0FFFFFFF) & 0x3FFFFFFC) != 0)
                            writeCheat();
                    }
                }
            } break;

            case 0x8000: {
                u32 mem = memRead32(PrevCheatAddr);
                memWrite32(((u32)p->addr & 0x0FFFFFFF), mem);
                PrevCheatType = 0;
            } break;

            case 0xA000: {
                u32 address = memRead32((u32)p->addr & 0x0FFFFFFF) + (u32)p->data;
                u32 data = memRead32(PrevCheatAddr);
                memWrite32(address, data);
                PrevCheatType = 0;
            } break;

            case 0xC000: {
                u32 typeData = (u32)p->data & 0xF0000000;
                u32 typeComp = (u32)p->data & 0x0F000000;
                u32 skipLines = (u32)p->data & 0x00FFFFFF;

                switch (typeComp) {
                    case 0x0000000: {
                        switch (typeData) {
                            case 0x00000000: {
                                u32 comp1 = memRead32(PrevCheatAddr);
                                u32 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

								if (comp1 != comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x10000000: {
                                u16 comp1 = memRead16(PrevCheatAddr);
                                u16 comp2 = memRead16((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 != comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x20000000: {
                                u8 comp1 = memRead32(PrevCheatAddr);
                                u8 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 != comp2)
                                    SkipCount = skipLines;
                            } break;
                        }
                    } break;
                    case 0x1000000: {
                        switch (typeData) {
                            case 0x00000000: {
                                u32 comp1 = memRead32(PrevCheatAddr);
                                u32 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 == comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x10000000: {
                                u16 comp1 = memRead16(PrevCheatAddr);
                                u16 comp2 = memRead16((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 == comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x20000000: {
                                u8 comp1 = memRead32(PrevCheatAddr);
                                u8 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 == comp2)
                                    SkipCount = skipLines;
                            } break;
                        }
                    } break;
                    case 0x2000000: {
                        switch (typeData) {
                            case 0x00000000: {
                                u32 comp1 = memRead32(PrevCheatAddr);
                                u32 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);
								
                                if (comp1 > comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x10000000: {
                                u16 comp1 = memRead16(PrevCheatAddr);
                                u16 comp2 = memRead16((u32)p->addr & 0x0FFFFFFF);

								if (comp1 > comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x20000000: {
                                u8 comp1 = memRead32(PrevCheatAddr);
                                u8 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

								if (comp1 > comp2)
                                    SkipCount = skipLines;
                            } break;
                        }
                    } break;
                    case 0x3000000: {
                        switch (typeData) {
                            case 0x00000000: {
                                u32 comp1 = memRead32(PrevCheatAddr);
                                u32 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

								if (comp1 < comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x10000000: {
                                u16 comp1 = memRead16(PrevCheatAddr);
                                u16 comp2 = memRead16((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 < comp2)
                                    SkipCount = skipLines;
                            } break;
                            case 0x20000000: {
                                u8 comp1 = memRead32(PrevCheatAddr);
                                u8 comp2 = memRead32((u32)p->addr & 0x0FFFFFFF);

                                if (comp1 < comp2)
                                    SkipCount = skipLines;
                            } break;
                        }
                    } break;
                }

				PrevCheatType = 0;
            } break;

            default:
                switch (p->addr & 0xF0000000) {
                    case 0x00000000: // 8-Bit Write
                        memWrite8(p->addr & 0x0FFFFFFF, (u8)p->data & 0x000000FF);
                        PrevCheatType = 0;
                        break;

                    case 0x10000000: // 16-Bit Write
                        memWrite16(p->addr & 0x0FFFFFFF, (u16)p->data & 0x0000FFFF);
                        PrevCheatType = 0;
                        break;

                    case 0x20000000: // 32-Bit Write
                        memWrite32(p->addr & 0x0FFFFFFF, (u32)p->data);
                        PrevCheatType = 0;
                        break;

                    case 0x30000000: { // Increment / Decrement
                        u8 mem8 = memRead8((u32)p->data);
                        u16 mem16 = memRead16((u32)p->data);

                        switch (p->addr & 0x00F00000) {
                            case 0x000000:
                                memWrite8((u32)p->data, mem8 + (p->addr & 0x000000FF));
                                break;
                            case 0x100000:
                                memWrite8((u32)p->data, mem8 - (p->addr & 0x000000FF));
                                break;
                            case 0x200000:
                                memWrite16((u32)p->data, mem16 + (p->addr & 0x000000FF));
                                break;
                            case 0x300000:
                                memWrite16((u32)p->data, mem16 - (p->addr & 0x000000FF));
                                break;
                            case 0x400000:
                                PrevCheatType = 0x3040;
                                PrevCheatAddr = (u32)p->data;
                                break;
                            case 0x500000:
                                PrevCheatType = 0x3050;
                                PrevCheatAddr = (u32)p->data;
                                break;

                                if (PrevCheatType != 0x3040 && PrevCheatType != 0x3050)
                                    PrevCheatType = 0;
                        }
                    } break;

                    case 0x40000000: // Multi-Write
                        IterationCount = ((u32)p->data & 0xFFFF0000) / 0x10000;
                        IterationIncrement = ((u32)p->data & 0x0000FFFF) * 4;
                        PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
                        PrevCheatType = 0x4000;
                        break;

                    case 0x50000000: // Copy Bytes
                        PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
                        IterationCount = ((u32)p->data);
                        PrevCheatType = 0x5000;
                        break;

                    case 0x60000000: // Pointer-Write
                        PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
                        IterationIncrement = ((u32)p->data);
                        IterationCount = 0;
                        PrevCheatType = 0x6000;
                        break;

                    case 0x70000000: { // Bitwise Operations
                        u8 mem8 = memRead8((u32)p->addr & 0x0FFFFFFF);
                        u16 mem16 = memRead16((u32)p->addr & 0x0FFFFFFF);

                        switch (p->data & 0x00F00000) {
                            case 0x000000:
                                memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem8 | (p->data & 0x000000FF)));
                                break;
                            case 0x100000:
                                memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem16 | (p->data & 0x0000FFFF)));
                                break;
                            case 0x200000:
                                memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem8 & (p->data & 0x000000FF)));
                                break;
                            case 0x300000:
                                memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem16 & (p->data & 0x0000FFFF)));
                                break;
                            case 0x400000:
                                memWrite8((u32)p->addr & 0x0FFFFFFF, (u8)(mem8 ^ (p->data & 0x000000FF)));
                                break;
                            case 0x500000:
                                memWrite16((u32)p->addr & 0x0FFFFFFF, (u16)(mem16 ^ (p->data & 0x0000FFFF)));
                                break;

                                PrevCheatType = 0;
                        }
                    } break;

                    case 0x80000000: { // Pointer Data Copy
                        PrevCheatAddr = memRead32((u32)p->addr & 0x0FFFFFFF) + (u32)p->data;
                        PrevCheatType = 0x8000;
                    } break;

                    case 0x90000000: { // Single Copy
                        u32 mem = memRead32((u32)p->addr & 0x0FFFFFFF);
                        memWrite32((u32)p->data & 0x0FFFFFFF, mem);
                        PrevCheatType = 0;
                    } break;

                    case 0xA0000000: { // Copy to Pointer
                        PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
                        PrevCheatType = 0xA000;
                    } break;

                    case 0xB0000000: {
                        SkipCount = ((u32)p->addr & 0x0FF00000) / 0x100000;

                        if (!isStarted) {
                            StartTime = std::chrono::high_resolution_clock::now();
                            isStarted = true;
                        }

                        else {
                            FinishTime = std::chrono::high_resolution_clock::now();
                            long long ElapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(FinishTime - StartTime).count();

                            if (ElapsedTime >= (long long)((u32)p->addr & 0x000FFFFF)) {
                                isStarted = false;
                                SkipCount = 0;
                            }
                        }
                    } break;

                    case 0xC0000000: {
                        PrevCheatAddr = (u32)p->addr & 0x0FFFFFFF;
                        PrevCheatType = 0xC000;
                    } break;

                    case 0xD0000000: { // IF Statements (Single)
                        u16 mem = memRead16((u32)p->addr & 0x0FFFFFFF);

                        switch (p->data & 0xFFFF0000) {
                            case 0x00000000:
                                if (mem != (0x0000FFFF & (u32)p->data))
                                    SkipCount = 1;
                                break;
                            case 0x00100000:
                                if (mem == (0x0000FFFF & (u32)p->data))
                                    SkipCount = 1;
                                break;
                            case 0x00200000:
                                if (mem > (0x0000FFFF & (u32)p->data))
                                    SkipCount = 1;
                                break;
                            case 0x00300000:
                                if (mem < (0x0000FFFF & (u32)p->data))
                                    SkipCount = 1;
                                break;

                                PrevCheatType = 0;
                        }
                    } break;

                    case 0xE0000000: { // IF Statements (Multi)
                        u8 mem8 = memRead8((u32)p->data & 0x0FFFFFFF);
                        u16 mem16 = memRead16((u32)p->data & 0x0FFFFFFF);

                        u32 type = (u32)p->addr & 0x0F000000;
                        u32 comp = (u32)p->data & 0xF0000000;
                        u32 cond8 = (u32)p->addr & 0x000000FF;
                        u32 cond16 = (u32)p->addr & 0x0000FFFF;
                        u32 skip = (u32)p->addr & 0x00FF0000;

                        switch (type) {
                            case 0x00000000: {
                                switch (comp) {
                                    case 0x00000000:
                                        if (mem16 != cond16)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x10000000:
                                        if (mem16 == cond16)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x20000000:
                                        if (mem16 > cond16)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x30000000:
                                        if (mem16 < cond16)
                                            SkipCount = skip / 0x10000;
                                        break;
                                }
                            } break;

                            case 0x01000000: {
                                switch (comp) {
                                    case 0x00000000:
                                        if (mem8 != cond8)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x10000000:
                                        if (mem8 == cond8)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x20000000:
                                        if (mem8 > cond8)
                                            SkipCount = skip / 0x10000;
                                        break;
                                    case 0x30000000:
                                        if (mem8 < cond8)
                                            SkipCount = skip / 0x10000;
                                        break;
                                }
                            } break;
                                PrevCheatType = 0;
                        }
                    } break;
                }
                break;
        }
}

// Only used from Patch.cpp and we don't export this in any h file.
// Patch.cpp itself declares this prototype, so make sure to keep in sync.
void _ApplyPatch(IniPatch *p)
{
    if (p->enabled == 0)
        return;

    switch (p->cpu) {
        case CPU_EE:
            switch (p->type) {
                case BYTE_T:
                    if (memRead8(p->addr) != (u8)p->data)
                        memWrite8(p->addr, (u8)p->data);
                    break;

                case SHORT_T:
                    if (memRead16(p->addr) != (u16)p->data)
                        memWrite16(p->addr, (u16)p->data);
                    break;

                case WORD_T:
                    if (memRead32(p->addr) != (u32)p->data)
                        memWrite32(p->addr, (u32)p->data);
                    break;

                case DOUBLE_T:
                    u64 mem;
                    memRead64(p->addr, &mem);
                    if (mem != p->data)
                        memWrite64(p->addr, &p->data);
                    break;

                case EXTENDED_T:
                    handle_extended_t(p);
                    break;

                default:
                    break;
            }
            break;

        case CPU_IOP:
            switch (p->type) {
                case BYTE_T:
                    if (iopMemRead8(p->addr) != (u8)p->data)
                        iopMemWrite8(p->addr, (u8)p->data);
                    break;
                case SHORT_T:
                    if (iopMemRead16(p->addr) != (u16)p->data)
                        iopMemWrite16(p->addr, (u16)p->data);
                    break;
                case WORD_T:
                    if (iopMemRead32(p->addr) != (u32)p->data)
                        iopMemWrite32(p->addr, (u32)p->data);
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}
