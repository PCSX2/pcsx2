/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "common/HTTPDownloader.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <curl/curl.h>

class HTTPDownloaderCurl final : public HTTPDownloader
{
public:
	HTTPDownloaderCurl();
	~HTTPDownloaderCurl() override;

	bool Initialize(std::string user_agent);

protected:
	Request* InternalCreateRequest() override;
	void InternalPollRequests() override;
	bool StartRequest(HTTPDownloader::Request* request) override;
	void CloseRequest(HTTPDownloader::Request* request) override;

private:
	struct Request : HTTPDownloader::Request
	{
		CURL* handle = nullptr;
	};

	static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata);

	CURLM* m_multi_handle = nullptr;
	std::string m_user_agent;
};
