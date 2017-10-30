#include "PrecompiledHeader.h"

#include "app.h"	//Counters.hをincludeするのに必要
#include "Counters.h"	// use"g_FrameCount"
#include "SaveState.h"	// create "SaveStateBase::keymovieFreeze()"
#include "AppSaveStates.h"	// use "States_GetCurrentSlot()"

#include "TAS/MovieControle.h"
#include "KeyMovie.h"

#include <vector>


KeyMovie g_KeyMovie;

//-----------------------------------------------
// save or loadでフレーム数の保存
//-----------------------------------------------
void SaveStateBase::keymovieFreeze()
{
	FreezeTag("keymovie");
	Freeze(g_FrameCount);	//Freeze関数でなぜかframeの保存がうまくいった

	if (IsLoading()) {
		g_KeyMovieData.addUndoCount();
	}
}

//----------------------------------
// Main func for handling recording and inputting controller data
// Called by Sio.cpp::sioWriteController
//----------------------------------
void KeyMovie::ControllerInterrupt(u8 &data, u8 &port, u16 & bufCount, u8 buf[])
{
	// Only examine controllers 1 / 2
	if (port < 0 || 1 < port )
		return;

	//==========================
	// This appears to try to ensure that we are only paying attention
	// to the frames that matter, the ones that are reading from
	// the controller.
	//
	// See - Lilypad.cpp:1254
	// 0x42 is the magic number for the default read query
	//
	// NOTE: this appears to possibly break if you have logging enabled in LilyPad's config!
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
	else if ( bufCount == 2 ){
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
	// bytes in the return from LilyPad
	if (bufCount < 3)
		return;

	//---------------
	// Read or Write
	//---------------
	const u8 &nowBuf = buf[bufCount];
	if (state == RECORD)
	{
		keyMovieData.updateFrameMax(g_FrameCount);
		keyMovieData.writeKeyBuf(g_FrameCount, port, bufCount - 3, nowBuf);
	}
	else if (state == REPLAY)
	{
		if (keyMovieData.getMaxFrame() <= g_FrameCount)
		{
			// Pause the emulation but the movie is not closed
			g_MovieControle.Pause();
			return;
		}
		u8 tmp = 0;
		if (keyMovieData.readKeyBuf(tmp, g_FrameCount, port, bufCount - 3)) {
			buf[bufCount] = tmp;
		}
	}
}


//----------------------------------
// stop
//----------------------------------
void KeyMovie::Stop() {
	state = NONE;
	if (keyMovieData.Close()) {
		Console.WriteLn(Color_StrongBlue, L"[KeyMovie]KeyMovie stop.");
	}
}

//----------------------------------
// start
//----------------------------------
void KeyMovie::Start(wxString FileName,bool fReadOnly, VmStateBuffer* ss)
{
	g_MovieControle.Pause();
	Stop();

	if (fReadOnly)
	{
		if (!keyMovieData.Open(FileName, false)) {
			return;
		}
		if (!keyMovieData.readHeaderAndCheck()) {
			Console.WriteLn(Color_StrongBlue, L"[KeyMovie]This file is not a correct KeyMovie file.");
			keyMovieData.Close();
			return;
		}
		// cdrom
		if (!g_Conf->CurrentIso.IsEmpty())
		{
			if (Path::GetFilename(g_Conf->CurrentIso) != keyMovieData.getHeader().cdrom) {
				Console.WriteLn(Color_StrongBlue, L"[KeyMovie]Information on CD in Movie file is different.");
			}
		}
		state = REPLAY;
		Console.WriteLn(Color_StrongBlue, wxString::Format( L"[KeyMovie]Replay movie.[%s]",FileName) );
		Console.WriteLn(Color_StrongBlue, L"MaxFrame:%d", keyMovieData.getMaxFrame());
		Console.WriteLn(Color_StrongBlue, L"UndoCount:%d", keyMovieData.getUndoCount());
	}
	else
	{
		// backup
		wxString bpfile = wxString::Format(L"%s_backup", FileName);
		if (CopyFile( FileName , bpfile, false)) {
			Console.WriteLn(Color_StrongBlue, wxString::Format(L"[KeyMovie]Create backup file.[%s]", bpfile) );
		}
		// create
		if (!keyMovieData.Open(FileName, true, ss)) {
			return;
		}
		// cdrom
		if (!g_Conf->CurrentIso.IsEmpty())
		{
			keyMovieData.getHeader().setCdrom(Path::GetFilename(g_Conf->CurrentIso));
		}
		keyMovieData.writeHeader();
		keyMovieData.writeSavestate();

		state = RECORD;
		Console.WriteLn(Color_StrongBlue, wxString::Format(L"[KeyMovie]Start new record.[%s]",FileName ));
	}
	// In every case, we reset the g_FrameCount
	g_FrameCount = 0;
}

//----------------------------------
// shortcut key
//----------------------------------
void KeyMovie::RecordModeToggle()
{
	if (state == REPLAY) {
		state = RECORD;
		Console.WriteLn(Color_StrongBlue, L"[KeyMovie]Record mode on.");
	}
	else if (state == RECORD) {
		state = REPLAY;
		Console.WriteLn(Color_StrongBlue, L"[KeyMovie]Replay mode on.");
	}
}


