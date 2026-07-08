// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/HTTPDownloader.h"

#include <atomic>
#include <jni.h>
#include <memory>
#include <string>
#include <thread>

// HTTPDownloader implementation backed by Java's HttpURLConnection.
// One worker std::thread per request; the worker attaches to the JVM,
// invokes kr.co.iefriends.pcsx2.HttpClient.doRequest synchronously, and
// flips the request's atomic state to Complete on return. The base
// HTTPDownloader's poll loop owns lifecycle (state observation,
// Cancelled/Timeout transitions, callback invocation) — we just provide
// the actual transport.
//
// Why not curl? On Android we'd need to bundle libcurl + an OpenSSL/
// mbedTLS build into 3rdparty/ to get TLS — significant build-system
// surface. HttpURLConnection uses Android's system networking stack
// (system CA store, OS-managed proxy, TLS via the platform's OpenSSL),
// so this approach has zero third-party dependencies.
class HTTPDownloaderAndroid final : public HTTPDownloader
{
public:
	HTTPDownloaderAndroid();
	~HTTPDownloaderAndroid() override;

	bool Initialize(JavaVM* jvm, std::string user_agent);

	// Static one-time setup of the HttpClient class + method ID. Called
	// from JNI_OnLoad / NativeApp.initialize once we have a Java env.
	// Subsequent calls are no-ops.
	static bool BindFromJNI(JNIEnv* env);

protected:
	struct Request : HTTPDownloader::Request
	{
		std::thread worker;
	};

	HTTPDownloader::Request* InternalCreateRequest() override;
	void InternalPollRequests() override;
	bool StartRequest(HTTPDownloader::Request* request) override;
	void CloseRequest(HTTPDownloader::Request* request) override;

private:
	void RunRequest(Request* req);

	JavaVM*     m_jvm = nullptr;
	std::string m_user_agent;
};
