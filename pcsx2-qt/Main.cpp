/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include <QtWidgets/QApplication>
#include <cstdlib>
#include <csignal>

#include "MainWindow.h"
#include "EmuThread.h"
#include "QtHost.h"

#include "CDVD/CDVD.h"
#include "Frontend/GameList.h"
#include "svnrev.h"

static void PrintCommandLineVersion()
{
	std::fprintf(stderr, "PCSX2 Version %s\n", GIT_REV);
	std::fprintf(stderr, "https://pcsx2.net/\n");
	std::fprintf(stderr, "\n");
}

static void PrintCommandLineHelp(const char* progname)
{
	PrintCommandLineVersion();
	std::fprintf(stderr, "Usage: %s [parameters] [--] [boot filename]\n", progname);
	std::fprintf(stderr, "\n");
	std::fprintf(stderr, "  -help: Displays this information and exits.\n");
	std::fprintf(stderr, "  -version: Displays version information and exits.\n");
	std::fprintf(stderr, "  -batch: Enables batch mode (exits after shutting down).\n");
	std::fprintf(stderr, "  -elf <file>: Overrides the boot ELF with the specified filename.\n");
	std::fprintf(stderr, "  -disc <path>: Uses the specified host DVD drive as a source.\n");
	std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
	std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
	std::fprintf(stderr, "  -resume: Load resume save state. If a boot filename is provided,\n"
						 "    that game's resume state will be loaded, otherwise the most\n"
						 "    recent resume save state will be loaded.\n");
	std::fprintf(stderr, "  -state <index>: Loads specified save state by index.\n");
	std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n"
						 "    No boot filename is required with this option.\n");
	std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
	std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
	std::fprintf(stderr, "  -portable: Forces \"portable mode\", data in same directory.\n");
	std::fprintf(stderr, "  -settings <filename>: Loads a custom settings configuration from the\n"
						 "    specified filename. Default settings applied if file not found.\n");
	std::fprintf(stderr, "  --: Signals that no more arguments will follow and the remaining\n"
						 "    parameters make up the filename. Use when the filename contains\n"
						 "    spaces or starts with a dash.\n");
	std::fprintf(stderr, "\n");
}

static std::shared_ptr<VMBootParameters>& AutoBoot(std::shared_ptr<VMBootParameters>& autoboot)
{
	if (!autoboot)
		autoboot = std::make_shared<VMBootParameters>();

	return autoboot;
}

static bool ParseCommandLineOptions(int argc, char* argv[], std::shared_ptr<VMBootParameters>& autoboot)
{
	std::optional<bool> fast_boot;
	std::optional<bool> start_fullscreen;
	std::optional<s32> state_index;
	std::string state_filename;
	bool no_more_args = false;

	for (int i = 1; i < argc; i++)
	{
		if (!no_more_args)
		{
#define CHECK_ARG(str) !std::strcmp(argv[i], str)
#define CHECK_ARG_PARAM(str) (!std::strcmp(argv[i], str) && ((i + 1) < argc))

			if (CHECK_ARG("-help"))
			{
				PrintCommandLineHelp(argv[0]);
				return false;
			}
			else if (CHECK_ARG("-version"))
			{
				PrintCommandLineVersion();
				return false;
			}
			else if (CHECK_ARG("-batch"))
			{
				Console.WriteLn("Enabling batch mode.");
				AutoBoot(autoboot)->batch_mode = true;
				continue;
			}
			else if (CHECK_ARG("-fastboot"))
			{
				Console.WriteLn("Forcing fast boot.");
				fast_boot = true;
				continue;
			}
			else if (CHECK_ARG("-slowboot"))
			{
				Console.WriteLn("Forcing slow boot.");
				fast_boot = false;
				continue;
			}
			else if (CHECK_ARG("-resume"))
			{
				state_index = -1;
				continue;
			}
			else if (CHECK_ARG_PARAM("-state"))
			{
				state_index = std::atoi(argv[++i]);
				continue;
			}
			else if (CHECK_ARG_PARAM("-statefile"))
			{
				AutoBoot(autoboot)->save_state = argv[++i];
				continue;
			}
			else if (CHECK_ARG_PARAM("-elf"))
			{
				AutoBoot(autoboot)->elf_override = argv[++i];
				continue;
			}
			else if (CHECK_ARG_PARAM("-disc"))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::Disc;
				AutoBoot(autoboot)->filename = argv[++i];
				continue;
			}
			else if (CHECK_ARG("-fullscreen"))
			{
				Console.WriteLn("Going fullscreen after booting.");
				start_fullscreen = true;
				continue;
			}
			else if (CHECK_ARG("-nofullscreen"))
			{
				Console.WriteLn("Preventing fullscreen after booting.");
				start_fullscreen = false;
				continue;
			}
			else if (CHECK_ARG("-portable"))
			{
				Console.WriteLn("Using portable mode.");
				// SetUserDirectoryToProgramDirectory();
				continue;
			}
			else if (CHECK_ARG("-resume"))
			{
				state_index = -1;
				continue;
			}
			else if (CHECK_ARG("--"))
			{
				no_more_args = true;
				continue;
			}
			else if (argv[i][0] == '-')
			{
				Console.Error("Unknown parameter: '%s'", argv[i]);
				return false;
			}

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
		}

		if (!AutoBoot(autoboot)->filename.empty())
			AutoBoot(autoboot)->filename += ' ';

		AutoBoot(autoboot)->filename += argv[i];
	}

	return true;
}

int main(int argc, char* argv[])
{
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	QApplication app(argc, argv);
	std::shared_ptr<VMBootParameters> autoboot;
	if (!ParseCommandLineOptions(argc, argv, autoboot))
		return EXIT_FAILURE;

	MainWindow* main_window = new MainWindow(QApplication::style()->objectName());

	if (!QtHost::Initialize())
	{
		delete main_window;
		return EXIT_FAILURE;
	}

	main_window->initialize();
	EmuThread::start();

	main_window->refreshGameList(false);
	main_window->show();

	if (autoboot)
		g_emu_thread->startVM(std::move(autoboot));

	const int result = app.exec();

	QtHost::Shutdown();
	return result;
}
