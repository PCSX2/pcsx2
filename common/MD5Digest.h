// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Types.h"

class MD5Digest
{
public:
  MD5Digest();

  void Update(const void* pData, u32 cbData);
  void Final(u8 Digest[16]);
  void Reset();

private:
  u32 buf[4];
  u32 bits[2];
  u8 in[64];
};
