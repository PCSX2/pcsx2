#pragma once
#ifndef __MOVIE_CONTROLE_H__
#define __MOVIE_CONTROLE_H__

class MovieControle {
public:

	// movie controle main function
	bool isStop();
	void StartCheck();
	void StopCheck();

	// shortcut key
	void FrameAdvance();
	void TogglePause();

	// doit
	void Pause();
	void UnPause();

	// getter
	bool getStopFlag() { return (fStop || fFrameAdvance); }

private:
	uint stopFrameCount = false;
	
	bool fStop = false;
	bool fStart = false;
	bool fFrameAdvance = false;
	bool fPauseState = false;

};
extern MovieControle g_MovieControle;

#endif
