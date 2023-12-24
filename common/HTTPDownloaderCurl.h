// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
