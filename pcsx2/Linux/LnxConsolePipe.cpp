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

#include "common/StringUtil.h"
#include "gui/App.h"
#include "gui/ConsoleLogger.h"
#include <unistd.h>

using namespace Threading;

// Redirects and reads data sent to stdout/stderr and sends it to the PCSX2
// console.
class LinuxPipeThread : public pxThread
{
	typedef pxThread _parent;

protected:
	FILE* m_stdstream;
	FILE* m_fp;
	const ConsoleColors m_color;
	int m_pipe_fd[2];

	void ExecuteTaskInThread();
public:
	LinuxPipeThread(FILE* stdstream);
	virtual ~LinuxPipeThread();
};

LinuxPipeThread::LinuxPipeThread(FILE* stdstream)
	: pxThread(stdstream == stderr? L"Redirect_Stderr" : L"Redirect_Stdout")
	, m_stdstream(stdstream)
	, m_fp(nullptr)
	, m_color(stdstream == stderr? Color_Red : Color_Black)
	, m_pipe_fd{-1, -1}
{
	const wxChar* stream_name = stdstream == stderr? L"stderr" : L"stdout";

	// Save the original stdout/stderr file descriptor
	int dup_fd = dup(fileno(stdstream));
	if (dup_fd == -1)
		throw Exception::RuntimeError().SetDiagMsg(StringUtil::StdStringFromFormat(
			"Redirect %s failed: dup: %s", stream_name, strerror(errno)));
	m_fp = fdopen(dup_fd, "w");
	if (m_fp == nullptr) {
		int error = errno;
		close(dup_fd);
		throw Exception::RuntimeError().SetDiagMsg(StringUtil::StdStringFromFormat(
			"Redirect %s failed: fdopen: %s", stream_name, strerror(error)));
	}

	if (pipe(m_pipe_fd) == -1) {
		int error = errno;
		fclose(m_fp);
		throw Exception::RuntimeError().SetDiagMsg(StringUtil::StdStringFromFormat(
			"Redirect %s failed: pipe: %s", stream_name, strerror(error)));
	}
}

LinuxPipeThread::~LinuxPipeThread()
{
	// Close write end of the pipe first so the redirection thread starts
	// finishing up and restore the original stdout/stderr file descriptor so
	// messages aren't lost.
	dup2(fileno(m_fp), m_pipe_fd[1]);

	if (m_stdstream == stdout)
		Console_SetStdout(stdout);
	fclose(m_fp);

	// Read end of pipe should only be closed after the thread terminates to
	// prevent messages being lost.
	m_mtx_InThread.Wait();
	close(m_pipe_fd[0]);
}

void LinuxPipeThread::ExecuteTaskInThread()
{
	const wxChar* stream_name = m_stdstream == stderr? L"stderr" : L"stdout";

	// Redirect stdout/stderr
	int stdstream_fd = fileno(m_stdstream);
	if (dup2(m_pipe_fd[1], stdstream_fd) != stdstream_fd) {
		Console.Error(wxString::Format(L"Redirect %s failed: dup2: %s",
			stream_name, strerror(errno)));
		return;
	}

	close(m_pipe_fd[1]);
	m_pipe_fd[1] = stdstream_fd;

	// Send console output to the original stdout, otherwise there'll be an
	// infinite loop.
	if (m_stdstream == stdout)
		Console_SetStdout(m_fp);

	char buffer[2049];
	while (ssize_t bytes_read = read(m_pipe_fd[0], buffer, sizeof(buffer) - 1)) {
		if (bytes_read == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				// Should never happen.
				Console.Error(wxString::Format(L"Redirect %s failed: read: %s",
					stream_name, strerror(errno)));
				break;
			}
		}

		buffer[bytes_read] = 0;

		{
			ConsoleColorScope cs(m_color);
			Console.WriteRaw(fromUTF8(buffer));
		}
	}
}

class LinuxPipeRedirection : public PipeRedirectionBase
{
	DeclareNoncopyableObject(LinuxPipeRedirection);

protected:
	LinuxPipeThread m_thread;

public:
	LinuxPipeRedirection(FILE* stdstream);
	virtual ~LinuxPipeRedirection() = default;
};

LinuxPipeRedirection::LinuxPipeRedirection(FILE* stdstream)
	: m_thread(stdstream)
{
	pxAssert((stdstream == stderr) || (stdstream == stdout));

	try {
		m_thread.Start();
	} catch (Exception::BaseThreadError& ex) {
		// thread object will become invalid because of scoping after we leave
		// the constructor, so re-pack a new exception:
		throw Exception::RuntimeError().SetDiagMsg(ex.FormatDiagnosticMessage());
	}
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
