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
#include <QtWidgets/QMessageBox>
#include <cstdlib>
#include <csignal>

#include "MainWindow.h"
#include "EmuThread.h"
#include "QtHost.h"

#include "CDVD/CDVD.h"
#include "Frontend/GameList.h"

#include "common/CrashHandler.h"

static void PrintCommandLineVersion()
{
	QtHost::InitializeEarlyConsole();
	std::fprintf(stderr, "%s\n", (QtHost::GetAppNameAndVersion() + QtHost::GetAppConfigSuffix()).toUtf8().constData());
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
	std::fprintf(stderr, "  -bios: Starts the BIOS (System Menu/OSDSYS).\n");
	std::fprintf(stderr, "  -fastboot: Force fast boot for provided filename.\n");
	std::fprintf(stderr, "  -slowboot: Force slow boot for provided filename.\n");
	std::fprintf(stderr, "  -state <index>: Loads specified save state by index.\n");
	std::fprintf(stderr, "  -statefile <filename>: Loads state from the specified filename.\n");
	std::fprintf(stderr, "  -fullscreen: Enters fullscreen mode immediately after starting.\n");
	std::fprintf(stderr, "  -nofullscreen: Prevents fullscreen mode from triggering if enabled.\n");
	std::fprintf(stderr, "  -earlyconsolelog: Forces logging of early console messages to console.\n");
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
				QtHost::SetBatchMode(true);
				continue;
			}
			else if (CHECK_ARG("-fastboot"))
			{
				AutoBoot(autoboot)->fast_boot = true;
				continue;
			}
			else if (CHECK_ARG("-slowboot"))
			{
				AutoBoot(autoboot)->fast_boot = false;
				continue;
			}
			else if (CHECK_ARG_PARAM("-state"))
			{
				AutoBoot(autoboot)->state_index = std::atoi(argv[++i]);
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
			else if (CHECK_ARG("-bios"))
			{
				AutoBoot(autoboot)->source_type = CDVD_SourceType::NoDisc;
				continue;
			}
			else if (CHECK_ARG("-fullscreen"))
			{
				AutoBoot(autoboot)->fullscreen = true;
				continue;
			}
			else if (CHECK_ARG("-nofullscreen"))
			{
				AutoBoot(autoboot)->fullscreen = false;
				continue;
			}
			else if (CHECK_ARG("-earlyconsolelog"))
			{
				QtHost::InitializeEarlyConsole();
				continue;
			}
			else if (CHECK_ARG("--"))
			{
				no_more_args = true;
				continue;
			}
			else if (argv[i][0] == '-')
			{
				QtHost::InitializeEarlyConsole();
				std::fprintf(stderr, "Unknown parameter: '%s'", argv[i]);
				return false;
			}

#undef CHECK_ARG
#undef CHECK_ARG_PARAM
		}

		if (!AutoBoot(autoboot)->filename.empty())
			AutoBoot(autoboot)->filename += ' ';

		AutoBoot(autoboot)->filename += argv[i];
	}

	// check autoboot parameters, if we set something like fullscreen without a bios
	// or disc, we don't want to actually start.
	if (autoboot && !autoboot->source_type.has_value() && autoboot->filename.empty() && autoboot->elf_override.empty())
	{
		QtHost::InitializeEarlyConsole();
		Console.Warning("Skipping autoboot due to no boot parameters.");
		autoboot.reset();
	}

	// if we don't have autoboot, we definitely don't want batch mode (because that'll skip
	// scanning the game list).
	if (QtHost::InBatchMode() && !autoboot)
	{
		QtHost::InitializeEarlyConsole();
		Console.Warning("Disabling batch mode, because we have no autoboot.");
		QtHost::SetBatchMode(false);
	}

	return true;
}

#ifndef _WIN32

// See note at the end of the file as to why we don't do this on Windows.
static bool PerformEarlyHardwareChecks()
{
	// NOTE: No point translating this message, because the configuration isn't loaded yet, so we
	// won't know which language to use, and loading the configuration uses float instructions.
	const char* error;
	if (VMManager::PerformEarlyHardwareChecks(&error))
		return true;

	QMessageBox::critical(nullptr, QStringLiteral("Hardware Check Failed"), QString::fromUtf8(error));
	return false;
}

#endif

int main(int argc, char* argv[])
{
	CrashHandler::Install();

	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

	QApplication app(argc, argv);

#ifndef _WIN32
	if (!PerformEarlyHardwareChecks())
		return EXIT_FAILURE;
#endif

	std::shared_ptr<VMBootParameters> autoboot;
	if (!ParseCommandLineOptions(argc, argv, autoboot))
		return EXIT_FAILURE;

	MainWindow* main_window = new MainWindow(QApplication::style()->objectName());

	if (!QtHost::Initialize())
	{
		delete main_window;
		return EXIT_FAILURE;
	}

	// actually show the window, the emuthread might still be starting up at this point
	main_window->initialize();

	// skip scanning the game list when running in batch mode
	if (!QtHost::InBatchMode())
		main_window->refreshGameList(false);

	main_window->show();

	if (autoboot)
		g_emu_thread->startVM(std::move(autoboot));
	else
		main_window->startupUpdateCheck();

	const int result = app.exec();

	QtHost::Shutdown();
	return result;
}
