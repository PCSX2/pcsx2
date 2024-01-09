// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

/* A reference client implementation for interfacing with PINE is available
 * here: https://code.govanify.com/govanify/pine/ */

#pragma once

// PINE uses a concept of "slot" to be able to communicate with multiple
// emulators at the same time, each slot should be unique to each emulator to
// allow PnP and configurable by the end user so that several runs don't
// conflict with each others
#define PINE_DEFAULT_SLOT 28011

namespace PINEServer
{
	bool IsInitialized();
	int GetSlot();

	bool Initialize(int slot = PINE_DEFAULT_SLOT);
	void Deinitialize();
} // namespace PINEServer
