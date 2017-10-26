#pragma once
#ifndef __KEY_MOVIE_H__
#define __KEY_MOVIE_H__

#include "KeyMovieOnFile.h"


//----------------------------
// KeyMovie
//----------------------------
class KeyMovie {
public:
	KeyMovie() {}
	~KeyMovie(){}
public:
	// controller
	void ControllerInterrupt(u8 &data, u8 &port, u16 & BufCount, u8 buf[]);

	// menu bar
	void Stop();
	void Start(wxString filename, bool fReadOnly, VmStateBuffer* ss = nullptr);

	// shortcut key
	void RecordModeToggle();


public:
	enum KEY_MOVIE_MODE {
		NONE,
		RECORD,
		REPLAY,
	};

public:
	// getter
	KEY_MOVIE_MODE getModeState() { return state; }
	KeyMovieOnFile & getKeyMovieData() {return keyMovieData;}
	bool isInterruptFrame() { return fInterruptFrame; }

private:
	KeyMovieOnFile keyMovieData;
	KEY_MOVIE_MODE state = NONE;
	bool fInterruptFrame = false;


};
extern KeyMovie g_KeyMovie;
#define g_KeyMovieData (g_KeyMovie.getKeyMovieData())
#define g_KeyMovieHeader (g_KeyMovie.getKeyMovieData().getHeader())


#endif
