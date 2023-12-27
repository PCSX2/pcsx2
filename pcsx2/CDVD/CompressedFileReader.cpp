// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "AsyncFileReader.h"
#include "CompressedFileReader.h"
#include "ChdFileReader.h"
#include "CsoFileReader.h"
#include "GzippedFileReader.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <cctype>

AsyncFileReader* CompressedFileReader::GetNewReader(const std::string& fileName)
{
	if (!FileSystem::FileExists(fileName.c_str()))
		return nullptr;

	const std::string_view extension = Path::GetExtension(fileName);

	if (StringUtil::compareNoCase(extension, "chd"))
		return new ChdFileReader();
	
	if (StringUtil::compareNoCase(extension, "cso") || StringUtil::compareNoCase(extension, "zso"))
		return new CsoFileReader();
	
	if (StringUtil::compareNoCase(extension, "gz"))
		return new GzippedFileReader();

	// Not a known compressed format.
	return nullptr;
}
