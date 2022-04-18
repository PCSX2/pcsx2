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

#include "common/HTTPDownloader.h"
#include "common/RedtapeWindows.h"

#include <winhttp.h>

namespace Common
{
	class HTTPDownloaderWinHttp final : public HTTPDownloader
	{
	public:
		HTTPDownloaderWinHttp();
		~HTTPDownloaderWinHttp() override;

		bool Initialize(const char* user_agent);

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

		HINTERNET m_hSession = NULL;
	};
} // namespace Common