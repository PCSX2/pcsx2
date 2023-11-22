/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#define _WIN32_WINNT 0x0603
#define NOMINMAX

#include <algorithm>
#include <atomic>
#include <audioclient.h>
#include <avrt.h>
#include <cmath>
#include <devicetopology.h>
#include <initguid.h>
#include <limits>
#include <memory>
#include <mmdeviceapi.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <windef.h>
#include <windows.h>
/* clang-format off */
/* These need to be included after windows.h */
#include <mmsystem.h>
/* clang-format on */

#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_mixer.h"
#include "cubeb_resampler.h"
#include "cubeb_strings.h"
#include "cubeb_tracing.h"
#include "cubeb_utils.h"

// Windows 10 exposes the IAudioClient3 interface to create low-latency streams.
// Copy the interface definition from audioclient.h here to make the code
// simpler and so that we can still access IAudioClient3 via COM if cubeb was
// compiled against an older SDK.
#ifndef __IAudioClient3_INTERFACE_DEFINED__
#define __IAudioClient3_INTERFACE_DEFINED__
MIDL_INTERFACE("7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42")
IAudioClient3 : public IAudioClient
{
public:
  virtual HRESULT STDMETHODCALLTYPE GetSharedModeEnginePeriod(
      /* [annotation][in] */
      _In_ const WAVEFORMATEX * pFormat,
      /* [annotation][out] */
      _Out_ UINT32 * pDefaultPeriodInFrames,
      /* [annotation][out] */
      _Out_ UINT32 * pFundamentalPeriodInFrames,
      /* [annotation][out] */
      _Out_ UINT32 * pMinPeriodInFrames,
      /* [annotation][out] */
      _Out_ UINT32 * pMaxPeriodInFrames) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetCurrentSharedModeEnginePeriod(
      /* [unique][annotation][out] */
      _Out_ WAVEFORMATEX * *ppFormat,
      /* [annotation][out] */
      _Out_ UINT32 * pCurrentPeriodInFrames) = 0;

  virtual HRESULT STDMETHODCALLTYPE InitializeSharedAudioStream(
      /* [annotation][in] */
      _In_ DWORD StreamFlags,
      /* [annotation][in] */
      _In_ UINT32 PeriodInFrames,
      /* [annotation][in] */
      _In_ const WAVEFORMATEX * pFormat,
      /* [annotation][in] */
      _In_opt_ LPCGUID AudioSessionGuid) = 0;
};
#ifdef __CRT_UUID_DECL
// Required for MinGW
__CRT_UUID_DECL(IAudioClient3, 0x7ED4EE07, 0x8E67, 0x4CD4, 0x8C, 0x1A, 0x2B,
                0x7A, 0x59, 0x87, 0xAD, 0x42)
#endif
#endif
// Copied from audioclient.h in the Windows 10 SDK
#ifndef AUDCLNT_E_ENGINE_PERIODICITY_LOCKED
#define AUDCLNT_E_ENGINE_PERIODICITY_LOCKED AUDCLNT_ERR(0x028)
#endif

#ifndef PKEY_Device_FriendlyName
DEFINE_PROPERTYKEY(PKEY_Device_FriendlyName, 0xa45c254e, 0xdf1c, 0x4efd, 0x80,
                   0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0,
                   14); // DEVPROP_TYPE_STRING
#endif
#ifndef PKEY_Device_InstanceId
DEFINE_PROPERTYKEY(PKEY_Device_InstanceId, 0x78c34fc8, 0x104a, 0x4aca, 0x9e,
                   0xa4, 0x52, 0x4d, 0x52, 0x99, 0x6e, 0x57,
                   0x00000100); //    VT_LPWSTR
#endif

namespace {

const int64_t LATENCY_NOT_AVAILABLE_YET = -1;

const DWORD DEVICE_CHANGE_DEBOUNCE_MS = 250;

struct com_heap_ptr_deleter {
  void operator()(void * ptr) const noexcept { CoTaskMemFree(ptr); }
};

template <typename T>
using com_heap_ptr = std::unique_ptr<T, com_heap_ptr_deleter>;

template <typename T, size_t N>
constexpr size_t
ARRAY_LENGTH(T (&)[N])
{
  return N;
}

template <typename T> class no_addref_release : public T {
  ULONG STDMETHODCALLTYPE AddRef() = 0;
  ULONG STDMETHODCALLTYPE Release() = 0;
};

template <typename T> class com_ptr {
public:
  com_ptr() noexcept = default;

  com_ptr(com_ptr const & other) noexcept = delete;
  com_ptr & operator=(com_ptr const & other) noexcept = delete;
  T ** operator&() const noexcept = delete;

  ~com_ptr() noexcept { release(); }

  com_ptr(com_ptr && other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }

  com_ptr & operator=(com_ptr && other) noexcept
  {
    if (ptr != other.ptr) {
      release();
      ptr = other.ptr;
      other.ptr = nullptr;
    }
    return *this;
  }

  explicit operator bool() const noexcept { return nullptr != ptr; }

  no_addref_release<T> * operator->() const noexcept
  {
    return static_cast<no_addref_release<T> *>(ptr);
  }

  T * get() const noexcept { return ptr; }

  T ** receive() noexcept
  {
    XASSERT(ptr == nullptr);
    return &ptr;
  }

  void ** receive_vpp() noexcept
  {
    return reinterpret_cast<void **>(receive());
  }

  com_ptr & operator=(std::nullptr_t) noexcept
  {
    release();
    return *this;
  }

  void reset(T * p = nullptr) noexcept
  {
    release();
    ptr = p;
  }

private:
  void release() noexcept
  {
    T * temp = ptr;

    if (temp) {
      ptr = nullptr;
      temp->Release();
    }
  }

  T * ptr = nullptr;
};

LONG
wasapi_stream_add_ref(cubeb_stream * stm);
LONG
wasapi_stream_release(cubeb_stream * stm);

struct auto_stream_ref {
  auto_stream_ref(cubeb_stream * stm_) : stm(stm_)
  {
    wasapi_stream_add_ref(stm);
  }
  ~auto_stream_ref() { wasapi_stream_release(stm); }
  cubeb_stream * stm;
};

extern cubeb_ops const wasapi_ops;

static com_heap_ptr<wchar_t>
wasapi_get_default_device_id(EDataFlow flow, ERole role,
                             IMMDeviceEnumerator * enumerator);

struct wasapi_default_devices {
  wasapi_default_devices(IMMDeviceEnumerator * enumerator)
      : render_console_id(
            wasapi_get_default_device_id(eRender, eConsole, enumerator)),
        render_comms_id(
            wasapi_get_default_device_id(eRender, eCommunications, enumerator)),
        capture_console_id(
            wasapi_get_default_device_id(eCapture, eConsole, enumerator)),
        capture_comms_id(
            wasapi_get_default_device_id(eCapture, eCommunications, enumerator))
  {
  }

  bool is_default(EDataFlow flow, ERole role, wchar_t const * id)
  {
    wchar_t const * default_id = nullptr;
    if (flow == eRender && role == eConsole) {
      default_id = this->render_console_id.get();
    } else if (flow == eRender && role == eCommunications) {
      default_id = this->render_comms_id.get();
    } else if (flow == eCapture && role == eConsole) {
      default_id = this->capture_console_id.get();
    } else if (flow == eCapture && role == eCommunications) {
      default_id = this->capture_comms_id.get();
    }

    return default_id && wcscmp(id, default_id) == 0;
  }

private:
  com_heap_ptr<wchar_t> render_console_id;
  com_heap_ptr<wchar_t> render_comms_id;
  com_heap_ptr<wchar_t> capture_console_id;
  com_heap_ptr<wchar_t> capture_comms_id;
};

struct AutoRegisterThread {
  AutoRegisterThread(const char * name) { CUBEB_REGISTER_THREAD(name); }
  ~AutoRegisterThread() { CUBEB_UNREGISTER_THREAD(); }
};

int
wasapi_stream_stop(cubeb_stream * stm);
int
wasapi_stream_start(cubeb_stream * stm);
void
close_wasapi_stream(cubeb_stream * stm);
int
setup_wasapi_stream(cubeb_stream * stm);
ERole
pref_to_role(cubeb_stream_prefs param);
int
wasapi_create_device(cubeb * ctx, cubeb_device_info & ret,
                     IMMDeviceEnumerator * enumerator, IMMDevice * dev,
                     wasapi_default_devices * defaults);
void
wasapi_destroy_device(cubeb_device_info * device_info);
static int
wasapi_enumerate_devices_internal(cubeb * context, cubeb_device_type type,
                                  cubeb_device_collection * out,
                                  DWORD state_mask);
static int
wasapi_device_collection_destroy(cubeb * ctx,
                                 cubeb_device_collection * collection);
static char const *
wstr_to_utf8(wchar_t const * str);
static std::unique_ptr<wchar_t const[]>
utf8_to_wstr(char const * str);

} // namespace

class wasapi_collection_notification_client;
class monitor_device_notifications;

struct cubeb {
  cubeb_ops const * ops = &wasapi_ops;
  owned_critical_section lock;
  cubeb_strings * device_ids;
  /* Device enumerator to get notifications when the
     device collection change. */
  com_ptr<IMMDeviceEnumerator> device_collection_enumerator;
  com_ptr<wasapi_collection_notification_client> collection_notification_client;
  /* Collection changed for input (capture) devices. */
  cubeb_device_collection_changed_callback input_collection_changed_callback =
      nullptr;
  void * input_collection_changed_user_ptr = nullptr;
  /* Collection changed for output (render) devices. */
  cubeb_device_collection_changed_callback output_collection_changed_callback =
      nullptr;
  void * output_collection_changed_user_ptr = nullptr;
  UINT64 performance_counter_frequency;
};

class wasapi_endpoint_notification_client;

/* We have three possible callbacks we can use with a stream:
 * - input only
 * - output only
 * - synchronized input and output
 *
 * Returns true when we should continue to play, false otherwise.
 */
typedef bool (*wasapi_refill_callback)(cubeb_stream * stm);

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context = nullptr;
  void * user_ptr = nullptr;
  /**/

  /* Mixer pameters. We need to convert the input stream to this
     samplerate/channel layout, as WASAPI does not resample nor upmix
     itself. */
  cubeb_stream_params input_mix_params = {CUBEB_SAMPLE_FLOAT32NE, 0, 0,
                                          CUBEB_LAYOUT_UNDEFINED,
                                          CUBEB_STREAM_PREF_NONE};
  cubeb_stream_params output_mix_params = {CUBEB_SAMPLE_FLOAT32NE, 0, 0,
                                           CUBEB_LAYOUT_UNDEFINED,
                                           CUBEB_STREAM_PREF_NONE};
  /* Stream parameters. This is what the client requested,
   * and what will be presented in the callback. */
  cubeb_stream_params input_stream_params = {CUBEB_SAMPLE_FLOAT32NE, 0, 0,
                                             CUBEB_LAYOUT_UNDEFINED,
                                             CUBEB_STREAM_PREF_NONE};
  cubeb_stream_params output_stream_params = {CUBEB_SAMPLE_FLOAT32NE, 0, 0,
                                              CUBEB_LAYOUT_UNDEFINED,
                                              CUBEB_STREAM_PREF_NONE};
  /* A MMDevice role for this stream: either communication or console here. */
  ERole role;
  /* True if this stream will transport voice-data. */
  bool voice;
  /* True if the input device of this stream is using bluetooth handsfree. */
  bool input_bluetooth_handsfree;
  /* The input and output device, or NULL for default. */
  std::unique_ptr<const wchar_t[]> input_device_id;
  std::unique_ptr<const wchar_t[]> output_device_id;
  com_ptr<IMMDevice> input_device;
  com_ptr<IMMDevice> output_device;
  /* The latency initially requested for this stream, in frames. */
  unsigned latency = 0;
  cubeb_state_callback state_callback = nullptr;
  cubeb_data_callback data_callback = nullptr;
  wasapi_refill_callback refill_callback = nullptr;
  /* True when a loopback device is requested with no output device. In this
     case a dummy output device is opened to drive the loopback, but should not
     be exposed. */
  bool has_dummy_output = false;
  /* Lifetime considerations:
     - client, render_client, audio_clock and audio_stream_volume are interface
       pointer to the IAudioClient.
     - The lifetime for device_enumerator and notification_client, resampler,
       mix_buffer are the same as the cubeb_stream instance. */

  /* Main handle on the WASAPI stream. */
  com_ptr<IAudioClient> output_client;
  /* Interface pointer to use the event-driven interface. */
  com_ptr<IAudioRenderClient> render_client;
#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
  /* Interface pointer to use the volume facilities. */
  com_ptr<IAudioStreamVolume> audio_stream_volume;
#endif
  /* Interface pointer to use the stream audio clock. */
  com_ptr<IAudioClock> audio_clock;
  /* Frames written to the stream since it was opened. Reset on device
     change. Uses mix_params.rate. */
  UINT64 frames_written = 0;
  /* Frames written to the (logical) stream since it was first
     created. Updated on device change. Uses stream_params.rate. */
  UINT64 total_frames_written = 0;
  /* Last valid reported stream position.  Used to ensure the position
     reported by stream_get_position increases monotonically. */
  UINT64 prev_position = 0;
  /* Device enumerator to be able to be notified when the default
     device change. */
  com_ptr<IMMDeviceEnumerator> device_enumerator;
  /* Device notification client, to be able to be notified when the default
     audio device changes and route the audio to the new default audio output
     device */
  com_ptr<wasapi_endpoint_notification_client> notification_client;
  /* Main andle to the WASAPI capture stream. */
  com_ptr<IAudioClient> input_client;
  /* Interface to use the event driven capture interface */
  com_ptr<IAudioCaptureClient> capture_client;
  /* This event is set by the stream_destroy function, so the render loop can
     exit properly. */
  HANDLE shutdown_event = 0;
  /* Set by OnDefaultDeviceChanged when a stream reconfiguration is required.
     The reconfiguration is handled by the render loop thread. */
  HANDLE reconfigure_event = 0;
  /* This is set by WASAPI when we should refill the stream. */
  HANDLE refill_event = 0;
  /* This is set by WASAPI when we should read from the input stream. In
   * practice, we read from the input stream in the output callback, so
   * this is not used, but it is necessary to start getting input data. */
  HANDLE input_available_event = 0;
  /* Each cubeb_stream has its own thread. */
  HANDLE thread = 0;
  /* The lock protects all members that are touched by the render thread or
     change during a device reset, including: audio_clock, audio_stream_volume,
     client, frames_written, mix_params, total_frames_written, prev_position. */
  owned_critical_section stream_reset_lock;
  /* Maximum number of frames that can be passed down in a callback. */
  uint32_t input_buffer_frame_count = 0;
  /* Maximum number of frames that can be requested in a callback. */
  uint32_t output_buffer_frame_count = 0;
  /* Resampler instance. Resampling will only happen if necessary. */
  std::unique_ptr<cubeb_resampler, decltype(&cubeb_resampler_destroy)>
      resampler = {nullptr, cubeb_resampler_destroy};
  /* Mixer interfaces */
  std::unique_ptr<cubeb_mixer, decltype(&cubeb_mixer_destroy)> output_mixer = {
      nullptr, cubeb_mixer_destroy};
  std::unique_ptr<cubeb_mixer, decltype(&cubeb_mixer_destroy)> input_mixer = {
      nullptr, cubeb_mixer_destroy};
  /* A buffer for up/down mixing multi-channel audio output. */
  std::vector<BYTE> mix_buffer;
  /* WASAPI input works in "packets". We re-linearize the audio packets
   * into this buffer before handing it to the resampler. */
  std::unique_ptr<auto_array_wrapper> linear_input_buffer;
  /* Bytes per sample. This multiplied by the number of channels is the number
   * of bytes per frame. */
  size_t bytes_per_sample = 0;
  /* WAVEFORMATEXTENSIBLE sub-format: either PCM or float. */
  GUID waveformatextensible_sub_format = GUID_NULL;
  /* Stream volume.  Set via stream_set_volume and used to reset volume on
     device changes. */
  float volume = 1.0;
  /* True if the stream is draining. */
  bool draining = false;
  /* This needs an active audio input stream to be known, and is updated in the
   * first audio input callback. */
  std::atomic<int64_t> input_latency_hns{LATENCY_NOT_AVAILABLE_YET};
  /* Those attributes count the number of frames requested (resp. received) by
  the OS, to be able to detect drifts. This is only used for logging for now. */
  size_t total_input_frames = 0;
  size_t total_output_frames = 0;
  /* This is set by the render loop thread once it has obtained a reference to
   * COM and this stream object. */
  HANDLE thread_ready_event = 0;
  /* Keep a ref count on this stream object. After both stream_destroy has been
   * called and the render loop thread has exited, destroy this stream object.
   */
  LONG ref_count = 0;

  /* True if the stream is active, false if inactive. */
  bool active = false;
};

class monitor_device_notifications {
public:
  monitor_device_notifications(cubeb * context) : cubeb_context(context)
  {
    create_thread();
  }

  ~monitor_device_notifications()
  {
    SetEvent(begin_shutdown);
    WaitForSingleObject(shutdown_complete, INFINITE);
    CloseHandle(thread);

    CloseHandle(input_changed);
    CloseHandle(output_changed);
    CloseHandle(begin_shutdown);
    CloseHandle(shutdown_complete);
  }

  void notify(EDataFlow flow)
  {
    XASSERT(cubeb_context);
    if (flow == eCapture && cubeb_context->input_collection_changed_callback) {
      bool res = SetEvent(input_changed);
      if (!res) {
        LOG("Failed to set input changed event");
      }
      return;
    }
    if (flow == eRender && cubeb_context->output_collection_changed_callback) {
      bool res = SetEvent(output_changed);
      if (!res) {
        LOG("Failed to set output changed event");
      }
    }
  }

private:
  static unsigned int __stdcall thread_proc(LPVOID args)
  {
    AutoRegisterThread raii("WASAPI device notification thread");
    XASSERT(args);
    auto mdn = static_cast<monitor_device_notifications *>(args);
    mdn->notification_thread_loop();
    SetEvent(mdn->shutdown_complete);
    return 0;
  }

  void notification_thread_loop()
  {
    struct auto_com {
      auto_com()
      {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        XASSERT(SUCCEEDED(hr));
      }
      ~auto_com() { CoUninitialize(); }
    } com;

    HANDLE wait_array[3] = {
        input_changed,
        output_changed,
        begin_shutdown,
    };

    while (true) {
      Sleep(200);

      DWORD wait_result = WaitForMultipleObjects(ARRAY_LENGTH(wait_array),
                                                 wait_array, FALSE, INFINITE);
      if (wait_result == WAIT_OBJECT_0) { // input changed
        cubeb_context->input_collection_changed_callback(
            cubeb_context, cubeb_context->input_collection_changed_user_ptr);
      } else if (wait_result == WAIT_OBJECT_0 + 1) { // output changed
        cubeb_context->output_collection_changed_callback(
            cubeb_context, cubeb_context->output_collection_changed_user_ptr);
      } else if (wait_result == WAIT_OBJECT_0 + 2) { // shutdown
        break;
      } else {
        LOG("Unexpected result %lu", wait_result);
      }
    } // loop
  }

  void create_thread()
  {
    output_changed = CreateEvent(nullptr, 0, 0, nullptr);
    if (!output_changed) {
      LOG("Failed to create output changed event.");
      return;
    }

    input_changed = CreateEvent(nullptr, 0, 0, nullptr);
    if (!input_changed) {
      LOG("Failed to create input changed event.");
      return;
    }

    begin_shutdown = CreateEvent(nullptr, 0, 0, nullptr);
    if (!begin_shutdown) {
      LOG("Failed to create begin_shutdown event.");
      return;
    }

    shutdown_complete = CreateEvent(nullptr, 0, 0, nullptr);
    if (!shutdown_complete) {
      LOG("Failed to create shutdown_complete event.");
      return;
    }

    thread = (HANDLE)_beginthreadex(nullptr, 256 * 1024, thread_proc, this,
                                    STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (!thread) {
      LOG("Failed to create thread.");
      return;
    }
  }

  HANDLE thread = INVALID_HANDLE_VALUE;
  HANDLE output_changed = INVALID_HANDLE_VALUE;
  HANDLE input_changed = INVALID_HANDLE_VALUE;
  HANDLE begin_shutdown = INVALID_HANDLE_VALUE;
  HANDLE shutdown_complete = INVALID_HANDLE_VALUE;

  cubeb * cubeb_context = nullptr;
};

class wasapi_collection_notification_client : public IMMNotificationClient {
public:
  /* The implementation of MSCOM was copied from MSDN. */
  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&ref_count); }

  ULONG STDMETHODCALLTYPE Release()
  {
    ULONG ulRef = InterlockedDecrement(&ref_count);
    if (0 == ulRef) {
      delete this;
    }
    return ulRef;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID ** ppvInterface)
  {
    if (__uuidof(IUnknown) == riid) {
      AddRef();
      *ppvInterface = (IUnknown *)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = (IMMNotificationClient *)this;
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  wasapi_collection_notification_client(cubeb * context)
      : ref_count(1), cubeb_context(context), monitor_notifications(context)
  {
    XASSERT(cubeb_context);
  }

  virtual ~wasapi_collection_notification_client() {}

  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                   LPCWSTR device_id)
  {
    LOG("collection: Audio device default changed, id = %S.", device_id);
    return S_OK;
  }

  /* The remaining methods are not implemented, they simply log when called (if
     log is enabled), for debugging. */
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR device_id)
  {
    LOG("collection: Audio device added.");
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR device_id)
  {
    LOG("collection: Audio device removed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR device_id,
                                                 DWORD new_state)
  {
    XASSERT(cubeb_context->output_collection_changed_callback ||
            cubeb_context->input_collection_changed_callback);
    LOG("collection: Audio device state changed, id = %S, state = %lu.",
        device_id, new_state);
    EDataFlow flow;
    HRESULT hr = GetDataFlow(device_id, &flow);
    if (FAILED(hr)) {
      return hr;
    }
    monitor_notifications.notify(flow);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR device_id,
                                                   const PROPERTYKEY key)
  {
    // Audio device property value changed.
    return S_OK;
  }

private:
  HRESULT GetDataFlow(LPCWSTR device_id, EDataFlow * flow)
  {
    com_ptr<IMMDevice> device;
    com_ptr<IMMEndpoint> endpoint;

    HRESULT hr = cubeb_context->device_collection_enumerator->GetDevice(
        device_id, device.receive());
    if (FAILED(hr)) {
      LOG("collection: Could not get device: %lx", hr);
      return hr;
    }

    hr = device->QueryInterface(IID_PPV_ARGS(endpoint.receive()));
    if (FAILED(hr)) {
      LOG("collection: Could not get endpoint: %lx", hr);
      return hr;
    }

    return endpoint->GetDataFlow(flow);
  }

  /* refcount for this instance, necessary to implement MSCOM semantics. */
  LONG ref_count;

  cubeb * cubeb_context = nullptr;
  monitor_device_notifications monitor_notifications;
};

class wasapi_endpoint_notification_client : public IMMNotificationClient {
public:
  /* The implementation of MSCOM was copied from MSDN. */
  ULONG STDMETHODCALLTYPE AddRef() { return InterlockedIncrement(&ref_count); }

  ULONG STDMETHODCALLTYPE Release()
  {
    ULONG ulRef = InterlockedDecrement(&ref_count);
    if (0 == ulRef) {
      delete this;
    }
    return ulRef;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID ** ppvInterface)
  {
    if (__uuidof(IUnknown) == riid) {
      AddRef();
      *ppvInterface = (IUnknown *)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
      AddRef();
      *ppvInterface = (IMMNotificationClient *)this;
    } else {
      *ppvInterface = NULL;
      return E_NOINTERFACE;
    }
    return S_OK;
  }

  wasapi_endpoint_notification_client(HANDLE event, ERole role)
      : ref_count(1), reconfigure_event(event), role(role),
        last_device_change(timeGetTime())
  {
  }

  virtual ~wasapi_endpoint_notification_client() {}

  HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role,
                                                   LPCWSTR device_id)
  {
    LOG("endpoint: Audio device default changed flow=%d role=%d "
        "new_device_id=%ws.",
        flow, role, device_id);

    /* we only support a single stream type for now. */
    if (flow != eRender || role != this->role) {
      return S_OK;
    }

    DWORD last_change_ms = timeGetTime() - last_device_change;
    bool same_device = default_device_id && device_id &&
                       wcscmp(default_device_id.get(), device_id) == 0;
    LOG("endpoint: Audio device default changed last_change=%u same_device=%d",
        last_change_ms, same_device);
    if (last_change_ms > DEVICE_CHANGE_DEBOUNCE_MS || !same_device) {
      if (device_id) {
        default_device_id.reset(_wcsdup(device_id));
      } else {
        default_device_id.reset();
      }
      BOOL ok = SetEvent(reconfigure_event);
      LOG("endpoint: Audio device default changed: trigger reconfig");
      if (!ok) {
        LOG("endpoint: SetEvent on reconfigure_event failed: %lx",
            GetLastError());
      }
    }

    return S_OK;
  }

  /* The remaining methods are not implemented, they simply log when called (if
     log is enabled), for debugging. */
  HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR device_id)
  {
    LOG("endpoint: Audio device added.");
    return S_OK;
  };

  HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR device_id)
  {
    LOG("endpoint: Audio device removed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR device_id,
                                                 DWORD new_state)
  {
    LOG("endpoint: Audio device state changed.");
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR device_id,
                                                   const PROPERTYKEY key)
  {
    // Audio device property value changed.
    return S_OK;
  }

private:
  /* refcount for this instance, necessary to implement MSCOM semantics. */
  LONG ref_count;
  HANDLE reconfigure_event;
  ERole role;
  std::unique_ptr<const wchar_t[]> default_device_id;
  DWORD last_device_change;
};

namespace {

long
wasapi_data_callback(cubeb_stream * stm, void * user_ptr,
                     void const * input_buffer, void * output_buffer,
                     long nframes)
{
  return stm->data_callback(stm, user_ptr, input_buffer, output_buffer,
                            nframes);
}

void
wasapi_state_callback(cubeb_stream * stm, void * user_ptr, cubeb_state state)
{
  return stm->state_callback(stm, user_ptr, state);
}

char const *
intern_device_id(cubeb * ctx, wchar_t const * id)
{
  XASSERT(id);

  auto_lock lock(ctx->lock);

  char const * tmp = wstr_to_utf8(id);
  if (!tmp) {
    return nullptr;
  }

  char const * interned = cubeb_strings_intern(ctx->device_ids, tmp);

  free((void *)tmp);

  return interned;
}

bool
has_input(cubeb_stream * stm)
{
  return stm->input_stream_params.rate != 0;
}

bool
has_output(cubeb_stream * stm)
{
  return stm->output_stream_params.rate != 0;
}

double
stream_to_mix_samplerate_ratio(cubeb_stream_params & stream,
                               cubeb_stream_params & mixer)
{
  return double(stream.rate) / mixer.rate;
}

/* Convert the channel layout into the corresponding KSAUDIO_CHANNEL_CONFIG.
   See more:
   https://msdn.microsoft.com/en-us/library/windows/hardware/ff537083(v=vs.85).aspx
 */

cubeb_channel_layout
mask_to_channel_layout(WAVEFORMATEX const * fmt)
{
  cubeb_channel_layout mask = 0;

  if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE const * ext =
        reinterpret_cast<WAVEFORMATEXTENSIBLE const *>(fmt);
    mask = ext->dwChannelMask;
  } else if (fmt->wFormatTag == WAVE_FORMAT_PCM ||
             fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    if (fmt->nChannels == 1) {
      mask = CHANNEL_FRONT_CENTER;
    } else if (fmt->nChannels == 2) {
      mask = CHANNEL_FRONT_LEFT | CHANNEL_FRONT_RIGHT;
    }
  }
  return mask;
}

uint32_t
get_rate(cubeb_stream * stm)
{
  return has_input(stm) ? stm->input_stream_params.rate
                        : stm->output_stream_params.rate;
}

uint32_t
hns_to_frames(uint32_t rate, REFERENCE_TIME hns)
{
  return std::ceil((hns - 1) / 10000000.0 * rate);
}

uint32_t
hns_to_frames(cubeb_stream * stm, REFERENCE_TIME hns)
{
  return hns_to_frames(get_rate(stm), hns);
}

REFERENCE_TIME
frames_to_hns(uint32_t rate, uint32_t frames)
{
  return std::ceil(frames * 10000000.0 / rate);
}

/* This returns the size of a frame in the stream, before the eventual upmix
   occurs. */
static size_t
frames_to_bytes_before_mix(cubeb_stream * stm, size_t frames)
{
  // This is called only when we has a output client.
  XASSERT(has_output(stm));
  return stm->output_stream_params.channels * stm->bytes_per_sample * frames;
}

/* This function handles the processing of the input and output audio,
 * converting it to rate and channel layout specified at initialization.
 * It then calls the data callback, via the resampler. */
long
refill(cubeb_stream * stm, void * input_buffer, long input_frames_count,
       void * output_buffer, long output_frames_needed)
{
  XASSERT(!stm->draining);
  /* If we need to upmix after resampling, resample into the mix buffer to
     avoid a copy. Avoid exposing output if it is a dummy stream. */
  void * dest = nullptr;
  if (has_output(stm) && !stm->has_dummy_output) {
    if (stm->output_mixer) {
      dest = stm->mix_buffer.data();
    } else {
      dest = output_buffer;
    }
  }

  long out_frames =
      cubeb_resampler_fill(stm->resampler.get(), input_buffer,
                           &input_frames_count, dest, output_frames_needed);
  if (out_frames < 0) {
    ALOGV("Callback refill error: %d", out_frames);
    wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return out_frames;
  }

  float volume = 1.0;
  {
    auto_lock lock(stm->stream_reset_lock);
    stm->frames_written += out_frames;
    volume = stm->volume;
  }

  /* Go in draining mode if we got fewer frames than requested. If the stream
     has no output we still expect the callback to return number of frames read
     from input, otherwise we stop. */
  if ((out_frames < output_frames_needed) ||
      (!has_output(stm) && out_frames < input_frames_count)) {
    LOG("start draining.");
    stm->draining = true;
  }

  /* If this is not true, there will be glitches.
     It is alright to have produced less frames if we are draining, though. */
  XASSERT(out_frames == output_frames_needed || stm->draining ||
          !has_output(stm) || stm->has_dummy_output);

#ifndef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
  if (has_output(stm) && !stm->has_dummy_output && volume != 1.0) {
    // Adjust the output volume.
    // Note: This could be integrated with the remixing below.
    long out_samples = out_frames * stm->output_stream_params.channels;
    if (volume == 0.0) {
      memset(dest, 0, out_samples * stm->bytes_per_sample);
    } else {
      switch (stm->output_stream_params.format) {
      case CUBEB_SAMPLE_FLOAT32NE: {
        float * buf = static_cast<float *>(dest);
        for (long i = 0; i < out_samples; ++i) {
          buf[i] *= volume;
        }
        break;
      }
      case CUBEB_SAMPLE_S16NE: {
        short * buf = static_cast<short *>(dest);
        for (long i = 0; i < out_samples; ++i) {
          buf[i] = static_cast<short>(static_cast<float>(buf[i]) * volume);
        }
        break;
      }
      default:
        XASSERT(false);
      }
    }
  }
#endif

  // We don't bother mixing dummy output as it will be silenced, otherwise mix
  // output if needed
  if (!stm->has_dummy_output && has_output(stm) && stm->output_mixer) {
    XASSERT(dest == stm->mix_buffer.data());
    size_t dest_size =
        out_frames * stm->output_stream_params.channels * stm->bytes_per_sample;
    XASSERT(dest_size <= stm->mix_buffer.size());
    size_t output_buffer_size =
        out_frames * stm->output_mix_params.channels * stm->bytes_per_sample;
    int ret = cubeb_mixer_mix(stm->output_mixer.get(), out_frames, dest,
                              dest_size, output_buffer, output_buffer_size);
    if (ret < 0) {
      LOG("Error remixing content (%d)", ret);
    }
  }

  return out_frames;
}

bool
trigger_async_reconfigure(cubeb_stream * stm)
{
  XASSERT(stm && stm->reconfigure_event);
  LOG("Try reconfiguring the stream");
  BOOL ok = SetEvent(stm->reconfigure_event);
  if (!ok) {
    LOG("SetEvent on reconfigure_event failed: %lx", GetLastError());
  }
  return static_cast<bool>(ok);
}

/* This helper grabs all the frames available from a capture client, put them in
 * the linear_input_buffer.  This helper does not work with exclusive mode
 * streams. */
bool
get_input_buffer(cubeb_stream * stm)
{
  XASSERT(has_input(stm));

  HRESULT hr;
  BYTE * input_packet = NULL;
  DWORD flags;
  UINT64 dev_pos;
  UINT64 pc_position;
  UINT32 next;
  /* Get input packets until we have captured enough frames, and put them in a
   * contiguous buffer. */
  uint32_t offset = 0;
  // If the input stream is event driven we should only ever expect to read a
  // single packet each time. However, if we're pulling from the stream we may
  // need to grab multiple packets worth of frames that have accumulated (so
  // need a loop).
  for (hr = stm->capture_client->GetNextPacketSize(&next); next > 0;
       hr = stm->capture_client->GetNextPacketSize(&next)) {
    if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
      // Application can recover from this error. More info
      // https://msdn.microsoft.com/en-us/library/windows/desktop/dd316605(v=vs.85).aspx
      LOG("Input device invalidated error");
      // No need to reset device if user asks to use particular device, or
      // switching is disabled.
      if (stm->input_device_id ||
          (stm->input_stream_params.prefs &
           CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING) ||
          !trigger_async_reconfigure(stm)) {
        wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
        return false;
      }
      return true;
    }

    if (FAILED(hr)) {
      LOG("cannot get next packet size: %lx", hr);
      return false;
    }

    UINT32 frames;
    hr = stm->capture_client->GetBuffer(&input_packet, &frames, &flags,
                                        &dev_pos, &pc_position);

    if (FAILED(hr)) {
      LOG("GetBuffer failed for capture: %lx", hr);
      return false;
    }
    XASSERT(frames == next);

    if (stm->context->performance_counter_frequency) {
      LARGE_INTEGER now;
      UINT64 now_hns;
      // See
      // https://docs.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer,
      // section "Remarks".
      QueryPerformanceCounter(&now);
      now_hns =
          10000000 * now.QuadPart / stm->context->performance_counter_frequency;
      if (now_hns >= pc_position) {
        stm->input_latency_hns = now_hns - pc_position;
      }
    }

    stm->total_input_frames += frames;

    UINT32 input_stream_samples = frames * stm->input_stream_params.channels;
    // We do not explicitly handle the AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY
    // flag. There a two primary (non exhaustive) scenarios we anticipate this
    // flag being set in:
    //   - The first GetBuffer after Start has this flag undefined. In this
    //     case the flag may be set but is meaningless and can be ignored.
    //   - If a glitch is introduced into the input. This should not happen
    //     for event based inputs, and should be mitigated by using a dummy
    //     stream to drive input in the case of input only loopback. Without
    //     a dummy output, input only loopback would glitch on silence. However,
    //     the dummy input should push silence to the loopback and prevent
    //     discontinuities. See
    //     https://blogs.msdn.microsoft.com/matthew_van_eerde/2008/12/16/sample-wasapi-loopback-capture-record-what-you-hear/
    // As the first scenario can be ignored, and we anticipate the second
    // scenario is mitigated, we ignore the flag.
    // For more info:
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd370859(v=vs.85).aspx,
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd371458(v=vs.85).aspx
    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
      LOG("insert silence: ps=%u", frames);
      stm->linear_input_buffer->push_silence(input_stream_samples);
    } else {
      if (stm->input_mixer) {
        bool ok = stm->linear_input_buffer->reserve(
            stm->linear_input_buffer->length() + input_stream_samples);
        XASSERT(ok);
        size_t input_packet_size =
            frames * stm->input_mix_params.channels *
            cubeb_sample_size(stm->input_mix_params.format);
        size_t linear_input_buffer_size =
            input_stream_samples *
            cubeb_sample_size(stm->input_stream_params.format);
        cubeb_mixer_mix(stm->input_mixer.get(), frames, input_packet,
                        input_packet_size, stm->linear_input_buffer->end(),
                        linear_input_buffer_size);
        stm->linear_input_buffer->set_length(
            stm->linear_input_buffer->length() + input_stream_samples);
      } else {
        stm->linear_input_buffer->push(input_packet, input_stream_samples);
      }
    }
    hr = stm->capture_client->ReleaseBuffer(frames);
    if (FAILED(hr)) {
      LOG("FAILED to release intput buffer");
      return false;
    }
    offset += input_stream_samples;
  }

  ALOGV("get_input_buffer: got %d frames", offset);

  XASSERT(stm->linear_input_buffer->length() >= offset);

  return true;
}

/* Get an output buffer from the render_client. It has to be released before
 * exiting the callback. */
bool
get_output_buffer(cubeb_stream * stm, void *& buffer, size_t & frame_count)
{
  UINT32 padding_out;
  HRESULT hr;

  XASSERT(has_output(stm));

  hr = stm->output_client->GetCurrentPadding(&padding_out);
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
    // Application can recover from this error. More info
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd316605(v=vs.85).aspx
    LOG("Output device invalidated error");
    // No need to reset device if user asks to use particular device, or
    // switching is disabled.
    if (stm->output_device_id ||
        (stm->output_stream_params.prefs &
         CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING) ||
        !trigger_async_reconfigure(stm)) {
      wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
      return false;
    }
    return true;
  }

  if (FAILED(hr)) {
    LOG("Failed to get padding: %lx", hr);
    return false;
  }

  XASSERT(padding_out <= stm->output_buffer_frame_count);

  if (stm->draining) {
    if (padding_out == 0) {
      LOG("Draining finished.");
      wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      return false;
    }
    LOG("Draining.");
    return true;
  }

  frame_count = stm->output_buffer_frame_count - padding_out;
  BYTE * output_buffer;

  hr = stm->render_client->GetBuffer(frame_count, &output_buffer);
  if (FAILED(hr)) {
    LOG("cannot get render buffer");
    return false;
  }

  buffer = output_buffer;

  return true;
}

/**
 * This function gets input data from a input device, and pass it along with an
 * output buffer to the resamplers.  */
bool
refill_callback_duplex(cubeb_stream * stm)
{
  HRESULT hr;
  void * output_buffer = nullptr;
  size_t output_frames = 0;
  size_t input_frames;
  bool rv;

  XASSERT(has_input(stm) && has_output(stm));

  if (stm->input_stream_params.prefs & CUBEB_STREAM_PREF_LOOPBACK) {
    HRESULT rv = get_input_buffer(stm);
    if (FAILED(rv)) {
      return rv;
    }
  }

  input_frames =
      stm->linear_input_buffer->length() / stm->input_stream_params.channels;

  rv = get_output_buffer(stm, output_buffer, output_frames);
  if (!rv) {
    hr = stm->render_client->ReleaseBuffer(output_frames, 0);
    return rv;
  }

  /* This can only happen when debugging, and having breakpoints set in the
   * callback in a way that it makes the stream underrun. */
  if (output_frames == 0) {
    return true;
  }

  /* Wait for draining is not important on duplex. */
  if (stm->draining) {
    return false;
  }

  stm->total_output_frames += output_frames;

  ALOGV("in: %zu, out: %zu, missing: %ld, ratio: %f", stm->total_input_frames,
        stm->total_output_frames,
        static_cast<long>(stm->total_output_frames) - stm->total_input_frames,
        static_cast<float>(stm->total_output_frames) / stm->total_input_frames);

  long got;
  if (stm->has_dummy_output) {
    ALOGV(
        "Duplex callback (dummy output): input frames: %Iu, output frames: %Iu",
        input_frames, output_frames);

    // We don't want to expose the dummy output to the callback so don't pass
    // the output buffer (it will be released later with silence in it)
    got =
        refill(stm, stm->linear_input_buffer->data(), input_frames, nullptr, 0);

  } else {
    ALOGV("Duplex callback: input frames: %Iu, output frames: %Iu",
          input_frames, output_frames);

    got = refill(stm, stm->linear_input_buffer->data(), input_frames,
                 output_buffer, output_frames);
  }

  stm->linear_input_buffer->clear();

  if (stm->has_dummy_output) {
    // If output is a dummy output, make sure it's silent
    hr = stm->render_client->ReleaseBuffer(output_frames,
                                           AUDCLNT_BUFFERFLAGS_SILENT);
  } else {
    hr = stm->render_client->ReleaseBuffer(output_frames, 0);
  }
  if (FAILED(hr)) {
    LOG("failed to release buffer: %lx", hr);
    return false;
  }
  if (got < 0) {
    return false;
  }
  return true;
}

bool
refill_callback_input(cubeb_stream * stm)
{
  bool rv;
  size_t input_frames;

  XASSERT(has_input(stm) && !has_output(stm));

  rv = get_input_buffer(stm);
  if (!rv) {
    return rv;
  }

  input_frames =
      stm->linear_input_buffer->length() / stm->input_stream_params.channels;
  if (!input_frames) {
    return true;
  }

  ALOGV("Input callback: input frames: %Iu", input_frames);

  long read =
      refill(stm, stm->linear_input_buffer->data(), input_frames, nullptr, 0);
  if (read < 0) {
    return false;
  }

  stm->linear_input_buffer->clear();

  return !stm->draining;
}

bool
refill_callback_output(cubeb_stream * stm)
{
  bool rv;
  HRESULT hr;
  void * output_buffer = nullptr;
  size_t output_frames = 0;

  XASSERT(!has_input(stm) && has_output(stm));

  rv = get_output_buffer(stm, output_buffer, output_frames);
  if (!rv) {
    return rv;
  }

  if (stm->draining || output_frames == 0) {
    return true;
  }

  long got = refill(stm, nullptr, 0, output_buffer, output_frames);

  ALOGV("Output callback: output frames requested: %Iu, got %ld", output_frames,
        got);
  if (got < 0) {
    return false;
  }
  XASSERT(size_t(got) == output_frames || stm->draining);

  hr = stm->render_client->ReleaseBuffer(got, 0);
  if (FAILED(hr)) {
    LOG("failed to release buffer: %lx", hr);
    return false;
  }

  return size_t(got) == output_frames || stm->draining;
}

void
wasapi_stream_destroy(cubeb_stream * stm);

static unsigned int __stdcall wasapi_stream_render_loop(LPVOID stream)
{
  AutoRegisterThread raii("cubeb rendering thread");
  cubeb_stream * stm = static_cast<cubeb_stream *>(stream);

  auto_stream_ref stream_ref(stm);
  struct auto_com {
    auto_com()
    {
      HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      XASSERT(SUCCEEDED(hr));
    }
    ~auto_com() { CoUninitialize(); }
  } com;

  bool is_playing = true;
  HANDLE wait_array[4] = {stm->shutdown_event, stm->reconfigure_event,
                          stm->refill_event, stm->input_available_event};
  HANDLE mmcss_handle = NULL;
  HRESULT hr = 0;
  DWORD mmcss_task_index = 0;

  // Signal wasapi_stream_start that we've initialized COM and incremented
  // the stream's ref_count.
  BOOL ok = SetEvent(stm->thread_ready_event);
  if (!ok) {
    LOG("thread_ready SetEvent failed: %lx", GetLastError());
    return 0;
  }

  /* We could consider using "Pro Audio" here for WebAudio and
     maybe WebRTC. */
  mmcss_handle = AvSetMmThreadCharacteristicsA("Audio", &mmcss_task_index);
  if (!mmcss_handle) {
    /* This is not fatal, but we might glitch under heavy load. */
    LOG("Unable to use mmcss to bump the render thread priority: %lx",
        GetLastError());
  }

  while (is_playing) {
    DWORD waitResult = WaitForMultipleObjects(ARRAY_LENGTH(wait_array),
                                              wait_array, FALSE, INFINITE);
    switch (waitResult) {
    case WAIT_OBJECT_0: { /* shutdown */
      is_playing = false;
      /* We don't check if the drain is actually finished here, we just want to
         shutdown. */
      if (stm->draining) {
        wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      }
      continue;
    }
    case WAIT_OBJECT_0 + 1: { /* reconfigure */
      auto_lock lock(stm->stream_reset_lock);
      if (!stm->active) {
        /* Avoid reconfiguring, stream start will handle it. */
        LOG("Stream is not active, ignoring reconfigure.");
        continue;
      }
      XASSERT(stm->output_client || stm->input_client);
      LOG("Reconfiguring the stream");
      /* Close the stream */
      bool was_running = false;
      if (stm->output_client) {
        was_running = stm->output_client->Stop() == S_OK;
        LOG("Output stopped.");
      }
      if (stm->input_client) {
        was_running = stm->input_client->Stop() == S_OK;
        LOG("Input stopped.");
      }
      close_wasapi_stream(stm);
      LOG("Stream closed.");
      /* Reopen a stream and start it immediately. This will automatically
          pick the new default device for this role. */
      int r = setup_wasapi_stream(stm);
      if (r != CUBEB_OK) {
        LOG("Error setting up the stream during reconfigure.");
        /* Don't destroy the stream here, since we expect the caller to do
            so after the error has propagated via the state callback. */
        is_playing = false;
        hr = E_FAIL;
        continue;
      }
      LOG("Stream setup successfuly.");
      XASSERT(stm->output_client || stm->input_client);
      if (was_running && stm->output_client) {
        hr = stm->output_client->Start();
        if (FAILED(hr)) {
          LOG("Error starting output after reconfigure, error: %lx", hr);
          is_playing = false;
          continue;
        }
        LOG("Output started after reconfigure.");
      }
      if (was_running && stm->input_client) {
        hr = stm->input_client->Start();
        if (FAILED(hr)) {
          LOG("Error starting input after reconfiguring, error: %lx", hr);
          is_playing = false;
          continue;
        }
        LOG("Input started after reconfigure.");
      }
      break;
    }
    case WAIT_OBJECT_0 + 2: /* refill */
      XASSERT((has_input(stm) && has_output(stm)) ||
              (!has_input(stm) && has_output(stm)));
      is_playing = stm->refill_callback(stm);
      break;
    case WAIT_OBJECT_0 + 3: { /* input available */
      HRESULT rv = get_input_buffer(stm);
      if (FAILED(rv)) {
        is_playing = false;
        continue;
      }

      if (!has_output(stm)) {
        is_playing = stm->refill_callback(stm);
      }

      break;
    }
    default:
      LOG("case %lu not handled in render loop.", waitResult);
      XASSERT(false);
    }
  }

  // Stop audio clients since this thread will no longer service
  // the events.
  if (stm->output_client) {
    stm->output_client->Stop();
  }
  if (stm->input_client) {
    stm->input_client->Stop();
  }

  if (mmcss_handle) {
    AvRevertMmThreadCharacteristics(mmcss_handle);
  }

  if (FAILED(hr)) {
    wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  }

  return 0;
}

void
wasapi_destroy(cubeb * context);

HRESULT
register_notification_client(cubeb_stream * stm)
{
  XASSERT(stm->device_enumerator && !stm->notification_client);

  stm->notification_client.reset(new wasapi_endpoint_notification_client(
      stm->reconfigure_event, stm->role));

  HRESULT hr = stm->device_enumerator->RegisterEndpointNotificationCallback(
      stm->notification_client.get());
  if (FAILED(hr)) {
    LOG("Could not register endpoint notification callback: %lx", hr);
    stm->notification_client = nullptr;
  }

  return hr;
}

HRESULT
unregister_notification_client(cubeb_stream * stm)
{
  XASSERT(stm->device_enumerator && stm->notification_client);

  HRESULT hr = stm->device_enumerator->UnregisterEndpointNotificationCallback(
      stm->notification_client.get());
  if (FAILED(hr)) {
    // We can't really do anything here, we'll probably leak the
    // notification client.
    return S_OK;
  }

  stm->notification_client = nullptr;

  return S_OK;
}

HRESULT
get_endpoint(com_ptr<IMMDevice> & device, LPCWSTR devid)
{
  com_ptr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }

  hr = enumerator->GetDevice(devid, device.receive());
  if (FAILED(hr)) {
    LOG("Could not get device: %lx", hr);
    return hr;
  }

  return S_OK;
}

HRESULT
register_collection_notification_client(cubeb * context)
{
  context->lock.assert_current_thread_owns();
  XASSERT(!context->device_collection_enumerator &&
          !context->collection_notification_client);
  HRESULT hr = CoCreateInstance(
      __uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
      IID_PPV_ARGS(context->device_collection_enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }

  context->collection_notification_client.reset(
      new wasapi_collection_notification_client(context));

  hr = context->device_collection_enumerator
           ->RegisterEndpointNotificationCallback(
               context->collection_notification_client.get());
  if (FAILED(hr)) {
    LOG("Could not register endpoint notification callback: %lx", hr);
    context->collection_notification_client.reset();
    context->device_collection_enumerator.reset();
  }

  return hr;
}

HRESULT
unregister_collection_notification_client(cubeb * context)
{
  context->lock.assert_current_thread_owns();
  XASSERT(context->device_collection_enumerator &&
          context->collection_notification_client);
  HRESULT hr = context->device_collection_enumerator
                   ->UnregisterEndpointNotificationCallback(
                       context->collection_notification_client.get());
  if (FAILED(hr)) {
    return hr;
  }

  context->collection_notification_client = nullptr;
  context->device_collection_enumerator = nullptr;

  return hr;
}

HRESULT
get_default_endpoint(com_ptr<IMMDevice> & device, EDataFlow direction,
                     ERole role)
{
  com_ptr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }
  hr = enumerator->GetDefaultAudioEndpoint(direction, role, device.receive());
  if (FAILED(hr)) {
    LOG("Could not get default audio endpoint: %lx", hr);
    return hr;
  }

  return ERROR_SUCCESS;
}

double
current_stream_delay(cubeb_stream * stm)
{
  stm->stream_reset_lock.assert_current_thread_owns();

  /* If the default audio endpoint went away during playback and we weren't
     able to configure a new one, it's possible the caller may call this
     before the error callback has propogated back. */
  if (!stm->audio_clock) {
    return 0;
  }

  UINT64 freq;
  HRESULT hr = stm->audio_clock->GetFrequency(&freq);
  if (FAILED(hr)) {
    LOG("GetFrequency failed: %lx", hr);
    return 0;
  }

  UINT64 pos;
  hr = stm->audio_clock->GetPosition(&pos, NULL);
  if (FAILED(hr)) {
    LOG("GetPosition failed: %lx", hr);
    return 0;
  }

  double cur_pos = static_cast<double>(pos) / freq;
  double max_pos =
      static_cast<double>(stm->frames_written) / stm->output_mix_params.rate;
  double delay = std::max(max_pos - cur_pos, 0.0);

  return delay;
}

#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
int
stream_set_volume(cubeb_stream * stm, float volume)
{
  stm->stream_reset_lock.assert_current_thread_owns();

  if (!stm->audio_stream_volume) {
    return CUBEB_ERROR;
  }

  uint32_t channels;
  HRESULT hr = stm->audio_stream_volume->GetChannelCount(&channels);
  if (FAILED(hr)) {
    LOG("could not get the channel count: %lx", hr);
    return CUBEB_ERROR;
  }

  /* up to 9.1 for now */
  if (channels > 10) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  float volumes[10];
  for (uint32_t i = 0; i < channels; i++) {
    volumes[i] = volume;
  }

  hr = stm->audio_stream_volume->SetAllVolumes(channels, volumes);
  if (FAILED(hr)) {
    LOG("could not set the channels volume: %lx", hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}
#endif
} // namespace

extern "C" {
int
wasapi_init(cubeb ** context, char const * context_name)
{
  /* We don't use the device yet, but need to make sure we can initialize one
     so that this backend is not incorrectly enabled on platforms that don't
     support WASAPI. */
  com_ptr<IMMDevice> device;
  HRESULT hr = get_default_endpoint(device, eRender, eConsole);
  if (FAILED(hr)) {
    XASSERT(hr != CO_E_NOTINITIALIZED);
    LOG("It wasn't able to find a default rendering device: %lx", hr);
    hr = get_default_endpoint(device, eCapture, eConsole);
    if (FAILED(hr)) {
      LOG("It wasn't able to find a default capture device: %lx", hr);
      return CUBEB_ERROR;
    }
  }

  cubeb * ctx = new cubeb();

  ctx->ops = &wasapi_ops;
  auto_lock lock(ctx->lock);
  if (cubeb_strings_init(&ctx->device_ids) != CUBEB_OK) {
    delete ctx;
    return CUBEB_ERROR;
  }

  LARGE_INTEGER frequency;
  if (QueryPerformanceFrequency(&frequency)) {
    ctx->performance_counter_frequency = frequency.QuadPart;
  } else {
    LOG("Failed getting performance counter frequency, latency reporting will "
        "be inacurate");
    ctx->performance_counter_frequency = 0;
  }

  *context = ctx;

  return CUBEB_OK;
}
}

namespace {
enum ShutdownPhase { OnStop, OnDestroy };

bool
stop_and_join_render_thread(cubeb_stream * stm)
{
  LOG("%p: Stop and join render thread: %p", stm, stm->thread);
  if (!stm->thread) {
    return true;
  }

  BOOL ok = SetEvent(stm->shutdown_event);
  if (!ok) {
    LOG("stop_and_join_render_thread: SetEvent failed: %lx", GetLastError());
    return false;
  }

  /* Wait five seconds for the rendering thread to return. It's supposed to
   * check its event loop very often, five seconds is rather conservative.
   * Note: 5*1s loop to work around timer sleep issues on pre-Windows 8. */
  DWORD r;
  for (int i = 0; i < 5; ++i) {
    r = WaitForSingleObject(stm->thread, 1000);
    if (r == WAIT_OBJECT_0) {
      break;
    }
  }
  if (r != WAIT_OBJECT_0) {
    LOG("stop_and_join_render_thread: WaitForSingleObject on thread failed: "
        "%lx, %lx",
        r, GetLastError());
    return false;
  }

  return true;
}

void
wasapi_destroy(cubeb * context)
{
  {
    auto_lock lock(context->lock);
    XASSERT(!context->device_collection_enumerator &&
            !context->collection_notification_client);

    if (context->device_ids) {
      cubeb_strings_destroy(context->device_ids);
    }
  }

  delete context;
}

char const *
wasapi_get_backend_id(cubeb * context)
{
  return "wasapi";
}

int
wasapi_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  XASSERT(ctx && max_channels);

  com_ptr<IMMDevice> device;
  HRESULT hr = get_default_endpoint(device, eRender, eConsole);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
                        client.receive_vpp());
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  WAVEFORMATEX * tmp = nullptr;
  hr = client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  *max_channels = mix_format->nChannels;

  return CUBEB_OK;
}

int
wasapi_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                       uint32_t * latency_frames)
{
  if (params.format != CUBEB_SAMPLE_FLOAT32NE &&
      params.format != CUBEB_SAMPLE_S16NE) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  ERole role = pref_to_role(params.prefs);

  com_ptr<IMMDevice> device;
  HRESULT hr = get_default_endpoint(device, eRender, role);
  if (FAILED(hr)) {
    LOG("Could not get default endpoint: %lx", hr);
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
                        client.receive_vpp());
  if (FAILED(hr)) {
    LOG("Could not activate device for latency: %lx", hr);
    return CUBEB_ERROR;
  }

  REFERENCE_TIME minimum_period;
  REFERENCE_TIME default_period;
  hr = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(hr)) {
    LOG("Could not get device period: %lx", hr);
    return CUBEB_ERROR;
  }

  LOG("default device period: %I64d, minimum device period: %I64d",
      default_period, minimum_period);

  /* If we're on Windows 10, we can use IAudioClient3 to get minimal latency.
     Otherwise, according to the docs, the best latency we can achieve is by
     synchronizing the stream and the engine.
     http://msdn.microsoft.com/en-us/library/windows/desktop/dd370871%28v=vs.85%29.aspx
   */

  // #ifdef _WIN32_WINNT_WIN10
#if 0
     *latency_frames = hns_to_frames(params.rate, minimum_period);
#else
  *latency_frames = hns_to_frames(params.rate, default_period);
#endif

  LOG("Minimum latency in frames: %u", *latency_frames);

  return CUBEB_OK;
}

int
wasapi_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  com_ptr<IMMDevice> device;
  HRESULT hr = get_default_endpoint(device, eRender, eConsole);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  com_ptr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
                        client.receive_vpp());
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }

  WAVEFORMATEX * tmp = nullptr;
  hr = client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  *rate = mix_format->nSamplesPerSec;

  LOG("Preferred sample rate for output: %u", *rate);

  return CUBEB_OK;
}

static void
waveformatex_update_derived_properties(WAVEFORMATEX * format)
{
  format->nBlockAlign = format->wBitsPerSample * format->nChannels / 8;
  format->nAvgBytesPerSec = format->nSamplesPerSec * format->nBlockAlign;
  if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE * format_pcm =
        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(format);
    format_pcm->Samples.wValidBitsPerSample = format->wBitsPerSample;
  }
}

/* Based on the mix format and the stream format, try to find a way to play
   what the user requested. */
static void
handle_channel_layout(cubeb_stream * stm, EDataFlow direction,
                      com_heap_ptr<WAVEFORMATEX> & mix_format,
                      const cubeb_stream_params * stream_params)
{
  com_ptr<IAudioClient> & audio_client =
      (direction == eRender) ? stm->output_client : stm->input_client;
  XASSERT(audio_client);
  /* The docs say that GetMixFormat is always of type WAVEFORMATEXTENSIBLE [1],
     so the reinterpret_cast below should be safe. In practice, this is not
     true, and we just want to bail out and let the rest of the code find a good
     conversion path instead of trying to make WASAPI do it by itself.
     [1]:
     http://msdn.microsoft.com/en-us/library/windows/desktop/dd370811%28v=vs.85%29.aspx*/
  if (mix_format->wFormatTag != WAVE_FORMAT_EXTENSIBLE) {
    return;
  }

  WAVEFORMATEXTENSIBLE * format_pcm =
      reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get());

  /* Stash a copy of the original mix format in case we need to restore it
   * later. */
  WAVEFORMATEXTENSIBLE hw_mix_format = *format_pcm;

  /* Get the channel mask by the channel layout.
     If the layout is not supported, we will get a closest settings below. */
  format_pcm->dwChannelMask = stream_params->layout;
  mix_format->nChannels = stream_params->channels;
  waveformatex_update_derived_properties(mix_format.get());

  /* Check if wasapi will accept our channel layout request. */
  WAVEFORMATEX * tmp = nullptr;
  HRESULT hr = audio_client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                               mix_format.get(), &tmp);
  com_heap_ptr<WAVEFORMATEX> closest(tmp);
  if (hr == S_FALSE) {
    /* Channel layout not supported, but WASAPI gives us a suggestion. Use it,
       and handle the eventual upmix/downmix ourselves. Ignore the subformat of
       the suggestion, since it seems to always be IEEE_FLOAT. */
    LOG("Using WASAPI suggested format: channels: %d", closest->nChannels);
    XASSERT(closest->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
    WAVEFORMATEXTENSIBLE * closest_pcm =
        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(closest.get());
    format_pcm->dwChannelMask = closest_pcm->dwChannelMask;
    mix_format->nChannels = closest->nChannels;
    waveformatex_update_derived_properties(mix_format.get());
  } else if (hr == AUDCLNT_E_UNSUPPORTED_FORMAT) {
    /* Not supported, no suggestion. This should not happen, but it does in the
       field with some sound cards. We restore the mix format, and let the rest
       of the code figure out the right conversion path. */
    XASSERT(mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE);
    *reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get()) = hw_mix_format;
  } else if (hr == S_OK) {
    LOG("Requested format accepted by WASAPI.");
  } else {
    LOG("IsFormatSupported unhandled error: %lx", hr);
  }
}

static int
initialize_iaudioclient2(com_ptr<IAudioClient> & audio_client)
{
  com_ptr<IAudioClient2> audio_client2;
  audio_client->QueryInterface<IAudioClient2>(audio_client2.receive());
  if (!audio_client2) {
    LOG("Could not get IAudioClient2 interface, not setting "
        "AUDCLNT_STREAMOPTIONS_RAW.");
    return CUBEB_OK;
  }
  AudioClientProperties properties = {0};
  properties.cbSize = sizeof(AudioClientProperties);
#ifndef __MINGW32__
  properties.Options |= AUDCLNT_STREAMOPTIONS_RAW;
#endif
  HRESULT hr = audio_client2->SetClientProperties(&properties);
  if (FAILED(hr)) {
    LOG("IAudioClient2::SetClientProperties error: %lx", GetLastError());
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

#if 0
bool
initialize_iaudioclient3(com_ptr<IAudioClient> & audio_client,
                         cubeb_stream * stm,
                         const com_heap_ptr<WAVEFORMATEX> & mix_format,
                         DWORD flags, EDataFlow direction)
{
  com_ptr<IAudioClient3> audio_client3;
  audio_client->QueryInterface<IAudioClient3>(audio_client3.receive());
  if (!audio_client3) {
    LOG("Could not get IAudioClient3 interface");
    return false;
  }

  if (flags & AUDCLNT_STREAMFLAGS_LOOPBACK) {
    // IAudioClient3 doesn't work with loopback streams, and will return error
    // 88890021: AUDCLNT_E_INVALID_STREAM_FLAG
    LOG("Audio stream is loopback, not using IAudioClient3");
    return false;
  }

  // Some people have reported glitches with capture streams:
  // http://blog.nirbheek.in/2018/03/low-latency-audio-on-windows-with.html
  if (direction == eCapture) {
    LOG("Audio stream is capture, not using IAudioClient3");
    return false;
  }

  // Possibly initialize a shared-mode stream using IAudioClient3. Initializing
  // a stream this way lets you request lower latencies, but also locks the
  // global WASAPI engine at that latency.
  // - If we request a shared-mode stream, streams created with IAudioClient
  // will
  //   have their latency adjusted to match. When  the shared-mode stream is
  //   closed, they'll go back to normal.
  // - If there's already a shared-mode stream running, then we cannot request
  //   the engine change to a different latency - we have to match it.
  // - It's antisocial to lock the WASAPI engine at its default latency. If we
  //   would do this, then stop and use IAudioClient instead.

  HRESULT hr;
  uint32_t default_period = 0, fundamental_period = 0, min_period = 0,
           max_period = 0;
  hr = audio_client3->GetSharedModeEnginePeriod(
      mix_format.get(), &default_period, &fundamental_period, &min_period,
      &max_period);
  if (FAILED(hr)) {
    LOG("Could not get shared mode engine period: error: %lx", hr);
    return false;
  }
  uint32_t requested_latency = stm->latency;
  if (requested_latency >= default_period) {
    LOG("Requested latency %i greater than default latency %i, not using "
        "IAudioClient3",
        requested_latency, default_period);
    return false;
  }
  LOG("Got shared mode engine period: default=%i fundamental=%i min=%i max=%i",
      default_period, fundamental_period, min_period, max_period);
  // Snap requested latency to a valid value
  uint32_t old_requested_latency = requested_latency;
  if (requested_latency < min_period) {
    requested_latency = min_period;
  }
  requested_latency -= (requested_latency - min_period) % fundamental_period;
  if (requested_latency != old_requested_latency) {
    LOG("Requested latency %i was adjusted to %i", old_requested_latency,
        requested_latency);
  }

  hr = audio_client3->InitializeSharedAudioStream(flags, requested_latency,
                                                  mix_format.get(), NULL);
  if (SUCCEEDED(hr)) {
    return true;
  } else if (hr == AUDCLNT_E_ENGINE_PERIODICITY_LOCKED) {
    LOG("Got AUDCLNT_E_ENGINE_PERIODICITY_LOCKED, adjusting latency request");
  } else {
    LOG("Could not initialize shared stream with IAudioClient3: error: %lx",
        hr);
    return false;
  }

  uint32_t current_period = 0;
  WAVEFORMATEX * current_format = nullptr;
  // We have to pass a valid WAVEFORMATEX** and not nullptr, otherwise
  // GetCurrentSharedModeEnginePeriod will return E_POINTER
  hr = audio_client3->GetCurrentSharedModeEnginePeriod(&current_format,
                                                       &current_period);
  CoTaskMemFree(current_format);
  if (FAILED(hr)) {
    LOG("Could not get current shared mode engine period: error: %lx", hr);
    return false;
  }

  if (current_period >= default_period) {
    LOG("Current shared mode engine period %i too high, not using IAudioClient",
        current_period);
    return false;
  }

  hr = audio_client3->InitializeSharedAudioStream(flags, current_period,
                                                  mix_format.get(), NULL);
  if (SUCCEEDED(hr)) {
    LOG("Current shared mode engine period is %i instead of requested %i",
        current_period, requested_latency);
    return true;
  }

  LOG("Could not initialize shared stream with IAudioClient3: error: %lx", hr);
  return false;
}
#endif

#define DIRECTION_NAME (direction == eCapture ? "capture" : "render")

template <typename T>
int
setup_wasapi_stream_one_side(cubeb_stream * stm,
                             cubeb_stream_params * stream_params,
                             wchar_t const * devid, EDataFlow direction,
                             REFIID riid, com_ptr<IAudioClient> & audio_client,
                             uint32_t * buffer_frame_count, HANDLE & event,
                             T & render_or_capture_client,
                             cubeb_stream_params * mix_params,
                             com_ptr<IMMDevice> & device)
{
  XASSERT(direction == eCapture || direction == eRender);

  HRESULT hr;
  bool is_loopback = stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK;
  if (is_loopback && direction != eCapture) {
    LOG("Loopback pref can only be used with capture streams!\n");
    return CUBEB_ERROR;
  }

  stm->stream_reset_lock.assert_current_thread_owns();
  // If user doesn't specify a particular device, we can choose another one when
  // the given devid is unavailable.
  bool allow_fallback =
      direction == eCapture ? !stm->input_device_id : !stm->output_device_id;
  bool try_again = false;
  // This loops until we find a device that works, or we've exhausted all
  // possibilities.
  do {
    if (devid) {
      hr = get_endpoint(device, devid);
      if (FAILED(hr)) {
        LOG("Could not get %s endpoint, error: %lx\n", DIRECTION_NAME, hr);
        return CUBEB_ERROR;
      }
    } else {
      // If caller has requested loopback but not specified a device, look for
      // the default render device. Otherwise look for the default device
      // appropriate to the direction.
      hr = get_default_endpoint(device, is_loopback ? eRender : direction,
                                pref_to_role(stream_params->prefs));
      if (FAILED(hr)) {
        if (is_loopback) {
          LOG("Could not get default render endpoint for loopback, error: "
              "%lx\n",
              hr);
        } else {
          LOG("Could not get default %s endpoint, error: %lx\n", DIRECTION_NAME,
              hr);
        }
        return CUBEB_ERROR;
      }
    }

    /* Get a client. We will get all other interfaces we need from
     * this pointer. */
#if 0 // See https://bugzilla.mozilla.org/show_bug.cgi?id=1590902
    hr = device->Activate(__uuidof(IAudioClient3),
                          CLSCTX_INPROC_SERVER,
                          NULL, audio_client.receive_vpp());
    if (hr == E_NOINTERFACE) {
#endif
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,
                          audio_client.receive_vpp());
#if 0
    }
#endif

    if (FAILED(hr)) {
      LOG("Could not activate the device to get an audio"
          " client for %s: error: %lx\n",
          DIRECTION_NAME, hr);
      // A particular device can't be activated because it has been
      // unplugged, try fall back to the default audio device.
      if (devid && hr == AUDCLNT_E_DEVICE_INVALIDATED && allow_fallback) {
        LOG("Trying again with the default %s audio device.", DIRECTION_NAME);
        devid = nullptr;
        device = nullptr;
        try_again = true;
      } else {
        return CUBEB_ERROR;
      }
    } else {
      try_again = false;
    }
  } while (try_again);

  /* We have to distinguish between the format the mixer uses,
   * and the format the stream we want to play uses. */
  WAVEFORMATEX * tmp = nullptr;
  hr = audio_client->GetMixFormat(&tmp);
  if (FAILED(hr)) {
    LOG("Could not fetch current mix format from the audio"
        " client for %s: error: %lx",
        DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }
  com_heap_ptr<WAVEFORMATEX> mix_format(tmp);

  mix_format->wBitsPerSample = stm->bytes_per_sample * 8;
  if (mix_format->wFormatTag == WAVE_FORMAT_PCM ||
      mix_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
    switch (mix_format->wBitsPerSample) {
    case 8:
    case 16:
      mix_format->wFormatTag = WAVE_FORMAT_PCM;
      break;
    case 32:
      mix_format->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
      break;
    default:
      LOG("%u bits per sample is incompatible with PCM wave formats",
          mix_format->wBitsPerSample);
      return CUBEB_ERROR;
    }
  }

  if (mix_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    WAVEFORMATEXTENSIBLE * format_pcm =
        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix_format.get());
    format_pcm->SubFormat = stm->waveformatextensible_sub_format;
  }
  waveformatex_update_derived_properties(mix_format.get());

  /* Set channel layout only when there're more than two channels. Otherwise,
   * use the default setting retrieved from the stream format of the audio
   * engine's internal processing by GetMixFormat. */
  if (mix_format->nChannels > 2) {
    handle_channel_layout(stm, direction, mix_format, stream_params);
  }

  mix_params->format = stream_params->format;
  mix_params->rate = mix_format->nSamplesPerSec;
  mix_params->channels = mix_format->nChannels;
  mix_params->layout = mask_to_channel_layout(mix_format.get());

  LOG("Setup requested=[f=%d r=%u c=%u l=%u] mix=[f=%d r=%u c=%u l=%u]",
      stream_params->format, stream_params->rate, stream_params->channels,
      stream_params->layout, mix_params->format, mix_params->rate,
      mix_params->channels, mix_params->layout);

  DWORD flags = 0;

  // Check if a loopback device should be requested. Note that event callbacks
  // do not work with loopback devices, so only request these if not looping.
  if (is_loopback) {
    flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
  } else {
    flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  }

  REFERENCE_TIME latency_hns = frames_to_hns(stream_params->rate, stm->latency);

  // Adjust input latency and check if input is using bluetooth handsfree
  // protocol.
  if (direction == eCapture) {
    stm->input_bluetooth_handsfree = false;

    wasapi_default_devices default_devices(stm->device_enumerator.get());
    cubeb_device_info device_info;
    if (wasapi_create_device(stm->context, device_info,
                             stm->device_enumerator.get(), device.get(),
                             &default_devices) == CUBEB_OK) {
      if (device_info.latency_hi == 0) {
        LOG("Input: could not query latency_hi to guess safe latency");
        wasapi_destroy_device(&device_info);
        return CUBEB_ERROR;
      }
      // This multiplicator has been found empirically.
      uint32_t latency_frames = device_info.latency_hi * 8;
      LOG("Input: latency increased to %u frames from a default of %u",
          latency_frames, device_info.latency_hi);
      latency_hns = frames_to_hns(device_info.default_rate, latency_frames);

      const char * HANDSFREE_TAG = "BTHHFENUM";
      size_t len = sizeof(HANDSFREE_TAG);
      if (strlen(device_info.group_id) >= len &&
          strncmp(device_info.group_id, HANDSFREE_TAG, len) == 0) {
        LOG("Input device is using bluetooth handsfree protocol");
        stm->input_bluetooth_handsfree = true;
      }

      wasapi_destroy_device(&device_info);
    } else {
      LOG("Could not get cubeb_device_info. Skip customizing input settings");
    }
  }

  if (stream_params->prefs & CUBEB_STREAM_PREF_RAW) {
    if (initialize_iaudioclient2(audio_client) != CUBEB_OK) {
      LOG("Can't initialize an IAudioClient2, error: %lx", GetLastError());
      // This is not fatal.
    }
  }

#if 0 // See https://bugzilla.mozilla.org/show_bug.cgi?id=1590902
  if (initialize_iaudioclient3(audio_client, stm, mix_format, flags, direction)) {
    LOG("Initialized with IAudioClient3");
  } else {
#endif
  hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, latency_hns, 0,
                                mix_format.get(), NULL);
#if 0
  }
#endif
  if (FAILED(hr)) {
    LOG("Unable to initialize audio client for %s: %lx.", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  hr = audio_client->GetBufferSize(buffer_frame_count);
  if (FAILED(hr)) {
    LOG("Could not get the buffer size from the client"
        " for %s %lx.",
        DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  LOG("Buffer size is: %d for %s\n", *buffer_frame_count, DIRECTION_NAME);

  // Events are used if not looping back
  if (!is_loopback) {
    hr = audio_client->SetEventHandle(event);
    if (FAILED(hr)) {
      LOG("Could set the event handle for the %s client %lx.", DIRECTION_NAME,
          hr);
      return CUBEB_ERROR;
    }
  }

  hr = audio_client->GetService(riid, render_or_capture_client.receive_vpp());
  if (FAILED(hr)) {
    LOG("Could not get the %s client %lx.", DIRECTION_NAME, hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

#undef DIRECTION_NAME

// Returns a non-null cubeb_devid if we find a matched device, or nullptr
// otherwise.
cubeb_devid
wasapi_find_bt_handsfree_output_device(cubeb_stream * stm)
{
  HRESULT hr;
  cubeb_device_info * input_device = nullptr;
  cubeb_device_collection collection;

  // Only try to match to an output device if the input device is a bluetooth
  // device that is using the handsfree protocol
  if (!stm->input_bluetooth_handsfree) {
    return nullptr;
  }

  wchar_t * tmp = nullptr;
  hr = stm->input_device->GetId(&tmp);
  if (FAILED(hr)) {
    LOG("Couldn't get input device id in "
        "wasapi_find_bt_handsfree_output_device");
    return nullptr;
  }
  com_heap_ptr<wchar_t> device_id(tmp);
  cubeb_devid input_device_id = reinterpret_cast<cubeb_devid>(
      intern_device_id(stm->context, device_id.get()));
  if (!input_device_id) {
    return nullptr;
  }

  int rv = wasapi_enumerate_devices_internal(
      stm->context,
      (cubeb_device_type)(CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT),
      &collection, DEVICE_STATE_ACTIVE);
  if (rv != CUBEB_OK) {
    return nullptr;
  }

  // Find the input device, and then find the output device with the same group
  // id and the same rate.
  for (uint32_t i = 0; i < collection.count; i++) {
    if (collection.device[i].devid == input_device_id) {
      input_device = &collection.device[i];
      break;
    }
  }

  cubeb_devid matched_output = nullptr;

  if (input_device) {
    for (uint32_t i = 0; i < collection.count; i++) {
      cubeb_device_info & dev = collection.device[i];
      if (dev.type == CUBEB_DEVICE_TYPE_OUTPUT && dev.group_id &&
          !strcmp(dev.group_id, input_device->group_id) &&
          dev.default_rate == input_device->default_rate) {
        LOG("Found matching device for %s: %s", input_device->friendly_name,
            dev.friendly_name);
        matched_output = dev.devid;
        break;
      }
    }
  }

  wasapi_device_collection_destroy(stm->context, &collection);
  return matched_output;
}

std::unique_ptr<wchar_t[]>
copy_wide_string(const wchar_t * src)
{
  XASSERT(src);
  size_t len = wcslen(src);
  std::unique_ptr<wchar_t[]> copy(new wchar_t[len + 1]);
  if (wcsncpy_s(copy.get(), len + 1, src, len) != 0) {
    return nullptr;
  }
  return copy;
}

int
setup_wasapi_stream(cubeb_stream * stm)
{
  int rv;

  stm->stream_reset_lock.assert_current_thread_owns();

  XASSERT((!stm->output_client || !stm->input_client) &&
          "WASAPI stream already setup, close it first.");

  std::unique_ptr<const wchar_t[]> selected_output_device_id;
  if (stm->output_device_id) {
    if (std::unique_ptr<wchar_t[]> tmp =
            copy_wide_string(stm->output_device_id.get())) {
      selected_output_device_id = std::move(tmp);
    } else {
      LOG("Failed to copy output device identifier.");
      return CUBEB_ERROR;
    }
  }

  if (has_input(stm)) {
    LOG("(%p) Setup capture: device=%p", stm, stm->input_device_id.get());
    rv = setup_wasapi_stream_one_side(
        stm, &stm->input_stream_params, stm->input_device_id.get(), eCapture,
        __uuidof(IAudioCaptureClient), stm->input_client,
        &stm->input_buffer_frame_count, stm->input_available_event,
        stm->capture_client, &stm->input_mix_params, stm->input_device);
    if (rv != CUBEB_OK) {
      LOG("Failure to open the input side.");
      return rv;
    }

    // We initializing an input stream, buffer ahead two buffers worth of
    // silence. This delays the input side slightly, but allow to not glitch
    // when no input is available when calling into the resampler to call the
    // callback: the input refill event will be set shortly after to compensate
    // for this lack of data. In debug, four buffers are used, to avoid tripping
    // up assertions down the line.
#if !defined(DEBUG)
    const int silent_buffer_count = 2;
#else
    const int silent_buffer_count = 6;
#endif
    stm->linear_input_buffer->push_silence(stm->input_buffer_frame_count *
                                           stm->input_stream_params.channels *
                                           silent_buffer_count);

    // If this is a bluetooth device, and the output device is the default
    // device, and the default device is the same bluetooth device, pick the
    // right output device, running at the same rate and with the same protocol
    // as the input.
    if (!selected_output_device_id) {
      cubeb_devid matched = wasapi_find_bt_handsfree_output_device(stm);
      if (matched) {
        selected_output_device_id =
            utf8_to_wstr(reinterpret_cast<char const *>(matched));
      }
    }
  }

  // If we don't have an output device but are requesting a loopback device,
  // we attempt to open that same device in output mode in order to drive the
  // loopback via the output events.
  stm->has_dummy_output = false;
  if (!has_output(stm) &&
      stm->input_stream_params.prefs & CUBEB_STREAM_PREF_LOOPBACK) {
    stm->output_stream_params.rate = stm->input_stream_params.rate;
    stm->output_stream_params.channels = stm->input_stream_params.channels;
    stm->output_stream_params.layout = stm->input_stream_params.layout;
    if (stm->input_device_id) {
      if (std::unique_ptr<wchar_t[]> tmp =
              copy_wide_string(stm->input_device_id.get())) {
        XASSERT(!selected_output_device_id);
        selected_output_device_id = std::move(tmp);
      } else {
        LOG("Failed to copy device identifier while copying input stream "
            "configuration to output stream configuration to drive loopback.");
        return CUBEB_ERROR;
      }
    }
    stm->has_dummy_output = true;
  }

  if (has_output(stm)) {
    LOG("(%p) Setup render: device=%p", stm, selected_output_device_id.get());
    rv = setup_wasapi_stream_one_side(
        stm, &stm->output_stream_params, selected_output_device_id.get(),
        eRender, __uuidof(IAudioRenderClient), stm->output_client,
        &stm->output_buffer_frame_count, stm->refill_event, stm->render_client,
        &stm->output_mix_params, stm->output_device);
    if (rv != CUBEB_OK) {
      LOG("Failure to open the output side.");
      return rv;
    }

    HRESULT hr = 0;
#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
    hr = stm->output_client->GetService(__uuidof(IAudioStreamVolume),
                                        stm->audio_stream_volume.receive_vpp());
    if (FAILED(hr)) {
      LOG("Could not get the IAudioStreamVolume: %lx", hr);
      return CUBEB_ERROR;
    }
#endif

    XASSERT(stm->frames_written == 0);
    hr = stm->output_client->GetService(__uuidof(IAudioClock),
                                        stm->audio_clock.receive_vpp());
    if (FAILED(hr)) {
      LOG("Could not get the IAudioClock: %lx", hr);
      return CUBEB_ERROR;
    }

#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
    /* Restore the stream volume over a device change. */
    if (stream_set_volume(stm, stm->volume) != CUBEB_OK) {
      LOG("Could not set the volume.");
      return CUBEB_ERROR;
    }
#endif
  }

  /* If we have both input and output, we resample to
   * the highest sample rate available. */
  int32_t target_sample_rate;
  if (has_input(stm) && has_output(stm)) {
    XASSERT(stm->input_stream_params.rate == stm->output_stream_params.rate);
    target_sample_rate = stm->input_stream_params.rate;
  } else if (has_input(stm)) {
    target_sample_rate = stm->input_stream_params.rate;
  } else {
    XASSERT(has_output(stm));
    target_sample_rate = stm->output_stream_params.rate;
  }

  LOG("Target sample rate: %d", target_sample_rate);

  /* If we are playing/capturing a mono stream, we only resample one channel,
   and copy it over, so we are always resampling the number
   of channels of the stream, not the number of channels
   that WASAPI wants. */
  cubeb_stream_params input_params = stm->input_mix_params;
  input_params.channels = stm->input_stream_params.channels;
  cubeb_stream_params output_params = stm->output_mix_params;
  output_params.channels = stm->output_stream_params.channels;

  stm->resampler.reset(cubeb_resampler_create(
      stm, has_input(stm) ? &input_params : nullptr,
      has_output(stm) && !stm->has_dummy_output ? &output_params : nullptr,
      target_sample_rate, wasapi_data_callback, stm->user_ptr,
      stm->voice ? CUBEB_RESAMPLER_QUALITY_VOIP
                 : CUBEB_RESAMPLER_QUALITY_DESKTOP,
      CUBEB_RESAMPLER_RECLOCK_NONE));
  if (!stm->resampler) {
    LOG("Could not get a resampler");
    return CUBEB_ERROR;
  }

  XASSERT(has_input(stm) || has_output(stm));

  if (has_input(stm) && has_output(stm)) {
    stm->refill_callback = refill_callback_duplex;
  } else if (has_input(stm)) {
    stm->refill_callback = refill_callback_input;
  } else if (has_output(stm)) {
    stm->refill_callback = refill_callback_output;
  }

  // Create input mixer.
  if (has_input(stm) &&
      ((stm->input_mix_params.layout != CUBEB_LAYOUT_UNDEFINED &&
        stm->input_mix_params.layout != stm->input_stream_params.layout) ||
       (stm->input_mix_params.channels != stm->input_stream_params.channels))) {
    if (stm->input_mix_params.layout == CUBEB_LAYOUT_UNDEFINED) {
      LOG("Input stream using undefined layout! Any mixing may be "
          "unpredictable!\n");
    }
    stm->input_mixer.reset(cubeb_mixer_create(
        stm->input_stream_params.format, stm->input_mix_params.channels,
        stm->input_mix_params.layout, stm->input_stream_params.channels,
        stm->input_stream_params.layout));
    assert(stm->input_mixer);
  }

  // Create output mixer.
  if (has_output(stm) &&
      stm->output_mix_params.layout != stm->output_stream_params.layout) {
    if (stm->output_mix_params.layout == CUBEB_LAYOUT_UNDEFINED) {
      LOG("Output stream using undefined layout! Any mixing may be "
          "unpredictable!\n");
    }
    stm->output_mixer.reset(cubeb_mixer_create(
        stm->output_stream_params.format, stm->output_stream_params.channels,
        stm->output_stream_params.layout, stm->output_mix_params.channels,
        stm->output_mix_params.layout));
    assert(stm->output_mixer);
    // Input is up/down mixed when depacketized in get_input_buffer.
    stm->mix_buffer.resize(
        frames_to_bytes_before_mix(stm, stm->output_buffer_frame_count));
  }

  return CUBEB_OK;
}

ERole
pref_to_role(cubeb_stream_prefs prefs)
{
  if (prefs & CUBEB_STREAM_PREF_VOICE) {
    return eCommunications;
  }

  return eConsole;
}

int
wasapi_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name, cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int latency_frames,
                   cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  int rv;

  XASSERT(context && stream && (input_stream_params || output_stream_params));

  if (output_stream_params && input_stream_params &&
      output_stream_params->format != input_stream_params->format) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  cubeb_stream * stm = new cubeb_stream();
  auto_stream_ref stream_ref(stm);

  stm->context = context;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->role = eConsole;
  stm->input_bluetooth_handsfree = false;

  HRESULT hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(stm->device_enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return hr;
  }

  if (input_stream_params) {
    stm->input_stream_params = *input_stream_params;
    stm->input_device_id =
        utf8_to_wstr(reinterpret_cast<char const *>(input_device));
  }
  if (output_stream_params) {
    stm->output_stream_params = *output_stream_params;
    stm->output_device_id =
        utf8_to_wstr(reinterpret_cast<char const *>(output_device));
  }

  if (stm->output_stream_params.prefs & CUBEB_STREAM_PREF_VOICE ||
      stm->input_stream_params.prefs & CUBEB_STREAM_PREF_VOICE) {
    stm->voice = true;
  } else {
    stm->voice = false;
  }

  switch (output_stream_params ? output_stream_params->format
                               : input_stream_params->format) {
  case CUBEB_SAMPLE_S16NE:
    stm->bytes_per_sample = sizeof(short);
    stm->waveformatextensible_sub_format = KSDATAFORMAT_SUBTYPE_PCM;
    stm->linear_input_buffer.reset(new auto_array_wrapper_impl<short>);
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    stm->bytes_per_sample = sizeof(float);
    stm->waveformatextensible_sub_format = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    stm->linear_input_buffer.reset(new auto_array_wrapper_impl<float>);
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  stm->latency = latency_frames;

  stm->reconfigure_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->reconfigure_event) {
    LOG("Can't create the reconfigure event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  /* Unconditionally create the two events so that the wait logic is simpler. */
  stm->refill_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->refill_event) {
    LOG("Can't create the refill event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  stm->input_available_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->input_available_event) {
    LOG("Can't create the input available event , error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  stm->shutdown_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->shutdown_event) {
    LOG("Can't create the shutdown event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  stm->thread_ready_event = CreateEvent(NULL, 0, 0, NULL);
  if (!stm->thread_ready_event) {
    LOG("Can't create the thread ready event, error: %lx", GetLastError());
    return CUBEB_ERROR;
  }

  {
    /* Locking here is not strictly necessary, because we don't have a
       notification client that can reset the stream yet, but it lets us
       assert that the lock is held in the function. */
    auto_lock lock(stm->stream_reset_lock);
    rv = setup_wasapi_stream(stm);
  }
  if (rv != CUBEB_OK) {
    return rv;
  }

  // Follow the system default devices when not specifying devices explicitly
  // and CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING is not set.
  if ((!input_device && input_stream_params &&
       !(input_stream_params->prefs &
         CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING)) ||
      (!output_device && output_stream_params &&
       !(output_stream_params->prefs &
         CUBEB_STREAM_PREF_DISABLE_DEVICE_SWITCHING))) {
    LOG("Follow the system default input or/and output devices");
    HRESULT hr = register_notification_client(stm);
    if (FAILED(hr)) {
      /* this is not fatal, we can still play audio, but we won't be able
         to keep using the default audio endpoint if it changes. */
      LOG("failed to register notification client, %lx", hr);
    }
  }

  cubeb_async_log_reset_threads();
  stm->thread =
      (HANDLE)_beginthreadex(NULL, 512 * 1024, wasapi_stream_render_loop, stm,
                             STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
  if (stm->thread == NULL) {
    LOG("could not create WASAPI render thread.");
    return CUBEB_ERROR;
  }

  // Wait for the wasapi_stream_render_loop thread to signal that COM has been
  // initialized and the stream's ref_count has been incremented.
  hr = WaitForSingleObject(stm->thread_ready_event, INFINITE);
  XASSERT(hr == WAIT_OBJECT_0);
  CloseHandle(stm->thread_ready_event);
  stm->thread_ready_event = 0;

  wasapi_stream_add_ref(stm);
  *stream = stm;

  LOG("Stream init successful (%p)", *stream);
  return CUBEB_OK;
}

void
close_wasapi_stream(cubeb_stream * stm)
{
  XASSERT(stm);

  stm->stream_reset_lock.assert_current_thread_owns();

#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
  stm->audio_stream_volume = nullptr;
#endif
  stm->audio_clock = nullptr;
  stm->render_client = nullptr;
  stm->output_client = nullptr;
  stm->output_device = nullptr;

  stm->capture_client = nullptr;
  stm->input_client = nullptr;
  stm->input_device = nullptr;

  stm->total_frames_written += static_cast<UINT64>(
      round(stm->frames_written *
            stream_to_mix_samplerate_ratio(stm->output_stream_params,
                                           stm->output_mix_params)));
  stm->frames_written = 0;

  stm->resampler.reset();
  stm->output_mixer.reset();
  stm->input_mixer.reset();
  stm->mix_buffer.clear();
  if (stm->linear_input_buffer) {
    stm->linear_input_buffer->clear();
  }
}

LONG
wasapi_stream_add_ref(cubeb_stream * stm)
{
  XASSERT(stm);
  LONG result = InterlockedIncrement(&stm->ref_count);
  LOGV("Stream ref count incremented = %i (%p)", result, stm);
  return result;
}

LONG
wasapi_stream_release(cubeb_stream * stm)
{
  XASSERT(stm);

  LONG result = InterlockedDecrement(&stm->ref_count);
  LOGV("Stream ref count decremented = %i (%p)", result, stm);
  if (result == 0) {
    LOG("Stream ref count hit zero, destroying (%p)", stm);

    if (stm->notification_client) {
      unregister_notification_client(stm);
    }

    CloseHandle(stm->shutdown_event);
    CloseHandle(stm->reconfigure_event);
    CloseHandle(stm->refill_event);
    CloseHandle(stm->input_available_event);

    CloseHandle(stm->thread);

    // The variables intialized in wasapi_stream_init,
    // must be destroyed in wasapi_stream_release.
    stm->linear_input_buffer.reset();

    {
      auto_lock lock(stm->stream_reset_lock);
      close_wasapi_stream(stm);
    }

    delete stm;
  }

  return result;
}

void
wasapi_stream_destroy(cubeb_stream * stm)
{
  XASSERT(stm);
  LOG("Stream destroy called, decrementing ref count (%p)", stm);

  stop_and_join_render_thread(stm);
  wasapi_stream_release(stm);
}

enum StreamDirection { OUTPUT, INPUT };

int
stream_start_one_side(cubeb_stream * stm, StreamDirection dir)
{
  XASSERT(stm);
  XASSERT((dir == OUTPUT && stm->output_client) ||
          (dir == INPUT && stm->input_client));

  HRESULT hr =
      dir == OUTPUT ? stm->output_client->Start() : stm->input_client->Start();
  if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
    LOG("audioclient invalidated for %s device, reconfiguring",
        dir == OUTPUT ? "output" : "input");

    BOOL ok = ResetEvent(stm->reconfigure_event);
    if (!ok) {
      LOG("resetting reconfig event failed for %s stream: %lx",
          dir == OUTPUT ? "output" : "input", GetLastError());
    }

    close_wasapi_stream(stm);
    int r = setup_wasapi_stream(stm);
    if (r != CUBEB_OK) {
      LOG("reconfigure failed");
      return r;
    }

    HRESULT hr2 = dir == OUTPUT ? stm->output_client->Start()
                                : stm->input_client->Start();
    if (FAILED(hr2)) {
      LOG("could not start the %s stream after reconfig: %lx",
          dir == OUTPUT ? "output" : "input", hr);
      return CUBEB_ERROR;
    }
  } else if (FAILED(hr)) {
    LOG("could not start the %s stream: %lx.",
        dir == OUTPUT ? "output" : "input", hr);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

int
wasapi_stream_start(cubeb_stream * stm)
{
  auto_lock lock(stm->stream_reset_lock);

  XASSERT(stm);
  XASSERT(stm->output_client || stm->input_client);

  if (stm->output_client) {
    int rv = stream_start_one_side(stm, OUTPUT);
    if (rv != CUBEB_OK) {
      return rv;
    }
  }

  if (stm->input_client) {
    int rv = stream_start_one_side(stm, INPUT);
    if (rv != CUBEB_OK) {
      return rv;
    }
  }

  stm->active = true;

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

int
wasapi_stream_stop(cubeb_stream * stm)
{
  XASSERT(stm);
  HRESULT hr;

  {
    auto_lock lock(stm->stream_reset_lock);

    if (stm->output_client) {
      hr = stm->output_client->Stop();
      if (FAILED(hr)) {
        LOG("could not stop AudioClient (output)");
        return CUBEB_ERROR;
      }
    }

    if (stm->input_client) {
      hr = stm->input_client->Stop();
      if (FAILED(hr)) {
        LOG("could not stop AudioClient (input)");
        return CUBEB_ERROR;
      }
    }

    stm->active = false;

    wasapi_state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  }

  return CUBEB_OK;
}

int
wasapi_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  XASSERT(stm && position);
  auto_lock lock(stm->stream_reset_lock);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

  /* Calculate how far behind the current stream head the playback cursor is. */
  uint64_t stream_delay = static_cast<uint64_t>(current_stream_delay(stm) *
                                                stm->output_stream_params.rate);

  /* Calculate the logical stream head in frames at the stream sample rate. */
  uint64_t max_pos =
      stm->total_frames_written +
      static_cast<uint64_t>(
          round(stm->frames_written *
                stream_to_mix_samplerate_ratio(stm->output_stream_params,
                                               stm->output_mix_params)));

  *position = max_pos;
  if (stream_delay <= *position) {
    *position -= stream_delay;
  }

  if (*position < stm->prev_position) {
    *position = stm->prev_position;
  }
  stm->prev_position = *position;

  return CUBEB_OK;
}

int
wasapi_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  XASSERT(stm && latency);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

  auto_lock lock(stm->stream_reset_lock);

  /* The GetStreamLatency method only works if the
     AudioClient has been initialized. */
  if (!stm->output_client) {
    LOG("get_latency: No output_client.");
    return CUBEB_ERROR;
  }

  REFERENCE_TIME latency_hns;
  HRESULT hr = stm->output_client->GetStreamLatency(&latency_hns);
  if (FAILED(hr)) {
    LOG("GetStreamLatency failed %lx.", hr);
    return CUBEB_ERROR;
  }
  // This happens on windows 10: no error, but always 0 for latency.
  if (latency_hns == 0) {
    LOG("GetStreamLatency returned 0, using workaround.");
    double delay_s = current_stream_delay(stm);
    // convert to sample-frames
    *latency = delay_s * stm->output_stream_params.rate;
  } else {
    *latency = hns_to_frames(stm, latency_hns);
  }

  LOG("Output latency %u frames.", *latency);

  return CUBEB_OK;
}

int
wasapi_stream_get_input_latency(cubeb_stream * stm, uint32_t * latency)
{
  XASSERT(stm && latency);

  if (!has_input(stm)) {
    LOG("Input latency queried on an output-only stream.");
    return CUBEB_ERROR;
  }

  auto_lock lock(stm->stream_reset_lock);

  if (stm->input_latency_hns == LATENCY_NOT_AVAILABLE_YET) {
    LOG("Input latency not available yet.");
    return CUBEB_ERROR;
  }

  *latency = hns_to_frames(stm, stm->input_latency_hns);

  return CUBEB_OK;
}

int
wasapi_stream_set_volume(cubeb_stream * stm, float volume)
{
  auto_lock lock(stm->stream_reset_lock);

  if (!has_output(stm)) {
    return CUBEB_ERROR;
  }

#ifdef CUBEB_WASAPI_USE_IAUDIOSTREAMVOLUME
  if (stream_set_volume(stm, volume) != CUBEB_OK) {
    return CUBEB_ERROR;
  }
#endif

  stm->volume = volume;

  return CUBEB_OK;
}

static char const *
wstr_to_utf8(LPCWSTR str)
{
  int size = ::WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, NULL, NULL);
  if (size <= 0) {
    return nullptr;
  }

  char * ret = static_cast<char *>(malloc(size));
  ::WideCharToMultiByte(CP_UTF8, 0, str, -1, ret, size, NULL, NULL);
  return ret;
}

static std::unique_ptr<wchar_t const[]>
utf8_to_wstr(char const * str)
{
  int size = ::MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
  if (size <= 0) {
    return nullptr;
  }

  std::unique_ptr<wchar_t[]> ret(new wchar_t[size]);
  ::MultiByteToWideChar(CP_UTF8, 0, str, -1, ret.get(), size);
  return ret;
}

static com_ptr<IMMDevice>
wasapi_get_device_node(IMMDeviceEnumerator * enumerator, IMMDevice * dev)
{
  com_ptr<IMMDevice> ret;
  com_ptr<IDeviceTopology> devtopo;
  com_ptr<IConnector> connector;

  if (SUCCEEDED(dev->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, NULL,
                              devtopo.receive_vpp())) &&
      SUCCEEDED(devtopo->GetConnector(0, connector.receive()))) {
    wchar_t * tmp = nullptr;
    if (SUCCEEDED(connector->GetDeviceIdConnectedTo(&tmp))) {
      com_heap_ptr<wchar_t> filterid(tmp);
      if (FAILED(enumerator->GetDevice(filterid.get(), ret.receive())))
        ret = NULL;
    }
  }

  return ret;
}

static com_heap_ptr<wchar_t>
wasapi_get_default_device_id(EDataFlow flow, ERole role,
                             IMMDeviceEnumerator * enumerator)
{
  com_ptr<IMMDevice> dev;

  HRESULT hr = enumerator->GetDefaultAudioEndpoint(flow, role, dev.receive());
  if (SUCCEEDED(hr)) {
    wchar_t * tmp = nullptr;
    if (SUCCEEDED(dev->GetId(&tmp))) {
      com_heap_ptr<wchar_t> devid(tmp);
      return devid;
    }
  }

  return nullptr;
}

/* `ret` must be deallocated with `wasapi_destroy_device`, iff the return value
 * of this function is `CUBEB_OK`. */
int
wasapi_create_device(cubeb * ctx, cubeb_device_info & ret,
                     IMMDeviceEnumerator * enumerator, IMMDevice * dev,
                     wasapi_default_devices * defaults)
{
  com_ptr<IMMEndpoint> endpoint;
  com_ptr<IMMDevice> devnode;
  com_ptr<IAudioClient> client;
  EDataFlow flow;
  DWORD state = DEVICE_STATE_NOTPRESENT;
  com_ptr<IPropertyStore> propstore;
  REFERENCE_TIME def_period, min_period;
  HRESULT hr;

  XASSERT(enumerator && dev && defaults);

  // zero-out to be able to safely delete the pointers to friendly_name and
  // group_id at all time in this function.
  PodZero(&ret, 1);

  struct prop_variant : public PROPVARIANT {
    prop_variant() { PropVariantInit(this); }
    ~prop_variant() { PropVariantClear(this); }
    prop_variant(prop_variant const &) = delete;
    prop_variant & operator=(prop_variant const &) = delete;
  };

  hr = dev->QueryInterface(IID_PPV_ARGS(endpoint.receive()));
  if (FAILED(hr)) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }

  hr = endpoint->GetDataFlow(&flow);
  if (FAILED(hr)) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }

  wchar_t * tmp = nullptr;
  hr = dev->GetId(&tmp);
  if (FAILED(hr)) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }
  com_heap_ptr<wchar_t> device_id(tmp);

  char const * device_id_intern = intern_device_id(ctx, device_id.get());
  if (!device_id_intern) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }

  hr = dev->OpenPropertyStore(STGM_READ, propstore.receive());
  if (FAILED(hr)) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }

  hr = dev->GetState(&state);
  if (FAILED(hr)) {
    wasapi_destroy_device(&ret);
    return CUBEB_ERROR;
  }

  ret.device_id = device_id_intern;
  ret.devid = reinterpret_cast<cubeb_devid>(ret.device_id);
  prop_variant namevar;
  hr = propstore->GetValue(PKEY_Device_FriendlyName, &namevar);
  if (SUCCEEDED(hr) && namevar.vt == VT_LPWSTR) {
    ret.friendly_name = wstr_to_utf8(namevar.pwszVal);
  }
  if (!ret.friendly_name) {
    // This is not fatal, but a valid string is expected in all cases.
    char * empty = new char[1];
    empty[0] = '\0';
    ret.friendly_name = empty;
  }

  devnode = wasapi_get_device_node(enumerator, dev);
  if (devnode) {
    com_ptr<IPropertyStore> ps;
    hr = devnode->OpenPropertyStore(STGM_READ, ps.receive());
    if (FAILED(hr)) {
      wasapi_destroy_device(&ret);
      return CUBEB_ERROR;
    }

    prop_variant instancevar;
    hr = ps->GetValue(PKEY_Device_InstanceId, &instancevar);
    if (SUCCEEDED(hr) && instancevar.vt == VT_LPWSTR) {
      ret.group_id = wstr_to_utf8(instancevar.pwszVal);
    }
  }

  if (!ret.group_id) {
    // This is not fatal, but a valid string is expected in all cases.
    char * empty = new char[1];
    empty[0] = '\0';
    ret.group_id = empty;
  }

  ret.preferred = CUBEB_DEVICE_PREF_NONE;
  if (defaults->is_default(flow, eConsole, device_id.get())) {
    ret.preferred =
        (cubeb_device_pref)(ret.preferred | CUBEB_DEVICE_PREF_MULTIMEDIA |
                            CUBEB_DEVICE_PREF_NOTIFICATION);
  } else if (defaults->is_default(flow, eCommunications, device_id.get())) {
    ret.preferred =
        (cubeb_device_pref)(ret.preferred | CUBEB_DEVICE_PREF_VOICE);
  }

  if (flow == eRender) {
    ret.type = CUBEB_DEVICE_TYPE_OUTPUT;
  } else if (flow == eCapture) {
    ret.type = CUBEB_DEVICE_TYPE_INPUT;
  }

  switch (state) {
  case DEVICE_STATE_ACTIVE:
    ret.state = CUBEB_DEVICE_STATE_ENABLED;
    break;
  case DEVICE_STATE_UNPLUGGED:
    ret.state = CUBEB_DEVICE_STATE_UNPLUGGED;
    break;
  default:
    ret.state = CUBEB_DEVICE_STATE_DISABLED;
    break;
  };

  ret.format = static_cast<cubeb_device_fmt>(CUBEB_DEVICE_FMT_F32NE |
                                             CUBEB_DEVICE_FMT_S16NE);
  ret.default_format = CUBEB_DEVICE_FMT_F32NE;
  prop_variant fmtvar;
  hr = propstore->GetValue(PKEY_AudioEngine_DeviceFormat, &fmtvar);
  if (SUCCEEDED(hr) && fmtvar.vt == VT_BLOB) {
    if (fmtvar.blob.cbSize == sizeof(PCMWAVEFORMAT)) {
      const PCMWAVEFORMAT * pcm =
          reinterpret_cast<const PCMWAVEFORMAT *>(fmtvar.blob.pBlobData);

      ret.max_rate = ret.min_rate = ret.default_rate = pcm->wf.nSamplesPerSec;
      ret.max_channels = pcm->wf.nChannels;
    } else if (fmtvar.blob.cbSize >= sizeof(WAVEFORMATEX)) {
      WAVEFORMATEX * wfx =
          reinterpret_cast<WAVEFORMATEX *>(fmtvar.blob.pBlobData);

      if (fmtvar.blob.cbSize >= sizeof(WAVEFORMATEX) + wfx->cbSize ||
          wfx->wFormatTag == WAVE_FORMAT_PCM) {
        ret.max_rate = ret.min_rate = ret.default_rate = wfx->nSamplesPerSec;
        ret.max_channels = wfx->nChannels;
      }
    }
  }

  if (SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER,
                              NULL, client.receive_vpp())) &&
      SUCCEEDED(client->GetDevicePeriod(&def_period, &min_period))) {
    ret.latency_lo = hns_to_frames(ret.default_rate, min_period);
    ret.latency_hi = hns_to_frames(ret.default_rate, def_period);
  } else {
    ret.latency_lo = 0;
    ret.latency_hi = 0;
  }

  XASSERT(ret.friendly_name && ret.group_id);

  return CUBEB_OK;
}

void
wasapi_destroy_device(cubeb_device_info * device)
{
  delete[] device->friendly_name;
  delete[] device->group_id;
}

static int
wasapi_enumerate_devices_internal(cubeb * context, cubeb_device_type type,
                                  cubeb_device_collection * out,
                                  DWORD state_mask)
{
  com_ptr<IMMDeviceEnumerator> enumerator;
  com_ptr<IMMDeviceCollection> collection;
  HRESULT hr;
  UINT cc, i;
  EDataFlow flow;

  hr =
      CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(enumerator.receive()));
  if (FAILED(hr)) {
    LOG("Could not get device enumerator: %lx", hr);
    return CUBEB_ERROR;
  }

  wasapi_default_devices default_devices(enumerator.get());

  if (type == CUBEB_DEVICE_TYPE_OUTPUT) {
    flow = eRender;
  } else if (type == CUBEB_DEVICE_TYPE_INPUT) {
    flow = eCapture;
  } else if (type & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) {
    flow = eAll;
  } else {
    return CUBEB_ERROR;
  }

  hr = enumerator->EnumAudioEndpoints(flow, state_mask, collection.receive());
  if (FAILED(hr)) {
    LOG("Could not enumerate audio endpoints: %lx", hr);
    return CUBEB_ERROR;
  }

  hr = collection->GetCount(&cc);
  if (FAILED(hr)) {
    LOG("IMMDeviceCollection::GetCount() failed: %lx", hr);
    return CUBEB_ERROR;
  }
  cubeb_device_info * devices = new cubeb_device_info[cc];
  if (!devices)
    return CUBEB_ERROR;

  PodZero(devices, cc);
  out->count = 0;
  for (i = 0; i < cc; i++) {
    com_ptr<IMMDevice> dev;
    hr = collection->Item(i, dev.receive());
    if (FAILED(hr)) {
      LOG("IMMDeviceCollection::Item(%u) failed: %lx", i - 1, hr);
      continue;
    }
    if (wasapi_create_device(context, devices[out->count], enumerator.get(),
                             dev.get(), &default_devices) == CUBEB_OK) {
      out->count += 1;
    }
  }

  out->device = devices;
  return CUBEB_OK;
}

static int
wasapi_enumerate_devices(cubeb * context, cubeb_device_type type,
                         cubeb_device_collection * out)
{
  return wasapi_enumerate_devices_internal(
      context, type, out,
      DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED | DEVICE_STATE_UNPLUGGED);
}

static int
wasapi_device_collection_destroy(cubeb * /*ctx*/,
                                 cubeb_device_collection * collection)
{
  XASSERT(collection);

  for (size_t n = 0; n < collection->count; n++) {
    cubeb_device_info & dev = collection->device[n];
    wasapi_destroy_device(&dev);
  }

  delete[] collection->device;
  return CUBEB_OK;
}

static int
wasapi_register_device_collection_changed(
    cubeb * context, cubeb_device_type devtype,
    cubeb_device_collection_changed_callback collection_changed_callback,
    void * user_ptr)
{
  auto_lock lock(context->lock);
  if (devtype == CUBEB_DEVICE_TYPE_UNKNOWN) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (collection_changed_callback) {
    // Make sure it has been unregistered first.
    XASSERT(((devtype & CUBEB_DEVICE_TYPE_INPUT) &&
             !context->input_collection_changed_callback) ||
            ((devtype & CUBEB_DEVICE_TYPE_OUTPUT) &&
             !context->output_collection_changed_callback));

    // Stop the notification client. Notifications arrive on
    // a separate thread. We stop them here to avoid
    // synchronization issues during the update.
    if (context->device_collection_enumerator.get()) {
      HRESULT hr = unregister_collection_notification_client(context);
      if (FAILED(hr)) {
        return CUBEB_ERROR;
      }
    }

    if (devtype & CUBEB_DEVICE_TYPE_INPUT) {
      context->input_collection_changed_callback = collection_changed_callback;
      context->input_collection_changed_user_ptr = user_ptr;
    }
    if (devtype & CUBEB_DEVICE_TYPE_OUTPUT) {
      context->output_collection_changed_callback = collection_changed_callback;
      context->output_collection_changed_user_ptr = user_ptr;
    }

    HRESULT hr = register_collection_notification_client(context);
    if (FAILED(hr)) {
      return CUBEB_ERROR;
    }
  } else {
    if (!context->device_collection_enumerator.get()) {
      // Already unregistered, ignore it.
      return CUBEB_OK;
    }

    HRESULT hr = unregister_collection_notification_client(context);
    if (FAILED(hr)) {
      return CUBEB_ERROR;
    }
    if (devtype & CUBEB_DEVICE_TYPE_INPUT) {
      context->input_collection_changed_callback = nullptr;
      context->input_collection_changed_user_ptr = nullptr;
    }
    if (devtype & CUBEB_DEVICE_TYPE_OUTPUT) {
      context->output_collection_changed_callback = nullptr;
      context->output_collection_changed_user_ptr = nullptr;
    }

    // If after the updates we still have registered
    // callbacks restart the notification client.
    if (context->input_collection_changed_callback ||
        context->output_collection_changed_callback) {
      hr = register_collection_notification_client(context);
      if (FAILED(hr)) {
        return CUBEB_ERROR;
      }
    }
  }

  return CUBEB_OK;
}

cubeb_ops const wasapi_ops = {
    /*.init =*/wasapi_init,
    /*.get_backend_id =*/wasapi_get_backend_id,
    /*.get_max_channel_count =*/wasapi_get_max_channel_count,
    /*.get_min_latency =*/wasapi_get_min_latency,
    /*.get_preferred_sample_rate =*/wasapi_get_preferred_sample_rate,
    /*.enumerate_devices =*/wasapi_enumerate_devices,
    /*.device_collection_destroy =*/wasapi_device_collection_destroy,
    /*.destroy =*/wasapi_destroy,
    /*.stream_init =*/wasapi_stream_init,
    /*.stream_destroy =*/wasapi_stream_destroy,
    /*.stream_start =*/wasapi_stream_start,
    /*.stream_stop =*/wasapi_stream_stop,
    /*.stream_get_position =*/wasapi_stream_get_position,
    /*.stream_get_latency =*/wasapi_stream_get_latency,
    /*.stream_get_input_latency =*/wasapi_stream_get_input_latency,
    /*.stream_set_volume =*/wasapi_stream_set_volume,
    /*.stream_set_name =*/NULL,
    /*.stream_get_current_device =*/NULL,
    /*.stream_device_destroy =*/NULL,
    /*.stream_register_device_changed_callback =*/NULL,
    /*.register_device_collection_changed =*/
    wasapi_register_device_collection_changed,
};
} // namespace
