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
#include "IopCommon.h"
#include "R5900.h" // for g_GameStarted

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>

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
	fio_stat_t stat;
	char name[256];
	unsigned int unknown;
} fio_dirent_t;

static char HostRoot[1024];

void Hle_SetElfPath(const char* elfFileName)
{
	DevCon.WriteLn("HLE Host: Will load ELF: %s\n", elfFileName);

	const char* pos1 = strrchr(elfFileName, '/');
	const char* pos2 = strrchr(elfFileName, '\\');

	if (pos2 > pos1) // we want the LAST path separator
		pos1 = pos2;

	if (!pos1) // if pos1 is NULL, then pos2 was not > pos1, so it must also be NULL
	{
		Console.WriteLn("HLE Notice: ELF does not have a path.\n");

		// use %CD%/host/
		char* cwd = getcwd(HostRoot, 1000); // save the other 23 chars to append /host/ :P
		HostRoot[1000] = 0; // Be Safe.
		if (cwd == nullptr)
		{
			Console.Error("Hle_SetElfPath: getcwd: buffer is too small");
			return;
		}

		char* last = HostRoot + strlen(HostRoot) - 1;

		if ((*last != '/') && (*last != '\\')) // PathAppend()-ish
			last++;

		strcpy(last, "/host/");

		return;
	}

	int len = pos1 - elfFileName + 1;
	memcpy(HostRoot, elfFileName, len); // include the / (or \\)
	HostRoot[len] = 0;

	Console.WriteLn("HLE Host: Set 'host:' root path to: %s\n", HostRoot);
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

#define FIO_SO_IXOTH 0x0001
#define FIO_SO_IWOTH 0x0002
#define FIO_SO_IROTH 0x0004

#define FIO_SO_IFLNK 0x0008
#define FIO_SO_IFREG 0x0010
#define FIO_SO_IFDIR 0x0020

	static std::string host_path(const std::string path)
	{
		// We are NOT allowing to use the root of the host unit.
		// For now it just supports relative folders from the location of the elf
		if (path.rfind(HostRoot, 0) == 0)
			return path;
		else // relative paths
			return HostRoot + path;
	}

	static int host_stat(const std::string path, fio_stat_t* host_stats)
	{
		struct stat file_stats;

		if (::stat(path.c_str(), &file_stats))
			return -IOP_ENOENT;

		host_stats->size = file_stats.st_size;
		host_stats->hisize = 0;

		// Convert the mode.
		host_stats->mode = (file_stats.st_mode & (FIO_SO_IROTH | FIO_SO_IWOTH | FIO_SO_IXOTH));
#ifndef _WIN32
		if (S_ISLNK(file_stats.st_mode))
		{
			host_stats->mode |= FIO_SO_IFLNK;
		}
#endif
		if (S_ISREG(file_stats.st_mode))
		{
			host_stats->mode |= FIO_SO_IFREG;
		}
		if (S_ISDIR(file_stats.st_mode))
		{
			host_stats->mode |= FIO_SO_IFDIR;
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
			const std::string path = full_path.substr(full_path.find(':') + 1);
			int native_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 0644
			int native_flags = O_BINARY;                             // necessary in Windows.

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

			int hostfd = ::open(host_path(path).data(), native_flags, native_mode);
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

		virtual int read(void* buf, u32 count)
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
		DIR* dir;
		std::string path;

		HostDir(DIR* native_dir, const std::string native_path)
		{
			dir = native_dir;
			path = native_path;
		}

		virtual ~HostDir() = default;

		static int open(IOManDir** dir, const std::string& full_path)
		{
			const std::string relativePath = full_path.substr(full_path.find(':') + 1);
			const std::string path = host_path(relativePath);

			DIR* dirent = ::opendir(path.c_str());
			if (!dirent)
				return -IOP_ENOENT; // Should return ENOTDIR if path is a file?

			*dir = new HostDir(dirent, relativePath);
			if (!*dir)
				return -IOP_ENOMEM;

			return 0;
		}

		virtual int read(void* buf)
		{
			fio_dirent_t* hostcontent = (fio_dirent_t*)buf;
			struct dirent* dire = ::readdir(dir);

			if (dire == NULL)
				return 0;

			strcpy(hostcontent->name, dire->d_name);
			host_stat(host_path(path + dire->d_name), &hostcontent->stat);

			return 1;
		}

		virtual void close()
		{
			::closedir(dir);
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

			return ((!g_GameStarted || EmuConfig.HostFs) && 0 == path.compare(0, 4, "host") && path[not_number_pos] == ':');
		}

		int open_HLE()
		{
			IOManFile* file = NULL;
			const std::string path = Ra0;
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
			const std::string path = Ra0;

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

		int dread_HLE()
		{
			s32 fh = a0;
			u32 data = a1;

			if (IOManDir* dir = getfd<IOManDir>(fh))
			{
				char buf[sizeof(fio_dirent_t)];
				v0 = dir->read(&buf);

				for (s32 i = 0; i < (s32)sizeof(fio_dirent_t); i++)
					iopMemWrite8(data + i, buf[i]);

				pc = ra;
				return 1;
			}

			return 0;
		}

		int getStat_HLE()
		{
			const std::string path = Ra0;
			u32 data = a1;

			if (is_host(path))
			{
				const std::string full_path = host_path(path.substr(path.find(':') + 1));
				char buf[sizeof(fio_stat_t)];
				v0 = host_stat(full_path, (fio_stat_t*)&buf);

				for (s32 i = 0; i < (s32)sizeof(fio_stat_t); i++)
					iopMemWrite8(data + i, buf[i]);

				pc = ra;
				return 1;
			}

			return 0;
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

		int mkdir_HLE()
		{
			const std::string full_path = Ra0;

			if (is_host(full_path))
			{
				const std::string path = full_path.substr(full_path.find(':') + 1);
				int tmpError;
#ifdef _WIN32
				tmpError = ::mkdir(host_path(path).data());
#else
				tmpError = ::mkdir(host_path(path).data(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
				v0 = HostFile::translate_error(tmpError);
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
			const std::string full_path = Ra0;

			if (is_host(full_path))
			{
				const std::string path = full_path.substr(full_path.find(':') + 1);
				v0 = HostFile::translate_error(::rmdir(host_path(path).data()));
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
		}
	} // namespace sysmem

	namespace loadcore
	{
		void RegisterLibraryEntries_DEBUG()
		{
			const std::string modname = iopMemReadString(a0 + 12);
			DevCon.WriteLn(Color_Gray, "RegisterLibraryEntries: %8.8s version %x.%02x", modname.data(), (unsigned)iopMemRead8(a0 + 9), (unsigned)iopMemRead8(a0 + 8));
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
			DevCon.WriteLn(Color_Gray, "RegisterIntrHandler: intr %s, handler %x", intrname[a0], a2);
		}
	} // namespace intrman

	namespace sifcmd
	{
		void sceSifRegisterRpc_DEBUG()
		{
			DevCon.WriteLn(Color_Gray, "sifcmd sceSifRegisterRpc: rpc_id %x", a1);
		}
	} // namespace sifcmd

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

		return 0;
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
		MODULE(sysmem)
			EXPORT_H( 14, Kprintf)
		END_MODULE
		
		MODULE(ioman)
			EXPORT_H(  4, open)
			EXPORT_H(  5, close)
			EXPORT_H(  6, read)
			EXPORT_H(  7, write)
			EXPORT_H(  8, lseek)
			EXPORT_H( 11, mkdir)
			EXPORT_H( 12, rmdir)
			EXPORT_H( 13, dopen)
			EXPORT_H( 14, dclose)
			EXPORT_H( 15, dread)
			EXPORT_H( 16, getStat)
		END_MODULE
		// clang-format on

		return 0;
	}

	irxDEBUG irxImportDebug(const std::string& libname, u16 index)
	{
		// clang-format off
		MODULE(loadcore)
			EXPORT_D(  6, RegisterLibraryEntries)
		END_MODULE
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

	void __fastcall irxImportLog_rec(u32 import_table, u16 index, const char* funcname)
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
