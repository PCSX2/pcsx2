// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ShortcutCreationDialog.h"
#include "QtUtils.h"
#include "QtHost.h"
#include <fmt/format.h>
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "pcsx2/Host.h"

#if defined(__WIN32__) 
#include <Windows.h>
#include <shlobj.h>
#include <winnls.h>
#include <shobjidl.h>
#include <objbase.h>
#include <objidl.h>
#include <shlguid.h>
#include <comdef.h>

#include <wrl/client.h>
#endif

#include <QtWidgets/QFileDialog>
#include <qline.h>
#include <qlineedit.h>
#include <qpushbutton.h>


ShortcutCreationDialog::ShortcutCreationDialog(QWidget* parent, const QString& title, const QString& path)
	: QDialog(parent)
	, m_title(title)
	, m_path(path)
{
	m_ui.setupUi(this);
	this->setWindowTitle(tr("Creating Shortcut For %1").arg(title));
	this->setWindowIcon(QtHost::GetAppIcon());

#if defined(__WIN32__) 
	m_ui.shortcutStartMenu->setText(tr("Start Menu"));
#else
	m_ui.shortcutStartMenu->setText(tr("Application Launcher"));
#endif

	connect(m_ui.overrideBootELFButton, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select ELF File"), QString(), tr("ELF Files (*.elf);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.overrideBootELFPath->setText(path);
	});

	connect(m_ui.loadStateFileBrowse, &QPushButton::clicked, [&]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Select Save State File"), QString(), tr("Save States (*.p2s);;All Files (*.*)"));
		if (!path.isEmpty())
			m_ui.loadStateFileToggle->setText(path);
	});

	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFPath, &QLineEdit::setEnabled);
	connect(m_ui.overrideBootELFToggle, &QCheckBox::toggled, m_ui.overrideBootELFButton, &QPushButton::setEnabled);
	connect(m_ui.gameArgsToggle, &QCheckBox::toggled, m_ui.gameArgs, &QLineEdit::setEnabled);
	connect(m_ui.loadStateIndexToggle, &QCheckBox::toggled, m_ui.loadStateIndex, &QSpinBox::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFilePath, &QLineEdit::setEnabled);
	connect(m_ui.loadStateFileToggle, &QCheckBox::toggled, m_ui.loadStateFileBrowse, &QPushButton::setEnabled);
	connect(m_ui.bootOptionToggle, &QCheckBox::toggled, m_ui.bootOptionDropdown, &QPushButton::setEnabled);
	connect(m_ui.fullscreenMode, &QCheckBox::toggled, m_ui.fullscreenModeDropdown, &QPushButton::setEnabled);

	m_ui.shortcutDesktop->setChecked(true);
	m_ui.overrideBootELFPath->setEnabled(false);
	m_ui.overrideBootELFButton->setEnabled(false);
	m_ui.gameArgs->setEnabled(false);
	m_ui.bootOptionDropdown->setEnabled(false);
	m_ui.fullscreenModeDropdown->setEnabled(false);
	m_ui.loadStateIndex->setEnabled(false);
	m_ui.loadStateFileBrowse->setEnabled(false);
	m_ui.loadStateFilePath->setEnabled(false);

	m_ui.customArgsInstruction->setText(tr("You may add additional (space-separated) <a href=\"https://pcsx2.net/docs/post/cli/\">custom arguments</a> that are not listed above here:"));
	m_ui.customArgsInstruction->setTextFormat(Qt::RichText);
	m_ui.customArgsInstruction->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_ui.customArgsInstruction->setOpenExternalLinks(true);

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

		if (m_ui.bootOptionToggle->isChecked() && m_ui.bootOptionDropdown->currentIndex() == 0)
			args.push_back("-fastboot");
		else if (m_ui.bootOptionToggle->isChecked() && m_ui.bootOptionDropdown->currentIndex() == 1)
			args.push_back("-slowboot");

		if (m_ui.loadStateIndexToggle->isChecked())
		{
			args.push_back("-state");
			args.push_back(StringUtil::ToChars(m_ui.loadStateIndex->value()));
		}
		else if (m_ui.loadStateIndexToggle->isChecked() && !m_ui.loadStateFilePath->text().isEmpty())
		{
			args.push_back("-statefile");
			args.push_back(m_ui.loadStateFilePath->text().toStdString());
		}

		if (m_ui.fullscreenMode->isChecked() && m_ui.fullscreenModeDropdown->currentIndex() == 0)
			args.push_back("-fullscreen");
		else if (m_ui.fullscreenMode->isChecked() && m_ui.fullscreenModeDropdown->currentIndex() == 1)
			args.push_back("-nofullscreen");

		if (m_ui.bigPictureModeToggle->isChecked())
			args.push_back("-bigpicture");

		if (!m_ui.customArgsInput->text().isEmpty())
			args.push_back(m_ui.customArgsInput->text().toStdString());

		if (m_ui.shortcutDesktop->isChecked())
			m_desktop = true;
		else if (m_ui.shortcutStartMenu->isChecked())
			m_desktop = false;

		ShortcutCreationDialog::CreateShortcut(title.toStdString(), path.toStdString(), args, m_desktop);
		accept();
	});
	connect(m_ui.dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ShortcutCreationDialog::CreateShortcut(const std::string name, const std::string game_path, std::vector<std::string> passed_cli_args, bool is_desktop)
{
#if defined(__WIN32__) 
	if (name.empty())
	{
		Console.Error("Cannot create shortcuts without a name.");
		return;
	}

	// Sanitize filename
	const std::string clean_name = Path::SanitizeFileName(name).c_str();
	const std::string clean_path = Path::ToNativePath(Path::RealPath(game_path)).c_str();
	if (!Path::IsValidFileName(clean_name))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Filename contains illegal character."));
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
				Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Could not create start menu directory."));
				return;
			}

			link_file = Path::ToNativePath(fmt::format("{}/{}.lnk", start_menu_dir, clean_name));
		}
	}
	else
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "Home path is empty."));
		return;
	}

	// Check if the same shortcut already exists
	if (FileSystem::FileExists(link_file.c_str()))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), TRANSLATE_SV("WinMisc", "A shortcut with the same name already exist."));
		return;
	}

	// Shortcut CmdLine Args
	bool lossless = true;
	for (std::string& arg : passed_cli_args)
		lossless &= Path::EscapeCmdLine(&arg);

	if (!lossless)
		Host::ReportWarningAsync(TRANSLATE_SV("WinMisc", "Warning"), TRANSLATE_SV("WinMisc", "File path contains invalid character(s). The resulting shortcut may not work."));

	std::string combined_args = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");
	std::string final_args = fmt::format("{} -- {}", combined_args, clean_path);

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
		Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), str_error(res));
		return;
	}

	Microsoft::WRL::ComPtr<IShellLink> pShellLink;
	Microsoft::WRL::ComPtr<IPersistFile> pPersistFile;

	const auto cleanup = [&](bool return_value, const std::string& fail_reason) -> bool {
		if (!return_value)
		{
			Console.ErrorFmt("Failed to create shortcut: {}", fail_reason);
			Host::ReportErrorAsync(TRANSLATE_SV("WinMisc", "Failed to create shortcut"), fail_reason);
		}
		CoUninitialize();
		return return_value;
	};

	res = CoCreateInstance(__uuidof(ShellLink), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pShellLink));
	if (FAILED(res))
	{
		cleanup(false, "CoCreateInstance failed");
		return;
	}

	// Set path to the executable
	const std::wstring target_file = StringUtil::UTF8StringToWideString(FileSystem::GetProgramPath());
	res = pShellLink->SetPath(target_file.c_str());
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetPath failed ({})", str_error(res)));
		return;
	}

	// Set the working directory
	const std::wstring working_dir = StringUtil::UTF8StringToWideString(FileSystem::GetWorkingDirectory());
	res = pShellLink->SetWorkingDirectory(working_dir.c_str());
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetWorkingDirectory failed ({})", str_error(res)));
		return;
	}

	// Set the launch arguments
	if (!final_args.empty())
	{
		const std::wstring target_cli_args = StringUtil::UTF8StringToWideString(final_args);
		res = pShellLink->SetArguments(target_cli_args.c_str());
		if (FAILED(res))
		{
			cleanup(false, fmt::format("SetArguments failed ({})", str_error(res)));
			return;
		}
	}

	// Set the icon
	std::string icon_path = Path::ToNativePath(Path::Combine(Path::GetDirectory(FileSystem::GetProgramPath()), "resources/icons/AppIconLarge.ico"));
	const std::wstring w_icon_path = StringUtil::UTF8StringToWideString(icon_path);
	res = pShellLink->SetIconLocation(w_icon_path.c_str(), 0);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("SetIconLocation failed ({})", str_error(res)));
		return;
	}

	// Use the IPersistFile object to save the shell link
	res = pShellLink.As(&pPersistFile);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("QueryInterface failed ({})", str_error(res)));
		return;
	}

	// Save shortcut link to disk
	const std::wstring w_link_file = StringUtil::UTF8StringToWideString(link_file);
	res = pPersistFile->Save(w_link_file.c_str(), TRUE);
	if (FAILED(res))
	{
		cleanup(false, fmt::format("Failed to save the shortcut ({})", str_error(res)));
		return;
	}

	Console.WriteLnFmt(Color_StrongGreen, "{} shortcut for {} has been created succesfully.", is_desktop ? "Desktop" : "Start Menu", clean_name);
	cleanup(true, {});

#elif !defined(__APPLE__)

	if (std::getenv("container"))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "Game shortcut creation are not supported when running under Flatpak due to insufficient permission."));
		return;
	}

	if (name.empty())
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "Cannot create shortcut without a name."));
		return;
	}

	// Sanitize filename and game path
	const std::string clean_name = Path::SanitizeFileName(name);
	const std::string clean_path = Path::Canonicalize(Path::RealPath(game_path));
	if (!Path::IsValidFileName(clean_name))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "Filename contains illegal character."));
		return;
	}

	// Find the executable path
	const std::string executable_path = FileSystem::GetPackagePath();
	if (executable_path.empty())
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "Executable path is empty."));
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
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "Home path is empty."));
		return;
	}

	// Checks if a shortcut already exist
	if (FileSystem::FileExists(link_path.c_str()))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Failed to create shortcut"), TRANSLATE_SV("LnxMisc", "A shortcut with the same name already exist."));
		return;
	}

	// Shortcut CmdLine Args
	bool lossless = true;
	for (std::string& arg : passed_cli_args)
		lossless &= Path::EscapeCmdLine(&arg);

	if (!lossless)
		Host::ReportWarningAsync(TRANSLATE_SV("LnxMisc", "Warning"), TRANSLATE_SV("LnxMisc", "File path contains invalid character(s). The resulting shortcut may not work."));

	std::string cmdline = StringUtil::JoinString(passed_cli_args.begin(), passed_cli_args.end(), " ");

	// Copy PCSX2 icon
	std::string icon_dest;
	if (xdg_data_home)
		icon_dest = fmt::format("{}/icons/hicolor/scalable/apps/", xdg_data_home);
	else
		icon_dest = fmt::format("{}/.local/share/icons/hicolor/scalable/apps/", home);

	std::string icon_name = "PCSX2.svg";
	std::string icon_path = fmt::format("{}/{}", icon_dest, icon_name).c_str();
	if (FileSystem::EnsureDirectoryExists(icon_dest.c_str(), true))
		FileSystem::CopyFilePath(Path::Combine(EmuFolders::Resources, "icons/PCSX2.svg").c_str(), icon_path.c_str(), false);

	std::string final_args;
	final_args = fmt::format("{} {} -- {}", executable_path, cmdline, clean_path);
	std::string file_content =
		"[Desktop Entry]\n"
		"Encoding=UTF-8\n"
		"Version=1.0\n"
		"Type=Application\n"
		"Terminal=false\n"
		"StartupWMClass=PCSX2\n"
		"Exec=" + final_args + "\n"
		"Name=" + clean_name + "\n"
		"Icon=PCSX2\n"
		"Categories=Game;Emulator;\n";
	std::string_view sv(file_content);

	// Write to .desktop file
	if (!FileSystem::WriteStringToFile(link_path.c_str(), sv))
	{
		Host::ReportErrorAsync(TRANSLATE_SV("LnxMisc", "Error"), TRANSLATE_SV("LnxMisc", "Failed to create .desktop file"));
		return;
	}

	Console.WriteLnFmt(Color_StrongGreen, "{} shortcut for {} has been created succesfully.", is_desktop ? "Desktop" : "Start Menu", clean_name);

	if (chmod(link_path.c_str(), S_IRWXU) != 0) // enables user to execute file
		Console.ErrorFmt("Failed to change file permissions for .desktop file: {} ({})", strerror(errno), errno);
#endif
}