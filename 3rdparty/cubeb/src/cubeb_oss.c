/*
 * Copyright © 2019-2020 Nia Alarie <nia@NetBSD.org>
 * Copyright © 2020 Ka Ho Ng <khng300@gmail.com>
 * Copyright © 2020 The FreeBSD Foundation
 *
 * Portions of this software were developed by Ka Ho Ng
 * under sponsorship from the FreeBSD Foundation.
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_mixer.h"
#include "cubeb_strings.h"
#include "cubeb_tracing.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/types.h>
#include <unistd.h>

/* Supported well by most hardware. */
#ifndef OSS_PREFER_RATE
#define OSS_PREFER_RATE (48000)
#endif

/* Standard acceptable minimum. */
#ifndef OSS_LATENCY_MS
#define OSS_LATENCY_MS (8)
#endif

#ifndef OSS_NFRAGS
#define OSS_NFRAGS (4)
#endif

#ifndef OSS_DEFAULT_DEVICE
#define OSS_DEFAULT_DEVICE "/dev/dsp"
#endif

#ifndef OSS_DEFAULT_MIXER
#define OSS_DEFAULT_MIXER "/dev/mixer"
#endif

#define ENV_AUDIO_DEVICE "AUDIO_DEVICE"

#ifndef OSS_MAX_CHANNELS
#if defined(__FreeBSD__) || defined(__DragonFly__)
/*
 * The current maximum number of channels supported
 * on FreeBSD is 8.
 *
 * Reference: FreeBSD 12.1-RELEASE
 */
#define OSS_MAX_CHANNELS (8)
#elif defined(__sun__)
/*
 * The current maximum number of channels supported
 * on Illumos is 16.
 *
 * Reference: PSARC 2008/318
 */
#define OSS_MAX_CHANNELS (16)
#else
#define OSS_MAX_CHANNELS (2)
#endif
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
#define SNDSTAT_BEGIN_STR "Installed devices:"
#define SNDSTAT_USER_BEGIN_STR "Installed devices from userspace:"
#define SNDSTAT_FV_BEGIN_STR "File Versions:"
#endif

static struct cubeb_ops const oss_ops;

struct cubeb {
  struct cubeb_ops const * ops;

  /* Our intern string store */
  pthread_mutex_t mutex; /* protects devid_strs */
  cubeb_strings * devid_strs;
};

struct oss_stream {
  oss_devnode_t name;
  int fd;
  void * buf;
  unsigned int bufframes;
  unsigned int maxframes;

  struct stream_info {
    int channels;
    int sample_rate;
    int fmt;
    int precision;
  } info;

  unsigned int frame_size; /* precision in bytes * channels */
  bool floating;
};

struct cubeb_stream {
  struct cubeb * context;
  void * user_ptr;
  pthread_t thread;
  bool doorbell;              /* (m) */
  pthread_cond_t doorbell_cv; /* (m) */
  pthread_cond_t stopped_cv;  /* (m) */
  pthread_mutex_t mtx; /* Members protected by this should be marked (m) */
  bool thread_created; /* (m) */
  bool running;        /* (m) */
  bool destroying;     /* (m) */
  cubeb_state state;   /* (m) */
  float volume /* (m) */;
  struct oss_stream play;
  struct oss_stream record;
  cubeb_data_callback data_cb;
  cubeb_state_callback state_cb;
  uint64_t frames_written /* (m) */;
};

static char const *
oss_cubeb_devid_intern(cubeb * context, char const * devid)
{
  char const * is;
  pthread_mutex_lock(&context->mutex);
  is = cubeb_strings_intern(context->devid_strs, devid);
  pthread_mutex_unlock(&context->mutex);
  return is;
}

int
oss_init(cubeb ** context, char const * context_name)
{
  cubeb * c;

  (void)context_name;
  if ((c = calloc(1, sizeof(cubeb))) == NULL) {
    return CUBEB_ERROR;
  }

  if (cubeb_strings_init(&c->devid_strs) == CUBEB_ERROR) {
    goto fail;
  }

  if (pthread_mutex_init(&c->mutex, NULL) != 0) {
    goto fail;
  }

  c->ops = &oss_ops;
  *context = c;
  return CUBEB_OK;

fail:
  cubeb_strings_destroy(c->devid_strs);
  free(c);
  return CUBEB_ERROR;
}

static void
oss_destroy(cubeb * context)
{
  pthread_mutex_destroy(&context->mutex);
  cubeb_strings_destroy(context->devid_strs);
  free(context);
}

static char const *
oss_get_backend_id(cubeb * context)
{
  return "oss";
}

static int
oss_get_preferred_sample_rate(cubeb * context, uint32_t * rate)
{
  (void)context;

  *rate = OSS_PREFER_RATE;
  return CUBEB_OK;
}

static int
oss_get_max_channel_count(cubeb * context, uint32_t * max_channels)
{
  (void)context;

  *max_channels = OSS_MAX_CHANNELS;
  return CUBEB_OK;
}

static int
oss_get_min_latency(cubeb * context, cubeb_stream_params params,
                    uint32_t * latency_frames)
{
  (void)context;

  *latency_frames = (OSS_LATENCY_MS * params.rate) / 1000;
  return CUBEB_OK;
}

static void
oss_free_cubeb_device_info_strings(cubeb_device_info * cdi)
{
  free((char *)cdi->device_id);
  free((char *)cdi->friendly_name);
  free((char *)cdi->group_id);
  cdi->device_id = NULL;
  cdi->friendly_name = NULL;
  cdi->group_id = NULL;
}

#if defined(__FreeBSD__) || defined(__DragonFly__)
/*
 * Check if the specified DSP is okay for the purpose specified
 * in type. Here type can only specify one operation each time
 * this helper is called.
 *
 * Return 0 if OK, otherwise 1.
 */
static int
oss_probe_open(const char * dsppath, cubeb_device_type type, int * fdp,
               oss_audioinfo * resai)
{
  oss_audioinfo ai;
  int error;
  int oflags = (type == CUBEB_DEVICE_TYPE_INPUT) ? O_RDONLY : O_WRONLY;
  int dspfd = open(dsppath, oflags);
  if (dspfd == -1)
    return 1;

  ai.dev = -1;
  error = ioctl(dspfd, SNDCTL_AUDIOINFO, &ai);
  if (error < 0) {
    close(dspfd);
    return 1;
  }

  if (resai)
    *resai = ai;
  if (fdp)
    *fdp = dspfd;
  else
    close(dspfd);
  return 0;
}

struct sndstat_info {
  oss_devnode_t devname;
  const char * desc;
  cubeb_device_type type;
  int preferred;
};

static int
oss_sndstat_line_parse(char * line, int is_ud, struct sndstat_info * sinfo)
{
  char *matchptr = line, *n = NULL;
  struct sndstat_info res;

  memset(&res, 0, sizeof(res));

  n = strchr(matchptr, ':');
  if (n == NULL)
    goto fail;
  if (is_ud == 0) {
    unsigned int devunit;

    if (sscanf(matchptr, "pcm%u: ", &devunit) < 1)
      goto fail;

    if (snprintf(res.devname, sizeof(res.devname), "/dev/dsp%u", devunit) < 1)
      goto fail;
  } else {
    if (n - matchptr >= (ssize_t)(sizeof(res.devname) - strlen("/dev/")))
      goto fail;

    strlcpy(res.devname, "/dev/", sizeof(res.devname));
    strncat(res.devname, matchptr, n - matchptr);
  }
  matchptr = n + 1;

  n = strchr(matchptr, '<');
  if (n == NULL)
    goto fail;
  matchptr = n + 1;
  n = strrchr(matchptr, '>');
  if (n == NULL)
    goto fail;
  *n = 0;
  res.desc = matchptr;
  matchptr = n + 1;

  n = strchr(matchptr, '(');
  if (n == NULL)
    goto fail;
  matchptr = n + 1;
  n = strrchr(matchptr, ')');
  if (n == NULL)
    goto fail;
  *n = 0;
  if (!isdigit(matchptr[0])) {
    if (strstr(matchptr, "play") != NULL)
      res.type |= CUBEB_DEVICE_TYPE_OUTPUT;
    if (strstr(matchptr, "rec") != NULL)
      res.type |= CUBEB_DEVICE_TYPE_INPUT;
  } else {
    int p, r;
    if (sscanf(matchptr, "%dp:%*dv/%dr:%*dv", &p, &r) != 2)
      goto fail;
    if (p > 0)
      res.type |= CUBEB_DEVICE_TYPE_OUTPUT;
    if (r > 0)
      res.type |= CUBEB_DEVICE_TYPE_INPUT;
  }
  matchptr = n + 1;
  if (strstr(matchptr, "default") != NULL)
    res.preferred = 1;

  *sinfo = res;
  return 0;

fail:
  return 1;
}

/*
 * XXX: On FreeBSD we have to rely on SNDCTL_CARDINFO to get all
 * the usable audio devices currently, as SNDCTL_AUDIOINFO will
 * never return directly usable audio device nodes.
 */
static int
oss_enumerate_devices(cubeb * context, cubeb_device_type type,
                      cubeb_device_collection * collection)
{
  cubeb_device_info * devinfop = NULL;
  char * line = NULL;
  size_t linecap = 0;
  FILE * sndstatfp = NULL;
  int collection_cnt = 0;
  int is_ud = 0;
  int skipall = 0;

  devinfop = calloc(1, sizeof(cubeb_device_info));
  if (devinfop == NULL)
    goto fail;

  sndstatfp = fopen("/dev/sndstat", "r");
  if (sndstatfp == NULL)
    goto fail;
  while (getline(&line, &linecap, sndstatfp) > 0) {
    const char * devid = NULL;
    struct sndstat_info sinfo;
    oss_audioinfo ai;

    if (!strncmp(line, SNDSTAT_FV_BEGIN_STR, strlen(SNDSTAT_FV_BEGIN_STR))) {
      skipall = 1;
      continue;
    }
    if (!strncmp(line, SNDSTAT_BEGIN_STR, strlen(SNDSTAT_BEGIN_STR))) {
      is_ud = 0;
      skipall = 0;
      continue;
    }
    if (!strncmp(line, SNDSTAT_USER_BEGIN_STR,
                 strlen(SNDSTAT_USER_BEGIN_STR))) {
      is_ud = 1;
      skipall = 0;
      continue;
    }
    if (skipall || isblank(line[0]))
      continue;

    if (oss_sndstat_line_parse(line, is_ud, &sinfo))
      continue;

    devinfop[collection_cnt].type = 0;
    switch (sinfo.type) {
    case CUBEB_DEVICE_TYPE_INPUT:
      if (type & CUBEB_DEVICE_TYPE_OUTPUT)
        continue;
      break;
    case CUBEB_DEVICE_TYPE_OUTPUT:
      if (type & CUBEB_DEVICE_TYPE_INPUT)
        continue;
      break;
    case 0:
      continue;
    }

    if (oss_probe_open(sinfo.devname, type, NULL, &ai))
      continue;

    devid = oss_cubeb_devid_intern(context, sinfo.devname);
    if (devid == NULL)
      continue;

    devinfop[collection_cnt].device_id = strdup(sinfo.devname);
    asprintf((char **)&devinfop[collection_cnt].friendly_name, "%s: %s",
             sinfo.devname, sinfo.desc);
    devinfop[collection_cnt].group_id = strdup(sinfo.devname);
    devinfop[collection_cnt].vendor_name = NULL;
    if (devinfop[collection_cnt].device_id == NULL ||
        devinfop[collection_cnt].friendly_name == NULL ||
        devinfop[collection_cnt].group_id == NULL) {
      oss_free_cubeb_device_info_strings(&devinfop[collection_cnt]);
      continue;
    }

    devinfop[collection_cnt].type = type;
    devinfop[collection_cnt].devid = devid;
    devinfop[collection_cnt].state = CUBEB_DEVICE_STATE_ENABLED;
    devinfop[collection_cnt].preferred =
        (sinfo.preferred) ? CUBEB_DEVICE_PREF_ALL : CUBEB_DEVICE_PREF_NONE;
    devinfop[collection_cnt].format = CUBEB_DEVICE_FMT_S16NE;
    devinfop[collection_cnt].default_format = CUBEB_DEVICE_FMT_S16NE;
    devinfop[collection_cnt].max_channels = ai.max_channels;
    devinfop[collection_cnt].default_rate = OSS_PREFER_RATE;
    devinfop[collection_cnt].max_rate = ai.max_rate;
    devinfop[collection_cnt].min_rate = ai.min_rate;
    devinfop[collection_cnt].latency_lo = 0;
    devinfop[collection_cnt].latency_hi = 0;

    collection_cnt++;

    void * newp =
        reallocarray(devinfop, collection_cnt + 1, sizeof(cubeb_device_info));
    if (newp == NULL)
      goto fail;
    devinfop = newp;
  }

  free(line);
  fclose(sndstatfp);

  collection->count = collection_cnt;
  collection->device = devinfop;

  return CUBEB_OK;

fail:
  free(line);
  if (sndstatfp)
    fclose(sndstatfp);
  free(devinfop);
  return CUBEB_ERROR;
}

#else

static int
oss_enumerate_devices(cubeb * context, cubeb_device_type type,
                      cubeb_device_collection * collection)
{
  oss_sysinfo si;
  int error, i;
  cubeb_device_info * devinfop = NULL;
  int collection_cnt = 0;
  int mixer_fd = -1;

  mixer_fd = open(OSS_DEFAULT_MIXER, O_RDWR);
  if (mixer_fd == -1) {
    LOG("Failed to open mixer %s. errno: %d", OSS_DEFAULT_MIXER, errno);
    return CUBEB_ERROR;
  }

  error = ioctl(mixer_fd, SNDCTL_SYSINFO, &si);
  if (error) {
    LOG("Failed to run SNDCTL_SYSINFO on mixer %s. errno: %d",
        OSS_DEFAULT_MIXER, errno);
    goto fail;
  }

  devinfop = calloc(si.numaudios, sizeof(cubeb_device_info));
  if (devinfop == NULL)
    goto fail;

  collection->count = 0;
  for (i = 0; i < si.numaudios; i++) {
    oss_audioinfo ai;
    cubeb_device_info cdi = {0};
    const char * devid = NULL;

    ai.dev = i;
    error = ioctl(mixer_fd, SNDCTL_AUDIOINFO, &ai);
    if (error)
      goto fail;

    assert(ai.dev < si.numaudios);
    if (!ai.enabled)
      continue;

    cdi.type = 0;
    switch (ai.caps & DSP_CAP_DUPLEX) {
    case DSP_CAP_INPUT:
      if (type & CUBEB_DEVICE_TYPE_OUTPUT)
        continue;
      break;
    case DSP_CAP_OUTPUT:
      if (type & CUBEB_DEVICE_TYPE_INPUT)
        continue;
      break;
    case 0:
      continue;
    }
    cdi.type = type;

    devid = oss_cubeb_devid_intern(context, ai.devnode);
    cdi.device_id = strdup(ai.name);
    cdi.friendly_name = strdup(ai.name);
    cdi.group_id = strdup(ai.name);
    if (devid == NULL || cdi.device_id == NULL || cdi.friendly_name == NULL ||
        cdi.group_id == NULL) {
      oss_free_cubeb_device_info_strings(&cdi);
      continue;
    }

    cdi.devid = devid;
    cdi.vendor_name = NULL;
    cdi.state = CUBEB_DEVICE_STATE_ENABLED;
    cdi.preferred = CUBEB_DEVICE_PREF_NONE;
    cdi.format = CUBEB_DEVICE_FMT_S16NE;
    cdi.default_format = CUBEB_DEVICE_FMT_S16NE;
    cdi.max_channels = ai.max_channels;
    cdi.default_rate = OSS_PREFER_RATE;
    cdi.max_rate = ai.max_rate;
    cdi.min_rate = ai.min_rate;
    cdi.latency_lo = 0;
    cdi.latency_hi = 0;

    devinfop[collection_cnt++] = cdi;
  }

  collection->count = collection_cnt;
  collection->device = devinfop;

  if (mixer_fd != -1)
    close(mixer_fd);
  return CUBEB_OK;

fail:
  if (mixer_fd != -1)
    close(mixer_fd);
  free(devinfop);
  return CUBEB_ERROR;
}

#endif

static int
oss_device_collection_destroy(cubeb * context,
                              cubeb_device_collection * collection)
{
  size_t i;
  for (i = 0; i < collection->count; i++) {
    oss_free_cubeb_device_info_strings(&collection->device[i]);
  }
  free(collection->device);
  collection->device = NULL;
  collection->count = 0;
  return 0;
}

static unsigned int
oss_chn_from_cubeb(cubeb_channel chn)
{
  switch (chn) {
  case CHANNEL_FRONT_LEFT:
    return CHID_L;
  case CHANNEL_FRONT_RIGHT:
    return CHID_R;
  case CHANNEL_FRONT_CENTER:
    return CHID_C;
  case CHANNEL_LOW_FREQUENCY:
    return CHID_LFE;
  case CHANNEL_BACK_LEFT:
    return CHID_LR;
  case CHANNEL_BACK_RIGHT:
    return CHID_RR;
  case CHANNEL_SIDE_LEFT:
    return CHID_LS;
  case CHANNEL_SIDE_RIGHT:
    return CHID_RS;
  default:
    return CHID_UNDEF;
  }
}

static unsigned long long
oss_cubeb_layout_to_chnorder(cubeb_channel_layout layout)
{
  unsigned int i, nchns = 0;
  unsigned long long chnorder = 0;

  for (i = 0; layout; i++, layout >>= 1) {
    unsigned long long chid = oss_chn_from_cubeb((layout & 1) << i);
    if (chid == CHID_UNDEF)
      continue;

    chnorder |= (chid & 0xf) << nchns * 4;
    nchns++;
  }

  return chnorder;
}

static int
oss_copy_params(int fd, cubeb_stream * stream, cubeb_stream_params * params,
                struct stream_info * sinfo)
{
  unsigned long long chnorder;

  sinfo->channels = params->channels;
  sinfo->sample_rate = params->rate;
  switch (params->format) {
  case CUBEB_SAMPLE_S16LE:
    sinfo->fmt = AFMT_S16_LE;
    sinfo->precision = 16;
    break;
  case CUBEB_SAMPLE_S16BE:
    sinfo->fmt = AFMT_S16_BE;
    sinfo->precision = 16;
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    sinfo->fmt = AFMT_S32_NE;
    sinfo->precision = 32;
    break;
  default:
    LOG("Unsupported format");
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  if (ioctl(fd, SNDCTL_DSP_CHANNELS, &sinfo->channels) == -1) {
    return CUBEB_ERROR;
  }
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &sinfo->fmt) == -1) {
    return CUBEB_ERROR;
  }
  if (ioctl(fd, SNDCTL_DSP_SPEED, &sinfo->sample_rate) == -1) {
    return CUBEB_ERROR;
  }
  /* Mono layout is an exception */
  if (params->layout != CUBEB_LAYOUT_UNDEFINED &&
      params->layout != CUBEB_LAYOUT_MONO) {
    chnorder = oss_cubeb_layout_to_chnorder(params->layout);
    if (ioctl(fd, SNDCTL_DSP_SET_CHNORDER, &chnorder) == -1)
      LOG("Non-fatal error %d occured when setting channel order.", errno);
  }
  return CUBEB_OK;
}

static int
oss_stream_stop(cubeb_stream * s)
{
  pthread_mutex_lock(&s->mtx);
  if (s->thread_created && s->running) {
    s->running = false;
    s->doorbell = false;
    pthread_cond_wait(&s->stopped_cv, &s->mtx);
  }
  if (s->state != CUBEB_STATE_STOPPED) {
    s->state = CUBEB_STATE_STOPPED;
    pthread_mutex_unlock(&s->mtx);
    s->state_cb(s, s->user_ptr, CUBEB_STATE_STOPPED);
  } else {
    pthread_mutex_unlock(&s->mtx);
  }
  return CUBEB_OK;
}

static void
oss_stream_destroy(cubeb_stream * s)
{
  pthread_mutex_lock(&s->mtx);
  if (s->thread_created) {
    s->destroying = true;
    s->doorbell = true;
    pthread_cond_signal(&s->doorbell_cv);
  }
  pthread_mutex_unlock(&s->mtx);
  pthread_join(s->thread, NULL);

  pthread_cond_destroy(&s->doorbell_cv);
  pthread_cond_destroy(&s->stopped_cv);
  pthread_mutex_destroy(&s->mtx);
  if (s->play.fd != -1) {
    close(s->play.fd);
  }
  if (s->record.fd != -1) {
    close(s->record.fd);
  }
  free(s->play.buf);
  free(s->record.buf);
  free(s);
}

static void
oss_float_to_linear32(void * buf, unsigned sample_count, float vol)
{
  float * in = buf;
  int32_t * out = buf;
  int32_t * tail = out + sample_count;

  while (out < tail) {
    int64_t f = *(in++) * vol * 0x80000000LL;
    if (f < -INT32_MAX)
      f = -INT32_MAX;
    else if (f > INT32_MAX)
      f = INT32_MAX;
    *(out++) = f;
  }
}

static void
oss_linear32_to_float(void * buf, unsigned sample_count)
{
  int32_t * in = buf;
  float * out = buf;
  float * tail = out + sample_count;

  while (out < tail) {
    *(out++) = (1.0 / 0x80000000LL) * *(in++);
  }
}

static void
oss_linear16_set_vol(int16_t * buf, unsigned sample_count, float vol)
{
  unsigned i;
  int32_t multiplier = vol * 0x8000;

  for (i = 0; i < sample_count; ++i) {
    buf[i] = (buf[i] * multiplier) >> 15;
  }
}

static int
oss_get_rec_frames(cubeb_stream * s, unsigned int nframes)
{
  size_t rem = nframes * s->record.frame_size;
  size_t read_ofs = 0;
  while (rem > 0) {
    ssize_t n;
    if ((n = read(s->record.fd, (uint8_t *)s->record.buf + read_ofs, rem)) <
        0) {
      if (errno == EINTR)
        continue;
      return CUBEB_ERROR;
    }
    read_ofs += n;
    rem -= n;
  }
  return 0;
}

static int
oss_put_play_frames(cubeb_stream * s, unsigned int nframes)
{
  size_t rem = nframes * s->play.frame_size;
  size_t write_ofs = 0;
  while (rem > 0) {
    ssize_t n;
    if ((n = write(s->play.fd, (uint8_t *)s->play.buf + write_ofs, rem)) < 0) {
      if (errno == EINTR)
        continue;
      return CUBEB_ERROR;
    }
    pthread_mutex_lock(&s->mtx);
    s->frames_written += n / s->play.frame_size;
    pthread_mutex_unlock(&s->mtx);
    write_ofs += n;
    rem -= n;
  }
  return 0;
}

static int
oss_wait_fds_for_space(cubeb_stream * s, long * nfrp)
{
  audio_buf_info bi;
  struct pollfd pfds[2];
  long nfr, tnfr;
  int i;

  assert(s->play.fd != -1 || s->record.fd != -1);
  pfds[0].events = POLLOUT | POLLHUP;
  pfds[0].revents = 0;
  pfds[0].fd = s->play.fd;
  pfds[1].events = POLLIN | POLLHUP;
  pfds[1].revents = 0;
  pfds[1].fd = s->record.fd;

retry:
  nfr = LONG_MAX;

  if (poll(pfds, 2, 1000) == -1) {
    return CUBEB_ERROR;
  }

  for (i = 0; i < 2; i++) {
    if (pfds[i].revents & POLLHUP) {
      return CUBEB_ERROR;
    }
  }

  if (s->play.fd != -1) {
    if (ioctl(s->play.fd, SNDCTL_DSP_GETOSPACE, &bi) == -1) {
      return CUBEB_STATE_ERROR;
    }
    tnfr = bi.bytes / s->play.frame_size;
    if (tnfr <= 0) {
      /* too little space - stop polling record, if any */
      pfds[0].fd = s->play.fd;
      pfds[1].fd = -1;
      goto retry;
    } else if (tnfr > (long)s->play.maxframes) {
      /* too many frames available - limit */
      tnfr = (long)s->play.maxframes;
    }
    if (nfr > tnfr) {
      nfr = tnfr;
    }
  }
  if (s->record.fd != -1) {
    if (ioctl(s->record.fd, SNDCTL_DSP_GETISPACE, &bi) == -1) {
      return CUBEB_STATE_ERROR;
    }
    tnfr = bi.bytes / s->record.frame_size;
    if (tnfr <= 0) {
      /* too little space - stop polling playback, if any */
      pfds[0].fd = -1;
      pfds[1].fd = s->record.fd;
      goto retry;
    } else if (tnfr > (long)s->record.maxframes) {
      /* too many frames available - limit */
      tnfr = (long)s->record.maxframes;
    }
    if (nfr > tnfr) {
      nfr = tnfr;
    }
  }

  *nfrp = nfr;
  return 0;
}

/* 1 - Stopped by cubeb_stream_stop, otherwise 0 */
static int
oss_audio_loop(cubeb_stream * s, cubeb_state * new_state)
{
  cubeb_state state = CUBEB_STATE_STOPPED;
  int trig = 0, drain = 0;
  const bool play_on = s->play.fd != -1, record_on = s->record.fd != -1;
  long nfr = 0;

  if (record_on) {
    if (ioctl(s->record.fd, SNDCTL_DSP_SETTRIGGER, &trig)) {
      LOG("Error %d occured when setting trigger on record fd", errno);
      state = CUBEB_STATE_ERROR;
      goto breakdown;
    }

    trig |= PCM_ENABLE_INPUT;
    memset(s->record.buf, 0, s->record.bufframes * s->record.frame_size);

    if (ioctl(s->record.fd, SNDCTL_DSP_SETTRIGGER, &trig) == -1) {
      LOG("Error %d occured when setting trigger on record fd", errno);
      state = CUBEB_STATE_ERROR;
      goto breakdown;
    }
  }

  if (!play_on && !record_on) {
    /*
     * Stop here if the stream is not play & record stream,
     * play-only stream or record-only stream
     */

    goto breakdown;
  }

  while (1) {
    pthread_mutex_lock(&s->mtx);
    if (!s->running || s->destroying) {
      pthread_mutex_unlock(&s->mtx);
      break;
    }
    pthread_mutex_unlock(&s->mtx);

    long got = 0;
    if (nfr > 0) {
      if (record_on) {
        if (oss_get_rec_frames(s, nfr) == CUBEB_ERROR) {
          state = CUBEB_STATE_ERROR;
          goto breakdown;
        }
        if (s->record.floating) {
          oss_linear32_to_float(s->record.buf, s->record.info.channels * nfr);
        }
      }

      got = s->data_cb(s, s->user_ptr, s->record.buf, s->play.buf, nfr);
      if (got == CUBEB_ERROR) {
        state = CUBEB_STATE_ERROR;
        goto breakdown;
      }
      if (got < nfr) {
        if (s->play.fd != -1) {
          drain = 1;
        } else {
          /*
           * This is a record-only stream and number of frames
           * returned from data_cb() is smaller than number
           * of frames required to read. Stop here.
           */
          state = CUBEB_STATE_STOPPED;
          goto breakdown;
        }
      }

      if (got > 0 && play_on) {
        float vol;

        pthread_mutex_lock(&s->mtx);
        vol = s->volume;
        pthread_mutex_unlock(&s->mtx);

        if (s->play.floating) {
          oss_float_to_linear32(s->play.buf, s->play.info.channels * got, vol);
        } else {
          oss_linear16_set_vol((int16_t *)s->play.buf,
                               s->play.info.channels * got, vol);
        }
        if (oss_put_play_frames(s, got) == CUBEB_ERROR) {
          state = CUBEB_STATE_ERROR;
          goto breakdown;
        }
      }
      if (drain) {
        state = CUBEB_STATE_DRAINED;
        goto breakdown;
      }
    }

    if (oss_wait_fds_for_space(s, &nfr) != 0) {
      state = CUBEB_STATE_ERROR;
      goto breakdown;
    }
  }

  return 1;

breakdown:
  pthread_mutex_lock(&s->mtx);
  *new_state = s->state = state;
  s->running = false;
  pthread_mutex_unlock(&s->mtx);
  return 0;
}

static void *
oss_io_routine(void * arg)
{
  cubeb_stream * s = arg;
  cubeb_state new_state;
  int stopped;

  CUBEB_REGISTER_THREAD("cubeb rendering thread");

  do {
    pthread_mutex_lock(&s->mtx);
    if (s->destroying) {
      pthread_mutex_unlock(&s->mtx);
      break;
    }
    pthread_mutex_unlock(&s->mtx);

    stopped = oss_audio_loop(s, &new_state);
    if (s->record.fd != -1)
      ioctl(s->record.fd, SNDCTL_DSP_HALT_INPUT, NULL);
    if (!stopped)
      s->state_cb(s, s->user_ptr, new_state);

    pthread_mutex_lock(&s->mtx);
    pthread_cond_signal(&s->stopped_cv);
    if (s->destroying) {
      pthread_mutex_unlock(&s->mtx);
      break;
    }
    while (!s->doorbell) {
      pthread_cond_wait(&s->doorbell_cv, &s->mtx);
    }
    s->doorbell = false;
    pthread_mutex_unlock(&s->mtx);
  } while (1);

  pthread_mutex_lock(&s->mtx);
  s->thread_created = false;
  pthread_mutex_unlock(&s->mtx);

  CUBEB_UNREGISTER_THREAD();

  return NULL;
}

static inline int
oss_calc_frag_shift(unsigned int frames, unsigned int frame_size)
{
  int n = 4;
  int blksize = frames * frame_size;
  while ((1 << n) < blksize) {
    n++;
  }
  return n;
}

static inline int
oss_get_frag_params(unsigned int shift)
{
  return (OSS_NFRAGS << 16) | shift;
}

static int
oss_stream_init(cubeb * context, cubeb_stream ** stream,
                char const * stream_name, cubeb_devid input_device,
                cubeb_stream_params * input_stream_params,
                cubeb_devid output_device,
                cubeb_stream_params * output_stream_params,
                unsigned int latency_frames, cubeb_data_callback data_callback,
                cubeb_state_callback state_callback, void * user_ptr)
{
  int ret = CUBEB_OK;
  cubeb_stream * s = NULL;
  const char * defdsp;

  if (!(defdsp = getenv(ENV_AUDIO_DEVICE)) || *defdsp == '\0')
    defdsp = OSS_DEFAULT_DEVICE;

  (void)stream_name;
  if ((s = calloc(1, sizeof(cubeb_stream))) == NULL) {
    ret = CUBEB_ERROR;
    goto error;
  }
  s->state = CUBEB_STATE_STOPPED;
  s->record.fd = s->play.fd = -1;
  if (input_device != NULL) {
    strlcpy(s->record.name, input_device, sizeof(s->record.name));
  } else {
    strlcpy(s->record.name, defdsp, sizeof(s->record.name));
  }
  if (output_device != NULL) {
    strlcpy(s->play.name, output_device, sizeof(s->play.name));
  } else {
    strlcpy(s->play.name, defdsp, sizeof(s->play.name));
  }
  if (input_stream_params != NULL) {
    unsigned int nb_channels;
    uint32_t minframes;

    if (input_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
      LOG("Loopback not supported");
      ret = CUBEB_ERROR_NOT_SUPPORTED;
      goto error;
    }
    nb_channels = cubeb_channel_layout_nb_channels(input_stream_params->layout);
    if (input_stream_params->layout != CUBEB_LAYOUT_UNDEFINED &&
        nb_channels != input_stream_params->channels) {
      LOG("input_stream_params->layout does not match "
          "input_stream_params->channels");
      ret = CUBEB_ERROR_INVALID_PARAMETER;
      goto error;
    }
    if ((s->record.fd = open(s->record.name, O_RDONLY)) == -1) {
      LOG("Audio device \"%s\" could not be opened as read-only",
          s->record.name);
      ret = CUBEB_ERROR_DEVICE_UNAVAILABLE;
      goto error;
    }
    if ((ret = oss_copy_params(s->record.fd, s, input_stream_params,
                               &s->record.info)) != CUBEB_OK) {
      LOG("Setting record params failed");
      goto error;
    }
    s->record.floating =
        (input_stream_params->format == CUBEB_SAMPLE_FLOAT32NE);
    s->record.frame_size =
        s->record.info.channels * (s->record.info.precision / 8);
    s->record.bufframes = latency_frames;

    oss_get_min_latency(context, *input_stream_params, &minframes);
    if (s->record.bufframes < minframes) {
      s->record.bufframes = minframes;
    }
  }
  if (output_stream_params != NULL) {
    unsigned int nb_channels;
    uint32_t minframes;

    if (output_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
      LOG("Loopback not supported");
      ret = CUBEB_ERROR_NOT_SUPPORTED;
      goto error;
    }
    nb_channels =
        cubeb_channel_layout_nb_channels(output_stream_params->layout);
    if (output_stream_params->layout != CUBEB_LAYOUT_UNDEFINED &&
        nb_channels != output_stream_params->channels) {
      LOG("output_stream_params->layout does not match "
          "output_stream_params->channels");
      ret = CUBEB_ERROR_INVALID_PARAMETER;
      goto error;
    }
    if ((s->play.fd = open(s->play.name, O_WRONLY)) == -1) {
      LOG("Audio device \"%s\" could not be opened as write-only",
          s->play.name);
      ret = CUBEB_ERROR_DEVICE_UNAVAILABLE;
      goto error;
    }
    if ((ret = oss_copy_params(s->play.fd, s, output_stream_params,
                               &s->play.info)) != CUBEB_OK) {
      LOG("Setting play params failed");
      goto error;
    }
    s->play.floating = (output_stream_params->format == CUBEB_SAMPLE_FLOAT32NE);
    s->play.frame_size = s->play.info.channels * (s->play.info.precision / 8);
    s->play.bufframes = latency_frames;

    oss_get_min_latency(context, *output_stream_params, &minframes);
    if (s->play.bufframes < minframes) {
      s->play.bufframes = minframes;
    }
  }
  if (s->play.fd != -1) {
    int frag = oss_get_frag_params(
        oss_calc_frag_shift(s->play.bufframes, s->play.frame_size));
    if (ioctl(s->play.fd, SNDCTL_DSP_SETFRAGMENT, &frag))
      LOG("Failed to set play fd with SNDCTL_DSP_SETFRAGMENT. frag: 0x%x",
          frag);
    audio_buf_info bi;
    if (ioctl(s->play.fd, SNDCTL_DSP_GETOSPACE, &bi))
      LOG("Failed to get play fd's buffer info.");
    else {
      s->play.bufframes = (bi.fragsize * bi.fragstotal) / s->play.frame_size;
    }
    int lw;

    /*
     * Force 32 ms service intervals at most, or when recording is
     * active, use the recording service intervals as a reference.
     */
    s->play.maxframes = (32 * output_stream_params->rate) / 1000;
    if (s->record.fd != -1 || s->play.maxframes >= s->play.bufframes) {
      lw = s->play.frame_size; /* Feed data when possible. */
      s->play.maxframes = s->play.bufframes;
    } else {
      lw = (s->play.bufframes - s->play.maxframes) * s->play.frame_size;
    }
    if (ioctl(s->play.fd, SNDCTL_DSP_LOW_WATER, &lw))
      LOG("Audio device \"%s\" (play) could not set trigger threshold",
          s->play.name);
  }
  if (s->record.fd != -1) {
    int frag = oss_get_frag_params(
        oss_calc_frag_shift(s->record.bufframes, s->record.frame_size));
    if (ioctl(s->record.fd, SNDCTL_DSP_SETFRAGMENT, &frag))
      LOG("Failed to set record fd with SNDCTL_DSP_SETFRAGMENT. frag: 0x%x",
          frag);
    audio_buf_info bi;
    if (ioctl(s->record.fd, SNDCTL_DSP_GETISPACE, &bi))
      LOG("Failed to get record fd's buffer info.");
    else {
      s->record.bufframes =
          (bi.fragsize * bi.fragstotal) / s->record.frame_size;
    }

    s->record.maxframes = s->record.bufframes;
    int lw = s->record.frame_size;
    if (ioctl(s->record.fd, SNDCTL_DSP_LOW_WATER, &lw))
      LOG("Audio device \"%s\" (record) could not set trigger threshold",
          s->record.name);
  }
  s->context = context;
  s->volume = 1.0;
  s->state_cb = state_callback;
  s->data_cb = data_callback;
  s->user_ptr = user_ptr;

  if (pthread_mutex_init(&s->mtx, NULL) != 0) {
    LOG("Failed to create mutex");
    goto error;
  }
  if (pthread_cond_init(&s->doorbell_cv, NULL) != 0) {
    LOG("Failed to create cv");
    goto error;
  }
  if (pthread_cond_init(&s->stopped_cv, NULL) != 0) {
    LOG("Failed to create cv");
    goto error;
  }
  s->doorbell = false;

  if (s->play.fd != -1) {
    if ((s->play.buf = calloc(s->play.bufframes, s->play.frame_size)) == NULL) {
      ret = CUBEB_ERROR;
      goto error;
    }
  }
  if (s->record.fd != -1) {
    if ((s->record.buf = calloc(s->record.bufframes, s->record.frame_size)) ==
        NULL) {
      ret = CUBEB_ERROR;
      goto error;
    }
  }

  *stream = s;
  return CUBEB_OK;
error:
  if (s != NULL) {
    oss_stream_destroy(s);
  }
  return ret;
}

static int
oss_stream_thr_create(cubeb_stream * s)
{
  if (s->thread_created) {
    s->doorbell = true;
    pthread_cond_signal(&s->doorbell_cv);
    return CUBEB_OK;
  }

  if (pthread_create(&s->thread, NULL, oss_io_routine, s) != 0) {
    LOG("Couldn't create thread");
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
oss_stream_start(cubeb_stream * s)
{
  s->state_cb(s, s->user_ptr, CUBEB_STATE_STARTED);
  pthread_mutex_lock(&s->mtx);
  /* Disallow starting an already started stream */
  assert(!s->running && s->state != CUBEB_STATE_STARTED);
  if (oss_stream_thr_create(s) != CUBEB_OK) {
    pthread_mutex_unlock(&s->mtx);
    s->state_cb(s, s->user_ptr, CUBEB_STATE_ERROR);
    return CUBEB_ERROR;
  }
  s->state = CUBEB_STATE_STARTED;
  s->thread_created = true;
  s->running = true;
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}

static int
oss_stream_get_position(cubeb_stream * s, uint64_t * position)
{
  pthread_mutex_lock(&s->mtx);
  *position = s->frames_written;
  pthread_mutex_unlock(&s->mtx);
  return CUBEB_OK;
}

static int
oss_stream_get_latency(cubeb_stream * s, uint32_t * latency)
{
  int delay;

  if (ioctl(s->play.fd, SNDCTL_DSP_GETODELAY, &delay) == -1) {
    return CUBEB_ERROR;
  }

  /* Return number of frames there */
  *latency = delay / s->play.frame_size;
  return CUBEB_OK;
}

static int
oss_stream_set_volume(cubeb_stream * stream, float volume)
{
  if (volume < 0.0)
    volume = 0.0;
  else if (volume > 1.0)
    volume = 1.0;
  pthread_mutex_lock(&stream->mtx);
  stream->volume = volume;
  pthread_mutex_unlock(&stream->mtx);
  return CUBEB_OK;
}

static int
oss_get_current_device(cubeb_stream * stream, cubeb_device ** const device)
{
  *device = calloc(1, sizeof(cubeb_device));
  if (*device == NULL) {
    return CUBEB_ERROR;
  }
  (*device)->input_name =
      stream->record.fd != -1 ? strdup(stream->record.name) : NULL;
  (*device)->output_name =
      stream->play.fd != -1 ? strdup(stream->play.name) : NULL;
  return CUBEB_OK;
}

static int
oss_stream_device_destroy(cubeb_stream * stream, cubeb_device * device)
{
  (void)stream;
  free(device->input_name);
  free(device->output_name);
  free(device);
  return CUBEB_OK;
}

static struct cubeb_ops const oss_ops = {
    .init = oss_init,
    .get_backend_id = oss_get_backend_id,
    .get_max_channel_count = oss_get_max_channel_count,
    .get_min_latency = oss_get_min_latency,
    .get_preferred_sample_rate = oss_get_preferred_sample_rate,
    .enumerate_devices = oss_enumerate_devices,
    .device_collection_destroy = oss_device_collection_destroy,
    .destroy = oss_destroy,
    .stream_init = oss_stream_init,
    .stream_destroy = oss_stream_destroy,
    .stream_start = oss_stream_start,
    .stream_stop = oss_stream_stop,
    .stream_get_position = oss_stream_get_position,
    .stream_get_latency = oss_stream_get_latency,
    .stream_get_input_latency = NULL,
    .stream_set_volume = oss_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = oss_get_current_device,
    .stream_device_destroy = oss_stream_device_destroy,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};
