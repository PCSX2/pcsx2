/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

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
