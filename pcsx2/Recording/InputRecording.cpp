#include "PrecompiledHeader.h"

#include "AppSaveStates.h"
#include "AppGameDatabase.h"
#include "Common.h"
#include "Counters.h"
#include "MemoryTypes.h"
#include "SaveState.h"

#include "Recording/RecordingControls.h"
#include "InputRecording.h"

#include <vector>


InputRecording g_InputRecording;

// --------------------------------
// Tag and save framecount along with savestate
// --------------------------------
void SaveStateBase::InputRecordingFreeze()
{
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);
	if (IsLoading()) {
		g_InputRecordingData.addUndoCount();
	}
}

//----------------------------------
// Main func for handling controller input data
// Called by Sio.cpp::sioWriteController
//----------------------------------
void InputRecording::ControllerInterrupt(u8 &data, u8 &port, u16 & bufCount, u8 buf[])
{
	// TODO - Multi-Tap Support
	// Only examine controllers 1 / 2
	if (port < 0 || 1 < port)
		return;

	//==========================
	// This appears to try to ensure that we are only paying attention
	// to the frames that matter, the ones that are reading from
	// the controller.
	//
	// See - Lilypad.cpp:1254
	// 0x42 is the magic number for the default read query
	//
	// NOTE: this appears to break if you have logging enabled in LilyPad's config!
	//==========================
	if (bufCount == 1) {
		if (data == 0x42)
		{
			fInterruptFrame = true;
		}
		else {
			fInterruptFrame = false;
			return;
		}
	}
	else if ( bufCount == 2 ) {
		// See - LilyPad.cpp:1255
		// 0x5A is always the second byte in the buffer
		// when the normal READ_DATA_AND_VIBRRATE (0x42)
		// query is executed, this looks like a sanity check
		if (buf[bufCount] != 0x5A) {
			fInterruptFrame = false;
			return;
		}
	}

	if (!fInterruptFrame)
		return;

	if (state == NONE)
		return;

	// We do not want to record or save the first two
	// bytes in the data returned from LilyPad
	if (bufCount < 3)
		return;

	//---------------
	// Read or Write
	//---------------
	const u8 &nowBuf = buf[bufCount];
	if (state == RECORD)
	{
		InputRecordingData.updateFrameMax(g_FrameCount);
		InputRecordingData.writeKeyBuf(g_FrameCount, port, bufCount - 3, nowBuf);
	}
	else if (state == REPLAY)
	{
		if (InputRecordingData.getMaxFrame() <= g_FrameCount)
		{
			// Pause the emulation but the movie is not closed
			g_RecordingControls.Pause();
			return;
		}
		u8 tmp = 0;
		if (InputRecordingData.readKeyBuf(tmp, g_FrameCount, port, bufCount - 3)) {
			buf[bufCount] = tmp;
		}
	}
}


//----------------------------------
// stop
//----------------------------------
void InputRecording::Stop() {
	state = NONE;
	if (InputRecordingData.Close()) {
		recordingConLog(L"[REC]: InputRecording Recording Stopped.\n");
	}
}

//----------------------------------
// start
//----------------------------------
void InputRecording::Create(wxString FileName, bool fromSaveState, wxString authorName)
{
	g_RecordingControls.Pause();
	Stop();

	// create
	if (!InputRecordingData.Open(FileName, true, fromSaveState)) {
		return;
	}
	// Set author name
	if (!authorName.IsEmpty())
	{
		InputRecordingData.getHeader().setAuthor(authorName);
	}
	// Set Game Name
	// Code loosely taken from AppCoreThread.cpp to resolve the Game Name
	// Fallback is ISO name
	wxString gameName;
	const wxString gameKey(SysGetDiscID());
	if (!gameKey.IsEmpty())
	{
		if (IGameDatabase* GameDB = AppHost_GetGameDatabase())
		{
			Game_Data game;
			if (GameDB->findGame(game, gameKey))
			{
				gameName = game.getString("Name");
				gameName += L" (" + game.getString("Region") + L")";
			}
		}
	}
	InputRecordingData.getHeader().setGameName(!gameName.IsEmpty() ? gameName : Path::GetFilename(g_Conf->CurrentIso));
	InputRecordingData.writeHeader();
	state = RECORD;
	recordingConLog(wxString::Format(L"[REC]: Started new recording - [%s]\n", FileName));

	// In every case, we reset the g_FrameCount
	g_FrameCount = 0;
}

void InputRecording::Play(wxString FileName, bool fromSaveState)
{
	g_RecordingControls.Pause();
	Stop();

	if (!InputRecordingData.Open(FileName, false, false)) {
		return;
	}
	if (!InputRecordingData.readHeaderAndCheck()) {
		recordingConLog(L"[REC]: This file is not a correct InputRecording file.\n");
		InputRecordingData.Close();
		return;
	}
	// Check author name
	if (!g_Conf->CurrentIso.IsEmpty())
	{
		if (Path::GetFilename(g_Conf->CurrentIso) != InputRecordingData.getHeader().gameName) {
			recordingConLog(L"[REC]: Information on CD in Movie file is Different.\n");
		}
	}
	// TODO - probably output more informatiion on it
	state = REPLAY;
	recordingConLog(wxString::Format(L"[REC]: Replaying movie - [%s]\n", FileName));
	recordingConLog(wxString::Format(L"MaxFrame: %d\n", InputRecordingData.getMaxFrame()));
	recordingConLog(wxString::Format(L"UndoCount: %d\n", InputRecordingData.getUndoCount()));
}

//----------------------------------
// shortcut key
//----------------------------------
void InputRecording::RecordModeToggle()
{
	if (state == REPLAY) {
		state = RECORD;
		recordingConLog("[REC]: Record mode ON.\n");
	}
	else if (state == RECORD) {
		state = REPLAY;
		recordingConLog("[REC]: Replay mode ON.\n");
	}
}


