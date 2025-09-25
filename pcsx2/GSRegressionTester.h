#pragma once
#include "common/Pcsx2Defs.h"

#include <mutex>
#include <functional>
#include <array>
#include <atomic>
#include <cstring>

#ifdef __WIN32__
#include <windows.h>
#endif

// Atomic integer for inter-process shared memory since std::atomic is not guaranteed across processes.
struct GSIntSharedMemory
{

#ifdef __WIN32__
	using ValType = LONG;
	ValType val;
#else
	using ValType = long;
	std::atomic<ValType> val;
#endif
	ValType CompareExchange(ValType expected, ValType desired);
	ValType Get();
	void Set(ValType i);
	void Init();
	static std::size_t GetTotalSize();
};

struct GSEvent
{
#ifdef __WIN32__
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

	static constexpr std::array<const char*, 2> STATE_STR_WR = {
		"WRITEABLE",
		"READABLE"
	};

	GSIntSharedMemory lock;

	GSSpinlockSharedMemory();

	void Init();
	bool LockWrite(GSEvent* event, std::function<bool()> cond);
	bool LockRead(GSEvent* event, std::function<bool()> cond);
	bool UnlockWrite();
	bool UnlockRead();
	bool Writeable();
	bool Readable();

	// For lock/unlock.
	enum : GSIntSharedMemory::ValType
	{
		UNLOCKED = 0, // Must be same as WRITEABLE.
		LOCKED = 1
	};

	static constexpr std::array<const char*, 2> STATE_STR_LOCK = {
		"UNLOCKED",
		"LOCKED"
	};

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
	void Init(std::size_t packet_size);

	// Static
	static std::size_t GetTotalSize(std::size_t packet_size);
};

// Cross-platform shared memory file for regression testing.
struct GSSharedMemoryFile
{
	std::string name = "";
	void* data = nullptr;
	std::size_t size;
#ifdef __WIN32__
	HANDLE handle; // Handle to shared memory.
#else
	// Not implemented.
#endif

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
	void Init(std::size_t dump_size);

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

	enum : u32
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

	static constexpr std::array<const char*, NUM_STATE_TYPES> STATE_STR = []() {
		std::array<const char*, NUM_STATE_TYPES> arr;
		arr[DEFAULT] = "DEFAULT";
		arr[ALIVE] = "ALIVE";
		arr[WRITE_DATA] = "WRITE_DATA";
		arr[WAIT_DUMP] = "WAIT_DUMP";
		arr[DONE_RUNNING] = "DONE_RUNNING";
		arr[DONE_UPLOADING] = "DONE_UPLOADING";
		arr[EXIT] = "EXIT";
		return arr;
	}();

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
	u32 GetState(u32 which);
	void SetState(u32 which, u32 state);
	u32 GetStateRunner();
	u32 GetStateTester();
	void SetStateRunner(u32 state); // (Runner) GS thread only.
	void SetStateTester(u32 state);

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

// To be call by the runner process when in regression test mode.
bool GSIsRegressionTesting();
void GSStartRegressionTest(
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
bool GSCheckTesterStatus(bool exit, bool done_uploading);
void GSSignalRunnerHeartbeat();

// Used by the tester process to compare images.
int GSRegressionImageMemCmp(const GSRegressionPacket* p1, const GSRegressionPacket* p2);

// Cross-platform process.
struct GSProcess
{
#ifdef __WIN32__
	using PID_t = DWORD;
	using Handle_t = HANDLE;
	using Time_t = DWORD;
	static constexpr double infinite = static_cast<double>(0xFFFFFFFF);
#else
	using PID_t = int;
	using Handle_t = int;
	using Time_t = u32;
	static constexpr double infinite = static_cast<double>(0x7FFFFFFF);
#endif

	static PID_t current_pid;
	static PID_t parent_pid;
	static Handle_t parent_h;

	std::string command;
#ifdef __WIN32__
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
#else
	// Not implemented.
#endif
	static bool IsRunning(Handle_t handle, double seconds = 0.0); // private
	
	bool Start(const std::string& command, bool detached);

	bool IsRunning(double seconds = 0.0);
	bool WaitForExit(double seconds = infinite);
	bool Close();
	void Terminate();
	PID_t GetPID();
	static bool SetParentPID(PID_t pid);
	static PID_t GetParentPID();
	static bool IsParentRunning(double seconds = 0.0);
	static PID_t GetCurrentPID();
};