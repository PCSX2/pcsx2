// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/HTTPDownloaderCurl.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/format.h"

#include <algorithm>
#include <functional>
#include <pthread.h>
#include <signal.h>

HTTPDownloaderCurl::HTTPDownloaderCurl()
	: HTTPDownloader()
{
}

HTTPDownloaderCurl::~HTTPDownloaderCurl()
{
	if (m_multi_handle)
		curl_multi_cleanup(m_multi_handle);
}

std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string user_agent)
{
	std::unique_ptr<HTTPDownloaderCurl> instance(std::make_unique<HTTPDownloaderCurl>());
	if (!instance->Initialize(std::move(user_agent)))
		return {};

	return instance;
}

static bool s_curl_initialized = false;
static std::once_flag s_curl_initialized_once_flag;

bool HTTPDownloaderCurl::Initialize(std::string user_agent)
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

	m_multi_handle = curl_multi_init();
	if (!m_multi_handle)
	{
		Console.Error("curl_multi_init() failed");
		return false;
	}

	m_user_agent = std::move(user_agent);
	return true;
}

size_t HTTPDownloaderCurl::WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	Request* req = static_cast<Request*>(userdata);
	const size_t current_size = req->data.size();
	const size_t transfer_size = size * nmemb;
	const size_t new_size = current_size + transfer_size;
	req->data.resize(new_size);
	req->start_time = Common::Timer::GetCurrentValue();
	std::memcpy(&req->data[current_size], ptr, transfer_size);

	if (req->content_length == 0)
	{
		curl_off_t length;
		if (curl_easy_getinfo(req->handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length) == CURLE_OK)
			req->content_length = static_cast<u32>(length);
	}

	return nmemb;
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
	// Apparently OpenSSL can fire SIGPIPE...
	sigset_t old_block_mask = {};
	sigset_t new_block_mask = {};
	sigemptyset(&old_block_mask);
	sigemptyset(&new_block_mask);
	sigaddset(&new_block_mask, SIGPIPE);
	if (pthread_sigmask(SIG_BLOCK, &new_block_mask, &old_block_mask) != 0)
		Console.Warning("Failed to block SIGPIPE");

	int running_handles;
	const CURLMcode err = curl_multi_perform(m_multi_handle, &running_handles);
	if (err != CURLM_OK)
		Console.Error(fmt::format("curl_multi_perform() returned {}", static_cast<int>(err)));

	for (;;)
	{
		int msgq;
		struct CURLMsg* msg = curl_multi_info_read(m_multi_handle, &msgq);
		if (!msg)
			break;

		if (msg->msg != CURLMSG_DONE)
		{
			Console.Warning(fmt::format("Unexpected multi message {}", static_cast<int>(msg->msg)));
			continue;
		}

		Request* req;
		if (curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &req) != CURLE_OK)
		{
			Console.Error("curl_easy_getinfo() failed");
			continue;
		}

		if (msg->data.result == CURLE_OK)
		{
			long response_code = 0;
			curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &response_code);
			req->status_code = static_cast<s32>(response_code);

			char* content_type = nullptr;
			if (curl_easy_getinfo(req->handle, CURLINFO_CONTENT_TYPE, &content_type) == CURLE_OK && content_type)
				req->content_type = content_type;

			DevCon.WriteLn(fmt::format("Request for '{}' returned status code {} and {} bytes", req->url, req->status_code, req->data.size()));
		}
		else
		{
			Console.Error(fmt::format("Request for '{}' returned error {}", req->url, static_cast<int>(msg->data.result)));
		}

		req->state.store(Request::State::Complete, std::memory_order_release);
	}

	if (pthread_sigmask(SIG_UNBLOCK, &new_block_mask, &old_block_mask) != 0)
		Console.Warning("Failed to unblock SIGPIPE");
}

bool HTTPDownloaderCurl::StartRequest(HTTPDownloader::Request* request)
{
	Request* req = static_cast<Request*>(request);
	curl_easy_setopt(req->handle, CURLOPT_URL, request->url.c_str());
	curl_easy_setopt(req->handle, CURLOPT_USERAGENT, m_user_agent.c_str());
	curl_easy_setopt(req->handle, CURLOPT_WRITEFUNCTION, &HTTPDownloaderCurl::WriteCallback);
	curl_easy_setopt(req->handle, CURLOPT_WRITEDATA, req);
	curl_easy_setopt(req->handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(req->handle, CURLOPT_PRIVATE, req);
	curl_easy_setopt(req->handle, CURLOPT_FOLLOWLOCATION, 1L);

	if (request->type == Request::Type::Post)
	{
		curl_easy_setopt(req->handle, CURLOPT_POST, 1L);
		curl_easy_setopt(req->handle, CURLOPT_POSTFIELDS, request->post_data.c_str());
	}

	DevCon.WriteLn(fmt::format("Started HTTP request for '{}'", req->url));
	req->state.store(Request::State::Started, std::memory_order_release);
	req->start_time = Common::Timer::GetCurrentValue();

	const CURLMcode err = curl_multi_add_handle(m_multi_handle, req->handle);
	if (err != CURLM_OK)
	{
		Console.Error(fmt::format("curl_multi_add_handle() returned {}", static_cast<int>(err)));
		req->callback(HTTP_STATUS_ERROR, std::string(), req->data);
		curl_easy_cleanup(req->handle);
		delete req;
		return false;
	}

	return true;
}

void HTTPDownloaderCurl::CloseRequest(HTTPDownloader::Request* request)
{
	Request* req = static_cast<Request*>(request);
	pxAssert(req->handle);
	curl_multi_remove_handle(m_multi_handle, req->handle);
	curl_easy_cleanup(req->handle);
	delete req;
}
