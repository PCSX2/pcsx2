#pragma once

#include "common/Pcsx2Defs.h"
#include "common/Timer.h"
#include "common/Threading.h"

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

class GSCompileJob
{
public:
	enum JobType
	{
		SHADER,
		PIPELINE,
	};

	GSCompileJob(JobType type) : m_job_type(type)
	{
		m_compile_timer.Reset();
	}

	bool IsShaderJob() { return m_job_type == SHADER; }
	bool IsPipelineJob() { return m_job_type == PIPELINE; }

	virtual ~GSCompileJob() {}

	bool IsDone() const { return m_done; }
	void SetDone() { m_done = true; }
	float GetCompileTimeMS() const { return static_cast<float>(m_compile_timer.GetTimeMilliseconds()); }
	u32 GetThreadID() const { return m_thread_id; }
	void SetThreadID(u32 id) { m_thread_id = id; }
private:
	JobType m_job_type;
	bool m_done = false; // Set when job is both compiled and cached.
	Common::Timer m_compile_timer; // Compile time for debugging.
	u32 m_thread_id = UINT_MAX; // Thread ID for debugging.
};

class GSShaderCompilerAsync
{
public:
	GSShaderCompilerAsync(u32 num_threads, u32 check_latency_ms);
	virtual ~GSShaderCompilerAsync();

	void GetCompletedJobs(std::vector<GSCompileJob*>& jobs);
	void StartCompileJobAsync(GSCompileJob* job);

protected:
	virtual void DoCompileJobSync(GSCompileJob* job, u32 thread_id) = 0;
	virtual void OnWorkersStarted() {}

	size_t GetNumThreads() { return m_worker_threads.size(); }

private:
	void StopWorkerThreads();

	// GSDevice and the shader cache own the jobs so use raw pointers.
	std::deque<GSCompileJob*> m_jobs_waiting;
	std::deque<GSCompileJob*> m_jobs_done;

	// Timer to limit amount of locking to check if jobs are ready.
	Common::Timer m_check_timer;
	u32 m_check_latency_ms = 20;

	std::vector<Threading::Thread> m_worker_threads;
	std::mutex m_mutex;
	std::condition_variable m_worker_cv;
	bool m_workers_stop = false;
	bool m_workers_started = false;

	void WorkerThreadFunc(u32 thread_id);
};
