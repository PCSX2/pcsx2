// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "FileSystem.h"
#include "Error.h"
#include "Path.h"
#include "Assertions.h"
#include "Console.h"
#include "StringUtil.h"
#include "Path.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <sys/param.h>
#endif

#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

#if defined(_WIN32)
#include "RedtapeWindows.h"
#include <io.h>
#include <winioctl.h>
#include <share.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static std::time_t ConvertFileTimeToUnixTime(const FILETIME& ft)
{
	// based off https://stackoverflow.com/a/6161842
	static constexpr s64 WINDOWS_TICK = 10000000;
	static constexpr s64 SEC_TO_UNIX_EPOCH = 11644473600LL;

	const s64 full = static_cast<s64>((static_cast<u64>(ft.dwHighDateTime) << 32) | static_cast<u64>(ft.dwLowDateTime));
	return static_cast<std::time_t>(full / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}
#endif

static inline bool FileSystemCharacterIsSane(char c, bool strip_slashes)
{
#ifdef _WIN32
	// https://docs.microsoft.com/en-gb/windows/win32/fileio/naming-a-file?redirectedfrom=MSDN#naming-conventions
	if ((c == U'/' || c == U'\\') && strip_slashes)
		return false;

	if (c == U'<' || c == U'>' || c == U':' || c == U'"' || c == U'|' || c == U'?' || c == U'*' || c == 0 ||
		c <= static_cast<char32_t>(31))
	{
		return false;
	}
#else
	if (c == '/' && strip_slashes)
		return false;

	// drop asterisks too, they make globbing annoying
	if (c == '*')
		return false;

		// macos doesn't allow colons, apparently
#ifdef __APPLE__
	if (c == U':')
		return false;
#endif
#endif

	return true;
}

template <typename T>
static inline void PathAppendString(std::string& dst, const T& src)
{
	if (dst.capacity() < (dst.length() + src.length()))
		dst.reserve(dst.length() + src.length());

	bool last_separator = (!dst.empty() && dst.back() == FS_OSPATH_SEPARATOR_CHARACTER);

	size_t index = 0;

#ifdef _WIN32
	// special case for UNC paths here
	if (dst.empty() && src.length() >= 3 && src[0] == '\\' && src[1] == '\\' && src[2] != '\\')
	{
		dst.append("\\\\");
		index = 2;
	}
#endif

	for (; index < src.length(); index++)
	{
		const char ch = src[index];

#ifdef _WIN32
		// convert forward slashes to backslashes
		if (ch == '\\' || ch == '/')
#else
		if (ch == '/')
#endif
		{
			if (last_separator)
				continue;
			last_separator = true;
			dst.push_back(FS_OSPATH_SEPARATOR_CHARACTER);
		}
		else
		{
			last_separator = false;
			dst.push_back(ch);
		}
	}
}

std::string Path::SanitizeFileName(const std::string_view& str, bool strip_slashes /* = true */)
{
	std::string ret;
	ret.reserve(str.length());

	size_t pos = 0;
	while (pos < str.length())
	{
		char32_t ch;
		pos += StringUtil::DecodeUTF8(str, pos, &ch);
		ch = FileSystemCharacterIsSane(ch, strip_slashes) ? ch : U'_';
		StringUtil::EncodeAndAppendUTF8(ret, ch);
	}

#ifdef _WIN32
	// Windows: Can't end filename with a period.
	if (ret.length() > 0 && ret.back() == '.')
		ret.back() = '_';
#endif

	return ret;
}

void Path::SanitizeFileName(std::string* str, bool strip_slashes /* = true */)
{
	const size_t len = str->length();

	char small_buf[128];
	std::unique_ptr<char[]> large_buf;
	char* str_copy = small_buf;
	if (len >= std::size(small_buf))
	{
		large_buf = std::make_unique<char[]>(len + 1);
		str_copy = large_buf.get();
	}
	std::memcpy(str_copy, str->c_str(), sizeof(char) * (len + 1));
	str->clear();

	size_t pos = 0;
	while (pos < len)
	{
		char32_t ch;
		pos += StringUtil::DecodeUTF8(str_copy + pos, pos - len, &ch);
		ch = FileSystemCharacterIsSane(ch, strip_slashes) ? ch : U'_';
		StringUtil::EncodeAndAppendUTF8(*str, ch);
	}

#ifdef _WIN32
	// Windows: Can't end filename with a period.
	if (str->length() > 0 && str->back() == '.')
		str->back() = '_';
#endif
}

bool Path::IsValidFileName(const std::string_view& str, bool allow_slashes)
{
	const size_t len = str.length();
	size_t pos = 0;
	while (pos < len)
	{
		char32_t ch;
		pos += StringUtil::DecodeUTF8(str.data() + pos, pos - len, &ch);
		if (!FileSystemCharacterIsSane(ch, !allow_slashes))
			return false;
	}

#ifdef _WIN32
	// Windows: Can't end filename with a period.
	if (len > 0 && str.back() == '.')
		return false;
#endif

	return true;
}

bool Path::IsAbsolute(const std::string_view& path)
{
#ifdef _WIN32
	return (path.length() >= 3 && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
			   path[1] == ':' && (path[2] == '/' || path[2] == '\\')) ||
		   (path.length() >= 3 && path[0] == '\\' && path[1] == '\\');
#else
	return (path.length() >= 1 && path[0] == '/');
#endif
}

std::string Path::RealPath(const std::string_view& path)
{
	// Resolve non-absolute paths first.
	std::vector<std::string_view> components;
	if (!IsAbsolute(path))
		components = Path::SplitNativePath(Path::Combine(FileSystem::GetWorkingDirectory(), path));
	else
		components = Path::SplitNativePath(path);

	std::string realpath;
	if (components.empty())
		return realpath;

	// Different to path because relative.
	realpath.reserve(std::accumulate(components.begin(), components.end(), static_cast<size_t>(0),
						 [](size_t l, const std::string_view& s) { return l + s.length(); }) +
					 components.size() + 1);

#ifdef _WIN32
	std::wstring wrealpath;
	std::vector<WCHAR> symlink_buf;
	wrealpath.reserve(realpath.size());
	symlink_buf.resize(path.size() + 1);

	// Check for any symbolic links throughout the path while adding components.
	bool test_symlink = true;
	for (const std::string_view& comp : components)
	{
		if (!realpath.empty())
			realpath.push_back(FS_OSPATH_SEPARATOR_CHARACTER);
		realpath.append(comp);
		if (test_symlink)
		{
			DWORD attribs;
			if (StringUtil::UTF8StringToWideString(wrealpath, realpath) &&
				(attribs = GetFileAttributesW(wrealpath.c_str())) != INVALID_FILE_ATTRIBUTES)
			{
				// if not a link, go to the next component
				if (attribs & FILE_ATTRIBUTE_REPARSE_POINT)
				{
					const HANDLE hFile =
						CreateFileW(wrealpath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
							nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
					if (hFile != INVALID_HANDLE_VALUE)
					{
						// is a link! resolve it.
						DWORD ret = GetFinalPathNameByHandleW(hFile, symlink_buf.data(), static_cast<DWORD>(symlink_buf.size()),
							FILE_NAME_NORMALIZED);
						if (ret > symlink_buf.size())
						{
							symlink_buf.resize(ret);
							ret = GetFinalPathNameByHandleW(hFile, symlink_buf.data(), static_cast<DWORD>(symlink_buf.size()),
								FILE_NAME_NORMALIZED);
						}
						if (ret != 0)
							StringUtil::WideStringToUTF8String(realpath, std::wstring_view(symlink_buf.data(), ret));
						else
							test_symlink = false;

						CloseHandle(hFile);
					}
				}
			}
			else
			{
				// not a file or link
				test_symlink = false;
			}
		}
	}

	// GetFinalPathNameByHandleW() adds a \\?\ prefix, so remove it.
	if (realpath.starts_with("\\\\?\\") && IsAbsolute(std::string_view(realpath.data() + 4, realpath.size() - 4)))
		realpath.erase(0, 4);

#else
	// Why this monstrosity instead of calling realpath()? realpath() only works on files that exist.
	std::string basepath;
	std::string symlink;

	basepath.reserve(realpath.capacity());
	symlink.resize(realpath.capacity());

	// Check for any symbolic links throughout the path while adding components.
	bool test_symlink = true;
	for (const std::string_view& comp : components)
	{
		if (!test_symlink)
		{
			realpath.push_back(FS_OSPATH_SEPARATOR_CHARACTER);
			realpath.append(comp);
			continue;
		}

		basepath = realpath;
		if (realpath.empty() || realpath.back() != FS_OSPATH_SEPARATOR_CHARACTER)
			realpath.push_back(FS_OSPATH_SEPARATOR_CHARACTER);
		realpath.append(comp);

		// Check if the last component added is a symlink
		struct stat sb;
		if (lstat(realpath.c_str(), &sb) != 0)
		{
			// Don't bother checking any further components once we error out.
			test_symlink = false;
			continue;
		}
		else if (!S_ISLNK(sb.st_mode))
		{
			// Nope, keep going.
			continue;
		}

		for (;;)
		{
			ssize_t sz = readlink(realpath.c_str(), symlink.data(), symlink.size());
			if (sz < 0)
			{
				// shouldn't happen, due to the S_ISLNK check above.
				test_symlink = false;
				break;
			}
			else if (static_cast<size_t>(sz) == symlink.size())
			{
				// need a larger buffer
				symlink.resize(symlink.size() * 2);
				continue;
			}
			else
			{
				// is a link, and we resolved it. gotta check if the symlink itself is relative :(
				symlink.resize(static_cast<size_t>(sz));
				if (!Path::IsAbsolute(symlink))
				{
					// symlink is relative to the directory of the symlink
					realpath = basepath;
					if (realpath.empty() || realpath.back() != FS_OSPATH_SEPARATOR_CHARACTER)
						realpath.push_back(FS_OSPATH_SEPARATOR_CHARACTER);
					realpath.append(symlink);
				}
				else
				{
					// Use the new, symlinked path.
					realpath = symlink;
				}

				break;
			}
		}
	}
#endif

	return realpath;
}

std::string Path::ToNativePath(const std::string_view& path)
{
	std::string ret;
	PathAppendString(ret, path);

	// remove trailing slashes
	if (ret.length() > 1)
	{
		while (ret.back() == FS_OSPATH_SEPARATOR_CHARACTER)
			ret.pop_back();
	}

	return ret;
}

void Path::ToNativePath(std::string* path)
{
	*path = Path::ToNativePath(*path);
}

std::string Path::Canonicalize(const std::string_view& path)
{
	std::vector<std::string_view> components = Path::SplitNativePath(path);
	std::vector<std::string_view> new_components;
	new_components.reserve(components.size());
	for (const std::string_view& component : components)
	{
		if (component == ".")
		{
			// current directory, so it can be skipped, unless it's the only component
			if (components.size() == 1)
				new_components.push_back(std::move(component));
		}
		else if (component == "..")
		{
			// parent directory, pop one off if we're not at the beginning, otherwise preserve.
			if (!new_components.empty())
				new_components.pop_back();
			else
				new_components.push_back(std::move(component));
		}
		else
		{
			// anything else, preserve
			new_components.push_back(std::move(component));
		}
	}

	return Path::JoinNativePath(new_components);
}

void Path::Canonicalize(std::string* path)
{
	*path = Canonicalize(*path);
}

std::string Path::MakeRelative(const std::string_view& path, const std::string_view& relative_to)
{
	// simple algorithm, we just work on the components. could probably be better, but it'll do for now.
	std::vector<std::string_view> path_components(SplitNativePath(path));
	std::vector<std::string_view> relative_components(SplitNativePath(relative_to));
	std::vector<std::string_view> new_components;

	// both must be absolute paths
	if (Path::IsAbsolute(path) && Path::IsAbsolute(relative_to))
	{
		// find the number of same components
		size_t num_same = 0;
		for (size_t i = 0; i < path_components.size() && i < relative_components.size(); i++)
		{
			if (path_components[i] == relative_components[i])
				num_same++;
			else
				break;
		}

		// we need at least one same component
		if (num_same > 0)
		{
			// from the relative_to directory, back up to the start of the common components
			const size_t num_ups = relative_components.size() - num_same;
			for (size_t i = 0; i < num_ups; i++)
				new_components.emplace_back("..");

			// and add the remainder of the path components
			for (size_t i = num_same; i < path_components.size(); i++)
				new_components.push_back(std::move(path_components[i]));
		}
		else
		{
			// no similarity
			new_components = std::move(path_components);
		}
	}
	else
	{
		// not absolute
		new_components = std::move(path_components);
	}

	return JoinNativePath(new_components);
}

std::string_view Path::GetExtension(const std::string_view& path)
{
	const std::string_view::size_type pos = path.rfind('.');
	if (pos == std::string_view::npos)
		return std::string_view();
	else
		return path.substr(pos + 1);
}

std::string_view Path::StripExtension(const std::string_view& path)
{
	const std::string_view::size_type pos = path.rfind('.');
	if (pos == std::string_view::npos)
		return path;

	return path.substr(0, pos);
}

std::string Path::ReplaceExtension(const std::string_view& path, const std::string_view& new_extension)
{
	const std::string_view::size_type pos = path.rfind('.');
	if (pos == std::string_view::npos)
		return std::string(path);

	std::string ret(path, 0, pos + 1);
	ret.append(new_extension);
	return ret;
}

static std::string_view::size_type GetLastSeperatorPosition(const std::string_view& filename, bool include_separator)
{
	std::string_view::size_type last_separator = filename.rfind('/');
	if (include_separator && last_separator != std::string_view::npos)
		last_separator++;

#if defined(_WIN32)
	std::string_view::size_type other_last_separator = filename.rfind('\\');
	if (other_last_separator != std::string_view::npos)
	{
		if (include_separator)
			other_last_separator++;
		if (last_separator == std::string_view::npos || other_last_separator > last_separator)
			last_separator = other_last_separator;
	}
#endif

	return last_separator;
}

std::string_view Path::GetDirectory(const std::string_view& path)
{
	const std::string::size_type pos = GetLastSeperatorPosition(path, false);
	if (pos == std::string_view::npos)
		return {};

	return path.substr(0, pos);
}

std::string_view Path::GetFileName(const std::string_view& path)
{
	const std::string_view::size_type pos = GetLastSeperatorPosition(path, true);
	if (pos == std::string_view::npos)
		return path;

	return path.substr(pos);
}

std::string_view Path::GetFileTitle(const std::string_view& path)
{
	const std::string_view filename(GetFileName(path));
	const std::string::size_type pos = filename.rfind('.');
	if (pos == std::string_view::npos)
		return filename;

	return filename.substr(0, pos);
}

std::string Path::ChangeFileName(const std::string_view& path, const std::string_view& new_filename)
{
	std::string ret;
	PathAppendString(ret, path);

	const std::string_view::size_type pos = GetLastSeperatorPosition(ret, true);
	if (pos == std::string_view::npos)
	{
		ret.clear();
		PathAppendString(ret, new_filename);
	}
	else
	{
		if (!new_filename.empty())
		{
			ret.erase(pos);
			PathAppendString(ret, new_filename);
		}
		else
		{
			ret.erase(pos - 1);
		}
	}

	return ret;
}

void Path::ChangeFileName(std::string* path, const std::string_view& new_filename)
{
	*path = ChangeFileName(*path, new_filename);
}

std::string Path::AppendDirectory(const std::string_view& path, const std::string_view& new_dir)
{
	std::string ret;
	if (!new_dir.empty())
	{
		const std::string_view::size_type pos = GetLastSeperatorPosition(path, true);

		ret.reserve(path.length() + new_dir.length() + 1);
		if (pos != std::string_view::npos)
			PathAppendString(ret, path.substr(0, pos));

		while (!ret.empty() && ret.back() == FS_OSPATH_SEPARATOR_CHARACTER)
			ret.pop_back();

		if (!ret.empty())
			ret += FS_OSPATH_SEPARATOR_CHARACTER;

		PathAppendString(ret, new_dir);

		if (pos != std::string_view::npos)
		{
			const std::string_view filepart(path.substr(pos));
			if (!filepart.empty())
			{
				ret += FS_OSPATH_SEPARATOR_CHARACTER;
				PathAppendString(ret, filepart);
			}
		}
		else if (!path.empty())
		{
			ret += FS_OSPATH_SEPARATOR_CHARACTER;
			PathAppendString(ret, path);
		}
	}
	else
	{
		PathAppendString(ret, path);
	}

	return ret;
}

void Path::AppendDirectory(std::string* path, const std::string_view& new_dir)
{
	*path = AppendDirectory(*path, new_dir);
}

std::vector<std::string_view> Path::SplitWindowsPath(const std::string_view& path)
{
	std::vector<std::string_view> parts;

	std::string::size_type start = 0;
	std::string::size_type pos = 0;

	// preserve unc paths
	if (path.size() > 2 && path[0] == '\\' && path[1] == '\\')
		pos = 2;

	while (pos < path.size())
	{
		if (path[pos] != '/' && path[pos] != '\\')
		{
			pos++;
			continue;
		}

		// skip consecutive separators
		if (pos != start)
			parts.push_back(path.substr(start, pos - start));

		pos++;
		start = pos;
	}

	if (start != pos)
		parts.push_back(path.substr(start));

	return parts;
}

std::string Path::JoinWindowsPath(const std::vector<std::string_view>& components)
{
	return StringUtil::JoinString(components.begin(), components.end(), '\\');
}

std::vector<std::string_view> Path::SplitNativePath(const std::string_view& path)
{
#ifdef _WIN32
	return SplitWindowsPath(path);
#else
	std::vector<std::string_view> parts;

	std::string::size_type start = 0;
	std::string::size_type pos = 0;
	while (pos < path.size())
	{
		if (path[pos] != '/')
		{
			pos++;
			continue;
		}

		// skip consecutive separators
		// for unix, we create an empty element at the beginning when it's an absolute path
		// that way, when it's re-joined later, we preserve the starting slash.
		if (pos != start || pos == 0)
			parts.push_back(path.substr(start, pos - start));

		pos++;
		start = pos;
	}

	if (start != pos)
		parts.push_back(path.substr(start));

	return parts;
#endif
}

std::string Path::JoinNativePath(const std::vector<std::string_view>& components)
{
	return StringUtil::JoinString(components.begin(), components.end(), FS_OSPATH_SEPARATOR_CHARACTER);
}

std::vector<std::string> FileSystem::GetRootDirectoryList()
{
	std::vector<std::string> results;

#if defined(_WIN32)
	char buf[256];
	const DWORD size = GetLogicalDriveStringsA(sizeof(buf), buf);
	if (size != 0 && size < (sizeof(buf) - 1))
	{
		const char* ptr = buf;
		while (*ptr != '\0')
		{
			const std::size_t len = std::strlen(ptr);
			results.emplace_back(ptr, len);
			ptr += len + 1u;
		}
	}
#else
	const char* home_path = std::getenv("HOME");
	if (home_path)
		results.push_back(home_path);

	results.push_back("/");
#endif

	return results;
}

std::string Path::BuildRelativePath(const std::string_view& filename, const std::string_view& new_filename)
{
	std::string new_string;

	std::string_view::size_type pos = GetLastSeperatorPosition(filename, true);
	if (pos != std::string_view::npos)
		new_string.assign(filename, 0, pos);
	new_string.append(new_filename);
	return new_string;
}

std::string Path::Combine(const std::string_view& base, const std::string_view& next)
{
	std::string ret;
	ret.reserve(base.length() + next.length() + 1);

	PathAppendString(ret, base);
	while (!ret.empty() && ret.back() == FS_OSPATH_SEPARATOR_CHARACTER)
		ret.pop_back();

	ret += FS_OSPATH_SEPARATOR_CHARACTER;
	PathAppendString(ret, next);
	while (!ret.empty() && ret.back() == FS_OSPATH_SEPARATOR_CHARACTER)
		ret.pop_back();

	return ret;
}

std::FILE* FileSystem::OpenCFile(const char* filename, const char* mode, Error* error)
{
#ifdef _WIN32
	const std::wstring wfilename(StringUtil::UTF8StringToWideString(filename));
	const std::wstring wmode(StringUtil::UTF8StringToWideString(mode));
	if (!wfilename.empty() && !wmode.empty())
	{
		std::FILE* fp;
		const errno_t err = _wfopen_s(&fp, wfilename.c_str(), wmode.c_str());
		if (err != 0)
		{
			Error::SetErrno(error, err);
			return nullptr;
		}

		return fp;
	}

	std::FILE* fp;
	const errno_t err = fopen_s(&fp, filename, mode);
	if (err != 0)
	{
		Error::SetErrno(error, err);
		return nullptr;
	}

	return fp;
#else
	std::FILE* fp = std::fopen(filename, mode);
	if (!fp)
		Error::SetErrno(error, errno);
	return fp;
#endif
}

int FileSystem::OpenFDFile(const char* filename, int flags, int mode, Error* error)
{
#ifdef _WIN32
	const std::wstring wfilename(StringUtil::UTF8StringToWideString(filename));
	if (!wfilename.empty())
		return _wopen(wfilename.c_str(), flags, mode);

	return -1;
#else
	const int fd = open(filename, flags, mode);
	if (fd < 0)
		Error::SetErrno(error, errno);
	return fd;
#endif
}

FileSystem::ManagedCFilePtr FileSystem::OpenManagedCFile(const char* filename, const char* mode, Error* error)
{
	return ManagedCFilePtr(OpenCFile(filename, mode, error));
}

std::FILE* FileSystem::OpenSharedCFile(const char* filename, const char* mode, FileShareMode share_mode, Error* error)
{
#ifdef _WIN32
	const std::wstring wfilename(StringUtil::UTF8StringToWideString(filename));
	const std::wstring wmode(StringUtil::UTF8StringToWideString(mode));
	if (wfilename.empty() || wmode.empty())
		return nullptr;

	int share_flags = 0;
	switch (share_mode)
	{
		case FileShareMode::DenyNone:
			share_flags = _SH_DENYNO;
			break;
		case FileShareMode::DenyRead:
			share_flags = _SH_DENYRD;
			break;
		case FileShareMode::DenyWrite:
			share_flags = _SH_DENYWR;
			break;
		case FileShareMode::DenyReadWrite:
		default:
			share_flags = _SH_DENYRW;
			break;
	}

	std::FILE* fp = _wfsopen(wfilename.c_str(), wmode.c_str(), share_flags);
	if (fp)
		return fp;

	Error::SetErrno(error, errno);
	return nullptr;
#else
	std::FILE* fp = std::fopen(filename, mode);
	if (!fp)
		Error::SetErrno(error, errno);
	return fp;
#endif
}

FileSystem::ManagedCFilePtr FileSystem::OpenManagedSharedCFile(const char* filename, const char* mode, FileShareMode share_mode, Error* error)
{
	return ManagedCFilePtr(OpenSharedCFile(filename, mode, share_mode, error));
}

int FileSystem::FSeek64(std::FILE* fp, s64 offset, int whence)
{
#ifdef _WIN32
	return _fseeki64(fp, offset, whence);
#else
	return fseeko(fp, static_cast<off_t>(offset), whence);
#endif
}

s64 FileSystem::FTell64(std::FILE* fp)
{
#ifdef _WIN32
	return static_cast<s64>(_ftelli64(fp));
#else
	return static_cast<s64>(ftello(fp));
#endif
}

s64 FileSystem::FSize64(std::FILE* fp)
{
	const s64 pos = FTell64(fp);
	if (pos >= 0)
	{
		if (FSeek64(fp, 0, SEEK_END) == 0)
		{
			const s64 size = FTell64(fp);
			if (FSeek64(fp, pos, SEEK_SET) == 0)
				return size;
		}
	}

	return -1;
}

s64 FileSystem::GetPathFileSize(const char* Path)
{
	FILESYSTEM_STAT_DATA sd;
	if (!StatFile(Path, &sd))
		return -1;

	return sd.Size;
}

std::optional<std::time_t> FileSystem::GetFileTimestamp(const char* path)
{
	FILESYSTEM_STAT_DATA sd;
	if (!StatFile(path, &sd))
		return std::nullopt;

	return sd.ModificationTime;
}

std::optional<std::vector<u8>> FileSystem::ReadBinaryFile(const char* filename)
{
	ManagedCFilePtr fp = OpenManagedCFile(filename, "rb");
	if (!fp)
		return std::nullopt;

	return ReadBinaryFile(fp.get());
}

std::optional<std::vector<u8>> FileSystem::ReadBinaryFile(std::FILE* fp)
{
	const s64 size = FSize64(fp);
	if (size < 0)
		return std::nullopt;

	std::fseek(fp, 0, SEEK_SET);
	std::vector<u8> res(static_cast<size_t>(size));
	if (size > 0 && std::fread(res.data(), 1u, static_cast<size_t>(size), fp) != static_cast<size_t>(size))
		return std::nullopt;

	return res;
}

std::optional<std::string> FileSystem::ReadFileToString(const char* filename)
{
	ManagedCFilePtr fp = OpenManagedCFile(filename, "rb");
	if (!fp)
		return std::nullopt;

	return ReadFileToString(fp.get());
}

std::optional<std::string> FileSystem::ReadFileToString(std::FILE* fp)
{
	const s64 size = FSize64(fp);
	if (size < 0)
		return std::nullopt;

	std::fseek(fp, 0, SEEK_SET);
	std::string res;
	res.resize(static_cast<size_t>(size));
	// NOTE - assumes mode 'rb', for example, this will fail over missing Windows carriage return bytes
	if (size > 0 && std::fread(res.data(), 1u, static_cast<size_t>(size), fp) != static_cast<size_t>(size))
		return std::nullopt;

	return res;
}

bool FileSystem::WriteBinaryFile(const char* filename, const void* data, size_t data_length)
{
	ManagedCFilePtr fp = OpenManagedCFile(filename, "wb");
	if (!fp)
		return false;

	if (data_length > 0 && std::fwrite(data, 1u, data_length, fp.get()) != data_length)
		return false;

	return true;
}

bool FileSystem::WriteStringToFile(const char* filename, const std::string_view& sv)
{
	ManagedCFilePtr fp = OpenManagedCFile(filename, "wb");
	if (!fp)
		return false;

	if (sv.length() > 0 && std::fwrite(sv.data(), 1u, sv.length(), fp.get()) != sv.length())
		return false;

	return true;
}

bool FileSystem::EnsureDirectoryExists(const char* path, bool recursive)
{
	if (FileSystem::DirectoryExists(path))
		return true;

	// if it fails to create, we're not going to be able to use it anyway
	return FileSystem::CreateDirectoryPath(path, recursive);
}

bool FileSystem::RecursiveDeleteDirectory(const char* path)
{
	FindResultsArray results;
	if (FindFiles(path, "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES, &results))
	{
		for (const FILESYSTEM_FIND_DATA& fd : results)
		{
			if (fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!RecursiveDeleteDirectory(fd.FileName.c_str()))
					return false;
			}
			else
			{
				if (!DeleteFilePath(fd.FileName.c_str()))
					return false;
			}
		}
	}

	return DeleteDirectory(path);
}

bool FileSystem::CopyFilePath(const char* source, const char* destination, bool replace)
{
#ifndef _WIN32
	// TODO: There's technically a race here between checking and opening the file..
	// But fopen doesn't specify any way to say "don't create if it exists"...
	if (!replace && FileExists(destination))
		return false;

	auto in_fp = OpenManagedCFile(source, "rb");
	if (!in_fp)
		return false;

	auto out_fp = OpenManagedCFile(destination, "wb");
	if (!out_fp)
		return false;

	u8 buf[4096];
	while (!std::feof(in_fp.get()))
	{
		size_t bytes_in = std::fread(buf, 1, sizeof(buf), in_fp.get());
		if ((bytes_in == 0 && !std::feof(in_fp.get())) ||
			(bytes_in > 0 && std::fwrite(buf, 1, bytes_in, out_fp.get()) != bytes_in))
		{
			out_fp.reset();
			DeleteFilePath(destination);
			return false;
		}
	}

	if (std::fflush(out_fp.get()) != 0)
	{
		out_fp.reset();
		DeleteFilePath(destination);
		return false;
	}

	return true;
#else
	return CopyFileW(StringUtil::UTF8StringToWideString(source).c_str(),
		StringUtil::UTF8StringToWideString(destination).c_str(), !replace);
#endif
}

#ifdef _WIN32

static u32 TranslateWin32Attributes(u32 Win32Attributes)
{
	u32 r = 0;

	if (Win32Attributes & FILE_ATTRIBUTE_DIRECTORY)
		r |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;
	if (Win32Attributes & FILE_ATTRIBUTE_READONLY)
		r |= FILESYSTEM_FILE_ATTRIBUTE_READ_ONLY;
	if (Win32Attributes & FILE_ATTRIBUTE_COMPRESSED)
		r |= FILESYSTEM_FILE_ATTRIBUTE_COMPRESSED;

	return r;
}

static u32 RecursiveFindFiles(const char* origin_path, const char* parent_path, const char* path, const char* pattern,
	u32 flags, FileSystem::FindResultsArray* results, std::vector<std::string>& visited)
{
	std::string search_dir;
	if (path)
	{
		if (parent_path)
			search_dir = fmt::format("{}\\{}\\{}\\*", origin_path, parent_path, path);
		else
			search_dir = fmt::format("{}\\{}\\*", origin_path, path);
	}
	else
	{
		search_dir = fmt::format("{}\\*", origin_path);
	}

	// holder for utf-8 conversion
	WIN32_FIND_DATAW wfd;
	std::string utf8_filename;
	utf8_filename.reserve((sizeof(wfd.cFileName) / sizeof(wfd.cFileName[0])) * 2);

	const HANDLE hFind = FindFirstFileW(StringUtil::UTF8StringToWideString(search_dir).c_str(), &wfd);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;

	// small speed optimization for '*' case
	bool hasWildCards = false;
	bool wildCardMatchAll = false;
	u32 nFiles = 0;
	if (std::strpbrk(pattern, "*?"))
	{
		hasWildCards = true;
		wildCardMatchAll = !(std::strcmp(pattern, "*"));
	}

	// iterate results
	do
	{
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN && !(flags & FILESYSTEM_FIND_HIDDEN_FILES))
			continue;

		if (wfd.cFileName[0] == L'.')
		{
			if (wfd.cFileName[1] == L'\0' || (wfd.cFileName[1] == L'.' && wfd.cFileName[2] == L'\0'))
				continue;
		}

		if (!StringUtil::WideStringToUTF8String(utf8_filename, wfd.cFileName))
			continue;

		FILESYSTEM_FIND_DATA outData;
		outData.Attributes = 0;

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (flags & FILESYSTEM_FIND_RECURSIVE)
			{
				// check that we're not following an infinite symbolic link loop
				std::string real_recurse_dir;
				if (parent_path)
					real_recurse_dir = Path::RealPath(fmt::format("{}\\{}\\{}\\{}", origin_path, parent_path, path, utf8_filename));
				else if (path)
					real_recurse_dir = Path::RealPath(fmt::format("{}\\{}\\{}", origin_path, path, utf8_filename));
				else
					real_recurse_dir = Path::RealPath(fmt::format("{}\\{}", origin_path, utf8_filename));
				if (real_recurse_dir.empty() || std::find(visited.begin(), visited.end(), real_recurse_dir) == visited.end())
				{
					if (!real_recurse_dir.empty())
						visited.push_back(std::move(real_recurse_dir));

					// recurse into this directory
					if (parent_path)
					{
						const std::string recurse_dir = fmt::format("{}\\{}", parent_path, path);
						nFiles += RecursiveFindFiles(origin_path, recurse_dir.c_str(), utf8_filename.c_str(), pattern, flags, results, visited);
					}
					else
					{
						nFiles += RecursiveFindFiles(origin_path, path, utf8_filename.c_str(), pattern, flags, results, visited);
					}
				}
			}

			if (!(flags & FILESYSTEM_FIND_FOLDERS))
				continue;

			outData.Attributes |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;
		}
		else
		{
			if (!(flags & FILESYSTEM_FIND_FILES))
				continue;
		}

		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
			outData.Attributes |= FILESYSTEM_FILE_ATTRIBUTE_READ_ONLY;

		// match the filename
		if (hasWildCards)
		{
			if (!wildCardMatchAll && !StringUtil::WildcardMatch(utf8_filename.c_str(), pattern))
				continue;
		}
		else
		{
			if (std::strcmp(utf8_filename.c_str(), pattern) != 0)
				continue;
		}

		// add file to list
		if (!(flags & FILESYSTEM_FIND_RELATIVE_PATHS))
		{
			if (parent_path)
				outData.FileName = fmt::format("{}\\{}\\{}\\{}", origin_path, parent_path, path, utf8_filename);
			else if (path)
				outData.FileName = fmt::format("{}\\{}\\{}", origin_path, path, utf8_filename);
			else
				outData.FileName = fmt::format("{}\\{}", origin_path, utf8_filename);
		}
		else
		{
			if (parent_path)
				outData.FileName = fmt::format("{}\\{}\\{}", parent_path, path, utf8_filename);
			else if (path)
				outData.FileName = fmt::format("{}\\{}", path, utf8_filename);
			else
				outData.FileName = utf8_filename;
		}

		outData.CreationTime = ConvertFileTimeToUnixTime(wfd.ftCreationTime);
		outData.ModificationTime = ConvertFileTimeToUnixTime(wfd.ftLastWriteTime);
		outData.Size = (static_cast<u64>(wfd.nFileSizeHigh) << 32) | static_cast<u64>(wfd.nFileSizeLow);

		nFiles++;
		results->push_back(std::move(outData));
	} while (FindNextFileW(hFind, &wfd) == TRUE);
	FindClose(hFind);

	return nFiles;
}

bool FileSystem::FindFiles(const char* path, const char* pattern, u32 flags, FindResultsArray* results)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// clear result array
	if (!(flags & FILESYSTEM_FIND_KEEP_ARRAY))
		results->clear();

	// add self if recursive, we don't want to visit it twice
	std::vector<std::string> visited;
	if (flags & FILESYSTEM_FIND_RECURSIVE)
	{
		std::string real_path = Path::RealPath(path);
		if (!real_path.empty())
			visited.push_back(std::move(real_path));
	}

	// enter the recursive function
	return (RecursiveFindFiles(path, nullptr, nullptr, pattern, flags, results, visited) > 0);
}

static void TranslateStat64(struct stat* st, const struct _stat64& st64)
{
	static constexpr __int64 MAX_SIZE = static_cast<__int64>(std::numeric_limits<decltype(st->st_size)>::max());
	st->st_dev = st64.st_dev;
	st->st_ino = st64.st_ino;
	st->st_mode = st64.st_mode;
	st->st_nlink = st64.st_nlink;
	st->st_uid = st64.st_uid;
	st->st_rdev = st64.st_rdev;
	st->st_size = static_cast<decltype(st->st_size)>((st64.st_size > MAX_SIZE) ? MAX_SIZE : st64.st_size);
	st->st_atime = static_cast<time_t>(st64.st_atime);
	st->st_mtime = static_cast<time_t>(st64.st_mtime);
	st->st_ctime = static_cast<time_t>(st64.st_ctime);
}

bool FileSystem::StatFile(const char* path, struct stat* st)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// convert to wide string
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	if (wpath.empty())
		return false;

	struct _stat64 st64;
	if (_wstat64(wpath.c_str(), &st64) != 0)
		return false;

	TranslateStat64(st, st64);
	return true;
}

bool FileSystem::StatFile(std::FILE* fp, struct stat* st)
{
	const int fd = _fileno(fp);
	if (fd < 0)
		return false;

	struct _stat64 st64;
	if (_fstat64(fd, &st64) != 0)
		return false;

	TranslateStat64(st, st64);
	return true;
}

bool FileSystem::StatFile(const char* path, FILESYSTEM_STAT_DATA* sd)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// convert to wide string
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	if (wpath.empty())
		return false;

	// determine attributes for the path. if it's a directory, things have to be handled differently..
	DWORD fileAttributes = GetFileAttributesW(wpath.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES)
		return false;

	// test if it is a directory
	HANDLE hFile;
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
	}
	else
	{
		hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING, 0, nullptr);
	}

	// createfile succeded?
	if (hFile == INVALID_HANDLE_VALUE)
		return false;

	// use GetFileInformationByHandle
	BY_HANDLE_FILE_INFORMATION bhfi;
	if (GetFileInformationByHandle(hFile, &bhfi) == FALSE)
	{
		CloseHandle(hFile);
		return false;
	}

	// close handle
	CloseHandle(hFile);

	// fill in the stat data
	sd->Attributes = TranslateWin32Attributes(bhfi.dwFileAttributes);
	sd->CreationTime = ConvertFileTimeToUnixTime(bhfi.ftCreationTime);
	sd->ModificationTime = ConvertFileTimeToUnixTime(bhfi.ftLastWriteTime);
	sd->Size = static_cast<s64>(((u64)bhfi.nFileSizeHigh) << 32 | (u64)bhfi.nFileSizeLow);
	return true;
}

bool FileSystem::StatFile(std::FILE* fp, FILESYSTEM_STAT_DATA* sd)
{
	const int fd = _fileno(fp);
	if (fd < 0)
		return false;

	struct _stat64 st;
	if (_fstat64(fd, &st) != 0)
		return false;

	// parse attributes
	sd->CreationTime = st.st_ctime;
	sd->ModificationTime = st.st_mtime;
	sd->Attributes = 0;
	if ((st.st_mode & _S_IFMT) == _S_IFDIR)
		sd->Attributes |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;

	// parse size
	if ((st.st_mode & _S_IFMT) == _S_IFREG)
		sd->Size = st.st_size;
	else
		sd->Size = 0;

	return true;
}

bool FileSystem::FileExists(const char* path)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// convert to wide string
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	if (wpath.empty())
		return false;

	// determine attributes for the path. if it's a directory, things have to be handled differently..
	DWORD fileAttributes = GetFileAttributesW(wpath.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES)
		return false;

	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;
	else
		return true;
}

bool FileSystem::DirectoryExists(const char* path)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// convert to wide string
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	if (wpath.empty())
		return false;

	// determine attributes for the path. if it's a directory, things have to be handled differently..
	DWORD fileAttributes = GetFileAttributesW(wpath.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES)
		return false;

	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return true;
	else
		return false;
}

bool FileSystem::DirectoryIsEmpty(const char* path)
{
	std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	wpath += L"\\*";

	WIN32_FIND_DATAW wfd;
	HANDLE hFind = FindFirstFileW(wpath.c_str(), &wfd);

	if (hFind == INVALID_HANDLE_VALUE)
		return true;

	do
	{
		if (wfd.cFileName[0] == L'.')
		{
			if (wfd.cFileName[1] == L'\0' || (wfd.cFileName[1] == L'.' && wfd.cFileName[2] == L'\0'))
				continue;
		}

		FindClose(hFind);
		return false;
	} while (FindNextFileW(hFind, &wfd));

	FindClose(hFind);
	return true;
}

bool FileSystem::CreateDirectoryPath(const char* Path, bool Recursive)
{
	const std::wstring wpath(StringUtil::UTF8StringToWideString(Path));

	// has a path
	if (wpath.empty())
		return false;

	// try just flat-out, might work if there's no other segments that have to be made
	if (CreateDirectoryW(wpath.c_str(), nullptr))
		return true;

	if (!Recursive)
		return false;

	// check error
	DWORD lastError = GetLastError();
	if (lastError == ERROR_ALREADY_EXISTS)
	{
		// check the attributes
		u32 Attributes = GetFileAttributesW(wpath.c_str());
		if (Attributes != INVALID_FILE_ATTRIBUTES && Attributes & FILE_ATTRIBUTE_DIRECTORY)
			return true;
		else
			return false;
	}
	else if (lastError == ERROR_PATH_NOT_FOUND)
	{
		// part of the path does not exist, so we'll create the parent folders, then
		// the full path again.
		const size_t pathLength = wpath.size();
		std::wstring tempPath;
		tempPath.reserve(pathLength);

		// create directories along the path
		for (size_t i = 0; i < pathLength; i++)
		{
			if (wpath[i] == L'\\' || wpath[i] == L'/')
			{
				const BOOL result = CreateDirectoryW(tempPath.c_str(), nullptr);
				if (!result)
				{
					lastError = GetLastError();
					if (lastError != ERROR_ALREADY_EXISTS) // fine, continue to next path segment
						return false;
				}

				// replace / with \.
				tempPath.push_back('\\');
			}
			else
			{
				tempPath.push_back(wpath[i]);
			}
		}

		// re-create the end if it's not a separator, check / as well because windows can interpret them
		if (wpath[pathLength - 1] != L'\\' && wpath[pathLength - 1] != L'/')
		{
			const BOOL result = CreateDirectoryW(wpath.c_str(), nullptr);
			if (!result)
			{
				lastError = GetLastError();
				if (lastError != ERROR_ALREADY_EXISTS)
					return false;
			}
		}

		// ok
		return true;
	}
	else
	{
		// unhandled error
		return false;
	}
}

bool FileSystem::DeleteFilePath(const char* path)
{
	if (path[0] == '\0')
		return false;

	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	const DWORD fileAttributes = GetFileAttributesW(wpath.c_str());
	if (fileAttributes == INVALID_FILE_ATTRIBUTES || fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		return false;

	return (DeleteFileW(wpath.c_str()) == TRUE);
}

bool FileSystem::RenamePath(const char* old_path, const char* new_path)
{
	const std::wstring old_wpath(StringUtil::UTF8StringToWideString(old_path));
	const std::wstring new_wpath(StringUtil::UTF8StringToWideString(new_path));

	if (!MoveFileExW(old_wpath.c_str(), new_wpath.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		Console.Error("MoveFileEx('%s', '%s') failed: %08X", old_path, new_path, GetLastError());
		return false;
	}

	return true;
}

bool FileSystem::DeleteDirectory(const char* path)
{
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	return RemoveDirectoryW(wpath.c_str());
}

std::string FileSystem::GetProgramPath()
{
	std::wstring buffer;
	buffer.resize(MAX_PATH);

	// Fall back to the main module if this fails.
	HMODULE module = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&GetProgramPath), &module);

	for (;;)
	{
		DWORD nChars = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (nChars == static_cast<DWORD>(buffer.size()) && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			buffer.resize(buffer.size() * 2);
			continue;
		}

		buffer.resize(nChars);
		break;
	}

	// Windows symlinks don't behave silly like Linux, so no need to RealPath() it.
	return StringUtil::WideStringToUTF8String(buffer);
}

std::string FileSystem::GetWorkingDirectory()
{
	DWORD required_size = GetCurrentDirectoryW(0, nullptr);
	if (!required_size)
		return {};

	std::wstring buffer;
	buffer.resize(required_size - 1);

	if (!GetCurrentDirectoryW(static_cast<DWORD>(buffer.size() + 1), buffer.data()))
		return {};

	return StringUtil::WideStringToUTF8String(buffer);
}

bool FileSystem::SetWorkingDirectory(const char* path)
{
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	return (SetCurrentDirectoryW(wpath.c_str()) == TRUE);
}

bool FileSystem::SetPathCompression(const char* path, bool enable)
{
	const std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	const DWORD attrs = GetFileAttributesW(wpath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES)
		return false;

	const bool isCompressed = (attrs & FILE_ATTRIBUTE_COMPRESSED) != 0;
	if (enable == isCompressed)
	{
		// already compressed/not compressed
		return true;
	}

	const bool isFile = !(attrs & FILE_ATTRIBUTE_DIRECTORY);
	const DWORD flags = isFile ? FILE_ATTRIBUTE_NORMAL : (FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_DIRECTORY);

	const HANDLE handle = CreateFileW(wpath.c_str(),
		FILE_GENERIC_WRITE | FILE_GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		flags,
		nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	DWORD bytesReturned = 0;
	DWORD compressMode = enable ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;

	bool result = DeviceIoControl(
		handle, FSCTL_SET_COMPRESSION,
		&compressMode, 2, nullptr, 0,
		&bytesReturned, nullptr);

	CloseHandle(handle);
	return result;
}

#else

// No 32-bit file offsets breaking stuff please.
static_assert(sizeof(off_t) == sizeof(s64));

static u32 RecursiveFindFiles(const char* OriginPath, const char* ParentPath, const char* Path, const char* Pattern,
	u32 Flags, FileSystem::FindResultsArray* pResults, std::vector<std::string>& visited)
{
	std::string tempStr;
	if (Path)
	{
		if (ParentPath)
			tempStr = fmt::format("{}/{}/{}", OriginPath, ParentPath, Path);
		else
			tempStr = fmt::format("{}/{}", OriginPath, Path);
	}
	else
	{
		tempStr = fmt::format("{}", OriginPath);
	}

	DIR* pDir = opendir(tempStr.c_str());
	if (!pDir)
		return 0;

	// small speed optimization for '*' case
	bool hasWildCards = false;
	bool wildCardMatchAll = false;
	u32 nFiles = 0;
	if (std::strpbrk(Pattern, "*?"))
	{
		hasWildCards = true;
		wildCardMatchAll = (std::strcmp(Pattern, "*") == 0);
	}

	// iterate results
	struct dirent* pDirEnt;
	while ((pDirEnt = readdir(pDir)) != nullptr)
	{
		if (pDirEnt->d_name[0] == '.')
		{
			if (pDirEnt->d_name[1] == '\0' || (pDirEnt->d_name[1] == '.' && pDirEnt->d_name[2] == '\0'))
				continue;

			if (!(Flags & FILESYSTEM_FIND_HIDDEN_FILES))
				continue;
		}

		std::string full_path;
		if (ParentPath)
			full_path = fmt::format("{}/{}/{}/{}", OriginPath, ParentPath, Path, pDirEnt->d_name);
		else if (Path)
			full_path = fmt::format("{}/{}/{}", OriginPath, Path, pDirEnt->d_name);
		else
			full_path = fmt::format("{}/{}", OriginPath, pDirEnt->d_name);

		FILESYSTEM_FIND_DATA outData;
		outData.Attributes = 0;

		struct stat sDir;
		if (stat(full_path.c_str(), &sDir) < 0)
			continue;

		if (S_ISDIR(sDir.st_mode))
		{
			if (Flags & FILESYSTEM_FIND_RECURSIVE)
			{
				// check that we're not following an infinite symbolic link loop
				if (std::string real_recurse_dir = Path::RealPath(full_path);
					real_recurse_dir.empty() || std::find(visited.begin(), visited.end(), real_recurse_dir) == visited.end())
				{
					if (!real_recurse_dir.empty())
						visited.push_back(std::move(real_recurse_dir));

					// recurse into this directory
					if (ParentPath)
					{
						const std::string recursive_dir = fmt::format("{}/{}", ParentPath, Path);
						nFiles += RecursiveFindFiles(OriginPath, recursive_dir.c_str(), pDirEnt->d_name, Pattern, Flags, pResults, visited);
					}
					else
					{
						nFiles += RecursiveFindFiles(OriginPath, Path, pDirEnt->d_name, Pattern, Flags, pResults, visited);
					}
				}
			}

			if (!(Flags & FILESYSTEM_FIND_FOLDERS))
				continue;

			outData.Attributes |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;
		}
		else
		{
			if (!(Flags & FILESYSTEM_FIND_FILES))
				continue;
		}

		outData.Size = static_cast<u64>(sDir.st_size);
		outData.CreationTime = sDir.st_ctime;
		outData.ModificationTime = sDir.st_mtime;

		// match the filename
		if (hasWildCards)
		{
			if (!wildCardMatchAll && !StringUtil::WildcardMatch(pDirEnt->d_name, Pattern))
				continue;
		}
		else
		{
			if (std::strcmp(pDirEnt->d_name, Pattern) != 0)
				continue;
		}

		// add file to list
		if (!(Flags & FILESYSTEM_FIND_RELATIVE_PATHS))
		{
			outData.FileName = std::move(full_path);
		}
		else
		{
			if (ParentPath)
				outData.FileName = fmt::format("{}/{}/{}", ParentPath, Path, pDirEnt->d_name);
			else if (Path)
				outData.FileName = fmt::format("{}/{}", Path, pDirEnt->d_name);
			else
				outData.FileName = pDirEnt->d_name;
		}

		nFiles++;
		pResults->push_back(std::move(outData));
	}

	closedir(pDir);
	return nFiles;
}

bool FileSystem::FindFiles(const char* path, const char* pattern, u32 flags, FindResultsArray* results)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// clear result array
	if (!(flags & FILESYSTEM_FIND_KEEP_ARRAY))
		results->clear();

	// add self if recursive, we don't want to visit it twice
	std::vector<std::string> visited;
	if (flags & FILESYSTEM_FIND_RECURSIVE)
	{
		std::string real_path = Path::RealPath(path);
		if (!real_path.empty())
			visited.push_back(std::move(real_path));
	}

	// enter the recursive function
	return (RecursiveFindFiles(path, nullptr, nullptr, pattern, flags, results, visited) > 0);
}

bool FileSystem::StatFile(const char* path, struct stat* st)
{
	return stat(path, st) == 0;
}

bool FileSystem::StatFile(std::FILE* fp, struct stat* st)
{
	const int fd = fileno(fp);
	if (fd < 0)
		return false;

	return fstat(fd, st) == 0;
}

bool FileSystem::StatFile(const char* path, FILESYSTEM_STAT_DATA* sd)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// stat file
	struct stat sysStatData;
	if (stat(path, &sysStatData) < 0)
		return false;

	// parse attributes
	sd->CreationTime = sysStatData.st_ctime;
	sd->ModificationTime = sysStatData.st_mtime;
	sd->Attributes = 0;
	if (S_ISDIR(sysStatData.st_mode))
		sd->Attributes |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;

	// parse size
	if (S_ISREG(sysStatData.st_mode))
		sd->Size = sysStatData.st_size;
	else
		sd->Size = 0;

	// ok
	return true;
}

bool FileSystem::StatFile(std::FILE* fp, FILESYSTEM_STAT_DATA* sd)
{
	const int fd = fileno(fp);
	if (fd < 0)
		return false;

	// stat file
	struct stat sysStatData;
	if (fstat(fd, &sysStatData) < 0)
		return false;

	// parse attributes
	sd->CreationTime = sysStatData.st_ctime;
	sd->ModificationTime = sysStatData.st_mtime;
	sd->Attributes = 0;
	if (S_ISDIR(sysStatData.st_mode))
		sd->Attributes |= FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY;

	// parse size
	if (S_ISREG(sysStatData.st_mode))
		sd->Size = sysStatData.st_size;
	else
		sd->Size = 0;

	// ok
	return true;
}

bool FileSystem::FileExists(const char* path)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// stat file
	struct stat sysStatData;
	if (stat(path, &sysStatData) < 0)
		return false;

	if (S_ISDIR(sysStatData.st_mode))
		return false;
	else
		return true;
}

bool FileSystem::DirectoryExists(const char* path)
{
	// has a path
	if (path[0] == '\0')
		return false;

	// stat file
	struct stat sysStatData;
	if (stat(path, &sysStatData) < 0)
		return false;

	if (S_ISDIR(sysStatData.st_mode))
		return true;
	else
		return false;
}

bool FileSystem::DirectoryIsEmpty(const char* path)
{
	DIR* pDir = opendir(path);
	if (pDir == nullptr)
		return true;

	// iterate results
	struct dirent* pDirEnt;
	while ((pDirEnt = readdir(pDir)) != nullptr)
	{
		if (pDirEnt->d_name[0] == '.')
		{
			if (pDirEnt->d_name[1] == '\0' || (pDirEnt->d_name[1] == '.' && pDirEnt->d_name[2] == '\0'))
				continue;
		}

		closedir(pDir);
		return false;
	}

	closedir(pDir);
	return true;
}

bool FileSystem::CreateDirectoryPath(const char* path, bool recursive)
{
	// has a path
	const size_t pathLength = std::strlen(path);
	if (pathLength == 0)
		return false;

	// try just flat-out, might work if there's no other segments that have to be made
	if (mkdir(path, 0777) == 0)
		return true;

	if (!recursive)
		return false;

	// check error
	int lastError = errno;
	if (lastError == EEXIST)
	{
		// check the attributes
		struct stat sysStatData;
		if (stat(path, &sysStatData) == 0 && S_ISDIR(sysStatData.st_mode))
			return true;
		else
			return false;
	}
	else if (lastError == ENOENT)
	{
		// part of the path does not exist, so we'll create the parent folders, then
		// the full path again.
		std::string tempPath;
		tempPath.reserve(pathLength);

		// create directories along the path
		for (size_t i = 0; i < pathLength; i++)
		{
			if (i > 0 && path[i] == '/')
			{
				if (mkdir(tempPath.c_str(), 0777) < 0)
				{
					lastError = errno;
					if (lastError != EEXIST) // fine, continue to next path segment
						return false;
				}
			}

			tempPath.push_back(path[i]);
		}

		// re-create the end if it's not a separator, check / as well because windows can interpret them
		if (path[pathLength - 1] != '/')
		{
			if (mkdir(path, 0777) < 0)
			{
				lastError = errno;
				if (lastError != EEXIST)
					return false;
			}
		}

		// ok
		return true;
	}
	else
	{
		// unhandled error
		return false;
	}
}

bool FileSystem::DeleteFilePath(const char* path)
{
	if (path[0] == '\0')
		return false;

	struct stat sysStatData;
	if (stat(path, &sysStatData) != 0 || S_ISDIR(sysStatData.st_mode))
		return false;

	return (unlink(path) == 0);
}

bool FileSystem::RenamePath(const char* old_path, const char* new_path)
{
	if (old_path[0] == '\0' || new_path[0] == '\0')
		return false;

	if (rename(old_path, new_path) != 0)
	{
		Console.Error("rename('%s', '%s') failed: %d", old_path, new_path, errno);
		return false;
	}

	return true;
}

bool FileSystem::DeleteDirectory(const char* path)
{
	if (path[0] == '\0')
		return false;

	struct stat sysStatData;
	if (stat(path, &sysStatData) != 0 || !S_ISDIR(sysStatData.st_mode))
		return false;

	return (rmdir(path) == 0);
}

std::string FileSystem::GetProgramPath()
{
#if defined(__linux__)
	static const char* exeFileName = "/proc/self/exe";

	int curSize = PATH_MAX;
	char* buffer = static_cast<char*>(std::realloc(nullptr, curSize));
	for (;;)
	{
		int len = readlink(exeFileName, buffer, curSize);
		if (len < 0)
		{
			std::free(buffer);
			return {};
		}
		else if (len < curSize)
		{
			buffer[len] = '\0';
			std::string ret(buffer, len);
			std::free(buffer);
			return ret;
		}

		curSize *= 2;
		buffer = static_cast<char*>(std::realloc(buffer, curSize));
	}

#elif defined(__APPLE__)

	int curSize = PATH_MAX;
	char* buffer = static_cast<char*>(std::realloc(nullptr, curSize));
	for (;;)
	{
		u32 nChars = curSize - 1;
		int res = _NSGetExecutablePath(buffer, &nChars);
		if (res == 0)
		{
			buffer[nChars] = 0;

			char* resolvedBuffer = realpath(buffer, nullptr);
			if (resolvedBuffer == nullptr)
			{
				std::free(buffer);
				return {};
			}

			std::string ret(buffer);
			std::free(buffer);
			return ret;
		}

		curSize *= 2;
		buffer = static_cast<char*>(std::realloc(buffer, curSize + 1));
	}

#elif defined(__FreeBSD__)
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
	char buffer[PATH_MAX];
	size_t cb = sizeof(buffer) - 1;
	int res = sysctl(mib, std::size(mib), buffer, &cb, nullptr, 0);
	if (res != 0)
		return {};

	buffer[cb] = '\0';
	return buffer;
#else
	return {};
#endif
}

std::string FileSystem::GetWorkingDirectory()
{
	std::string buffer;
	buffer.resize(PATH_MAX);
	while (!getcwd(buffer.data(), buffer.size()))
	{
		if (errno != ERANGE)
			return {};

		buffer.resize(buffer.size() * 2);
	}

	buffer.resize(std::strlen(buffer.c_str())); // Remove excess nulls
	return buffer;
}

bool FileSystem::SetWorkingDirectory(const char* path)
{
	return (chdir(path) == 0);
}

bool FileSystem::SetPathCompression(const char* path, bool enable)
{
	return false;
}

FileSystem::POSIXLock::POSIXLock(int fd)
{
	if (lockf(fd, F_LOCK, 0) == 0)
	{
		m_fd = fd;
	}
	else
	{
		Console.Error("lockf() failed: %d", errno);
		m_fd = -1;
	}
}

FileSystem::POSIXLock::POSIXLock(std::FILE* fp)
{
	m_fd = fileno(fp);
	if (m_fd >= 0)
	{
		if (lockf(m_fd, F_LOCK, 0) != 0)
		{
			Console.Error("lockf() failed: %d", errno);
			m_fd = -1;
		}
	}
}

FileSystem::POSIXLock::~POSIXLock()
{
	if (m_fd >= 0)
		lockf(m_fd, F_ULOCK, m_fd);
}

#endif
