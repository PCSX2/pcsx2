// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/HTTPDownloaderAndroid.h"
#include "common/Console.h"
#include "common/Timer.h"

#include "fmt/format.h"

#include <chrono>

// Cached at JNI_OnLoad / NativeApp.initialize via BindFromJNI() since
// the worker thread doesn't have a Java class loader and must use a
// global ref looked up earlier.
static jclass    s_HttpClient_class       = nullptr; // GlobalRef
static jmethodID s_HttpClient_doRequest   = nullptr;
static jclass    s_Response_class         = nullptr; // GlobalRef
static jfieldID  s_Response_statusCode    = nullptr;
static jfieldID  s_Response_contentType   = nullptr;
static jfieldID  s_Response_data          = nullptr;
static JavaVM*   s_jvm                    = nullptr;

// Helper: clear a pending JNI exception (FindClass / GetMethodID throw on
// failure; the pending exception will abort the JVM the moment we return
// to Java code if not cleared). Returns whether one was pending.
static bool ClearPendingException(JNIEnv* env)
{
	if (!env->ExceptionCheck())
		return false;
	env->ExceptionDescribe(); // logs to logcat for triage
	env->ExceptionClear();
	return true;
}

bool HTTPDownloaderAndroid::BindFromJNI(JNIEnv* env)
{
	if (s_HttpClient_class)
		return true;

	if (env->GetJavaVM(&s_jvm) != JNI_OK)
		return false;

	// IMPORTANT: every JNI call below can throw. After each, clear any
	// pending exception BEFORE deciding success/failure — otherwise the
	// JVM aborts the moment we return to Java code, even if we wanted to
	// soft-fail (e.g. cover-art download not strictly required).

	jclass local = env->FindClass("kr/co/iefriends/pcsx2/HttpClient");
	ClearPendingException(env);
	if (!local)
	{
		Console.Error("HTTPDownloaderAndroid: FindClass(HttpClient) failed");
		return false;
	}
	s_HttpClient_class = static_cast<jclass>(env->NewGlobalRef(local));
	env->DeleteLocalRef(local);

	s_HttpClient_doRequest = env->GetStaticMethodID(s_HttpClient_class, "doRequest",
		"(Ljava/lang/String;Ljava/lang/String;[BLjava/lang/String;I)"
		"Lkr/co/iefriends/pcsx2/HttpClient$Response;");
	ClearPendingException(env);
	if (!s_HttpClient_doRequest)
	{
		Console.Error("HTTPDownloaderAndroid: GetStaticMethodID(doRequest) failed");
		return false;
	}

	jclass response_local = env->FindClass("kr/co/iefriends/pcsx2/HttpClient$Response");
	ClearPendingException(env);
	if (!response_local)
	{
		Console.Error("HTTPDownloaderAndroid: FindClass(Response) failed");
		return false;
	}
	s_Response_class       = static_cast<jclass>(env->NewGlobalRef(response_local));
	env->DeleteLocalRef(response_local);
	s_Response_statusCode  = env->GetFieldID(s_Response_class, "statusCode",  "I");
	ClearPendingException(env);
	s_Response_contentType = env->GetFieldID(s_Response_class, "contentType", "Ljava/lang/String;");
	ClearPendingException(env);
	s_Response_data        = env->GetFieldID(s_Response_class, "data",        "[B");
	ClearPendingException(env);
	if (!s_Response_statusCode || !s_Response_contentType || !s_Response_data)
	{
		Console.Error("HTTPDownloaderAndroid: GetFieldID failed");
		return false;
	}

	return true;
}

std::unique_ptr<HTTPDownloader> HTTPDownloader::Create(std::string user_agent)
{
	if (!s_jvm || !s_HttpClient_class)
	{
		Console.Error("HTTPDownloaderAndroid: BindFromJNI hasn't run; can't create downloader");
		return {};
	}
	auto inst = std::make_unique<HTTPDownloaderAndroid>();
	if (!inst->Initialize(s_jvm, std::move(user_agent)))
		return {};
	return inst;
}

HTTPDownloaderAndroid::HTTPDownloaderAndroid()
	: HTTPDownloader()
{
}

HTTPDownloaderAndroid::~HTTPDownloaderAndroid() = default;

bool HTTPDownloaderAndroid::Initialize(JavaVM* jvm, std::string user_agent)
{
	m_jvm = jvm;
	m_user_agent = std::move(user_agent);
	return true;
}

HTTPDownloader::Request* HTTPDownloaderAndroid::InternalCreateRequest()
{
	return new Request();
}

void HTTPDownloaderAndroid::InternalPollRequests()
{
	// Worker threads update Request::state directly (Complete on return),
	// so no per-poll reconciliation needed here. The base poll loop
	// observes the atomic transition and fires the callback.
}

bool HTTPDownloaderAndroid::StartRequest(HTTPDownloader::Request* request)
{
	auto* req = static_cast<Request*>(request);
	req->state.store(Request::State::Started, std::memory_order_release);
	req->start_time = Common::Timer::GetCurrentValue();
	// Detach the std::thread inside RunRequest after it completes — but
	// we still need to join in CloseRequest to make sure JNI cleanup is
	// done before deletion. So keep it joinable.
	req->worker = std::thread(&HTTPDownloaderAndroid::RunRequest, this, req);
	return true;
}

void HTTPDownloaderAndroid::CloseRequest(HTTPDownloader::Request* request)
{
	auto* req = static_cast<Request*>(request);
	if (req->worker.joinable())
		req->worker.join();
	delete req;
}

// Helper: log + clear any pending exception on the JNI worker thread.
// Worker JNI calls (NewStringUTF / NewByteArray / SetByteArrayRegion)
// can throw OutOfMemoryError / ArrayIndexOutOfBoundsException — leaving
// one pending poisons subsequent JNI calls and aborts the JVM the moment
// we DetachCurrentThread.
static bool ClearWorkerException(JNIEnv* env, const char* tag)
{
	if (!env->ExceptionCheck())
		return false;
	Console.Error(fmt::format("HTTPDownloaderAndroid: pending exception at {}", tag));
	env->ExceptionDescribe();
	env->ExceptionClear();
	return true;
}

void HTTPDownloaderAndroid::RunRequest(Request* req)
{
	if (!m_jvm || !s_HttpClient_class || !s_HttpClient_doRequest || !s_Response_class)
	{
		Console.Error("HTTPDownloaderAndroid: JNI bind state missing — failing request");
		req->status_code = HTTP_STATUS_ERROR;
		req->state.store(Request::State::Complete, std::memory_order_release);
		return;
	}

	JNIEnv* env = nullptr;
	bool attached = false;
	if (m_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
	{
		if (m_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK)
		{
			Console.Error(fmt::format("HTTPDownloaderAndroid: AttachCurrentThread failed for {}", req->url));
			req->status_code = HTTP_STATUS_ERROR;
			req->state.store(Request::State::Complete, std::memory_order_release);
			return;
		}
		attached = true;
	}

	// Defensive: clear any pending exception we may have inherited from
	// the caller's JNI frame (shouldn't happen since we just attached,
	// but cheap insurance).
	ClearWorkerException(env, "entry");

	jstring j_url    = env->NewStringUTF(req->url.c_str());
	ClearWorkerException(env, "NewStringUTF(url)");
	jstring j_method = env->NewStringUTF(req->type == Request::Type::Post ? "POST" : "GET");
	ClearWorkerException(env, "NewStringUTF(method)");
	jstring j_ua     = env->NewStringUTF(m_user_agent.c_str());
	ClearWorkerException(env, "NewStringUTF(ua)");

	jbyteArray j_post = nullptr;
	if (req->type == Request::Type::Post && !req->post_data.empty())
	{
		j_post = env->NewByteArray(static_cast<jsize>(req->post_data.size()));
		ClearWorkerException(env, "NewByteArray(post)");
		if (j_post)
		{
			env->SetByteArrayRegion(j_post, 0, static_cast<jsize>(req->post_data.size()),
				reinterpret_cast<const jbyte*>(req->post_data.data()));
			ClearWorkerException(env, "SetByteArrayRegion(post)");
		}
	}

	bool transport_failed = !j_url || !j_method || !j_ua;
	jobject response = nullptr;
	if (!transport_failed)
	{
		const jint timeout_ms = static_cast<jint>(m_timeout * 1000.0f);
		response = env->CallStaticObjectMethod(
			s_HttpClient_class, s_HttpClient_doRequest,
			j_url, j_method, j_post, j_ua, timeout_ms);
		if (ClearWorkerException(env, "CallStaticObjectMethod"))
			transport_failed = true;
	}

	if (transport_failed)
	{
		req->status_code = HTTP_STATUS_ERROR;
	}
	else if (response)
	{
		req->status_code = env->GetIntField(response, s_Response_statusCode);
		ClearWorkerException(env, "GetIntField(statusCode)");

		jstring j_ct = static_cast<jstring>(env->GetObjectField(response, s_Response_contentType));
		ClearWorkerException(env, "GetObjectField(contentType)");
		if (j_ct)
		{
			const char* utf = env->GetStringUTFChars(j_ct, nullptr);
			if (utf)
			{
				req->content_type = utf;
				env->ReleaseStringUTFChars(j_ct, utf);
			}
			env->DeleteLocalRef(j_ct);
		}

		jbyteArray j_data = static_cast<jbyteArray>(env->GetObjectField(response, s_Response_data));
		ClearWorkerException(env, "GetObjectField(data)");
		if (j_data)
		{
			const jsize len = env->GetArrayLength(j_data);
			req->data.resize(static_cast<size_t>(len));
			if (len > 0)
			{
				env->GetByteArrayRegion(j_data, 0, len, reinterpret_cast<jbyte*>(req->data.data()));
				ClearWorkerException(env, "GetByteArrayRegion(data)");
			}
			req->content_length = static_cast<u32>(len);
			env->DeleteLocalRef(j_data);
		}
		env->DeleteLocalRef(response);
	}
	else
	{
		// HttpClient.doRequest returned null — shouldn't happen since
		// the Java side always allocates Response, but treat as transport
		// error.
		req->status_code = HTTP_STATUS_ERROR;
	}

	if (j_url) env->DeleteLocalRef(j_url);
	if (j_method) env->DeleteLocalRef(j_method);
	if (j_ua) env->DeleteLocalRef(j_ua);
	if (j_post) env->DeleteLocalRef(j_post);

	// Final clear before detach — any leaked exception aborts the JVM.
	ClearWorkerException(env, "exit");

	if (attached)
		m_jvm->DetachCurrentThread();

	req->state.store(Request::State::Complete, std::memory_order_release);
}
