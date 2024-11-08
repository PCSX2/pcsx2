// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "svnrev.h"

namespace BuildVersion
{
	const char* GitTag = GIT_TAG;
	bool GitTaggedCommit = GIT_TAGGED_COMMIT;
	int GitTagHi = GIT_TAG_HI;
	int GitTagMid = GIT_TAG_MID;
	int GitTagLo = GIT_TAG_LO;
	const char* GitRev = GIT_REV;
	const char* GitHash = GIT_HASH;
	const char* GitDate = GIT_DATE;
} // namespace BuildVersion
