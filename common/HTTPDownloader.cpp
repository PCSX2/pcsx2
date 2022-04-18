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

#include "common/HTTPDownloader.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Timer.h"

using namespace Common;

static constexpr float DEFAULT_TIMEOUT_IN_SECONDS = 30;
static constexpr u32 DEFAULT_MAX_ACTIVE_REQUESTS = 4;

const char HTTPDownloader::DEFAULT_USER_AGENT[] =
	"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:85.0) Gecko/20100101 Firefox/85.0";

HTTPDownloader::HTTPDownloader()
	: m_timeout(DEFAULT_TIMEOUT_IN_SECONDS)
	, m_max_active_requests(DEFAULT_MAX_ACTIVE_REQUESTS)
{
}

HTTPDownloader::~HTTPDownloader() = default;

void HTTPDownloader::SetTimeout(float timeout)
{
	m_timeout = timeout;
}

void HTTPDownloader::SetMaxActiveRequests(u32 max_active_requests)
{
	pxAssert(max_active_requests > 0);
	m_max_active_requests = max_active_requests;
}

void HTTPDownloader::CreateRequest(std::string url, Request::Callback callback)
{
	Request* req = InternalCreateRequest();
	req->parent = this;
	req->type = Request::Type::Get;
	req->url = std::move(url);
	req->callback = std::move(callback);
	req->start_time = Timer::GetCurrentValue();

	std::unique_lock<std::mutex> lock(m_pending_http_request_lock);
	if (LockedGetActiveRequestCount() < m_max_active_requests)
	{
		if (!StartRequest(req))
			return;
	}

	LockedAddRequest(req);
}

void HTTPDownloader::CreatePostRequest(std::string url, std::string post_data, Request::Callback callback)
{
	Request* req = InternalCreateRequest();
	req->parent = this;
	req->type = Request::Type::Post;
	req->url = std::move(url);
	req->post_data = std::move(post_data);
	req->callback = std::move(callback);
	req->start_time = Timer::GetCurrentValue();

	std::unique_lock<std::mutex> lock(m_pending_http_request_lock);
	if (LockedGetActiveRequestCount() < m_max_active_requests)
	{
		if (!StartRequest(req))
			return;
	}

	LockedAddRequest(req);
}

void HTTPDownloader::LockedPollRequests(std::unique_lock<std::mutex>& lock)
{
	if (m_pending_http_requests.empty())
		return;

	InternalPollRequests();

	const Common::Timer::Value current_time = Timer::GetCurrentValue();
	u32 active_requests = 0;
	u32 unstarted_requests = 0;

	for (size_t index = 0; index < m_pending_http_requests.size();)
	{
		Request* req = m_pending_http_requests[index];
		if (req->state == Request::State::Pending)
		{
			unstarted_requests++;
			index++;
			continue;
		}

		if (req->state == Request::State::Started && current_time >= req->start_time &&
			Common::Timer::ConvertValueToSeconds(current_time - req->start_time) >= m_timeout)
		{
			// request timed out
			Console.Error("Request for '%s' timed out", req->url.c_str());

			req->state.store(Request::State::Cancelled);
			m_pending_http_requests.erase(m_pending_http_requests.begin() + index);
			lock.unlock();

			req->callback(-1, Request::Data());

			CloseRequest(req);

			lock.lock();
			continue;
		}

		if (req->state != Request::State::Complete)
		{
			active_requests++;
			index++;
			continue;
		}

		// request complete
		DevCon.WriteLn("Request for '%s' complete, returned status code %u and %zu bytes", req->url.c_str(),
			req->status_code, req->data.size());
		m_pending_http_requests.erase(m_pending_http_requests.begin() + index);

		// run callback with lock unheld
		lock.unlock();
		req->callback(req->status_code, std::move(req->data));
		CloseRequest(req);
		lock.lock();
	}

	// start new requests when we finished some
	if (unstarted_requests > 0 && active_requests < m_max_active_requests)
	{
		for (size_t index = 0; index < m_pending_http_requests.size();)
		{
			Request* req = m_pending_http_requests[index];
			if (req->state != Request::State::Pending)
			{
				index++;
				continue;
			}

			if (!StartRequest(req))
			{
				m_pending_http_requests.erase(m_pending_http_requests.begin() + index);
				continue;
			}

			active_requests++;
			index++;

			if (active_requests >= m_max_active_requests)
				break;
		}
	}
}

void HTTPDownloader::PollRequests()
{
	std::unique_lock<std::mutex> lock(m_pending_http_request_lock);
	LockedPollRequests(lock);
}

void HTTPDownloader::WaitForAllRequests()
{
	std::unique_lock<std::mutex> lock(m_pending_http_request_lock);
	while (!m_pending_http_requests.empty())
		LockedPollRequests(lock);
}

void HTTPDownloader::LockedAddRequest(Request* request)
{
	m_pending_http_requests.push_back(request);
}

u32 HTTPDownloader::LockedGetActiveRequestCount()
{
	u32 count = 0;
	for (Request* req : m_pending_http_requests)
	{
		if (req->state == Request::State::Started || req->state == Request::State::Receiving)
			count++;
	}
	return count;
}

bool HTTPDownloader::HasAnyRequests()
{
	std::unique_lock<std::mutex> lock(m_pending_http_request_lock);
	return !m_pending_http_requests.empty();
}
