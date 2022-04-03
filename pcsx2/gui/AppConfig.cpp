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
#include "App.h"
#include "MainFrame.h"
#include "IniInterface.h"
#include "common/FileSystem.h"
#include "common/SettingsWrapper.h"
#include "wxSettingsInterface.h"

#include <wx/stdpaths.h>
#include "DebugTools/Debug.h"
#include <memory>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////////////////////
// PathDefs Namespace -- contains default values for various pcsx2 path names and locations.
//
// Note: The members of this namespace are intended for default value initialization only.
// Most of the time you should use the path folder assignments in Conf() instead, since those
// are user-configurable.
//
namespace PathDefs
{
	namespace Base
	{
		const wxDirName& Snapshots()
		{
			static const wxDirName retval(L"snaps");
			return retval;
		}

		const wxDirName& Savestates()
		{
			static const wxDirName retval(L"sstates");
			return retval;
		}

		const wxDirName& MemoryCards()
		{
			static const wxDirName retval(L"memcards");
			return retval;
		}

		const wxDirName& Settings()
		{
			static const wxDirName retval(L"inis");
			return retval;
		}

		const wxDirName& Logs()
		{
			static const wxDirName retval(L"logs");
			return retval;
		}

		const wxDirName& Bios()
		{
			static const wxDirName retval(L"bios");
			return retval;
		}

		const wxDirName& Cheats()
		{
			static const wxDirName retval(L"cheats");
			return retval;
		}

		const wxDirName& CheatsWS()
		{
			static const wxDirName retval(L"cheats_ws");
			return retval;
		}

		const wxDirName& Langs()
		{
			static const wxDirName retval(L"locale");
			return retval;
		}

		const wxDirName& Dumps()
		{
			static const wxDirName retval(L"dumps");
			return retval;
		}

		const wxDirName& Docs()
		{
			static const wxDirName retval(L"docs");
			return retval;
		}

		const wxDirName& Resources()
		{
			static const wxDirName retval(L"resources");
			return retval;
		}

		const wxDirName& Cache()
		{
			static const wxDirName retval(L"cache");
			return retval;
		}

		const wxDirName& Textures()
		{
			static const wxDirName retval(L"textures");
			return retval;
		}
	};

	// Specifies the root folder for the application install.
	// (currently it's the CWD, but in the future I intend to move all binaries to a "bin"
	// sub folder, in which case the approot will become "..") [- Air?]

	//The installer installs the folders which are relative to AppRoot (that's langs)
	//  relative to the exe folder, and not relative to cwd. So the exe should be default AppRoot. - avih
	const wxDirName& AppRoot()
	{
		//AffinityAssert_AllowFrom_MainUI();
		/*
		if (InstallationMode == InstallMode_Registered)
		{
			static const wxDirName cwdCache( (wxDirName)Path::Normalize(wxGetCwd()) );
			return cwdCache;
		}
		else if (InstallationMode == InstallMode_Portable)
*/
		if (InstallationMode == InstallMode_Registered || InstallationMode == InstallMode_Portable)
		{
			static const wxDirName appCache((wxDirName)
												wxFileName(wxStandardPaths::Get().GetExecutablePath())
													.GetPath());
			return appCache;
		}
		else
			pxFail("Unimplemented user local folder mode encountered.");

		static const wxDirName dotFail(L".");
		return dotFail;
	}

	// Specifies the main configuration folder.
	wxDirName GetUserLocalDataDir()
	{
		return wxDirName(wxStandardPaths::Get().GetUserLocalDataDir());
	}

	// Fetches the path location for user-consumable documents -- stuff users are likely to want to
	// share with other programs: screenshots, memory cards, and savestates.
	wxDirName GetDocuments(DocsModeType mode)
	{
		switch (mode)
		{
#if defined(XDG_STD) || defined(__APPLE__) // Expected location for this kind of stuff on macOS
			// Move all user data file into central configuration directory (XDG_CONFIG_DIR)
			case DocsFolder_User:
				return GetUserLocalDataDir();
#else
			case DocsFolder_User:
				return (wxDirName)Path::CombineWx(wxStandardPaths::Get().GetDocumentsDir(), pxGetAppName());
#endif
			case DocsFolder_Custom:
				return CustomDocumentsFolder;

				jNO_DEFAULT
		}

		return wxDirName();
	}

	wxDirName GetDocuments()
	{
		return GetDocuments(DocsFolderMode);
	}

	wxDirName GetProgramDataDir()
	{
#ifdef __APPLE__
		return wxDirName(wxStandardPaths::Get().GetResourcesDir());
#elif !defined(PCSX2_APP_DATADIR)
		return AppRoot();
#else
		// Each linux distributions have his rules for path so we give them the possibility to
		// change it with compilation flags. -- Gregory
		return wxDirName(PCSX2_APP_DATADIR).MakeAbsolute(AppRoot().ToString());
#endif
	}

	wxDirName GetSnapshots()
	{
		return GetDocuments() + Base::Snapshots();
	}

	wxDirName GetBios()
	{
		return GetDocuments() + Base::Bios();
	}

	wxDirName GetCheats()
	{
		return GetDocuments() + Base::Cheats();
	}

	wxDirName GetCheatsWS()
	{
		return GetDocuments() + Base::CheatsWS();
	}

	wxDirName GetDocs()
	{
#if !defined(PCSX2_APP_DOCDIR)
		return AppRoot() + Base::Docs();
#else
		return wxDirName(PCSX2_APP_DOCDIR).MakeAbsolute(AppRoot().ToString());
#endif
	}

	wxDirName GetSavestates()
	{
		return GetDocuments() + Base::Savestates();
	}

	wxDirName GetMemoryCards()
	{
		return GetDocuments() + Base::MemoryCards();
	}

	wxDirName GetSettings()
	{
		return GetDocuments() + Base::Settings();
	}

	wxDirName GetLogs()
	{
		return GetDocuments() + Base::Logs();
	}

	wxDirName GetResources()
	{
		// ifdef is only needed here because mac doesn't put its resources in a subdirectory..
#ifdef __APPLE__
		return wxDirName(wxStandardPaths::Get().GetResourcesDir());
#else
		return GetProgramDataDir() + Base::Resources();
#endif
	}

	wxDirName GetLangs()
	{
		return GetResources() + Base::Langs();
	}

	wxDirName GetCache()
	{
		return GetDocuments() + Base::Cache();
	}

	wxDirName GetTextures()
	{
		return GetDocuments() + Base::Textures();
	}

	wxDirName Get(FoldersEnum_t folderidx)
	{
		switch (folderidx)
		{
			case FolderId_Settings:
				return GetSettings();
			case FolderId_Bios:
				return GetBios();
			case FolderId_Snapshots:
				return GetSnapshots();
			case FolderId_Savestates:
				return GetSavestates();
			case FolderId_MemoryCards:
				return GetMemoryCards();
			case FolderId_Logs:
				return GetLogs();
			case FolderId_Langs:
				return GetLangs();
			case FolderId_Cheats:
				return GetCheats();
			case FolderId_CheatsWS:
				return GetCheatsWS();
			case FolderId_Cache:
				return GetCache();
			case FolderId_Textures:
				return GetTextures();

			case FolderId_Documents:
				return CustomDocumentsFolder;

				jNO_DEFAULT
		}
		return wxDirName();
	}
}; // namespace PathDefs


// --------------------------------------------------------------------------------------
//  Default Filenames
// --------------------------------------------------------------------------------------
namespace FilenameDefs
{
	wxFileName GetUiConfig()
	{
		return pxGetAppName() + L"_ui.ini";
	}

	wxFileName GetUiKeysConfig()
	{
		return pxGetAppName() + L"_keys.ini";
	}

	wxFileName GetVmConfig()
	{
		return pxGetAppName() + L"_vm.ini";
	}

	wxFileName GetUsermodeConfig()
	{
		return wxFileName(L"usermode.ini");
	}

	const wxFileName& Memcard(uint port, uint slot)
	{
		static const wxFileName retval[2][4] =
			{
				{
					wxFileName(L"Mcd001.ps2"),
					wxFileName(L"Mcd003.ps2"),
					wxFileName(L"Mcd005.ps2"),
					wxFileName(L"Mcd007.ps2"),
				},
				{
					wxFileName(L"Mcd002.ps2"),
					wxFileName(L"Mcd004.ps2"),
					wxFileName(L"Mcd006.ps2"),
					wxFileName(L"Mcd008.ps2"),
				}};

		pxAssert(port < 2);
		pxAssert(slot < 4);

		return retval[port][slot];
	}
}; // namespace FilenameDefs

static wxDirName GetResolvedFolder(FoldersEnum_t id)
{
	return g_Conf->Folders.IsDefault(id) ? PathDefs::Get(id) : g_Conf->Folders[id];
}

static wxDirName GetSettingsFolder()
{
	if (wxGetApp().Overrides.SettingsFolder.IsOk())
		return wxGetApp().Overrides.SettingsFolder;

	return UseDefaultSettingsFolder ? PathDefs::GetSettings() : SettingsFolder;
}

wxString GetVmSettingsFilename()
{
	wxFileName fname(wxGetApp().Overrides.VmSettingsFile.IsOk() ? wxGetApp().Overrides.VmSettingsFile : FilenameDefs::GetVmConfig());
	return GetSettingsFolder().Combine(fname).GetFullPath();
}

wxString GetUiSettingsFilename()
{
	wxFileName fname(FilenameDefs::GetUiConfig());
	return GetSettingsFolder().Combine(fname).GetFullPath();
}

wxString GetUiKeysFilename()
{
	wxFileName fname(FilenameDefs::GetUiKeysConfig());
	return GetSettingsFolder().Combine(fname).GetFullPath();
}

wxDirName& AppConfig::FolderOptions::operator[](FoldersEnum_t folderidx)
{
	switch (folderidx)
	{
		case FolderId_Settings:
			return SettingsFolder;
		case FolderId_Bios:
			return Bios;
		case FolderId_Snapshots:
			return Snapshots;
		case FolderId_Savestates:
			return Savestates;
		case FolderId_MemoryCards:
			return MemoryCards;
		case FolderId_Logs:
			return Logs;
		case FolderId_Langs:
			return Langs;
		case FolderId_Cheats:
			return Cheats;
		case FolderId_CheatsWS:
			return CheatsWS;
		case FolderId_Cache:
			return Cache;
		case FolderId_Textures:
			return Textures;

		case FolderId_Documents:
			return CustomDocumentsFolder;

			jNO_DEFAULT
	}
	return SettingsFolder; // unreachable, but suppresses warnings.
}

const wxDirName& AppConfig::FolderOptions::operator[](FoldersEnum_t folderidx) const
{
	return const_cast<FolderOptions*>(this)->operator[](folderidx);
}

bool AppConfig::FolderOptions::IsDefault(FoldersEnum_t folderidx) const
{
	switch (folderidx)
	{
		case FolderId_Settings:
			return UseDefaultSettingsFolder;
		case FolderId_Bios:
			return UseDefaultBios;
		case FolderId_Snapshots:
			return UseDefaultSnapshots;
		case FolderId_Savestates:
			return UseDefaultSavestates;
		case FolderId_MemoryCards:
			return UseDefaultMemoryCards;
		case FolderId_Logs:
			return UseDefaultLogs;
		case FolderId_Langs:
			return UseDefaultLangs;
		case FolderId_Cheats:
			return UseDefaultCheats;
		case FolderId_CheatsWS:
			return UseDefaultCheatsWS;
		case FolderId_Cache:
			return UseDefaultCache;
		case FolderId_Textures:
			return UseDefaultTextures;

		case FolderId_Documents:
			return false;

			jNO_DEFAULT
	}
	return false;
}

void AppConfig::FolderOptions::Set(FoldersEnum_t folderidx, const wxString& src, bool useDefault)
{
	switch (folderidx)
	{
		case FolderId_Settings:
			SettingsFolder = src;
			UseDefaultSettingsFolder = useDefault;
			EmuFolders::Settings = GetSettingsFolder().ToUTF8();
			break;

		case FolderId_Bios:
			Bios = src;
			UseDefaultBios = useDefault;
			EmuFolders::Bios = GetResolvedFolder(FolderId_Bios).ToUTF8();
			break;

		case FolderId_Snapshots:
			Snapshots = src;
			UseDefaultSnapshots = useDefault;
			EmuFolders::Snapshots = GetResolvedFolder(FolderId_Snapshots).ToUTF8();
			break;

		case FolderId_Savestates:
			Savestates = src;
			UseDefaultSavestates = useDefault;
			EmuFolders::Savestates = GetResolvedFolder(FolderId_Savestates).ToUTF8();
			break;

		case FolderId_MemoryCards:
			MemoryCards = src;
			UseDefaultMemoryCards = useDefault;
			EmuFolders::MemoryCards = GetResolvedFolder(FolderId_MemoryCards).ToUTF8();
			break;

		case FolderId_Logs:
			Logs = src;
			UseDefaultLogs = useDefault;
			EmuFolders::Logs = GetResolvedFolder(FolderId_Logs).ToUTF8();
			break;

		case FolderId_Langs:
			Langs = src;
			UseDefaultLangs = useDefault;
			EmuFolders::Langs = GetResolvedFolder(FolderId_Langs).ToUTF8();
			break;

		case FolderId_Documents:
			CustomDocumentsFolder = src;
			break;

		case FolderId_Cheats:
			Cheats = src;
			UseDefaultCheats = useDefault;
			EmuFolders::Cheats = GetResolvedFolder(FolderId_Cheats).ToUTF8();
			break;

		case FolderId_CheatsWS:
			CheatsWS = src;
			UseDefaultCheatsWS = useDefault;
			EmuFolders::CheatsWS = GetResolvedFolder(FolderId_CheatsWS).ToUTF8();
			break;

		case FolderId_Cache:
			Cache = src;
			UseDefaultCache = useDefault;
			EmuFolders::Cache = GetResolvedFolder(FolderId_Cache).ToUTF8();
			FileSystem::CreateDirectoryPath(EmuFolders::Cache.c_str(), false);
			break;

		case FolderId_Textures:
			Textures = src;
			UseDefaultTextures = useDefault;
			EmuFolders::Textures = GetResolvedFolder(FolderId_Textures).ToUTF8();
			FileSystem::CreateDirectoryPath(EmuFolders::Textures.c_str(), false);
			break;

			jNO_DEFAULT
	}
}

bool IsPortable()
{
	return InstallationMode == InstallMode_Portable;
}

AppConfig::AppConfig()
	: MainGuiPosition(wxDefaultPosition)
	, SysSettingsTabName(L"Cpu")
	, McdSettingsTabName(L"none")
	, AppSettingsTabName(L"none")
	, GameDatabaseTabName(L"none")
{
	LanguageId = wxLANGUAGE_DEFAULT;
	LanguageCode = L"default";
	RecentIsoCount = 20;
	Listbook_ImageSize = 32;
	Toolbar_ImageSize = 24;
	Toolbar_ShowLabels = true;

	EnableSpeedHacks = true;
	EnableGameFixes = false;
	EnableFastBoot = true;

	DevMode = false;

	EnablePresets = true;
	PresetIndex = 1;

	CdvdSource = CDVD_SourceType::Iso;
}

// ------------------------------------------------------------------------
void App_LoadSaveInstallSettings(IniInterface& ini)
{
	// Portable installs of PCSX2 should not save any of the following information to
	// the INI file.  Only the Run First Time Wizard option is saved, and that's done
	// from EstablishAppUserMode code.  All other options have assumed (fixed) defaults in
	// portable mode which cannot be changed/saved.

	// Note: Settins are still *loaded* from portable.ini, in case the user wants to do
	// low-level overrides of the default behavior of portable mode installs.

	if (ini.IsSaving() && (InstallationMode == InstallMode_Portable))
		return;

	static const wxChar* DocsFolderModeNames[] =
		{
			L"User",
			L"Custom",
			// WARNING: array must be NULL terminated to compute it size
			NULL};

	ini.EnumEntry(L"DocumentsFolderMode", DocsFolderMode, DocsFolderModeNames, (InstallationMode == InstallMode_Registered) ? DocsFolder_User : DocsFolder_Custom);

	ini.Entry(L"CustomDocumentsFolder", CustomDocumentsFolder, PathDefs::AppRoot());

	ini.Entry(L"UseDefaultSettingsFolder", UseDefaultSettingsFolder, true);
	ini.Entry(L"SettingsFolder", SettingsFolder, PathDefs::GetSettings());

	// "Install_Dir" conforms to the NSIS standard install directory key name.
	ini.Entry(L"Install_Dir", InstallFolder, (wxDirName)(wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath()));
	SetFullBaseDir(InstallFolder);

	ini.Flush();
}

void App_LoadInstallSettings(wxConfigBase* ini)
{
	IniLoader loader(ini);
	App_LoadSaveInstallSettings(loader);
}

void App_SaveInstallSettings(wxConfigBase* ini)
{
	IniSaver saver(ini);
	App_LoadSaveInstallSettings(saver);
}

// ------------------------------------------------------------------------
const wxChar* CDVD_SourceLabels[] =
	{
		L"ISO",
		L"Disc",
		L"NoDisc",
		NULL};

void AppConfig::LoadSaveRootItems(IniInterface& ini)
{
	IniEntry(MainGuiPosition);
	IniEntry(SysSettingsTabName);
	IniEntry(McdSettingsTabName);
	IniEntry(ComponentsTabName);
	IniEntry(AppSettingsTabName);
	IniEntry(GameDatabaseTabName);
	ini.EnumEntry(L"LanguageId", LanguageId, NULL, LanguageId);
	IniEntry(LanguageCode);
	IniEntry(RecentIsoCount);
	ini.Entry(wxT("GzipIsoIndexTemplate"), EmuConfig.GzipIsoIndexTemplate, EmuConfig.GzipIsoIndexTemplate);
	IniEntry(Listbook_ImageSize);
	IniEntry(Toolbar_ImageSize);
	IniEntry(Toolbar_ShowLabels);

	wxFileName res(CurrentIso);
	ini.Entry(L"CurrentIso", res, res, ini.IsLoading() || IsPortable());
	CurrentIso = res.GetFullPath();

	ini.Entry(wxT("CurrentBlockdump"), EmuConfig.CurrentBlockdump, EmuConfig.CurrentBlockdump);
	IniEntry(CurrentELF);
	ini.Entry(wxT("CurrentIRX"), EmuConfig.CurrentIRX, EmuConfig.CurrentIRX);

	IniEntry(EnableSpeedHacks);
	IniEntry(EnableGameFixes);
	IniEntry(EnableFastBoot);

	IniEntry(EnablePresets);
	IniEntry(PresetIndex);
	IniEntry(AskOnBoot);

	IniEntry(DevMode);

	ini.EnumEntry(L"CdvdSource", CdvdSource, CDVD_SourceLabels, CdvdSource);

#ifdef __WXMSW__
	ini.Entry(wxT("McdCompressNTFS"), EmuOptions.McdCompressNTFS, EmuOptions.McdCompressNTFS);
#endif
}

// ------------------------------------------------------------------------
void AppConfig::LoadSave(IniInterface& ini, SettingsWrapper& wrap)
{
	// do all the wx stuff first so it doesn't screw with the wrapper's path
	LoadSaveRootItems(ini);
	ProgLogBox.LoadSave(ini, L"ProgramLog");
	Folders.LoadSave(ini);

	GSWindow.LoadSave(ini);
	inputRecording.loadSave(ini);
	AudioCapture.LoadSave(ini);
	Templates.LoadSave(ini);

	// Process various sub-components:
	EmuOptions.LoadSaveMemcards(wrap);
	EmuOptions.BaseFilenames.LoadSave(wrap);
	EmuOptions.Framerate.LoadSave(wrap);

	ini.Flush();
}

// ------------------------------------------------------------------------
AppConfig::ConsoleLogOptions::ConsoleLogOptions()
	: DisplayPosition(wxDefaultPosition)
	, DisplaySize(wxSize(680, 560))
	, Theme(L"Default")
{
	Visible = true;
	AutoDock = true;
	FontSize = 8;
}

void AppConfig::ConsoleLogOptions::LoadSave(IniInterface& ini, const wxChar* logger)
{
	ScopedIniGroup path(ini, logger);

	IniEntry(Visible);
	IniEntry(AutoDock);
	IniEntry(DisplayPosition);
	IniEntry(DisplaySize);
	IniEntry(FontSize);
	IniEntry(Theme);
}

void AppConfig::FolderOptions::ApplyDefaults()
{
	if (UseDefaultBios)
		Bios = PathDefs::GetBios();
	if (UseDefaultSnapshots)
		Snapshots = PathDefs::GetSnapshots();
	if (UseDefaultSavestates)
		Savestates = PathDefs::GetSavestates();
	if (UseDefaultMemoryCards)
		MemoryCards = PathDefs::GetMemoryCards();
	if (UseDefaultLogs)
		Logs = PathDefs::GetLogs();
	if (UseDefaultLangs)
		Langs = PathDefs::GetLangs();
	if (UseDefaultCheats)
		Cheats = PathDefs::GetCheats();
	if (UseDefaultCheatsWS)
		CheatsWS = PathDefs::GetCheatsWS();
}

// ------------------------------------------------------------------------
AppConfig::FolderOptions::FolderOptions()
	: Bios(PathDefs::GetBios())
	, Snapshots(PathDefs::GetSnapshots())
	, Savestates(PathDefs::GetSavestates())
	, MemoryCards(PathDefs::GetMemoryCards())
	, Langs(PathDefs::GetLangs())
	, Logs(PathDefs::GetLogs())
	, Cheats(PathDefs::GetCheats())
	, CheatsWS(PathDefs::GetCheatsWS())
	, Resources(PathDefs::GetResources())
	, Cache(PathDefs::GetCache())

	, RunIso(PathDefs::GetDocuments()) // raw default is always the Documents folder.
	, RunELF(PathDefs::GetDocuments()) // raw default is always the Documents folder.
	, RunDisc()
{
	bitset = 0xffffffff;
}

void AppConfig::FolderOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"Folders");

	if (ini.IsSaving())
	{
		ApplyDefaults();
	}

	IniBitBool(UseDefaultBios);
	IniBitBool(UseDefaultSnapshots);
	IniBitBool(UseDefaultSavestates);
	IniBitBool(UseDefaultMemoryCards);
	IniBitBool(UseDefaultLogs);
	IniBitBool(UseDefaultLangs);
	IniBitBool(UseDefaultCheats);
	IniBitBool(UseDefaultCheatsWS);
	IniBitBool(UseDefaultTextures);

	//when saving in portable mode, we save relative paths if possible
	//  --> on load, these relative paths will be expanded relative to the exe folder.
	bool rel = (ini.IsLoading() || IsPortable());

	IniEntryDirFile(Bios, rel);
	IniEntryDirFile(Snapshots, rel);
	IniEntryDirFile(Savestates, rel);
	IniEntryDirFile(MemoryCards, rel);
	IniEntryDirFile(Logs, rel);
	IniEntryDirFile(Langs, rel);
	IniEntryDirFile(Cheats, rel);
	IniEntryDirFile(CheatsWS, rel);
	IniEntryDirFile(Cache, rel);
	IniEntryDirFile(Textures, rel);

	IniEntryDirFile(RunIso, rel);
	IniEntryDirFile(RunELF, rel);
	IniEntry(RunDisc);

	if (ini.IsLoading())
	{
		ApplyDefaults();

		for (int i = 0; i < FolderId_COUNT; ++i)
			operator[]((FoldersEnum_t)i).Normalize();

		AppSetEmuFolders();
	}
}

void AppSetEmuFolders()
{
	EmuFolders::Settings = GetSettingsFolder().ToUTF8();
	EmuFolders::Bios = GetResolvedFolder(FolderId_Bios).ToUTF8();
	EmuFolders::Snapshots = GetResolvedFolder(FolderId_Snapshots).ToUTF8();
	EmuFolders::Savestates = GetResolvedFolder(FolderId_Savestates).ToUTF8();
	EmuFolders::MemoryCards = GetResolvedFolder(FolderId_MemoryCards).ToUTF8();
	EmuFolders::Logs = GetResolvedFolder(FolderId_Logs).ToUTF8();
	EmuFolders::Langs = GetResolvedFolder(FolderId_Langs).ToUTF8();
	EmuFolders::Cheats = GetResolvedFolder(FolderId_Cheats).ToUTF8();
	EmuFolders::CheatsWS = GetResolvedFolder(FolderId_CheatsWS).ToUTF8();
	EmuFolders::Resources = g_Conf->Folders.Resources.ToUTF8();
	EmuFolders::Cache = GetResolvedFolder(FolderId_Cache).ToUTF8();
	EmuFolders::Textures = GetResolvedFolder(FolderId_Textures).ToUTF8();

	// Ensure cache directory exists, since we're going to write to it (e.g. game database)
	FileSystem::CreateDirectoryPath(EmuFolders::Cache.c_str(), false);
}

// ------------------------------------------------------------------------
AppConfig::GSWindowOptions::GSWindowOptions()
{
	CloseOnEsc = true;
	DefaultToFullscreen = false;
	AlwaysHideMouse = false;
	DisableResizeBorders = false;
	DisableScreenSaver = true;

	WindowSize = wxSize(640, 480);
	WindowPos = wxDefaultPosition;
	IsMaximized = false;
	IsFullscreen = false;
	EnableVsyncWindowFlag = false;

	IsToggleFullscreenOnDoubleClick = true;
}

void AppConfig::GSWindowOptions::SanityCheck()
{
	// Ensure Conformation of various options...

	WindowSize.x = std::max(WindowSize.x, 8);
	WindowSize.x = std::min(WindowSize.x, wxGetDisplayArea().GetWidth() - 16);

	WindowSize.y = std::max(WindowSize.y, 8);
	WindowSize.y = std::min(WindowSize.y, wxGetDisplayArea().GetHeight() - 48);

	// Make sure the upper left corner of the window is visible enought o grab and
	// move into view:
	if (!wxGetDisplayArea().Contains(wxRect(WindowPos, wxSize(48, 48))))
		WindowPos = wxDefaultPosition;
}

void AppConfig::GSWindowOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"GSWindow");

	IniEntry(CloseOnEsc);
	IniEntry(DefaultToFullscreen);
	IniEntry(AlwaysHideMouse);
	IniEntry(DisableResizeBorders);
	IniEntry(DisableScreenSaver);

	IniEntry(WindowSize);
	IniEntry(WindowPos);
	IniEntry(IsMaximized);
	IniEntry(IsFullscreen);
	IniEntry(EnableVsyncWindowFlag);

	IniEntry(IsToggleFullscreenOnDoubleClick);

	static const wxChar* AspectRatioNames[] =
		{
			L"Stretch",
			L"Auto 4:3/3:2 (Progressive)",
			L"4:3",
			L"16:9",
			// WARNING: array must be NULL terminated to compute it size
			NULL};

	ini.EnumEntry(L"AspectRatio", g_Conf->EmuOptions.GS.AspectRatio, AspectRatioNames, g_Conf->EmuOptions.GS.AspectRatio);
	if (ini.IsLoading())
		EmuConfig.CurrentAspectRatio = g_Conf->EmuOptions.GS.AspectRatio;

	static const wxChar* FMVAspectRatioSwitchNames[] =
		{
			L"Off",
			L"Auto 4:3/3:2 (Progressive)",
			L"4:3",
			L"16:9",
			// WARNING: array must be NULL terminated to compute it size
			NULL};
	ini.EnumEntry(L"FMVAspectRatioSwitch", g_Conf->EmuOptions.GS.FMVAspectRatioSwitch, FMVAspectRatioSwitchNames, g_Conf->EmuOptions.GS.FMVAspectRatioSwitch);

	ini.Entry(wxT("Zoom"), g_Conf->EmuOptions.GS.Zoom, g_Conf->EmuOptions.GS.Zoom);

	if (ini.IsLoading())
		SanityCheck();
}

AppConfig::InputRecordingOptions::InputRecordingOptions()
	: VirtualPadPosition(wxDefaultPosition)
	, m_frame_advance_amount(1)
{
}

void AppConfig::InputRecordingOptions::loadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"InputRecording");

	IniEntry(VirtualPadPosition);
	IniEntry(m_frame_advance_amount);
}

AppConfig::CaptureOptions::CaptureOptions()
{
	EnableAudio = true;
}

void AppConfig::CaptureOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"Capture");

	IniEntry(EnableAudio);
}

AppConfig::UiTemplateOptions::UiTemplateOptions()
{
	LimiterUnlimited = L"Max";
	LimiterTurbo = L"Turbo";
	LimiterSlowmo = L"Slowmo";
	LimiterNormal = L"Normal";
	OutputFrame = L"Frame";
	OutputField = L"Field";
	OutputProgressive = L"Progressive";
	OutputInterlaced = L"Interlaced";
	Paused = L"<PAUSED> ";
	TitleTemplate = L"Slot: ${slot} | Speed: ${speed} (${vfps}) | ${videomode} | Limiter: ${limiter} | ${gsdx} | ${omodei} | ${cpuusage}";
	RecordingTemplate = L"Slot: ${slot} | Frame: ${frame}/${maxFrame} | Rec. Mode: ${mode} | Speed: ${speed} (${vfps}) | Limiter: ${limiter}";
}

void AppConfig::UiTemplateOptions::LoadSave(IniInterface& ini)
{
	ScopedIniGroup path(ini, L"UiTemplates");

	IniEntry(LimiterUnlimited);
	IniEntry(LimiterTurbo);
	IniEntry(LimiterSlowmo);
	IniEntry(LimiterNormal);
	IniEntry(OutputFrame);
	IniEntry(OutputField);
	IniEntry(OutputProgressive);
	IniEntry(OutputInterlaced);
	IniEntry(Paused);
	IniEntry(TitleTemplate);
	IniEntry(RecordingTemplate);
}

int AppConfig::GetMaxPresetIndex()
{
	return 2;
}

bool AppConfig::isOkGetPresetTextAndColor(int n, wxString& label, wxColor& c)
{
	const wxString presetNamesAndColors[][2] =
		{
			{_t("Safest (No hacks)"), L"Blue"},
			{_t("Safe (Default)"), L"Dark Green"},
			{_t("Balanced"), L"Forest Green"}
		} ;
		n  = std::clamp(n, 0, GetMaxPresetIndex());

	label = wxsFormat(L"%d - ", n + 1) + presetNamesAndColors[n][0];
	c = wxColor(presetNamesAndColors[n][1]);

	return true;
}


// Apply one of several (currently 6) configuration subsets.
// The scope of the subset which each preset controlls is hardcoded here.
// Use ignoreMTVU to avoid updating the MTVU field.
// Main purpose is for the preset enforcement at launch, to avoid overwriting a user's setting.
bool AppConfig::IsOkApplyPreset(int n, bool ignoreMTVU)
{
	if (n < 0 || n > GetMaxPresetIndex())
	{
		Console.WriteLn("DEV Warning: ApplyPreset(%d): index out of range, Aborting.", n);
		return false;
	}

	//Console.WriteLn("Applying Preset %d ...", n);

	//Have some original and default values at hand to be used later.
	Pcsx2Config::FramerateOptions original_Framerate = EmuOptions.Framerate;
	Pcsx2Config::SpeedhackOptions original_SpeedHacks = EmuOptions.Speedhacks;
	AppConfig default_AppConfig;
	Pcsx2Config default_Pcsx2Config;

	//  NOTE:	Because the system currently only supports passing of an entire AppConfig to the GUI panels/menus to apply/reflect,
	//			the GUI entities should be aware of the settings which the presets control, such that when presets are used:
	//			1. The panels/entities should prevent manual modifications (by graying out) of settings which the presets control.
	//			2. The panels should not apply values which the presets don't control if the value is initiated by a preset.
	//			Currently controlled by the presets:
	//			- AppConfig:	Framerate (except turbo/slowmo factors), EnableSpeedHacks, EnableGameFixes.
	//			- EmuOptions:	Cpu, Gamefixes, SpeedHacks (except mtvu), EnablePatches, GS (except for FrameLimitEnable and VsyncEnable).
	//
	//			This essentially currently covers all the options on all the panels except for framelimiter which isn't
	//			controlled by the presets, and the entire GSWindow panel which also isn't controlled by presets
	//
	//			So, if changing the scope of the presets (making them affect more or less values), the relevant GUI entities
	//			should me modified to support it.


	//Force some settings as a (current) base for all presets.

	EmuOptions.Framerate = default_Pcsx2Config.Framerate;
	EmuOptions.Framerate.SlomoScalar = original_Framerate.SlomoScalar;
	EmuOptions.Framerate.TurboScalar = original_Framerate.TurboScalar;

	EnableGameFixes = false;

	EmuOptions.EnablePatches = true;

	EmuOptions.GS.SynchronousMTGS = default_Pcsx2Config.GS.SynchronousMTGS;
	EmuOptions.GS.FrameSkipEnable = default_Pcsx2Config.GS.FrameSkipEnable;
	EmuOptions.GS.FramesToDraw = default_Pcsx2Config.GS.FramesToDraw;
	EmuOptions.GS.FramesToSkip = default_Pcsx2Config.GS.FramesToSkip;

	EmuOptions.Cpu = default_Pcsx2Config.Cpu;
	EmuOptions.Gamefixes = default_Pcsx2Config.Gamefixes;
	EmuOptions.Speedhacks = default_Pcsx2Config.Speedhacks;
	EmuOptions.Speedhacks.bitset = 0; //Turn off individual hacks to make it visually clear they're not used.
	EmuOptions.Speedhacks.vuThread = original_SpeedHacks.vuThread;
	EmuOptions.Speedhacks.vu1Instant = original_SpeedHacks.vu1Instant;
	EnableSpeedHacks = true;

	// Actual application of current preset over the base settings which all presets use (mostly pcsx2's default values).

	bool  isMTVUSet = ignoreMTVU ? true : false; // used to prevent application of specific lower preset values on fallthrough.
	switch (n) // Settings will waterfall down to the Safe preset, then stop. So, Balanced and higher will inherit any settings through Safe.
	{
		case 2: // Balanced
			isMTVUSet ? 0 : (isMTVUSet = true, EmuOptions.Speedhacks.vuThread = true); // Enable MTVU
			[[fallthrough]];

		case 1: // Safe (Default)
			EmuOptions.Speedhacks.IntcStat = true;
			EmuOptions.Speedhacks.WaitLoop = true;
			EmuOptions.Speedhacks.vuFlagHack = true;
			EmuOptions.Speedhacks.vu1Instant = true;

			// If waterfalling from > Safe, break to avoid MTVU disable.
			if (n > 1)
				break;
			[[fallthrough]];

		case 0: // Safest
			if (n == 0)
				EmuOptions.Speedhacks.vu1Instant = false;
			isMTVUSet ? 0 : (isMTVUSet = true, EmuOptions.Speedhacks.vuThread = false); // Disable MTVU
			break;

		default:
			Console.WriteLn("Developer Warning: Preset #%d is not implemented. (--> Using application default).", n);
	}


	EnablePresets = true;
	PresetIndex = n;

	return true;
}


wxFileConfig* OpenFileConfig(const wxString& filename)
{
	return new wxFileConfig(wxEmptyString, wxEmptyString, filename, wxEmptyString, wxCONFIG_USE_RELATIVE_PATH);
}

void RelocateLogfile()
{
	g_Conf->Folders.Logs.Mkdir();

	std::string newlogname(StringUtil::wxStringToUTF8String(Path::CombineWx(g_Conf->Folders.Logs.ToString(), L"emuLog.txt")));

	if ((emuLog != NULL) && (emuLogName != newlogname))
	{
		Console.WriteLn("\nRelocating Logfile...\n\tFrom: %ls\n\tTo  : %ls\n", emuLogName.c_str(), newlogname.c_str());
		wxGetApp().DisableDiskLogging();

		fclose(emuLog);
		emuLog = NULL;
	}

	if (emuLog == NULL)
	{
		emuLogName = newlogname;
		emuLog = FileSystem::OpenCFile(emuLogName.c_str(), "wb");
	}

	wxGetApp().EnableAllLogging();
}

// Parameters:
//   overwrite - this option forces the current settings to overwrite any existing settings
//      that might be saved to the configured ini/settings folder.
//
// Notes:
//   The overwrite option applies to PCSX2 options only.
//
void AppConfig_OnChangedSettingsFolder(bool overwrite)
{
	PathDefs::GetDocuments().Mkdir();
	GetSettingsFolder().Mkdir();

	const wxString iniFilename(GetUiSettingsFilename());

	if (overwrite)
	{
		if (wxFileExists(iniFilename) && !wxRemoveFile(iniFilename))
			throw Exception::AccessDenied(StringUtil::wxStringToUTF8String(iniFilename))
				.SetBothMsgs("Failed to overwrite existing settings file; permission was denied.");

		const wxString vmIniFilename(GetVmSettingsFilename());

		if (wxFileExists(vmIniFilename) && !wxRemoveFile(vmIniFilename))
			throw Exception::AccessDenied(StringUtil::wxStringToUTF8String(vmIniFilename))
				.SetBothMsgs("Failed to overwrite existing settings file; permission was denied.");
	}

	// Bind into wxConfigBase to allow wx to use our config internally, and delete whatever
	// comes out (cleans up prev config, if one).
	delete wxConfigBase::Set(OpenFileConfig(iniFilename));
	GetAppConfig()->SetRecordDefaults(true);

	if (!overwrite)
		AppLoadSettings();
	else
		AppSetEmuFolders();

	AppApplySettings();
	AppSaveSettings(); //Make sure both ini files are created if needed.
}

// --------------------------------------------------------------------------------------
//  pxDudConfig
// --------------------------------------------------------------------------------------
// Used to handle config actions prior to the creation of the ini file (for example, the
// first time wizard).  Attempts to save ini settings are simply ignored through this
// class, which allows us to give the user a way to set everything up in the wizard, apply
// settings as usual, and only *save* something once the whole wizard is complete.
//
class pxDudConfig : public wxConfigBase
{
protected:
	wxString m_empty;

public:
	virtual ~pxDudConfig() = default;

	virtual void SetPath(const wxString&) {}
	virtual const wxString& GetPath() const { return m_empty; }

	virtual bool GetFirstGroup(wxString&, long&) const { return false; }
	virtual bool GetNextGroup(wxString&, long&) const { return false; }
	virtual bool GetFirstEntry(wxString&, long&) const { return false; }
	virtual bool GetNextEntry(wxString&, long&) const { return false; }
	virtual size_t GetNumberOfEntries(bool) const { return 0; }
	virtual size_t GetNumberOfGroups(bool) const { return 0; }

	virtual bool HasGroup(const wxString&) const { return false; }
	virtual bool HasEntry(const wxString&) const { return false; }

	virtual bool Flush(bool) { return false; }

	virtual bool RenameEntry(const wxString&, const wxString&) { return false; }

	virtual bool RenameGroup(const wxString&, const wxString&) { return false; }

	virtual bool DeleteEntry(const wxString&, bool bDeleteGroupIfEmpty = true) { return false; }
	virtual bool DeleteGroup(const wxString&) { return false; }
	virtual bool DeleteAll() { return false; }

protected:
	virtual bool DoReadString(const wxString&, wxString*) const { return false; }
	virtual bool DoReadLong(const wxString&, long*) const { return false; }

	virtual bool DoWriteString(const wxString&, const wxString&) { return false; }
	virtual bool DoWriteLong(const wxString&, long) { return false; }

#if wxUSE_BASE64
	virtual bool DoReadBinary(const wxString& key, wxMemoryBuffer* buf) const
	{
		return false;
	}
	virtual bool DoWriteBinary(const wxString& key, const wxMemoryBuffer& buf) { return false; }
#endif
};

static pxDudConfig _dud_config;

// --------------------------------------------------------------------------------------
//  AppIniSaver / AppIniLoader
// --------------------------------------------------------------------------------------
class AppIniSaver : public IniSaver
{
public:
	AppIniSaver();
	virtual ~AppIniSaver() = default;
};

class AppIniLoader : public IniLoader
{
public:
	AppIniLoader();
	virtual ~AppIniLoader() = default;
};

AppIniSaver::AppIniSaver()
	: IniSaver((GetAppConfig() != NULL) ? *GetAppConfig() : _dud_config)
{
}

AppIniLoader::AppIniLoader()
	: IniLoader((GetAppConfig() != NULL) ? *GetAppConfig() : _dud_config)
{
}

static void LoadUiSettings()
{
	AppIniLoader loader;
	ConLog_LoadSaveSettings(loader);
	SysTraceLog_LoadSaveSettings(loader);

	{
		wxSettingsInterface wxsi(&loader.GetConfig());
		SettingsLoadWrapper wrapper(wxsi);
		g_Conf = std::make_unique<AppConfig>();
		g_Conf->LoadSave(loader, wrapper);
	}

	if (!wxFile::Exists(g_Conf->CurrentIso))
	{
		g_Conf->CurrentIso.clear();
	}

	sApp.DispatchUiSettingsEvent(loader);
}

static void LoadVmSettings()
{
	// Load virtual machine options and apply some defaults overtop saved items, which
	// are regulated by the PCSX2 UI.

	std::unique_ptr<wxFileConfig> vmini(OpenFileConfig(GetVmSettingsFilename()));
	IniLoader vmloader(vmini.get());
	{
		wxSettingsInterface wxsi(vmini.get());
		SettingsLoadWrapper vmwrapper(wxsi);
		g_Conf->EmuOptions.LoadSave(vmwrapper);
		g_Conf->EmuOptions.GS.LimitScalar = g_Conf->EmuOptions.Framerate.NominalScalar;
	}

	if (g_Conf->EnablePresets)
	{
		g_Conf->IsOkApplyPreset(g_Conf->PresetIndex, true);
	}

	sApp.DispatchVmSettingsEvent(vmloader);
}

void AppLoadSettings()
{
	if (wxGetApp().Rpc_TryInvoke(AppLoadSettings))
		return;

	LoadUiSettings();
	LoadVmSettings();
}

static void SaveUiSettings()
{
	if (!wxFile::Exists(g_Conf->CurrentIso))
	{
		g_Conf->CurrentIso.clear();
	}

	sApp.GetRecentIsoManager().Add(g_Conf->CurrentIso);

	AppIniSaver saver;
	{
		wxSettingsInterface wxsi(&saver.GetConfig());
		SettingsSaveWrapper wrapper(wxsi);
		g_Conf->LoadSave(saver, wrapper);
	}
	ConLog_LoadSaveSettings(saver);
	SysTraceLog_LoadSaveSettings(saver);

	sApp.DispatchUiSettingsEvent(saver);
}

static void SaveVmSettings()
{
	std::unique_ptr<wxFileConfig> vmini(OpenFileConfig(GetVmSettingsFilename()));
	IniSaver vmsaver(vmini.get());
	{
		wxSettingsInterface wxsi(vmini.get());
		SettingsSaveWrapper vmwrapper(wxsi);
		g_Conf->EmuOptions.LoadSave(vmwrapper);
	}

	sApp.DispatchVmSettingsEvent(vmsaver);
}

void AppSaveSettings()
{
	// If multiple SaveSettings messages are requested, we want to ignore most of them.
	// Saving settings once when the GUI is idle should be fine. :)

	static std::atomic<bool> isPosted(false);

	if (!wxThread::IsMain())
	{
		if (!isPosted.exchange(true))
			wxGetApp().PostIdleMethod(AppSaveSettings);

		return;
	}

	//Console.WriteLn("Saving ini files...");

	SaveUiSettings();
	SaveVmSettings();

	isPosted = false;
}


// Returns the current application configuration file.  This is preferred over using
// wxConfigBase::GetAppConfig(), since it defaults to *not* creating a config file
// automatically (which is typically highly undesired behavior in our system)
wxConfigBase* GetAppConfig()
{
	return wxConfigBase::Get(false);
}


//Tests if a string is a valid name for a new file within a specified directory.
//returns true if:
//     - the file name has a minimum length of minNumCharacters chars (default is 5 chars: at least 1 char + '.' + 3-chars extension)
// and - the file name is within the basepath directory (doesn't contain .. , / , \ , etc)
// and - file name doesn't already exist
// and - can be created on current system (it is actually created and deleted for this test).
bool isValidNewFilename(wxString filenameStringToTest, wxDirName atBasePath, wxString& out_errorMessage, uint minNumCharacters)
{
	if (filenameStringToTest.Length() < 1 || filenameStringToTest.Length() < minNumCharacters)
	{
		out_errorMessage = _("File name empty or too short");
		return false;
	}

	if ((atBasePath + wxFileName(filenameStringToTest)).GetFullPath() != (atBasePath + wxFileName(filenameStringToTest).GetFullName()).GetFullPath())
	{
		out_errorMessage = _("File name outside of required directory");
		return false;
	}

	if (wxFileExists((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("File name already exists");
		return false;
	}
	if (wxDirExists((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("File name already exists");
		return false;
	}

	wxFile fp;
	if (!fp.Create((atBasePath + wxFileName(filenameStringToTest)).GetFullPath()))
	{
		out_errorMessage = _("The Operating-System prevents this file from being created");
		return false;
	}
	fp.Close();
	wxRemoveFile((atBasePath + wxFileName(filenameStringToTest)).GetFullPath());

	out_errorMessage = L"[OK - New file name is valid]"; //shouldn't be displayed on success, hence not translatable.
	return true;
}
