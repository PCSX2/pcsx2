// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/HTTPDownloader.h"
#include "common/RedtapeWindows.h"

#include <winhttp.h>

class HTTPDownloaderWinHttp final : public HTTPDownloader
{
public:
	HTTPDownloaderWinHttp();
	~HTTPDownloaderWinHttp() override;

	bool Initialize(std::string user_agent);

protected:
	Request* InternalCreateRequest() override;
	void InternalPollRequests() override;
	bool StartRequest(HTTPDownloader::Request* request) override;
	void CloseRequest(HTTPDownloader::Request* request) override;

private:
	struct Request : HTTPDownloader::Request
	{
		std::wstring object_name;
		HINTERNET hConnection = NULL;
		HINTERNET hRequest = NULL;
		u32 io_position = 0;
	};

	static void CALLBACK HTTPStatusCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
		LPVOID lpvStatusInformation, DWORD dwStatusInformationLength);

	static bool CheckCancelled(Request* request);

	HINTERNET m_hSession = NULL;
};
