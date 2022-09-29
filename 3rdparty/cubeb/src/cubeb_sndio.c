/*
 * Copyright (c) 2011 Alexandre Ratchov <alex@caoua.org>
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_tracing.h"
#include <assert.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <sndio.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(CUBEB_SNDIO_DEBUG)
#define DPR(...) fprintf(stderr, __VA_ARGS__);
#else
#define DPR(...)                                                               \
  do {                                                                         \
  } while (0)
#endif

#ifdef DISABLE_LIBSNDIO_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) (*cubeb_##x)
#define LIBSNDIO_API_VISIT(X)                                                  \
  X(sio_close)                                                                 \
  X(sio_eof)                                                                   \
  X(sio_getpar)                                                                \
  X(sio_initpar)                                                               \
  X(sio_nfds)                                                                  \
  X(sio_onmove)                                                                \
  X(sio_open)                                                                  \
  X(sio_pollfd)                                                                \
  X(sio_read)                                                                  \
  X(sio_revents)                                                               \
  X(sio_setpar)                                                                \
  X(sio_start)                                                                 \
  X(sio_stop)                                                                  \
  X(sio_write)

#define MAKE_TYPEDEF(x) static typeof(x) * cubeb_##x;
LIBSNDIO_API_VISIT(MAKE_TYPEDEF);
#undef MAKE_TYPEDEF
#endif

static struct cubeb_ops const sndio_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * libsndio;
};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * arg; /* user arg to {data,state}_cb */
  /**/
  pthread_t th;                  /* to run real-time audio i/o */
  pthread_mutex_t mtx;           /* protects hdl and pos */
  struct sio_hdl * hdl;          /* link us to sndio */
  int mode;                      /* bitmap of SIO_{PLAY,REC} */
  int active;                    /* cubec_start() called */
  int conv;                      /* need float->s16 conversion */
  unsigned char * rbuf;          /* rec data consumed from here */
  unsigned char * pbuf;          /* play data is prepared here */
  unsigned int nfr;              /* number of frames in ibuf and obuf */
  unsigned int rbpf;             /* rec bytes per frame */
  unsigned int pbpf;             /* play bytes per frame */
  unsigned int rchan;            /* number of rec channels */
  unsigned int pchan;            /* number of play channels */
  unsigned int nblks;            /* number of blocks in the buffer */
  uint64_t hwpos;                /* frame number Joe hears right now */
  uint64_t swpos;                /* number of frames produced/consumed */
  cubeb_data_callback data_cb;   /* cb to preapare data */
  cubeb_state_callback state_cb; /* cb to notify about state changes */
  float volume;                  /* current volume */
};

static void
s16_setvol(void * ptr, long nsamp, float volume)
{
  int16_t * dst = ptr;
  int32_t mult = volume * 32768;
  int32_t s;

  while (nsamp-- > 0) {
    s = *dst;
    s = (s * mult) >> 15;
    *(dst++) = s;
  }
}

static void
float_to_s16(void * ptr, long nsamp, float volume)
{
  int16_t * dst = ptr;
  float * src = ptr;
  float mult = volume * 32768;
  int s;

  while (nsamp-- > 0) {
    s = lrintf(*(src++) * mult);
    if (s < -32768)
      s = -32768;
    else if (s > 32767)
      s = 32767;
    *(dst++) = s;
  }
}

static void
s16_to_float(void * ptr, long nsamp)
{
  int16_t * src = ptr;
  float * dst = ptr;

  src += nsamp;
  dst += nsamp;
  while (nsamp-- > 0)
    *(--dst) = (1. / 32768) * *(--src);
}

static const char *
sndio_get_device()
{
#ifdef __linux__
  /*
   * On other platforms default to sndio devices,
   * so cubebs other backends can be used instead.
   */
  const char * dev = getenv("AUDIODEVICE");
  if (dev == NULL || *dev == '\0')
    return "snd/0";
  return dev;
#else
  return SIO_DEVANY;
#endif
}

static void
sndio_onmove(void * arg, int delta)
{
  cubeb_stream * s = (cubeb_stream *)arg;

  s->hwpos += delta;
}

static void *
sndio_mainloop(void * arg)
{
  struct pollfd * pfds;
  cubeb_stream * s = arg;
  int n, eof = 0, prime, nfds, events, revents, state = CUBEB_STATE_STARTED;
  size_t pstart = 0, pend = 0, rstart = 0, rend = 0;
  long nfr;

  CUBEB_REGISTER_THREAD("cubeb rendering thread");

  nfds = WRAP(sio_nfds)(s->hdl);
  pfds = calloc(nfds, sizeof(struct pollfd));
  if (pfds == NULL) {
    CUBEB_UNREGISTER_THREAD();
    return NULL;
  }

  DPR("sndio_mainloop()\n");
  s->state_cb(s, s->arg, CUBEB_STATE_STARTED);
  pthread_mutex_lock(&s->mtx);
  if (!WRAP(sio_start)(s->hdl)) {
    pthread_mutex_unlock(&s->mtx);
    free(pfds);
    CUBEB_UNREGISTER_THREAD();
    return NULL;
  }
  DPR("sndio_mainloop(), started\n");

  if (s->mode & SIO_PLAY) {
    pstart = pend = s->nfr * s->pbpf;
    prime = s->nblks;
    if (s->mode & SIO_REC) {
      memset(s->rbuf, 0, s->nfr * s->rbpf);
      rstart = rend = s->nfr * s->rbpf;
    }
  } else {
    prime = 0;
    rstart = 0;
    rend = s->nfr * s->rbpf;
  }

  for (;;) {
    if (!s->active) {
      DPR("sndio_mainloop() stopped\n");
      state = CUBEB_STATE_STOPPED;
      break;
    }

    /* do we have a complete block? */
    if ((!(s->mode & SIO_PLAY) || pstart == pend) &&
        (!(s->mode & SIO_REC) || rstart == rend)) {

      if (eof) {
        DPR("sndio_mainloop() drained\n");
        state = CUBEB_STATE_DRAINED;
        break;
      }

      if ((s->mode & SIO_REC) && s->conv)
        s16_to_float(s->rbuf, s->nfr * s->rchan);

      /* invoke call-back, it returns less that s->nfr if done */
      pthread_mutex_unlock(&s->mtx);
      nfr = s->data_cb(s, s->arg, s->rbuf, s->pbuf, s->nfr);
      pthread_mutex_lock(&s->mtx);
      if (nfr < 0) {
        DPR("sndio_mainloop() cb err\n");
        state = CUBEB_STATE_ERROR;
        break;
      }
      s->swpos += nfr;

      /* was this last call-back invocation (aka end-of-stream) ? */
      if (nfr < s->nfr) {

        if (!(s->mode & SIO_PLAY) || nfr == 0) {
          state = CUBEB_STATE_DRAINED;
          break;
        }

        /* need to write (aka drain) the partial play block we got */
        pend = nfr * s->pbpf;
        eof = 1;
      }

      if (prime > 0)
        prime--;

      if (s->mode & SIO_PLAY) {
        if (s->conv)
          float_to_s16(s->pbuf, nfr * s->pchan, s->volume);
        else
          s16_setvol(s->pbuf, nfr * s->pchan, s->volume);
      }

      if (s->mode & SIO_REC)
        rstart = 0;
      if (s->mode & SIO_PLAY)
        pstart = 0;
    }

    events = 0;
    if ((s->mode & SIO_REC) && rstart < rend && prime == 0)
      events |= POLLIN;
    if ((s->mode & SIO_PLAY) && pstart < pend)
      events |= POLLOUT;
    nfds = WRAP(sio_pollfd)(s->hdl, pfds, events);

    if (nfds > 0) {
      pthread_mutex_unlock(&s->mtx);
      n = poll(pfds, nfds, -1);
      pthread_mutex_lock(&s->mtx);
      if (n < 0)
        continue;
    }

    revents = WRAP(sio_revents)(s->hdl, pfds);

    if (revents & POLLHUP) {
      state = CUBEB_STATE_ERROR;
      break;
    }

    if (revents & POLLOUT) {
      n = WRAP(sio_write)(s->hdl, s->pbuf + pstart, pend - pstart);
      if (n == 0 && WRAP(sio_eof)(s->hdl)) {
        DPR("sndio_mainloop() werr\n");
        state = CUBEB_STATE_ERROR;
        break;
      }
      pstart += n;
    }

    if (revents & POLLIN) {
      n = WRAP(sio_read)(s->hdl, s->rbuf + rstart, rend - rstart);
      if (n == 0 && WRAP(sio_eof)(s->hdl)) {
        DPR("sndio_mainloop() rerr\n");
        state = CUBEB_STATE_ERROR;
        break;
      }
      rstart += n;
    }

    /* skip rec block, if not recording (yet) */
    if (prime > 0 && (s->mode & SIO_REC))
      rstart = rend;
  }
  WRAP(sio_stop)(s->hdl);
  s->hwpos = s->swpos;
  pthread_mutex_unlock(&s->mtx);
  s->state_cb(s, s->arg, state);
  free(pfds);
  CUBEB_UNREGISTER_THREAD();
  return NULL;
}

/*static*/ int
sndio_init(cubeb ** context, char const * context_name)
{
  void * libsndio = NULL;
  struct sio_hdl * hdl;

  assert(context);

#ifndef DISABLE_LIBSNDIO_DLOPEN
  libsndio = dlopen("libsndio.so.7.0", RTLD_LAZY);
  if (!libsndio) {
    libsndio = dlopen("libsndio.so", RTLD_LAZY);
    if (!libsndio) {
      DPR("sndio_init(%s) failed dlopen(libsndio.so)\n", context_name);
      return CUBEB_ERROR;
    }
  }

#define LOAD(x)                                                                \
  {                                                                            \
    cubeb_##x = dlsym(libsndio, #x);                                           \
    if (!cubeb_##x) {                                                          \
      DPR("sndio_init(%s) failed dlsym(%s)\n", context_name, #x);              \
      dlclose(libsndio);                                                       \
      return CUBEB_ERROR;                                                      \
    }                                                                          \
  }

  LIBSNDIO_API_VISIT(LOAD);
#undef LOAD
#endif

  /* test if sndio works */
  hdl = WRAP(sio_open)(sndio_get_device(), SIO_PLAY, 1);
  if (hdl == NULL) {
    return CUBEB_ERROR;
  }
  WRAP(sio_close)(hdl);

  DPR("sndio_init(%s)\n", context_name);
  *context = malloc(sizeof(**context));
  if (*context == NULL)
    return CUBEB_ERROR;
  (*context)->libsndio = libsndio;
  (*context)->ops = &sndio_ops;
  (void)context_name;
  return CUBEB_OK;
}

static char const *
sndio_get_backend_id(cubeb * context)
{
  return "sndio";
}

static void
sndio_destroy(cubeb * context)
{
  DPR("sndio_destroy()\n");
#ifndef DISABLE_LIBSNDIO_DLOPEN
  if (context->libsndio)
    dlclose(context->libsndio);
#endif
  free(context);
}

static int
sndio_stream_init(cubeb * context, cubeb_stream ** stream,
                  char const * stream_name, cubeb_devid input_device,
                  cubeb_stream_params * input_stream_params,
                  cubeb_devid output_device,
                  cubeb_stream_params * output_stream_params,
                  unsigned int latency_frames,
                  cubeb_data_callback data_callback,
                  cubeb_state_callback state_callback, void * user_ptr)
{
  cubeb_stream * s;
  struct sio_par wpar, rpar;
  cubeb_sample_format format;
  int rate;
  size_t bps;

  DPR("sndio_stream_init(%s)\n", stream_name);

  s = malloc(sizeof(cubeb_stream));
  if (s == NULL)
    return CUBEB_ERROR;
  memset(s, 0, sizeof(cubeb_stream));
  s->mode = 0;
  if (input_stream_params) {
    if (input_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
      DPR("sndio_stream_init(), loopback not supported\n");
      goto err;
    }
    s->mode |= SIO_REC;
    format = input_stream_params->format;
    rate = input_stream_params->rate;
  }
  if (output_stream_params) {
    if (output_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
      DPR("sndio_stream_init(), loopback not supported\n");
      goto err;
    }
    s->mode |= SIO_PLAY;
    format = output_stream_params->format;
    rate = output_stream_params->rate;
  }
  if (s->mode == 0) {
    DPR("sndio_stream_init(), neither playing nor recording\n");
    goto err;
  }
  s->context = context;
  s->hdl = WRAP(sio_open)(sndio_get_device(), s->mode, 1);
  if (s->hdl == NULL) {
    DPR("sndio_stream_init(), sio_open() failed\n");
    goto err;
  }
  WRAP(sio_initpar)(&wpar);
  wpar.sig = 1;
  wpar.bits = 16;
  switch (format) {
  case CUBEB_SAMPLE_S16LE:
    wpar.le = 1;
    break;
  case CUBEB_SAMPLE_S16BE:
    wpar.le = 0;
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    wpar.le = SIO_LE_NATIVE;
    break;
  default:
    DPR("sndio_stream_init() unsupported format\n");
    goto err;
  }
  wpar.rate = rate;
  if (s->mode & SIO_REC)
    wpar.rchan = input_stream_params->channels;
  if (s->mode & SIO_PLAY)
    wpar.pchan = output_stream_params->channels;
  wpar.appbufsz = latency_frames;
  if (!WRAP(sio_setpar)(s->hdl, &wpar) || !WRAP(sio_getpar)(s->hdl, &rpar)) {
    DPR("sndio_stream_init(), sio_setpar() failed\n");
    goto err;
  }
  if (rpar.bits != wpar.bits || rpar.le != wpar.le || rpar.sig != wpar.sig ||
      rpar.rate != wpar.rate ||
      ((s->mode & SIO_REC) && rpar.rchan != wpar.rchan) ||
      ((s->mode & SIO_PLAY) && rpar.pchan != wpar.pchan)) {
    DPR("sndio_stream_init() unsupported params\n");
    goto err;
  }
  WRAP(sio_onmove)(s->hdl, sndio_onmove, s);
  s->active = 0;
  s->nfr = rpar.round;
  s->rbpf = rpar.bps * rpar.rchan;
  s->pbpf = rpar.bps * rpar.pchan;
  s->rchan = rpar.rchan;
  s->pchan = rpar.pchan;
  s->nblks = rpar.bufsz / rpar.round;
  s->data_cb = data_callback;
  s->state_cb = state_callback;
  s->arg = user_ptr;
  s->mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  s->hwpos = s->swpos = 0;
  if (format == CUBEB_SAMPLE_FLOAT32LE) {
    s->conv = 1;
    bps = sizeof(float);
  } else {
    s->conv = 0;
    bps = rpar.bps;
  }
  if (s->mode & SIO_PLAY) {
    s->pbuf = malloc(bps * rpar.pchan * rpar.round);
    if (s->pbuf == NULL)
      goto err;
  }
  if (s->mode & SIO_REC) {
    s->rbuf = malloc(bps * rpar.rchan * rpar.round);
    if (s->rbuf == NULL)
      goto err;
  }
  s->volume = 1.;
  *stream = s;
  DPR("sndio_stream_init() end, ok\n");
  (void)context;
  (void)stream_name;
  return CUBEB_OK;
err:
  if (s->hdl)
    WRAP(sio_close)(s->hdl);
  if (s->pbuf)
    free(s->pbuf);
  if (s->rbuf)
    free(s->pbuf);
  free(s);
  return CUBEB_ERROR;
}

static int
sndio_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);

  *max_channels = 8;

  return CUBEB_OK;
}

static int
sndio_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  /*
   * We've no device-independent prefered rate; any rate will work if
   * sndiod is running. If it isn't, 48kHz is what is most likely to
   * work as most (but not all) devices support it.
   */
  *rate = 48000;
  return CUBEB_OK;
}

static int
sndio_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                      uint32_t * latency_frames)
{
  /*
   * We've no device-independent minimum latency.
   */
  *latency_frames = 2048;

  return CUBEB_OK;
}

static void
sndio_stream_destroy(cubeb_stream * s)
{
  DPR("sndio_stream_destroy()\n");
  WRAP(sio_close)(s->hdl);
  if (s->mode & SIO_PLAY)
    free(s->pbuf);
  if (s->mode & SIO_REC)
    free(s->rbuf);
  free(s);
}

static int
sndio_stream_start(cubeb_stream * s)
{
  int err;

  DPR("sndio_stream_start()\n");
  s->active = 1;
  err = pthread_create(&s->th, NULL, sndio_mainloop, s);
  if (err) {
    s->active = 0;
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

static int
sndio_stream_stop(cubeb_stream * s)
{
  void * dummy;

  DPR("sndio_stream_stop()\n");
  if (s->active) {
    s->active = 0;
    pthread_join(s->th, &dummy);
  }
  return CUBEB_OK;
}

static int
sndio_stream_get_position(cubeb_stream * s, uint64_t * p)
{
  pthread_mutex_lock(&s->mtx);
  DPR("sndio_stream_get_position() %" PRId64 "\n", s->hwpos);
  *p = s->hwpos;
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}

static int
sndio_stream_set_volume(cubeb_stream * s, float volume)
{
  DPR("sndio_stream_set_volume(%f)\n", volume);
  pthread_mutex_lock(&s->mtx);
  if (volume < 0.)
    volume = 0.;
  else if (volume > 1.0)
    volume = 1.;
  s->volume = volume;
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}

int
sndio_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  // http://www.openbsd.org/cgi-bin/man.cgi?query=sio_open
  // in the "Measuring the latency and buffers usage" paragraph.
  *latency = stm->swpos - stm->hwpos;
  return CUBEB_OK;
}

static int
sndio_enumerate_devices(cubeb * context, cubeb_device_type type,
                        cubeb_device_collection * collection)
{
  static char dev[] = SIO_DEVANY;
  cubeb_device_info * device;

  device = malloc(sizeof(cubeb_device_info));
  if (device == NULL)
    return CUBEB_ERROR;

  device->devid = dev;         /* passed to stream_init() */
  device->device_id = dev;     /* printable in UI */
  device->friendly_name = dev; /* same, but friendly */
  device->group_id = dev;      /* actual device if full-duplex */
  device->vendor_name = NULL;  /* may be NULL */
  device->type = type;         /* Input/Output */
  device->state = CUBEB_DEVICE_STATE_ENABLED;
  device->preferred = CUBEB_DEVICE_PREF_ALL;
  device->format = CUBEB_DEVICE_FMT_S16NE;
  device->default_format = CUBEB_DEVICE_FMT_S16NE;
  device->max_channels = (type == CUBEB_DEVICE_TYPE_INPUT) ? 2 : 8;
  device->default_rate = 48000;
  device->min_rate = 4000;
  device->max_rate = 192000;
  device->latency_lo = 480;
  device->latency_hi = 9600;
  collection->device = device;
  collection->count = 1;
  return CUBEB_OK;
}

static int
sndio_device_collection_destroy(cubeb * context,
                                cubeb_device_collection * collection)
{
  free(collection->device);
  return CUBEB_OK;
}

static struct cubeb_ops const sndio_ops = {
    .init = sndio_init,
    .get_backend_id = sndio_get_backend_id,
    .get_max_channel_count = sndio_get_max_channel_count,
    .get_min_latency = sndio_get_min_latency,
    .get_preferred_sample_rate = sndio_get_preferred_sample_rate,
    .enumerate_devices = sndio_enumerate_devices,
    .device_collection_destroy = sndio_device_collection_destroy,
    .destroy = sndio_destroy,
    .stream_init = sndio_stream_init,
    .stream_destroy = sndio_stream_destroy,
    .stream_start = sndio_stream_start,
    .stream_stop = sndio_stream_stop,
    .stream_get_position = sndio_stream_get_position,
    .stream_get_latency = sndio_stream_get_latency,
    .stream_set_volume = sndio_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = NULL,
    .stream_device_destroy = NULL,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};
