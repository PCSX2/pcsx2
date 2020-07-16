/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#ifndef O_BINARY
#define O_BINARY 0
#endif

// set this to 0 to disable rewriting 'host:' paths!
#define USE_HOST_REWRITE 1

#if USE_HOST_REWRITE
#	ifdef _WIN32
		// disable this if you DON'T want "host:/usr/local/" paths
		// to get rewritten into host:/
#		define HOST_REWRITE_USR_LOCAL 1
#	else
		// unix/linux users might want to set it to 1
		// if they DO want to keep demos from accessing their systems' /usr/local
#		define HOST_REWRITE_USR_LOCAL 0
#	endif

static char HostRoot[1024];
#endif

void Hle_SetElfPath(const char* elfFileName)
{
#if USE_HOST_REWRITE
	DevCon.WriteLn("HLE Host: Will load ELF: %s\n", elfFileName);

	const char* pos1 = strrchr(elfFileName,'/');
	const char* pos2 = strrchr(elfFileName,'\\');

	if(pos2 > pos1) // we want the LAST path separator
		pos1=pos2;

	if(!pos1) // if pos1 is NULL, then pos2 was not > pos1, so it must also be NULL
	{
		Console.WriteLn("HLE Notice: ELF does not have a path.\n");

		// use %CD%/host/
		char* cwd = getcwd(HostRoot,1000); // save the other 23 chars to append /host/ :P
		HostRoot[1000]=0; // Be Safe.
		if (cwd == nullptr) {
			Console.Error("Hle_SetElfPath: getcwd: buffer is too small");
			return;
		}

		char* last = HostRoot + strlen(HostRoot) - 1;

		if((*last!='/') && (*last!='\\')) // PathAppend()-ish
			last++;

		strcpy(last,"/host/");

		return;
	}

	int len = pos1-elfFileName+1;
	memcpy(HostRoot,elfFileName,len); // include the / (or \\)
	HostRoot[len] = 0;

	Console.WriteLn("HLE Host: Set 'host:' root path to: %s\n", HostRoot);

#endif
}

namespace R3000A {

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

static std::string host_path(const std::string path)
{
// WIP code. Works well on win32, not so sure on unixes
// TODO: get rid of dependency on CWD/PWD
#if USE_HOST_REWRITE
	// we want filenames to be relative to pcs2dir / host

	std::string pathMod;

	// partial "rooting",
	// it will NOT avoid a path like "../../x" from escaping the pcsx2 folder!


	const std::string _local_root = "/usr/local/";
	if (HOST_REWRITE_USR_LOCAL && 0 == path.compare(0, _local_root.size(), _local_root.data())) {
		return HostRoot + path.substr(_local_root.size());
	} else if ((path[0] == '/') || (path[0] == '\\') || (isalpha(path[0]) && (path[1] == ':'))) // absolute NATIVE path (X:\blah)
	{
		// TODO: allow some way to use native paths in non-windows platforms
		// maybe hack it so linux prefixes the path with "X:"? ;P
		// or have all platforms use a common prefix for native paths
		// FIXME: Why the hell would we allow this?
		return path;
	} else // relative paths
		return HostRoot + path;

	return pathMod;
#else
	return path;
#endif
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

		switch(err)
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

	static int open(IOManFile **file, const std::string &full_path, s32 flags, u16 mode)
	{
		const std::string path = full_path.substr(full_path.find(':') + 1);

		// host: actually DOES let you write!
		//if (flags != IOP_O_RDONLY)
		//	return -IOP_EROFS;

		int native_flags = O_BINARY; // necessary in Windows.

		switch(flags&IOP_O_RDWR)
		{
		case IOP_O_RDONLY:	native_flags |= O_RDONLY;	break;
		case IOP_O_WRONLY:	native_flags |= O_WRONLY; break;
		case IOP_O_RDWR:	native_flags |= O_RDWR;	break;
		}

		if(flags&IOP_O_APPEND)	native_flags |= O_APPEND;
		if(flags&IOP_O_CREAT)	native_flags |= O_CREAT;
		if(flags&IOP_O_TRUNC)	native_flags |= O_TRUNC;

		int hostfd = ::open(host_path(path).data(), native_flags);
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

	virtual int read(void *buf, u32 count)
	{
		return translate_error(::read(fd, buf, count));
	}

	virtual int write(void *buf, u32 count)
	{
		return translate_error(::write(fd, buf, count));
	}
};

namespace ioman {
	const int firstfd = 0x100;
	const int maxfds = 0x100;
	int openfds = 0;

	int freefdcount()
	{
		return maxfds - openfds;
	}

	struct filedesc
	{
		enum {
			FILE_FREE,
			FILE_FILE,
			FILE_DIR,
		} type;
		union {
			IOManFile *file;
			IOManDir *dir;
		};

		operator bool() const { return type != FILE_FREE; }
		operator IOManFile*() const { return type == FILE_FILE ? file : NULL; }
		operator IOManDir*() const { return type == FILE_DIR ? dir : NULL; }
		void operator=(IOManFile *f) { type = FILE_FILE; file = f; openfds++; }
		void operator=(IOManDir *d) { type = FILE_DIR; dir = d; openfds++; }

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

	template<typename T>
	T* getfd(int fd)
	{
		fd -= firstfd;

		if (fd < 0 || fd >= maxfds)
			return NULL;

		return fds[fd];
	}

	template <typename T>
	int allocfd(T *obj)
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
			fds[i].close();
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
		IOManFile *file = NULL;
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

	int lseek_HLE()
	{
		s32 fd = a0;
		s32 offset = a1;
		s32 whence = a2;

		if (IOManFile *file = getfd<IOManFile>(fd))
		{
			v0 = file->lseek(offset, whence);
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

		if (IOManFile *file = getfd<IOManFile>(fd))
		{
			try {
				std::unique_ptr<char[]> buf(new char[count]);

				v0 = file->read(buf.get(), count);

				for (s32 i = 0; i < (s32)v0; i++)
					iopMemWrite8(data + i, buf[i]);
			}
			catch (const std::bad_alloc &) {
				v0 = -IOP_ENOMEM;
			}

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
		else if (IOManFile *file = getfd<IOManFile>(fd))
		{
			try {
				std::unique_ptr<char[]> buf(new char[count]);

				for (u32 i = 0; i < count; i++)
					buf[i] = iopMemRead8(data + i);

				v0 = file->write(buf.get(), count);
			}
			catch (const std::bad_alloc &) {
				v0 = -IOP_ENOMEM;
			}

			pc = ra;
			return 1;
		}

		return 0;
	}
}

namespace sysmem {
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

		if (!SysConsole.iopConsole.IsActive()) return 1;

		char tmp[1024], tmp2[1024];
		char *ptmp = tmp;
		int n=1, i=0, j = 0;

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
						case 'f': case 'F':
							ptmp+= sprintf(ptmp, tmp2, (float)iopMemRead32(sp + n * 4));
							n++;
							break;

						case 'a': case 'A':
						case 'e': case 'E':
						case 'g': case 'G':
							ptmp+= sprintf(ptmp, tmp2, (double)iopMemRead32(sp + n * 4));
							n++;
							break;

						case 'p':
						case 'i':
						case 'd': case 'D':
						case 'o': case 'O':
						case 'x': case 'X':
							ptmp+= sprintf(ptmp, tmp2, (u32)iopMemRead32(sp + n * 4));
							n++;
							break;

						case 'c':
							ptmp+= sprintf(ptmp, tmp2, (u8)iopMemRead32(sp + n * 4));
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
		iopConLog( ShiftJIS_ConvertString(tmp, 1023) );

		return 1;
	}
}

namespace loadcore {
	void RegisterLibraryEntries_DEBUG()
	{
		const std::string modname = iopMemReadString(a0 + 12);
		DevCon.WriteLn(Color_Gray, "RegisterLibraryEntries: %8.8s version %x.%02x", modname.data(), (unsigned)iopMemRead8(a0 + 9), (unsigned)iopMemRead8(a0 + 8));
	}
}

namespace intrman {
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

	void RegisterIntrHandler_DEBUG()
	{
		DevCon.WriteLn(Color_Gray, "RegisterIntrHandler: intr %s, handler %x", intrname[a0], a2);
	}
}

namespace sifcmd {
	void sceSifRegisterRpc_DEBUG()
	{
		DevCon.WriteLn( Color_Gray, "sifcmd sceSifRegisterRpc: rpc_id %x", a1);
	}
}

u32 irxImportTableAddr(u32 entrypc)
{
	u32 i;

	i = entrypc - 0x18;
	while (entrypc - i < 0x2000) {
		if (iopMemRead32(i) == 0x41e00000)
			return i;
		i -= 4;
	}

	return 0;
}

const char* irxImportFuncname(const std::string &libname, u16 index)
{
	#include "IopModuleNames.cpp"

	switch (index) {
		case 0: return "start";
		// case 1: reinit?
		case 2: return "shutdown";
		// case 3: ???
	}

	return 0;
}

#define MODULE(n) if (#n == libname) { using namespace n; switch (index) {
#define END_MODULE }}
#define EXPORT_D(i, n) case (i): return n ## _DEBUG;
#define EXPORT_H(i, n) case (i): return n ## _HLE;

irxHLE irxImportHLE(const std::string &libname, u16 index)
{
	// debugging output
	MODULE(sysmem)
		EXPORT_H( 14, Kprintf)
	END_MODULE

	MODULE(ioman)
		EXPORT_H(  4, open)
		EXPORT_H(  5, close)
		EXPORT_H(  6, read)
		EXPORT_H(  7, write)
		EXPORT_H(  8, lseek)
	END_MODULE

	return 0;
}

irxDEBUG irxImportDebug(const std::string &libname, u16 index)
{
	MODULE(loadcore)
		EXPORT_D(  6, RegisterLibraryEntries)
	END_MODULE
	MODULE(intrman)
		EXPORT_D(  4, RegisterIntrHandler)
	END_MODULE
	MODULE(sifcmd)
		EXPORT_D( 17, sceSifRegisterRpc)
	END_MODULE

	return 0;
}

#undef MODULE
#undef END_MODULE
#undef EXPORT_D
#undef EXPORT_H

void irxImportLog(const std::string &libname, u16 index, const char *funcname)
{
	PSXBIOS_LOG("%8.8s.%03d: %s (%x, %x, %x, %x)",
		libname.data(), index, funcname ? funcname : "unknown",
		a0, a1, a2, a3);
}

void __fastcall irxImportLog_rec(u32 import_table, u16 index, const char *funcname)
{
	irxImportLog(iopMemReadString(import_table + 12, 8), index, funcname);
}

int irxImportExec(u32 import_table, u16 index)
{
	if (!import_table)
		return 0;

	std::string libname = iopMemReadString(import_table + 12, 8);
	const char *funcname = irxImportFuncname(libname, index);
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

}	// end namespace R3000A
