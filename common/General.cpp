// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "General.h"

// --------------------------------------------------------------------------------------
//  PageProtectionMode  (implementations)
// --------------------------------------------------------------------------------------
std::string PageProtectionMode::ToString() const
{
	std::string modeStr;

	if (m_read)
		modeStr += "Read";
	if (m_write)
		modeStr += "Write";
	if (m_exec)
		modeStr += "Exec";

	if (modeStr.empty())
		return "NoAccess";
	if (modeStr.length() <= 5)
		modeStr += "Only";

	return modeStr;
}
