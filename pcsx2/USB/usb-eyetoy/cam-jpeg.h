// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Pcsx2Defs.h"

#include <vector>

bool CompressCamJPEG(std::vector<u8>* buffer, const u8* image, u32 width, u32 height, int quality);
bool DecompressCamJPEG(std::vector<u8>* buffer, u32* width, u32* height, const u8* data, size_t data_size);