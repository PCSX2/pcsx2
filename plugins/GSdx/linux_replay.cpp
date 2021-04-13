/*
 *	Copyright (C) 2011-2012 Hainaut gregory
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static void* handle;
static const char* progName;

void help()
{
	fprintf(stderr, "Usage: %s [-p GSdx plugin] [-c INI directory] [-o png output template] [-r sw|hw] input.gs\n", progName);
	fprintf(stderr, "PNG output template can contain a %%d to indicate the frame number\n");
	fprintf(stderr, "GSdx plugin and INI directory can also be supplied via the GSDUMP_SO and GSDUMP_CONF environment variables\n");
	fprintf(stderr, "If no renderer is supplied, the default specified in the ini will be used\n");
	if (handle) {
		dlclose(handle);
	}
	exit(1);
}

char* read_env(const char* var) {
	char* v = getenv(var);
	if (!v) {
		fprintf(stderr, "Failed to get %s\n", var);
		help();
	}
	return v;
}

int main ( int argc, char *argv[] )
{
	progName = argv[0];
	const char* plugin = nullptr;
	const char* gs = nullptr;
	const char* ini = nullptr;
	const char* output = nullptr;
	int renderer = 0;

	int c;
	while ((c = getopt(argc, argv, "p:o:c:r:")) != -1)
	{
		switch (c)
		{
			case 'p':
				plugin = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'c':
				ini = optarg;
				break;
			case 'r':
				if (0 == strcmp(optarg, "sw"))
					renderer = 13;
				else if (0 == strcmp(optarg, "gl") || 0 == strcmp(optarg, "hw"))
					renderer = 12;
				else
				{
					fprintf(stderr, "Unknown renderer %s", optarg);
					help();
				}
				break;
			case '?':
				if (strchr("pocr", optopt) != nullptr)
					fprintf(stderr, "Option -%c requires an argument\n", optopt);
				else if (isprint(optopt))
					fprintf(stderr, "Unknown option -%c\n", optopt);
				else
					fprintf(stderr, "Unknown option character '\\x%x'\n", optopt);
				help();
		}
	}
	if (argc - optind != 1)
	{
		if (argc - optind > 1)
			fprintf(stderr, "Additional unrecognized arguments\n");
		help();
	}
	gs = argv[optind];

	if (!plugin)
		plugin = read_env("GSDUMP_SO");

	handle = dlopen(plugin, RTLD_LAZY|RTLD_GLOBAL);
	if (handle == NULL) {
		fprintf(stderr, "Failed to dlopen plugin %s\n", plugin);
		help();
	}

	__attribute__((stdcall)) void (*GSsetSettingsDir_ptr)(const char*);
	__attribute__((stdcall)) void (*GSReplay_ptr)(const char*, int);
	__attribute__((stdcall)) void (*GSReplayDumpFrames_ptr)(const char*, int, const char*);

	GSsetSettingsDir_ptr = reinterpret_cast<decltype(GSsetSettingsDir_ptr)>(dlsym(handle, "GSsetSettingsDir"));
	GSReplay_ptr = reinterpret_cast<decltype(GSReplay_ptr)>(dlsym(handle, "GSReplay"));
	GSReplayDumpFrames_ptr = reinterpret_cast<decltype(GSReplayDumpFrames_ptr)>(dlsym(handle, "GSReplayDumpFrames"));

	if (!ini)
		ini = getenv("GSDUMP_CONF");
	if (ini)
	{
		GSsetSettingsDir_ptr(ini);
	}
	else
	{
#ifdef XDG_STD
		char *val = read_env("HOME");

		std::string ini_dir(val);
		ini_dir += "/.config/pcsx2/inis";

		GSsetSettingsDir_ptr(ini_dir.c_str());
#else
		fprintf(stderr, "default ini dir only supported on XDG\n");
		help();
#endif
	}

	if (output)
		GSReplayDumpFrames_ptr(gs, renderer, output);
	else
		GSReplay_ptr(gs, renderer);

	if (handle) {
		dlclose(handle);
	}
}
