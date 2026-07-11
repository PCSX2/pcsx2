#include "common/Assertions.h"

#include "fmt/format.h"

#include "GS/Renderers/Common/GSShaderCompilerAsync.h"

GSShaderCompilerAsync::GSShaderCompilerAsync(u32 num_threads, u32 check_latency_ms)
	: m_check_latency_ms(check_latency_ms)
{
	m_worker_threads.resize(num_threads);
}

void GSShaderCompilerAsync::GetCompletedJobs(std::vector<GSCompileJob*>& jobs)
{
	if (static_cast<u32>(m_check_timer.GetTimeMilliseconds()) < m_check_latency_ms)
		return;

	m_check_timer.Reset();

	std::lock_guard lock(m_mutex);

	while (!m_jobs_done.empty())
	{
		GSCompileJob* job = m_jobs_done.front();

		jobs.push_back(job);

		m_jobs_done.pop_front();
	}
}

void GSShaderCompilerAsync::StartCompileJobAsync(GSCompileJob* job)
{
	if (!m_workers_started)
	{
		m_workers_started = true;
		u32 thread_id = 0;
		for (Threading::Thread& t : m_worker_threads)
			t.Start([this, id = thread_id++]() { WorkerThreadFunc(id); });

		OnWorkersStarted();

		m_check_timer.Reset();
	}

	{
		std::lock_guard lock(m_mutex);

		m_jobs_waiting.push_back(job);

		m_worker_cv.notify_one();
	}
}

void GSShaderCompilerAsync::WorkerThreadFunc(u32 thread_id)
{
	std::string name = fmt::format("Shader Compiler {}", thread_id);
	Threading::SetNameOfCurrentThread(name.c_str());

	while (true)
	{
		GSCompileJob* job;
		
		// Acquire a waiting job.
		{
			std::unique_lock lock(m_mutex);

			m_worker_cv.wait(lock, [&]() {
				return m_workers_stop || !m_jobs_waiting.empty();
			});

			if (m_workers_stop)
				return;

			job = m_jobs_waiting.front();

			m_jobs_waiting.pop_front();
		}

		// Compile the job.
		DoCompileJobSync(job, thread_id);

		job->SetThreadID(thread_id); // For debugging
		
		// Release the completed job.
		{
			std::lock_guard lock(m_mutex);

			m_jobs_done.push_back(job);
		}
	}
}

void GSShaderCompilerAsync::StopWorkerThreads()
{
	// Set stop flag.
	{
		std::lock_guard lock(m_mutex);

		m_workers_stop = true;

		m_worker_cv.notify_all();
	}

	// Join threads.
	for (Threading::Thread& t : m_worker_threads)
	{
		if (t.Joinable())
			t.Join();
	}
}

GSShaderCompilerAsync::~GSShaderCompilerAsync()
{
	StopWorkerThreads();
}
