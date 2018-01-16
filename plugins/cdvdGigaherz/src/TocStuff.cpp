/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
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

#include "CDVD.h"

#include <algorithm>
#include <array>

void cdvdParseTOC()
{
    tracks[1].start_lba = 0;

    if (!src->GetSectorCount()) {
        curDiskType = CDVD_TYPE_NODISC;
        strack = 1;
        etrack = 0;
        return;
    }

    if (src->GetMediaType() >= 0) {
        tracks[1].type = CDVD_MODE1_TRACK;

        strack = 1;
        etrack = 1;
        return;
    }

    strack = 0xFF;
    etrack = 0;

    for (auto &entry : src->ReadTOC()) {
        if (entry.track < 1 || entry.track > 99)
            continue;
        strack = std::min(strack, entry.track);
        etrack = std::max(etrack, entry.track);
        tracks[entry.track].start_lba = entry.lba;
        if ((entry.control & 0x0C) == 0x04) {
            std::array<u8, 2352> buffer;
            // Byte 15 of a raw CD data sector determines the track mode
            if (src->ReadSectors2352(entry.lba, 1, buffer.data()) && (buffer[15] & 3) == 2) {
                tracks[entry.track].type = CDVD_MODE2_TRACK;
            } else {
                tracks[entry.track].type = CDVD_MODE1_TRACK;
            }
        } else {
            tracks[entry.track].type = CDVD_AUDIO_TRACK;
        }
        fprintf(stderr, "Track %u start sector: %u\n", entry.track, entry.lba);
    }
}
