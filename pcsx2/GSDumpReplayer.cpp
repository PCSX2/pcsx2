// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS.h"
#include "GS/GSPerfMon.h"
#include "GS/GSState.h"
#include "GS/GSLzma.h"
#include "GS/Renderers/Common/GSRenderer.h"
#include "GSDumpReplayer.h"
#include "GameList.h"
#include "Gif.h"
#include "Gif_Unit.h"
#include "Host.h"
#include "ImGui/ImGuiManager.h"
#include "ImGui/ImGuiOverlays.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "VUmicro.h"
#include "GSRegressionTester.h"

#include "imgui.h"

#include "fmt/format.h"

#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Threading.h"
#include "common/Timer.h"
#include "common/ScopedGuard.h"

#include <filesystem>
#include <atomic>
#include <fstream>

void GSDumpReplayerCpuReserve();
void GSDumpReplayerCpuShutdown();
void GSDumpReplayerCpuReset();
void GSDumpReplayerCpuStep();
void GSDumpReplayerCpuExecute();
void GSDumpReplayerExitExecution();
void GSDumpReplayerCancelInstruction();
void GSDumpReplayerCpuClear(u32 addr, u32 size);

static std::unique_ptr<GSDumpFile> s_dump_file;
static std::string s_runner_name;
static std::string s_dump_dir;
static std::string s_dump_name;
static std::string s_dump_filename;
static std::vector<std::string> s_dump_file_list;
static u32 s_current_packet = 0;
static u32 s_dump_frame_number_max = 0;
static u32 s_dump_frame_number = 0;
static s32 s_dump_loop_count_start = 0;
static s32 s_dump_loop_count = 0;
static bool s_dump_running = false;
static bool s_needs_state_loaded = false;
static u64 s_frame_ticks = 0;
static u64 s_next_frame_time = 0;
static bool s_is_dump_runner = false;
static bool s_batch_mode = false;
static u32 s_num_batches = 0;
static u32 s_batch_id = 0;
static Pcsx2Config::GSOptions s_batch_gs_config;
static bool s_batch_recreate_device = true; // Safer to recreate but adds more overhead.
static std::string s_batch_start_from_dump;
static bool s_regression_test_send_hwstats = false; // Only send HWSTAT packets if the log is not written.
static bool s_verbose_logging = false;
static GSDumpFileLoader s_dump_file_loader; // For batch mode.
static std::size_t s_dump_buffer_size = SIZE_MAX; // For batch mode.
static bool s_lazy_dump = false;
static u32 s_lazy_dump_buffer_size = _1mb * 256;
static u32 s_lazy_dump_buffer_num_dumps = 1;
static GSDumpFileLoaderLazy s_dump_file_loader_lazy;

R5900cpu GSDumpReplayerCpu = {
	GSDumpReplayerCpuReserve,
	GSDumpReplayerCpuShutdown,
	GSDumpReplayerCpuReset,
	GSDumpReplayerCpuStep,
	GSDumpReplayerCpuExecute,
	GSDumpReplayerExitExecution,
	GSDumpReplayerCancelInstruction,
	GSDumpReplayerCpuClear};

static InterpVU0 gsDumpVU0;
static InterpVU1 gsDumpVU1;

bool GSDumpReplayer::IsReplayingDump()
{
	return static_cast<bool>(s_dump_file) || (IsRunner() && IsBatchMode());
}

bool GSDumpReplayer::IsRunner()
{
	return s_is_dump_runner;
}

bool GSDumpReplayer::IsBatchMode()
{
	return s_batch_mode;
}

void GSDumpReplayer::SetIsDumpRunner(bool is_runner, const std::string& runner_name)
{
	s_is_dump_runner = is_runner;
	if (is_runner)
		s_runner_name = runner_name;
}

std::string GSDumpReplayer::GetRunnerName()
{
	std::string name = s_runner_name;
	if (GSIsRegressionTesting())
		name += "/" + std::to_string(GSProcess::GetCurrentPID());
	if (s_batch_mode)
		//name += "/" + std::to_string(s_batch_id);
		name += "/" + std::to_string(GSProcess::GetCurrentPID()); // FIXME!!! PURGE OLD CODE OR MAKE SURE BOTH WORK CORRECTLY
	return name;
}

void GSDumpReplayer::SetVerboseLogging(bool verbose)
{
	s_verbose_logging = verbose;
}

bool GSDumpReplayer::IsVerboseLogging()
{
	return s_verbose_logging;
}

void GSDumpReplayer::SetIsBatchMode(bool batch_mode)
{
	s_batch_mode = batch_mode;
}

void GSDumpReplayer::SetNumBatches(u32 n_batches)
{
	s_num_batches = n_batches;
}

void GSDumpReplayer::SetBatchID(u32 batch_id)
{
	s_batch_id = batch_id;
}

void GSDumpReplayer::SetBatchDefaultGSOptions(const Pcsx2Config::GSOptions& gs_options)
{
	s_batch_gs_config = gs_options;
}

void GSDumpReplayer::SetBatchRecreateDevice(bool recreate)
{
	s_batch_recreate_device = recreate;
}

void GSDumpReplayer::SetBatchStartFromDump(const std::string& start_from_dump)
{
	s_batch_start_from_dump = start_from_dump;
}

void GSDumpReplayer::SetBatchRunnerLazyDump(size_t size, size_t num_dumps)
{
	s_lazy_dump = true;
	s_lazy_dump_buffer_size = size;
	s_lazy_dump_buffer_num_dumps = num_dumps;
}

void GSDumpReplayer::SetRegressionSendHWSTAT(bool send_hwstat)
{
	s_regression_test_send_hwstats = send_hwstat;
}

void GSDumpReplayer::SetDumpBufferSize(std::size_t size)
{
	s_dump_buffer_size = size;
}

void GSDumpReplayer::SetLoopCount(s32 loop_count)
{
	s_dump_loop_count = loop_count - 1;
}

void GSDumpReplayer::SetLoopCountStart(s32 loop_count)
{
	s_dump_loop_count_start = loop_count;
}

int GSDumpReplayer::GetLoopCount()
{
	return s_dump_loop_count;
}

void GSDumpReplayer::EndDumpRegressionTest()
{
	MTGS::RunOnGSThread([]() {
		pxAssert(GSIsRegressionTesting());

		GSRegressionBuffer* rbp = GSGetRegressionBuffer();

		rbp->SetStateRunner(GSRegressionBuffer::WRITE_DATA);
		ScopedGuard set_default([&]() {
			rbp->SetStateRunner(GSRegressionBuffer::DEFAULT);
		});

		// Note: must process only one packet sequentially or it will break ring buffer locking.

		if (GSIsHardwareRenderer())
		{
			if (s_regression_test_send_hwstats)
			{
				// Send HW stats packet
				GSRegressionPacket* packet_hwstat = nullptr;
				ScopedGuard done_hwstat([&]() {
					if (packet_hwstat)
						rbp->DonePacketWrite();
				});

				if (packet_hwstat = rbp->GetPacketWrite(std::bind(GSCheckTesterStatus_RegressionTest, true, false)))
				{
					const std::string name_dump = rbp->GetNameDump();
					packet_hwstat->SetNameDump(name_dump);
					packet_hwstat->SetNamePacket(name_dump + " HWStat");

					GSRegressionPacket::HWStat hwstat;
					hwstat.frames = 0; // FIXME
					hwstat.draws = g_perfmon.GetCounter(GSPerfMon::DrawCalls);
					hwstat.render_passes = g_perfmon.GetCounter(GSPerfMon::RenderPasses);
					hwstat.barriers = g_perfmon.GetCounter(GSPerfMon::Barriers);
					hwstat.copies = g_perfmon.GetCounter(GSPerfMon::TextureCopies);
					hwstat.uploads = g_perfmon.GetCounter(GSPerfMon::TextureUploads);
					hwstat.readbacks = g_perfmon.GetCounter(GSPerfMon::Readbacks);
					packet_hwstat->SetHWStat(hwstat);

					if (s_verbose_logging)
					{
						Console.WriteLnFmt("(GSDumpReplayer/{}) New regression packet: {} / {}",
							GetRunnerName(), packet_hwstat->GetNameDump(), packet_hwstat->GetNamePacket());
					}
				}
				else
				{
					Console.ErrorFmt("(GSDumpReplayer/{}) Failed to get regression packet for HW stats.", GetRunnerName());
				}
			}
		}

		{
			// Send done dump packet.
			GSRegressionPacket* packet_done_dump = nullptr;
			ScopedGuard done_done_dump([&]() {
				if (packet_done_dump)
					rbp->DonePacketWrite();
			});

			if (packet_done_dump = rbp->GetPacketWrite(std::bind(GSCheckTesterStatus_RegressionTest, true, false)))
			{
				const std::string name_dump = rbp->GetNameDump();
				packet_done_dump->SetNameDump(name_dump);
				packet_done_dump->SetNamePacket(name_dump + " Done");
				packet_done_dump->SetDoneDump();

				if (s_verbose_logging)
				{
					Console.WriteLnFmt("(GSDumpReplayer/{}) New regression packet: {} / {}",
						GetRunnerName(), packet_done_dump->GetNameDump(), packet_done_dump->GetNamePacket());
				}
			}
			else
			{
				Console.ErrorFmt("(GSDumpReplayer/{}) Failed to get regression packet for done dump signal.",
					GetRunnerName(), GSProcess::GetCurrentPID());
			}
		}
	});
}

bool GSDumpReplayer::GetDumpFileList(const std::string& dir, std::vector<std::string>& file_list, u32 nbatches, u32 batch_id,
	const std::string& start_from)
{
	if (nbatches == 0)
	{
		Console.ErrorFmt("(GSDumpReplayer/{}) Number of batches must be positive (got {}).", GetRunnerName(), nbatches);
		return false;
	}

	file_list.clear();

	if (!FileSystem::DirectoryExists(dir.c_str()))
	{
		Console.ErrorFmt("(GSDumpReplayer/{}) Directory does not exist: '{}'", GetRunnerName(), dir);
		return false;
	}

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(
		dir.c_str(),
		"*",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);

	std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
		return a.FileName < b.FileName;
	});

	std::erase_if(files, [](const auto& f) {
		return !VMManager::IsGSDumpFileName(f.FileName);
	});

	if (files.empty())
	{
		Console.ErrorFmt("(GSDumpReplayer/{}) Could not find any dumps in '{}'", GetRunnerName(), dir);
		return false;
	}

	for (u32 i = batch_id; i < files.size(); i += nbatches)
	{
		file_list.push_back(files[i].FileName);
	}

	if (!start_from.empty())
	{
		auto first = std::find_if(
			file_list.begin(),
			file_list.end(),
			[&start_from](const std::string& s) {
				return Path::GetFileName(s) >= start_from;
			}
		);

		std::vector<std::string> new_file_list(first, file_list.end());

		file_list = std::move(new_file_list);
	}

	Console.WriteLnFmt("(GSDumpReplayer/{}) Read a dump file list with {} dumps", GetRunnerName(), file_list.size());

	return true;
}

bool GSDumpReplayer::Initialize(const char* filename)
{
	if (GSIsRegressionTesting())
	{
		if (!ChangeDump())
			return false;
	}
	else if (GSIsBatchRunning())
	{
		if (!ChangeDump())
			return false;

		GSSetChildState_BatchRun(GSBatchRunBuffer::RUNNING);
	}
	else if (IsBatchMode())
	{
		if (!GetDumpFileList(filename, s_dump_file_list, s_num_batches, s_batch_id, s_batch_start_from_dump))
			return false;

		if (!ChangeDump())
			return false;
	}
	else
	{
		// Standard mode.
		if (!ChangeDump(filename))
			return false;
	}

	// We replace all CPUs.
	Cpu = &GSDumpReplayerCpu;
	psxCpu = &psxInt;
	CpuVU0 = &gsDumpVU0;
	CpuVU1 = &gsDumpVU1;

	// loop infinitely by default
	s_dump_loop_count = -1;

	return true;
}

void GSDumpReplayer::UpdateBatchGameSettings()
{

	EmuConfig.GS = s_batch_gs_config;

	const GameDatabaseSchema::GameEntry* game = GameDatabase::findGame(s_dump_file->GetSerial());
	if (game)
	{
		game->applyGSHardwareFixes(EmuConfig.GS);
	}

	// Re-remove upscaling fixes, make sure they don't apply at native res.
	// We do this in LoadCoreSettings(), but game fixes get applied afterwards because of the unsafe warning.
	EmuConfig.GS.MaskUpscalingHacks();

	MTGS::ApplySettings();
}
bool GSDumpReplayer::ChangeDump(const char* filename)
{
	if (GSIsRegressionTesting())
	{
		// Regression testing with dumps shared in memory.

		pxAssert(filename == nullptr);

		GSRegressionBuffer* rbp = GSGetRegressionBuffer();
		GSDumpFileSharedMemory* dump = nullptr;

		MTGS::RunOnGSThread([rbp]() {
			rbp->SetStateRunner(GSRegressionBuffer::WAIT_DUMP);
		});

		// Cleanup.
		ScopedGuard sg([&dump, rbp]() {
			if (s_dump_running)
			{
				MTGS::RunOnGSThread([rbp]() {
					rbp->SetStateRunner(GSRegressionBuffer::DEFAULT);
				});
			}
			else
			{
				MTGS::RunOnGSThread([rbp]() {
					rbp->SetStateRunner(GSRegressionBuffer::DONE_RUNNING);
				});
			}
			if (dump)
				rbp->DoneDumpRead();
		});

		MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
			Console.WriteLnFmt("(GSRunner/{}) Waiting for new dump.", runner_name);
		});

		// Acquire the dump slot from the shared memory ring buffer.
		Common::Timer timer;

		dump = rbp->GetDumpRead(); // First, one non-blocking check since otherwise the done uploading status is polled too early.
		if (!dump)
		{
			dump = rbp->GetDumpRead(std::bind(GSCheckTesterStatus_RegressionTest, true, true));
		}

		if (!dump)
		{
			u32 state_tester = rbp->GetStateTester();

			if (state_tester == GSRegressionBuffer::EXIT)
			{
				MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
					Console.WarningFmt("(GSRunner/{}) Got EXIT from tester.", runner_name);
				});
			}
			else if (state_tester == GSRegressionBuffer::DONE_UPLOADING)
			{
				MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
					Console.WriteLnFmt("(GSRunner/{}) Got DONE_UPLOADING from tester.", runner_name);
				});
			}
			else if (!GSProcess::IsParentRunning())
			{
				MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
					Console.ErrorFmt("(GSRunner/{}) Tester process exited.", runner_name);
				});
			}
			else
			{
				// Should be impossible.
				MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
					Console.ErrorFmt("(GSRunner/{}) Dump read failed for unknown reason.", runner_name);
				});
			}

			GSDumpReplayerExitExecution();
			return false;
		}

		MTGS::RunOnGSThread([runner_name = GetRunnerName(), sec = timer.GetTimeSeconds()]() {
			Console.WriteLnFmt("(GSRunner/{}) Waited {:.2} sec for dump.", runner_name, sec);
		});

		s_dump_name = dump->GetNameDump();

		MTGS::RunOnGSThread([dump_name = s_dump_name, rbp]() {
			rbp->SetNameDump(dump_name);
		});

		// Parse the dump file. Note: we cannot release the ring buffer slot until this is done
		// since we are using the data in-place.
		timer.Reset();

		GSDumpFile::OpenGSDumpMemory(s_dump_file, dump->GetPtrDump(), dump->GetSizeDump());

		Error error;
		if (!s_dump_file->ReadFile(&error))
		{
			MTGS::RunOnGSThread([runner_name = GetRunnerName(), dump_name = s_dump_name, err = error.GetDescription()]() {
				Console.ErrorFmt("(GSDumpReplayer/{}) Failed to read GS dump '{}' (error: {})", runner_name, dump_name, err);
			});
			// Don't try again. Otherwise, we will most likely we will get desynchronized from the other runner.
			return false;
		}

		MTGS::RunOnGSThread([runner_name = GetRunnerName(), dump_name = s_dump_name, sec = timer.GetTimeSeconds()]() {
			Console.WriteLnFmt("(GSDumpReplayer/{}) Read GS dump in '{}' ({:.2} sec)", runner_name, dump_name, sec);
		});

		GSSignalRunnerHeartbeat_RegressionTest();
	}
	else if (GSIsBatchRunning())
	{
		pxAssert(filename == nullptr);

		if (s_lazy_dump)
		{
			if (!s_dump_file_loader_lazy.Started())
				s_dump_file_loader_lazy.Start(s_lazy_dump_buffer_num_dumps, s_lazy_dump_buffer_size);

			const auto AcquireAndAddToLoader = [](GSDumpFileLoaderLazy& loader) {
				while (!loader.Full())
				{
					std::string file_str;
					if (!GSBatchRunAcquireFile(file_str))
						return false;
					Error error;
					GSDumpFileLoaderLazy::RetVal ret = loader.AddFile(file_str, &error);
					if (ret == GSDumpFileLoaderLazy::SUCCESS)
					{
						return true;
					}
					else if (ret == GSDumpFileLoaderLazy::FAILURE)
					{
						MTGS::RunOnGSThread([err = error.GetDescription(), runner_name = GetRunnerName()]() {
							Console.ErrorFmt("(GSRunner/{}) Error loading/reading dump: {}.", runner_name,
								err.empty() ? std::string("Unspecified reason") : err);
						});
						continue;
					}
					else if (ret == GSDumpFileLoaderLazy::FULL)
					{
						// Should be impossible.
						MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
							Console.ErrorFmt("(GSRunner/{}) Attempting to add to full dump loader.", runner_name);
						});
						return false;
					}
					else
					{
						MTGS::RunOnGSThread([err = error.GetDescription(), runner_name = GetRunnerName(), ret]() {
							Console.ErrorFmt("(GSRunner/{}) Unknown return value from loader: {}.", runner_name,
								static_cast<int>(ret));
						});
						continue;
					}
				}
				return false;
			};

			while (AcquireAndAddToLoader(s_dump_file_loader_lazy))
				;

			while (true)
			{
				Error error;
				GSDumpFileLoaderLazy::RetVal ret = s_dump_file_loader_lazy.GetFile(s_dump_file, s_dump_filename, &error);

				AcquireAndAddToLoader(s_dump_file_loader_lazy); // Add one more after getting one.

				if (ret == GSDumpFileLoaderLazy::SUCCESS)
				{
					break;
				}
				else if (ret == GSDumpFileLoaderLazy::EMPTY)
				{
					MTGS::RunOnGSThread([runner_name = GetRunnerName()]() {
						Console.WriteLnFmt("(GSRunner/{}) Finished all dumps", runner_name);
					});
					GSSetChildState_BatchRun(GSBatchRunBuffer::DONE_RUNNING);
					return false;
				}
				else if (ret == GSDumpFileLoaderLazy::FAILURE)
				{
					MTGS::RunOnGSThread([filename = s_dump_filename, runner_name = GetRunnerName(), err = error.GetDescription()]() {
						Console.ErrorFmt("(GSRunner/{}) Error loading/reading dump: {} (error: {}).", runner_name, filename, err);
					});
				}
				else
				{
					pxFail("Unknown return value."); // Impossible.
				}
			}

			s_dump_name = Path::GetFileName(s_dump_filename);
			GSGetBatchRunBuffer()->SetFileStatus(s_dump_filename, GSBatchRunBuffer::STARTED);

			MTGS::RunOnGSThread(
				[name = s_dump_name, size = s_dump_file->GetFileSize(), runner_name = GetRunnerName(), memory = GSProcess::GetMemoryUsageBytes()]() {
					Console.WriteLnFmt("(GSRunner/{}) Loaded dump '{}' ({:.2} MB)", runner_name, name, static_cast<double>(size) / _1mb);
					Console.WriteLnFmt("(GSRunner/{}) Current memory usage: {:.2} MB", runner_name, static_cast<double>(memory) / _1mb);
				});

			GSSignalRunnerHeartbeat_BatchRun();
		}
		else
		{
			if (!s_dump_file_loader.Started())
			{
				s_dump_file_loader.SetMaxFileSize(s_dump_buffer_size);
				s_dump_file_loader.Start(std::vector<std::string>());
			}

			GSDumpFileLoader::DumpInfo dump;

			const auto AcquireAndAddToLoader = [](GSDumpFileLoader& loader) {
				std::string file_str;
				if (GSBatchRunAcquireFile(file_str))
				{
					loader.AddFile(file_str);
					return true;
				}
				else
				{
					return false;
				}
			};

			// FIXME: Code duplication with other batch mode....
			// Get/read the next available ready dump, skipping any that errored.

			// Fill up the dump loader queue.
			while (s_dump_file_loader.DumpsRemaining() < s_dump_file_loader.num_dumps_buffered)
			{
				if (!AcquireAndAddToLoader(s_dump_file_loader))
					break;
			}

			while (true)
			{
				// This reads the dump file as well.
				GSDumpFileLoader::ReturnValue ret = s_dump_file_loader.Get(s_dump_file, &dump);

				AcquireAndAddToLoader(s_dump_file_loader); // Add one more after getting one.

				if (ret == GSDumpFileLoader::SUCCESS)
				{
					break;
				}
				else if (ret == GSDumpFileLoader::FINISHED)
				{
					Console.WriteLnFmt("(GSRunner/{}) Finished all dumps", GetRunnerName());
					GSSetChildState_BatchRun(GSBatchRunBuffer::DONE_RUNNING);
					return false;
				}
				else if (ret == GSDumpFileLoader::ERROR_)
				{
					MTGS::RunOnGSThread([error = dump.error]() {
						Console.ErrorFmt("(GSRunner/{}) Error loading/reading dump: {}.", GetRunnerName(),
							error.empty() ? std::string("Unspecified reason") : error);
					});
				}
				else
				{
					pxFail("Unknown return value."); // Impossible.
				}
			}

			s_dump_filename = dump.filename;
			s_dump_name = Path::GetFileName(dump.filename);
			GSGetBatchRunBuffer()->SetFileStatus(dump.filename, GSBatchRunBuffer::STARTED);

			MTGS::RunOnGSThread(
				[
					name = s_dump_name,
					block_time_write = dump.block_time_write,
					block_time_read = dump.block_time_read,
					load_time = dump.load_time,
					size = s_dump_file->GetFileSize()
				]() {
					Console.WriteLnFmt(
						"(GSRunner/{}) Loaded dump '{}' (size: {:.2} MB; block time write: {:.2} sec; block time read: {:.2} sec, load time: {:.2} sec)",
						GetRunnerName(), name, static_cast<double>(size) / _1mb, block_time_write, block_time_read, load_time);
				});
		}

		GSSignalRunnerHeartbeat_BatchRun();
	}
	else if (IsBatchMode())
	{
		// FIXME: Removee if not needed any more.
		// Batch mode but not regression testing; asynchronous dump loading.

		pxAssert(filename == nullptr);

		if (!s_dump_file_loader.Started())
		{
			s_dump_file_loader.Start(s_dump_file_list);
		}

		GSDumpFileLoader::DumpInfo dump;

		// Get/read the next available ready dump, skipping any that errored.
		while (true)
		{
			// This reads the dump file as well.
			GSDumpFileLoader::ReturnValue ret = s_dump_file_loader.Get(s_dump_file, &dump);

			if (ret == GSDumpFileLoader::SUCCESS)
			{
				break;
			}
			else if (ret == GSDumpFileLoader::FINISHED)
			{
				return false;
			}
			else if (ret == GSDumpFileLoader::ERROR_)
			{
				MTGS::RunOnGSThread([error = dump.error]() {
					Console.ErrorFmt("(GSRunner/{}) Error loading/reading dump: {}.", GetRunnerName(),
						error.empty() ? std::string("Unspecified reason") : error);
				});
			}
			else
			{
				pxFail("Unknown return value."); // Impossible.
			}
		}

		s_dump_name = Path::GetFileName(filename);

		MTGS::RunOnGSThread(
			[name = s_dump_name,
			block_time_write = dump.block_time_write,
			block_time_read = dump.block_time_read,
			load_time = dump.load_time,
			size = s_dump_file->GetFileSize()]() {

			Console.WriteLnFmt(
				"(GSRunner/{}) Loaded dump '{}' (size: {:.2} MB; block time write: {:.2} sec; block time read: {:.2} sec, load time: {:.2} sec)",
				GetRunnerName(), name, static_cast<double>(size) / _1mb, block_time_write, block_time_read, load_time);
		});
	}
	else
	{
		// Standard mode reading dumps synchronously.

		if (!VMManager::IsGSDumpFileName(filename))
		{
			MTGS::RunOnGSThread([filename, runner_name = GetRunnerName()]() {
				Console.ErrorFmt("(GSDumpReplayer/{}) {} is not a GS dump.", runner_name, filename);
			});
			return false;
		}

		Error error;
		std::unique_ptr<GSDumpFile> new_dump(GSDumpFile::OpenGSDump(filename));
		if (!new_dump)
		{
			MTGS::RunOnGSThread([fn = std::string(Path::GetFileName(filename)), runner_name = GetRunnerName(),
				err = error.GetDescription()]() {
				Console.ErrorFmt("(GSDumpReplayer/{}) Failed to open '{}' (error: {})", runner_name, fn, err);
			});
			return false;
		}

		s_dump_name = Path::GetFileName(filename);
		s_dump_file = std::move(new_dump);

		Common::Timer timer;

		if (!s_dump_file->ReadFile(&error))
		{
			MTGS::RunOnGSThread([runner_name = GetRunnerName(), dump_name = s_dump_name, err = error.GetDescription()]() {
				Console.ErrorFmt("(GSDumpReplayer/{}) Failed to read GS dump '{}' (error: {})", runner_name, dump_name, err);
			});
			return false;
		}

		double sec = timer.GetTimeSeconds();

		MTGS::RunOnGSThread([runner_name = GetRunnerName(), dump_name = s_dump_name, sec]() {
			Console.WriteLnFmt("(GSDumpReplayer/{}) Read GS dump in '{}' ({:.2} seconds)", runner_name, dump_name, sec);
		});
	}

	if (IsBatchMode())
	{
		Host::OnBatchDumpStart(s_dump_name);

		UpdateBatchGameSettings();
	}

	MTGS::RunOnGSThread([runner_name = GetRunnerName(), dump_name = s_dump_name]() {
		Console.WriteLnFmt("(GSDumpReplayer/{}) Switching to dump '{}'", runner_name, dump_name);
	});

	// Don't forget to reset the GS!
	GSDumpReplayerCpuReset();

	return true;
}

void GSDumpReplayer::Shutdown()
{
	Console.WriteLn("(GSDumpReplayer) Shutting down.");

	Cpu = nullptr;
	psxCpu = nullptr;
	CpuVU0 = nullptr;
	CpuVU1 = nullptr;
	s_dump_file.reset();
}

std::string GSDumpReplayer::GetDumpSerial()
{
	if (IsBatchMode())
		return ""; // We will update game settings later.

	std::string ret;

	if (!s_dump_file->GetSerial().empty())
	{
		ret = s_dump_file->GetSerial();
	}
	else if (s_dump_file->GetCRC() != 0)
	{
		// old dump files don't have serials, but we have the crc...
		// so, let's try searching the game list for a crc match.
		auto lock = GameList::GetLock();
		const GameList::Entry* entry = GameList::GetEntryByCRC(s_dump_file->GetCRC());
		if (entry)
			ret = entry->serial;
	}

	return ret;
}

u32 GSDumpReplayer::GetDumpCRC()
{
	return IsBatchMode() ? 0 : s_dump_file->GetCRC();
}

void GSDumpReplayer::SetFrameNumberMax(u32 frame_number_max)
{
	s_dump_frame_number_max = frame_number_max;
}

u32 GSDumpReplayer::GetFrameNumber()
{
	return s_dump_frame_number;
}

void GSDumpReplayerCpuReserve()
{
}

void GSDumpReplayerCpuShutdown()
{
}

void GSDumpReplayerCpuReset()
{
	s_needs_state_loaded = true;
	s_current_packet = 0;
	s_dump_frame_number = 0;
}

static void GSDumpReplayerLoadInitialState()
{
	// reset GS registers to initial dump values
	std::memcpy(PS2MEM_GS, s_dump_file->GetRegsData().data(),
		std::min(Ps2MemSize::GSregs, static_cast<u32>(s_dump_file->GetRegsData().size())));

	if (GSDumpReplayer::IsBatchMode())
	{
		MTGS::RunOnGSThread([]() {
			GSState::s_n = 0;
			GSState::s_last_transfer_draw_n = 0;
			GSState::s_transfer_n = 0;

			Common::Timer timer;

			// Order is important here:
			// 1. Reset vertex queue before recreating renderer to prevent unwanted vertex flush.
			// 2. Recreate renderer before resetting/recreating device to allow all texture cache textures to be properly purged.
			// 3. Reset/recreate device last to cleanup texture (done correctly in GSreopen if recreating device).
			g_gs_renderer->ResetVertexQueue();

			GSreopen(s_batch_recreate_device, true, EmuConfig.GS.Renderer, std::nullopt);

			if (!s_batch_recreate_device)
				g_gs_device->ResetRenderState(); // Fast reset

			g_perfmon.Reset(); // Resetting the render state can end a render pass.

			double sec = timer.GetTimeSeconds();

			Console.WriteLnFmt("(GSRunner/{}) GS reopened ({}) ({:.2} seconds).", GSDumpReplayer::GetRunnerName(),
				s_batch_recreate_device ? "renderer and device" : "renderer only", sec);

			if (GSIsRegressionTesting())
				GSSignalRunnerHeartbeat_RegressionTest();
		});
	}

	// load GS state
	freezeData fd = {static_cast<int>(s_dump_file->GetStateData().size()),
		const_cast<u8*>(s_dump_file->GetStateData().data())};
	MTGS::FreezeData mfd = {&fd, 0};
	MTGS::Freeze(FreezeAction::Load, mfd);
	if (mfd.retval != 0)
		Host::ReportFormattedErrorAsync("GSDumpReplayer", "Failed to load GS state.");

}

static void GSDumpReplayerSendPacketToMTGS(GIF_PATH path, const u8* data, u32 length)
{
	pxAssert((length % 16) == 0);

	Gif_Path& gifPath = gifUnit.gifPath[path];
	gifPath.CopyGSPacketData(const_cast<u8*>(data), length);

	GS_Packet gsPack;
	gsPack.offset = gifPath.curOffset;
	gsPack.size = length;
	gifPath.curOffset += length;
	Gif_AddCompletedGSPacket(gsPack, path);
}

static void GSDumpReplayerUpdateFrameLimit()
{
	constexpr u32 default_frame_limit = 60;
	const u32 frame_limit = static_cast<u32>(default_frame_limit * VMManager::GetTargetSpeed());

	if (frame_limit > 0)
		s_frame_ticks = (GetTickFrequency() + (frame_limit / 2)) / frame_limit;
	else
		s_frame_ticks = 0;
}

static void GSDumpReplayerFrameLimit()
{
	if (s_frame_ticks == 0)
		return;

	// Frame limiter
	u64 now = GetCPUTicks();
	const s64 ms = GetTickFrequency() / 1000;
	const s64 sleep = s_next_frame_time - now - ms;
	if (sleep > ms)
		Threading::Sleep(sleep / ms);
	while ((now = GetCPUTicks()) < s_next_frame_time)
		ShortSpin();
	s_next_frame_time = std::max(now, s_next_frame_time + s_frame_ticks);
}

void GSDumpReplayerCpuStep()
{
	if ((s_current_packet & 0xFF) == 0)
	{
		// Let parent process know we are not deadlocked.
		if (GSIsRegressionTesting())
			GSSignalRunnerHeartbeat_RegressionTest();
		if (GSIsBatchRunning())
			GSSignalRunnerHeartbeat_BatchRun();
	}

	if (s_needs_state_loaded)
	{
		GSDumpReplayerLoadInitialState();
		s_needs_state_loaded = false;
	}

	bool done_all_dumps = false;
	bool done_dump = false;

	GSDumpFile::GSData packet;
	Error error;
	s64 ret = s_dump_file->GetPacket(s_current_packet, packet, &error);
	if (ret > 0)
	{
		s_current_packet++;
		if (s_dump_file->DonePackets(s_current_packet))
		{
			s_current_packet = 0;
			s_dump_frame_number = 0;
			if (s_dump_loop_count > 0)
				s_dump_loop_count--;
			else if (s_dump_loop_count == 0)
				done_dump = true;
		}

		constexpr bool DRY_RUN = true;

		switch (packet.id)
		{
			case GSDumpTypes::GSType::Transfer:
			{
				switch (packet.path)
				{
					case GSDumpTypes::GSTransferPath::Path1Old:
					if (!DRY_RUN)
					{
						std::unique_ptr<u8[]> data(new u8[16384]);
						const s32 addr = 16384 - packet.length;
						std::memcpy(data.get(), packet.data + addr, packet.length);
						GSDumpReplayerSendPacketToMTGS(GIF_PATH_1, data.get(), packet.length);
					}
					break;

					case GSDumpTypes::GSTransferPath::Path1New:
					case GSDumpTypes::GSTransferPath::Path2:
					case GSDumpTypes::GSTransferPath::Path3:
					if (!DRY_RUN)
					{
						GSDumpReplayerSendPacketToMTGS(static_cast<GIF_PATH>(static_cast<u8>(packet.path) - 1),
							packet.data, packet.length);
					}
					break;

					default:
						break;
				}
				break;
			}

			case GSDumpTypes::GSType::VSync:
			{
				s_dump_frame_number++;
				if (!DRY_RUN)
				{
					GSDumpReplayerUpdateFrameLimit();
					GSDumpReplayerFrameLimit();
					MTGS::PostVsyncStart(false);
					VMManager::Internal::VSyncOnCPUThread();
					if (VMManager::Internal::IsExecutionInterrupted())
						GSDumpReplayerExitExecution();
				}
				Host::PumpMessagesOnCPUThread();
			}
			break;

			case GSDumpTypes::GSType::ReadFIFO2:
			if (!DRY_RUN)
			{
				u32 size;
				std::memcpy(&size, packet.data, sizeof(size));

				// Allocate an extra quadword, some transfers write too much (e.g. Lego Racers 2 with Z24 downloads).
				std::unique_ptr<u8[]> arr(new u8[(size + 1) * 16]);
				MTGS::InitAndReadFIFO(arr.get(), size);
			}
			break;

			case GSDumpTypes::GSType::Registers:
			if (!DRY_RUN)
			{
				std::memcpy(PS2MEM_GS, packet.data, std::min<s32>(packet.length, Ps2MemSize::GSregs));
			}
			break;
		}
	}
	else
	{
		MTGS::RunOnGSThread([runner_name = GSDumpReplayer::GetRunnerName(), err = error.GetDescription()]() {
			Console.ErrorFmt("(GSDumpReplayer/{}) Error getting packet: '{}'", runner_name, err);
		});
		done_dump = true;
	}

	done_dump = done_dump || (s_dump_frame_number_max > 0 && s_dump_frame_number >= s_dump_frame_number_max);

	if (GSIsRegressionTesting() && GSCheckTesterStatus_RegressionTest(true, false))
	{
		MTGS::RunOnGSThread([runner_name = GSDumpReplayer::GetRunnerName()]() {
			Console.WarningFmt("(GSDumpReplayer/{}) Got exit status from tester.", runner_name);
		});
		done_all_dumps = true;
	}
	else if (GSIsBatchRunning() && GSCheckParentStatus_BatchRun())
	{
		MTGS::RunOnGSThread([runner_name = GSDumpReplayer::GetRunnerName()]() {
			Console.WarningFmt("(GSDumpReplayer/{}) Got exit status from tester.", runner_name);
		});
		done_all_dumps = true;
	}
	else if (done_dump)
	{
		// Check if we need to change dumps for batch mode; or done with all dumps.

		if (GSDumpReplayer::IsBatchMode())
		{
			if (GSIsBatchRunning())
			{
				GSGetBatchRunBuffer()->SetFileStatus(s_dump_filename, GSBatchRunBuffer::COMPLETED);
			}

			Host::OnBatchDumpEnd(s_dump_name); // Dump stats

			// Send HW stats and done packet if needed.
			if (GSIsRegressionTesting())
			{
				GSDumpReplayer::EndDumpRegressionTest();
			}

			if (GSDumpReplayer::ChangeDump())
			{
				GSDumpReplayer::SetLoopCount(s_dump_loop_count_start);
				GSDumpReplayerCpuReset();
			}
			else
			{
				MTGS::RunOnGSThread([runner_name = GSDumpReplayer::GetRunnerName()]() {
					Console.WriteLnFmt("(GSDumpReplayer/{}) Batch mode has no more dumps.", runner_name);
				});
				done_all_dumps = true;
			}
		}
		else // Normal (non-batch) mode
		{
			done_all_dumps = true;
		}
	}

	if (done_all_dumps)
	{
		Host::RequestVMShutdown(false, false, false);
		GSDumpReplayerExitExecution();
	}
}

void GSDumpReplayerCpuExecute()
{
	s_dump_running = true;
	s_next_frame_time = GetCPUTicks();

	while (s_dump_running)
	{
		GSDumpReplayerCpuStep();
	}
}

void GSDumpReplayerExitExecution()
{
	s_dump_running = false;
}

void GSDumpReplayerCancelInstruction()
{
}

void GSDumpReplayerCpuClear(u32 addr, u32 size)
{
}

void GSDumpReplayer::RenderUI()
{
	const float scale = ImGuiManager::GetGlobalScale();
	const float shadow_offset = std::ceil(1.0f * scale);
	const float margin = std::ceil(10.0f * scale);
	const float spacing = std::ceil(5.0f * scale);
	float position_y = margin;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	ImFont* const font = ImGuiManager::GetFixedFont();
	const float font_size = ImGuiManager::GetFontSizeStandard();

	std::string text;
	ImVec2 text_size;
	text.reserve(128);

#define DRAW_LINE(font, size, text, color) \
	do \
	{ \
		text_size = font->CalcTextSizeA(size, std::numeric_limits<float>::max(), -1.0f, (text), nullptr, nullptr); \
		const ImVec2 text_pos = CalculatePerformanceOverlayTextPosition(GSConfig.OsdMessagesPos, margin, text_size, ImGuiManager::GetWindowWidth(), position_y); \
		dl->AddText(font, size, ImVec2(text_pos.x + shadow_offset, text_pos.y + shadow_offset), IM_COL32(0, 0, 0, 100), (text)); \
		dl->AddText(font, size, text_pos, color, (text)); \
		position_y += text_size.y + spacing; \
	} while (0)

	fmt::format_to(std::back_inserter(text), "Dump Frame: {}", s_dump_frame_number);
	DRAW_LINE(font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

	text.clear();
	fmt::format_to(std::back_inserter(text), "Packet Number: {}/{}", s_current_packet, static_cast<u32>(s_dump_file->GetPackets().size()));
	DRAW_LINE(font, font_size, text.c_str(), IM_COL32(255, 255, 255, 255));

#undef DRAW_LINE
}
