/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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
#include "ConsoleLogger.h"
#include <unistd.h>

using namespace Threading;

// Reads data sent to stdout/stderr and sends it to the PCSX2 console.
class LinuxPipeThread : public pxThread
{
	typedef pxThread _parent;

protected:
	const int& m_read_fd;
	const ConsoleColors m_color;

	void ExecuteTaskInThread();
public:
	LinuxPipeThread(const int& read_fd, ConsoleColors color);
	virtual ~LinuxPipeThread() noexcept;
};

LinuxPipeThread::LinuxPipeThread(const int& read_fd, ConsoleColors color)
	: pxThread(color == Color_Red ? L"Redirect_Stderr" : L"Redirect_Stdout")
	, m_read_fd(read_fd)
	, m_color(color)
{
}

LinuxPipeThread::~LinuxPipeThread() noexcept
{
	_parent::Cancel();
}

void LinuxPipeThread::ExecuteTaskInThread()
{
	char buffer[2049];
	while (ssize_t bytes_read = read(m_read_fd, buffer, sizeof(buffer) - 1)) {
		TestCancel();

		if (bytes_read == -1) {
			if (errno == EINTR) {
				continue;
			}
			else {
				// Should never happen.
				Console.Error(wxString::Format(L"Redirect %s failed: read: %s",
					m_color == Color_Red ? L"stderr" : L"stdout", strerror(errno)));
				break;
			}
		}

		buffer[bytes_read] = 0;

		{
			ConsoleColorScope cs(m_color);
			Console.WriteRaw(fromUTF8(buffer));
		}

		TestCancel();
	}
}

// Redirects data sent to stdout/stderr into a pipe, and sets up a thread to
// forward data to the console.
class LinuxPipeRedirection : public PipeRedirectionBase
{
	DeclareNoncopyableObject(LinuxPipeRedirection);

protected:
	FILE* m_stdstream;
	FILE* m_fp;
	int m_pipe_fd[2];
	LinuxPipeThread m_thread;

	void Cleanup() noexcept;
public:
	LinuxPipeRedirection(FILE* stdstream);
	virtual ~LinuxPipeRedirection() noexcept;
};

LinuxPipeRedirection::LinuxPipeRedirection(FILE* stdstream)
	: m_stdstream(stdstream)
	, m_fp(nullptr)
	, m_pipe_fd{-1, -1}
	, m_thread(m_pipe_fd[0], stdstream == stderr ? Color_Red : Color_Black)
{
	pxAssert((stdstream == stderr) || (stdstream == stdout));

	const wxChar* stream_name = stdstream == stderr? L"stderr" : L"stdout";

	try {
		int stdstream_fd = fileno(stdstream);

		// Save the original stdout/stderr file descriptor...
		int dup_fd = dup(stdstream_fd);
		if (dup_fd == -1)
			throw Exception::RuntimeError().SetDiagMsg(wxString::Format(
				L"Redirect %s failed: dup: %s", stream_name, strerror(errno)));
		m_fp = fdopen(dup_fd, "w");
		if (m_fp == nullptr)
			throw Exception::RuntimeError().SetDiagMsg(wxString::Format(
				L"Redirect %s failed: fdopen: %s", stream_name, strerror(errno)));

		// and now redirect stdout/stderr.
		if (pipe(m_pipe_fd) == -1)
			throw Exception::RuntimeError().SetDiagMsg(wxString::Format(
				L"Redirect %s failed: pipe: %s", stream_name, strerror(errno)));
		if (dup2(m_pipe_fd[1], stdstream_fd) != stdstream_fd)
			throw Exception::RuntimeError().SetDiagMsg(wxString::Format(
				L"Redirect %s failed: dup2: %s", stream_name, strerror(errno)));
		close(m_pipe_fd[1]);
		m_pipe_fd[1] = stdstream_fd;

		// And send the final console output goes to the original stdout,
		// otherwise we'll have an infinite data loop.
		if (stdstream == stdout)
			Console_SetStdout(m_fp);

		m_thread.Start();
	} catch (Exception::BaseThreadError& ex) {
		// thread object will become invalid because of scoping after we leave
		// the constructor, so re-pack a new exception:
		Cleanup();
		throw Exception::RuntimeError().SetDiagMsg(ex.FormatDiagnosticMessage());
	} catch (...) {
		Cleanup();
		throw;
	}
}

LinuxPipeRedirection::~LinuxPipeRedirection() noexcept
{
	Cleanup();
}

void LinuxPipeRedirection::Cleanup() noexcept
{
	// Restore stdout/stderr file descriptor - mostly useful if the thread
	// fails to start, but then you have bigger issues to worry about.
	if (m_pipe_fd[1] != -1) {
		if (m_pipe_fd[1] == fileno(m_stdstream)) {
			// FIXME: Use lock for better termination.
			// The redirect thread is most likely waiting at read(). Send a
			// newline so it returns and the thread begins to terminate.
			fflush(m_stdstream);
			m_thread.Cancel();
			fputc('\n', m_stdstream);
			fflush(m_stdstream);
			dup2(fileno(m_fp), fileno(m_stdstream));
		} else {
			close(m_pipe_fd[1]);
		}
	}

	if (m_fp != nullptr) {
		if (m_stdstream == stdout)
			Console_SetStdout(stdout);
		fclose(m_fp);
	}

	if (m_pipe_fd[0] != -1)
		close(m_pipe_fd[0]);
}

PipeRedirectionBase* NewPipeRedir(FILE* stdstream)
{
	try {
		return new LinuxPipeRedirection(stdstream);
	} catch (Exception::RuntimeError& ex) {
		Console.Error(ex.FormatDiagnosticMessage());
	}

	return nullptr;
}
