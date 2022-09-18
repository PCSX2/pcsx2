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

#pragma once
#include "common/Pcsx2Defs.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Common
{
	class HTTPDownloader
	{
	public:
		enum : s32
		{
			HTTP_OK = 200
		};

		struct Request
		{
			using Data = std::vector<u8>;
			using Callback = std::function<void(s32 status_code, const std::string& content_type, Data data)>;

			enum class Type
			{
				Get,
				Post,
			};

			enum class State
			{
				Pending,
				Cancelled,
				Started,
				Receiving,
				Complete,
			};

			HTTPDownloader* parent;
			Callback callback;
			std::string url;
			std::string post_data;
			std::string content_type;
			Data data;
			u64 start_time;
			s32 status_code = 0;
			u32 content_length = 0;
			Type type = Type::Get;
			std::atomic<State> state{State::Pending};
		};

		HTTPDownloader();
		virtual ~HTTPDownloader();

		static std::unique_ptr<HTTPDownloader> Create(const char* user_agent = DEFAULT_USER_AGENT);
		static std::string URLEncode(const std::string_view& str);
		static std::string URLDecode(const std::string_view& str);
		static std::string GetExtensionForContentType(const std::string& content_type);

		void SetTimeout(float timeout);
		void SetMaxActiveRequests(u32 max_active_requests);

		void CreateRequest(std::string url, Request::Callback callback);
		void CreatePostRequest(std::string url, std::string post_data, Request::Callback callback);
		void PollRequests();
		void WaitForAllRequests();
		bool HasAnyRequests();

		static const char DEFAULT_USER_AGENT[];

	protected:
		virtual Request* InternalCreateRequest() = 0;
		virtual void InternalPollRequests() = 0;

		virtual bool StartRequest(Request* request) = 0;
		virtual void CloseRequest(Request* request) = 0;

		void LockedAddRequest(Request* request);
		u32 LockedGetActiveRequestCount();
		void LockedPollRequests(std::unique_lock<std::mutex>& lock);

		float m_timeout;
		u32 m_max_active_requests;

		std::mutex m_pending_http_request_lock;
		std::vector<Request*> m_pending_http_requests;
	};

} // namespace Common