// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace GameMode {
#if defined(__linux__)
	bool IsAvailable();
	void Update(bool enabled);
#else
	static inline bool IsAvailable() { return false; }
	static inline void Update(bool enabled) {}
#endif
}
