// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ShortcutCreationDialog.h"
#include "QtHost.h"
#include <fmt/format.h>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "VMManager.h"

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include <Knownfolders.h>
#endif

ShortcutCreationDialog::ShortcutCreationDialog(QWidget* parent, const QString& game_title, const QString& game_serial, const QString& game_path)
	: QDialog(parent)
	, m_game_title(Path::SanitizeFileName(game_title.toStdString()))
	, m_game_serial(game_serial.toStdString())
	, m_game_path(game_path.toStdString())
{
	if (m_game_title.empty())
	{
		Console.Error("Cannot create shortcut: Game title is empty.");
		QMessageBox::critical(this, tr("Cannot create shortcut"), tr("Game title is empty."),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		QTimer::singleShot(0, this, &QDialog::reject);
		return;
	}

	if (m_game_path.empty())
	{
		Console.Error("Cannot create shortcut: Game path is empty.");
		QMessageBox::critical(this, tr("Cannot create shortcut"), tr("Game path is empty."),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		QTimer::singleShot(0, this, &QDialog::reject);
		return;
	}

	m_ui.setupUi(this);
	this->setWindowTitle(tr("Create Shortcut For %1").arg(game_title));
	this->setWindowIcon(QtHost::GetAppIcon());

#if defined(_WIN32)
	m_ui.shortcutStartMenu->setText(tr("Start Menu"));
#else
	m_ui.shortcutStartMenu->setText(tr("Application Launcher"));
#endif

	connect(m_ui.overrideBootELFBrowse, &QPushButton::clicked, [&]() {
		const QString elf_file_path = QFileDialog::getOpenFileName(this, tr("Select ELF File"),
			QString(), tr("ELF Files (*.elf);;All Files (*.*)"));
		if (!elf_file_path.isEmpty())
			m_ui.overrideBootELFPath->setText(Path::ToNativePath(elf_file_path.toStdString()).c_str());
	});

	connect(m_ui.loadStateFileBrowse, &QPushButton::clicked, [&]() {
		const QString state_file_path = QFileDialog::getOpenFileName(this, tr("Select Save State File"),
			QString(), tr("Save States (*.p2s);;All Files (*.*)"));
		if (!state_file_path.isEmpty())
			m_ui.loadStateFilePath->setText(Path::ToNativePath(state_file_path.toStdString()).c_str());
	});

	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFPath, &QLineEdit::setEnabled);
	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFBrowse, &QPushButton::setEnabled);
	connect(m_ui.gameArgsToggle, &QCheckBox::toggled, m_ui.gameArgs, &QLineEdit::setEnabled);
	connect(m_ui.loadStateIndexToggle, &QCheckBox::toggled, m_ui.loadStateIndex, &QSpinBox::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFilePath, &QLineEdit::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFileBrowse, &QPushButton::setEnabled);
	connect(m_ui.bootOptionToggle, &QCheckBox::toggled, m_ui.bootOptionDropdown, &QPushButton::setEnabled);
	connect(m_ui.fullscreenMode, &QCheckBox::toggled, m_ui.fullscreenModeDropdown, &QPushButton::setEnabled);

	m_ui.loadStateIndex->setMaximum(VMManager::NUM_SAVE_STATE_SLOTS);

	// Check for Flatpak environment.
	if (std::getenv("container"))
	{
		m_ui.portableModeToggle->setEnabled(false);
		m_ui.shortcutDesktop->setEnabled(false);
		m_ui.shortcutStartMenu->setEnabled(false);
	}

	connect(m_ui.dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_ui.dialogButtons, &QDialogButtonBox::accepted, this, [&]() {
		std::vector<std::string> standard_arguments;
		ShortcutCreationDialog::FillArgumentsList(standard_arguments);

		ShortcutCreationDialog::CreateGameShortcut(standard_arguments, m_ui.customArgsInput->text().toStdString(), m_ui.shortcutDesktop->isChecked());

		accept();
	});
}


void ShortcutCreationDialog::CreateGameShortcut(std::vector<std::string>& standard_arguments, std::string custom_arguments, const bool is_desktop)
{
	std::string shortcut_comment;
	switch (m_save_state_type)
	{
		case StateType::None:
			shortcut_comment = tr("Start %1 (%2) on PCSX2.")
			                       .arg(m_game_title, m_game_serial)
			                       .toStdString();
			break;
		case StateType::Index:
			shortcut_comment = tr("Start %1 (%2) on PCSX2 from save state index %3.")
			                       .arg(m_game_title, m_game_serial, m_save_state_name)
			                       .toStdString();
			break;
		case StateType::File:
			shortcut_comment = tr("Start %1 (%2) on PCSX2 from save state file %3.")
			                       .arg(m_game_title, m_game_serial, m_save_state_name)
			                       .toStdString();
			break;
		default:
			shortcut_comment = "";
			break;
	}

#if defined(_WIN32)
	// Sanitize provided command line arguments and attach custom arguments afterward.
	for (std::string& argument : standard_arguments)
		ShortcutCreationDialog::EscapeCommandLineArgumentWindows(argument);

	if (!custom_arguments.empty())
		standard_arguments.push_back(custom_arguments);

	ShortcutCreationDialog::EscapeCommandLineArgumentWindows(m_game_path);

	const std::string combined_launch_arguments =
		fmt::format("{} -- {}", StringUtil::JoinString(standard_arguments.begin(), standard_arguments.end(), " "), m_game_path);

	const std::string shortcut_destination = is_desktop ? fmt::format("{}.lnk", m_game_title) : fmt::format("PCSX2\\{}.lnk", m_game_title);

	// Write to .lnk file.
	if (!FileSystem::CreateWin32IShellLink(shortcut_destination, combined_launch_arguments, shortcut_comment, is_desktop ? FOLDERID_Desktop : FOLDERID_Programs))
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Failed to create a shortcut. See the log for more information."),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
#else
	// Locate home directory.
	const char* home = std::getenv("HOME");
	if (!home)
	{
		Console.Error("Failed to create shortcut: Home path is empty.");
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Home path is empty."),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Create initial file path for shortcut (suggested to user).
	std::string shortcut_destination;
	const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
	if (is_desktop)
	{
		const char* xdg_desktop_dir = std::getenv("XDG_DESKTOP_DIR");
		if (xdg_desktop_dir)
			shortcut_destination = fmt::format("{}/{}.desktop", xdg_desktop_dir, m_game_title);
		else
			shortcut_destination = fmt::format("{}/Desktop/{}.desktop", home, m_game_title);
	}
	else
	{
		if (xdg_data_home)
			shortcut_destination = fmt::format("{}/applications/{}.desktop", xdg_data_home, m_game_title);
		else
			shortcut_destination = fmt::format("{}/.local/share/applications/{}.desktop", home, m_game_title);
	}

	// Locate executable path and, on AppImage, copy PCSX2 icon.
	std::string executable_path;
	if (std::getenv("container"))
		executable_path = "flatpak run net.pcsx2.PCSX2";
	else
	{
		executable_path = FileSystem::GetPackagePath();
		if (executable_path.empty())
		{
			Console.Error("Failed to create shortcut: Executable path is empty.");
			QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Executable path is empty."),
				QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			return;
		}
		ShortcutCreationDialog::EscapeCommandLineArgumentLinux(executable_path);

		std::string icon_destination_directory;
		if (xdg_data_home)
			icon_destination_directory = fmt::format("{}/icons/hicolor/512x512/apps/", xdg_data_home);
		else
			icon_destination_directory = fmt::format("{}/.local/share/icons/hicolor/512x512/apps/", home);

		const std::string icon_destination_path = fmt::format("{}/PCSX2.png", icon_destination_directory);
		if (FileSystem::EnsureDirectoryExists(icon_destination_directory.c_str(), true) && !FileSystem::FileExists(icon_destination_path.c_str()))
			FileSystem::CopyFilePath(Path::Combine(EmuFolders::Resources, "icons/AppIconLarge.png").c_str(), icon_destination_path.c_str(), false);
	}

	// Sanitize provided command line arguments and attach custom arguments afterward.
	bool lossless = true;
	for (std::string& arg : standard_arguments)
	{
		// Only print warning once for lossy escape.
		if (!ShortcutCreationDialog::EscapeCommandLineArgumentLinux(arg) && lossless)
		{
			Console.Warning("File path contains invalid character(s). The resulting shortcut may not work.");
			QMessageBox::warning(this, tr("Problem creating shortcut"),
				tr("File path contains invalid character(s). The resulting shortcut may not work."),
				QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
			lossless = false;
		}
	}

	if (!custom_arguments.empty())
		standard_arguments.push_back(custom_arguments);

	ShortcutCreationDialog::EscapeCommandLineArgumentLinux(m_game_path);

	// Prompt user for shortcut destination, starting at the "suggested" directory made earlier.
	shortcut_destination = QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Select Shortcut Save Destination"), QString::fromStdString(shortcut_destination),
														tr("Desktop Shortcut Files (*.desktop)")))
	                           .toStdString();

	const std::string shortcut_exec = fmt::format("{} {} -- {}", executable_path, StringUtil::JoinString(standard_arguments.begin(), standard_arguments.end(), " "), m_game_path);

	if (!FileSystem::CreateLinuxDesktopFile(shortcut_destination, shortcut_exec, m_game_title, shortcut_comment))
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Could not create .desktop file."),
			QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
	}
#endif
}

#if defined(_WIN32)
void ShortcutCreationDialog::EscapeCommandLineArgumentWindows(std::string& cli_argument)
{
	if (!cli_argument.empty() && cli_argument.find_first_of(" \t\n\v\"") == std::string::npos)
		return;

	std::string temp;
	temp.reserve(cli_argument.length() + 10);
	temp += '"';

	for (auto it = cli_argument.begin();; ++it)
	{
		int backslash_count = 0;
		while (it != cli_argument.end() && *it == '\\')
		{
			++it;
			++backslash_count;
		}

		if (it == cli_argument.end())
		{
			temp.append(backslash_count * 2, '\\');
			break;
		}

		if (*it == '"')
		{
			temp.append(backslash_count * 2 + 1, '\\');
			temp += '"';
		}
		else
		{
			temp.append(backslash_count, '\\');
			temp += *it;
		}
	}

	temp += '"';
	cli_argument = std::move(temp);
	return;
}
#else
bool ShortcutCreationDialog::EscapeCommandLineArgumentLinux(std::string& cli_argument)
{
	const char* carg = cli_argument.c_str();
	const char* cend = carg + cli_argument.size();
	constexpr const char* RESERVED_CHARS = " \t\n\\\"'\\\\><~|%&;$*?#()`"
										   "\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0d\x0e\x0f"
										   "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f";
	const char* next = carg + std::strcspn(carg, RESERVED_CHARS);

	if (next == cend)
		return true; // No escaping needed, don't modify

	bool lossless = true;
	std::string temp = "\"";
	constexpr const char* NOT_VALID_IN_QUOTE = "%`$\"\\\n"
											   "\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0d\x0e\x0f"
											   "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f";

	while (true)
	{
		next = carg + std::strcspn(carg, NOT_VALID_IN_QUOTE);
		temp.append(carg, next);
		carg = next;

		if (carg == cend)
			break;

		switch (*carg)
		{
			case '"':
			case '`':
				temp.push_back('\\');
				temp.push_back(*carg);
				break;
			case '\\':
				temp.append("\\\\\\\\");
				break;
			case '$':
				temp.push_back('\\');
				temp.push_back('\\');
				temp.push_back(*carg);
				break;
			case '%':
				temp.push_back('%');
				temp.push_back(*carg);
				break;
			default:
				temp.push_back(' ');
				lossless = false;
				break;
		}
		++carg;
	}

	temp.push_back('"');
	cli_argument = std::move(temp);
	return lossless;
}
#endif

void ShortcutCreationDialog::FillArgumentsList(std::vector<std::string>& arg_list)
{
	if (m_ui.portableModeToggle->isChecked())
		arg_list.push_back("-portable");

	if (m_ui.overrideBootELFToggle->isChecked() && !m_ui.overrideBootELFPath->text().isEmpty())
	{
		arg_list.push_back("-elf");
		arg_list.push_back(Path::ToNativePath(Path::RealPath(m_ui.overrideBootELFPath->text().toStdString())));
	}

	if (m_ui.gameArgsToggle->isChecked() && !m_ui.gameArgs->text().isEmpty())
	{
		arg_list.push_back("-gameargs");
		arg_list.push_back(m_ui.gameArgs->text().toStdString());
	}

	if (m_ui.bootOptionToggle->isChecked())
		arg_list.push_back(m_ui.bootOptionDropdown->currentIndex() ? "-slowboot" : "-fastboot");

	if (m_ui.loadStateIndexToggle->isChecked())
	{
		// Bounds are enforced by the UI, but check just in case.
		const s32 load_state_index = m_ui.loadStateIndex->value();
		if (load_state_index > 0 && load_state_index <= VMManager::NUM_SAVE_STATE_SLOTS)
		{
			m_save_state_type = StateType::Index;
			m_save_state_name = StringUtil::ToChars(load_state_index);
			arg_list.push_back("-state");
			arg_list.push_back(m_save_state_name);
		}
	}
	else if (m_ui.loadStateFileToggle->isChecked() && !m_ui.loadStateFilePath->text().isEmpty())
	{
		m_save_state_type = StateType::File;
		const std::string save_state_file_path = Path::ToNativePath(Path::RealPath(m_ui.loadStateFilePath->text().toStdString()));
		m_save_state_name = Path::GetFileName(save_state_file_path);
		arg_list.push_back("-statefile");
		arg_list.push_back(save_state_file_path);
	}

	if (m_ui.fullscreenMode->isChecked())
		arg_list.push_back(m_ui.fullscreenModeDropdown->currentIndex() ? "-nofullscreen" : "-fullscreen");

	if (m_ui.bigPictureModeToggle->isChecked())
		arg_list.push_back("-bigpicture");
}
