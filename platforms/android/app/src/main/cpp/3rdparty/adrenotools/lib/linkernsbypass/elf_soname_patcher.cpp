// SPDX-License-Identifier: BSD-2-Clause
// Copyright © 2021 Billy Laws

#include <initializer_list>
#include <cstdint>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <link.h>
#include <elf.h>
#include "elf_soname_patcher.h"

bool elf_soname_patch(const char *libPath, int targetFd, const char *sonamePatch) {
    struct stat libStat{};
    if (stat(libPath, &libStat))
        return false;

    if (ftruncate(targetFd, libStat.st_size) == -1)
        return false;

    // Map the memory so we can read our elf into it
    auto mappedLib{reinterpret_cast<uint8_t *>(mmap(nullptr, libStat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, targetFd, 0))};
    if (!mappedLib)
        return false;

    int libFd{open(libPath, O_RDONLY)};
    if (libFd == false)
        return false;

    // Read lib elf into target file
    if (read(libFd, mappedLib, libStat.st_size) != libStat.st_size)
        return false;

    // No longer needed
    close(libFd);

    auto eHdr{reinterpret_cast<ElfW(Ehdr) *>(mappedLib)};
    auto sHdrEntries{reinterpret_cast<ElfW(Shdr) *>(mappedLib + eHdr->e_shoff)};

    // Iterate over section headers to find the .dynamic section
    for (ElfW(Half) i{}; i < eHdr->e_shnum; i++) {
        auto &sHdr{sHdrEntries[i]};
        if (sHdr.sh_type == SHT_DYNAMIC) {
            auto strTab{reinterpret_cast<char *>(mappedLib + sHdrEntries[sHdr.sh_link].sh_offset)};
            auto dynHdrEntries{reinterpret_cast<ElfW(Dyn) *>(mappedLib + sHdr.sh_offset)};

            // Iterate over .dynamic entries to find DT_SONAME
            for (ElfW(Xword) k{}; k < (sHdr.sh_size / sHdr.sh_entsize); k++) {
                auto &dynHdrEntry{dynHdrEntries[k]};
                if (dynHdrEntry.d_tag == DT_SONAME) {
                    char *soname{strTab + dynHdrEntry.d_un.d_val};

                    // Partially replace the old soname with the soname patch
                    size_t charIdx{};
                    for (; soname[charIdx] != 0 && sonamePatch[charIdx] != 0; charIdx++)
                        soname[charIdx] = sonamePatch[charIdx];

                    return true;
                }
            }
        }
    }

    return false;
}
