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

#pragma once

#include "App.h"
#include <array>
#include <map>
#include <memory>

static const bool EnableThreadedLoggingTest = false; //true;

class LogWriteEvent;

// --------------------------------------------------------------------------------------
//  PipeRedirectionBase
// --------------------------------------------------------------------------------------
// Implementations for this class are found in Win/Lnx specific modules.  Class creation
// should be done using NewPipeRedir() only (hence the protected constructor in this class).
//
class PipeRedirectionBase
{
	DeclareNoncopyableObject( PipeRedirectionBase );

public:
	virtual ~PipeRedirectionBase() =0;	// abstract destructor, forces abstract class behavior

protected:
	PipeRedirectionBase() {}
};

extern PipeRedirectionBase* NewPipeRedir( FILE* stdstream );

// --------------------------------------------------------------------------------------
//  pxLogConsole
// --------------------------------------------------------------------------------------
// This is a custom logging facility that pipes wxLog messages to our very own console
// log window.  Useful for catching and redirecting wx's internal logs (although like
// 3/4ths of them are worthless and we would probably rather ignore them anyway).
//
class pxLogConsole : public wxLog
{
public:
	pxLogConsole() {}

protected:
	virtual void DoLogRecord(wxLogLevel level, const wxString &message, const wxLogRecordInfo &info);
};


// --------------------------------------------------------------------------------------
//  ConsoleThreadTest -- useful class for unit testing the thread safety and general performance
//  of the console logger.
// --------------------------------------------------------------------------------------

class ConsoleTestThread : public Threading::pxThread
{
	typedef pxThread _parent;

protected:
	std::atomic<bool> m_done;
	void ExecuteTaskInThread();

public:
	ConsoleTestThread() :
		m_done( false )
	{
	}

	~ConsoleTestThread()
	{
		m_done = true;
	}
};

// --------------------------------------------------------------------------------------
//  ConsoleLogFrame  --  Because the one built in wx is poop.
// --------------------------------------------------------------------------------------
class ConsoleLogFrame : public wxFrame
{
	DeclareNoncopyableObject(ConsoleLogFrame);

public:
	typedef AppConfig::ConsoleLogOptions ConLogConfig;

protected:
	class ColorArray
	{
		DeclareNoncopyableObject(ColorArray);

	protected:
		std::array<wxTextAttr, ConsoleColors_Count> m_table;

	public:
		virtual ~ColorArray() = default;
		ColorArray( int fontsize=8 );

		void SetFont( const wxFont& font );
		void SetFont( int fontsize );
		u32 GetRGBA( const ConsoleColors color );

		const wxTextAttr& operator[]( ConsoleColors coloridx ) const
		{
			return m_table[(int)coloridx];
		}
		
		void SetColorScheme_Dark();
		void SetColorScheme_Light();
	};

	class ColorSection
	{
	public:
		ConsoleColors	color;
		int				startpoint;

		ColorSection() : color(Color_Default), startpoint(0) {}
		ColorSection( ConsoleColors _color, int msgptr ) : color(_color), startpoint(msgptr) { }
	};

private:
	wxColor lightThemeBgColor = wxColor(230, 235, 242);
	wxColor darkThemeBgColor = wxColor(38, 41, 48);

protected:
	ConLogConfig&	m_conf;
	wxTextCtrl&		m_TextCtrl;
	wxTimer			m_timer_FlushLimiter;
	wxTimer			m_timer_FlushUnlocker;
	ColorArray		m_ColorTable;

	int				m_flushevent_counter;
	bool			m_FlushRefreshLocked;

	// ----------------------------------------------------------------------------
	//  Queue State Management Vars
	// ----------------------------------------------------------------------------

	// Boolean indicating if a flush message is already in the Main message queue.  Used
	// to prevent spamming the main thread with redundant messages.
	std::atomic<bool>			m_pendingFlushMsg;

	// This is a counter of the number of threads waiting for the Queue to flush.
	std::atomic<int>			m_WaitingThreadsForFlush;

	// Indicates to the main thread if a child thread is actively writing to the log.  If
	// true the main thread will sleep briefly to allow the child a chance to accumulate
	// more messages (helps avoid rapid successive flushes on high volume logging).
	std::atomic<bool>			m_ThreadedLogInQueue;

	// Used by threads waiting on the queue to flush.
	Semaphore				m_sem_QueueFlushed;

	// Lock object for accessing or modifying the following three vars:
	//  m_QueueBuffer, m_QueueColorSelection, m_CurQueuePos
	Mutex					m_mtx_Queue;

	// Describes a series of colored text sections in the m_QueueBuffer.
	SafeList<ColorSection>	m_QueueColorSection;

	// Series of Null-terminated strings, each one has a corresponding entry in
	// m_QueueColorSelection.
	SafeArray<wxChar>		m_QueueBuffer;

	// Current write position into the m_QueueBuffer;
	int						m_CurQueuePos;

	// Threaded log spammer, useful for testing console logging performance.
	// (alternatively you can enable Disasm logging in any recompiler and achieve
	// a similar effect)
	std::unique_ptr<ConsoleTestThread> m_threadlogger;

public:
	// ctor & dtor
	ConsoleLogFrame( MainEmuFrame *pParent, const wxString& szTitle, ConLogConfig& options );
	virtual ~ConsoleLogFrame();

	// Retrieves the current configuration options settings for this box.
	// (settings change if the user moves the window or changes the font size)
	const ConLogConfig& GetConfig() const { return m_conf; }
	u32 GetRGBA( const ConsoleColors color ) { return m_ColorTable.GetRGBA( color ); }

	bool Write( ConsoleColors color, const wxString& text );
	bool Newline();

	void UpdateLogList();

protected:
	// menu callbacks
	void OnOpen (wxCommandEvent& event);
	void OnClose(wxCommandEvent& event);
	void OnSave (wxCommandEvent& event);
	void OnClear(wxCommandEvent& event);
	void OnLogSettings(wxCommandEvent& event);

	void OnEnableAllLogging(wxCommandEvent& event);
	void OnDisableAllLogging(wxCommandEvent& event);
	void OnSetDefaultLogging(wxCommandEvent& event);

	void OnToggleTheme(wxCommandEvent& event);
	void OnFontSize(wxCommandEvent& event);
	void OnAutoDock(wxCommandEvent& event);
	void OnToggleSource(wxCommandEvent& event);
	void OnToggleCDVDInfo(wxCommandEvent& event);

	virtual void OnCloseWindow(wxCloseEvent& event);

	void OnSetTitle( wxCommandEvent& event );
	void OnFlushUnlockerTimer( wxTimerEvent& evt );
	void OnFlushEvent( wxCommandEvent& event );

	void DoFlushQueue();
	void DoFlushEvent( bool isPending );

	void OnMoveAround( wxMoveEvent& evt );
	void OnResize( wxSizeEvent& evt );
	void OnActivate( wxActivateEvent& evt );

	void OnLoggingChanged();
};

void OSDlog(ConsoleColors color, bool console, const std::string& str);

template<typename ... Args>
void OSDlog(ConsoleColors color, bool console, const std::string& format, Args ... args) {
	if (!console) return;

	size_t size = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
	std::vector<char> buf(size);
	snprintf( buf.data(), size, format.c_str(), args ... );

	OSDlog(color, console, buf.data());
}


