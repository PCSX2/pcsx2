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
#include "MainFrame.h"
#include "common/PathUtils.h"
#include "Dialogs/ModalPopups.h"
#include "yaml-cpp/yaml.h"
#include "AppConfig.h"
#include <wx/stdpaths.h>
#include "PathDefs.h"

#ifdef __WXMSW__
#include "wx/msw/regconf.h"
#endif

DocsModeType			DocsFolderMode = DocsFolder_User;
bool					UseDefaultSettingsFolder = true;
bool 					runWizard  = true; // This should default to true unless the stream says otherwise. If this doesn't get flipped somethings wrong and setup required

std::vector<std::string> ErrorFolders;

fs::path usermodePath;

fs::path CustomDocumentsFolder;
fs::path SettingsFolder;
fs::path InstallFolder;

YAML::Node stream;

const std::string PermissionFolders[] =
{
		PathDefs::Base::Settings(),
		PathDefs::Base::MemoryCards(),
		PathDefs::Base::Savestates(),
		PathDefs::Base::Snapshots(),
		PathDefs::Base::Logs(),
		PathDefs::Base::CheatsWS(),
#ifdef PCSX2_DEVBUILD
		PathDefs::Base::Dumps(),
#endif 
};

// The UserLocalData folder can be redefined depending on whether or not PCSX2 is in
// "portable install" mode or not.  when PCSX2 has been configured for portable install, the
// UserLocalData folder is the current working directory.
InstallationModeType			InstallationMode;

static fs::path GetPortableYamlFilePath()
{
	return Path::GetExecutableDirectory() / "portable.yaml";
}

static wxString GetMsg_PortableModeRights()
{
	return pxE( L"Please ensure that these folders are created and that your user account is granted write permissions to them -- or re-run PCSX2 with elevated (administrator) rights, which should grant PCSX2 the ability to create the necessary folders itself.  If you do not have elevated rights on this computer, then you will need to switch to User Documents mode (click button below).");
};

bool Pcsx2App::TestUserPermissionsRights(const fs::path& testFolder)
{
	std::string createme, accessme;

	for (int i = 0; i < 5; ++i)
	{
		fs::path folder = (testFolder / PermissionFolders[i]);

		if (!fs::exists(folder))
			if (!fs::create_directories(folder))
				ErrorFolders.push_back(folder);
	}

	for (int i = 0; i < ErrorFolders.size(); i++)
	{
		Console.WriteLn((wxString)_("The following folder is missing and cannot be created:") + L"\n" + ErrorFolders[i]);
	}

	if (ErrorFolders.empty())
		return true;
	else
		return false;
}

static void DoFirstTimeWizard()
{
	// first time startup, so give the user the choice of user mode:
	while (true)
	{
		// PCSX2's FTWizard allows improptu restarting of the wizard without cancellation.
		// This is typically used to change the user's language selection.

		FirstTimeWizard wiz(NULL);
		if (wiz.RunWizard(wiz.GetFirstPage()))
			break;
		if (wiz.GetReturnCode() != pxID_RestartWizard)
			throw Exception::StartupAborted(L"User canceled FirstTime Wizard.");

		Console.WriteLn(Color_StrongBlack, "Restarting First Time Wizard!");
	}
}

// Portable installations are assumed to be run in either administrator rights mode, or run
// from "insecure media" such as a removable flash drive.  In these cases, the default path for
// PCSX2 user documents becomes ".", which is the current working directory.
//
// Portable installation mode is typically enabled via the presence of a portable file in the
// same directory that PCSX2 is installed to.
//
bool Pcsx2App::TestForPortableInstall()
{
	InstallationMode = InstallMode_Portable;

	fs::path portableYamlFile = GetPortableYamlFilePath();
	fs::path portableDocsFolder = portableYamlFile.parent_path();

	bool isPortable = Load(portableYamlFile);

	if (isPortable)
	{
		if (Startup.PortableMode)
			Console.WriteLn(L"(UserMode) Portable mode requested via commandline switch!");
		else
		{
			wxString temp = Path::ToWxString(portableYamlFile);
			Console.WriteLn(L"(UserMode) Found portable install yaml @ %s", WX_STR(temp));
		}
		// Just because the portable yaml file exists doesn't mean we can actually run in portable
		// mode.  In order to determine our read/write permissions to the PCSX2, we must try to
		// modify the configured documents folder, and catch any ensuing error.

		if (!TestUserPermissionsRights(portableDocsFolder))
		{
			Console.WriteLn(L"Install Mode Activated");
			return false;
		}

		// Success -- all user-based folders have write access.  PCSX2 should be able to run error-free!
		try
		{
			runWizard = stream["RunWizard"].as<bool>();
		}
		catch (const YAML::Exception& e)
		{
			// If the portable.yaml file gets into a bad state, we clear it and reset it
			DevCon.Error("Invalid portable.yaml file, resetting... ");
			runWizard = true;
			stream = YAML::Node();
			stream["RunWizard"] = runWizard;
			// Save it immediately incase the Wizard is exited early, etc.
			Save(GetPortableYamlFilePath());
		}

		// Force-set the custom documents mode
		InstallationMode = InstallMode_Portable;
		DocsFolderMode = DocsFolder_Custom;
		CustomDocumentsFolder = portableDocsFolder;

	  // Run the First Time Wizard!
		// ----------------------------
		// Wizard is only run once.  The status of the wizard having been run is stored in
		// the installation yaml file, which can be either the portable install (useful for admins)
		// or the registry/user local documents position.		
		if (runWizard)
		{
			DoFirstTimeWizard();	
			stream["RunWizard"] = false;	
			Save(GetPortableYamlFilePath()); // Save Portable Yaml
			
			// Save user's new general settings
			AppConfig_OnChangedSettingsFolder(true);
			AppSaveSettings();
		}
		
		return isPortable;
	}
	else
		return false;
}
// Reset RunWizard so the FTWizard is run again on next PCSX2 start.
void Pcsx2App::WipeUserModeSettings()
{
	if (InstallationMode == InstallMode_Portable)
	{
		// Remove the portable file entry "RunWizard" conforming to this instance of PCSX2.
		fs::path portableYamlFile(GetPortableYamlFilePath());
		stream["RunWizard"] = true;
		Save(portableYamlFile);
	}
	else
	{
		// Remove the registry entry "RunWizard" conforming to this instance of PCSX2.
		bool conf_install = OpenInstallSettingsFile();
		stream["RunWizard"] = true;
		Save(usermodePath);
	}
}

std::ifstream Pcsx2App::getFileAsInputStream(const fs::path& file)
{
#ifdef _WIN32
	return std::ifstream(file.wstring());
#else
	return std::ifstream(file);
#endif
}

std::ofstream Pcsx2App::getFileAsOutputStream(const fs::path& file)
{
#ifdef _WIN32
	return std::ofstream(file.wstring());
#else
	return std::ofstream(file);
#endif
}

bool Pcsx2App::Load(const fs::path& fileName)
{
	if (fs::exists(fileName))
	{
		try
		{
			std::ifstream in = getFileAsInputStream(fileName);
			stream = YAML::Load(in);
			in.close();
			return true;
		}
		catch (const std::exception& e)
		{
			DevCon.Error("Could not load '%s'. Error: %s", WX_STR(Path::ToWxString(fileName)), e.what());
			return false;
		}
	}
	else
	{
		return false;
	}
}

void Pcsx2App::Save(const fs::path& fileName)
{
	if (!fs::exists(fileName.parent_path().make_preferred()))
	{
		fs::create_directories(fileName.parent_path().make_preferred());
	}

	if (stream)
	{
		try
		{
			std::ofstream out = getFileAsOutputStream(fileName);
			out << stream;
			out.close();
		}
		catch (const std::exception& e)
		{
			DevCon.Warning("ERROR: ", e.what());
		}
	}
}

bool Pcsx2App::OpenInstallSettingsFile()
{
	// Implementation Notes:
	//
	// As of 0.9.8 and beyond, PCSX2's versioning should be strong enough to base yaml and
	// plugin compatibility on version information alone.  This in turn allows us to ditch
	// the old system (CWD-based yaml file mess) in favor of a system that simply stores
	// most core application-level settings in the registry.

	InstallationMode = InstallationModeType::InstallMode_Registered;
	fs::path usrlocaldir = PathDefs::GetUserLocalDataDir();

	InstallFolder = Path::FromWxString((wxFileName(wxStandardPaths::Get().GetExecutablePath())).GetPath());

	fs::path usermodeFile = (Path::FromWxString(GetAppName()).concat("-reg.yaml"));
	usermodePath = (usrlocaldir / usermodeFile); 

	if (!fs::exists(usermodePath))
	{
		CustomDocumentsFolder = PathDefs::AppRoot();

		// Install Mode, genereate a new stream and manually write data to new file if it doesn't.
		stream["DocumentsFolderMode"] = (int)DocsFolderMode;
		stream["CustomDocumentsFolder"] = CustomDocumentsFolder.string();
		stream["UseDefaultSettingsFolder"] = UseDefaultSettingsFolder;
		stream["SettingsFolder"] = SettingsFolder.string();
		stream["Install_Dir"] = InstallFolder.string();
		stream["RunWizard"] = false;
		Save(usermodePath);
		DoFirstTimeWizard();
		// Save user's new settings
		AppConfig_OnChangedSettingsFolder(true);
		AppSaveSettings();
	}
	else
	{
		Load(usermodePath);
		runWizard = stream["RunWizard"].as<bool>();
		if (stream["RunWizard"].as<bool>())
		{
			DoFirstTimeWizard(); // covering the case the file was edited to true manually
			stream["RunWizard"] = false;
			Save(usermodePath);
		}

		DocsFolderMode = (DocsModeType)stream["DocumentsFolderMode"].as<int>();
		CustomDocumentsFolder = stream["CustomDocumentsFolder"].as<std::string>();
		UseDefaultSettingsFolder = stream["UseDefaultSettingsFolder"].as<bool>();
		SettingsFolder = stream["SettingsFolder"].as<std::string>();
		InstallFolder = stream["Install_Dir"].as<std::string>();
	}
	return true;
}

void Pcsx2App::ForceFirstTimeWizardOnNextRun()
{
	bool conf_install = TestForPortableInstall();

	if (!conf_install)
		conf_install = OpenInstallSettingsFile();

	stream["RunWizard"] = true;
}

void Pcsx2App::EstablishAppUserMode()
{
	// TODO - stop mutating the yaml directly, serialize and deserialize!

	bool conf_install = TestForPortableInstall();

	if (!conf_install)
		conf_install = OpenInstallSettingsFile();

	if (!runWizard)
	{
		//No first time wizard. No save
		AppConfig_OnChangedSettingsFolder(false);
		return;
	}
	return;
}
