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
#include <shlobj.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <comdef.h>

#include <wrl/client.h>
#endif

ShortcutCreationDialog::ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path)
	: QDialog(parent)
	, m_title(title)
	, m_path(path)
{
	m_ui.setupUi(this);
	this->setWindowTitle(tr("Create Shortcut For %1").arg(title));
	this->setWindowIcon(QtHost::GetAppIcon());

#if defined(_WIN32)
	m_ui.shortcutStartMenu->setText(tr("Start Menu"));
#else
	m_ui.shortcutStartMenu->setText(tr("Application Launcher"));
#endif

	connect(m_ui.overrideBootELFButton, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select ELF File"), QString(), tr("ELF Files (*.elf);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.overrideBootELFPath->setText(Path::ToNativePath(path.toStdString()).c_str());
	});

	connect(m_ui.loadStateFileBrowse, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.loadStateFilePath->setText(Path::ToNativePath(path.toStdString()).c_str());
	});

	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFPath, &QLineEdit::setEnabled);
	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFButton, &QPushButton::setEnabled);
	connect(m_ui.gameArgsToggle, &QCheckBox::toggled, m_ui.gameArgs, &QLineEdit::setEnabled);
	connect(m_ui.loadStateIndexToggle, &QCheckBox::toggled, m_ui.loadStateIndex, &QSpinBox::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFilePath, &QLineEdit::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFileBrowse, &QPushButton::setEnabled);
	connect(m_ui.bootOptionToggle, &QCheckBox::toggled, m_ui.bootOptionDropdown, &QPushButton::setEnabled);
	connect(m_ui.fullscreenMode, &QCheckBox::toggled, m_ui.fullscreenModeDropdown, &QPushButton::setEnabled);

	m_ui.loadStateIndex->setMaximum(VMManager::NUM_SAVE_STATE_SLOTS);

	if (std::getenv("container"))
	{
		m_ui.portableModeToggle->setEnabled(false);
		m_ui.shortcutDesktop->setEnabled(false);
		m_ui.shortcutStartMenu->setEnabled(false);
	}

	connect(m_ui.dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(m_ui.dialogButtons, &QDialogButtonBox::accepted, this, [&]() {
		std::vector<std::string> args;

		if (m_ui.portableModeToggle->isChecked())
			args.push_back("-portable");

		if (m_ui.overrideBootELFToggle->isChecked() && !m_ui.overrideBootELFPath->text().isEmpty())
		{
			args.push_back("-elf");
			args.push_back(m_ui.overrideBootELFPath->text().toStdString());
		}

		if (m_ui.gameArgsToggle->isChecked() && !m_ui.gameArgs->text().isEmpty())
		{
			args.push_back("-gameargs");
			args.push_back(m_ui.gameArgs->text().toStdString());
		}

		if (m_ui.bootOptionToggle->isChecked())
			args.push_back(m_ui.bootOptionDropdown->currentIndex() ? "-slowboot" : "-fastboot");

		if (m_ui.loadStateIndexToggle->isChecked())
		{
			const s32 load_state_index = m_ui.loadStateIndex->value();
			if (load_state_index >= 1 && load_state_index <= VMManager::NUM_SAVE_STATE_SLOTS)
			{
				args.push_back("-state");
				args.push_back(StringUtil::ToChars(load_state_index));
			}
		}

		if (m_ui.loadStateFileToggle->isChecked() && !m_ui.loadStateFilePath->text().isEmpty())
		{
			args.push_back("-statefile");
			args.push_back(m_ui.loadStateFilePath->text().toStdString());
		}

		if (m_ui.fullscreenMode->isChecked())
			args.push_back(m_ui.fullscreenModeDropdown->currentIndex() ? "-nofullscreen" : "-fullscreen");

		if (m_ui.bigPictureModeToggle->isChecked())
			args.push_back("-bigpicture");

		std::string custom_args = m_ui.customArgsInput->text().toStdString();

		ShortcutCreationDialog::CreateShortcut(title.toStdString(), path.toStdString(), args, custom_args, m_ui.shortcutDesktop->isChecked());

		accept();
	});
}

void ShortcutCreationDialog::CreateShortcut(const std::string name, const std::string game_path, std::vector<std::string> passed_cli_args, std::string custom_args, bool is_desktop)
{
#if defined(_WIN32)
	if (name.empty())
	{
		Console.Error("Cannot create shortcuts without a name.");
		return;
	}

	// Sanitize filename
	const std::string clean_name = Path::SanitizeFileName(name).c_str();
	std::string clean_path = Path::ToNativePath(Path::RealPath(game_path)).c_str();
	if (!Path::IsValidFileName(clean_name))
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Filename contains illegal character."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Locate home directory
	std::string link_file;
	if (const char* home = std::getenv("USERPROFILE"))
	{
		if (is_desktop)
			link_file = Path::ToNativePath(fmt::format("{}/Desktop/{}.lnk", home, clean_name));
		else
		{
			const std::string start_menu_dir = Path::ToNativePath(fmt::format("{}/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/PCSX2", home));
			if (!FileSystem::EnsureDirectoryExists(start_menu_dir.c_str(), false))
			{
				QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Could not create start menu directory."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
				return;
			}

			link_file = Path::ToNativePath(fmt::format("{}/{}.lnk", start_menu_dir, clean_name));
		}
	}
	else
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Home path is empty."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Check if the same shortcut already exists
	if (FileSystem::FileExists(link_file.c_str()))
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("A shortcut with the same name already exists."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Shortcut CmdLine Args
	bool lossless = true;
	for (std::string& arg : passed_cli_args)
		lossless &= ShortcutCreationDialog::EscapeShortcutCommandLine(&arg);

	if (!lossless)
	{
		QMessageBox::warning(this, tr("Failed to create shortcut"), tr("File path contains invalid character(s)."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	ShortcutCreationDialog::EscapeShortcutCommandLine(&clean_path);
	std::string combined_args = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");
	std::string final_args = fmt::format("{} {} -- {}", combined_args, custom_args, clean_path);

	Console.WriteLnFmt("Creating a shortcut '{}' with arguments '{}'", link_file, final_args);

	const auto str_error = [](HRESULT hr) -> std::string {
		_com_error err(hr);
		const TCHAR* errMsg = err.ErrorMessage();
		return fmt::format("{} [{}]", StringUtil::WideStringToUTF8String(errMsg), hr);
	};

	// Construct the shortcut
	// https://stackoverflow.com/questions/3906974/how-to-programmatically-create-a-shortcut-using-win32
	HRESULT res = CoInitialize(NULL);
	if (FAILED(res))
	{
		Console.ErrorFmt("Failed to create shortcut: CoInitialize failed ({})", str_error(res));
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("CoInitialize failed (%1)").arg(str_error(res)), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	Microsoft::WRL::ComPtr<IShellLink> pShellLink;
	Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;

	const auto cleanup = [&](bool return_value, const QString& fail_reason) -> bool {
		if (!return_value)
		{
			Console.ErrorFmt("Failed to create shortcut: {}", fail_reason.toStdString());
			QMessageBox::critical(this, tr("Failed to create shortcut"), fail_reason, QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		}
		CoUninitialize();
		return return_value;
	};

	res = CoCreateInstance(__uuidof(ShellLink), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pShellLink));
	if (FAILED(res))
	{
		cleanup(false, tr("CoCreateInstance failed"));
		return;
	}

	// Set path to the executable
	const std::wstring target_file = StringUtil::UTF8StringToWideString(FileSystem::GetProgramPath());
	res = pShellLink->SetPath(target_file.c_str());
	if (FAILED(res))
	{
		cleanup(false, tr("SetPath failed (%1)").arg(str_error(res)));
		return;
	}

	// Set the working directory
	const std::wstring working_dir = StringUtil::UTF8StringToWideString(FileSystem::GetWorkingDirectory());
	res = pShellLink->SetWorkingDirectory(working_dir.c_str());
	if (FAILED(res))
	{
		cleanup(false, tr("SetWorkingDirectory failed (%1)").arg(str_error(res)));
		return;
	}

	// Set the launch arguments
	if (!final_args.empty())
	{
		const std::wstring target_cli_args = StringUtil::UTF8StringToWideString(final_args);
		res = pShellLink->SetArguments(target_cli_args.c_str());
		if (FAILED(res))
		{
			cleanup(false, tr("SetArguments failed (%1)").arg(str_error(res)));
			return;
		}
	}

	// Set the icon
	std::string icon_path = Path::ToNativePath(Path::Combine(Path::GetDirectory(FileSystem::GetProgramPath()), "resources/icons/AppIconLarge.ico"));
	const std::wstring w_icon_path = StringUtil::UTF8StringToWideString(icon_path);
	res = pShellLink->SetIconLocation(w_icon_path.c_str(), 0);
	if (FAILED(res))
	{
		cleanup(false, tr("SetIconLocation failed (%1)").arg(str_error(res)));
		return;
	}

	// Use the IPersistFile object to save the shell link
	res = pShellLink.As(&pPersistFile);
	if (FAILED(res))
	{
		cleanup(false, tr("QueryInterface failed (%1)").arg(str_error(res)));
		return;
	}

	// Save shortcut link to disk
	const std::wstring w_link_file = StringUtil::UTF8StringToWideString(link_file);
	res = pPersistFile->Save(w_link_file.c_str(), TRUE);
	if (FAILED(res))
	{
		cleanup(false, tr("Failed to save the shortcut (%1)").arg(str_error(res)));
		return;
	}

	cleanup(true, {});

#else

	if (name.empty())
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Cannot create a shortcut without a title."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	bool is_flatpak = (std::getenv("container"));

	// Sanitize filename and game path
	const std::string clean_name = Path::SanitizeFileName(name);
	std::string clean_path = Path::Canonicalize(Path::RealPath(game_path));
	if (!Path::IsValidFileName(clean_name))
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Filename contains illegal character."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Find the executable path
	std::string executable_path = FileSystem::GetPackagePath();
	if (executable_path.empty())
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Executable path is empty."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Find home directory
	std::string link_path;
	const char* home = std::getenv("HOME");
	const char* xdg_desktop_dir = std::getenv("XDG_DESKTOP_DIR");
	const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
	if (home)
	{
		if (is_desktop)
		{
			if (xdg_desktop_dir)
				link_path = fmt::format("{}/{}.desktop", xdg_desktop_dir, clean_name);
			else
				link_path = fmt::format("{}/Desktop/{}.desktop", home, clean_name);
		}
		else
		{
			if (xdg_data_home)
				link_path = fmt::format("{}/applications/{}.desktop", xdg_data_home, clean_name);
			else
				link_path = fmt::format("{}/.local/share/applications/{}.desktop", home, clean_name);
		}
	}
	else
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Path to the Home directory is empty."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	// Copy PCSX2 icon
	std::string icon_dest;
	if (xdg_data_home)
		icon_dest = fmt::format("{}/icons/hicolor/512x512/apps/", xdg_data_home);
	else
		icon_dest = fmt::format("{}/.local/share/icons/hicolor/512x512/apps/", home);

	std::string icon_name;
	if (is_flatpak) // Flatpak
	{
		executable_path = "flatpak run net.pcsx2.PCSX2";
		icon_name = "net.pcsx2.PCSX2";

	}
	else
	{
		icon_name = "PCSX2";
		std::string icon_path = fmt::format("{}/{}.png", icon_dest, icon_name).c_str();
		if (FileSystem::EnsureDirectoryExists(icon_dest.c_str(), true))
			FileSystem::CopyFilePath(Path::Combine(EmuFolders::Resources, "icons/AppIconLarge.png").c_str(), icon_path.c_str(), false);
	}

	// Shortcut CmdLine Args
	bool lossless = true;
	for (std::string& arg : passed_cli_args)
		lossless &= ShortcutCreationDialog::EscapeShortcutCommandLine(&arg);

	if (!lossless)
	{
		QMessageBox::warning(this, tr("Failed to create shortcut"), tr("File path contains invalid character(s)."), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	std::string cmdline = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");

	// Further string sanitization
	if (!is_flatpak)
		ShortcutCreationDialog::EscapeShortcutCommandLine(&executable_path);
	ShortcutCreationDialog::EscapeShortcutCommandLine(&clean_path);

	// Assembling the .desktop file
	std::string final_args;
	final_args = fmt::format("{} {} {} -- {}", executable_path, cmdline, custom_args, clean_path);
	std::string file_content =
		"[Desktop Entry]\n"
		"Encoding=UTF-8\n"
		"Version=1.0\n"
		"Type=Application\n"
		"Terminal=false\n"
		"StartupWMClass=PCSX2\n"
		"Exec=" + final_args + "\n"
		"Name=" + clean_name + "\n"
		"Icon=" + icon_name + "\n"
		"Categories=Game;Emulator;\n";
	std::string_view sv(file_content);

	// Prompt user for shortcut saving destination
	QString final_path(QStringLiteral("%1").arg(QString::fromStdString(link_path)));
	const QString filter(tr("Desktop Shortcut Files (*.desktop)"));

	final_path = QDir::toNativeSeparators(QFileDialog::getSaveFileName(this, tr("Select Shortcut Save Destination"), final_path, filter));

	if (final_path.isEmpty())
		return;

	// Write to .desktop file
	if (!FileSystem::WriteStringToFile(final_path.toStdString().c_str(), sv))
	{
		QMessageBox::critical(this, tr("Failed to create shortcut"), tr("Failed to create .desktop file"), QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
		return;
	}

	if (chmod(final_path.toStdString().c_str(), S_IRWXU) != 0) // enables user to execute file
		Console.ErrorFmt("Failed to change file permissions for .desktop file: {} ({})", strerror(errno), errno);
#endif
}

bool ShortcutCreationDialog::EscapeShortcutCommandLine(std::string* arg)
{
#ifdef _WIN32
	if (!arg->empty() && arg->find_first_of(" \t\n\v\"") == std::string::npos)
		return true;

	std::string temp;
	temp.reserve(arg->length() + 10);
	temp += '"';

	for (auto it = arg->begin();; ++it)
	{
		int backslash_count = 0;
		while (it != arg->end() && *it == '\\')
		{
			++it;
			++backslash_count;
		}

		if (it == arg->end())
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
	*arg = std::move(temp);
	return true;
#else
	const char* carg = arg->c_str();
	const char* cend = carg + arg->size();
	const char* RESERVED_CHARS = " \t\n\\\"'\\\\><~|%&;$*?#()`"
								 "\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0d\x0e\x0f"
								 "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f";
	const char* next = carg + std::strcspn(carg, RESERVED_CHARS);

	if (next == cend)
		return true; // No escaping needed, don't modify

	bool lossless = true;
	std::string temp = "\"";
	const char* NOT_VALID_IN_QUOTE = "%`$\"\\\n"
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
	*arg = std::move(temp);
	return lossless;
#endif
}
