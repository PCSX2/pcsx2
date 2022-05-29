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
#include "Host.h"
#include "ApplyState.h"
#include "ConsoleLogger.h"

#include "AppAccelerators.h"
#include "AppSaveStates.h"

#include "Recording/InputRecordingControls.h"
#include "Recording/InputRecording.h"

// Various includes needed for dumping...
#include "GS.h"
#include "Dump.h"
#include "DebugTools/Debug.h"
#include "R3000A.h"
#include "SPU2/spu2.h"
#include "gui/Dialogs/ModalPopups.h"

static bool g_Pcsx2Recording = false; // true if recording video and sound


KeyAcceleratorCode::KeyAcceleratorCode(const wxKeyEvent& evt)
{
	val32 = 0;

	keycode = evt.GetKeyCode();

	if (evt.AltDown())
		Alt();
	if (evt.CmdDown())
		Cmd();
	if (evt.ShiftDown())
		Shift();
}

wxString KeyAcceleratorCode::ToString() const
{
	// Let's use wx's string formatter:

	return wxAcceleratorEntry(
			   (cmd ? wxACCEL_CMD : 0) |
				   (shift ? wxACCEL_SHIFT : 0) |
				   (alt ? wxACCEL_ALT : 0),
			   keycode)
		.ToString();
}

namespace Implementations
{
	void Frameskip_Toggle()
	{
		g_Conf->EmuOptions.GS.FrameSkipEnable = !g_Conf->EmuOptions.GS.FrameSkipEnable;
		EmuConfig.GS.FrameSkipEnable = g_Conf->EmuOptions.GS.FrameSkipEnable;

		if (EmuConfig.GS.FrameSkipEnable)
		{
			Host::AddKeyedFormattedOSDMessage("FrameSkipping", 2.0f, "Frameskip ENABLED. FrameDraws=%d, FrameSkips=%d", g_Conf->EmuOptions.GS.FramesToDraw, g_Conf->EmuOptions.GS.FramesToSkip);
		}
		else
		{
			Host::AddKeyedOSDMessage("FrameSkipping", "Frameskip DISABLED.");
		}
	}

	void Framelimiter_TurboToggle()
	{
		ScopedCoreThreadPause pauser;

		if (!g_Conf->EmuOptions.GS.FrameLimitEnable)
		{
			g_Conf->EmuOptions.GS.FrameLimitEnable = true;
			g_Conf->EmuOptions.LimiterMode = LimiterModeType::Turbo;
			Host::AddKeyedOSDMessage("FrameLimiter", "Turbo + Frame limiter ENABLED.");
			g_Conf->EmuOptions.GS.FrameSkipEnable = !!EmuConfig.Framerate.SkipOnTurbo;
		}
		else if (g_Conf->EmuOptions.LimiterMode == LimiterModeType::Turbo)
		{
			g_Conf->EmuOptions.LimiterMode = LimiterModeType::Nominal;

			if (g_Conf->EmuOptions.Framerate.SkipOnLimit)
			{
				Host::AddKeyedOSDMessage("FrameLimiter", "Turbo DISABLED.");
				Host::AddKeyedOSDMessage("FrameSkipping", "Frameskip ENABLED.");
				g_Conf->EmuOptions.GS.FrameSkipEnable = true;
			}
			else
			{
				Host::AddKeyedOSDMessage("FrameLimiter", "Turbo DISABLED.");
				g_Conf->EmuOptions.GS.FrameSkipEnable = false;
			}
		}
		else
		{
			g_Conf->EmuOptions.LimiterMode = LimiterModeType::Turbo;

			if (g_Conf->EmuOptions.Framerate.SkipOnTurbo)
			{
				Host::AddKeyedOSDMessage("FrameLimiter", "Turbo ENABLED.");
				Host::AddKeyedOSDMessage("FrameSkipping", "Frameskip ENABLED.");
				g_Conf->EmuOptions.GS.FrameSkipEnable = true;
			}
			else
			{
				Host::AddKeyedOSDMessage("FrameLimiter", "Turbo ENABLED.");
				g_Conf->EmuOptions.GS.FrameSkipEnable = false;
			}
		}

		pauser.AllowResume();
	}

	void Framelimiter_SlomoToggle()
	{
		// Slow motion auto-enables the framelimiter even if it's disabled.
		// This seems like desirable and expected behavior.

		// FIXME: Inconsistent use of g_Conf->EmuOptions vs. EmuConfig.  Should figure
		// out a better consistency approach... -air

		ScopedCoreThreadPause pauser;
		if (g_Conf->EmuOptions.LimiterMode == LimiterModeType::Slomo)
		{
			g_Conf->EmuOptions.LimiterMode = LimiterModeType::Nominal;
			Host::AddKeyedOSDMessage("FrameLimiter", "Slow motion DISABLED.");
		}
		else
		{
			g_Conf->EmuOptions.LimiterMode = LimiterModeType::Slomo;
			Host::AddKeyedOSDMessage("FrameLimiter", "Slow motion ENABLED.");
			g_Conf->EmuOptions.GS.FrameLimitEnable = true;
		}

		pauser.AllowResume();
	}

	void Framelimiter_MasterToggle()
	{
		ScopedCoreThreadPause pauser;
		g_Conf->EmuOptions.GS.FrameLimitEnable = !g_Conf->EmuOptions.GS.FrameLimitEnable;
		Host::AddKeyedFormattedOSDMessage("FrameLimiter", 2.0f, "Frame limiter %s.", g_Conf->EmuOptions.GS.FrameLimitEnable ? "ENABLED" : "DISABLED");

		// Turbo/Slowmo don't make sense when framelimiter is toggled
		g_Conf->EmuOptions.LimiterMode = LimiterModeType::Nominal;

		pauser.AllowResume();
	}

	void GSwindow_CycleAspectRatio()
	{
		AspectRatioType& art = EmuConfig.CurrentAspectRatio;
		const char* arts = "Not modified";
		switch (art)
		{
			case AspectRatioType::Stretch:
				art = AspectRatioType::RAuto4_3_3_2;
				arts = "Auto 4:3/3:2";
				break;
			case AspectRatioType::RAuto4_3_3_2:
				art = AspectRatioType::R4_3;
				arts = "4:3";
				break;
			case AspectRatioType::R4_3:
				art = AspectRatioType::R16_9;
				arts = "16:9";
				break;
			case AspectRatioType::R16_9:
				art = AspectRatioType::Stretch;
				arts = "Stretch";
				break;
			default:
				break;
		}

		// Sync the mode with the settings. This is kinda silly, since they won't be
		// saved until shutdown, but it matches the behavior pre-settings-move.
		g_Conf->EmuOptions.GS.AspectRatio = art;

		// Prevent GS reopening for the setting change.
		EmuConfig.GS.AspectRatio = art;

		Host::AddKeyedFormattedOSDMessage("AspectRatio", 2.0f, "Aspect ratio: %s", arts);
	}

	// NOTE: The settings below are super janky and race the GS thread when updating.
	// But because they don't go through the proper settings update procedure, it's necessary to avoid reopening GS.
	void SetOffset(float x, float y)
	{
		g_Conf->EmuOptions.GS.OffsetX = x;
		g_Conf->EmuOptions.GS.OffsetY = y;
		EmuConfig.GS.OffsetX = x;
		EmuConfig.GS.OffsetY = y;
		GSConfig.OffsetX = x;
		GSConfig.OffsetY = y;
		Host::AddKeyedFormattedOSDMessage("WindowOffset", 2.0f, "Offset: x=%f, y=%f", x, y);
	}

	void GSwindow_OffsetYplus()
	{
		SetOffset(EmuConfig.GS.OffsetX, EmuConfig.GS.OffsetY + 1);
	}

	void GSwindow_OffsetYminus()
	{
		SetOffset(EmuConfig.GS.OffsetX, EmuConfig.GS.OffsetY - 1);
	}

	void GSwindow_OffsetXplus()
	{
		SetOffset(EmuConfig.GS.OffsetX + 1, EmuConfig.GS.OffsetY);
	}

	void GSwindow_OffsetXminus()
	{
		SetOffset(EmuConfig.GS.OffsetX - 1, EmuConfig.GS.OffsetY);
	}

	void GSwindow_OffsetReset()
	{
		SetOffset(0, 0);
	}

	void SetZoomY(float zoom)
	{
		if (zoom <= 0)
			return;
		g_Conf->EmuOptions.GS.StretchY = zoom;
		EmuConfig.GS.StretchY = zoom;
		GSConfig.StretchY = zoom;
		Host::AddKeyedFormattedOSDMessage("WindowVStretch", 2.0f, "Vertical stretch: %f", zoom);
	}

	void GSwindow_ZoomInY()
	{
		SetZoomY(EmuConfig.GS.StretchY + 1);
	}
	void GSwindow_ZoomOutY()
	{
		SetZoomY(EmuConfig.GS.StretchY - 1);
	}
	void GSwindow_ZoomResetY()
	{
		SetZoomY(100);
	}

	void SetZoom(float zoom)
	{
		if (zoom < 0)
			return;
		g_Conf->EmuOptions.GS.Zoom = zoom;
		EmuConfig.GS.Zoom = zoom;
		GSConfig.Zoom = zoom;

		if (zoom == 0)
			Host::AddKeyedOSDMessage("WindowZoom", "Zoom: 0 (auto, no black bars)");
		else
			Host::AddKeyedFormattedOSDMessage("WindowZoom", 2.0f, "Zoom: %f", zoom);
	}


	void GSwindow_ZoomIn()
	{
		float z = EmuConfig.GS.Zoom;
		if (z == 0)
			z = 100;
		z++;
		SetZoom(z);
	}
	void GSwindow_ZoomOut()
	{
		float z = EmuConfig.GS.Zoom;
		if (z == 0)
			z = 100;
		z--;
		SetZoom(z);
	}
	void GSwindow_ZoomToggle()
	{
		float z = EmuConfig.GS.Zoom;
		if (z == 100)
			z = 0;
		else
			z = 100;

		SetZoom(z);
	}


	void Sys_Suspend()
	{
		CoreThread.Suspend();

		GSFrame* gsframe = wxGetApp().GetGsFramePtr();
		if (gsframe && !wxGetApp().HasGUI() && g_Conf->GSWindow.CloseOnEsc)
		{
			// When we run with --nogui, PCSX2 only knows to exit when the gs window closes.
			// However, by default suspend just hides the gs window, so PCSX2 will not exit
			// and there will also be no way to exit it even if no windows are left.
			// If the gs window is not set to close on suspend, then the user can still
			// close it with the X button, which PCSX2 will recognize and exit.
			// So if we're set to close on esc and nogui:
			// If the user didn't specify --noguiprompt - exit immediately.
			// else prompt to either exit or abort the suspend.
			if (!wxGetApp().ExitPromptWithNoGUI()          // configured to exit without a dialog
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
				// then pressing ESC would have exited fullscreen without PCSX2 knowing about it,
				// and since it's not suspended it would not re-init the fullscreen state if the
				// confirmation is aborted. On such case we'd have needed to set the gsframe
				// fullscreen mode here according to g_Conf->GSWindow.IsFullscreen
				CoreThread.Resume();
				return;
			}
		}

		if (g_Conf->GSWindow.CloseOnEsc)
		{
			sMainFrame.SetFocus();
			// Disable recording controls that only make sense if the game is running
			sMainFrame.enableRecordingMenuItem(MenuId_Recording_FrameAdvance, false);
			sMainFrame.enableRecordingMenuItem(MenuId_Recording_TogglePause, false);
			sMainFrame.enableRecordingMenuItem(MenuId_Recording_ToggleRecordingMode, false);
		}
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
		GSQueueSnapshot(std::string(), 0);
	}

	void Sys_RenderToggle()
	{
		if (GSDump::isRunning)
			return;
		static bool reentrant = false;
		if (!reentrant)
		{
			reentrant = true;
			ScopedCoreThreadPause paused_core;
			GetMTGS().ToggleSoftwareRendering();
			paused_core.AllowResume();
			reentrant = false;
		}
	}

	void Sys_LoggingToggle()
	{
		// There's likely a better way to implement this, but this seemed useful.
		// I might add turning EE, VU0, and VU1 recs on and off by hotkey at some point, too.
		// --arcum42

		// FIXME: Some of the trace logs will require recompiler resets to be activated properly.
#ifdef PCSX2_DEVBUILD
		// This is touching the CPU thread's settings, it really shouldn't be, but it'll desync with the UI if we don't.
		g_Conf->EmuOptions.Trace.Enabled = !g_Conf->EmuOptions.Trace.Enabled;
		EmuConfig.Trace.Enabled = g_Conf->EmuOptions.Trace.Enabled;
		Console.WriteLn(g_Conf->EmuOptions.Trace.Enabled ? "Logging Enabled." : "Logging Disabled.");
#endif
	}

	void Sys_RecordingToggle()
	{
		ScopedCoreThreadPause paused_core;
		paused_core.AllowResume();

		if (wxGetApp().HasGUI())
		{
			sMainFrame.VideoCaptureToggle();
			return;
		}

		GetMTGS().WaitGS(); // make sure GS is in sync with the audio stream when we start.
		g_Pcsx2Recording = !g_Pcsx2Recording;
		if (g_Pcsx2Recording)
		{
			// start recording

			// make the recording setup dialog[s] pseudo-modal also for the main PCSX2 window
			// (the GS dialog is already properly modal for the GS window)
			if (GetMainFramePtr() && GetMainFramePtr()->IsEnabled())
				GetMainFramePtr()->Disable();

			// GSsetupRecording can be aborted/canceled by the user. Don't go on to record the audio if that happens.
			std::string filename;
			if (GSsetupRecording(filename))
			{
				if (g_Conf->AudioCapture.EnableAudio && !SPU2setupRecording(&filename))
				{
					GSendRecording();
					g_Pcsx2Recording = false;
				}
			}
			else // recording dialog canceled by the user. align our state
				g_Pcsx2Recording = false;
		}
		else
		{
			// stop recording
			GSendRecording();
			if (g_Conf->AudioCapture.EnableAudio)
				SPU2endRecording();
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
		if (GSFrame* gsframe = wxGetApp().GetGsFramePtr())
			gsframe->ShowFullScreen(!gsframe->IsFullScreen());
	}
	void FrameAdvance()
	{
		if (g_Conf->EmuOptions.EnableRecordingTools)
		{
			g_InputRecordingControls.FrameAdvance();
		}
	}

	void TogglePause()
	{
		if (g_Conf->EmuOptions.EnableRecordingTools)
		{
			g_InputRecordingControls.TogglePause();
		}
	}

	void InputRecordingModeToggle()
	{
		if (g_Conf->EmuOptions.EnableRecordingTools)
		{
			g_InputRecordingControls.RecordModeToggle();
		}
	}

	void GoToFirstFrame()
	{
		if (g_Conf->EmuOptions.EnableRecordingTools && g_InputRecording.IsActive())
		{
			// Assumes that gui is active, as you can't access recording options without it
			g_InputRecording.GoToFirstFrame(GetMainFramePtr());
		}
	}

	void States_SaveSlot(int slot)
	{
		States_SetCurrentSlot(slot);
		States_FreezeCurrentSlot();
	}

	void States_LoadSlot(int slot)
	{
		States_SetCurrentSlot(slot);
		States_DefrostCurrentSlot();
	}

	void States_SaveSlot0()
	{
		States_SaveSlot(0);
	}

	void States_SaveSlot1()
	{
		States_SaveSlot(1);
	}

	void States_SaveSlot2()
	{
		States_SaveSlot(2);
	}

	void States_SaveSlot3()
	{
		States_SaveSlot(3);
	}

	void States_SaveSlot4()
	{
		States_SaveSlot(4);
	}

	void States_SaveSlot5()
	{
		States_SaveSlot(5);
	}

	void States_SaveSlot6()
	{
		States_SaveSlot(6);
	}

	void States_SaveSlot7()
	{
		States_SaveSlot(7);
	}

	void States_SaveSlot8()
	{
		States_SaveSlot(8);
	}

	void States_SaveSlot9()
	{
		States_SaveSlot(9);
	}

	void States_LoadSlot0()
	{
		States_LoadSlot(0);
	}

	void States_LoadSlot1()
	{
		States_LoadSlot(1);
	}

	void States_LoadSlot2()
	{
		States_LoadSlot(2);
	}

	void States_LoadSlot3()
	{
		States_LoadSlot(3);
	}

	void States_LoadSlot4()
	{
		States_LoadSlot(4);
	}

	void States_LoadSlot5()
	{
		States_LoadSlot(5);
	}

	void States_LoadSlot6()
	{
		States_LoadSlot(6);
	}

	void States_LoadSlot7()
	{
		States_LoadSlot(7);
	}

	void States_LoadSlot8()
	{
		States_LoadSlot(8);
	}

	void States_LoadSlot9()
	{
		States_LoadSlot(9);
	}
} // namespace Implementations

// --------------------------------------------------------------------------------------
//  CommandDeclarations table
// --------------------------------------------------------------------------------------
// This is our manualized introspection/reflection table.  In a cool language like C# we'd
// have just grabbed this info from enumerating the members of a class and assigning
// properties to each method in the class.  But since this is C++, we have to do the the
// goold old fashioned way! :)

static const GlobalCommandDescriptor CommandDeclarations[] =
	{
		{
			"States_FreezeCurrentSlot",
			States_FreezeCurrentSlot,
			pxL("Save state"),
			pxL("Saves the virtual machine state to the current slot."),
			false,
		},

		{
			"States_DefrostCurrentSlot",
			States_DefrostCurrentSlot,
			pxL("Load state"),
			pxL("Loads a virtual machine state from the current slot."),
			false,
		},

		{
			"States_DefrostCurrentSlotBackup",
			States_DefrostCurrentSlotBackup,
			pxL("Load State Backup"),
			pxL("Loads virtual machine state backup for current slot."),
			false,
		},

		{
			"States_CycleSlotForward",
			States_CycleSlotForward,
			pxL("Cycle to next slot"),
			pxL("Cycles the current save slot in +1 fashion!"),
			false,
		},

		{
			"States_CycleSlotBackward",
			States_CycleSlotBackward,
			pxL("Cycle to prev slot"),
			pxL("Cycles the current save slot in -1 fashion!"),
			false,
		},

		{
			"Frameskip_Toggle",
			Implementations::Frameskip_Toggle,
			NULL,
			NULL,
			false,
		},

		{
			"Framelimiter_TurboToggle",
			Implementations::Framelimiter_TurboToggle,
			NULL,
			NULL,
			false,
		},

		{
			"Framelimiter_SlomoToggle",
			Implementations::Framelimiter_SlomoToggle,
			NULL,
			NULL,
			false,
		},

		{
			"Framelimiter_MasterToggle",
			Implementations::Framelimiter_MasterToggle,
			NULL,
			NULL,
			false,
		},

		{
			"GSwindow_CycleAspectRatio",
			Implementations::GSwindow_CycleAspectRatio,
			NULL,
			NULL,
			true,
		},

		{
			"GSwindow_ZoomIn",
			Implementations::GSwindow_ZoomIn,
			NULL,
			NULL,
			false,
		},

		{
			"GSwindow_ZoomOut",
			Implementations::GSwindow_ZoomOut,
			NULL,
			NULL,
			false,
		},

		{
			"GSwindow_ZoomToggle",
			Implementations::GSwindow_ZoomToggle,
			NULL,
			NULL,
			false,
		},

		{"GSwindow_ZoomInY", Implementations::GSwindow_ZoomInY, NULL, NULL, false},
		{"GSwindow_ZoomOutY", Implementations::GSwindow_ZoomOutY, NULL, NULL, false},
		{"GSwindow_ZoomResetY", Implementations::GSwindow_ZoomResetY, NULL, NULL, false},

		{"GSwindow_OffsetYminus", Implementations::GSwindow_OffsetYminus, NULL, NULL, false},
		{"GSwindow_OffsetYplus", Implementations::GSwindow_OffsetYplus, NULL, NULL, false},
		{"GSwindow_OffsetXminus", Implementations::GSwindow_OffsetXminus, NULL, NULL, false},
		{"GSwindow_OffsetXplus", Implementations::GSwindow_OffsetXplus, NULL, NULL, false},
		{"GSwindow_OffsetReset", Implementations::GSwindow_OffsetReset, NULL, NULL, false},

		{
			"Sys_SuspendResume",
			Implementations::Sys_SuspendResume,
			NULL,
			NULL,
			false,
		},

		{
			"Sys_TakeSnapshot",
			Implementations::Sys_TakeSnapshot,
			NULL,
			NULL,
			false,
		},

		{
			"Sys_RenderswitchToggle",
			Implementations::Sys_RenderToggle,
			NULL,
			NULL,
			false,
		},

		{
			"Sys_LoggingToggle",
			Implementations::Sys_LoggingToggle,
			NULL,
			NULL,
			false,
		},

		{
			"Sys_RecordingToggle",
			Implementations::Sys_RecordingToggle,
			NULL,
			NULL,
			false,
		},

		{
			"FullscreenToggle",
			Implementations::FullscreenToggle,
			NULL,
			NULL,
			false,
		},

		{"FrameAdvance", Implementations::FrameAdvance, NULL, NULL, false},
		{"TogglePause", Implementations::TogglePause, NULL, NULL, false},
		{"InputRecordingModeToggle", Implementations::InputRecordingModeToggle, NULL, NULL, false},
		{"GoToFirstFrame", Implementations::GoToFirstFrame, NULL, NULL, false},

		{"States_SaveSlot0", Implementations::States_SaveSlot0, NULL, NULL, false},
		{"States_SaveSlot1", Implementations::States_SaveSlot1, NULL, NULL, false},
		{"States_SaveSlot2", Implementations::States_SaveSlot2, NULL, NULL, false},
		{"States_SaveSlot3", Implementations::States_SaveSlot3, NULL, NULL, false},
		{"States_SaveSlot4", Implementations::States_SaveSlot4, NULL, NULL, false},
		{"States_SaveSlot5", Implementations::States_SaveSlot5, NULL, NULL, false},
		{"States_SaveSlot6", Implementations::States_SaveSlot6, NULL, NULL, false},
		{"States_SaveSlot7", Implementations::States_SaveSlot7, NULL, NULL, false},
		{"States_SaveSlot8", Implementations::States_SaveSlot8, NULL, NULL, false},
		{"States_SaveSlot9", Implementations::States_SaveSlot9, NULL, NULL, false},

		{"States_LoadSlot0", Implementations::States_LoadSlot0, NULL, NULL, false},
		{"States_LoadSlot1", Implementations::States_LoadSlot1, NULL, NULL, false},
		{"States_LoadSlot2", Implementations::States_LoadSlot2, NULL, NULL, false},
		{"States_LoadSlot3", Implementations::States_LoadSlot3, NULL, NULL, false},
		{"States_LoadSlot4", Implementations::States_LoadSlot4, NULL, NULL, false},
		{"States_LoadSlot5", Implementations::States_LoadSlot5, NULL, NULL, false},
		{"States_LoadSlot6", Implementations::States_LoadSlot6, NULL, NULL, false},
		{"States_LoadSlot7", Implementations::States_LoadSlot7, NULL, NULL, false},
		{"States_LoadSlot8", Implementations::States_LoadSlot8, NULL, NULL, false},
		{"States_LoadSlot9", Implementations::States_LoadSlot9, NULL, NULL, false},
		// Command Declarations terminator:
		// (must always be last in list!!)
		{NULL}};

void AcceleratorDictionary::Map(const KeyAcceleratorCode& _acode, const char* searchfor)
{
	// Search override mapping at ini file
	KeyAcceleratorCode acode = _acode;
	wxString overrideStr;
	wxAcceleratorEntry codeParser; //Provides string parsing capabilities
	wxFileConfig cfg(L"", L"", L"", GetUiKeysFilename(), wxCONFIG_USE_GLOBAL_FILE);
	if (cfg.Read(wxString::FromUTF8(searchfor), &overrideStr))
	{
		// needs a '\t' prefix (originally used for wxMenu accelerators parsing)...
		if (codeParser.FromString(wxString(L"\t") + overrideStr))
		{
			// ini file contains alternative parsable key combination for current 'searchfor'.
			acode = codeParser;
			if (acode.keycode >= 'A' && acode.keycode <= 'Z')
			{
				// Note that this needs to match the key event codes at Pcsx2App::PadKeyDispatch
				// Our canonical representation is the char code (at lower case if
				// applicable) with a separate modifier indicator, including shift.
				// The parser deviates from this by setting the keycode to upper case if
				// modifiers are used with plain letters. Luckily, it sets the modifiers
				// correctly, including shift (for letters without modifiers it parses lower case).
				// So we only need to change upper case letters to lower case.
				acode.keycode += 'a' - 'A';
			}
			if (_acode.ToString() != acode.ToString())
			{
				Console.WriteLn(Color_StrongGreen, "Overriding '%ls': assigning %ls (instead of %ls)",
								WX_STR(fromUTF8(searchfor)), WX_STR(acode.ToString()), WX_STR(_acode.ToString()));
			}
		}
		else
		{
			Console.Error("Error overriding KB shortcut for '%ls': can't understand '%ls'",
						  WX_STR(fromUTF8(searchfor)), WX_STR(overrideStr));
		}
	}
	// End of overrides section

	const GlobalCommandDescriptor* result = NULL;

	std::unordered_map<int, const GlobalCommandDescriptor*>::const_iterator iter(find(acode.val32));
	if (iter != end())
		result = iter->second;

	if (result != NULL)
	{
		Console.Warning(
			"Kbd Accelerator '%ls' is mapped multiple times.\n"
			"\t'Command %ls' is being replaced by '%ls'",
			WX_STR(acode.ToString()), WX_STR(fromUTF8(result->Id)), WX_STR(fromUTF8(searchfor)));
	}

	std::unordered_map<std::string, const GlobalCommandDescriptor*>::const_iterator acceleratorIter(wxGetApp().GlobalCommands->find(searchfor));

	if (acceleratorIter != wxGetApp().GlobalCommands->end())
		result = acceleratorIter->second;

	if (result == NULL)
	{
		Console.Warning("Kbd Accelerator '%ls' is mapped to unknown command '%ls'",
						WX_STR(acode.ToString()), WX_STR(fromUTF8(searchfor)));
	}
	else
	{
		if (!strcmp("Sys_TakeSnapshot", searchfor))
		{
			// Sys_TakeSnapshot is special in a bad way. On its own it creates a screenshot
			// but GS also checks whether shift or ctrl are held down, and for each of
			// them it does a different thing (gs dumps). So we need to map a shortcut and
			// also the same shortcut with shift and the same with ctrl to the same function.
			// So make sure the shortcut doesn't include shift or ctrl, and then add two more
			// which are derived from it.
			// Also, looking at the GS code, it seems that it never cares about both shift
			// and ctrl held together, but PCSX2 traditionally mapped f8, shift-f8 and ctrl-shift-f8
			// to Sys_TakeSnapshot, so let's not change it - we'll keep adding only shift and
			// ctrl-shift to the base shortcut.
			if (acode.cmd || acode.shift)
			{
				Console.Error("Cannot map %ls to Sys_TakeSnapshot - must not include Shift or Ctrl - these modifiers will be added automatically.",
							  WX_STR(acode.ToString()));
			}
			else
			{
				KeyAcceleratorCode shifted(acode);
				shifted.Shift();
				KeyAcceleratorCode controlledShifted(shifted);
				controlledShifted.Cmd();
				operator[](acode.val32) = result;
				operator[](shifted.val32) = result;
				operator[](controlledShifted.val32) = result;

				if (_acode.val32 != acode.val32)
				{ // overriding default
					Console.WriteLn(Color_Green, "Sys_TakeSnapshot: automatically mapping also %ls and %ls",
									WX_STR(shifted.ToString()),
									WX_STR(controlledShifted.ToString()));
				}
			}
		}
		else
		{
			operator[](acode.val32) = result;
		}
	}
}

KeyAcceleratorCode AcceleratorDictionary::findKeycodeWithCommandId(const char* commandId)
{
	for (auto entry = this->begin(); entry != this->end(); entry++)
	{
		if (strcmp(entry->second->Id, commandId) == 0)
		{
			const KeyAcceleratorCode keycode(entry->first);
			return keycode;
		}
	}
	return KeyAcceleratorCode(0);
}

void Pcsx2App::BuildCommandHash()
{
	if (!GlobalCommands)
		GlobalCommands = std::unique_ptr<CommandDictionary>(new CommandDictionary);

	const GlobalCommandDescriptor* curcmd = CommandDeclarations;
	while (curcmd->Invoke != NULL)
	{
		(*GlobalCommands)[curcmd->Id] = curcmd;
		curcmd++;
	}
}

void Pcsx2App::InitDefaultGlobalAccelerators()
{
	typedef KeyAcceleratorCode AAC;

	if (!GlobalAccels)
		GlobalAccels = std::unique_ptr<AcceleratorDictionary>(new AcceleratorDictionary);

	// Why do we even have those here? all of them seem to be overridden
	// by GSPanel::m_Accels ( GSPanel::InitDefaultAccelerators() )
	// - One reason is because this is used to initialize shortcuts in the MainFrame's UI (see - MainFrame::AppendShortcutToMenuOption)
	//   this is before the GS Window has been initialized.

	GlobalAccels->Map(AAC(WXK_F1), "States_FreezeCurrentSlot");
	GlobalAccels->Map(AAC(WXK_F3), "States_DefrostCurrentSlot");
	GlobalAccels->Map(AAC(WXK_F2), "States_CycleSlotForward");
	GlobalAccels->Map(AAC(WXK_F2).Shift(), "States_CycleSlotBackward");

	GlobalAccels->Map(AAC(WXK_F4), "Framelimiter_MasterToggle");
	GlobalAccels->Map(AAC(WXK_F4).Shift(), "Frameskip_Toggle");

	// At this early stage of startup, the application assumes installed mode, so portable mode custom keybindings may present issues.
	// Relevant - https://github.com/PCSX2/pcsx2/blob/678829a5b2b8ca7a3e42d8edc9ab201bf00b0fe9/pcsx2/gui/AppInit.cpp#L479
	// Compared to L990 of GlobalCommands.cpp which also does an init for the GlobalAccelerators.
	// The idea was to have: Reading from the PCSX2_keys.ini in the ini folder based on PCSX2_keys.ini.default which get overridden. 
	// We also need to make it easier to do custom hotkeys for both normal/portable PCSX2 in the GUI.
	GlobalAccels->Map(AAC(WXK_TAB), "Framelimiter_TurboToggle");
	GlobalAccels->Map(AAC(WXK_TAB).Shift(), "Framelimiter_SlomoToggle");

	GlobalAccels->Map(AAC(WXK_F6), "GSwindow_CycleAspectRatio");
	GlobalAccels->Map(AAC(WXK_RETURN).Alt(), "FullscreenToggle");

	GlobalAccels->Map(AAC(WXK_ESCAPE), "Sys_SuspendResume");

	// Fixme: GS Dumps could need a seperate label and hotkey binding or less interlinked with normal screenshots/snapshots , which messes with overloading lots of different mappings, commented the other GlobalAccels for this reason. GS hardcodes keybindings.
	GlobalAccels->Map(AAC(WXK_F8), "Sys_TakeSnapshot");
	// GlobalAccels->Map(AAC(WXK_F8).Shift(), "Sys_TakeSnapshot");
	// GlobalAccels->Map(AAC(WXK_F8).Shift().Cmd(), "Sys_TakeSnapshot");
	GlobalAccels->Map(AAC(WXK_F9), "Sys_RenderswitchToggle");

	// GlobalAccels->Map(AAC(WXK_F10),	"Sys_LoggingToggle");
	// GlobalAccels->Map(AAC(WXK_F11),	"Sys_FreezeGS");
	GlobalAccels->Map(AAC(WXK_F12), "Sys_RecordingToggle");
}
