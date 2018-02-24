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
#include "GSFrame.h"
#include "ApplyState.h"
#include "ConsoleLogger.h"

#include "AppAccelerators.h"
#include "AppSaveStates.h"

#include "Utilities/HashMap.h"

// Various includes needed for dumping...
#include "GS.h"
#include "Dump.h"
#include "DebugTools/Debug.h"
#include "R3000A.h"

// renderswitch - tells GSdx to go into dx9 sw if "renderswitch" is set.
bool renderswitch = false;

extern bool switchAR;

static int g_Pcsx2Recording = 0; // true 1 if recording video and sound


KeyAcceleratorCode::KeyAcceleratorCode( const wxKeyEvent& evt )
{
	val32 = 0;

	keycode = evt.GetKeyCode();

	if( evt.AltDown() ) Alt();
	if( evt.CmdDown() ) Cmd();
	if( evt.ShiftDown() ) Shift();
}

wxString KeyAcceleratorCode::ToString() const
{
	// Let's use wx's string formatter:

	return wxAcceleratorEntry(
		(cmd ? wxACCEL_CMD : 0) |
		(shift ? wxACCEL_SHIFT : 0) |
		(alt ? wxACCEL_ALT : 0),
		keycode
	).ToString();
}

LimiterModeType g_LimiterMode = Limit_Nominal;

namespace Implementations
{
	void Frameskip_Toggle()
	{
		g_Conf->EmuOptions.GS.FrameSkipEnable = !g_Conf->EmuOptions.GS.FrameSkipEnable;
		SetGSConfig().FrameSkipEnable = g_Conf->EmuOptions.GS.FrameSkipEnable;

		if( EmuConfig.GS.FrameSkipEnable ) {
			OSDlog( Color_StrongRed, true, "(FrameSkipping) Enabled." );
			OSDlog( Color_StrongRed, true, "  FrameDraws=%d, FrameSkips=%d", g_Conf->EmuOptions.GS.FramesToDraw, g_Conf->EmuOptions.GS.FramesToSkip );
		} else {
			OSDlog( Color_StrongRed, true, "(FrameSkipping) Disabled." );
		}
	}

	void Framelimiter_TurboToggle()
	{
		ScopedCoreThreadPause pauser;

		if( !g_Conf->EmuOptions.GS.FrameLimitEnable )
		{
			g_Conf->EmuOptions.GS.FrameLimitEnable = true;
			g_LimiterMode = Limit_Turbo;
			OSDlog( Color_StrongRed, true, "(FrameLimiter) Turbo + FrameLimit ENABLED." );
			g_Conf->EmuOptions.GS.FrameSkipEnable = !!g_Conf->Framerate.SkipOnTurbo;
		}
		else if( g_LimiterMode == Limit_Turbo )
		{
			g_LimiterMode = Limit_Nominal;

			if ( g_Conf->Framerate.SkipOnLimit)
			{
				OSDlog( Color_StrongRed, true,  "(FrameLimiter) Turbo DISABLED. Frameskip ENABLED" );
				g_Conf->EmuOptions.GS.FrameSkipEnable = true;
			}
			else
			{
				OSDlog( Color_StrongRed, true, "(FrameLimiter) Turbo DISABLED." );
				g_Conf->EmuOptions.GS.FrameSkipEnable = false;
			}
		}
		else
		{
			g_LimiterMode = Limit_Turbo;

			if ( g_Conf->Framerate.SkipOnTurbo)
			{
				OSDlog( Color_StrongRed, true, "(FrameLimiter) Turbo + Frameskip ENABLED." );
				g_Conf->EmuOptions.GS.FrameSkipEnable = true;
			}
			else
			{
				OSDlog( Color_StrongRed, true, "(FrameLimiter) Turbo ENABLED." );
				g_Conf->EmuOptions.GS.FrameSkipEnable = false;
			}
		}

		gsUpdateFrequency(g_Conf->EmuOptions);

		pauser.AllowResume();
	}

	void Framelimiter_SlomoToggle()
	{
		// Slow motion auto-enables the framelimiter even if it's disabled.
		// This seems like desirable and expected behavior.

		// FIXME: Inconsistent use of g_Conf->EmuOptions vs. EmuConfig.  Should figure
		// out a better consistency approach... -air

		ScopedCoreThreadPause pauser;
		if( g_LimiterMode == Limit_Slomo )
		{
			g_LimiterMode = Limit_Nominal;
			OSDlog( Color_StrongRed, true, "(FrameLimiter) SlowMotion DISABLED." );
		}
		else
		{
			g_LimiterMode = Limit_Slomo;
			OSDlog( Color_StrongRed, true, "(FrameLimiter) SlowMotion ENABLED." );
			g_Conf->EmuOptions.GS.FrameLimitEnable = true;
		}

		gsUpdateFrequency(g_Conf->EmuOptions);

		pauser.AllowResume();
	}

	void Framelimiter_MasterToggle()
	{
		ScopedCoreThreadPause pauser;
		g_Conf->EmuOptions.GS.FrameLimitEnable = !g_Conf->EmuOptions.GS.FrameLimitEnable;
		OSDlog( Color_StrongRed, true, "(FrameLimiter) %s.", g_Conf->EmuOptions.GS.FrameLimitEnable ? "ENABLED" : "DISABLED" );

		// Turbo/Slowmo don't make sense when framelimiter is toggled
		g_LimiterMode = Limit_Nominal;

		pauser.AllowResume();
	}

	void UpdateImagePosition()
	{
		//AppApplySettings() would have been nicer, since it also immidiately affects the GUI (if open).
		//However, the events sequence it generates also "depresses" Shift/CTRL/etc, so consecutive zoom with CTRL down breaks.
		//Since zoom only affects the window viewport anyway, we can live with directly calling it.
		if (GSFrame* gsFrame = wxGetApp().GetGsFramePtr())
			if (GSPanel* woot = gsFrame->GetViewport())
				woot->DoResize();
	}

	void GSwindow_CycleAspectRatio()
	{
		AspectRatioType& art = g_Conf->GSWindow.AspectRatio;
		const char *arts = "Not modified";
		if (art == AspectRatio_Stretch && switchAR) //avoids a double 4:3 when coming from FMV aspect ratio switch
			art = AspectRatio_4_3;
		switch( art )
		{
			case AspectRatio_Stretch:	art = AspectRatio_4_3; arts = "4:3"; break;
			case AspectRatio_4_3:		art = AspectRatio_16_9; arts = "16:9"; break;
			case AspectRatio_16_9:		art = AspectRatio_Stretch; arts = "Stretch"; break;
			default: break;
		}

		OSDlog(Color_StrongBlue, true, "(GSwindow) Aspect ratio: %s", arts);
		UpdateImagePosition();
	}

	void SetOffset(float x, float y)
	{
		g_Conf->GSWindow.OffsetX = x;
		g_Conf->GSWindow.OffsetY = y;
		OSDlog( Color_StrongBlue, true, "(GSwindow) Offset: x=%f, y=%f", x,y);

		UpdateImagePosition();

	}

	void GSwindow_OffsetYplus(){
		SetOffset(g_Conf->GSWindow.OffsetX.ToFloat(), g_Conf->GSWindow.OffsetY.ToFloat()+1);
	}

	void GSwindow_OffsetYminus(){
		SetOffset(g_Conf->GSWindow.OffsetX.ToFloat(), g_Conf->GSWindow.OffsetY.ToFloat()-1);
	}

	void GSwindow_OffsetXplus(){
		SetOffset(g_Conf->GSWindow.OffsetX.ToFloat()+1, g_Conf->GSWindow.OffsetY.ToFloat());
	}

	void GSwindow_OffsetXminus(){
		SetOffset(g_Conf->GSWindow.OffsetX.ToFloat()-1, g_Conf->GSWindow.OffsetY.ToFloat());
	}

	void GSwindow_OffsetReset(){
		SetOffset(0,0);
	}

	void SetZoomY(float zoom)
	{
		if( zoom <= 0 )
			return;
		g_Conf->GSWindow.StretchY = zoom;
		OSDlog( Color_StrongBlue, true, "(GSwindow) Vertical stretch: %f", zoom);

		UpdateImagePosition();
	}

	void GSwindow_ZoomInY()
	{
		SetZoomY( g_Conf->GSWindow.StretchY.ToFloat()+1 );
	}
	void GSwindow_ZoomOutY()
	{
		SetZoomY( g_Conf->GSWindow.StretchY.ToFloat()-1 );
	}
	void GSwindow_ZoomResetY()
	{
		SetZoomY( 100 );
	}

	void SetZoom(float zoom)
	{
		if( zoom < 0 )
			return;
		g_Conf->GSWindow.Zoom = zoom;
		
		if ( zoom == 0 )
			OSDlog( Color_StrongBlue, true, "(GSwindow) Zoom: 0 (auto, no black bars)");
		else
			OSDlog( Color_StrongBlue, true, "(GSwindow) Zoom: %f", zoom);
		
		UpdateImagePosition();
	}


	void GSwindow_ZoomIn()
	{
		float z = g_Conf->GSWindow.Zoom.ToFloat();
		if( z==0 ) z = 100;
		z++;
		SetZoom( z );
	}
	void GSwindow_ZoomOut()
	{
		float z = g_Conf->GSWindow.Zoom.ToFloat();
		if( z==0 ) z = 100;
		z--;
		SetZoom( z );
	}
	void GSwindow_ZoomToggle()
	{
		float z = g_Conf->GSWindow.Zoom.ToFloat();
		if( z==100 )	z = 0;
		else			z = 100;

		SetZoom( z );
	}


	void Sys_Suspend()
	{
		GSFrame* gsframe = wxGetApp().GetGsFramePtr();
		if (gsframe && gsframe->IsShown() && gsframe->IsFullScreen()) {
			// On some cases, probably due to driver bugs, if we don't exit fullscreen then
			// the content stays on screen. Try to prevent that by first exiting fullscreen,
			// but don't update the internal PCSX2 state/config, and PCSX2 will restore
			// fullscreen correctly when emulation resumes according to its state/config.
			// This is similar to what LilyPad's "Safe fullscreen exit on escape" hack does,
			// and thus hopefully makes LilyPad's hack redundant.
			gsframe->ShowFullScreen(false, false);
		}

		CoreThread.Suspend();

		gsframe = wxGetApp().GetGsFramePtr(); // just in case suspend removes this window
		if (gsframe && !wxGetApp().HasGUI() && g_Conf->GSWindow.CloseOnEsc) {
			// When we run with --nogui, PCSX2 only knows to exit when the gs window closes.
			// However, by default suspend just hides the gs window, so PCSX2 will not exit
			// and there will also be no way to exit it even if no windows are left.
			// If the gs window is not set to close on suspend, then the user can still
			// close it with the X button, which PCSX2 will recognize and exit.
			// So if we're set to close on esc and nogui:
			// If the user didn't specify --noguiprompt - exit immediately.
			// else prompt to either exit or abort the suspend.
			if (!wxGetApp().ExitPromptWithNoGUI() // configured to exit without a dialog
				|| (wxOK == wxMessageBox(_("Exit PCSX2?"), // or confirmed exit at the dialog
										 L"PCSX2",
										 wxICON_WARNING | wxOK | wxCANCEL)))
			{
				// Pcsx2App knows to exit if no gui and the GS window closes.
				gsframe->Close();
				return;
			}
			else
			{
				// aborting suspend request
				// Note: if we didn't want to suspend emulation for this confirmation dialog,
				// and if LilyPad has "Safe fullscreen exit on ESC", then pressing ESC would
				// have exited fullscreen without PCSX2 knowing about it, and since it's not
				// suspended it would not re-init the fullscreen state if the confirmation is
				// aborted. On such case we'd have needed to set the gsframe fullscreen mode
				// here according to g_Conf->GSWindow.IsFullscreen
				CoreThread.Resume();
				return;
			}
		}

		if (g_Conf->GSWindow.CloseOnEsc)
			sMainFrame.SetFocus();
	}

	void Sys_Resume()
	{
		CoreThread.Resume();
	}

	void Sys_SuspendResume()
	{
		if (CoreThread.HasPendingStateChangeRequest())
			return;

		if (CoreThread.IsPaused())
			Sys_Resume();
		else
			Sys_Suspend();
	}

	void Sys_TakeSnapshot()
	{
		GSmakeSnapshot( g_Conf->Folders.Snapshots.ToAscii() );
	}

	void Sys_RenderToggle()
	{
		ScopedCoreThreadPause paused_core( new SysExecEvent_SaveSinglePlugin(PluginId_GS) );
		renderswitch = !renderswitch;
		paused_core.AllowResume();
	}

	void Sys_LoggingToggle()
	{
		// There's likely a better way to implement this, but this seemed useful.
		// I might add turning EE, VU0, and VU1 recs on and off by hotkey at some point, too.
		// --arcum42

		// FIXME: Some of the trace logs will require recompiler resets to be activated properly.
#ifdef PCSX2_DEVBUILD		
		SetTraceConfig().Enabled = !EmuConfig.Trace.Enabled;
		Console.WriteLn(EmuConfig.Trace.Enabled ? "Logging Enabled." : "Logging Disabled.");
#endif
	}

	void Sys_FreezeGS()
	{
		// fixme : fix up gsstate mess and make it mtgs compatible -- air
#ifdef _STGS_GSSTATE_CODE
		wxString Text;
		if( strgametitle[0] != 0 )
		{
			// only take the first two words
			wxString gsText;

			wxStringTokenizer parts( strgametitle, L" " );

			wxString name( parts.GetNextToken() );	// first part
			wxString part2( parts.GetNextToken() );

			if( !!part2 )
				name += L"_" + part2;

			gsText.Printf( L"%s.%d.gs", WX_STR(name), StatesC );
			Text = Path::Combine( g_Conf->Folders.Savestates, gsText );
		}
		else
		{
			Text = GetGSStateFilename();
		}
#endif

	}

	void Sys_RecordingToggle()
	{
		ScopedCoreThreadPause paused_core;
		paused_core.AllowResume();

		g_Pcsx2Recording ^= 1;

		GetMTGS().WaitGS();		// make sure GS is in sync with the audio stream when we start.
		if (g_Pcsx2Recording) {
			// start recording

			// make the recording setup dialog[s] pseudo-modal also for the main PCSX2 window
			// (the GSdx dialog is already properly modal for the GS window)
			bool needsMainFrameEnable = false;
			if (GetMainFramePtr() && GetMainFramePtr()->IsEnabled()) {
				needsMainFrameEnable = true;
				GetMainFramePtr()->Disable();
			}

			if (GSsetupRecording) {
				// GSsetupRecording can be aborted/canceled by the user. Don't go on to record the audio if that happens.
				if (GSsetupRecording(g_Pcsx2Recording, NULL)) {
					if (SPU2setupRecording) SPU2setupRecording(g_Pcsx2Recording, NULL);
				} else {
					// recording dialog canceled by the user. align our state
					g_Pcsx2Recording ^= 1;
				}
			} else {
				// the GS doesn't support recording.
				if (SPU2setupRecording) SPU2setupRecording(g_Pcsx2Recording, NULL);
			}

			if (GetMainFramePtr() && needsMainFrameEnable)
				GetMainFramePtr()->Enable();

		} else {
			// stop recording
			if (GSsetupRecording) GSsetupRecording(g_Pcsx2Recording, NULL);
			if (SPU2setupRecording) SPU2setupRecording(g_Pcsx2Recording, NULL);
		}
	}

	void Cpu_DumpRegisters()
	{
#ifdef PCSX2_DEVBUILD
		iDumpRegisters(cpuRegs.pc, 0);
		Console.Warning("hardware registers dumped EE:%x, IOP:%x\n", cpuRegs.pc, psxRegs.pc);
#endif
	}

	void FullscreenToggle()
	{
		if( GSFrame* gsframe = wxGetApp().GetGsFramePtr() )
			gsframe->ShowFullScreen( !gsframe->IsFullScreen() );
	}
}

// --------------------------------------------------------------------------------------
//  CommandDeclarations table
// --------------------------------------------------------------------------------------
// This is our manualized introspection/reflection table.  In a cool language like C# we'd
// have just grabbed this info from enumerating the members of a class and assigning
// properties to each method in the class.  But since this is C++, we have to do the the
// goold old fashioned way! :)

static const GlobalCommandDescriptor CommandDeclarations[] =
{
	{	"States_FreezeCurrentSlot",
		States_FreezeCurrentSlot,
		pxL( "Save state" ),
		pxL( "Saves the virtual machine state to the current slot." ),
		false,
	},

	{	"States_DefrostCurrentSlot",
		States_DefrostCurrentSlot,
		pxL( "Load state" ),
		pxL( "Loads a virtual machine state from the current slot." ),
		false,
	},

	{	"States_DefrostCurrentSlotBackup",
		States_DefrostCurrentSlotBackup,
		pxL( "Load State Backup" ),
		pxL( "Loads virtual machine state backup for current slot." ),
		false,
	},

	{	"States_CycleSlotForward",
		States_CycleSlotForward,
		pxL( "Cycle to next slot" ),
		pxL( "Cycles the current save slot in +1 fashion!" ),
		false,
	},

	{	"States_CycleSlotBackward",
		States_CycleSlotBackward,
		pxL( "Cycle to prev slot" ),
		pxL( "Cycles the current save slot in -1 fashion!" ),
		false,
	},

	{	"Frameskip_Toggle",
		Implementations::Frameskip_Toggle,
		NULL,
		NULL,
		false,
	},

	{	"Framelimiter_TurboToggle",
		Implementations::Framelimiter_TurboToggle,
		NULL,
		NULL,
		false,
	},

	{	"Framelimiter_SlomoToggle",
		Implementations::Framelimiter_SlomoToggle,
		NULL,
		NULL,
		false,
	},

	{	"Framelimiter_MasterToggle",
		Implementations::Framelimiter_MasterToggle,
		NULL,
		NULL,
		true,
	},

	{	"GSwindow_CycleAspectRatio",
		Implementations::GSwindow_CycleAspectRatio,
		NULL,
		NULL,
		true,
	},

	{	"GSwindow_ZoomIn",
		Implementations::GSwindow_ZoomIn,
		NULL,
		NULL,
		false,
	},

	{	"GSwindow_ZoomOut",
		Implementations::GSwindow_ZoomOut,
		NULL,
		NULL,
		false,
	},

	{	"GSwindow_ZoomToggle",
		Implementations::GSwindow_ZoomToggle,
		NULL,
		NULL,
		false,
	},

	{	"GSwindow_ZoomInY"     , Implementations::GSwindow_ZoomInY     , NULL, NULL, false},
	{	"GSwindow_ZoomOutY"    , Implementations::GSwindow_ZoomOutY    , NULL, NULL, false},
	{	"GSwindow_ZoomResetY"  , Implementations::GSwindow_ZoomResetY  , NULL, NULL, false},

	{	"GSwindow_OffsetYminus", Implementations::GSwindow_OffsetYminus, NULL, NULL, false},
	{	"GSwindow_OffsetYplus" , Implementations::GSwindow_OffsetYplus , NULL, NULL, false},
	{	"GSwindow_OffsetXminus", Implementations::GSwindow_OffsetXminus, NULL, NULL, false},
	{	"GSwindow_OffsetXplus" , Implementations::GSwindow_OffsetXplus , NULL, NULL, false},
	{	"GSwindow_OffsetReset" , Implementations::GSwindow_OffsetReset , NULL, NULL, false},

	{	"Sys_SuspendResume",
		Implementations::Sys_SuspendResume,
		NULL,
		NULL,
		false,
	},

	{	"Sys_TakeSnapshot",
		Implementations::Sys_TakeSnapshot,
		NULL,
		NULL,
		false,
	},

	{	"Sys_RenderswitchToggle",
		Implementations::Sys_RenderToggle,
		NULL,
		NULL,
		false,
	},

	{	"Sys_LoggingToggle",
		Implementations::Sys_LoggingToggle,
		NULL,
		NULL,
		false,
	},

	{	"Sys_FreezeGS",
		Implementations::Sys_FreezeGS,
		NULL,
		NULL,
		false,
	},
	{	"Sys_RecordingToggle",
		Implementations::Sys_RecordingToggle,
		NULL,
		NULL,
		false,
	},

	{	"FullscreenToggle",
		Implementations::FullscreenToggle,
		NULL,
		NULL,
		false,
	},

	// Command Declarations terminator:
	// (must always be last in list!!)
	{ NULL }
};

void AcceleratorDictionary::Map( const KeyAcceleratorCode& _acode, const char *searchfor )
{
	// Search override mapping at ini file
	KeyAcceleratorCode acode = _acode;
	wxString overrideStr;
	wxAcceleratorEntry codeParser;	//Provides string parsing capabilities
	wxFileConfig cfg(L"", L"", L"" , GetUiKeysFilename(), wxCONFIG_USE_GLOBAL_FILE );
	if( cfg.Read( wxString::FromUTF8(searchfor), &overrideStr) )
	{
		// needs a '\t' prefix (originally used for wxMenu accelerators parsing)...
		if (codeParser.FromString(wxString(L"\t") + overrideStr))
		{
			// ini file contains alternative parsable key combination for current 'searchfor'.
			acode = codeParser;
			if (acode.keycode >= 'A' && acode.keycode <= 'Z') {
				// Note that this needs to match the key event codes at Pcsx2App::PadKeyDispatch
				// Our canonical representation is the char code (at lower case if
				// applicable) with a separate modifier indicator, including shift.
				// The parser deviates from this by setting the keycode to upper case if
				// modifiers are used with plain letters. Luckily, it sets the modifiers
				// correctly, including shift (for letters without modifiers it parses lower case).
				// So we only need to change upper case letters to lower case.
				acode.keycode += 'a' - 'A';
			}
			if (_acode.ToString() != acode.ToString()) {
				Console.WriteLn(Color_StrongGreen, L"Overriding '%s': assigning %s (instead of %s)",
					WX_STR(fromUTF8(searchfor)), WX_STR(acode.ToString()), WX_STR(_acode.ToString()));
			}
		}
		else
		{
			Console.Error(L"Error overriding KB shortcut for '%s': can't understand '%s'",
						  WX_STR(fromUTF8(searchfor)), WX_STR(overrideStr));
		}
	}
	// End of overrides section

	const GlobalCommandDescriptor* result = NULL;

	std::unordered_map<int, const GlobalCommandDescriptor*>::const_iterator iter(find(acode.val32));
	if (iter != end())
		result = iter->second;

	if( result != NULL )
	{
		Console.Warning(
			L"Kbd Accelerator '%s' is mapped multiple times.\n"
			L"\t'Command %s' is being replaced by '%s'",
			WX_STR(acode.ToString()), WX_STR(fromUTF8( result->Id )), WX_STR(fromUTF8( searchfor ))
		);
	}

	std::unordered_map<std::string, const GlobalCommandDescriptor*>::const_iterator acceleratorIter(wxGetApp().GlobalCommands->find(searchfor));

	if (acceleratorIter != wxGetApp().GlobalCommands->end())
		result = acceleratorIter->second;

	if( result == NULL )
	{
		Console.Warning( L"Kbd Accelerator '%s' is mapped to unknown command '%s'",
			WX_STR(acode.ToString()), WX_STR(fromUTF8( searchfor ))
		);
	}
	else
	{
		if (!strcmp("Sys_TakeSnapshot", searchfor)) {
			// Sys_TakeSnapshot is special in a bad way. On its own it creates a screenshot
			// but GSdx also checks whether shift or ctrl are held down, and for each of
			// them it does a different thing (gs dumps). So we need to map a shortcut and
			// also the same shortcut with shift and the same with ctrl to the same function.
			// So make sure the shortcut doesn't include shift or ctrl, and then add two more
			// which are derived from it.
			// Also, looking at the GSdx code, it seems that it never cares about both shift
			// and ctrl held together, but PCSX2 traditionally mapped f8, shift-f8 and ctrl-shift-f8
			// to Sys_TakeSnapshot, so let's not change it - we'll keep adding only shift and
			// ctrl-shift to the base shortcut.
			if (acode.cmd || acode.shift) {
				Console.Error(L"Cannot map %s to Sys_TakeSnapshot - must not include Shift or Ctrl - these modifiers will be added automatically.",
					WX_STR(acode.ToString()));
			}
			else {
				KeyAcceleratorCode shifted(acode); shifted.Shift();
				KeyAcceleratorCode controlledShifted(shifted); controlledShifted.Cmd();
				operator[](acode.val32) = result;
				operator[](shifted.val32) = result;
				operator[](controlledShifted.val32) = result;

				if (_acode.val32 != acode.val32) { // overriding default
					Console.WriteLn(Color_Green, L"Sys_TakeSnapshot: automatically mapping also %s and %s",
						WX_STR(shifted.ToString()),
						WX_STR(controlledShifted.ToString())
						);
				}
			}
		}
		else {
			operator[](acode.val32) = result;
		}
	}
}

void Pcsx2App::BuildCommandHash()
{
	if( !GlobalCommands ) GlobalCommands = std::unique_ptr<CommandDictionary>(new CommandDictionary);

	const GlobalCommandDescriptor* curcmd = CommandDeclarations;
	while( curcmd->Invoke != NULL )
	{
		(*GlobalCommands)[curcmd->Id] = curcmd;
		curcmd++;
	}
}

void Pcsx2App::InitDefaultGlobalAccelerators()
{
	typedef KeyAcceleratorCode AAC;

	if( !GlobalAccels ) GlobalAccels = std::unique_ptr<AcceleratorDictionary>(new AcceleratorDictionary);

	// Why do we even have those here? all of them seem to be overridden
	// by GSPanel::m_Accels ( GSPanel::InitDefaultAccelerators() )

	GlobalAccels->Map( AAC( WXK_F1 ),			"States_FreezeCurrentSlot" );
	GlobalAccels->Map( AAC( WXK_F3 ),			"States_DefrostCurrentSlot" );
	GlobalAccels->Map( AAC( WXK_F2 ),			"States_CycleSlotForward" );
	GlobalAccels->Map( AAC( WXK_F2 ).Shift(),	"States_CycleSlotBackward" );

	GlobalAccels->Map( AAC( WXK_F4 ),			"Framelimiter_MasterToggle");
	GlobalAccels->Map( AAC( WXK_F4 ).Shift(),	"Frameskip_Toggle");

	/*GlobalAccels->Map( AAC( WXK_ESCAPE ),		"Sys_Suspend");
	GlobalAccels->Map( AAC( WXK_F8 ),			"Sys_TakeSnapshot");
	GlobalAccels->Map( AAC( WXK_F8 ).Shift(),	"Sys_TakeSnapshot");
	GlobalAccels->Map( AAC( WXK_F8 ).Shift().Cmd(),"Sys_TakeSnapshot");
	GlobalAccels->Map( AAC( WXK_F9 ),			"Sys_RenderswitchToggle");

	GlobalAccels->Map( AAC( WXK_F10 ),			"Sys_LoggingToggle");
	GlobalAccels->Map( AAC( WXK_F11 ),			"Sys_FreezeGS");
	GlobalAccels->Map( AAC( WXK_F12 ),			"Sys_RecordingToggle");

	GlobalAccels->Map( AAC( WXK_RETURN ).Alt(),	"FullscreenToggle" );*/
}
