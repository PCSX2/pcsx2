// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/HTTPDownloaderWinHTTP.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include <VersionHelpers.h>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

HTTPDownloaderWinHttp::HTTPDownloaderWinHttp()
	: HTTPDownloader()
{
}

HTTPDownloaderWinHttp::~HTTPDownloaderWinHttp()
{
	if (m_hSession)
	{
		WinHttpSetStatusCallback(m_hSession, nullptr, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, NULL);
		WinHttpCloseHandle(m_hSession);
	}
}

std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string user_agent)
{
	std::unique_ptr<HTTPDownloaderWinHttp> instance(std::make_unique<HTTPDownloaderWinHttp>());
	if (!instance->Initialize(std::move(user_agent)))
		return {};

	return instance;
}

bool HTTPDownloaderWinHttp::Initialize(std::string user_agent)
{
	const DWORD dwAccessType = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;

	m_hSession = WinHttpOpen(StringUtil::UTF8StringToWideString(user_agent).c_str(), dwAccessType, nullptr, nullptr,
		WINHTTP_FLAG_ASYNC);
	if (m_hSession == NULL)
	{
		Console.Error("WinHttpOpen() failed: %u", GetLastError());
		return false;
	}

	const DWORD notification_flags = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_REQUEST_ERROR |
									 WINHTTP_CALLBACK_FLAG_HANDLES | WINHTTP_CALLBACK_FLAG_SECURE_FAILURE;
	if (WinHttpSetStatusCallback(m_hSession, HTTPStatusCallback, notification_flags, NULL) ==
		WINHTTP_INVALID_STATUS_CALLBACK)
	{
		Console.Error("WinHttpSetStatusCallback() failed: %u", GetLastError());
		return false;
	}

	return true;
}

void CALLBACK HTTPDownloaderWinHttp::HTTPStatusCallback(HINTERNET hRequest, DWORD_PTR dwContext, DWORD dwInternetStatus,
	LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	Request* req = reinterpret_cast<Request*>(dwContext);
	switch (dwInternetStatus)
	{
		case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
			return;

		case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		{
			if (!req)
				return;

			pxAssert(hRequest == req->hRequest);

			HTTPDownloaderWinHttp* parent = static_cast<HTTPDownloaderWinHttp*>(req->parent);
			std::unique_lock<std::mutex> lock(parent->m_pending_http_request_lock);
			pxAssertRel(std::none_of(parent->m_pending_http_requests.begin(), parent->m_pending_http_requests.end(),
							[req](HTTPDownloader::Request* it) { return it == req; }),
				"Request is not pending at close time");

			// we can clean up the connection as well
			pxAssert(req->hConnection != NULL);
			WinHttpCloseHandle(req->hConnection);
			delete req;
			return;
		}

		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
		{
			const WINHTTP_ASYNC_RESULT* res = reinterpret_cast<const WINHTTP_ASYNC_RESULT*>(lpvStatusInformation);
			Console.Error("WinHttp async function %p returned error %u", res->dwResult, res->dwError);
			req->status_code = HTTP_STATUS_ERROR;
			req->state.store(Request::State::Complete);
			return;
		}
		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
		{
			DbgCon.WriteLn("SendRequest complete");
			if (!WinHttpReceiveResponse(hRequest, nullptr))
			{
				Console.Error("WinHttpReceiveResponse() failed: %u", GetLastError());
				req->status_code = HTTP_STATUS_ERROR;
				req->state.store(Request::State::Complete);
			}

			return;
		}
		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
		{
			DbgCon.WriteLn("Headers available");

			DWORD buffer_size = sizeof(req->status_code);
			if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &req->status_code, &buffer_size, WINHTTP_NO_HEADER_INDEX))
			{
				Console.Error("WinHttpQueryHeaders() for status code failed: %u", GetLastError());
				req->status_code = HTTP_STATUS_ERROR;
				req->state.store(Request::State::Complete);
				return;
			}

			buffer_size = sizeof(req->content_length);
			if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &req->content_length, &buffer_size,
					WINHTTP_NO_HEADER_INDEX))
			{
				if (GetLastError() != ERROR_WINHTTP_HEADER_NOT_FOUND)
					Console.Warning("WinHttpQueryHeaders() for content length failed: %u", GetLastError());

				req->content_length = 0;
			}

			DWORD content_type_length = 0;
			if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
					WINHTTP_NO_OUTPUT_BUFFER, &content_type_length, WINHTTP_NO_HEADER_INDEX) &&
				GetLastError() == ERROR_INSUFFICIENT_BUFFER && content_type_length >= sizeof(content_type_length))
			{
				std::wstring content_type_wstring;
				content_type_wstring.resize((content_type_length / sizeof(wchar_t)) - 1);
				if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
						content_type_wstring.data(), &content_type_length, WINHTTP_NO_HEADER_INDEX))
				{
					req->content_type = StringUtil::WideStringToUTF8String(content_type_wstring);
				}
			}

			DbgCon.WriteLn("Status code %d, content-length is %u, content-type is %s", req->status_code, req->content_length,
				req->content_type.c_str());
			req->data.reserve(req->content_length);
			req->state = Request::State::Receiving;

			// start reading
			if (!WinHttpQueryDataAvailable(hRequest, nullptr) && GetLastError() != ERROR_IO_PENDING)
			{
				Console.Error("WinHttpQueryDataAvailable() failed: %u", GetLastError());
				req->status_code = HTTP_STATUS_ERROR;
				req->state.store(Request::State::Complete);
			}

			return;
		}
		case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
		{
			DWORD bytes_available;
			std::memcpy(&bytes_available, lpvStatusInformation, sizeof(bytes_available));
			if (bytes_available == 0)
			{
				// end of request
				DbgCon.WriteLn("End of request '%s', %zu bytes received", req->url.c_str(), req->data.size());
				req->state.store(Request::State::Complete);
				return;
			}

			// start the transfer
			DbgCon.WriteLn("%u bytes available", bytes_available);
			req->io_position = static_cast<u32>(req->data.size());
			req->data.resize(req->io_position + bytes_available);
			if (!WinHttpReadData(hRequest, req->data.data() + req->io_position, bytes_available, nullptr) &&
				GetLastError() != ERROR_IO_PENDING)
			{
				Console.Error("WinHttpReadData() failed: %u", GetLastError());
				req->status_code = HTTP_STATUS_ERROR;
				req->state.store(Request::State::Complete);
			}

			return;
		}
		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
		{
			DbgCon.WriteLn("Read of %u complete", dwStatusInformationLength);

			const u32 new_size = req->io_position + dwStatusInformationLength;
			pxAssertRel(new_size <= req->data.size(), "HTTP overread occurred");
			req->data.resize(new_size);
			req->start_time = Common::Timer::GetCurrentValue();

			if (!WinHttpQueryDataAvailable(hRequest, nullptr) && GetLastError() != ERROR_IO_PENDING)
			{
				Console.Error("WinHttpQueryDataAvailable() failed: %u", GetLastError());
				req->status_code = HTTP_STATUS_ERROR;
				req->state.store(Request::State::Complete);
			}

			return;
		}
		default:
			// unhandled, ignore
			return;
	}
}

HTTPDownloader::Request* HTTPDownloaderWinHttp::InternalCreateRequest()
{
	Request* req = new Request();
	return req;
}

void HTTPDownloaderWinHttp::InternalPollRequests()
{
	// noop - it uses windows's worker threads
}

bool HTTPDownloaderWinHttp::StartRequest(HTTPDownloader::Request* request)
{
	Request* req = static_cast<Request*>(request);

	std::wstring host_name;
	host_name.resize(req->url.size());
	req->object_name.resize(req->url.size());

	URL_COMPONENTSW uc = {};
	uc.dwStructSize = sizeof(uc);
	uc.lpszHostName = host_name.data();
	uc.dwHostNameLength = static_cast<DWORD>(host_name.size());
	uc.lpszUrlPath = req->object_name.data();
	uc.dwUrlPathLength = static_cast<DWORD>(req->object_name.size());

	const std::wstring url_wide(StringUtil::UTF8StringToWideString(req->url));
	if (!WinHttpCrackUrl(url_wide.c_str(), static_cast<DWORD>(url_wide.size()), 0, &uc))
	{
		Console.Error("WinHttpCrackUrl() failed: %u", GetLastError());
		req->callback(HTTP_STATUS_ERROR, req->content_type, Request::Data());
		delete req;
		return false;
	}

	host_name.resize(uc.dwHostNameLength);
	req->object_name.resize(uc.dwUrlPathLength);

	req->hConnection = WinHttpConnect(m_hSession, host_name.c_str(), uc.nPort, 0);
	if (!req->hConnection)
	{
		Console.Error("Failed to start HTTP request for '%s': %u", req->url.c_str(), GetLastError());
		req->callback(HTTP_STATUS_ERROR, req->content_type, Request::Data());
		delete req;
		return false;
	}

	const DWORD request_flags = uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
	req->hRequest =
		WinHttpOpenRequest(req->hConnection, (req->type == HTTPDownloader::Request::Type::Post) ? L"POST" : L"GET",
			req->object_name.c_str(), NULL, NULL, NULL, request_flags);
	if (!req->hRequest)
	{
		Console.Error("WinHttpOpenRequest() failed: %u", GetLastError());
		WinHttpCloseHandle(req->hConnection);
		return false;
	}

	BOOL result;
	if (req->type == HTTPDownloader::Request::Type::Post)
	{
		const std::wstring_view additional_headers(L"Content-Type: application/x-www-form-urlencoded\r\n");
		result = WinHttpSendRequest(req->hRequest, additional_headers.data(), static_cast<DWORD>(additional_headers.size()),
			req->post_data.data(), static_cast<DWORD>(req->post_data.size()),
			static_cast<DWORD>(req->post_data.size()), reinterpret_cast<DWORD_PTR>(req));
	}
	else
	{
		result = WinHttpSendRequest(req->hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0,
			reinterpret_cast<DWORD_PTR>(req));
	}

	if (!result && GetLastError() != ERROR_IO_PENDING)
	{
		Console.Error("WinHttpSendRequest() failed: %u", GetLastError());
		req->status_code = HTTP_STATUS_ERROR;
		req->state.store(Request::State::Complete);
	}

	DevCon.WriteLn("Started HTTP request for '%s'", req->url.c_str());
	req->state = Request::State::Started;
	req->start_time = Common::Timer::GetCurrentValue();
	return true;
}

void HTTPDownloaderWinHttp::CloseRequest(HTTPDownloader::Request* request)
{
	Request* req = static_cast<Request*>(request);

	if (req->hRequest != NULL)
	{
		// req will be freed by the callback.
		// the callback can fire immediately here if there's nothing running async, so don't touch req afterwards
		WinHttpCloseHandle(req->hRequest);
		return;
	}

	if (req->hConnection != NULL)
		WinHttpCloseHandle(req->hConnection);

	delete req;
}
