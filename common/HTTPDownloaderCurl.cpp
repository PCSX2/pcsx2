/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "common/PrecompiledHeader.h"

#include "common/HTTPDownloaderCurl.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include <algorithm>
#include <functional>
#include <pthread.h>
#include <signal.h>

using namespace Common;

HTTPDownloaderCurl::HTTPDownloaderCurl()
	: HTTPDownloader()
{
}

HTTPDownloaderCurl::~HTTPDownloaderCurl() = default;

std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(const char* user_agent)
{
	std::unique_ptr<HTTPDownloaderCurl> instance(std::make_unique<HTTPDownloaderCurl>());
	if (!instance->Initialize(user_agent))
		return {};

	return instance;
}

static bool s_curl_initialized = false;
static std::once_flag s_curl_initialized_once_flag;

bool HTTPDownloaderCurl::Initialize(const char* user_agent)
{
	if (!s_curl_initialized)
	{
		std::call_once(s_curl_initialized_once_flag, []() {
			s_curl_initialized = curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK;
			if (s_curl_initialized)
			{
				std::atexit([]() {
					curl_global_cleanup();
					s_curl_initialized = false;
				});
			}
		});
		if (!s_curl_initialized)
		{
			Console.Error("curl_global_init() failed");
			return false;
		}
	}

	m_user_agent = user_agent;
	m_thread_pool = std::make_unique<cb::ThreadPool>(m_max_active_requests);
	return true;
}

size_t HTTPDownloaderCurl::WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	Request* req = static_cast<Request*>(userdata);
	const size_t current_size = req->data.size();
	const size_t transfer_size = size * nmemb;
	const size_t new_size = current_size + transfer_size;
	req->data.resize(new_size);
	std::memcpy(&req->data[current_size], ptr, transfer_size);
	return nmemb;
}

void HTTPDownloaderCurl::ProcessRequest(Request* req)
{
	std::unique_lock<std::mutex> cancel_lock(m_cancel_mutex);
	if (req->closed.load())
		return;

	cancel_lock.unlock();

	// Apparently OpenSSL can fire SIGPIPE...
	sigset_t old_block_mask = {};
	sigset_t new_block_mask = {};
	sigemptyset(&old_block_mask);
	sigemptyset(&new_block_mask);
	sigaddset(&new_block_mask, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &new_block_mask, &old_block_mask) != 0)
		Console.Warning("Failed to block SIGPIPE");

	req->start_time = Common::Timer::GetCurrentValue();
	int ret = curl_easy_perform(req->handle);
	if (ret == CURLE_OK)
	{
		long response_code = 0;
		curl_easy_getinfo(req->handle, CURLINFO_RESPONSE_CODE, &response_code);
		req->status_code = static_cast<s32>(response_code);

		char* content_type = nullptr;
		if (!curl_easy_getinfo(req->handle, CURLINFO_CONTENT_TYPE, &content_type) && content_type)
			req->content_type = content_type;

		DevCon.WriteLn("Request for '%s' returned status code %d and %zu bytes", req->url.c_str(), req->status_code,
			req->data.size());
	}
	else
	{
		Console.Error("Request for '%s' returned %d", req->url.c_str(), ret);
	}

	curl_easy_cleanup(req->handle);

	if (pthread_sigmask(SIG_UNBLOCK, &new_block_mask, &old_block_mask) != 0)
		Console.Warning("Failed to unblock SIGPIPE");

	cancel_lock.lock();
	req->state = Request::State::Complete;
	if (req->closed.load())
		delete req;
	else
		req->closed.store(true);
}

HTTPDownloader::Request* HTTPDownloaderCurl::InternalCreateRequest()
{
	Request* req = new Request();
	req->handle = curl_easy_init();
	if (!req->handle)
	{
		delete req;
		return nullptr;
	}

	return req;
}

void HTTPDownloaderCurl::InternalPollRequests()
{
	// noop - uses thread pool
}

bool HTTPDownloaderCurl::StartRequest(HTTPDownloader::Request* request)
{
	Request* req = static_cast<Request*>(request);
	curl_easy_setopt(req->handle, CURLOPT_URL, request->url.c_str());
	curl_easy_setopt(req->handle, CURLOPT_USERAGENT, m_user_agent.c_str());
	curl_easy_setopt(req->handle, CURLOPT_WRITEFUNCTION, &HTTPDownloaderCurl::WriteCallback);
	curl_easy_setopt(req->handle, CURLOPT_WRITEDATA, req);
	curl_easy_setopt(req->handle, CURLOPT_NOSIGNAL, 1);

	if (request->type == Request::Type::Post)
	{
		curl_easy_setopt(req->handle, CURLOPT_POST, 1L);
		curl_easy_setopt(req->handle, CURLOPT_POSTFIELDS, request->post_data.c_str());
	}

	DbgCon.WriteLn("Started HTTP request for '%s'", req->url.c_str());
	req->state = Request::State::Started;
	req->start_time = Common::Timer::GetCurrentValue();
	m_thread_pool->Schedule(std::bind(&HTTPDownloaderCurl::ProcessRequest, this, req));
	return true;
}

void HTTPDownloaderCurl::CloseRequest(HTTPDownloader::Request* request)
{
	std::unique_lock<std::mutex> cancel_lock(m_cancel_mutex);
	Request* req = static_cast<Request*>(request);
	if (req->closed.load())
		delete req;
	else
		req->closed.store(true);
}
