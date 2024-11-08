// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// This file provides the same information as svnrev.h except you don't need to
// recompile each object file using it when said information is updated.
namespace BuildVersion
{
	extern const char* GitTag;
	extern bool GitTaggedCommit;
	extern int GitTagHi;
	extern int GitTagMid;
	extern int GitTagLo;
	extern const char* GitRev;
	extern const char* GitHash;
	extern const char* GitDate;
} // namespace BuildVersion
