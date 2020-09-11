/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "Counters.h"
#include "Common.h"
#include "GSFrame.h"
#include "MemoryTypes.h"

#include "RecordingControls.h"


#ifndef DISABLE_RECORDING
RecordingControls g_RecordingControls;

//-----------------------------------------------
// Current recording status, returns true if:
// - Recording is Paused
// - GSFrame CoreThread is both running AND paused
//-----------------------------------------------
bool RecordingControls::IsEmulationAndRecordingPaused()
{
	return fPauseState && CoreThread.IsOpen() && CoreThread.IsPaused();
}

//-----------------------------------------------
// Called after inputs are recorded for that frame, places lock on input recording, effectively releasing resources and resuming CoreThread.
//-----------------------------------------------
void RecordingControls::ResumeCoreThreadIfStarted()
{
	if (fStart && CoreThread.IsOpen() && CoreThread.IsPaused())
	{
		CoreThread.Resume();
		fStart = false;
		fPauseState = false;
	}
}

//-----------------------------------------------
// Called at VSYNC End / VRender Begin, updates everything recording related for the next frame,
// toggles RecordingControl flags back to enable input recording for the next frame.
//-----------------------------------------------
void RecordingControls::HandleFrameAdvanceAndStop()
{
	if (fFrameAdvance)
	{
		if (stopFrameCount < g_FrameCount)
		{
			fFrameAdvance = false;
			fStop = true;
			stopFrameCount = g_FrameCount;

			// We force the frame counter in the title bar to change
			wxString oldTitle = wxGetApp().GetGsFrame().GetTitle();
			wxString title = g_Conf->Templates.RecordingTemplate;
			wxString frameCount = wxString::Format("%d", g_FrameCount);

			title.Replace(L"${frame}", frameCount);
			int frameIndex = title.find(wxString::Format(L"%d", g_FrameCount));
			frameIndex += frameCount.length();

			title.replace(frameIndex, oldTitle.length() - frameIndex, oldTitle.c_str().AsChar() + frameIndex);

			wxGetApp().GetGsFrame().SetTitle(title);
		}
	}
	if (fStop && CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		fPauseState = true;
		CoreThread.PauseSelf();
	}
}

bool RecordingControls::GetStopFlag()
{
	return (fStop || fFrameAdvance);
}

void RecordingControls::FrameAdvance()
{
	stopFrameCount = g_FrameCount;
	fFrameAdvance = true;
	fStop = false;
	fStart = true;
}

void RecordingControls::TogglePause()
{
	fStop = !fStop;
	if (fStop == false)
	{
		fStart = true;
	}
}

void RecordingControls::Pause()
{
	fStop = true;
}

void RecordingControls::Unpause()
{
	fStop = false;
	fStart = true;
}
#endif
