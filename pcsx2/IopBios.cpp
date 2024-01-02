// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "DebugTools/SymbolMap.h"
#include "IopBios.h"
#include "IopMem.h"
#include "R3000A.h"
#include "R5900.h"
#include "ps2/BiosTools.h"
#include "x86/iR3000A.h"
#include "VMManager.h"

#include <ctype.h>
#include <fmt/format.h>
#include <string.h>
#include <sys/stat.h>
#include "common/FileSystem.h"
#include "common/Path.h"

#include <cctype>
#include <cstring>
#include <fmt/format.h>
#include <sys/stat.h>

#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct
{
	unsigned int mode;
	unsigned int attr;
	unsigned int size;
	unsigned char ctime[8];
	unsigned char atime[8];
	unsigned char mtime[8];
	unsigned int hisize;
} fio_stat_t;
typedef struct
{
	fio_stat_t _fioStat;
	/** Number of subs (main) / subpart number (sub) */
	unsigned int private_0;
	unsigned int private_1;
	unsigned int private_2;
	unsigned int private_3;
	unsigned int private_4;
	/** Sector start.  */
	unsigned int private_5;
} fxio_stat_t;

typedef struct
{
	fio_stat_t stat;
	char name[256];
	unsigned int unknown;
} fio_dirent_t;

typedef struct
{
	fxio_stat_t stat;
	char name[256];
	unsigned int unknown;
} fxio_dirent_t;

static std::string hostRoot;

void Hle_SetHostRoot(const char* bootFilename)
{
	hostRoot = Path::ToNativePath(Path::GetDirectory(bootFilename));
	Console.WriteLn("HLE Host: Set 'host:' root path to: %s\n", hostRoot.c_str());
}

void Hle_ClearHostRoot()
{
	hostRoot = {};
}

namespace R3000A
{

#define v0 (psxRegs.GPR.n.v0)
#define a0 (psxRegs.GPR.n.a0)
#define a1 (psxRegs.GPR.n.a1)
#define a2 (psxRegs.GPR.n.a2)
#define a3 (psxRegs.GPR.n.a3)
#define sp (psxRegs.GPR.n.sp)
#define ra (psxRegs.GPR.n.ra)
#define pc (psxRegs.pc)

#define Ra0 (iopMemReadString(a0))
#define Ra1 (iopMemReadString(a1))
#define Ra2 (iopMemReadString(a2))
#define Ra3 (iopMemReadString(a3))

	// Stat values differ between iomanX and ioman
	// These values have been taken from the PS2SDK
	// Specifically iox_stat.h
	struct fio_stat_flags
	{
		// Access flags
		// Execute
		int IXOTH;
		// Write
		int IWOTH;
		// Read
		int IROTH;

		// File mode flags
		// Symlink
		int IFLNK;
		// Regular file
		int IFREG;
		// Directory
		int IFDIR;
	};

	fio_stat_flags ioman_stat{
		0x01,
		0x02,
		0x04,
		0x08,
		0x10,
		0x20,
	};

	fio_stat_flags iomanx_stat{
		0x01,
		0x02,
		0x04,
		0x4000,
		0x2000,
		0x1000,
	};

	static std::string host_path(const std::string& path, bool allow_open_host_root)
	{
		// We are NOT allowing to use the root of the host unit.
		// For now it just supports relative folders from the location of the elf
		std::string native_path(Path::Canonicalize(path));
		std::string new_path;
		if (!hostRoot.empty() && native_path.starts_with(hostRoot))
			new_path = std::move(native_path);
		else if (!hostRoot.empty()) // relative paths
			new_path = Path::Combine(hostRoot, native_path);

		// Allow opening the ELF override.
		if (new_path == VMManager::Internal::GetELFOverride())
			return new_path;

		// Allow nothing if hostfs isn't enabled.
		if (!EmuConfig.HostFs)
		{
			new_path.clear();
			return new_path;
		}

		// Double-check that it falls within the directory of the elf.
		// Not a real sandbox, but emulators shouldn't be treated as such. Don't run untrusted code!
		std::string canonicalized_path(Path::Canonicalize(new_path));

		// Are we opening the root of host? (i.e. `host:.` or `host:`)
		// We want to allow this as a directory open, but not as a file open.
		if (!allow_open_host_root || canonicalized_path != hostRoot)
		{
			// Only allow descendants of the hostfs directory.
			if (canonicalized_path.length() <= hostRoot.length() || // Length has to be equal or longer,
				!canonicalized_path.starts_with(hostRoot) || // and start with the host root,
				canonicalized_path[hostRoot.length()] != FS_OSPATH_SEPARATOR_CHARACTER) // and we can't access a sibling.
			{
				Console.Error(fmt::format(
					"IopHLE: Denying access to path outside of ELF directory. Requested path: '{}', Resolved path: '{}', ELF directory: '{}'",
					path, new_path, hostRoot));
				new_path.clear();
			}
		}

		return new_path;
	}

	// This is a workaround for GHS on *NIX platforms
	// Whenever a program splits directories with a backslash (ulaunchelf)
	// the directory is considered non-existant
	static __fi std::string clean_path(const std::string path)
	{
		std::string ret = path;
		std::replace(ret.begin(), ret.end(), '\\', '/');
		return ret;
	}

	static int host_stat(const std::string path, fio_stat_t* host_stats, fio_stat_flags& stat = ioman_stat)
	{
		struct stat file_stats;
		const std::string file_path(host_path(path, true));

		if (!FileSystem::StatFile(file_path.c_str(), &file_stats))
			return -IOP_ENOENT;

		host_stats->size = file_stats.st_size;
		host_stats->hisize = 0;

		// Convert the mode.
		host_stats->mode = (file_stats.st_mode & (stat.IROTH | stat.IWOTH | stat.IXOTH));
#ifndef _WIN32
		if (S_ISLNK(file_stats.st_mode))
		{
			host_stats->mode |= stat.IFLNK;
		}
#endif
		if (S_ISREG(file_stats.st_mode))
		{
			host_stats->mode |= stat.IFREG;
		}
		if (S_ISDIR(file_stats.st_mode))
		{
			host_stats->mode |= stat.IFDIR;
		}

		// Convert the creation time.
		struct tm* loctime;
		loctime = localtime(&(file_stats.st_ctime));
		host_stats->ctime[6] = (unsigned char)loctime->tm_year;
		host_stats->ctime[5] = (unsigned char)loctime->tm_mon + 1;
		host_stats->ctime[4] = (unsigned char)loctime->tm_mday;
		host_stats->ctime[3] = (unsigned char)loctime->tm_hour;
		host_stats->ctime[2] = (unsigned char)loctime->tm_min;
		host_stats->ctime[1] = (unsigned char)loctime->tm_sec;

		// Convert the access time.
		loctime = localtime(&(file_stats.st_atime));
		host_stats->atime[6] = (unsigned char)loctime->tm_year;
		host_stats->atime[5] = (unsigned char)loctime->tm_mon + 1;
		host_stats->atime[4] = (unsigned char)loctime->tm_mday;
		host_stats->atime[3] = (unsigned char)loctime->tm_hour;
		host_stats->atime[2] = (unsigned char)loctime->tm_min;
		host_stats->atime[1] = (unsigned char)loctime->tm_sec;

		// Convert the last modified time.
		loctime = localtime(&(file_stats.st_mtime));
		host_stats->mtime[6] = (unsigned char)loctime->tm_year;
		host_stats->mtime[5] = (unsigned char)loctime->tm_mon + 1;
		host_stats->mtime[4] = (unsigned char)loctime->tm_mday;
		host_stats->mtime[3] = (unsigned char)loctime->tm_hour;
		host_stats->mtime[2] = (unsigned char)loctime->tm_min;
		host_stats->mtime[1] = (unsigned char)loctime->tm_sec;

		return 0;
	}

	static int host_stat(const std::string path, fxio_stat_t* host_stats)
	{
		return host_stat(path, &host_stats->_fioStat, iomanx_stat);
	}

	// TODO: sandbox option, other permissions
	class HostFile : public IOManFile
	{
	public:
		int fd;

		HostFile(int hostfd)
		{
			fd = hostfd;
		}

		virtual ~HostFile() = default;

		static __fi int translate_error(int err)
		{
			if (err >= 0)
				return err;

			switch (err)
			{
				case -ENOENT:
					return -IOP_ENOENT;
				case -EACCES:
					return -IOP_EACCES;
				case -EISDIR:
					return -IOP_EISDIR;
				case -EIO:
				default:
					return -IOP_EIO;
			}
		}

		static int open(IOManFile** file, const std::string& full_path, s32 flags, u16 mode)
		{
			const std::string path(full_path.substr(full_path.find(':') + 1));
			const std::string file_path(host_path(path, false));
			int native_flags = O_BINARY; // necessary in Windows.

			switch (flags & IOP_O_RDWR)
			{
				case IOP_O_RDONLY:
					native_flags |= O_RDONLY;
					break;
				case IOP_O_WRONLY:
					native_flags |= O_WRONLY;
					break;
				case IOP_O_RDWR:
					native_flags |= O_RDWR;
					break;
			}

			if (flags & IOP_O_APPEND)
				native_flags |= O_APPEND;
			if (flags & IOP_O_CREAT)
				native_flags |= O_CREAT;
			if (flags & IOP_O_TRUNC)
				native_flags |= O_TRUNC;

#ifdef _WIN32
			const int native_mode = _S_IREAD | _S_IWRITE;
#else
			const int native_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
#endif

			const int hostfd = FileSystem::OpenFDFile(file_path.c_str(), native_flags, native_mode);
			if (hostfd < 0)
				return translate_error(hostfd);

			*file = new HostFile(hostfd);
			if (!*file)
				return -IOP_ENOMEM;

			return 0;
		}

		virtual void close()
		{
			::close(fd);
			delete this;
		}

		virtual int lseek(s32 offset, s32 whence)
		{
			int err;

			switch (whence)
			{
				case IOP_SEEK_SET:
					err = ::lseek(fd, offset, SEEK_SET);
					break;
				case IOP_SEEK_CUR:
					err = ::lseek(fd, offset, SEEK_CUR);
					break;
				case IOP_SEEK_END:
					err = ::lseek(fd, offset, SEEK_END);
					break;
				default:
					return -IOP_EIO;
			}

			return translate_error(err);
		}

		virtual int read(void* buf, u32 count) /* Flawfinder: ignore */
		{
			return translate_error(::read(fd, buf, count));
		}

		virtual int write(void* buf, u32 count)
		{
			return translate_error(::write(fd, buf, count));
		}
	};

	class HostDir : public IOManDir
	{
	public:
		FileSystem::FindResultsArray results;
		FileSystem::FindResultsArray::iterator dir;
		std::string basedir;

		HostDir(FileSystem::FindResultsArray results_, std::string basedir_)
			: results(std::move(results_))
			, basedir(std::move(basedir_))
		{
			dir = results.begin();
		}

		virtual ~HostDir() = default;

		static int open(IOManDir** dir, const std::string& full_path)
		{
			std::string relativePath = full_path.substr(full_path.find(':') + 1);
			std::string path = host_path(relativePath, true);

			if (!FileSystem::DirectoryExists(path.c_str()))
				return -IOP_ENOENT; // Should return ENOTDIR if path is a file?

			FileSystem::FindResultsArray results;
			FileSystem::FindFiles(path.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_RELATIVE_PATHS | FILESYSTEM_FIND_HIDDEN_FILES, &results);

			*dir = new HostDir(std::move(results), std::move(path));
			if (!*dir)
				return -IOP_ENOMEM;

			return 0;
		}

		virtual int read(void* buf, bool iomanX) /* Flawfinder: ignore */
		{
			if (dir == results.end())
				return 0;

			if (iomanX)
			{
				fxio_dirent_t* hostcontent = (fxio_dirent_t*)buf;
				StringUtil::Strlcpy(hostcontent->name, dir->FileName, sizeof(hostcontent->name));
				host_stat(host_path(Path::Combine(basedir, dir->FileName), true), &hostcontent->stat);
			}
			else
			{
				fio_dirent_t* hostcontent = (fio_dirent_t*)buf;
				StringUtil::Strlcpy(hostcontent->name, dir->FileName, sizeof(hostcontent->name));
				host_stat(host_path(Path::Combine(basedir, dir->FileName), true), &hostcontent->stat);
			}

			dir = std::next(dir);
			return 1;
		}

		virtual void close()
		{
			delete this;
		}
	};

	namespace ioman
	{
		const int firstfd = 0x100;
		const int maxfds = 0x100;
		int openfds = 0;

		int freefdcount()
		{
			return maxfds - openfds;
		}

		struct filedesc
		{
			enum
			{
				FILE_FREE,
				FILE_FILE,
				FILE_DIR,
			} type;
			union
			{
				IOManFile* file;
				IOManDir* dir;
			};

			constexpr filedesc()
				: type(FILE_FREE)
				, file(nullptr)
			{
			}
			operator bool() const { return type != FILE_FREE; }
			operator IOManFile*() const { return type == FILE_FILE ? file : NULL; }
			operator IOManDir*() const { return type == FILE_DIR ? dir : NULL; }
			void operator=(IOManFile* f)
			{
				type = FILE_FILE;
				file = f;
				openfds++;
			}
			void operator=(IOManDir* d)
			{
				type = FILE_DIR;
				dir = d;
				openfds++;
			}

			void close()
			{
				switch (type)
				{
					case FILE_FILE:
						file->close();
						file = NULL;
						break;
					case FILE_DIR:
						dir->close();
						dir = NULL;
						break;
					case FILE_FREE:
						return;
				}

				type = FILE_FREE;
				openfds--;
			}
		};

		filedesc fds[maxfds];

		template <typename T>
		T* getfd(int fd)
		{
			fd -= firstfd;

			if (fd < 0 || fd >= maxfds)
				return NULL;

			return fds[fd];
		}

		template <typename T>
		int allocfd(T* obj)
		{
			for (int i = 0; i < maxfds; i++)
			{
				if (!fds[i])
				{
					fds[i] = obj;
					return firstfd + i;
				}
			}

			obj->close();
			return -IOP_EMFILE;
		}

		void freefd(int fd)
		{
			fd -= firstfd;

			if (fd < 0 || fd >= maxfds)
				return;

			fds[fd].close();
		}

		void reset()
		{
			for (int i = 0; i < maxfds; i++)
			{
				if (fds[i])
					fds[i].close();
			}
		}

		bool is_host(const std::string path)
		{
			auto not_number_pos = path.find_first_not_of("0123456789", 4);
			if (not_number_pos == std::string::npos)
				return false;

			return (path.compare(0, 4, "host") == 0 && path[not_number_pos] == ':');
		}

		int open_HLE()
		{
			IOManFile* file = NULL;
			const std::string path = clean_path(Ra0);
			s32 flags = a1;
			u16 mode = a2;

			if (is_host(path))
			{
				if (!freefdcount())
				{
					v0 = -IOP_EMFILE;
					pc = ra;
					return 1;
				}

				int err = HostFile::open(&file, path, flags, mode);

				if (err != 0 || !file)
				{
					if (err == 0) // ???
						err = -IOP_EIO;
					if (file) // ??????
						file->close();
					v0 = err;
				}
				else
				{
					v0 = allocfd(file);
					if ((s32)v0 < 0)
						file->close();
				}

				pc = ra;
				return 1;
			}

			return 0;
		}

		int close_HLE()
		{
			s32 fd = a0;

			if (getfd<IOManFile>(fd))
			{
				freefd(fd);
				v0 = 0;
				pc = ra;
				return 1;
			}

			return 0;
		}

		int dopen_HLE()
		{
			IOManDir* dir = NULL;
			const std::string path = clean_path(Ra0);

			if (is_host(path))
			{
				int err = HostDir::open(&dir, path);

				if (err != 0 || !dir)
				{
					if (err == 0)
						err = -IOP_EIO;
					if (dir)
						dir->close();
					v0 = err;
				}
				else
				{
					v0 = allocfd(dir);
					if ((s32)v0 < 0)
						dir->close();
				}

				pc = ra;
				return 1;
			}

			return 0;
		}

		int dclose_HLE()
		{
			s32 dir = a0;

			if (getfd<IOManDir>(dir))
			{
				freefd(dir);
				v0 = 0;
				pc = ra;
				return 1;
			}

			return 0;
		}

		int _dread_HLE(bool iomanX)
		{
			s32 fh = a0;
			u32 data = a1;
			if (iomanX)
			{
				if (IOManDir* dir = getfd<IOManDir>(fh))
				{
					char buf[sizeof(fxio_dirent_t)];
					v0 = dir->read(&buf, iomanX); /* Flawfinder: ignore */

					for (s32 i = 0; i < (s32)sizeof(fxio_dirent_t); i++)
						iopMemWrite8(data + i, buf[i]);

					pc = ra;
					return 1;
				}
			}
			else
			{
				if (IOManDir* dir = getfd<IOManDir>(fh))
				{
					char buf[sizeof(fio_dirent_t)];
					v0 = dir->read(&buf); /* Flawfinder: ignore */

					for (s32 i = 0; i < (s32)sizeof(fio_dirent_t); i++)
						iopMemWrite8(data + i, buf[i]);

					pc = ra;
					return 1;
				}
			}
			return 0;
		}

		int dread_HLE()
		{
			return _dread_HLE(false);
		}

		int dreadx_HLE()
		{
			return _dread_HLE(true);
		}

		int _getStat_HLE(bool iomanx)
		{
			const std::string path = clean_path(Ra0);
			u32 data = a1;

			if (is_host(path))
			{
				const std::string full_path = host_path(path.substr(path.find(':') + 1), true);
				if (iomanx)
				{
					char buf[sizeof(fxio_stat_t)];
					v0 = host_stat(full_path, (fxio_stat_t*)&buf);

					for (size_t i = 0; i < sizeof(fxio_stat_t); i++)
						iopMemWrite8(data + i, buf[i]);
				}
				else
				{
					char buf[sizeof(fio_stat_t)];
					v0 = host_stat(full_path, (fio_stat_t*)&buf);

					for (size_t i = 0; i < sizeof(fio_stat_t); i++)
						iopMemWrite8(data + i, buf[i]);
				}
				pc = ra;
				return 1;
			}

			return 0;
		}

		int getStat_HLE()
		{
			return _getStat_HLE(false);
		}

		int getStatx_HLE()
		{
			return _getStat_HLE(true);
		}

		int lseek_HLE()
		{
			s32 fd = a0;
			s32 offset = a1;
			s32 whence = a2;

			if (IOManFile* file = getfd<IOManFile>(fd))
			{
				v0 = file->lseek(offset, whence);
				pc = ra;
				return 1;
			}

			return 0;
		}

		int remove_HLE()
		{
			const std::string full_path = clean_path(Ra0);

			if (is_host(full_path))
			{
				const std::string path = full_path.substr(full_path.find(':') + 1);
				const std::string file_path(host_path(path, false));
				const bool succeeded = FileSystem::DeleteFilePath(file_path.c_str());
				if (!succeeded)
					Console.Warning("IOPHLE remove_HLE failed for '%s'", file_path.c_str());
				v0 = succeeded ? 0 : -IOP_EIO;
				pc = ra;
			}
			return 0;
		}

		int mkdir_HLE()
		{
			const std::string full_path = clean_path(Ra0);

			if (is_host(full_path))
			{
				const std::string path = full_path.substr(full_path.find(':') + 1);
				const std::string folder_path(host_path(path, false)); // NOTE: Don't allow creating the ELF directory.
				const bool succeeded = FileSystem::CreateDirectoryPath(folder_path.c_str(), false);
				if (!succeeded)
					Console.Warning("IOPHLE mkdir_HLE failed for '%s'", folder_path.c_str());
				v0 = succeeded ? 0 : -IOP_EIO;
				pc = ra;
				return 1;
			}

			return 0;
		}

		int read_HLE()
		{
			s32 fd = a0;
			u32 data = a1;
			u32 count = a2;

			if (IOManFile* file = getfd<IOManFile>(fd))
			{
				auto buf = std::make_unique<char[]>(count);

				v0 = file->read(buf.get(), count);

				for (s32 i = 0; i < (s32)v0; i++)
					iopMemWrite8(data + i, buf[i]);

				pc = ra;
				return 1;
			}

			return 0;
		}

		int rmdir_HLE()
		{
			const std::string full_path = clean_path(Ra0);

			if (is_host(full_path))
			{
				const std::string path = full_path.substr(full_path.find(':') + 1);
				const std::string folder_path(host_path(path, false)); // NOTE: Don't allow removing the elf directory itself.
				const bool succeeded = FileSystem::DeleteDirectory(folder_path.c_str());
				if (!succeeded)
					Console.Warning("IOPHLE rmdir_HLE failed for '%s'", folder_path.c_str());
				v0 = succeeded ? 0 : -IOP_EIO;
				pc = ra;
				return 1;
			}

			return 0;
		}

		int write_HLE()
		{
			s32 fd = a0;
			u32 data = a1;
			u32 count = a2;

			if (fd == 1) // stdout
			{
				const std::string s = Ra1;
				iopConLog(ShiftJIS_ConvertString(s.data(), a2));
				pc = ra;
				v0 = a2;
				return 1;
			}
			else if (IOManFile* file = getfd<IOManFile>(fd))
			{
				auto buf = std::make_unique<char[]>(count);

				for (u32 i = 0; i < count; i++)
					buf[i] = iopMemRead8(data + i);

				v0 = file->write(buf.get(), count);

				pc = ra;
				return 1;
			}

			return 0;
		}
	} // namespace ioman

	namespace sysmem
	{
		int Kprintf_HLE()
		{
			// Using sprintf here is a bit nasty, but it has a large buffer..
			// Don't feel like rewriting it.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

			// Emulate the expected Kprintf functionality:
			iopMemWrite32(sp, a0);
			iopMemWrite32(sp + 4, a1);
			iopMemWrite32(sp + 8, a2);
			iopMemWrite32(sp + 12, a3);
			pc = ra;

			const std::string fmt = Ra0;

			// From here we're intercepting the Kprintf and piping it to our console, complete with
			// printf-style formatting processing.  This part can be skipped if the user has the
			// console disabled.

			if (!SysConsole.iopConsole.IsActive())
				return 1;

			char tmp[1024], tmp2[1024];
			char* ptmp = tmp;
			int n = 1, i = 0, j = 0;

			while (fmt[i])
			{
				switch (fmt[i])
				{
					case '%':
						j = 0;
						tmp2[j++] = '%';
					_start:
						switch (fmt[++i])
						{
							case '.':
							case 'l':
								tmp2[j++] = fmt[i];
								goto _start;
							default:
								if (fmt[i] >= '0' && fmt[i] <= '9')
								{
									tmp2[j++] = fmt[i];
									goto _start;
								}
								break;
						}

						tmp2[j++] = fmt[i];
						tmp2[j] = 0;

						switch (fmt[i])
						{
							case 'f':
							case 'F':
								ptmp += sprintf(ptmp, tmp2, (float)iopMemRead32(sp + n * 4));
								n++;
								break;

							case 'a':
							case 'A':
							case 'e':
							case 'E':
							case 'g':
							case 'G':
								ptmp += sprintf(ptmp, tmp2, (double)iopMemRead32(sp + n * 4));
								n++;
								break;

							case 'p':
							case 'i':
							case 'd':
							case 'D':
							case 'o':
							case 'O':
							case 'x':
							case 'X':
								ptmp += sprintf(ptmp, tmp2, (u32)iopMemRead32(sp + n * 4));
								n++;
								break;

							case 'c':
								ptmp += sprintf(ptmp, tmp2, (u8)iopMemRead32(sp + n * 4));
								n++;
								break;

							case 's':
							{
								std::string s = iopMemReadString(iopMemRead32(sp + n * 4));
								ptmp += sprintf(ptmp, tmp2, s.data());
								n++;
							}
							break;

							case '%':
								*ptmp++ = fmt[i];
								break;

							default:
								break;
						}
						i++;
						break;

					default:
						*ptmp++ = fmt[i++];
						break;
				}
			}
			*ptmp = 0;
			iopConLog(ShiftJIS_ConvertString(tmp, 1023));

			return 1;

#ifdef __clang__
#pragma clang diagnostic pop
#endif
		}
	} // namespace sysmem

	namespace loadcore
	{

		u32 GetModList(u32 a0reg)
		{
			u32 lcptr = iopMemRead32(0x3f0);
			u32 lcstring = irxFindLoadcore(lcptr);
			u32 list = 0;

			if (lcstring == 0)
			{
				list = lcptr - 0x20;
			}
			else
			{
				list = lcstring + 0x18;
			}

			u32 mod = iopMemRead32(list);

			while (mod != 0)
			{
				mod = iopMemRead32(mod);
			}

			return list;
		}

		// Gets the thread list ptr from thbase
		u32 GetThreadList(u32 a0reg, u32 version)
		{
			// Function 3 returns the main thread manager struct
			u32 function = iopMemRead32(a0reg + 0x20);

			// read the lui
			u32 thstruct = (iopMemRead32(function) & 0xFFFF) << 16;
			thstruct |= iopMemRead32(function + 4) & 0xFFFF;

			u32 list = thstruct + 0x42c;

			if (version > 0x101)
				list = thstruct + 0x430;

			return list;
		}

		void LoadFuncs(u32 a0reg)
		{
			const std::string modname = iopMemReadString(a0reg + 12, 8);
			ModuleVersion version = {iopMemRead8(a0 + 9), iopMemRead8(a0 + 8)};
			DevCon.WriteLn(Color_Gray, "RegisterLibraryEntries: %8.8s version %x.%02x", modname.data(), version.major, version.minor);

			if (R3000SymbolMap.AddModule(modname, version))
			{
				u32 func = a0reg + 20;
				u32 funcptr = iopMemRead32(func);
				u32 index = 0;
				while (funcptr != 0)
				{
					const std::string funcname = std::string(irxImportFuncname(modname, index));
					if (!funcname.empty())
					{
						R3000SymbolMap.AddModuleExport(modname, version, fmt::format("{}[{:02}]::{}", modname, index, funcname).c_str(), funcptr, 0);
					}
					else
					{
						R3000SymbolMap.AddModuleExport(modname, version, fmt::format("{}[{:02}]::unkn_{:02}", modname, index, index).c_str(), funcptr, 0);
					}
					index++;
					func += 4;
					funcptr = iopMemRead32(func);
				}
			}
		}

		void ReleaseFuncs(u32 a0reg)
		{
			const std::string modname = iopMemReadString(a0reg + 12, 8);
			ModuleVersion version = {iopMemRead8(a0 + 9), iopMemRead8(a0 + 8)};

			DevCon.WriteLn(Color_Gray, "ReleaseLibraryEntries: %8.8s version %x.%02x", modname.data(), version.major, version.minor);

			R3000SymbolMap.RemoveModule(modname, version);
		}

		int RegisterLibraryEntries_HLE()
		{
			LoadFuncs(a0);

			const std::string modname = iopMemReadString(a0 + 12);
			if (modname == "thbase")
			{
				const u32 version = iopMemRead32(a0 + 8);
				CurrentBiosInformation.iopThreadListAddr = GetThreadList(a0, version);
			}

			CurrentBiosInformation.iopModListAddr = GetModList(a0);
			return 0;
		}

		int ReleaseLibraryEntries_HLE()
		{
			ReleaseFuncs(a0);
			return 0;
		}
	} // namespace loadcore

	namespace intrman
	{
		// clang-format off
		static const char* intrname[] = {
			"INT_VBLANK",   "INT_GM",       "INT_CDROM",   "INT_DMA",		//00
			"INT_RTC0",     "INT_RTC1",     "INT_RTC2",    "INT_SIO0",		//04
			"INT_SIO1",     "INT_SPU",      "INT_PIO",     "INT_EVBLANK",	//08
			"INT_DVD",      "INT_PCMCIA",   "INT_RTC3",    "INT_RTC4",		//0C
			"INT_RTC5",     "INT_SIO2",     "INT_HTR0",    "INT_HTR1",		//10
			"INT_HTR2",     "INT_HTR3",     "INT_USB",     "INT_EXTR",		//14
			"INT_FWRE",     "INT_FDMA",     "INT_1A",      "INT_1B",		//18
			"INT_1C",       "INT_1D",       "INT_1E",      "INT_1F",		//1C
			"INT_dmaMDECi", "INT_dmaMDECo", "INT_dmaGPU",  "INT_dmaCD",		//20
			"INT_dmaSPU",   "INT_dmaPIO",   "INT_dmaOTC",  "INT_dmaBERR",	//24
			"INT_dmaSPU2",  "INT_dma8",     "INT_dmaSIF0", "INT_dmaSIF1",	//28
			"INT_dmaSIO2i", "INT_dmaSIO2o", "INT_2E",      "INT_2F",		//2C
			"INT_30",       "INT_31",       "INT_32",      "INT_33",		//30
			"INT_34",       "INT_35",       "INT_36",      "INT_37",		//34
			"INT_38",       "INT_39",       "INT_3A",      "INT_3B",		//38
			"INT_3C",       "INT_3D",       "INT_3E",      "INT_3F",		//3C
			"INT_MAX"														//40
		};
		// clang-format on

		void RegisterIntrHandler_DEBUG()
		{
			if (a0 < std::size(intrname) - 1)
			{
				DevCon.WriteLn(Color_Gray, "RegisterIntrHandler: intr %s, handler %x", intrname[a0], a2);
			}
			else
			{
				DevCon.WriteLn(Color_Gray, "RegisterIntrHandler: intr UNKNOWN (%d), handler %x", a0, a2);
			}
		}
	} // namespace intrman

	namespace sifcmd
	{
		void sceSifRegisterRpc_DEBUG()
		{
			DevCon.WriteLn(Color_Gray, "sifcmd sceSifRegisterRpc: rpc_id %x", a1);
		}
	} // namespace sifcmd

	u32 irxFindLoadcore(u32 entrypc)
	{
		u32 i;

		i = entrypc;
		while (entrypc - i < 0x50)
		{
			// find loadcore string
			if (iopMemRead32(i) == 0x49497350 && iopMemRead32(i + 4) == 0x64616F6C)
			{
				return i;
			}
			i -= 4;
		}

		return 0;
	}

	u32 irxImportTableAddr(u32 entrypc)
	{
		u32 i;

		i = entrypc - 0x18;
		while (entrypc - i < 0x2000)
		{
			if (iopMemRead32(i) == 0x41e00000)
				return i;
			i -= 4;
		}

		return 0;
	}

	const char* irxImportFuncname(const std::string& libname, u16 index)
	{
#include "IopModuleNames.cpp"

		switch (index)
		{
			case 0:
				return "start";
			// case 1: reinit?
			case 2:
				return "shutdown";
				// case 3: ???
		}

		return "";
	}

	// clang-format off
#define MODULE(n)          \
	if (#n == libname)     \
	{                      \
		using namespace n; \
		switch (index)     \
		{
#define END_MODULE \
	}              \
	}
#define EXPORT_D(i, n) \
	case (i):          \
		return n##_DEBUG;
#define EXPORT_H(i, n) \
	case (i):          \
		return n##_HLE;
	// clang-format on

	irxHLE irxImportHLE(const std::string& libname, u16 index)
	{
		// debugging output
		// clang-format off
		MODULE(loadcore)
			EXPORT_H(  6, RegisterLibraryEntries)
			EXPORT_H(  7, ReleaseLibraryEntries);
		END_MODULE
		MODULE(sysmem)
			EXPORT_H( 14, Kprintf)
		END_MODULE

		// Special case with ioman and iomanX
		// They are mostly compatible excluding stat structures
		if(libname == "ioman" || libname == "iomanx")
		{
			const bool use_ioman = libname == "ioman";
			using namespace ioman;
				switch(index)
				{
					EXPORT_H(  4, open)
					EXPORT_H(  5, close)
					EXPORT_H(  6, read)
					EXPORT_H(  7, write)
					EXPORT_H(  8, lseek)
					EXPORT_H( 10, remove)
					EXPORT_H( 11, mkdir)
					EXPORT_H( 12, rmdir)
					EXPORT_H( 13, dopen)
					EXPORT_H( 14, dclose)
					case 15: // dread
					if(use_ioman)
						return dread_HLE;
					else
						return dreadx_HLE;
					case 16: // getStat
					if(use_ioman)
						return getStat_HLE;
					else
						return getStatx_HLE;
				}
		}
		// clang-format on
		return 0;
	}

	irxDEBUG irxImportDebug(const std::string& libname, u16 index)
	{
		// clang-format off
		MODULE(intrman)
			EXPORT_D(  4, RegisterIntrHandler)
		END_MODULE
		MODULE(sifcmd)
			EXPORT_D( 17, sceSifRegisterRpc)
		END_MODULE
		// clang-format off

		return 0;
	}

#undef MODULE
#undef END_MODULE
#undef EXPORT_D
#undef EXPORT_H

	void irxImportLog(const std::string& libname, u16 index, const char* funcname)
	{
		PSXBIOS_LOG("%8.8s.%03d: %s (%x, %x, %x, %x)",
			libname.data(), index, funcname ? funcname : "unknown",
			a0, a1, a2, a3);
	}

	void irxImportLog_rec(u32 import_table, u16 index, const char* funcname)
	{
		irxImportLog(iopMemReadString(import_table + 12, 8), index, funcname);
	}

	int irxImportExec(u32 import_table, u16 index)
	{
		if (!import_table)
			return 0;

		std::string libname = iopMemReadString(import_table + 12, 8);
		const char* funcname = irxImportFuncname(libname, index);
		irxHLE hle = irxImportHLE(libname, index);
		irxDEBUG debug = irxImportDebug(libname, index);

		irxImportLog(libname, index, funcname);

		if (debug)
			debug();

		if (hle)
			return hle();
		else
			return 0;
	}

} // end namespace R3000A
