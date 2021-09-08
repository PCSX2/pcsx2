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
#include "AppSaveStates.h"
#include "AppGameDatabase.h"

#include <wx/stdpaths.h>
#include "fmt/core.h"

#include "common/StringUtil.h"
#include "common/Threading.h"

#include "ps2/BiosTools.h"
#include "GS.h"

#include "CDVD/CDVD.h"
#include "USB/USB.h"
#include "Elfheader.h"
#include "Patch.h"
#include "R5900Exceptions.h"
#include "Sio.h"

#ifndef DISABLE_RECORDING
#include "Recording/InputRecordingControls.h"
#endif

alignas(16) SysMtgsThread mtgsThread;
alignas(16) AppCoreThread CoreThread;

typedef void (AppCoreThread::*FnPtr_CoreThreadMethod)();

SysCoreThread& GetCoreThread()
{
	return CoreThread;
}

SysMtgsThread& GetMTGS()
{
	return mtgsThread;
}

namespace GameInfo
{
	wxString gameName;
	wxString gameSerial;
	wxString gameCRC;
	wxString gameVersion;
}; // namespace GameInfo

// --------------------------------------------------------------------------------------
//  SysExecEvent_InvokeCoreThreadMethod
// --------------------------------------------------------------------------------------
class SysExecEvent_InvokeCoreThreadMethod : public SysExecEvent
{
protected:
	FnPtr_CoreThreadMethod m_method;
	bool m_IsCritical;

public:
	wxString GetEventName() const { return L"CoreThreadMethod"; }
	virtual ~SysExecEvent_InvokeCoreThreadMethod() = default;
	SysExecEvent_InvokeCoreThreadMethod* Clone() const { return new SysExecEvent_InvokeCoreThreadMethod(*this); }

	bool AllowCancelOnExit() const { return false; }
	bool IsCriticalEvent() const { return m_IsCritical; }

	SysExecEvent_InvokeCoreThreadMethod(FnPtr_CoreThreadMethod method, bool critical = false)
	{
		m_method = method;
		m_IsCritical = critical;
	}

	SysExecEvent_InvokeCoreThreadMethod& Critical()
	{
		m_IsCritical = true;
		return *this;
	}

protected:
	void InvokeEvent()
	{
		if (m_method)
			(CoreThread.*m_method)();
	}
};

static void PostCoreStatus(CoreThreadStatus pevt)
{
	sApp.PostAction(CoreThreadStatusEvent(pevt));
}

// --------------------------------------------------------------------------------------
//  AppCoreThread Implementations
// --------------------------------------------------------------------------------------
AppCoreThread::AppCoreThread()
	: SysCoreThread()
{
	m_resetCdvd = false;
}

AppCoreThread::~AppCoreThread()
{
	try
	{
		_parent::Cancel(); // use parent's, skips thread affinity check.
	}
	DESTRUCTOR_CATCHALL
}

static void _Cancel()
{
	GetCoreThread().Cancel();
}

void AppCoreThread::Cancel(bool isBlocking)
{
	if (GetSysExecutorThread().IsRunning() && !GetSysExecutorThread().Rpc_TryInvoke(_Cancel, L"AppCoreThread::Cancel"))
		_parent::Cancel(wxTimeSpan(0, 0, 4, 0));
}

void AppCoreThread::Reset()
{
	if (!GetSysExecutorThread().IsSelf())
	{
		GetSysExecutorThread().PostEvent(SysExecEvent_InvokeCoreThreadMethod(&AppCoreThread::Reset));
		return;
	}

	_parent::Reset();
}

void AppCoreThread::ResetQuick()
{
	if (!GetSysExecutorThread().IsSelf())
	{
		GetSysExecutorThread().PostEvent(SysExecEvent_InvokeCoreThreadMethod(&AppCoreThread::ResetQuick));
		return;
	}

	_parent::ResetQuick();
}

ExecutorThread& GetSysExecutorThread()
{
	return wxGetApp().SysExecutorThread;
}

static void _Suspend()
{
	GetCoreThread().Suspend(true);
}

void AppCoreThread::Suspend(bool isBlocking)
{
	if (IsClosed())
		return;

	if (IsSelf())
	{
		// this should never fail...
		bool result = GetSysExecutorThread().Rpc_TryInvokeAsync(_Suspend, L"AppCoreThread::Suspend");
		pxAssert(result);
	}
	else if (!GetSysExecutorThread().Rpc_TryInvoke(_Suspend, L"AppCoreThread::Suspend"))
		_parent::Suspend(true);
}

void AppCoreThread::Resume()
{
	if (!GetSysExecutorThread().IsSelf())
	{
		GetSysExecutorThread().PostEvent(SysExecEvent_InvokeCoreThreadMethod(&AppCoreThread::Resume));
		return;
	}
	_parent::Resume();
}

void AppCoreThread::ChangeCdvdSource()
{
	if (!GetSysExecutorThread().IsSelf())
	{
		GetSysExecutorThread().PostEvent(new SysExecEvent_InvokeCoreThreadMethod(&AppCoreThread::ChangeCdvdSource));
		return;
	}

	CDVD_SourceType cdvdsrc(g_Conf->CdvdSource);
	if (cdvdsrc == CDVDsys_GetSourceType())
		return;

	// Fast change of the CDVD source only -- a Pause will suffice.

	ScopedCoreThreadPause paused_core;
	CDVDsys_ChangeSource(cdvdsrc);
	paused_core.AllowResume();

	// TODO: Add a listener for CDVDsource changes?  Or should we bother?
}

void Pcsx2App::SysApplySettings()
{
	if (AppRpc_TryInvoke(&Pcsx2App::SysApplySettings))
		return;
	CoreThread.ApplySettings(g_Conf->EmuOptions);

	const CDVD_SourceType cdvdsrc(g_Conf->CdvdSource);
	const std::string currentIso(StringUtil::wxStringToUTF8String(g_Conf->CurrentIso));
	const std::string currentDisc(StringUtil::wxStringToUTF8String(g_Conf->Folders.RunDisc));
	if (cdvdsrc != CDVDsys_GetSourceType() ||
			CDVDsys_GetFile(CDVD_SourceType::Iso) != currentIso ||
			CDVDsys_GetFile(CDVD_SourceType::Disc) != currentDisc)
	{
		CoreThread.ResetCdvd();
	}

	CDVDsys_SetFile(CDVD_SourceType::Iso, currentIso);
	CDVDsys_SetFile(CDVD_SourceType::Disc, currentDisc);
}

void AppCoreThread::OnResumeReady()
{
#ifndef DISABLE_RECORDING
	if (!g_InputRecordingControls.IsFrameAdvancing())
	{
		wxGetApp().SysApplySettings();
		wxGetApp().PostMethod(AppSaveSettings);
	}
#else
	wxGetApp().SysApplySettings();
	wxGetApp().PostMethod(AppSaveSettings);
#endif

	sApp.PostAppMethod(&Pcsx2App::leaveDebugMode);
	_parent::OnResumeReady();
}

void AppCoreThread::OnPause()
{
	//sApp.PostAppMethod( &Pcsx2App::enterDebugMode );
	_parent::OnPause();
}

void AppCoreThread::OnPauseDebug()
{
	sApp.PostAppMethod(&Pcsx2App::enterDebugMode);
	_parent::OnPause();
}

// Load Game Settings found in database
// (game fixes, round modes, clamp modes, etc...)
// Returns number of gamefixes set
static int loadGameSettings(Pcsx2Config& dest, const GameDatabaseSchema::GameEntry& game)
{
	if (!game.isValid)
		return 0;

	int gf = 0;

	if (game.eeRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		SSE_RoundMode eeRM = (SSE_RoundMode)enum_cast(game.eeRoundMode);
		if (EnumIsValid(eeRM))
		{
			PatchesCon->WriteLn(L"(GameDB) Changing EE/FPU roundmode to %d [%s]", eeRM, EnumToString(eeRM));
			dest.Cpu.sseMXCSR.SetRoundMode(eeRM);
			gf++;
		}
	}

	if (game.vuRoundMode != GameDatabaseSchema::RoundMode::Undefined)
	{
		SSE_RoundMode vuRM = (SSE_RoundMode)enum_cast(game.vuRoundMode);
		if (EnumIsValid(vuRM))
		{
			PatchesCon->WriteLn(L"(GameDB) Changing VU0/VU1 roundmode to %d [%s]", vuRM, EnumToString(vuRM));
			dest.Cpu.sseVUMXCSR.SetRoundMode(vuRM);
			gf++;
		}
	}

	if (game.eeClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		int clampMode = enum_cast(game.eeClampMode);
		PatchesCon->WriteLn(L"(GameDB) Changing EE/FPU clamp mode [mode=%d]", clampMode);
		dest.Cpu.Recompiler.fpuOverflow = (clampMode >= 1);
		dest.Cpu.Recompiler.fpuExtraOverflow = (clampMode >= 2);
		dest.Cpu.Recompiler.fpuFullMode = (clampMode >= 3);
		gf++;
	}

	if (game.vuClampMode != GameDatabaseSchema::ClampMode::Undefined)
	{
		int clampMode = enum_cast(game.vuClampMode);
		PatchesCon->WriteLn("(GameDB) Changing VU0/VU1 clamp mode [mode=%d]", clampMode);
		dest.Cpu.Recompiler.vuOverflow = (clampMode >= 1);
		dest.Cpu.Recompiler.vuExtraOverflow = (clampMode >= 2);
		dest.Cpu.Recompiler.vuSignOverflow = (clampMode >= 3);
		gf++;
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (SpeedhackId id = SpeedhackId_FIRST; id < pxEnumEnd; id++)
	{
		std::string key = fmt::format("{}SpeedHack", wxString(EnumToString(id)).ToUTF8());

		// Gamefixes are already guaranteed to be valid, any invalid ones are dropped
		if (game.speedHacks.count(key) == 1)
		{
			// Legacy note - speedhacks are setup in the GameDB as integer values, but
			// are effectively booleans like the gamefixes
			bool mode = game.speedHacks.at(key) ? 1 : 0;
			dest.Speedhacks.Set(id, mode);
			PatchesCon->WriteLn(fmt::format("(GameDB) Setting Speedhack '{}' to [mode={}]", key, (int)mode));
			gf++;
		}
	}

	// TODO - config - this could be simplified with maps instead of bitfields and enums
	for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; id++)
	{
		std::string key = fmt::format("{}Hack", wxString(EnumToString(id)).ToUTF8());

		// Gamefixes are already guaranteed to be valid, any invalid ones are dropped
		if (std::find(game.gameFixes.begin(), game.gameFixes.end(), key) != game.gameFixes.end())
		{
			// if the fix is present, it is said to be enabled
			dest.Gamefixes.Set(id, true);
			PatchesCon->WriteLn("(GameDB) Enabled Gamefix: " + key);
			gf++;

			// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
			if (id == Fix_GoemonTlbMiss && true)
				vtlb_Alloc_Ppmap();
		}
	}

	return gf;
}

// Used to track the current game serial/id, and used to disable verbose logging of
// applied patches if the game info hasn't changed.  (avoids spam when suspending/resuming
// or using TAB or other things), but gets verbose again when booting (even if the same game).
// File scope since it gets reset externally when rebooting
#define _UNKNOWN_GAME_KEY (L"_UNKNOWN_GAME_KEY")
static wxString curGameKey = _UNKNOWN_GAME_KEY;

void PatchesVerboseReset()
{
	curGameKey = _UNKNOWN_GAME_KEY;
}

// PatchesCon points to either Console or ConsoleWriter_Null, such that if we're in Devel mode
// or the user enabled the devel/verbose console it prints all patching info whenever it's applied,
// else it prints the patching info only once - right after boot.
const IConsoleWriter* PatchesCon = &Console;

static void SetupPatchesCon(bool verbose)
{
	bool devel = false;
#ifdef PCSX2_DEVBUILD
	devel = true;
#endif

	if (verbose || DevConWriterEnabled || devel)
		PatchesCon = &Console;
	else
		PatchesCon = &ConsoleWriter_Null;
}

// fixup = src + command line overrides + game overrides (according to elfCRC).
// While at it, also [re]loads the relevant patches (but doesn't apply them),
// updates the console title, and, for good measures, does some (static) sio stuff.
// Oh, and updates curGameKey. I think that's it.
// It doesn't require that the emulation is paused, and console writes/title should
// be thread safe, but it's best if things don't move around much while it runs.
static Threading::Mutex mtx__ApplySettings;
static void _ApplySettings(const Pcsx2Config& src, Pcsx2Config& fixup)
{
	Threading::ScopedLock lock(mtx__ApplySettings);
	// 'fixup' is the EmuConfig we're going to upload to the emulator, which very well may
	// differ from the user-configured EmuConfig settings.  So we make a copy here and then
	// we apply the commandline overrides and database gamefixes, and then upload 'fixup'
	// to the global EmuConfig.
	//
	// Note: It's important that we apply the commandline overrides *before* database fixes.
	// The database takes precedence (if enabled).

	fixup.CopyConfig(src);

	const CommandlineOverrides& overrides(wxGetApp().Overrides);
	if (overrides.DisableSpeedhacks || !g_Conf->EnableSpeedHacks)
		fixup.Speedhacks.DisableAll();

	if (overrides.ApplyCustomGamefixes)
	{
		for (GamefixId id = GamefixId_FIRST; id < pxEnumEnd; ++id)
			fixup.Gamefixes.Set(id, overrides.Gamefixes.Get(id));
	}
	else if (!g_Conf->EnableGameFixes)
		fixup.Gamefixes.DisableAll();

	if (overrides.ProfilingMode)
	{
		fixup.GS.FrameLimitEnable = false;
		fixup.GS.VsyncEnable = VsyncMode::Off;
	}

	wxString gamePatch;
	wxString gameFixes;
	wxString gameCheats;
	wxString gameWsHacks;

	wxString gameCompat;
	wxString gameMemCardFilter;

	// The CRC can be known before the game actually starts (at the bios), so when
	// we have the CRC but we're still at the bios and the settings are changed
	// (e.g. the user presses TAB to speed up emulation), we don't want to apply the
	// settings as if the game is already running (title, loadeding patches, etc).
	bool ingame = (ElfCRC && (g_GameLoading || g_GameStarted));
	if (ingame)
		GameInfo::gameCRC.Printf(L"%8.8x", ElfCRC);
	else
		GameInfo::gameCRC = L""; // Needs to be reset when rebooting otherwise previously loaded patches may load

	if (ingame && !DiscSerial.IsEmpty())
		GameInfo::gameSerial = L" [" + DiscSerial + L"]";

	const wxString newGameKey(ingame ? SysGetDiscID() : SysGetBiosDiscID());
	const bool verbose(newGameKey != curGameKey && ingame);
	//Console.WriteLn(L"------> patches verbose: %d   prev: '%s'   new: '%s'", (int)verbose, WX_STR(curGameKey), WX_STR(newGameKey));
	SetupPatchesCon(verbose);

	curGameKey = newGameKey;

	ForgetLoadedPatches();

	if (!curGameKey.IsEmpty())
	{
		if (IGameDatabase* GameDB = AppHost_GetGameDatabase())
		{
			GameDatabaseSchema::GameEntry game = GameDB->findGame(std::string(curGameKey.ToUTF8()));
			if (game.isValid)
			{
				GameInfo::gameName = fromUTF8(game.name);
				GameInfo::gameName += L" (" + fromUTF8(game.region) + L")";
				gameCompat = L" [Status = " + compatToStringWX(game.compat) + L"]";
				gameMemCardFilter = fromUTF8(game.memcardFiltersAsString());
			}
			else
			{
				// Set correct title for loading standalone/homebrew ELFs
				GameInfo::gameName = LastELF.AfterLast('\\');
			}

			if (fixup.EnablePatches)
			{
				if (int patches = LoadPatchesFromGamesDB(GameInfo::gameCRC, game))
				{
					gamePatch.Printf(L" [%d Patches]", patches);
					PatchesCon->WriteLn(Color_Green, "(GameDB) Patches Loaded: %d", patches);
				}
				if (int fixes = loadGameSettings(fixup, game))
					gameFixes.Printf(L" [%d Fixes]", fixes);
			}
		}
	}

	if (!gameMemCardFilter.IsEmpty())
		sioSetGameSerial(gameMemCardFilter);
	else
		sioSetGameSerial(curGameKey);

	if (GameInfo::gameName.IsEmpty() && GameInfo::gameSerial.IsEmpty() && GameInfo::gameCRC.IsEmpty())
	{
		// if all these conditions are met, it should mean that we're currently running BIOS code.
		// Chances are the BiosChecksum value is still zero or out of date, however -- because
		// the BIos isn't loaded until after initial calls to ApplySettings.

		GameInfo::gameName = L"Booting PS2 BIOS... ";
	}

	//Till the end of this function, entry CRC will be 00000000
	if (!GameInfo::gameCRC.Length())
	{
		Console.WriteLn(Color_Gray, "Patches: No CRC found, using 00000000 instead.");
		GameInfo::gameCRC = L"00000000";
	}

	// regular cheat patches
	if (fixup.EnableCheats)
		gameCheats.Printf(L" [%d Cheats]", LoadPatchesFromDir(GameInfo::gameCRC, EmuFolders::Cheats, L"Cheats"));

	// wide screen patches
	if (fixup.EnableWideScreenPatches)
	{
		if (int numberLoadedWideScreenPatches = LoadPatchesFromDir(GameInfo::gameCRC, EmuFolders::CheatsWS, L"Widescreen hacks"))
		{
			gameWsHacks.Printf(L" [%d widescreen hacks]", numberLoadedWideScreenPatches);
			Console.WriteLn(Color_Gray, "Found widescreen patches in the cheats_ws folder --> skipping cheats_ws.zip");
		}
		else
		{
			// No ws cheat files found at the cheats_ws folder, try the ws cheats zip file.
			wxString cheats_ws_archive = Path::Combine(PathDefs::GetProgramDataDir(), wxFileName(L"cheats_ws.zip"));
			int numberDbfCheatsLoaded = LoadPatchesFromZip(GameInfo::gameCRC, cheats_ws_archive);
			PatchesCon->WriteLn(Color_Green, "(Wide Screen Cheats DB) Patches Loaded: %d", numberDbfCheatsLoaded);
			gameWsHacks.Printf(L" [%d widescreen hacks]", numberDbfCheatsLoaded);
		}
	}

	// When we're booting, the bios loader will set a a title which would be more interesting than this
	// to most users - with region, version, etc, so don't overwrite it with patch info. That's OK. Those
	// users which want to know the status of the patches at the bios can check the console content.
	wxString consoleTitle = GameInfo::gameName + GameInfo::gameSerial;
	consoleTitle += L" [" + GameInfo::gameCRC.MakeUpper() + L"]" + gameCompat + gameFixes + gamePatch + gameCheats + gameWsHacks;
	if (ingame)
		Console.SetTitle(consoleTitle);

	gsUpdateFrequency(fixup);
}

// FIXME: This function is not for general consumption. Its only consumer (and
//        the only place it's declared at) is i5900-32.cpp .
// It's here specifically to allow loading the patches synchronously (to the caller)
// when the recompiler detects the game's elf entry point, such that the patches
// are applied to the elf in memory before it's getting recompiled.
// TODO: Find a way to pause the recompiler once it detects the elf entry, then
//       make AppCoreThread::ApplySettings run more normally, and then resume
//       the recompiler.
//       The key point is that the patches should be loaded exactly before the elf
//       is recompiled. Loading them earlier might incorrectly patch the bios memory,
//       and later might be too late since the code was already recompiled
void LoadAllPatchesAndStuff(const Pcsx2Config& cfg)
{
	Pcsx2Config dummy;
	PatchesVerboseReset();
	_ApplySettings(cfg, dummy);

	// And I'm hacking in updating the UI here too.
#ifdef USE_SAVESLOT_UI_UPDATES
	UI_UpdateSysControls();
#endif
}

void AppCoreThread::ApplySettings(const Pcsx2Config& src)
{
	// Re-entry guard protects against cases where code wants to manually set core settings
	// which are not part of g_Conf.  The subsequent call to apply g_Conf settings (which is
	// usually the desired behavior) will be ignored.
	Pcsx2Config fixup;
	_ApplySettings(src, fixup);

	static int localc = 0;
	RecursionGuard guard(localc);
	if (guard.IsReentrant())
		return;
	if (fixup == EmuConfig)
		return;

	if (m_ExecMode >= ExecMode_Opened && !IsSelf())
	{
		ScopedCoreThreadPause paused_core;
		_parent::ApplySettings(fixup);
		paused_core.AllowResume();
	}
	else
	{
		_parent::ApplySettings(fixup);
	}

	if (m_ExecMode >= ExecMode_Paused)
		GSsetVsync(EmuConfig.GS.GetVsync());
}

// --------------------------------------------------------------------------------------
//  AppCoreThread *Worker* Implementations
//    (Called from the context of this thread only)
// --------------------------------------------------------------------------------------

void AppCoreThread::DoCpuReset()
{
	PostCoreStatus(CoreThread_Reset);
	_parent::DoCpuReset();
}

void AppCoreThread::OnResumeInThread(SystemsMask systemsToReinstate)
{
	if (m_resetCdvd)
	{
		CDVDsys_ChangeSource(g_Conf->CdvdSource);
		DoCDVDopen();
		cdvdCtrlTrayOpen();
		m_resetCdvd = false;
	}
	else if (systemsToReinstate & System_CDVD)
		DoCDVDopen();

	_parent::OnResumeInThread(systemsToReinstate);
	PostCoreStatus(CoreThread_Resumed);
}

void AppCoreThread::OnSuspendInThread()
{
	_parent::OnSuspendInThread();
	PostCoreStatus(CoreThread_Suspended);
}

// Called whenever the thread has terminated, for either regular or irregular reasons.
// Typically the thread handles all its own errors, so there's no need to have error
// handling here.  However it's a good idea to update the status of the GUI to reflect
// the new (lack of) thread status, so this posts a message to the App to do so.
void AppCoreThread::OnCleanupInThread()
{
	m_ExecMode = ExecMode_Closing;
	PostCoreStatus(CoreThread_Stopped);
	_parent::OnCleanupInThread();
}

void AppCoreThread::VsyncInThread()
{
	wxGetApp().LogicalVsync();
	_parent::VsyncInThread();
}

void AppCoreThread::GameStartingInThread()
{
	// Simulate a Close/Resume, so that settings get re-applied and the database
	// lookups and other game-based detections are done.

	m_ExecMode = ExecMode_Paused;
	OnResumeReady();
	_reset_stuff_as_needed();
	ClearMcdEjectTimeoutNow(); // probably safe to do this when a game boots, eliminates annoying prompts
	m_ExecMode = ExecMode_Opened;

	_parent::GameStartingInThread();
}

bool AppCoreThread::StateCheckInThread()
{
	return _parent::StateCheckInThread();
}

static uint m_except_threshold = 0;

void AppCoreThread::ExecuteTaskInThread()
{
	PostCoreStatus(CoreThread_Started);
	m_except_threshold = 0;
	_parent::ExecuteTaskInThread();
}

void AppCoreThread::DoCpuExecute()
{
	try
	{
		_parent::DoCpuExecute();
	}
	catch (BaseR5900Exception& ex)
	{
		Console.Error(ex.FormatMessage());

		// [TODO] : Debugger Hook!

		if (++m_except_threshold > 6)
		{
			// If too many TLB Misses occur, we're probably going to crash and
			// the game is probably running miserably.

			m_except_threshold = 0;
			//Suspend();

			// [TODO] Issue error dialog to the user here...
			Console.Error("Too many execution errors.  VM execution has been suspended!");

			// Hack: this keeps the EE thread from running more code while the SysExecutor
			// thread catches up and signals it for suspension.
			m_ExecMode = ExecMode_Closing;
		}
	}
}

// --------------------------------------------------------------------------------------
//  BaseSysExecEvent_ScopedCore / SysExecEvent_CoreThreadClose / SysExecEvent_CoreThreadPause
// --------------------------------------------------------------------------------------
void BaseSysExecEvent_ScopedCore::_post_and_wait(IScopedCoreThread& core)
{
	DoScopedTask();

	ScopedLock lock(m_mtx_resume);
	PostResult();

	if (m_resume)
	{
		// If the sender of the message requests a non-blocking resume, then we need
		// to deallocate the m_sync object, since the sender will likely leave scope and
		// invalidate it.
		switch (m_resume->WaitForResult())
		{
			case ScopedCore_BlockingResume:
				if (m_sync)
					m_sync->ClearResult();
				core.AllowResume();
				break;

			case ScopedCore_NonblockingResume:
				m_sync = NULL;
				core.AllowResume();
				break;

			case ScopedCore_SkipResume:
				m_sync = NULL;
				break;
		}
	}
}


void SysExecEvent_CoreThreadClose::InvokeEvent()
{
	ScopedCoreThreadClose closed_core;
	_post_and_wait(closed_core);
	closed_core.AllowResume();
}


void SysExecEvent_CoreThreadPause::InvokeEvent()
{
	ScopedCoreThreadPause paused_core(m_systemsToTearDown);
	_post_and_wait(paused_core);
	paused_core.AllowResume();
}


// --------------------------------------------------------------------------------------
//  ScopedCoreThreadClose / ScopedCoreThreadPause
// --------------------------------------------------------------------------------------

static DeclareTls(bool) ScopedCore_IsPaused = false;
static DeclareTls(bool) ScopedCore_IsFullyClosed = false;

BaseScopedCoreThread::BaseScopedCoreThread()
{
	//AffinityAssert_AllowFrom_MainUI();

	m_allowResume = false;
	m_alreadyStopped = false;
	m_alreadyScoped = false;
}

BaseScopedCoreThread::~BaseScopedCoreThread() = default;

// Allows the object to resume execution upon object destruction.  Typically called as the last thing
// in the object's scope.  Any code prior to this call that causes exceptions will not resume the emulator,
// which is *typically* the intended behavior when errors occur.
void BaseScopedCoreThread::AllowResume()
{
	m_allowResume = true;
}

void BaseScopedCoreThread::DisallowResume()
{
	m_allowResume = false;
}

void BaseScopedCoreThread::DoResume()
{
	if (m_alreadyStopped)
		return;
	if (!GetSysExecutorThread().IsSelf())
	{
		//DbgCon.WriteLn("(ScopedCoreThreadPause) Threaded Scope Created!");
		m_sync_resume.PostResult(m_allowResume ? ScopedCore_NonblockingResume : ScopedCore_SkipResume);
		m_mtx_resume.Wait();
	}
	else
		CoreThread.Resume();
}

// Returns TRUE if the event is posted to the SysExecutor.
// Returns FALSE if the thread *is* the SysExecutor (no message is posted, calling code should
//  handle the code directly).
bool BaseScopedCoreThread::PostToSysExec(std::unique_ptr<BaseSysExecEvent_ScopedCore> msg)
{
	if (!msg || GetSysExecutorThread().IsSelf())
		return false;

	msg->SetSyncState(m_sync);
	msg->SetResumeStates(m_sync_resume, m_mtx_resume);

	GetSysExecutorThread().PostEvent(msg.release());
	m_sync.WaitForResult();
	m_sync.RethrowException();

	return true;
}

ScopedCoreThreadClose::ScopedCoreThreadClose()
{
	if (ScopedCore_IsFullyClosed)
	{
		// tracks if we're already in scope or not.
		m_alreadyScoped = true;
		return;
	}

	if (!PostToSysExec(std::make_unique<SysExecEvent_CoreThreadClose>()))
	{
		m_alreadyStopped = CoreThread.IsClosed();
		if (!m_alreadyStopped)
			CoreThread.Suspend();
	}

	ScopedCore_IsFullyClosed = true;
}

ScopedCoreThreadClose::~ScopedCoreThreadClose()
{
	if (m_alreadyScoped)
		return;
	try
	{
		_parent::DoResume();
		ScopedCore_IsFullyClosed = false;
	}
	DESTRUCTOR_CATCHALL
}

ScopedCoreThreadPause::ScopedCoreThreadPause(SystemsMask systemsToTearDown)
{
	if (ScopedCore_IsFullyClosed || ScopedCore_IsPaused)
	{
		// tracks if we're already in scope or not.
		m_alreadyScoped = true;
		return;
	}

	if (!PostToSysExec(std::make_unique<SysExecEvent_CoreThreadPause>(systemsToTearDown)))
	{
		m_alreadyStopped = CoreThread.IsPaused();
		if (!m_alreadyStopped)
			CoreThread.Pause(systemsToTearDown);
	}

	ScopedCore_IsPaused = true;
}

ScopedCoreThreadPause::~ScopedCoreThreadPause()
{
	if (m_alreadyScoped)
		return;
	try
	{
		_parent::DoResume();
		ScopedCore_IsPaused = false;
	}
	DESTRUCTOR_CATCHALL
}

ScopedCoreThreadPopup::ScopedCoreThreadPopup()
	: m_scoped_core(std::unique_ptr<BaseScopedCoreThread>(new ScopedCoreThreadPause()))
{
};

void ScopedCoreThreadPopup::AllowResume()
{
	if (m_scoped_core)
		m_scoped_core->AllowResume();
}

void ScopedCoreThreadPopup::DisallowResume()
{
	if (m_scoped_core)
		m_scoped_core->DisallowResume();
}
