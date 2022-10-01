/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _XOPEN_SOURCE 500
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_tracing.h"
#include <alsa/asoundlib.h>
#include <assert.h>
#include <dlfcn.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef DISABLE_LIBASOUND_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) (*cubeb_##x)
#define LIBASOUND_API_VISIT(X)                                                 \
  X(snd_config)                                                                \
  X(snd_config_add)                                                            \
  X(snd_config_copy)                                                           \
  X(snd_config_delete)                                                         \
  X(snd_config_get_id)                                                         \
  X(snd_config_get_string)                                                     \
  X(snd_config_imake_integer)                                                  \
  X(snd_config_search)                                                         \
  X(snd_config_search_definition)                                              \
  X(snd_lib_error_set_handler)                                                 \
  X(snd_pcm_avail_update)                                                      \
  X(snd_pcm_close)                                                             \
  X(snd_pcm_delay)                                                             \
  X(snd_pcm_drain)                                                             \
  X(snd_pcm_frames_to_bytes)                                                   \
  X(snd_pcm_get_params)                                                        \
  X(snd_pcm_hw_params_any)                                                     \
  X(snd_pcm_hw_params_get_channels_max)                                        \
  X(snd_pcm_hw_params_get_rate)                                                \
  X(snd_pcm_hw_params_set_rate_near)                                           \
  X(snd_pcm_hw_params_sizeof)                                                  \
  X(snd_pcm_nonblock)                                                          \
  X(snd_pcm_open)                                                              \
  X(snd_pcm_open_lconf)                                                        \
  X(snd_pcm_pause)                                                             \
  X(snd_pcm_poll_descriptors)                                                  \
  X(snd_pcm_poll_descriptors_count)                                            \
  X(snd_pcm_poll_descriptors_revents)                                          \
  X(snd_pcm_readi)                                                             \
  X(snd_pcm_recover)                                                           \
  X(snd_pcm_set_params)                                                        \
  X(snd_pcm_start)                                                             \
  X(snd_pcm_state)                                                             \
  X(snd_pcm_writei)

#define MAKE_TYPEDEF(x) static typeof(x) * cubeb_##x;
LIBASOUND_API_VISIT(MAKE_TYPEDEF);
#undef MAKE_TYPEDEF
/* snd_pcm_hw_params_alloca is actually a macro */
#define snd_pcm_hw_params_sizeof cubeb_snd_pcm_hw_params_sizeof
#endif

#define CUBEB_STREAM_MAX 16
#define CUBEB_WATCHDOG_MS 10000

#define CUBEB_ALSA_PCM_NAME "default"

#define ALSA_PA_PLUGIN "ALSA <-> PulseAudio PCM I/O Plugin"

/* ALSA is not thread-safe.  snd_pcm_t instances are individually protected
   by the owning cubeb_stream's mutex.  snd_pcm_t creation and destruction
   is not thread-safe until ALSA 1.0.24 (see alsa-lib.git commit 91c9c8f1),
   so those calls must be wrapped in the following mutex. */
static pthread_mutex_t cubeb_alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static int cubeb_alsa_error_handler_set = 0;

static struct cubeb_ops const alsa_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * libasound;

  pthread_t thread;

  /* Mutex for streams array, must not be held while blocked in poll(2). */
  pthread_mutex_t mutex;

  /* Sparse array of streams managed by this context. */
  cubeb_stream * streams[CUBEB_STREAM_MAX];

  /* fds and nfds are only updated by alsa_run when rebuild is set. */
  struct pollfd * fds;
  nfds_t nfds;
  int rebuild;

  int shutdown;

  /* Control pipe for forcing poll to wake and rebuild fds or recalculate the
   * timeout. */
  int control_fd_read;
  int control_fd_write;

  /* Track number of active streams.  This is limited to CUBEB_STREAM_MAX
     due to resource contraints. */
  unsigned int active_streams;

  /* Local configuration with handle_underrun workaround set for PulseAudio
     ALSA plugin.  Will be NULL if the PA ALSA plugin is not in use or the
     workaround is not required. */
  snd_config_t * local_config;
  int is_pa;
};

enum stream_state { INACTIVE, RUNNING, DRAINING, PROCESSING, ERROR };

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * user_ptr;
  /**/
  pthread_mutex_t mutex;
  snd_pcm_t * pcm;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  snd_pcm_uframes_t stream_position;
  snd_pcm_uframes_t last_position;
  snd_pcm_uframes_t buffer_size;
  cubeb_stream_params params;

  /* Every member after this comment is protected by the owning context's
     mutex rather than the stream's mutex, or is only used on the context's
     run thread. */
  pthread_cond_t cond; /* Signaled when the stream's state is changed. */

  enum stream_state state;

  struct pollfd * saved_fds; /* A copy of the pollfds passed in at init time. */
  struct pollfd *
      fds; /* Pointer to this waitable's pollfds within struct cubeb's fds. */
  nfds_t nfds;

  struct timeval drain_timeout;

  /* XXX: Horrible hack -- if an active stream has been idle for
     CUBEB_WATCHDOG_MS it will be disabled and the error callback will be
     called.  This works around a bug seen with older versions of ALSA and
     PulseAudio where streams would stop requesting new data despite still
     being logically active and playing. */
  struct timeval last_activity;
  float volume;

  char * buffer;
  snd_pcm_uframes_t bufframes;
  snd_pcm_stream_t stream_type;

  struct cubeb_stream * other_stream;
};

static int
any_revents(struct pollfd * fds, nfds_t nfds)
{
  nfds_t i;

  for (i = 0; i < nfds; ++i) {
    if (fds[i].revents) {
      return 1;
    }
  }

  return 0;
}

static int
cmp_timeval(struct timeval * a, struct timeval * b)
{
  if (a->tv_sec == b->tv_sec) {
    if (a->tv_usec == b->tv_usec) {
      return 0;
    }
    return a->tv_usec > b->tv_usec ? 1 : -1;
  }
  return a->tv_sec > b->tv_sec ? 1 : -1;
}

static int
timeval_to_relative_ms(struct timeval * tv)
{
  struct timeval now;
  struct timeval dt;
  long long t;
  int r;

  gettimeofday(&now, NULL);
  r = cmp_timeval(tv, &now);
  if (r >= 0) {
    timersub(tv, &now, &dt);
  } else {
    timersub(&now, tv, &dt);
  }
  t = dt.tv_sec;
  t *= 1000;
  t += (dt.tv_usec + 500) / 1000;

  if (t > INT_MAX) {
    t = INT_MAX;
  } else if (t < INT_MIN) {
    t = INT_MIN;
  }

  return r >= 0 ? t : -t;
}

static int
ms_until(struct timeval * tv)
{
  return timeval_to_relative_ms(tv);
}

static int
ms_since(struct timeval * tv)
{
  return -timeval_to_relative_ms(tv);
}

static void
rebuild(cubeb * ctx)
{
  nfds_t nfds;
  int i;
  nfds_t j;
  cubeb_stream * stm;

  assert(ctx->rebuild);

  /* Always count context's control pipe fd. */
  nfds = 1;
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm) {
      stm->fds = NULL;
      if (stm->state == RUNNING) {
        nfds += stm->nfds;
      }
    }
  }

  free(ctx->fds);
  ctx->fds = calloc(nfds, sizeof(struct pollfd));
  assert(ctx->fds);
  ctx->nfds = nfds;

  /* Include context's control pipe fd. */
  ctx->fds[0].fd = ctx->control_fd_read;
  ctx->fds[0].events = POLLIN | POLLERR;

  for (i = 0, j = 1; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm && stm->state == RUNNING) {
      memcpy(&ctx->fds[j], stm->saved_fds, stm->nfds * sizeof(struct pollfd));
      stm->fds = &ctx->fds[j];
      j += stm->nfds;
    }
  }

  ctx->rebuild = 0;
}

static void
poll_wake(cubeb * ctx)
{
  if (write(ctx->control_fd_write, "x", 1) < 0) {
    /* ignore write error */
  }
}

static void
set_timeout(struct timeval * timeout, unsigned int ms)
{
  gettimeofday(timeout, NULL);
  timeout->tv_sec += ms / 1000;
  timeout->tv_usec += (ms % 1000) * 1000;
}

static void
stream_buffer_decrement(cubeb_stream * stm, long count)
{
  char * bufremains =
      stm->buffer + WRAP(snd_pcm_frames_to_bytes)(stm->pcm, count);
  memmove(stm->buffer, bufremains,
          WRAP(snd_pcm_frames_to_bytes)(stm->pcm, stm->bufframes - count));
  stm->bufframes -= count;
}

static void
alsa_set_stream_state(cubeb_stream * stm, enum stream_state state)
{
  cubeb * ctx;
  int r;

  ctx = stm->context;
  stm->state = state;
  r = pthread_cond_broadcast(&stm->cond);
  assert(r == 0);
  ctx->rebuild = 1;
  poll_wake(ctx);
}

static enum stream_state
alsa_process_stream(cubeb_stream * stm)
{
  unsigned short revents;
  snd_pcm_sframes_t avail;
  int draining;

  draining = 0;

  pthread_mutex_lock(&stm->mutex);

  /* Call _poll_descriptors_revents() even if we don't use it
     to let underlying plugins clear null events.  Otherwise poll()
     may wake up again and again, producing unnecessary CPU usage. */
  WRAP(snd_pcm_poll_descriptors_revents)
  (stm->pcm, stm->fds, stm->nfds, &revents);

  avail = WRAP(snd_pcm_avail_update)(stm->pcm);

  /* Got null event? Bail and wait for another wakeup. */
  if (avail == 0) {
    pthread_mutex_unlock(&stm->mutex);
    return RUNNING;
  }

  /* This could happen if we were suspended with SIGSTOP/Ctrl+Z for a long time.
   */
  if ((unsigned int)avail > stm->buffer_size) {
    avail = stm->buffer_size;
  }

  /* Capture: Read available frames */
  if (stm->stream_type == SND_PCM_STREAM_CAPTURE && avail > 0) {
    snd_pcm_sframes_t got;

    if (avail + stm->bufframes > stm->buffer_size) {
      /* Buffer overflow. Skip and overwrite with new data. */
      stm->bufframes = 0;
      // TODO: should it be marked as DRAINING?
    }

    got = WRAP(snd_pcm_readi)(stm->pcm, stm->buffer + stm->bufframes, avail);

    if (got < 0) {
      avail = got; // the error handler below will recover us
    } else {
      stm->bufframes += got;
      stm->stream_position += got;

      gettimeofday(&stm->last_activity, NULL);
    }
  }

  /* Capture: Pass read frames to callback function */
  if (stm->stream_type == SND_PCM_STREAM_CAPTURE && stm->bufframes > 0 &&
      (!stm->other_stream ||
       stm->other_stream->bufframes < stm->other_stream->buffer_size)) {
    snd_pcm_sframes_t wrote = stm->bufframes;
    struct cubeb_stream * mainstm = stm->other_stream ? stm->other_stream : stm;
    void * other_buffer = stm->other_stream ? stm->other_stream->buffer +
                                                  stm->other_stream->bufframes
                                            : NULL;

    /* Correct write size to the other stream available space */
    if (stm->other_stream &&
        wrote > (snd_pcm_sframes_t)(stm->other_stream->buffer_size -
                                    stm->other_stream->bufframes)) {
      wrote = stm->other_stream->buffer_size - stm->other_stream->bufframes;
    }

    pthread_mutex_unlock(&stm->mutex);
    wrote = stm->data_callback(mainstm, stm->user_ptr, stm->buffer,
                               other_buffer, wrote);
    pthread_mutex_lock(&stm->mutex);

    if (wrote < 0) {
      avail = wrote; // the error handler below will recover us
    } else {
      stream_buffer_decrement(stm, wrote);

      if (stm->other_stream) {
        stm->other_stream->bufframes += wrote;
      }
    }
  }

  /* Playback: Don't have enough data? Let's ask for more. */
  if (stm->stream_type == SND_PCM_STREAM_PLAYBACK &&
      avail > (snd_pcm_sframes_t)stm->bufframes &&
      (!stm->other_stream || stm->other_stream->bufframes > 0)) {
    long got = avail - stm->bufframes;
    void * other_buffer = stm->other_stream ? stm->other_stream->buffer : NULL;
    char * buftail =
        stm->buffer + WRAP(snd_pcm_frames_to_bytes)(stm->pcm, stm->bufframes);

    /* Correct read size to the other stream available frames */
    if (stm->other_stream &&
        got > (snd_pcm_sframes_t)stm->other_stream->bufframes) {
      got = stm->other_stream->bufframes;
    }

    pthread_mutex_unlock(&stm->mutex);
    got = stm->data_callback(stm, stm->user_ptr, other_buffer, buftail, got);
    pthread_mutex_lock(&stm->mutex);

    if (got < 0) {
      avail = got; // the error handler below will recover us
    } else {
      stm->bufframes += got;

      if (stm->other_stream) {
        stream_buffer_decrement(stm->other_stream, got);
      }
    }
  }

  /* Playback: Still don't have enough data? Add some silence. */
  if (stm->stream_type == SND_PCM_STREAM_PLAYBACK &&
      avail > (snd_pcm_sframes_t)stm->bufframes) {
    long drain_frames = avail - stm->bufframes;
    double drain_time = (double)drain_frames / stm->params.rate;

    char * buftail =
        stm->buffer + WRAP(snd_pcm_frames_to_bytes)(stm->pcm, stm->bufframes);
    memset(buftail, 0, WRAP(snd_pcm_frames_to_bytes)(stm->pcm, drain_frames));
    stm->bufframes = avail;

    /* Mark as draining, unless we're waiting for capture */
    if (!stm->other_stream || stm->other_stream->bufframes > 0) {
      set_timeout(&stm->drain_timeout, drain_time * 1000);

      draining = 1;
    }
  }

  /* Playback: Have enough data and no errors. Let's write it out. */
  if (stm->stream_type == SND_PCM_STREAM_PLAYBACK && avail > 0) {
    snd_pcm_sframes_t wrote;

    if (stm->params.format == CUBEB_SAMPLE_FLOAT32NE) {
      float * b = (float *)stm->buffer;
      for (uint32_t i = 0; i < avail * stm->params.channels; i++) {
        b[i] *= stm->volume;
      }
    } else {
      short * b = (short *)stm->buffer;
      for (uint32_t i = 0; i < avail * stm->params.channels; i++) {
        b[i] *= stm->volume;
      }
    }

    wrote = WRAP(snd_pcm_writei)(stm->pcm, stm->buffer, avail);
    if (wrote < 0) {
      avail = wrote; // the error handler below will recover us
    } else {
      stream_buffer_decrement(stm, wrote);

      stm->stream_position += wrote;
      gettimeofday(&stm->last_activity, NULL);
    }
  }

  /* Got some error? Let's try to recover the stream. */
  if (avail < 0) {
    avail = WRAP(snd_pcm_recover)(stm->pcm, avail, 0);

    /* Capture pcm must be started after initial setup/recover */
    if (avail >= 0 && stm->stream_type == SND_PCM_STREAM_CAPTURE &&
        WRAP(snd_pcm_state)(stm->pcm) == SND_PCM_STATE_PREPARED) {
      avail = WRAP(snd_pcm_start)(stm->pcm);
    }
  }

  /* Failed to recover, this stream must be broken. */
  if (avail < 0) {
    pthread_mutex_unlock(&stm->mutex);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    return ERROR;
  }

  pthread_mutex_unlock(&stm->mutex);
  return draining ? DRAINING : RUNNING;
}

static int
alsa_run(cubeb * ctx)
{
  int r;
  int timeout;
  int i;
  char dummy;
  cubeb_stream * stm;
  enum stream_state state;

  pthread_mutex_lock(&ctx->mutex);

  if (ctx->rebuild) {
    rebuild(ctx);
  }

  /* Wake up at least once per second for the watchdog. */
  timeout = 1000;
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    stm = ctx->streams[i];
    if (stm && stm->state == DRAINING) {
      r = ms_until(&stm->drain_timeout);
      if (r >= 0 && timeout > r) {
        timeout = r;
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);
  r = poll(ctx->fds, ctx->nfds, timeout);
  pthread_mutex_lock(&ctx->mutex);

  if (r > 0) {
    if (ctx->fds[0].revents & POLLIN) {
      if (read(ctx->control_fd_read, &dummy, 1) < 0) {
        /* ignore read error */
      }

      if (ctx->shutdown) {
        pthread_mutex_unlock(&ctx->mutex);
        return -1;
      }
    }

    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      /* We can't use snd_pcm_poll_descriptors_revents here because of
         https://github.com/kinetiknz/cubeb/issues/135. */
      if (stm && stm->state == RUNNING && stm->fds &&
          any_revents(stm->fds, stm->nfds)) {
        alsa_set_stream_state(stm, PROCESSING);
        pthread_mutex_unlock(&ctx->mutex);
        state = alsa_process_stream(stm);
        pthread_mutex_lock(&ctx->mutex);
        alsa_set_stream_state(stm, state);
      }
    }
  } else if (r == 0) {
    for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
      stm = ctx->streams[i];
      if (stm) {
        if (stm->state == DRAINING && ms_since(&stm->drain_timeout) >= 0) {
          alsa_set_stream_state(stm, INACTIVE);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
        } else if (stm->state == RUNNING &&
                   ms_since(&stm->last_activity) > CUBEB_WATCHDOG_MS) {
          alsa_set_stream_state(stm, ERROR);
          stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
        }
      }
    }
  }

  pthread_mutex_unlock(&ctx->mutex);

  return 0;
}

static void *
alsa_run_thread(void * context)
{
  cubeb * ctx = context;
  int r;

  CUBEB_REGISTER_THREAD("cubeb rendering thread");

  do {
    r = alsa_run(ctx);
  } while (r >= 0);

  CUBEB_UNREGISTER_THREAD();

  return NULL;
}

static snd_config_t *
get_slave_pcm_node(snd_config_t * lconf, snd_config_t * root_pcm)
{
  int r;
  snd_config_t * slave_pcm;
  snd_config_t * slave_def;
  snd_config_t * pcm;
  char const * string;
  char node_name[64];

  slave_def = NULL;

  r = WRAP(snd_config_search)(root_pcm, "slave", &slave_pcm);
  if (r < 0) {
    return NULL;
  }

  r = WRAP(snd_config_get_string)(slave_pcm, &string);
  if (r >= 0) {
    r = WRAP(snd_config_search_definition)(lconf, "pcm_slave", string,
                                           &slave_def);
    if (r < 0) {
      return NULL;
    }
  }

  do {
    r = WRAP(snd_config_search)(slave_def ? slave_def : slave_pcm, "pcm", &pcm);
    if (r < 0) {
      break;
    }

    r = WRAP(snd_config_get_string)(slave_def ? slave_def : slave_pcm, &string);
    if (r < 0) {
      break;
    }

    r = snprintf(node_name, sizeof(node_name), "pcm.%s", string);
    if (r < 0 || r > (int)sizeof(node_name)) {
      break;
    }
    r = WRAP(snd_config_search)(lconf, node_name, &pcm);
    if (r < 0) {
      break;
    }

    return pcm;
  } while (0);

  if (slave_def) {
    WRAP(snd_config_delete)(slave_def);
  }

  return NULL;
}

/* Work around PulseAudio ALSA plugin bug where the PA server forces a
   higher than requested latency, but the plugin does not update its (and
   ALSA's) internal state to reflect that, leading to an immediate underrun
   situation.  Inspired by WINE's make_handle_underrun_config.
   Reference: http://mailman.alsa-project.org/pipermail/alsa-devel/2012-July/05
 */
static snd_config_t *
init_local_config_with_workaround(char const * pcm_name)
{
  int r;
  snd_config_t * lconf;
  snd_config_t * pcm_node;
  snd_config_t * node;
  char const * string;
  char node_name[64];

  lconf = NULL;

  if (WRAP(snd_config) == NULL) {
    return NULL;
  }

  r = WRAP(snd_config_copy)(&lconf, WRAP(snd_config));
  if (r < 0) {
    return NULL;
  }

  do {
    r = WRAP(snd_config_search_definition)(lconf, "pcm", pcm_name, &pcm_node);
    if (r < 0) {
      break;
    }

    r = WRAP(snd_config_get_id)(pcm_node, &string);
    if (r < 0) {
      break;
    }

    r = snprintf(node_name, sizeof(node_name), "pcm.%s", string);
    if (r < 0 || r > (int)sizeof(node_name)) {
      break;
    }
    r = WRAP(snd_config_search)(lconf, node_name, &pcm_node);
    if (r < 0) {
      break;
    }

    /* If this PCM has a slave, walk the slave configurations until we reach the
     * bottom. */
    while ((node = get_slave_pcm_node(lconf, pcm_node)) != NULL) {
      pcm_node = node;
    }

    /* Fetch the PCM node's type, and bail out if it's not the PulseAudio
     * plugin. */
    r = WRAP(snd_config_search)(pcm_node, "type", &node);
    if (r < 0) {
      break;
    }

    r = WRAP(snd_config_get_string)(node, &string);
    if (r < 0) {
      break;
    }

    if (strcmp(string, "pulse") != 0) {
      break;
    }

    /* Don't clobber an explicit existing handle_underrun value, set it only
       if it doesn't already exist. */
    r = WRAP(snd_config_search)(pcm_node, "handle_underrun", &node);
    if (r != -ENOENT) {
      break;
    }

    /* Disable pcm_pulse's asynchronous underrun handling. */
    r = WRAP(snd_config_imake_integer)(&node, "handle_underrun", 0);
    if (r < 0) {
      break;
    }

    r = WRAP(snd_config_add)(pcm_node, node);
    if (r < 0) {
      break;
    }

    return lconf;
  } while (0);

  WRAP(snd_config_delete)(lconf);

  return NULL;
}

static int
alsa_locked_pcm_open(snd_pcm_t ** pcm, char const * pcm_name,
                     snd_pcm_stream_t stream, snd_config_t * local_config)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  if (local_config) {
    r = WRAP(snd_pcm_open_lconf)(pcm, pcm_name, stream, SND_PCM_NONBLOCK,
                                 local_config);
  } else {
    r = WRAP(snd_pcm_open)(pcm, pcm_name, stream, SND_PCM_NONBLOCK);
  }
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
alsa_locked_pcm_close(snd_pcm_t * pcm)
{
  int r;

  pthread_mutex_lock(&cubeb_alsa_mutex);
  r = WRAP(snd_pcm_close)(pcm);
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  return r;
}

static int
alsa_register_stream(cubeb * ctx, cubeb_stream * stm)
{
  int i;

  pthread_mutex_lock(&ctx->mutex);
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    if (!ctx->streams[i]) {
      ctx->streams[i] = stm;
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mutex);

  return i == CUBEB_STREAM_MAX;
}

static void
alsa_unregister_stream(cubeb_stream * stm)
{
  cubeb * ctx;
  int i;

  ctx = stm->context;

  pthread_mutex_lock(&ctx->mutex);
  for (i = 0; i < CUBEB_STREAM_MAX; ++i) {
    if (ctx->streams[i] == stm) {
      ctx->streams[i] = NULL;
      break;
    }
  }
  pthread_mutex_unlock(&ctx->mutex);
}

static void
silent_error_handler(char const * file, int line, char const * function,
                     int err, char const * fmt, ...)
{
  (void)file;
  (void)line;
  (void)function;
  (void)err;
  (void)fmt;
}

/*static*/ int
alsa_init(cubeb ** context, char const * context_name)
{
  (void)context_name;
  void * libasound = NULL;
  cubeb * ctx;
  int r;
  int i;
  int fd[2];
  pthread_attr_t attr;
  snd_pcm_t * dummy;

  assert(context);
  *context = NULL;

#ifndef DISABLE_LIBASOUND_DLOPEN
  libasound = dlopen("libasound.so.2", RTLD_LAZY);
  if (!libasound) {
    libasound = dlopen("libasound.so", RTLD_LAZY);
    if (!libasound) {
      return CUBEB_ERROR;
    }
  }

#define LOAD(x)                                                                \
  {                                                                            \
    cubeb_##x = dlsym(libasound, #x);                                          \
    if (!cubeb_##x) {                                                          \
      dlclose(libasound);                                                      \
      return CUBEB_ERROR;                                                      \
    }                                                                          \
  }

  LIBASOUND_API_VISIT(LOAD);
#undef LOAD
#endif

  pthread_mutex_lock(&cubeb_alsa_mutex);
  if (!cubeb_alsa_error_handler_set) {
    WRAP(snd_lib_error_set_handler)(silent_error_handler);
    cubeb_alsa_error_handler_set = 1;
  }
  pthread_mutex_unlock(&cubeb_alsa_mutex);

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &alsa_ops;
  ctx->libasound = libasound;

  r = pthread_mutex_init(&ctx->mutex, NULL);
  assert(r == 0);

  r = pipe(fd);
  assert(r == 0);

  for (i = 0; i < 2; ++i) {
    fcntl(fd[i], F_SETFD, fcntl(fd[i], F_GETFD) | FD_CLOEXEC);
    fcntl(fd[i], F_SETFL, fcntl(fd[i], F_GETFL) | O_NONBLOCK);
  }

  ctx->control_fd_read = fd[0];
  ctx->control_fd_write = fd[1];

  /* Force an early rebuild when alsa_run is first called to ensure fds and
     nfds have been initialized. */
  ctx->rebuild = 1;

  r = pthread_attr_init(&attr);
  assert(r == 0);

  r = pthread_attr_setstacksize(&attr, 256 * 1024);
  assert(r == 0);

  r = pthread_create(&ctx->thread, &attr, alsa_run_thread, ctx);
  assert(r == 0);

  r = pthread_attr_destroy(&attr);
  assert(r == 0);

  /* Open a dummy PCM to force the configuration space to be evaluated so that
     init_local_config_with_workaround can find and modify the default node. */
  r = alsa_locked_pcm_open(&dummy, CUBEB_ALSA_PCM_NAME, SND_PCM_STREAM_PLAYBACK,
                           NULL);
  if (r >= 0) {
    alsa_locked_pcm_close(dummy);
  }
  ctx->is_pa = 0;
  pthread_mutex_lock(&cubeb_alsa_mutex);
  ctx->local_config = init_local_config_with_workaround(CUBEB_ALSA_PCM_NAME);
  pthread_mutex_unlock(&cubeb_alsa_mutex);
  if (ctx->local_config) {
    ctx->is_pa = 1;
    r = alsa_locked_pcm_open(&dummy, CUBEB_ALSA_PCM_NAME,
                             SND_PCM_STREAM_PLAYBACK, ctx->local_config);
    /* If we got a local_config, we found a PA PCM.  If opening a PCM with that
       config fails with EINVAL, the PA PCM is too old for this workaround. */
    if (r == -EINVAL) {
      pthread_mutex_lock(&cubeb_alsa_mutex);
      WRAP(snd_config_delete)(ctx->local_config);
      pthread_mutex_unlock(&cubeb_alsa_mutex);
      ctx->local_config = NULL;
    } else if (r >= 0) {
      alsa_locked_pcm_close(dummy);
    }
  }

  *context = ctx;

  return CUBEB_OK;
}

static char const *
alsa_get_backend_id(cubeb * ctx)
{
  (void)ctx;
  return "alsa";
}

static void
alsa_destroy(cubeb * ctx)
{
  int r;

  assert(ctx);

  pthread_mutex_lock(&ctx->mutex);
  ctx->shutdown = 1;
  poll_wake(ctx);
  pthread_mutex_unlock(&ctx->mutex);

  r = pthread_join(ctx->thread, NULL);
  assert(r == 0);

  close(ctx->control_fd_read);
  close(ctx->control_fd_write);
  pthread_mutex_destroy(&ctx->mutex);
  free(ctx->fds);

  if (ctx->local_config) {
    pthread_mutex_lock(&cubeb_alsa_mutex);
    WRAP(snd_config_delete)(ctx->local_config);
    pthread_mutex_unlock(&cubeb_alsa_mutex);
  }
#ifndef DISABLE_LIBASOUND_DLOPEN
  if (ctx->libasound) {
    dlclose(ctx->libasound);
  }
#endif
  free(ctx);
}

static void
alsa_stream_destroy(cubeb_stream * stm);

static int
alsa_stream_init_single(cubeb * ctx, cubeb_stream ** stream,
                        char const * stream_name, snd_pcm_stream_t stream_type,
                        cubeb_devid deviceid,
                        cubeb_stream_params * stream_params,
                        unsigned int latency_frames,
                        cubeb_data_callback data_callback,
                        cubeb_state_callback state_callback, void * user_ptr)
{
  (void)stream_name;
  cubeb_stream * stm;
  int r;
  snd_pcm_format_t format;
  snd_pcm_uframes_t period_size;
  int latency_us = 0;
  char const * pcm_name =
      deviceid ? (char const *)deviceid : CUBEB_ALSA_PCM_NAME;

  assert(ctx && stream);

  *stream = NULL;

  if (stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  switch (stream_params->format) {
  case CUBEB_SAMPLE_S16LE:
    format = SND_PCM_FORMAT_S16_LE;
    break;
  case CUBEB_SAMPLE_S16BE:
    format = SND_PCM_FORMAT_S16_BE;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    format = SND_PCM_FORMAT_FLOAT_LE;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    format = SND_PCM_FORMAT_FLOAT_BE;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  pthread_mutex_lock(&ctx->mutex);
  if (ctx->active_streams >= CUBEB_STREAM_MAX) {
    pthread_mutex_unlock(&ctx->mutex);
    return CUBEB_ERROR;
  }
  ctx->active_streams += 1;
  pthread_mutex_unlock(&ctx->mutex);

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = ctx;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->params = *stream_params;
  stm->state = INACTIVE;
  stm->volume = 1.0;
  stm->buffer = NULL;
  stm->bufframes = 0;
  stm->stream_type = stream_type;
  stm->other_stream = NULL;

  r = pthread_mutex_init(&stm->mutex, NULL);
  assert(r == 0);

  r = pthread_cond_init(&stm->cond, NULL);
  assert(r == 0);

  r = alsa_locked_pcm_open(&stm->pcm, pcm_name, stm->stream_type,
                           ctx->local_config);
  if (r < 0) {
    alsa_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  r = WRAP(snd_pcm_nonblock)(stm->pcm, 1);
  assert(r == 0);

  latency_us = latency_frames * 1e6 / stm->params.rate;

  /* Ugly hack: the PA ALSA plugin allows buffer configurations that can't
     possibly work.  See https://bugzilla.mozilla.org/show_bug.cgi?id=761274.
     Only resort to this hack if the handle_underrun workaround failed. */
  if (!ctx->local_config && ctx->is_pa) {
    const int min_latency = 5e5;
    latency_us = latency_us < min_latency ? min_latency : latency_us;
  }

  r = WRAP(snd_pcm_set_params)(stm->pcm, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                               stm->params.channels, stm->params.rate, 1,
                               latency_us);
  if (r < 0) {
    alsa_stream_destroy(stm);
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  r = WRAP(snd_pcm_get_params)(stm->pcm, &stm->buffer_size, &period_size);
  assert(r == 0);

  /* Double internal buffer size to have enough space when waiting for the other
   * side of duplex connection */
  stm->buffer_size *= 2;
  stm->buffer =
      calloc(1, WRAP(snd_pcm_frames_to_bytes)(stm->pcm, stm->buffer_size));
  assert(stm->buffer);

  stm->nfds = WRAP(snd_pcm_poll_descriptors_count)(stm->pcm);
  assert(stm->nfds > 0);

  stm->saved_fds = calloc(stm->nfds, sizeof(struct pollfd));
  assert(stm->saved_fds);
  r = WRAP(snd_pcm_poll_descriptors)(stm->pcm, stm->saved_fds, stm->nfds);
  assert((nfds_t)r == stm->nfds);

  if (alsa_register_stream(ctx, stm) != 0) {
    alsa_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static int
alsa_stream_init(cubeb * ctx, cubeb_stream ** stream, char const * stream_name,
                 cubeb_devid input_device,
                 cubeb_stream_params * input_stream_params,
                 cubeb_devid output_device,
                 cubeb_stream_params * output_stream_params,
                 unsigned int latency_frames, cubeb_data_callback data_callback,
                 cubeb_state_callback state_callback, void * user_ptr)
{
  int result = CUBEB_OK;
  cubeb_stream *instm = NULL, *outstm = NULL;

  if (result == CUBEB_OK && input_stream_params) {
    result = alsa_stream_init_single(ctx, &instm, stream_name,
                                     SND_PCM_STREAM_CAPTURE, input_device,
                                     input_stream_params, latency_frames,
                                     data_callback, state_callback, user_ptr);
  }

  if (result == CUBEB_OK && output_stream_params) {
    result = alsa_stream_init_single(ctx, &outstm, stream_name,
                                     SND_PCM_STREAM_PLAYBACK, output_device,
                                     output_stream_params, latency_frames,
                                     data_callback, state_callback, user_ptr);
  }

  if (result == CUBEB_OK && input_stream_params && output_stream_params) {
    instm->other_stream = outstm;
    outstm->other_stream = instm;
  }

  if (result != CUBEB_OK && instm) {
    alsa_stream_destroy(instm);
  }

  *stream = outstm ? outstm : instm;

  return result;
}

static void
alsa_stream_destroy(cubeb_stream * stm)
{
  int r;
  cubeb * ctx;

  assert(stm && (stm->state == INACTIVE || stm->state == ERROR ||
                 stm->state == DRAINING));

  ctx = stm->context;

  if (stm->other_stream) {
    stm->other_stream->other_stream = NULL; // to stop infinite recursion
    alsa_stream_destroy(stm->other_stream);
  }

  pthread_mutex_lock(&stm->mutex);
  if (stm->pcm) {
    if (stm->state == DRAINING) {
      WRAP(snd_pcm_drain)(stm->pcm);
    }
    alsa_locked_pcm_close(stm->pcm);
    stm->pcm = NULL;
  }
  free(stm->saved_fds);
  pthread_mutex_unlock(&stm->mutex);
  pthread_mutex_destroy(&stm->mutex);

  r = pthread_cond_destroy(&stm->cond);
  assert(r == 0);

  alsa_unregister_stream(stm);

  pthread_mutex_lock(&ctx->mutex);
  assert(ctx->active_streams >= 1);
  ctx->active_streams -= 1;
  pthread_mutex_unlock(&ctx->mutex);

  free(stm->buffer);

  free(stm);
}

static int
alsa_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  int r;
  cubeb_stream * stm;
  snd_pcm_hw_params_t * hw_params;
  cubeb_stream_params params;
  params.rate = 44100;
  params.format = CUBEB_SAMPLE_FLOAT32NE;
  params.channels = 2;

  snd_pcm_hw_params_alloca(&hw_params);

  assert(ctx);

  r = alsa_stream_init(ctx, &stm, "", NULL, NULL, NULL, &params, 100, NULL,
                       NULL, NULL);
  if (r != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  assert(stm);

  r = WRAP(snd_pcm_hw_params_any)(stm->pcm, hw_params);
  if (r < 0) {
    return CUBEB_ERROR;
  }

  r = WRAP(snd_pcm_hw_params_get_channels_max)(hw_params, max_channels);
  if (r < 0) {
    return CUBEB_ERROR;
  }

  alsa_stream_destroy(stm);

  return CUBEB_OK;
}

static int
alsa_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  (void)ctx;
  int r, dir;
  snd_pcm_t * pcm;
  snd_pcm_hw_params_t * hw_params;

  snd_pcm_hw_params_alloca(&hw_params);

  /* get a pcm, disabling resampling, so we get a rate the
   * hardware/dmix/pulse/etc. supports. */
  r = WRAP(snd_pcm_open)(&pcm, CUBEB_ALSA_PCM_NAME, SND_PCM_STREAM_PLAYBACK,
                         SND_PCM_NO_AUTO_RESAMPLE);
  if (r < 0) {
    return CUBEB_ERROR;
  }

  r = WRAP(snd_pcm_hw_params_any)(pcm, hw_params);
  if (r < 0) {
    WRAP(snd_pcm_close)(pcm);
    return CUBEB_ERROR;
  }

  r = WRAP(snd_pcm_hw_params_get_rate)(hw_params, rate, &dir);
  if (r >= 0) {
    /* There is a default rate: use it. */
    WRAP(snd_pcm_close)(pcm);
    return CUBEB_OK;
  }

  /* Use a common rate, alsa may adjust it based on hw/etc. capabilities. */
  *rate = 44100;

  r = WRAP(snd_pcm_hw_params_set_rate_near)(pcm, hw_params, rate, NULL);
  if (r < 0) {
    WRAP(snd_pcm_close)(pcm);
    return CUBEB_ERROR;
  }

  WRAP(snd_pcm_close)(pcm);

  return CUBEB_OK;
}

static int
alsa_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                     uint32_t * latency_frames)
{
  (void)ctx;
  /* 40ms is found to be an acceptable minimum, even on a super low-end
   * machine. */
  *latency_frames = 40 * params.rate / 1000;

  return CUBEB_OK;
}

static int
alsa_stream_start(cubeb_stream * stm)
{
  cubeb * ctx;

  assert(stm);
  ctx = stm->context;

  if (stm->stream_type == SND_PCM_STREAM_PLAYBACK && stm->other_stream) {
    int r = alsa_stream_start(stm->other_stream);
    if (r != CUBEB_OK)
      return r;
  }

  pthread_mutex_lock(&stm->mutex);
  /* Capture pcm must be started after initial setup/recover */
  if (stm->stream_type == SND_PCM_STREAM_CAPTURE &&
      WRAP(snd_pcm_state)(stm->pcm) == SND_PCM_STATE_PREPARED) {
    WRAP(snd_pcm_start)(stm->pcm);
  }
  WRAP(snd_pcm_pause)(stm->pcm, 0);
  gettimeofday(&stm->last_activity, NULL);
  pthread_mutex_unlock(&stm->mutex);

  pthread_mutex_lock(&ctx->mutex);
  if (stm->state != INACTIVE) {
    pthread_mutex_unlock(&ctx->mutex);
    return CUBEB_ERROR;
  }
  alsa_set_stream_state(stm, RUNNING);
  pthread_mutex_unlock(&ctx->mutex);

  return CUBEB_OK;
}

static int
alsa_stream_stop(cubeb_stream * stm)
{
  cubeb * ctx;
  int r;

  assert(stm);
  ctx = stm->context;

  if (stm->stream_type == SND_PCM_STREAM_PLAYBACK && stm->other_stream) {
    int r = alsa_stream_stop(stm->other_stream);
    if (r != CUBEB_OK)
      return r;
  }

  pthread_mutex_lock(&ctx->mutex);
  while (stm->state == PROCESSING) {
    r = pthread_cond_wait(&stm->cond, &ctx->mutex);
    assert(r == 0);
  }

  alsa_set_stream_state(stm, INACTIVE);
  pthread_mutex_unlock(&ctx->mutex);

  pthread_mutex_lock(&stm->mutex);
  WRAP(snd_pcm_pause)(stm->pcm, 1);
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

static int
alsa_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  snd_pcm_sframes_t delay;

  assert(stm && position);

  pthread_mutex_lock(&stm->mutex);

  delay = -1;
  if (WRAP(snd_pcm_state)(stm->pcm) != SND_PCM_STATE_RUNNING ||
      WRAP(snd_pcm_delay)(stm->pcm, &delay) != 0) {
    *position = stm->last_position;
    pthread_mutex_unlock(&stm->mutex);
    return CUBEB_OK;
  }

  assert(delay >= 0);

  *position = 0;
  if (stm->stream_position >= (snd_pcm_uframes_t)delay) {
    *position = stm->stream_position - delay;
  }

  stm->last_position = *position;

  pthread_mutex_unlock(&stm->mutex);
  return CUBEB_OK;
}

static int
alsa_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  snd_pcm_sframes_t delay;
  /* This function returns the delay in frames until a frame written using
     snd_pcm_writei is sent to the DAC. The DAC delay should be < 1ms anyways.
   */
  if (WRAP(snd_pcm_delay)(stm->pcm, &delay)) {
    return CUBEB_ERROR;
  }

  *latency = delay;

  return CUBEB_OK;
}

static int
alsa_stream_set_volume(cubeb_stream * stm, float volume)
{
  /* setting the volume using an API call does not seem very stable/supported */
  pthread_mutex_lock(&stm->mutex);
  stm->volume = volume;
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

static int
alsa_enumerate_devices(cubeb * context, cubeb_device_type type,
                       cubeb_device_collection * collection)
{
  cubeb_device_info * device = NULL;

  if (!context)
    return CUBEB_ERROR;

  uint32_t rate, max_channels;
  int r;

  r = alsa_get_preferred_sample_rate(context, &rate);
  if (r != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  r = alsa_get_max_channel_count(context, &max_channels);
  if (r != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  char const * a_name = "default";
  device = (cubeb_device_info *)calloc(1, sizeof(cubeb_device_info));
  assert(device);
  if (!device)
    return CUBEB_ERROR;

  device->device_id = a_name;
  device->devid = (cubeb_devid)device->device_id;
  device->friendly_name = a_name;
  device->group_id = a_name;
  device->vendor_name = a_name;
  device->type = type;
  device->state = CUBEB_DEVICE_STATE_ENABLED;
  device->preferred = CUBEB_DEVICE_PREF_ALL;
  device->format = CUBEB_DEVICE_FMT_S16NE;
  device->default_format = CUBEB_DEVICE_FMT_S16NE;
  device->max_channels = max_channels;
  device->min_rate = rate;
  device->max_rate = rate;
  device->default_rate = rate;
  device->latency_lo = 0;
  device->latency_hi = 0;

  collection->device = device;
  collection->count = 1;

  return CUBEB_OK;
}

static int
alsa_device_collection_destroy(cubeb * context,
                               cubeb_device_collection * collection)
{
  assert(collection->count == 1);
  (void)context;
  free(collection->device);
  return CUBEB_OK;
}

static struct cubeb_ops const alsa_ops = {
    .init = alsa_init,
    .get_backend_id = alsa_get_backend_id,
    .get_max_channel_count = alsa_get_max_channel_count,
    .get_min_latency = alsa_get_min_latency,
    .get_preferred_sample_rate = alsa_get_preferred_sample_rate,
    .enumerate_devices = alsa_enumerate_devices,
    .device_collection_destroy = alsa_device_collection_destroy,
    .destroy = alsa_destroy,
    .stream_init = alsa_stream_init,
    .stream_destroy = alsa_stream_destroy,
    .stream_start = alsa_stream_start,
    .stream_stop = alsa_stream_stop,
    .stream_get_position = alsa_stream_get_position,
    .stream_get_latency = alsa_stream_get_latency,
    .stream_get_input_latency = NULL,
    .stream_set_volume = alsa_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = NULL,
    .stream_device_destroy = NULL,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};
