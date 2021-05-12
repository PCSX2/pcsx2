/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
#include <string>

static void* handle;

void help()
{
	fprintf(stderr, "Loader gs file\n");
	fprintf(stderr, "ARG1 GS plugin\n");
	fprintf(stderr, "ARG2 .gs file\n");
	fprintf(stderr, "ARG3 Ini directory\n");
	if (handle)
	{
		dlclose(handle);
	}
	exit(1);
}

char* read_env(const char* var)
{
	char* v = getenv(var);
	if (!v)
	{
		fprintf(stderr, "Failed to get %s\n", var);
		help();
	}
	return v;
}

int main(int argc, char* argv[])
{
	if (argc < 1)
		help();

	char* plugin;
	char* gs;
	if (argc > 2)
	{
		plugin = argv[1];
		gs = argv[2];
	}
	else
	{
		plugin = read_env("GSDUMP_SO");
		gs = argv[1];
	}

	handle = dlopen(plugin, RTLD_LAZY | RTLD_GLOBAL);
	if (handle == NULL)
	{
		fprintf(stderr, "Failed to dlopen plugin %s\n", plugin);
		help();
	}

	__attribute__((stdcall)) void (*GSsetSettingsDir_ptr)(const char*);
	__attribute__((stdcall)) void (*GSReplay_ptr)(char*, int);

	GSsetSettingsDir_ptr = reinterpret_cast<decltype(GSsetSettingsDir_ptr)>(dlsym(handle, "GSsetSettingsDir"));
	GSReplay_ptr = reinterpret_cast<decltype(GSReplay_ptr)>(dlsym(handle, "GSReplay"));

	if (argc == 2)
	{
		char* ini = read_env("GSDUMP_CONF");

		GSsetSettingsDir_ptr(ini);
	}
	else if (argc == 4)
	{
		GSsetSettingsDir_ptr(argv[3]);
	}
	else if (argc == 3)
	{
#ifdef XDG_STD
		char* val = read_env("HOME");

		std::string ini_dir(val);
		ini_dir += "/.config/pcsx2/inis";

		GSsetSettingsDir_ptr(ini_dir.c_str());
#else
		fprintf(stderr, "default ini dir only supported on XDG\n");
		help();
#endif
	}

	GSReplay_ptr(gs, 12);

	if (handle)
	{
		dlclose(handle);
	}
}
