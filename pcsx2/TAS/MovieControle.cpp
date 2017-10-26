#include "PrecompiledHeader.h"

#include "GSFrame.h"

#include "App.h"	// use "CoreThread"
#include "Counters.h"	// use"g_FrameCount"

#include "MovieControle.h"


MovieControle g_MovieControle;


//-----------------------------------------------
// ゲームはCoreThreadで動いており、動いている間はGSFrame(wxFrame)の処理を受け付けない
// (キー入力の受け付けもCoreThread内とGSFrame内の2箇所が設定されてるっぽい)
// CoreThreadが停止している間はwxFrameの入力が動く仕組みっぽい
//-----------------------------------------------

bool MovieControle::isStop()
{
	return (fPauseState && CoreThread.IsOpen() && CoreThread.IsPaused());
}
//-----------------------------------------------
// Counters(CoreThread)内の停止判定用
//-----------------------------------------------
void MovieControle::StartCheck()
{
	if (fStart && CoreThread.IsOpen() && CoreThread.IsPaused()) {
		CoreThread.Resume();
		fStart = false;
		fPauseState = false;
	}
}

void MovieControle::StopCheck()
{
	if (fFrameAdvance) {
		if (stopFrameCount < g_FrameCount) {
			fFrameAdvance = false;
			fStop = true;
			stopFrameCount = g_FrameCount;

			// We force the frame counter in the title bar to change
			wxString oldTitle = wxGetApp().GetGsFrame().GetTitle();
			wxString title = g_Conf->Templates.TitleTemplate;
			wxString frameCount = wxString::Format("%d", g_FrameCount);

			title.Replace(L"${frame}", frameCount);	//--TAS--//
			int frameIndex = title.find(wxString::Format(L"%d", g_FrameCount));
			frameIndex += frameCount.length();

			title.replace(frameIndex, oldTitle.length() - frameIndex, oldTitle.c_str().AsChar() + frameIndex);
			
			wxGetApp().GetGsFrame().SetTitle(title);
		}
	}
	if (fStop && CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		fPauseState = true;
		CoreThread.PauseSelf();	//selfじゃないと止まらない
	}
}



//----------------------------------
// shortcut key
//----------------------------------
void MovieControle::FrameAdvance()
{
	stopFrameCount = g_FrameCount;
	fFrameAdvance = true;
	fStop = false;
	fStart = true;
}
void MovieControle::TogglePause()
{
	fStop = !fStop;
	if (fStop == false) {
		fStart = true;
	}
}
void MovieControle::Pause()
{
	fStop = true;
	fFrameAdvance = true;
}
void MovieControle::UnPause()
{
	fStop = false;
	fStart = true;
	fFrameAdvance = true;
}

