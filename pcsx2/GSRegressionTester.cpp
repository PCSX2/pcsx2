#include "GSRegressionTester.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/Timer.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <thread>
#include <atomic>
#include <algorithm>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cerrno>
#endif

static GSRegressionBuffer* regression_buffer; // Used by GS runner processes.
static GSBatchRunBuffer* batch_run_buffer; // Used by GS runner processes.
static std::size_t batch_run_index; // What number child this is in a batch run.

__forceinline static std::string VecToString(const std::vector<std::string>& v) {
	std::string str;
	for (std::size_t i = 0; i < v.size(); i++)
	{
		if (i > 0)
			str += ", ";
		str += "\"" + v[i] + "\"";
	}
	return "{ " + str + " }";
};

__forceinline static void CopyStringToBuffer(char* dst, std::size_t dst_size, const std::string& src)
{
	if (src.length() > dst_size - 1)
	{
		Console.Warning("String too long for buffer.");
	}

	std::size_t n = std::min(src.length(), dst_size - 1);
	std::memcpy(dst, src.c_str(), n);
	dst[n] = '\0';
}

GSIntSharedMemory::ValType GSIntSharedMemory::CompareExchange(ValType expected, ValType desired)
{
#ifdef _WIN32
	return InterlockedCompareExchange(&val, desired, expected);
#else
	val.compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_seq_cst);
	return expected;
#endif
}

GSIntSharedMemory::ValType GSIntSharedMemory::Get()
{
#ifdef _WIN32
	return InterlockedCompareExchange(&val, 0, 0);
#else
	return val.load(std::memory_order_seq_cst);
#endif
}

void GSIntSharedMemory::Set(ValType i)
{
#ifdef _WIN32
	InterlockedExchange(&val, i);
#else
	val.store(i, std::memory_order_seq_cst);
#endif
}

GSIntSharedMemory::ValType GSIntSharedMemory::FetchAdd()
{
#ifdef _WIN32
	return InterlockedIncrement(&val) - 1;
#else
	return val.fetch_add(1, std::memory_order_seq_cst);
#endif
}

void GSIntSharedMemory::Init(bool reset)
{
	// Need correct alignment.
	pxAssertRel(reinterpret_cast<size_t>(&val) % sizeof(ValType) == 0, "Atomic not aligned correctly");
#ifndef _WIN32
	if (!reset)
		new (&val) std::atomic<ValType>;
	pxAssertRel(std::atomic_is_lock_free(&val), "Atomic must be lock free.");
#endif
	Set(0);
}

std::size_t GSIntSharedMemory::GetTotalSize()
{
	return sizeof(GSIntSharedMemory);
}

void GSSpinlockSharedMemory::Init(bool reset)
{
	lock.Init(reset);
	lock.Set(WRITEABLE);
}

bool GSSpinlockSharedMemory::LockWrite(GSEvent* event, std::function<bool()> cond)
{
	while (true)
	{
		if (cond && cond())
			return false;

		if (lock.CompareExchange(WRITEABLE, WRITEABLE) == WRITEABLE) // Check the lock
			return true; // Acquired write.

		if (!cond) // Fail after 1 try when non-blocking.
			return false;

		if (event)
			event->Wait(GSRegressionBuffer::EVENT_WAIT_SECONDS);
		else
			std::this_thread::yield();
	}
}

bool GSSpinlockSharedMemory::LockRead(GSEvent* event, std::function<bool()> cond)
{
	while (true)
	{
		if (cond && cond())
			return false;

		if (lock.CompareExchange(READABLE, READABLE) == READABLE) // Check the lock
			return true; // Acquired read.

		if (!cond) // Fail after 1 try when non-blocking.
			return false;

		if (event)
			event->Wait(GSRegressionBuffer::EVENT_WAIT_SECONDS);
		else
			std::this_thread::yield();
	}
}

bool GSSpinlockSharedMemory::UnlockWrite()
{
	bool success = lock.CompareExchange(WRITEABLE, READABLE) == WRITEABLE;
	if (!success)
		pxFail("Trying to unlock write when not writeable.");
	return success;
}


bool GSSpinlockSharedMemory::UnlockRead()
{
	bool success = lock.CompareExchange(READABLE, WRITEABLE) == READABLE;
	if (!success)
		pxFail("Trying to unlock read when not readable.");
	return success;
}

bool GSSpinlockSharedMemory::Writeable()
{
	return lock.Get() == WRITEABLE;
}

bool GSSpinlockSharedMemory::Readable()
{
	return lock.Get() == READABLE;
}

bool GSSpinlockSharedMemory::Lock(GSEvent* event, std::function<bool()> cond)
{
	while (true)
	{
		if (cond && cond())
			return false;

		if (lock.CompareExchange(LOCKED, UNLOCKED) == UNLOCKED) // Check the lock
			return true; // Locked successfully.

		if (!cond)
			return false; // Fail after 1 try when non-blocking.

		if (event)
			event->Wait(GSRegressionBuffer::EVENT_WAIT_SECONDS);
		else
			std::this_thread::yield();
	}
}

bool GSSpinlockSharedMemory::Unlock()
{
	bool success = lock.CompareExchange(UNLOCKED, LOCKED) == LOCKED;
	if (!success)
		pxFail("Trying to unlock when not locked.");
	return success;
}

GSRegressionPacket* GSRegressionBuffer::GetPacketWrite(std::function<bool()> cond)
{
	event[RUNNER].Reset();

	if (!GetPacket(packet_write % num_packets)->lock.LockWrite(&event[RUNNER], cond))
		return nullptr;

	return GetPacket(packet_write % num_packets);
}

GSRegressionPacket* GSRegressionBuffer::GetPacketRead(std::function<bool()> cond)
{
	event[TESTER].Reset();

	if (!GetPacket(packet_read % num_packets)->lock.LockRead(&event[TESTER], cond))
		return nullptr;

	return GetPacket(packet_read % num_packets);
}

void GSRegressionBuffer::DonePacketWrite()
{
	if (!GetPacket(packet_write % num_packets)->lock.UnlockWrite())
		pxFail("Unlock packet write is broken.");

	event[TESTER].Signal();

	packet_write++;
}

void GSRegressionBuffer::DonePacketRead()
{
	if (!GetPacket(packet_read % num_packets)->lock.UnlockRead())
		pxFail("Unlock packet read is broken.");

	event[RUNNER].Signal();

	packet_read++;
}

GSDumpFileSharedMemory* GSRegressionBuffer::GetDumpWrite(std::function<bool()> cond)
{
	event[TESTER].Reset();

	if (!GetDump(dump_write % num_dumps)->lock.LockWrite(&event[TESTER], cond))
		return nullptr;

	return GetDump(dump_write % num_dumps);
}

GSDumpFileSharedMemory* GSRegressionBuffer::GetDumpRead(std::function<bool()> cond)
{
	event[RUNNER].Reset();

	if (!GetDump(dump_read % num_dumps)->lock.LockRead(&event[RUNNER], cond))
		return nullptr;

	return GetDump(dump_read % num_dumps);
}

void GSRegressionBuffer::DoneDumpWrite()
{
	if (!GetDump(dump_write % num_dumps)->lock.UnlockWrite())
		pxFail("Unlock dump write is broken.");

	event[RUNNER].Signal();

	dump_write++;
}

void GSRegressionBuffer::DoneDumpRead()
{
	if (!GetDump(dump_read % num_dumps)->lock.UnlockRead())
		pxFail("Unlock dump read is broken.");

	event[TESTER].Signal();

	dump_read++;
}

std::size_t GSDumpFileSharedMemory::GetTotalSize(std::size_t dump_size)
{
	return sizeof(GSDumpFileSharedMemory) + dump_size;
}

void GSRegressionPacket::SetNamePacket(const std::string& path)
{
	SetName(name_packet, path);
}

void GSRegressionPacket::SetNameDump(const std::string& path)
{
	SetName(name_dump, path);
}

void GSRegressionPacket::SetName(char* dst, const std::string& path)
{
	std::string name(Path::GetFileName(path));

	CopyStringToBuffer(dst, name_size, path);
}

void GSRegressionPacket::SetImage(const void* src, int w, int h, int pitch, int bytes_per_pixel)
{
	if (src)
	{
		std::size_t src_size = h * pitch;

		if (src_size > packet_size)
		{
			Console.WarningFmt(
				"Image data is too large for regression packet (w={}, h={}, pitch={}, bytes_per_pixel={}, buffer size={}).",
				w, h, pitch, bytes_per_pixel, packet_size);
			
			h = std::min(static_cast<int>(packet_size / pitch), h);

			src_size = h * pitch;

			Console.WarningFmt(
				"Cropping image to (w={}, h={}, pitch={}, bytes_per_pixel={}, buffer size={}).",
				w, h, pitch, bytes_per_pixel, packet_size);

			pxAssertRel(src_size <= packet_size, "Cropped image still too large (impossible).");
		}

		std::memcpy(GetData(), src, src_size);

	}

	this->type = IMAGE;
	this->image_header.w = w;
	this->image_header.h = h;
	this->image_header.pitch = pitch;
	this->image_header.bytes_per_pixel = bytes_per_pixel;
}

void GSRegressionPacket::SetHWStat(const HWStat& hwstat)
{
	this->type = HWSTAT;
	this->hwstat = hwstat;
}

void GSRegressionPacket::SetDoneDump()
{
	this->type = DONE_DUMP;
}

void GSRegressionPacket::Init(std::size_t packet_size, bool reset)
{
	std::memset(GetData(), 0, packet_size);
	lock.Init(reset);
	this->type = NONE;
	std::memset(name_dump, 0, sizeof(name_dump));
	std::memset(name_packet, 0, sizeof(name_packet));
	this->packet_size = packet_size;
};

std::size_t GSRegressionPacket::GetTotalSize(std::size_t packet_size)
{
	return sizeof(GSRegressionPacket) + packet_size;
}

std::string GSRegressionPacket::GetNameDump()
{
	name_dump[std::size(name_dump) - 1] = '\0';

	return std::string(name_dump);
}

std::string GSRegressionPacket::GetNamePacket()
{
	name_packet[std::size(name_packet) - 1] = '\0';

	return std::string(name_packet);
}

void* GSRegressionPacket::GetData()
{
	return reinterpret_cast<u8*>(this) + sizeof(GSRegressionPacket);
}

const void* GSRegressionPacket::GetData() const
{
	return reinterpret_cast<const u8*>(this) + sizeof(GSRegressionPacket);
}

// Only once before sharing. Not thread safe.
void GSDumpFileSharedMemory::Init(std::size_t dump_size, bool reset)
{
	lock.Init(reset);
	std::memset(name, 0, sizeof(name));
	this->dump_size = dump_size;
	std::memset(GetPtrDump(), 0, dump_size);
}

static std::size_t GetTotalSize(std::size_t dump_size)
{
	return sizeof(GSDumpFileSharedMemory) + dump_size;
}

void* GSDumpFileSharedMemory::GetPtrDump()
{
	return reinterpret_cast<u8*>(this) + sizeof(GSDumpFileSharedMemory);
}

std::size_t GSDumpFileSharedMemory::GetSizeDump()
{
	return dump_size;
}

void GSDumpFileSharedMemory::SetSizeDump(std::size_t size)
{
	dump_size = size;
}

std::string GSDumpFileSharedMemory::GetNameDump()
{
	name[std::size(name) - 1] = '\0';

	return std::string(name);
}

void GSDumpFileSharedMemory::SetNameDump(const std::string& str)
{
	CopyStringToBuffer(name, name_size, str);
}

std::size_t GSRegressionBuffer::GetTotalSize(std::size_t num_packets, std::size_t packet_size, std::size_t num_dumps, std::size_t dump_size)
{
	return num_packets * GSRegressionPacket::GetTotalSize(packet_size) +
		num_dumps * GSDumpFileSharedMemory::GetTotalSize(dump_size) +
		num_states * GSIntSharedMemory::GetTotalSize();
}

void GSRegressionBuffer::SetSizesPointers(std::size_t num_packets, std::size_t packet_size, std::size_t num_dumps, std::size_t dump_size)
{
	std::size_t packet_offset;
	std::size_t dump_file_offset;
	std::size_t state_offset;

	const std::size_t start_offset = reinterpret_cast<std::size_t>(shm.data);
	std::size_t curr_offset = start_offset;

	packet_offset = curr_offset;
	curr_offset += num_packets * GSRegressionPacket::GetTotalSize(packet_size);

	dump_file_offset = curr_offset;
	curr_offset += num_dumps * GSDumpFileSharedMemory::GetTotalSize(dump_size);

	state_offset = curr_offset;
	curr_offset += num_states * GSIntSharedMemory::GetTotalSize();

	pxAssert(curr_offset - start_offset == GetTotalSize(num_packets, packet_size, num_dumps, dump_size));

	packets = reinterpret_cast<GSRegressionPacket*>(packet_offset);
	dumps = reinterpret_cast<GSDumpFileSharedMemory*>(dump_file_offset);
	state = reinterpret_cast<GSIntSharedMemory*>(state_offset);

	this->num_packets = num_packets;
	this->packet_size = packet_size;
	this->num_dumps = num_dumps;
	this->dump_size = dump_size;
}

bool GSRegressionBuffer::CreateFile_(
	const std::string& name,
	const std::string& event_runner_name,
	const std::string& event_tester_name,
	std::size_t num_packets,
	std::size_t packet_size,
	std::size_t num_dumps,
	std::size_t dump_size)
{
	if (!shm.CreateFile_(name, GetTotalSize(num_packets, packet_size, num_dumps, dump_size)))
		return false;

	if (!event[RUNNER].Create(event_runner_name))
	{
		CloseFile();
		return false;
	}

	// Should be created by tester process before.
	if (!event[TESTER].Open_(event_tester_name))
	{
		CloseFile();
		return false;
	}

	SetSizesPointers(num_packets, packet_size, num_dumps, dump_size);

	Init(name, num_packets, packet_size, num_dumps, dump_size);

	Reset();

	return true;
}

void GSRegressionBuffer::Init(
	const std::string& name,
	std::size_t num_packets,
	std::size_t packet_size,
	std::size_t num_dumps,
	std::size_t dump_size)
{
	for (std::size_t i = 0; i < num_packets; i++)
		GetPacket(i)->Init(packet_size);
	for (std::size_t i = 0; i < num_dumps; i++)
		GetDump(i)->Init(dump_size);
	for (std::size_t i = 0; i < num_states; i++)
		state[i].Init();
	for (std::size_t i = 0; i < num_events; i++)
		event[i].Reset();
}

void GSRegressionBuffer::Reset()
{
	this->packet_write = 0;
	this->packet_read = 0;
	this->dump_write = 0;
	this->dump_read = 0;
	this->dump_name.clear();

	for (std::size_t i = 0; i < num_packets; i++)
		GetPacket(i)->Init(packet_size, true);
	for (std::size_t i = 0; i < num_dumps; i++)
		GetDump(i)->Init(dump_size, true);
	for (std::size_t i = 0; i < num_states; i++)
		state[i].Set(DEFAULT);
	for (std::size_t i = 0; i < num_events; i++)
		event[i].Reset();
}

bool GSRegressionBuffer::OpenFile(
	const std::string& name,
	const std::string& event_runner_name,
	const std::string& event_tester_name,
	std::size_t num_packets,
	std::size_t packet_size,
	std::size_t num_dumps,
	std::size_t dump_size)
{
	if (!shm.OpenFile(name, GetTotalSize(num_packets, packet_size, num_dumps, dump_size)))
		return false;

	if (!event[RUNNER].Open_(event_runner_name))
	{
		CloseFile();
		return false;
	}

	if (!event[TESTER].Open_(event_tester_name))
	{
		CloseFile();
		return false;
	}

	SetSizesPointers(num_packets, packet_size, num_dumps, dump_size);

	return true;
}

void GSRegressionBuffer::DestroySharedMemory()
{
	for (std::size_t i = 0; i < num_packets; i++)
		GetPacket(i)->~GSRegressionPacket();

	for (std::size_t i = 0; i < num_dumps; i++)
		GetDump(i)->~GSDumpFileSharedMemory();

	for (std::size_t i = 0; i < num_states; i++)
		state[i].~GSIntSharedMemory();
}

bool GSRegressionBuffer::CloseFile()
{
	for (std::size_t i = 0; i < num_events; i++)
		event[i].Close();

	if (!shm.CloseFile())
		return false;

	packets = nullptr;
	dumps = nullptr;
	state = nullptr;
	packet_size = 0;
	num_packets = 0;
	dump_size = 0;
	num_dumps = 0;
	packet_write = 0;
	packet_read = 0;
	dump_write = 0;
	dump_read = 0;
	dump_name.clear();

	return true;
}

void GSRegressionBuffer::SetState(u32 which, GSRegressionBuffer::ProcessState s)
{
	state[which].Set(static_cast<GSIntSharedMemory::ValType>(s));
}

void GSRegressionBuffer::SetStateRunner(GSRegressionBuffer::ProcessState s)
{
	SetState(RUNNER, s);
}

void GSRegressionBuffer::SetStateTester(GSRegressionBuffer::ProcessState s)
{
	SetState(TESTER, s);
}

GSRegressionBuffer::ProcessState GSRegressionBuffer::GetState(u32 which)
{
	return static_cast<ProcessState>(state[which].Get());
};

GSRegressionBuffer::ProcessState GSRegressionBuffer::GetStateRunner()
{
	return GetState(RUNNER);
};

GSRegressionBuffer::ProcessState GSRegressionBuffer::GetStateTester()
{
	return GetState(TESTER);
};

void GSRegressionBuffer::SetNameDump(const std::string& name)
{
	dump_name = name;
}

std::string GSRegressionBuffer::GetNameDump()
{
	return dump_name;
}

void GSRegressionBuffer::SignalRunnerHeartbeat()
{
	state[RUNNER_HEARTBEAT].Set(ALIVE);
}

bool GSRegressionBuffer::CheckRunnerHeartbeat()
{
	return state[RUNNER_HEARTBEAT].Get() == ALIVE;
}

void GSRegressionBuffer::ResetRunnerHeartbeat()
{
	state[RUNNER_HEARTBEAT].Set(DEFAULT);
}

GSRegressionPacket* GSRegressionBuffer::GetPacket(std::size_t i)
{
	pxAssertMsg(i < num_packets, "Index out of bounds.");
	return reinterpret_cast<GSRegressionPacket*>(reinterpret_cast<u8*>(packets) + i * GSRegressionPacket::GetTotalSize(packet_size));
}

GSDumpFileSharedMemory* GSRegressionBuffer::GetDump(std::size_t i)
{
	pxAssertMsg(i < num_dumps, "Index out of bounds.");
	return reinterpret_cast<GSDumpFileSharedMemory*>(reinterpret_cast<u8*>(dumps) + i * GSDumpFileSharedMemory::GetTotalSize(dump_size));
}

void GSRegressionBuffer::DebugDumpBuffer()
{
	Console.WarningFmt("Dump buffer debug (write={}, read={}):", dump_write, dump_read);
	for (std::size_t i = 0; i < num_dumps; i++)
	{
		GSDumpFileSharedMemory* dump = GetDump(i);
		std::size_t name_size = strnlen(dump->name, std::size(dump->name));
		std::string name(dump->name, name_size);
		Console.WarningFmt("    {}: lock={} name='{}' size={}", i, GSSpinlockSharedMemory::GetStateStrLock(dump->lock.lock.Get()), name, dump->dump_size);
	}
}

void GSRegressionBuffer::DebugPacketBuffer()
{
	Console.WarningFmt("Packet buffer debug (write={}, read={}):", packet_write, packet_read);
	for (std::size_t i = 0; i < num_packets; i++)
	{
		GSRegressionPacket* packet = GetPacket(i);

		std::size_t name_packet_size = strnlen(packet->name_packet, std::size(packet->name_packet));
		std::size_t name_dump_size = strnlen(packet->name_dump, std::size(packet->name_dump));

		std::string name_packet(packet->name_packet, name_packet_size);
		std::string name_dump(packet->name_dump, name_dump_size);

		Console.WarningFmt("    {}: lock={} name_dump='{}' name_packet='{}'", i, GSSpinlockSharedMemory::GetStateStrRW(packet->lock.lock.Get()), name_dump, name_packet);
	}
}

void GSRegressionBuffer::DebugState()
{
	Console.WarningFmt("runner_state={} tester_state={}", GetStateStr(GetStateRunner()), GetStateStr(GetStateTester()));
}

bool GSProcess::Start(const std::vector<std::string>& args, bool detached)
{
#ifdef _WIN32
	std::memset(&si, 0, sizeof(STARTUPINFO));
	si.cb = sizeof(STARTUPINFO);
	std::memset(&pi, 0, sizeof(PROCESS_INFORMATION));

	std::vector<std::string> args_quoted;
	for (const std::string& arg : args)
	{
		// Only quote args with spaces.
		if (arg.find(' ') != std::string::npos)
			args_quoted.push_back("\"" + arg + "\"");
		else
			args_quoted.push_back(arg);
	}
	std::string command = StringUtil::JoinString(args_quoted.begin(), args_quoted.end(), " ");
	std::wstring wcommand = StringUtil::UTF8StringToWideString(command);
	std::vector<wchar_t> wcommand_buf(wcommand.begin(), wcommand.end());
	wcommand_buf.push_back(L'\0');

	HANDLE hNull = INVALID_HANDLE_VALUE; // For redirecting child's stdout/err.

	ScopedGuard close_null([&]() {
		if (hNull != INVALID_HANDLE_VALUE)
			CloseHandle(hNull);
	});

	if (detached)
	{
		// Redirect stdout/err/in to null.

		hNull = CreateFileA(
			"NUL",
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (hNull == INVALID_HANDLE_VALUE)
		{
			Console.Error("Unable to open null handle");
			return false;
		}

		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdError = hNull;
		si.hStdOutput = hNull;
		si.hStdInput = hNull;
	}

	if (!CreateProcessW(
			NULL,
			wcommand_buf.data(),
			NULL,
			NULL,
			TRUE,
			(detached ? (DETACHED_PROCESS | CREATE_NO_WINDOW) : (DWORD)0),
			NULL,
			NULL,
			&si,
			&pi))
	{
		Console.ErrorFmt("Unable to create runner process with args: {}", VecToString(args));
		return false;
	}

	this->command = command;

	return true;
#else
	pid = fork();
	if (pid == -1)
	{
		pid = 0;
		Console.Error("Unable to fork.");
		return false;
	}
	else if (pid == 0)
	{
		// Child

		// Detach from console if needed.
		if (detached)
		{
			if (setsid() < 0)
				Console.ErrorFmt("Unable to detach console.");
			int fd = open("/dev/null", O_RDWR);
			if (fd >= 0)
			{
				if (dup2(fd, STDIN_FILENO) < 0)
					Console.ErrorFmt("Failed to redirect stdin to /dev/null.");
				if (dup2(fd, STDOUT_FILENO) < 0)
					Console.ErrorFmt("Failed to redirect stdout to /dev/null.");
				if (dup2(fd, STDERR_FILENO) < 0)
					Console.ErrorFmt("Failed to redirect stderr to /dev/null.");
			}
			else
			{
				Console.ErrorFmt("Unable to open /dev/null.");
			}

			if (fd > STDERR_FILENO)
				close(fd);
		}

		// Get args
		std::vector<const char*> args_cstr;
		for (const std::string& arg : args)
			args_cstr.push_back(arg.c_str());
		args_cstr.push_back(nullptr);

		// Execute
		if (execvp(args_cstr[0], const_cast<char *const *>(args_cstr.data())) < 0)
		{
			Console.ErrorFmt("Unable to execute child process with args {}.", VecToString(args));
			_exit(1);
		}
	}
	else
	{
		this->command = args[0];
		return true;
	}
	return false;
#endif
}

bool GSProcess::IsRunning(Handle_t handle, bool reap_child, int* status)
{
#ifdef _WIN32
	DWORD ret = WaitForSingleObject(handle, 0);
	if (ret == WAIT_FAILED)
	{
		Console.ErrorFmt("WaitForSingleObject for handle failed (error: {}).", GetLastError());
		return false;
	}
	return ret == WAIT_TIMEOUT;
#else
	if (reap_child)
	{
    pid_t r = waitpid(handle, status, WNOHANG);
    if (r == 0)
			return true; // child still running
    else if (r == handle)
			return false; // child exited and reaped
		// fallthrough
	}
	if (kill(handle, 0) == 0)
	{
		return true;
	}
	else if (errno == ESRCH)
	{
		return false;
	}
	else if (errno == EPERM)
	{
		Console.ErrorFmt("IsRunning: Do not have permissions for process {}.", handle);
		return true;
	}
	return false;
#endif
}

bool GSProcess::IsRunning(bool reap_child)
{
#ifdef _WIN32
	if (!pi.hProcess)
	{
		Console.ErrorFmt("IsRunning: Do not have a valid handle.");
		return false;
	}
	return IsRunning(pi.hProcess);
#else
	if (!pid)
	{
		Console.ErrorFmt("IsRunning: Do not have a valid PID.");
		return false;
	}
	return IsRunning(pid, reap_child, &status);
#endif
}

bool GSProcess::ExitedNormally()
{
#ifdef _WIN32
	DWORD exit_code = 0;
	if (!GetExitCodeProcess(pi.hProcess, &exit_code))
	{
		Console.ErrorFmt("GetExitCodeProcess for process {} failed.", pi.hProcess);
		return false;
	}
	return exit_code == EXIT_SUCCESS;
#else
	if (IsRunning())
		return false;
	waitpid(pid, &status, WNOHANG);
	if (WIFEXITED(status))
		return WEXITSTATUS(status) == EXIT_SUCCESS;
	else
		return false;
#endif
}

bool GSProcess::Close()
{
#ifdef _WIN32
	bool b1 = CloseHandle(pi.hProcess);
	bool b2 = CloseHandle(pi.hThread);
	pi.hProcess = nullptr;
	pi.hThread = nullptr;
	return b1 && b2;
#else
	waitpid(pid, &status, WNOHANG);
	pid = 0;
	status = 0;
	return true;
#endif
}

void GSProcess::Terminate()
{
#ifdef _WIN32
	if (!TerminateProcess(pi.hProcess, EXIT_FAILURE))
		Console.ErrorFmt("Unable to terminate process {}.", GetPID());
#else
	if (kill(pid, SIGKILL) < 0)
		Console.ErrorFmt("Unable to terminate process {}.", GetPID());
#endif
}

GSProcess::PID_t GSProcess::GetPID()
{
#ifdef _WIN32
	return pi.dwProcessId;
#else
	return pid;
#endif
}

GSProcess::PID_t GSProcess::current_pid = 0;
GSProcess::PID_t GSProcess::parent_pid = 0;
GSProcess::Handle_t GSProcess::parent_h = 0;

bool GSProcess::SetParentPID(PID_t pid)
{
#ifdef _WIN32
	parent_pid = pid;
	parent_h = OpenProcess(SYNCHRONIZE, FALSE, pid);

	if (!parent_h)
	{
		Console.ErrorFmt("Unable to open handle to parent PID {}.", parent_pid);
		parent_pid = 0;
		return false;
	}

	return IsRunning(parent_h);
#else
	parent_pid = pid;
	parent_h = pid;
	return IsRunning(parent_pid);
#endif
}

GSProcess::PID_t GSProcess::GetParentPID()
{
	return parent_pid;
}

GSProcess::PID_t GSProcess::GetCurrentPID()
{
	if (current_pid == 0)
	{
#ifdef _WIN32
		current_pid = GetCurrentProcessId();
#else
		current_pid = getpid();
#endif
	}
	return current_pid;
}

bool GSProcess::IsParentRunning()
{
	if (!parent_pid || !parent_h)
	{
		Console.ErrorFmt("Do not have a valid parent PID and/or handle.");
		return false;
	}
	return IsRunning(parent_h);
}

// Windows defines CreateFile as a macro so use CreateFile_.
bool GSSharedMemoryFile::CreateFile_(const std::string& name, std::size_t size)
{
#ifdef _WIN32
	handle = CreateFileMappingA(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		size,
		name.c_str());

	if (!handle)
	{
		Console.ErrorFmt("Failed to create shared memory file: {}", name);
		return false;
	}

	data = static_cast<GSRegressionPacket*>(
		MapViewOfFile(
			handle,
			FILE_MAP_WRITE,
			0,
			0,
			size));

	if (!data)
	{
		Console.ErrorFmt("Failed to map view of shared memory file: {}", name);
		CloseHandle(handle);
		return false;
	}
#else
	handle = shm_open(name.c_str(), O_CREAT | O_RDWR, 0666);
	if (handle < 0)
	{
		Console.ErrorFmt("Failed to create shared memory file: {}", name);
		return false;
	}
	if (ftruncate(handle, size) < 0)
	{
		Console.ErrorFmt("Faile to set size of shared memory file: {}", name);
		close(handle);
		return false;
	}

	data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);

	if (data == MAP_FAILED)
	{
		Console.ErrorFmt("Failed to map view of shared memory file: {}", name);
		close(handle);
		return false;
	}

	main = true;
#endif

	this->name = name;
	this->size = size;

	return true;
}

bool GSSharedMemoryFile::OpenFile(const std::string& name, std::size_t size)
{
	// Note: num_packets must match the value used in creation!
#ifdef _WIN32
	handle = OpenFileMappingA(FILE_MAP_WRITE, FALSE, name.c_str());
	if (!handle)
	{
		Console.ErrorFmt("Failed to open shared memory: {}", name);
		return false;
	}

	data = static_cast<GSRegressionPacket*>(MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, size));
	if (!data)
	{
		Console.ErrorFmt("Failed to map shared memory file: {}", name);
		CloseHandle(handle);
		return false;
	}

	Console.WriteLnFmt("Opened/mapped shared memory file: {}", name);
#else
	handle = shm_open(name.c_str(), O_RDWR, 0666);
	if (handle < 0)
	{
		Console.ErrorFmt("Failed to open shared memory file: {}", name);
		return false;
	}

	data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);

	if (data == MAP_FAILED)
	{
		Console.ErrorFmt("Failed to map shared memory file: {}", name);
		close(handle);
		return false;
	}

	main = false;
#endif

	this->name = name;
	this->size = size;

	return true;
}

bool GSSharedMemoryFile::CloseFile()
{
	bool success = true;
#ifdef _WIN32
	if (data)
	{
		if (!UnmapViewOfFile(data))
		{
			success = false;
			Console.ErrorFmt("Failed to unmap shared memory file: {}", name);
		}
	}

	if (handle)
	{
		if (!CloseHandle(handle))
		{
			success = false;
			Console.ErrorFmt("Failed to close shared memory file: {}.", name);
		}
	}
#else
	if (data)
	{
		if (munmap(data, size) < 0)
		{
			success = false;
			Console.ErrorFmt("Failed to unmap shared memory file {}", name);
		}
	}

	if (handle)
	{
		if (close(handle) < 0)
		{
			success = false;
			Console.ErrorFmt("Failed to close shared memory file: {}.", name);
		}
	}

	if (main)
	{
		if (shm_unlink(name.c_str()) < 0)
		{
			success = false;
			Console.ErrorFmt("Failed to unlink shared memory file: {}.", name);
		}
		main = false;
	}
#endif
	if (success)
		Console.WriteLnFmt("Closed/unmapped shared memory file: {}", name);

	name = "";
	handle = 0;
	data = nullptr;

	return success;
}

void GSSharedMemoryFile::ResetFile()
{
	std::memset(data, 0, size);
}

#ifdef _WIN32
std::wstring GSEvent::GetGlobalName(const std::string& name)
{
	return StringUtil::UTF8StringToWideString("Global\\" + name);
}
#endif

bool GSEvent::Create(const std::string& name)
{
#ifdef _WIN32
	std::wstring name_global = GetGlobalName(name);

	handle = CreateEvent(
		nullptr,
		TRUE,
		FALSE,
		name_global.c_str());

	if (!handle)
	{
		Console.ErrorFmt("Failed to create event: {} (error: {}).", name, GetLastError());
		return false;
	}

	this->name = name;

	return true;
#else
	return false; // Not implemented.
#endif
}

std::string GSEvent::GetName()
{
#ifdef _WIN32
	return name;
#else
	return ""; // Not implemented.
#endif
}

bool GSEvent::Open_(const std::string& name)
{
#ifdef _WIN32
	std::wstring name_global = GetGlobalName(name);

	handle = OpenEvent(
		SYNCHRONIZE | EVENT_MODIFY_STATE,
		FALSE,
		name_global.c_str());

	if (!handle)
	{
		Console.ErrorFmt("Failed to open event: {} (error: {}).", name, GetLastError());
		return false;
	}

	this->name = name;

	return true;
#else
	return false;
#endif
}

bool GSEvent::Close() const
{
#ifdef _WIN32
	return CloseHandle(handle);
#else
	return false; // Not implemented.
#endif
}

bool GSEvent::Signal() const
{
#ifdef _WIN32
	if (!SetEvent(handle))
	{
		Console.ErrorFmt("Set event {} failed (Windows error: {}).", name, GetLastError());
		return false;
	}

	return true;
#else
	return false; // Not implemented
#endif
}

bool GSEvent::Wait(double seconds) const
{
#ifdef _WIN32
	DWORD status = WaitForSingleObject(handle, static_cast<DWORD>(seconds * 1000.0));

	if (status == WAIT_FAILED)
	{
		Console.ErrorFmt("Waiting for event {} failed (Windows error: {}).", name, GetLastError());
		return false;
	}

	return status != WAIT_TIMEOUT;
#else
	return false; // Not implemented.
#endif
}

bool GSEvent::Reset() const
{
#ifdef _WIN32
	if (!ResetEvent(handle))
	{
		Console.ErrorFmt("Set event {} failed (Windows error: {}).", name, GetLastError());
		return false;
	}

	return true;
#else
	return false; // Not implemented.
#endif
}

void GSBatchRunBuffer::SetSizesPointers(std::size_t num_files, std::size_t num_runners)
{
	std::size_t head_offset;
	std::size_t file_status_offset;
	std::size_t filenames_offset;
	std::size_t state_parent_offset;
	std::size_t state_runner_offset;
	std::size_t runner_heartbeats_offset;

	std::size_t start_offset = reinterpret_cast<std::size_t>(shm.data);
	std::size_t curr_offset = start_offset;

	head_offset = curr_offset;
	curr_offset += sizeof(GSIntSharedMemory);

	file_status_offset = curr_offset;
	curr_offset += num_files * sizeof(GSIntSharedMemory);

	filenames_offset = curr_offset;
	curr_offset += num_files * filename_size;

	state_parent_offset = curr_offset;
	curr_offset += num_runners * sizeof(GSIntSharedMemory);

	state_runner_offset = curr_offset;
	curr_offset += num_runners * sizeof(GSIntSharedMemory);

	runner_heartbeats_offset = curr_offset;
	curr_offset += num_runners * sizeof(GSIntSharedMemory);

	pxAssert(curr_offset - start_offset == GetTotalSize(num_files, num_runners));

	head = reinterpret_cast<GSIntSharedMemory*>(head_offset);
	file_status = reinterpret_cast<GSIntSharedMemory*>(file_status_offset);
	filenames = reinterpret_cast<char*>(filenames_offset);
	state_parent = reinterpret_cast<GSIntSharedMemory*>(state_parent_offset);
	state_runner = reinterpret_cast<GSIntSharedMemory*>(state_runner_offset);
	state_runner_heartbeat = reinterpret_cast<GSIntSharedMemory*>(runner_heartbeats_offset);

	this->num_files = num_files;
	this->num_runners = num_runners;
}

bool GSBatchRunBuffer::CreateFile_(const std::string& name, std::size_t num_files, std::size_t num_runners)
{
	if (!shm.CreateFile_(name, GetTotalSize(num_files, num_runners)))
		return false;

	SetSizesPointers(num_files, num_runners);

	Init();
	return true;
}

bool GSBatchRunBuffer::OpenFile(const std::string& name, std::size_t num_files, std::size_t num_runners)
{
	if (!shm.OpenFile(name, GetTotalSize(num_files, num_runners)))
		return false;

	SetSizesPointers(num_files, num_runners);
	CreateFileIndexMap();
	return true;
}

void GSBatchRunBuffer::DestroySharedMemory()
{
	for (std::size_t i = 0; i < num_files; i++)
		file_status[i].~GSIntSharedMemory();

	for (std::size_t i = 0; i < num_runners; i++)
	{
		state_parent[i].~GSIntSharedMemory();
		state_runner[i].~GSIntSharedMemory();
		state_runner_heartbeat[i].~GSIntSharedMemory();
	}
}

bool GSBatchRunBuffer::CloseFile()
{
	if (!shm.CloseFile())
		return false;

	head = nullptr;
	filenames = nullptr;
	num_files = 0;
	file_status = nullptr;
	state_parent = nullptr;
	state_runner = nullptr;
	state_runner_heartbeat = nullptr;
	num_runners = 0;
	filename_to_index.clear();

	return true;
}

bool GSBatchRunBuffer::Reset(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return false;

	state_parent[i].Init();
	state_runner[i].Init();
	state_runner_heartbeat[i].Init();

	return true;
}

void GSBatchRunBuffer::Init()
{
	head->Init();

	for (std::size_t i = 0; i < num_files; i++)
	{
		file_status[i].Init();
	}

	for (std::size_t i = 0; i < num_runners; i++)
	{
		state_parent[i].Init();
		state_runner[i].Init();
		state_runner_heartbeat[i].Init();
	}
}

bool GSBatchRunBuffer::AcquireFile(std::string& filename)
{
	std::size_t i = head->FetchAdd();

	if (i >= num_files)
	{
		head->Set(num_files);
		filename.clear();
		return false;
	}
	else
	{
		filename = GetFilename(i);
		return true;
	}
}

bool GSBatchRunBuffer::CheckRunnerIndex(std::size_t i)
{
	if (i >= num_runners)
	{
		Console.ErrorFmt("GSBatchRunBuffer: Runner index out of bounds ({} / {}).", i, num_runners);
		pxFail("");
		return false;
	}

	return true;
}

bool GSBatchRunBuffer::CheckFileIndex(std::size_t i)
{
	if (i >= num_files)
	{
		Console.ErrorFmt("GSBatchRunBuffer: File index out of bounds ({} / {}).", i, num_files);
		pxFail("");
		return false;
	}

	return true;
}

GSBatchRunBuffer::FileStatus GSBatchRunBuffer::GetFileStatus(const std::size_t file_index)
{
	if (!CheckFileIndex(file_index))
		return ERROR_FS;

	return static_cast<FileStatus>(file_status[file_index].Get());
}

GSBatchRunBuffer::FileStatus GSBatchRunBuffer::GetFileStatus(const std::string& filename)
{
	auto it = filename_to_index.find(filename);
	if (it == filename_to_index.end())
		return ERROR_FS;

	return GetFileStatus(it->second);
}

void GSBatchRunBuffer::SetFileStatus(std::size_t file_index, FileStatus status)
{
	if (!CheckFileIndex(file_index))
		return;

	file_status[file_index].Set(static_cast<GSIntSharedMemory::ValType>(status));
}

void GSBatchRunBuffer::SetFileStatus(const std::string& filename, FileStatus status)
{
	auto it = filename_to_index.find(filename);
	if (it == filename_to_index.end())
	{
		Console.ErrorFmt("GSBatchRunBuffer: SetFileStatus() filename not found: {}.", filename);
		return;
	}
	SetFileStatus(it->second, status);
}

std::string GSBatchRunBuffer::GetFilename(std::size_t i)
{
	if (!CheckFileIndex(i))
		return "";

	char* s = filenames + i * filename_size;
	s[filename_size - 1] = '\0';
	return std::string(s);
}

bool GSBatchRunBuffer::PopulateFilenames(const std::vector<std::string>& filenames_in)
{
	if (filenames_in.size() != num_files)
	{
		Console.ErrorFmt("GSBatchRunBuffer: PopulateFilenames() incorrect number of strings ({} / {})", filenames_in.size(), num_files);
		return false;
	}
	for (std::size_t i = 0; i < num_files; i++)
	{
		char* s = filenames + i * filename_size;
		CopyStringToBuffer(s, filename_size, filenames_in[i]);
	}
	CreateFileIndexMap();
	return true;
}

void GSBatchRunBuffer::CreateFileIndexMap()
{
	filename_to_index.clear();
	for (std::size_t i = 0; i < num_files; i++)
	{
		std::string filename = GetFilename(i);
		filename_to_index[filename] = i;
	}
}

void GSBatchRunBuffer::SignalRunnerHeartbeat(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return;

	state_runner_heartbeat[i].Set(ALIVE);
}

bool GSBatchRunBuffer::CheckRunnerHeartbeat(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return false;

	return state_runner_heartbeat[i].Get() == ALIVE;
}

void GSBatchRunBuffer::ResetRunnerHeartbeat(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return;

	state_runner_heartbeat[i].Set(DEFAULT_RH);
}

void GSBatchRunBuffer::SetStateChild(std::size_t i, ProcessState state)
{
	if (!CheckRunnerIndex(i))
		return;

	state_runner[i].Set(static_cast<GSIntSharedMemory::ValType>(state));
}

void GSBatchRunBuffer::SetStateParent(std::size_t i, ProcessState state)
{
	if (!CheckRunnerIndex(i))
		return;

	state_parent[i].Set(static_cast<GSIntSharedMemory::ValType>(state));
}

GSBatchRunBuffer::ProcessState GSBatchRunBuffer::GetStateChild(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return ERROR_PS;

	return static_cast<ProcessState>(state_runner[i].Get());
}

GSBatchRunBuffer::ProcessState GSBatchRunBuffer::GetStateParent(std::size_t i)
{
	if (!CheckRunnerIndex(i))
		return ERROR_PS;

	return static_cast<ProcessState>(state_parent[i].Get());
}

std::size_t GSBatchRunBuffer::GetTotalSize(std::size_t num_files, std::size_t num_runners)
{
	return
		(filename_size + sizeof(GSIntSharedMemory)) * num_files + // File status and names.
		sizeof(GSIntSharedMemory) * (3 * num_runners + 1); // Runner parent/child/child-heartbeat status and head.
}

bool GSIsRegressionTesting()
{
	return regression_buffer != nullptr;
}

// Start regression testing within the producer/GS runner process.
bool GSStartRegressionTest(
	GSRegressionBuffer* rbp,
	const std::string& fn,
	const std::string& event_name_runner,
	const std::string& event_name_tester,
	std::size_t num_packets,
	std::size_t packet_size,
	std::size_t num_dumps,
	std::size_t dump_size)
{
	if (!rbp->OpenFile(
		fn,
		event_name_runner,
		event_name_tester,
		num_packets,
		packet_size,
		num_dumps,
		dump_size))
	{
		return false;
	}

	Console.WriteLnFmt("Opened {} for regression testing.", fn);

	regression_buffer = rbp;
	
	return true;
}

void GSEndRegressionTest()
{
	if (!regression_buffer)
	{
		pxFail("No regression buffer to close.");
		return;
	}

	if (!regression_buffer->CloseFile())
	{
		pxFail("Unable to end regression test.");
		return;
	}
}

GSRegressionBuffer* GSGetRegressionBuffer()
{
	return regression_buffer;
}

void GSSignalRunnerHeartbeat_RegressionTest()
{
	if (regression_buffer)
		regression_buffer->SignalRunnerHeartbeat();
	else
		Console.ErrorFmt("Not regression testing.");
}

int GSRegressionImageMemCmp(const GSRegressionPacket* p1, const GSRegressionPacket* p2)
{
	const GSRegressionPacket::ImageHeader& img1 = p1->image_header;
	const GSRegressionPacket::ImageHeader& img2 = p2->image_header;

	if (img1.w != img2.w || img1.h != img2.h || img1.bytes_per_pixel != img2.bytes_per_pixel)
		return INT_MAX; // Formats are different.

	int w = img1.w;
	int h = img2.h;
	int bytes_per_pixel = img1.bytes_per_pixel;
	return StringUtil::StrideMemCmp(p1->GetData(), img1.pitch, p2->GetData(), img2.pitch, w * bytes_per_pixel, h);
}

bool GSCheckTesterStatus_RegressionTest(bool exit, bool done_uploading)
{
	if (regression_buffer)
	{
		u32 state = regression_buffer->GetStateTester();
		return (exit && state == GSRegressionBuffer::EXIT) ||
		       (exit && !GSProcess::IsParentRunning()) ||
		       (done_uploading && state == GSRegressionBuffer::DONE_UPLOADING);
	}
	else
	{
		Console.ErrorFmt("Not regression testing.");
		return false;
	}
}

bool GSIsBatchRunning()
{
	return batch_run_buffer != nullptr;
}

GSBatchRunBuffer* GSGetBatchRunBuffer()
{
	if (!batch_run_buffer)
	{
		Console.ErrorFmt("Not batch running.");
	}
	return batch_run_buffer;
}

bool GSStartBatchRun(
	GSBatchRunBuffer* bbp,
	const std::string& fn,
	std::size_t num_files,
	std::size_t num_runners,
	std::size_t runner_index)
{
	if (runner_index >= num_runners)
	{
		Console.ErrorFmt("Runner index out of range ({} / {})", runner_index, num_runners);
		return false;
	}

	if (!bbp->OpenFile(fn, num_files, num_runners))
	{
		Console.ErrorFmt("Unable to open {} for batch running.", fn);
		return false;
	}

	Console.WriteLnFmt("Opened {} for batch running.", fn);

	batch_run_buffer = bbp;
	batch_run_index = runner_index;

	return true;
}

void GSEndBatchRun()
{
	if (!batch_run_buffer)
	{
		pxFail("No batch run buffer to close.");
		return;
	}

	if (!batch_run_buffer->CloseFile())
	{
		pxFail("Unable to end batch run.");
		return;
	}
}

bool GSCheckParentStatus_BatchRun()
{
	if (!batch_run_buffer)
	{
		Console.ErrorFmt("Not batch running.");
		return false;
	}

	return !GSProcess::IsParentRunning() ||
		batch_run_buffer->GetStateParent(batch_run_index) == GSBatchRunBuffer::EXIT;
}

void GSSignalRunnerHeartbeat_BatchRun()
{
	if (batch_run_buffer)
		batch_run_buffer->SignalRunnerHeartbeat(batch_run_index);
	else
		Console.ErrorFmt("Not batch running.");
}

void GSSetChildState_BatchRun(GSBatchRunBuffer::ProcessState state)
{
	if (batch_run_buffer)
		batch_run_buffer->SetStateChild(batch_run_index, state);
	else
		Console.ErrorFmt("Not batch running.");
}

bool GSBatchRunAcquireFile(std::string& filename)
{
	if (batch_run_buffer)
	{
		return batch_run_buffer->AcquireFile(filename);
	}
	else
	{
		Console.ErrorFmt("Not batch running.");
		return false;
	}
}