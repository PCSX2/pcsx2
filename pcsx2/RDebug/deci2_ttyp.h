// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Common.h"
#include "deci2.h"

//void D2_(char *inbuffer, char *outbuffer, char *message);
void sendTTYP(u16 protocol, u8 source, char *data);
