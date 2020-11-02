#include "pulse.h"
#include <pulse/pulseaudio.h>
#include <dlfcn.h>
#include <iostream>
#include <atomic>

#if PA_CHECK_VERSION(12,99,1)
#define CONST const
#else
#define CONST
#endif

#define FUNDEFDECL(x) static decltype(&x) pfn_##x = nullptr
#define FUN_UNLOAD(fun) pfn_##fun = nullptr
#define FUN_LOAD(h,fun) \
	pfn_##fun = (decltype(&fun))(dlsym(h, #fun));	\
	if((error = dlerror()) != NULL) {				\
		std::cerr << error << std::endl;			\
		DynUnloadPulse();							\
		return false;								\
	}

FUNDEFDECL(pa_usec_to_bytes);
FUNDEFDECL(pa_bytes_per_second);
FUNDEFDECL(pa_threaded_mainloop_start);
FUNDEFDECL(pa_threaded_mainloop_free);
FUNDEFDECL(pa_threaded_mainloop_stop);
FUNDEFDECL(pa_stream_unref);
FUNDEFDECL(pa_stream_disconnect);
FUNDEFDECL(pa_threaded_mainloop_new);
FUNDEFDECL(pa_threaded_mainloop_get_api);
FUNDEFDECL(pa_stream_set_read_callback);
FUNDEFDECL(pa_stream_connect_record);
FUNDEFDECL(pa_stream_new);
FUNDEFDECL(pa_stream_peek);
FUNDEFDECL(pa_strerror);
FUNDEFDECL(pa_stream_drop);
FUNDEFDECL(pa_context_connect);
FUNDEFDECL(pa_operation_unref);
FUNDEFDECL(pa_context_set_state_callback);
FUNDEFDECL(pa_context_get_state);
FUNDEFDECL(pa_mainloop_get_api);
FUNDEFDECL(pa_context_unref);
FUNDEFDECL(pa_context_disconnect);
FUNDEFDECL(pa_operation_get_state);
FUNDEFDECL(pa_context_get_source_info_list);
FUNDEFDECL(pa_mainloop_new);
FUNDEFDECL(pa_context_new);
FUNDEFDECL(pa_mainloop_iterate);
FUNDEFDECL(pa_mainloop_free);
FUNDEFDECL(pa_context_get_sink_info_list);
FUNDEFDECL(pa_stream_connect_playback);
FUNDEFDECL(pa_stream_set_write_callback);
FUNDEFDECL(pa_stream_begin_write);
FUNDEFDECL(pa_stream_cancel_write);
FUNDEFDECL(pa_stream_write);
FUNDEFDECL(pa_stream_get_state);
FUNDEFDECL(pa_stream_cork);
FUNDEFDECL(pa_stream_is_corked);
FUNDEFDECL(pa_stream_is_suspended);
FUNDEFDECL(pa_stream_set_state_callback);
FUNDEFDECL(pa_threaded_mainloop_lock);
FUNDEFDECL(pa_threaded_mainloop_unlock);
FUNDEFDECL(pa_threaded_mainloop_signal);
FUNDEFDECL(pa_threaded_mainloop_wait);
FUNDEFDECL(pa_sample_size);
FUNDEFDECL(pa_frame_size);
FUNDEFDECL(pa_stream_get_latency);
FUNDEFDECL(pa_stream_update_timing_info);

static void* pulse_handle = nullptr;
static std::atomic<int> refCntPulse (0);

//TODO Probably needs mutex somewhere, but PCSX2 usually inits pretty serially
bool DynLoadPulse()
{
	const char* error = nullptr;

	refCntPulse++;
	if (pulse_handle && pfn_pa_mainloop_free)
		return true;

	//dlopen itself is refcounted too
	pulse_handle = dlopen ("libpulse.so.0", RTLD_LAZY);
	if (!pulse_handle) {
		std::cerr << dlerror() << std::endl;
		return false;
	}

	FUN_LOAD(pulse_handle, pa_stream_update_timing_info);
	FUN_LOAD(pulse_handle, pa_stream_get_latency);
	FUN_LOAD(pulse_handle, pa_usec_to_bytes);
	FUN_LOAD(pulse_handle, pa_bytes_per_second);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_start);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_free);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_stop);
	FUN_LOAD(pulse_handle, pa_stream_unref);
	FUN_LOAD(pulse_handle, pa_stream_disconnect);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_new);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_get_api);
	FUN_LOAD(pulse_handle, pa_stream_set_read_callback);
	FUN_LOAD(pulse_handle, pa_stream_connect_record);
	FUN_LOAD(pulse_handle, pa_stream_new);
	FUN_LOAD(pulse_handle, pa_stream_peek);
	FUN_LOAD(pulse_handle, pa_strerror);
	FUN_LOAD(pulse_handle, pa_stream_drop);
	FUN_LOAD(pulse_handle, pa_context_connect);
	FUN_LOAD(pulse_handle, pa_operation_unref);
	FUN_LOAD(pulse_handle, pa_context_set_state_callback);
	FUN_LOAD(pulse_handle, pa_context_get_state);
	FUN_LOAD(pulse_handle, pa_mainloop_get_api);
	FUN_LOAD(pulse_handle, pa_context_unref);
	FUN_LOAD(pulse_handle, pa_context_disconnect);
	FUN_LOAD(pulse_handle, pa_operation_get_state);
	FUN_LOAD(pulse_handle, pa_context_get_source_info_list);
	FUN_LOAD(pulse_handle, pa_mainloop_new);
	FUN_LOAD(pulse_handle, pa_context_new);
	FUN_LOAD(pulse_handle, pa_mainloop_iterate);
	FUN_LOAD(pulse_handle, pa_context_get_sink_info_list);
	FUN_LOAD(pulse_handle, pa_stream_connect_playback);
	FUN_LOAD(pulse_handle, pa_stream_set_write_callback);
	FUN_LOAD(pulse_handle, pa_stream_begin_write);
	FUN_LOAD(pulse_handle, pa_stream_cancel_write);
	FUN_LOAD(pulse_handle, pa_stream_write);
	FUN_LOAD(pulse_handle, pa_stream_get_state);
	FUN_LOAD(pulse_handle, pa_stream_cork);
	FUN_LOAD(pulse_handle, pa_stream_is_corked);
	FUN_LOAD(pulse_handle, pa_stream_is_suspended);
	FUN_LOAD(pulse_handle, pa_stream_set_state_callback);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_lock);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_unlock);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_signal);
	FUN_LOAD(pulse_handle, pa_threaded_mainloop_wait);
	FUN_LOAD(pulse_handle, pa_sample_size);
	FUN_LOAD(pulse_handle, pa_frame_size);
	FUN_LOAD(pulse_handle, pa_mainloop_free);
	return true;
}

void DynUnloadPulse()
{
	if (!pulse_handle && !pfn_pa_mainloop_free)
		return;

	if(!refCntPulse || --refCntPulse > 0)
		return;

	FUN_UNLOAD(pa_stream_update_timing_info);
	FUN_UNLOAD(pa_stream_get_latency);
	FUN_UNLOAD(pa_usec_to_bytes);
	FUN_UNLOAD(pa_bytes_per_second);
	FUN_UNLOAD(pa_threaded_mainloop_start);
	FUN_UNLOAD(pa_threaded_mainloop_free);
	FUN_UNLOAD(pa_threaded_mainloop_stop);
	FUN_UNLOAD(pa_stream_unref);
	FUN_UNLOAD(pa_stream_disconnect);
	FUN_UNLOAD(pa_threaded_mainloop_new);
	FUN_UNLOAD(pa_threaded_mainloop_get_api);
	FUN_UNLOAD(pa_stream_set_read_callback);
	FUN_UNLOAD(pa_stream_connect_record);
	FUN_UNLOAD(pa_stream_new);
	FUN_UNLOAD(pa_stream_peek);
	FUN_UNLOAD(pa_strerror);
	FUN_UNLOAD(pa_stream_drop);
	FUN_UNLOAD(pa_context_connect);
	FUN_UNLOAD(pa_operation_unref);
	FUN_UNLOAD(pa_context_set_state_callback);
	FUN_UNLOAD(pa_context_get_state);
	FUN_UNLOAD(pa_mainloop_get_api);
	FUN_UNLOAD(pa_context_unref);
	FUN_UNLOAD(pa_context_disconnect);
	FUN_UNLOAD(pa_operation_get_state);
	FUN_UNLOAD(pa_context_get_source_info_list);
	FUN_UNLOAD(pa_mainloop_new);
	FUN_UNLOAD(pa_context_new);
	FUN_UNLOAD(pa_mainloop_iterate);
	FUN_UNLOAD(pa_context_get_sink_info_list);
	FUN_UNLOAD(pa_stream_connect_playback);
	FUN_UNLOAD(pa_stream_set_write_callback);
	FUN_UNLOAD(pa_stream_begin_write);
	FUN_UNLOAD(pa_stream_cancel_write);
	FUN_UNLOAD(pa_stream_write);
	FUN_UNLOAD(pa_stream_get_state);
	FUN_UNLOAD(pa_stream_cork);
	FUN_UNLOAD(pa_stream_is_corked);
	FUN_UNLOAD(pa_stream_is_suspended);
	FUN_UNLOAD(pa_stream_set_state_callback);
	FUN_UNLOAD(pa_threaded_mainloop_lock);
	FUN_UNLOAD(pa_threaded_mainloop_unlock);
	FUN_UNLOAD(pa_threaded_mainloop_signal);
	FUN_UNLOAD(pa_threaded_mainloop_wait);
	FUN_UNLOAD(pa_sample_size);
	FUN_UNLOAD(pa_frame_size);
	FUN_UNLOAD(pa_mainloop_free);

	dlclose(pulse_handle);
	pulse_handle = nullptr;
}
#undef FUNDEFDECL
#undef FUN_LOAD
#undef FUN_UNLOAD

const char* pa_strerror(int error)
{
	if (pfn_pa_strerror)
		return pfn_pa_strerror(error);
	return NULL;
}

int pa_context_connect(pa_context *c, const char *server, pa_context_flags_t flags, const pa_spawn_api *api)
{
	if (pfn_pa_context_connect)
		return pfn_pa_context_connect(c, server, flags, api);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_mainloop_iterate(pa_mainloop *m, int block, int *retval)
{
	if (pfn_pa_mainloop_iterate)
		return pfn_pa_mainloop_iterate(m, block, retval);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_stream_disconnect(pa_stream *s)
{
	if (pfn_pa_stream_disconnect)
		return pfn_pa_stream_disconnect(s);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_stream_drop(pa_stream *p)
{
	if (pfn_pa_stream_drop)
		return pfn_pa_stream_drop(p);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_threaded_mainloop_start(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_start)
		return pfn_pa_threaded_mainloop_start(m);
	return PA_ERR_NOTIMPLEMENTED;
}

pa_context *pa_context_new(pa_mainloop_api *mainloop, const char *name)
{
	if (pfn_pa_context_new)
		return pfn_pa_context_new(mainloop, name);
	return NULL;
}

pa_context_state_t pa_context_get_state(CONST pa_context *c)
{
	if (pfn_pa_context_get_state)
		return pfn_pa_context_get_state(c);
	return PA_CONTEXT_FAILED;
}

pa_mainloop_api* pa_mainloop_get_api(pa_mainloop *m)
{
	if (pfn_pa_mainloop_get_api)
		return pfn_pa_mainloop_get_api(m);
	return NULL;
}

pa_mainloop_api* pa_threaded_mainloop_get_api(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_get_api)
		return pfn_pa_threaded_mainloop_get_api(m);
	return NULL;
}

pa_mainloop *pa_mainloop_new(void)
{
	if (pfn_pa_mainloop_new)
		return pfn_pa_mainloop_new();
	return NULL;
}

pa_operation* pa_context_get_source_info_list(pa_context *c, pa_source_info_cb_t cb, void *userdata)
{
	if (pfn_pa_context_get_source_info_list)
		return pfn_pa_context_get_source_info_list(c, cb, userdata);
	return NULL;
}

pa_operation_state_t pa_operation_get_state(CONST pa_operation *o)
{
	if (pfn_pa_operation_get_state)
		return pfn_pa_operation_get_state(o);
	return PA_OPERATION_CANCELLED;
}

pa_threaded_mainloop *pa_threaded_mainloop_new(void)
{
	if (pfn_pa_threaded_mainloop_new)
		return pfn_pa_threaded_mainloop_new();
	return NULL;
}

size_t pa_bytes_per_second(const pa_sample_spec *spec)
{
	if (pfn_pa_bytes_per_second)
		return pfn_pa_bytes_per_second(spec);
	return 0;
}

size_t pa_usec_to_bytes(pa_usec_t t, const pa_sample_spec *spec)
{
	if (pfn_pa_usec_to_bytes)
		return pfn_pa_usec_to_bytes(t, spec);
	return 0;
}

void pa_context_disconnect(pa_context *c)
{
	if (pfn_pa_context_disconnect)
		pfn_pa_context_disconnect(c);
}

void pa_context_set_state_callback(pa_context *c, pa_context_notify_cb_t cb, void *userdata)
{
	if (pfn_pa_context_set_state_callback)
		pfn_pa_context_set_state_callback(c, cb, userdata);
}

void pa_context_unref(pa_context *c)
{
	if (pfn_pa_context_unref)
		pfn_pa_context_unref(c);
}

void pa_mainloop_free(pa_mainloop *m)
{
	if (pfn_pa_mainloop_free)
		pfn_pa_mainloop_free(m);
}

void pa_operation_unref(pa_operation *o)
{
	if (pfn_pa_operation_unref)
		pfn_pa_operation_unref(o);
}

void pa_stream_set_read_callback(pa_stream *p, pa_stream_request_cb_t cb, void *userdata)
{
	if (pfn_pa_stream_set_read_callback)
		pfn_pa_stream_set_read_callback(p, cb, userdata);
}

void pa_stream_unref(pa_stream *s)
{
	if (pfn_pa_stream_unref)
		pfn_pa_stream_unref(s);
}

void pa_threaded_mainloop_free(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_free)
		pfn_pa_threaded_mainloop_free(m);
}

void pa_threaded_mainloop_stop(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_stop)
		pfn_pa_threaded_mainloop_stop(m);
}

int pa_stream_peek(pa_stream *p, const void **data, size_t *nbytes)
{
	if (pfn_pa_stream_peek)
		return pfn_pa_stream_peek(p, data, nbytes);
	return -PA_ERR_NOTIMPLEMENTED;
}

pa_stream* pa_stream_new(pa_context *c, const char *name, const pa_sample_spec *ss, const pa_channel_map *map)
{
	if (pfn_pa_stream_new)
		return pfn_pa_stream_new(c, name, ss, map);
	return NULL;
}

int pa_stream_connect_record(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags)
{
	if (pfn_pa_stream_connect_record)
		return pfn_pa_stream_connect_record(s, dev, attr, flags);
	return PA_ERR_NOTIMPLEMENTED;
}

pa_operation* pa_context_get_sink_info_list(pa_context * c, pa_sink_info_cb_t cb, void * userdata)
{
	if (pfn_pa_context_get_sink_info_list)
		return pfn_pa_context_get_sink_info_list(c, cb, userdata);
	return NULL;
}

int pa_stream_connect_playback(pa_stream *s, const char *dev,
		const pa_buffer_attr *attr, pa_stream_flags_t flags,
		const pa_cvolume *volume,
		pa_stream *sync_stream)
{
	if (pfn_pa_stream_connect_playback)
		return pfn_pa_stream_connect_playback(s, dev, attr, flags, volume, sync_stream);
	return PA_ERR_NOTIMPLEMENTED;
}

void pa_stream_set_write_callback(pa_stream *p, pa_stream_request_cb_t cb, void *userdata)
{
	if (pfn_pa_stream_set_write_callback)
		pfn_pa_stream_set_write_callback(p, cb, userdata);
}

int pa_stream_begin_write(pa_stream *p, void **data, size_t *nbytes)
{
	if (pfn_pa_stream_begin_write)
		return pfn_pa_stream_begin_write(p, data, nbytes);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_stream_cancel_write(pa_stream *p)
{
	if (pfn_pa_stream_cancel_write)
		return pfn_pa_stream_cancel_write(p);
	return PA_ERR_NOTIMPLEMENTED;
}

int pa_stream_write(pa_stream *p, const void *data, size_t nbytes, pa_free_cb_t free_cb, int64_t offset, pa_seek_mode_t seek)
{
	if (pfn_pa_stream_write)
		return pfn_pa_stream_write(p, data, nbytes, free_cb, offset, seek);
	return PA_ERR_NOTIMPLEMENTED;
}

pa_stream_state_t pa_stream_get_state(CONST pa_stream *p)
{
	if (pfn_pa_stream_get_state)
		return pfn_pa_stream_get_state(p);
	return PA_STREAM_UNCONNECTED;
}

pa_operation* pa_stream_cork(pa_stream *s, int b, pa_stream_success_cb_t cb, void *userdata)
{
	if (pfn_pa_stream_cork)
		return pfn_pa_stream_cork(s, b, cb, userdata);
	return NULL;
}

int pa_stream_is_corked(CONST pa_stream *s)
{
	if (pfn_pa_stream_is_corked)
		return pfn_pa_stream_is_corked (s);
	return -PA_ERR_NOTIMPLEMENTED;
}

void pa_threaded_mainloop_lock(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_lock)
		pfn_pa_threaded_mainloop_lock(m);
}

void pa_threaded_mainloop_unlock(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_unlock)
		pfn_pa_threaded_mainloop_unlock(m);
}

void pa_threaded_mainloop_signal(pa_threaded_mainloop *m, int wait_for_accept)
{
	if (pfn_pa_threaded_mainloop_signal)
		pfn_pa_threaded_mainloop_signal(m, wait_for_accept);
}

void pa_threaded_mainloop_wait(pa_threaded_mainloop *m)
{
	if (pfn_pa_threaded_mainloop_wait)
		pfn_pa_threaded_mainloop_wait(m);
}

int pa_stream_is_suspended(CONST pa_stream *s)
{
	if (pfn_pa_stream_is_suspended)
		return pfn_pa_stream_is_suspended(s);
	return -PA_ERR_NOTIMPLEMENTED;
}

void pa_stream_set_state_callback(pa_stream *s, pa_stream_notify_cb_t cb, void *userdata)
{
	if (pfn_pa_stream_set_state_callback)
		pfn_pa_stream_set_state_callback(s, cb, userdata);
}

size_t pa_sample_size(const pa_sample_spec *spec)
{
	if (pfn_pa_sample_size)
		return pfn_pa_sample_size(spec);
	return 0;
}

size_t pa_frame_size(const pa_sample_spec *spec)
{
	if (pfn_pa_frame_size)
		return pfn_pa_frame_size(spec);
	return 0;
}

pa_operation* pa_stream_update_timing_info(pa_stream *p, pa_stream_success_cb_t cb, void *userdata)
{
	if (pfn_pa_stream_update_timing_info)
		return pfn_pa_stream_update_timing_info(p, cb, userdata);
	return NULL;
}

int pa_stream_get_latency(pa_stream *s, pa_usec_t *r_usec, int *negative)
{
	if (pfn_pa_stream_get_latency)
		return pfn_pa_stream_get_latency(s, r_usec, negative);
	return -PA_ERR_NOTIMPLEMENTED;
}