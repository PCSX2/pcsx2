/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "FileUtils.h"
#include <wx/file.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include "ghc/filesystem.h"
#include <wx/utils.h>

// ---------------------------------------------------------------------------------
//  Path namespace
// ---------------------------------------------------------------------------------
bool FileUtils::IsDirectoryWithinDirectory(const fs::path& base, const fs::path& dir)
{
    fs::path relativePath = fs::relative(fs::absolute(dir), fs::absolute(base));
    if (relativePath.empty())
    {
        return false;
    }
    return relativePath.string().rfind("..", 0) != 0;
}

u64 FileUtils::GetFileSize(const fs::path &path)
{
    if (!fs::exists(path))
        return -1;
    return (u64)fs::file_size(path);
}

fs::path FileUtils::Normalize(const fs::path &src)
{
    if (src.empty())
    {
        return fs::path();
    }
    return src.lexically_normal();
}

fs::path FileUtils::GetFilename(const fs::path &src)
{
    if (fs::exists(src))
        return fs::path(src).filename();
    else
        return fs::path();
}

fs::path FileUtils::GetFilenameWithoutExt(const fs::path &src)
{
    if (fs::exists(src))
        return src.stem();
    else
    return fs::path();
}

fs::path FileUtils::GetDirectory(const fs::path &src)
{
    if (fs::exists(src))
        return src.parent_path();
    else
        return fs::path();
}

fs::path FileUtils::GetExecutableDirectory()
{
    fs::path exePath(FileUtils::FromWxString(wxStandardPaths::Get().GetExecutablePath()));
    return exePath.parent_path();
}

// returns the base/root directory of the given path.
// Example /this/that/something.txt -> dest == "/"
fs::path FileUtils::GetRootDirectory(const fs::path &src)
{
    if (fs::exists(src))
        return src.root_directory();
    else
        return fs::path();
}

bool FileUtils::Mkdir(const fs::path& path)
{
    try
    {
        fs::create_directories(path); // An attempt to create the User mode Dir which already exists
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
    return true;
}

bool FileUtils::Rmdir(const fs::path& path)
{
    try
    {
        fs::remove_all(path);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return false;
    }
    return true;
}

wxString FileUtils::ToWxString(const fs::path& path)
{
#ifdef _WIN32
    return wxString(path.wstring());
#else
    return wxString(path.string());
#endif
}

fs::path FileUtils::FromWxString(const wxString& path)
{
#ifdef _WIN32
    return fs::path(path.ToStdWstring());
#else
    return fs::path(path.ToStdString());
#endif
}

