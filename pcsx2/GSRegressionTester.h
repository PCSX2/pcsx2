#pragma once
#include "common/Pcsx2Defs.h"

#include <map>
#include <mutex>
#include <functional>
#include <array>
#include <atomic>
#include <cstring>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#else
#include <sys/types.h>
#endif

// Atomic integer for inter-process shared memory since std::atomic is not guaranteed across processes.
struct GSIntSharedMemory
{

#ifdef _WIN32
	static_assert(sizeof(LONG) == sizeof(int));
	using ValType = LONG;
	ValType val;
#else
	using ValType = int;
	std::atomic<ValType> val;
#endif
	ValType CompareExchange(ValType expected, ValType desired);
	ValType Get();
	void Set(ValType i);
	ValType FetchAdd(); // Return value before increment.
	void Init(bool reset = false);
	static std::size_t GetTotalSize();
};

struct GSEvent
{
#ifdef _WIN32
	HANDLE handle = NULL;
	std::string name;

	static std::wstring GetGlobalName(const std::string& name);
#else
	// Not implemented;
#endif
	std::string GetName();
	bool Open_(const std::string& name);
	bool Create(const std::string& name);
	bool Close() const;
	bool Wait(double seconds) const;
	bool Reset() const;
	bool Signal() const;
};

// Spinlock using the inter-process atomic.
struct GSSpinlockSharedMemory
{
	// For producer/consumer semantics.
	enum : GSIntSharedMemory::ValType
	{
		WRITEABLE = 0, // Must be same as UNLOCKED.
		READABLE = 1
	};

	// For lock/unlock.
	enum : GSIntSharedMemory::ValType
	{
		UNLOCKED = 0, // Must be same as WRITEABLE.
		LOCKED = 1
	};

	static constexpr const char* GetStateStrRW(GSIntSharedMemory::ValType state)
	{
		switch (state)
		{
		case WRITEABLE: return "WRITEABLE";
		case READABLE: return "READABLE";
		default: return "UNKNOWN";
		}
	}

	static constexpr const char* GetStateStrLock(GSIntSharedMemory::ValType state)
	{
		switch (state)
		{
		case UNLOCKED: return "UNLOCKED";
		case LOCKED: return "LOCKED";
		default: return "UNKNOWN";
		}
	}

	GSIntSharedMemory lock;

	GSSpinlockSharedMemory();

	void Init(bool reset = false);
	bool LockWrite(GSEvent* event, std::function<bool()> cond);
	bool LockRead(GSEvent* event, std::function<bool()> cond);
	bool UnlockWrite();
	bool UnlockRead();
	bool Writeable();
	bool Readable();
	bool Lock(GSEvent* event, std::function<bool()> cond);
	bool Unlock();
};

// Packet holding data uploaded by runners for tester to consume and diff.
// Lives in shared memory.
struct GSRegressionPacket
{
	static constexpr std::size_t name_size = 4096;

	enum : u32
	{
		NONE = 0,
		IMAGE,
		HWSTAT,
		DONE_DUMP
	};

	struct alignas(32) HWStat
	{
		std::size_t frames;
		std::size_t draws;
		std::size_t render_passes;
		std::size_t barriers;
		std::size_t copies;
		std::size_t uploads;
		std::size_t readbacks;

		bool operator==(const HWStat& other)
		{
			return std::memcmp(this, &other, sizeof(HWStat)) == 0;
		};

		bool operator!=(const HWStat& other)
		{
			return !operator==(other);
		};
	};

	struct alignas(32) ImageHeader
	{
		std::size_t size;
		std::size_t w;
		std::size_t h;
		std::size_t pitch;
		std::size_t bytes_per_pixel;
	};

	GSSpinlockSharedMemory lock;
	u32 type;
	char name_dump[name_size];
	char name_packet[name_size];

	union
	{
		ImageHeader image_header;
		HWStat hwstat;
	};

	std::size_t packet_size;

	// Call by owner.
	void SetNameDump(const std::string& name);
	void SetNamePacket(const std::string& name);
	void SetName(char* dst, const std::string& name); // Helper (private)
	void SetImage(const void* src, int w, int h, int pitch, int bytes_per_pixel);
	void SetHWStat(const HWStat& hwstat);
	void SetDoneDump();
	std::string GetNameDump();
	std::string GetNamePacket();
	void* GetData();
	const void* GetData() const;

	// Call only once before sharing. Not thread safe.
	void Init(std::size_t packet_size, bool reset = false);

	// Static
	static std::size_t GetTotalSize(std::size_t packet_size);
};

// Cross-platform shared memory file for regression testing.
struct GSSharedMemoryFile
{
	std::string name = "";
	void* data = nullptr;
	std::size_t size;
#ifdef _WIN32
	using Handle_t = HANDLE;
#else
	bool main = false;
	using Handle_t = int;
#endif
	Handle_t handle; // Shared memory file handle.

	// Windows defines CreateFile as a macro so use CreateFile_.
	bool CreateFile_(const std::string& name, std::size_t size);
	bool OpenFile(const std::string& name, std::size_t size);
	bool CloseFile();
	void ResetFile();
};

// GSDumpFile that lives in shared memory. Allows the tester to read/decode dump
// files from disk once and upload for runners.
struct GSDumpFileSharedMemory
{
	static constexpr std::size_t name_size = 4096;

	GSSpinlockSharedMemory lock;
	char name[name_size];

	// Note: not the true dump size; just size of buffer.
	// The actual dump size is obtained by parsing the buffer.
	std::size_t dump_size;

	// Call only once before sharing. Not thread safe.
	void Init(std::size_t dump_size, bool reset = false);

	// Call by owner.
	void* GetPtrDump();
	std::size_t GetSizeDump();
	std::string GetNameDump();
	void SetSizeDump(std::size_t size);
	void SetNameDump(const std::string& str);

	// Static.
	static std::size_t GetTotalSize(std::size_t dump_size);
};

// Ring buffers of regression packets and dump files.
struct GSRegressionBuffer
{
	static constexpr double EVENT_WAIT_SECONDS = 15.0 / 1000.0;

	enum : u32
	{
		RUNNER = 0,
		TESTER,
		RUNNER_HEARTBEAT
	};

	enum ProcessState
	{
		DEFAULT = 0, // Both
		ALIVE, // Runner
		WRITE_DATA, // Runner
		WAIT_DUMP, // Runner
		DONE_RUNNING, // Running
		DONE_UPLOADING, // Tester
		EXIT, // Tester
		NUM_STATE_TYPES
	};

	static constexpr const char* GetStateStr(ProcessState state)
	{
		switch (state)
		{
		case DEFAULT: return "DEFAULT";
		case ALIVE: return "ALIVE";
		case WRITE_DATA: return "WRITE_DATA";
		case WAIT_DUMP: return "WAIT_DUMP";
		case DONE_RUNNING: return "DONE_RUNNING";
		case DONE_UPLOADING: return "DONE_UPLOADING";
		case EXIT: return "EXIT";
		default: return "UNKNOWN";
		}
	}

	GSSharedMemoryFile shm;

	static constexpr std::size_t num_events = 2;
	GSEvent event[num_events]; // For signaling runner, tester, and runner heartbeat.

	// (Runner) Owned by GS thread.
	void* packets = nullptr;
	std::size_t num_packets = 0;
	std::size_t packet_size = 0;
	std::size_t packet_write = 0;
	std::size_t packet_read = 0;

	// (Runner) Owned by main thread.
	void* dumps = nullptr;
	std::size_t num_dumps = 0;
	std::size_t dump_write = 0;
	std::size_t dump_read = 0;
	std::size_t dump_size = 0;
	std::string dump_name;

	// (Runner) Owned by GS thread.
	static constexpr std::size_t num_states = 3;
	GSIntSharedMemory* state; // Two states owned by runner and tester.

	// Private - set pointer/sizes. Only after shared memory file is opened/created.
	void SetSizesPointers(
		std::size_t num_packets,
		std::size_t packet_size,
		std::size_t num_dumps,
		std::size_t dump_size);

	// Call only once before sharing.
	bool CreateFile_(
		const std::string& name,
		const std::string& event_runner_name,
		const std::string& event_tester_name,
		std::size_t num_packets,
		std::size_t packet_size,
		std::size_t num_dumps,
		std::size_t dump_size);

	// Call only once by child.
	bool OpenFile(
		const std::string& name,
		const std::string& event_runner_name,
		const std::string& event_tester_name,
		std::size_t num_packets,
		std::size_t packet_size,
		std::size_t num_dumps,
		std::size_t dump_size);

	// Call only once by parent.
	void DestroySharedMemory();

	// Call when done with file.
	bool CloseFile();

	// Call only once to initialize.
	void Init(
		const std::string& name,
		std::size_t num_packets,
		std::size_t packet_size,
		std::size_t num_dumps,
		std::size_t dump_size);
	void Reset();

	// Thread safe; acquire ownership.
	// (Runner) GS thread only.
	GSRegressionPacket* GetPacketWrite(std::function<bool()> cond = nullptr);
	GSRegressionPacket* GetPacketRead(std::function<bool()> cond = nullptr);

	// Call only by owner to release ownership.
	// (Runner) GS thread only.
	void DonePacketWrite();
	void DonePacketRead();

	// Thread safe; acquire ownership.
	// (Runner) Main thread only.
	GSDumpFileSharedMemory* GetDumpWrite(std::function<bool()> cond = nullptr);
	GSDumpFileSharedMemory* GetDumpRead(std::function<bool()> cond = nullptr);

	// Call only by owner to release ownership.
	// (Runner) Main thread only.
	void DoneDumpWrite();
	void DoneDumpRead();

	// Thread safe.
	ProcessState GetState(u32 which);
	void SetState(u32 which, ProcessState state);
	ProcessState GetStateRunner();
	ProcessState GetStateTester();
	void SetStateRunner(ProcessState state); // (Runner) GS thread only.
	void SetStateTester(ProcessState state); // (Tester) Main thread only.

	// Local copy of dump name.
	void SetNameDump(const std::string& name); // (Runner) main thread only.
	std::string GetNameDump(); // (Runner) GS thread only.

	// Always safe to call.
	void SignalRunnerHeartbeat(); // Only use by runner.
	bool CheckRunnerHeartbeat(); // Only use by tester.
	void ResetRunnerHeartbeat(); // Only use by tester.

	// Unsafe, for private use only.
	GSRegressionPacket* GetPacket(std::size_t i);
	GSDumpFileSharedMemory* GetDump(std::size_t i);

	// Static.
	static std::size_t GetTotalSize(std::size_t num_packets, std::size_t packet_size, std::size_t num_dumps, std::size_t dump_size);

	// Debug; unsafe.
	void DebugDumpBuffer();
	void DebugPacketBuffer();
	void DebugState();
};

// Cross-platform process.
struct GSProcess
{
#ifdef _WIN32
	using PID_t = DWORD;
	using Handle_t = HANDLE;
	using Time_t = DWORD;
	static constexpr double infinite = static_cast<double>(0xFFFFFFFF);
#else
	using PID_t = pid_t;
	using Handle_t = pid_t;
	using Time_t = time_t;
	static constexpr double infinite = static_cast<double>(0x7FFFFFFF);
#endif

	static PID_t current_pid;
	static PID_t parent_pid;
	static Handle_t parent_h;

	std::string command;
#ifdef _WIN32
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else
	PID_t pid = 0;
	int status = 0;
#endif
	static bool IsRunning(Handle_t handle, bool reap_child = false, int* status = nullptr); // private
	
	bool Start(const std::vector<std::string>& commands, bool detached);

	bool IsRunning(bool reap_child = false);
	bool ExitedNormally();
	bool Close();
	void Terminate();
	PID_t GetPID();
	static bool SetParentPID(PID_t pid);
	static PID_t GetParentPID();
	static bool IsParentRunning();
	static PID_t GetCurrentPID();
};

// Simple, read-only queue in shared memory.
struct GSBatchRunBuffer
{
	enum RunnerHeartbeat : GSIntSharedMemory::ValType
	{
		DEFAULT_RH = 0,
		ALIVE
	};

	enum ProcessState : GSIntSharedMemory::ValType
	{
		DEFAULT_PS = 0,
		RUNNING,
		DONE_RUNNING,
		EXIT,
		ERROR_PS
	};

	// For file status.
	enum FileStatus : GSIntSharedMemory::ValType
	{
		NOT_STARTED = 0,
		STARTED,
		COMPLETED,
		ERROR_FS
	};

	static constexpr const char* GetRunnerHeartbeatStr(RunnerHeartbeat state)
	{
		switch (state)
		{
		case DEFAULT_RH: return "DEFAULT";
		case ALIVE: return "ALIVE";
		default: return "UNKNOWN";
		}
	};

	static constexpr const char* GetProcessStateStr(ProcessState state)
	{
		switch (state)
		{
		case DEFAULT_PS: return "DEFAULT";
		case RUNNING: return "RUNNING";
		case DONE_RUNNING: return "DONE_RUNNING";
		case EXIT: return "EXIT";
		case ERROR_PS: return "ERROR";
		default: return "UNKNOWN";
		}
	};

	static constexpr const char* GetFileStatusStr(FileStatus status)
	{
		switch (status)
		{
		case NOT_STARTED: return "NOT_STARTED";
		case STARTED: return "STARTED";
		case COMPLETED: return "COMPLETED";
		case ERROR_FS: return "ERROR";
		default: return "UNKNOWN";
		}
	};

	GSSharedMemoryFile shm;

	GSIntSharedMemory* head;
	
	char* filenames;
	std::size_t num_files;
	static constexpr std::size_t filename_size = 8192;

	GSIntSharedMemory* file_status;
	
	GSIntSharedMemory* state_parent;
	GSIntSharedMemory* state_runner;
	GSIntSharedMemory* state_runner_heartbeat;
	std::size_t num_runners;

	std::map<std::string, std::size_t> filename_to_index;

	// Private - only call once after creating/opening shared memory.
	void SetSizesPointers(std::size_t num_files, std::size_t num_runners);

	bool CreateFile_(const std::string& name, std::size_t num_files, std::size_t num_runners);
	bool OpenFile(const std::string& name, std::size_t num_files, std::size_t num_runners);
	void DestroySharedMemory(); // Only use by creator.
	bool CloseFile();
	void Init(); // Private
	bool Reset(std::size_t i); // Only use by parent.
	bool PopulateFilenames(const std::vector<std::string>& filenames);
	bool AcquireFile(std::string& filename);

	void CreateFileIndexMap(); // Private
	
	// Call only by owner of slot.
	FileStatus GetFileStatus(const std::string& filename);
	FileStatus GetFileStatus(std::size_t file_index);
	void SetFileStatus(const std::string& filename, FileStatus status);
	void SetFileStatus(std::size_t file_index, FileStatus status);

	// Private - only call by owner of slot.
	std::string GetFilename(std::size_t i);

	// Call any time.
	void SignalRunnerHeartbeat(std::size_t i); // Child only.
	bool CheckRunnerHeartbeat(std::size_t i); // Parent only.
	void ResetRunnerHeartbeat(std::size_t i); // Parent only.
	void SetStateParent(std::size_t i, ProcessState state); // Parent only.
	void SetStateChild(std::size_t i, ProcessState state); // Child only.
	ProcessState GetStateParent(std::size_t i); // Child only.
	ProcessState GetStateChild(std::size_t i); // Parent only.

	// Private - helper.
	bool CheckRunnerIndex(std::size_t i);
	bool CheckFileIndex(std::size_t i);

	// Static.
	static std::size_t GetTotalSize(std::size_t num_files, std::size_t num_runners);
};

/// Interface for runner process in regression test mode.
bool GSIsRegressionTesting();
bool GSStartRegressionTest(
	GSRegressionBuffer* rpb,
	const std::string& fn,
	const std::string& event_name_runner,
	const std::string& event_name_tester,
	std::size_t num_packets,
	std::size_t packet_size,
	std::size_t num_dumps,
	std::size_t dump_size);
void GSEndRegressionTest();
GSRegressionBuffer* GSGetRegressionBuffer();
bool GSCheckTesterStatus_RegressionTest(bool exit, bool done_uploading);
void GSSignalRunnerHeartbeat_RegressionTest();
int GSRegressionImageMemCmp(const GSRegressionPacket* p1, const GSRegressionPacket* p2);

// Interface for runner process in batch run mode.
bool GSIsBatchRunning();
bool GSStartBatchRun(
	GSBatchRunBuffer* buffer,
	const std::string& fn,
	std::size_t num_files,
	std::size_t num_runners,
	std::size_t runner_index
);
void GSEndBatchRun();
GSBatchRunBuffer* GSGetBatchRunBuffer();
bool GSCheckParentStatus_BatchRun();
void GSSignalRunnerHeartbeat_BatchRun();
void GSSetChildState_BatchRun(GSBatchRunBuffer::ProcessState state);
bool GSBatchRunAcquireFile(std::string& filename);