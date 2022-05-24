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
#include "Win32.h"

#include "gui/App.h"
#include "gui/ConsoleLogger.h"

// --------------------------------------------------------------------------------------
//  Win32 Console Pipes
//  As a courtesy and convenience, we redirect stdout/stderr to the console and logfile.
// --------------------------------------------------------------------------------------

using namespace Threading;

// --------------------------------------------------------------------------------------
//  WinPipeThread
// --------------------------------------------------------------------------------------
class WinPipeThread : public pxThread
{
	typedef pxThread _parent;

protected:
	const HANDLE& m_outpipe;
	const ConsoleColors m_color;

public:
	WinPipeThread( const HANDLE& outpipe, ConsoleColors color )
		: m_outpipe( outpipe )
		, m_color( color )
	{
		m_name = (m_color == Color_Red) ? L"Redirect_Stderr" : L"Redirect_Stdout";
	}

	virtual ~WinPipeThread()
	{
		_parent::Cancel();
	}

protected:
	void ExecuteTaskInThread()
	{
		::SetThreadPriority( ::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );
		if( m_outpipe == INVALID_HANDLE_VALUE ) return;

		try
		{
			if (ConnectNamedPipe(m_outpipe, nullptr) == 0 && GetLastError() != ERROR_PIPE_CONNECTED)
				throw Exception::RuntimeError().SetDiagMsg("ConnectNamedPipe failed.");

			char s8_Buf[2049];
			DWORD u32_Read = 0;

			while( true )
			{
				if( !ReadFile(m_outpipe, s8_Buf, sizeof(s8_Buf)-1, &u32_Read, NULL) )
				{
					DWORD result = GetLastError();
					if( result == ERROR_HANDLE_EOF || result == ERROR_BROKEN_PIPE ) break;
					if( result == ERROR_IO_PENDING )
					{
						Yield( 10 );
						continue;
					}

					throw Exception::WinApiError().SetDiagMsg("ReadFile from pipe failed.");
				}

				if( u32_Read <= 3 )
				{
					// Windows has a habit of sending 1 or 2 characters of every message, and then sending
					// the rest in a second message.  This is "ok" really, except our Console class is hardly
					// free of overhead, so it's helpful if we can concatenate the couple of messages together.
					// But we don't want to break the ability to print progressive status bars, like '....'
					// so I use a clever Yield/Peek loop combo that keeps reading as long as there's new data
					// immediately being fed to our pipe. :)  --air

					DWORD u32_avail = 0;

					do
					{
						Yield();
						if( !PeekNamedPipe(m_outpipe, 0, 0, 0, &u32_avail, 0) )
							throw Exception::WinApiError().SetDiagMsg("Error peeking Pipe.");

						if( u32_avail == 0 ) break;

						DWORD loopread;
						if( !ReadFile(m_outpipe, &s8_Buf[u32_Read], sizeof(s8_Buf)-u32_Read-1, &loopread, NULL) ) break;
						u32_Read += loopread;

					} while( u32_Read < sizeof(s8_Buf)-32 );
				}

				// ATTENTION: The Console always prints ANSI to the pipe independent if compiled as UNICODE or MBCS!
				s8_Buf[u32_Read] = 0;

				ConsoleColorScope cs(m_color);
				Console.DoWriteFromStdout( s8_Buf );

				TestCancel();
			}
		}
		catch( Exception::RuntimeError& ex )
		{
			// Log error, and fail silently.  It's not really important if the
			// pipe fails.  PCSX2 will run fine without it in any case.
			Console.Error( ex.FormatDiagnosticMessage() );
		}
	}
};

// --------------------------------------------------------------------------------------
//  WinPipeRedirection
// --------------------------------------------------------------------------------------
class WinPipeRedirection : public PipeRedirectionBase
{
	DeclareNoncopyableObject( WinPipeRedirection );

protected:
	HANDLE		m_readpipe;
	FILE*		m_fp;

	WinPipeThread m_Thread;

public:
	WinPipeRedirection( FILE* stdstream );
	virtual ~WinPipeRedirection();

	void Cleanup() noexcept;
};

WinPipeRedirection::WinPipeRedirection( FILE* stdstream )
	: m_readpipe(INVALID_HANDLE_VALUE)
	, m_fp(nullptr)
	, m_Thread(m_readpipe, (stdstream == stderr) ? Color_Red : Color_Black)
{
	pxAssert( (stdstream == stderr) || (stdstream == stdout) );

	try
	{
		// freopen requires a pathname, so a named pipe must be used. The
		// pathname needs to be unique to allow multiple instances of PCSX2 to
		// work properly.
		const wxString stream_name(stdstream == stderr ? L"stderr" : L"stdout");
		const wxString pipe_name(wxString::Format(L"\\\\.\\pipe\\pcsx2_%s%d", stream_name, GetCurrentProcessId()));

		m_readpipe = CreateNamedPipe(pipe_name, PIPE_ACCESS_INBOUND, 0, 1, 2048, 2048, 0, nullptr);
		if (m_readpipe == INVALID_HANDLE_VALUE)
			throw Exception::WinApiError().SetDiagMsg("CreateNamedPipe failed.");

		m_Thread.Start();

		// Binary flag set to prevent multiple \r characters before each \n.
		m_fp = _wfreopen(pipe_name, L"wb", stdstream);
		if (m_fp == nullptr)
			throw Exception::RuntimeError().SetDiagMsg("_wfreopen returned NULL.");

		setvbuf(stdstream, nullptr, _IONBF, 0);
	}
	catch( Exception::BaseThreadError& ex )
	{
		// thread object will become invalid because of scoping after we leave
		// the constructor, so re-pack a new exception:

		Cleanup();
		throw Exception::RuntimeError().SetDiagMsg( ex.FormatDiagnosticMessage() ).SetUserMsg( ex.FormatDisplayMessage() );
	}
	catch( BaseException& ex )
	{
		Cleanup();
		ex.DiagMsg() = (std::string)((stdstream==stdout) ? "STDOUT" : "STDERR") + " Redirection Init failed: " + ex.DiagMsg();
		throw;
	}
	catch( ... )
	{
		// C++ doesn't execute the object destructor automatically, because it's fail++
		// (and I'm *not* encapsulating each handle into its own object >_<)

		Cleanup();
		throw;
	}
}

WinPipeRedirection::~WinPipeRedirection()
{
	Cleanup();
}

void WinPipeRedirection::Cleanup() noexcept
{
	// Cleanup Order Notes:
	//  * The redirection thread is most likely blocking on ReadFile(), so we can't Cancel yet, lest we deadlock --
	//    Closing the writepipe (either directly or through the fp/crt handles) issues an EOF to the thread,
	//    so it's safe to Cancel afterward.

	if( m_fp != NULL )
	{
		fclose( m_fp );
		m_fp = NULL;
	}

	m_Thread.Cancel();

	if( m_readpipe != INVALID_HANDLE_VALUE )
	{
		CloseHandle( m_readpipe );
		m_readpipe = INVALID_HANDLE_VALUE;
	}
}

// The win32 specific implementation of PipeRedirection.
PipeRedirectionBase* NewPipeRedir( FILE* stdstream )
{
	try
	{
		return new WinPipeRedirection( stdstream );
	}
	catch( Exception::RuntimeError& ex )
	{
		// Entirely non-critical errors.  Log 'em and move along.
		Console.Error( ex.FormatDiagnosticMessage() );
	}

	return NULL;
}
