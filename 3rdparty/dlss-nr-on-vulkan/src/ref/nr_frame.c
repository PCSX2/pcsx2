/*
 * nr_frame.c — `nr_frame.py` as a C library. See nr_frame.h for the contract.
 *
 * Three files of Python become one of C, and the order here follows them:
 *
 *   1. the weights: `nr_model.load_logical` (a safetensors reader) and the layout work
 *      `nr_resident.py` does on the way to the device — the attention-bias swizzle, the
 *      fused branched feed-forward, the folded global scale, the padded head;
 *   2. the graph: `nr_frame_resident.ResidentFrame._run` and every `record_*` in
 *      `nr_resident.py`, call for call, flag for flag, against libxmx's C entry points.
 *      Every dispatch the Python records, this records, in the same order with the same
 *      arguments, which is why the head comes out bit-identical;
 *   3. the frame: `nr_frame.build_features` and `nr_frame.compose`, on the passes
 *      `nr_image.c` already has.
 *
 * libxmx is reached through dlopen, from the directory this library lives in (or
 * `NR_XMX_DIR`), because it has no header and because the Python and the C must share one
 * copy of it when a test loads both into a process.
 *
 * Build: see the Makefile (`work/libnr_frame.so`). `-ffp-contract=off` is not optional:
 * the host maths here reproduces NumPy's float32 operation order, and a fused
 * multiply-add would move the last bit.
 */
#include "nr_frame.h"
#include "nr_image.h"
#include "nr_portable.h"
#include "nr_weights_embedded.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* errors                                                                      */
/* ------------------------------------------------------------------------- */

static char last_error[1536];

const char *nr_frame_error(void) { return (const char *)last_error; }

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
#define FAILF(...) do { sprintf_s(last_error, sizeof last_error, __VA_ARGS__); return -1; } while (0)
#define FAILP(...) do { sprintf_s(last_error, sizeof last_error, __VA_ARGS__); return NULL; } while (0)
#else
#define FAILF(...) do { snprintf(last_error, sizeof last_error, __VA_ARGS__); return -1; } while (0)
#define FAILP(...) do { snprintf(last_error, sizeof last_error, __VA_ARGS__); return NULL; } while (0)
#endif

/* ------------------------------------------------------------------------- */
/* libxmx, through dlopen                                                      */
/* ------------------------------------------------------------------------- */

struct xmx {
    nr_dl handle;

    char dir[1024];

    int (*open)(void);
    int (*init)(const char *);
    int (*res_init)(const char *, const char *, const char *, const char *, const char *, const char *);
    int (*portable)(void);
    int (*window_gather)(void);   /* optional: a runtime without it cannot gather */
    const char *(*error)(void);
    const char *(*device)(void);
    const char *(*path)(void);
    int (*buf_create_kind)(unsigned long long, int);
    int (*buf_host_visible)(int);
    void *(*buf_ptr)(int);
    int (*buf_upload)(int, const void *, unsigned long long, unsigned long long);
    int (*buf_download)(int, void *, unsigned long long, unsigned long long);
    int (*buf_zero)(int);
    int (*buf_destroy)(int);
    int (*begin)(void);
    int (*abort)(void);
    int (*sync)(int);
    int (*submit)(void);
    int (*rec_gemm)(int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                    unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*rec_unary)(unsigned, int, int, int, int, unsigned, unsigned, float, unsigned, unsigned,
                     unsigned, unsigned, unsigned);
    int (*rec_row)(unsigned, int, int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned, float);
    int (*rec_copy)(int, int, unsigned long long, unsigned long long, unsigned long long);
    /* the fused passes (xmx.h): every runtime exports them */
    int (*rec_gemm_residual)(int, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_window_residual)(int, int, int, int, int, unsigned, unsigned, unsigned, unsigned,
                                    unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_qkv)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_qkv_window)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned,
                               unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*rec_gemm_dual)(int, int, int, int, unsigned, unsigned, unsigned);
    int (*rec_unary2)(unsigned, int, int, int, int, int, unsigned, unsigned, float, unsigned, unsigned,
                      unsigned, unsigned, unsigned);
    int (*rec_qkv)(int, int, int, int, int, unsigned, unsigned, unsigned);
    int (*window_init)(const char *, unsigned);
    int (*rec_window_attention)(int, int, int, int, int, unsigned, unsigned, unsigned);
    int (*ffn_init)(const char *);
    int (*rec_ffn)(int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned);
    /* improve-b.md: the 32-row staged builds, the whole-row softmax, the feed-forwards that
     * make their own input, block 0's pooled residual, the window block, global attention */
    int (*staged32_init)(const char *, const char *);
    int (*rows_init)(const char *);
    int (*rec_ffn_merge)(int, int, int, int, int, int, int, unsigned, unsigned, unsigned, unsigned);
    int (*rec_ffn_stem)(int, int, int, int, int, int, unsigned, unsigned);
    int (*rec_gemm_window_residual_pool)(int, int, int, int, int, int, unsigned, unsigned, unsigned,
                                         unsigned, unsigned, unsigned, unsigned, unsigned);
    int (*window_block_init)(const char *);
    int (*rec_window_block)(int, int, int, int, int, int, int, int, unsigned, unsigned, unsigned,
                            unsigned, unsigned, unsigned);
    int (*global_attention_init)(const char *);
    int (*rec_global_attention)(int, int, int, int, unsigned, unsigned, unsigned, float);
    unsigned long long (*buf_total_bytes)(void);   /* optional: for the memory figure */
    int (*graph_capture)(void);
    int (*graph_run)(int);
    int (*graph_destroy)(int);
    size_t (*embedded_shader)(const char *);   /* optional: a Makefile-built libxmx has none */
    /* optional too: sharing a host's Vulkan device, and closing (notes on nr_frame_adopt_vulkan) */
    int (*adopt)(void *, void *, void *, void *, unsigned, int, void *,
                 void (*)(void *), void (*)(void *), void *);
    void (*close)(void);
    int (*adopted)(void);
};

static struct xmx X;

static int own_directory(char *out, size_t cap)
{
    const char *forced = getenv("NR_XMX_DIR");

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (forced && *forced) { sprintf_s(out, cap, "%s", forced); return 0; }
#else
    if (forced && *forced) { snprintf(out, cap, "%s", forced); return 0; }
#endif

    return nr_dl_self_dir((const void *)&nr_frame_open, out, cap);
}

#define BIND(field, symbol) do { X.field = nr_dl_sym(X.handle, symbol); \
    if (!X.field) FAILF("libxmx has no %s", symbol); } while (0)

#ifdef NR_STATIC_XMX
#include "xmx.h"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4152)
#endif

static int xmx_load(void)
{
    if (X.handle) return 0;
    if (own_directory(X.dir, sizeof X.dir)) FAILF("cannot locate this library's directory");
#ifdef NR_STATIC_XMX
    /* libdlssnr: the runtime's objects — libxmx's, libmetalmx's on Apple (NR_STATIC_METAL)
     * or libd3dmx's on Windows (NR_STATIC_D3D12) — are linked into this very library, so
     * there is nothing to load; the table points straight at them. */
    X.handle = (nr_dl)1;
    X.open = xmx_open; X.init = xmx_init; X.res_init = xmx_res_init;
    X.embedded_shader = xmx_embedded_shader;
    X.adopt = xmx_adopt; X.close = xmx_close; X.adopted = xmx_adopted;
    X.window_gather = xmx_window_gather;
    X.portable = xmx_portable; X.error = xmx_error; X.device = xmx_device; X.path = xmx_path;
    X.buf_create_kind = xmx_buf_create_kind; X.buf_host_visible = xmx_buf_host_visible;
    X.buf_ptr = xmx_buf_ptr; X.buf_upload = xmx_buf_upload; X.buf_download = xmx_buf_download;
    X.buf_zero = xmx_buf_zero; X.buf_destroy = xmx_buf_destroy;
    X.begin = xmx_begin; X.abort = xmx_abort; X.sync = xmx_sync; X.submit = xmx_submit;
    X.rec_gemm = xmx_rec_gemm; X.rec_unary = xmx_rec_unary; X.rec_row = xmx_rec_row;
    X.rec_copy = xmx_rec_copy;
    X.rec_gemm_residual = xmx_rec_gemm_residual; X.rec_gemm_window_residual = xmx_rec_gemm_window_residual;
    X.rec_gemm_qkv = xmx_rec_gemm_qkv; X.rec_gemm_qkv_window = xmx_rec_gemm_qkv_window;
    X.rec_gemm_dual = xmx_rec_gemm_dual; X.rec_unary2 = xmx_rec_unary2; X.rec_qkv = xmx_rec_qkv;
    X.window_init = xmx_window_init; X.rec_window_attention = xmx_rec_window_attention;
    X.ffn_init = xmx_ffn_init; X.rec_ffn = xmx_rec_ffn;
    X.staged32_init = xmx_staged32_init; X.rows_init = xmx_rows_init;
    X.rec_ffn_merge = xmx_rec_ffn_merge; X.rec_ffn_stem = xmx_rec_ffn_stem;
    X.rec_gemm_window_residual_pool = xmx_rec_gemm_window_residual_pool;
    X.window_block_init = xmx_window_block_init; X.rec_window_block = xmx_rec_window_block;
    X.global_attention_init = xmx_global_attention_init;
    X.rec_global_attention = xmx_rec_global_attention;
    X.buf_total_bytes = xmx_buf_total_bytes;
    X.graph_capture = xmx_graph_capture; X.graph_run = xmx_graph_run; X.graph_destroy = xmx_graph_destroy;
    return 0;
#else
    char path[1200];

#ifdef __APPLE__
    /* As xmx.py does, before MoltenVK is loaded: errors only, and no fast math — with it
     * on, every vendor rounding point in the graph moves (notes/phase67). */
    nr_setenv_default("MVK_CONFIG_LOG_LEVEL", "1");
    nr_setenv_default("MVK_CONFIG_FAST_MATH_ENABLED", "0");
#endif

    /* The compute runtime: libxmx (Vulkan) unless NR_GPU_BACKEND asks for libmetalmx (Metal
     * directly, built on Apple alone) or libd3dmx (Direct3D 12, built on Windows alone) —
     * the same entry points either way. */
    const char *runtime = "libxmx";
    const char *backend = getenv("NR_GPU_BACKEND");
    if (backend && !strcmp(backend, "metal")) {
#ifdef __APPLE__
        runtime = "libmetalmx";
#else
        FAILF("NR_GPU_BACKEND=metal: libmetalmx exists on macOS only");
#endif
    } else if (backend && !strcmp(backend, "d3d12")) {
#ifdef _WIN32
        runtime = "libd3dmx";
#else
        FAILF("NR_GPU_BACKEND=d3d12: libd3dmx exists on Windows only");
#endif
    } else if (backend && *backend && strcmp(backend, "vulkan")) {
        FAILF("NR_GPU_BACKEND must be 'vulkan', 'metal' or 'd3d12', not '%s'", backend);
    }

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(path, sizeof path, "%s/%s%s", X.dir, runtime, NR_SHARED_SUFFIX);
#else
    snprintf(path, sizeof path, "%s/%s%s", X.dir, runtime, NR_SHARED_SUFFIX);
#endif

    X.handle = nr_dl_open(path);
    if (!X.handle) FAILF("cannot load %s: %s", path, nr_dl_error());

    BIND(open, "xmx_open"); BIND(init, "xmx_init"); BIND(res_init, "xmx_res_init");
    X.embedded_shader = nr_dl_sym(X.handle, "xmx_embedded_shader");
    X.window_gather = nr_dl_sym(X.handle, "xmx_window_gather");
    X.adopt = nr_dl_sym(X.handle, "xmx_adopt");
    X.close = nr_dl_sym(X.handle, "xmx_close");
    X.adopted = nr_dl_sym(X.handle, "xmx_adopted");
    BIND(portable, "xmx_portable"); BIND(error, "xmx_error"); BIND(device, "xmx_device");
    BIND(path, "xmx_path");
    BIND(buf_create_kind, "xmx_buf_create_kind"); BIND(buf_host_visible, "xmx_buf_host_visible");
    BIND(buf_ptr, "xmx_buf_ptr"); BIND(buf_upload, "xmx_buf_upload");
    BIND(buf_download, "xmx_buf_download"); BIND(buf_zero, "xmx_buf_zero");
    BIND(buf_destroy, "xmx_buf_destroy");
    BIND(begin, "xmx_begin"); BIND(abort, "xmx_abort"); BIND(sync, "xmx_sync");
    BIND(submit, "xmx_submit");
    BIND(rec_gemm, "xmx_rec_gemm"); BIND(rec_unary, "xmx_rec_unary"); BIND(rec_row, "xmx_rec_row");
    BIND(rec_copy, "xmx_rec_copy");
    BIND(rec_gemm_residual, "xmx_rec_gemm_residual");
    BIND(rec_gemm_window_residual, "xmx_rec_gemm_window_residual");
    BIND(rec_gemm_qkv, "xmx_rec_gemm_qkv"); BIND(rec_gemm_qkv_window, "xmx_rec_gemm_qkv_window");
    BIND(rec_gemm_dual, "xmx_rec_gemm_dual"); BIND(rec_unary2, "xmx_rec_unary2");
    BIND(rec_qkv, "xmx_rec_qkv");
    BIND(window_init, "xmx_window_init"); BIND(rec_window_attention, "xmx_rec_window_attention");
    BIND(ffn_init, "xmx_ffn_init"); BIND(rec_ffn, "xmx_rec_ffn");
    BIND(staged32_init, "xmx_staged32_init"); BIND(rows_init, "xmx_rows_init");
    BIND(rec_ffn_merge, "xmx_rec_ffn_merge"); BIND(rec_ffn_stem, "xmx_rec_ffn_stem");
    BIND(rec_gemm_window_residual_pool, "xmx_rec_gemm_window_residual_pool");
    BIND(window_block_init, "xmx_window_block_init"); BIND(rec_window_block, "xmx_rec_window_block");
    BIND(global_attention_init, "xmx_global_attention_init");
    BIND(rec_global_attention, "xmx_rec_global_attention");
    X.buf_total_bytes = nr_dl_sym(X.handle, "xmx_buf_total_bytes");
    BIND(graph_capture, "xmx_graph_capture"); BIND(graph_run, "xmx_graph_run");
    BIND(graph_destroy, "xmx_graph_destroy");
    return 0;
#endif
}

/* The shader `xmx.py` and `xmxres.py` choose, environment overrides included: the bare
 * name when libxmx carries the module compiled in (it loads that), else the file beside
 * the library. */
static const char *spv(char *buf, size_t cap, const char *env, const char *name)
{
    const char *forced = env ? getenv(env) : NULL;
    if (forced && *forced) return forced;
    if (X.embedded_shader && X.embedded_shader(name)) return name;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(buf, cap, "%s/%s", X.dir, name);
#else
    snprintf(buf, cap, "%s/%s", X.dir, name);
#endif

    return buf;
}

/* Whether libxmx is open and its pipelines built; `nr_frame_shutdown` clears it. */
static int xmx_is_ready;

static int xmx_ready(void)
{
    if (xmx_is_ready) return 0;
    if (xmx_load()) return -1;
    if (X.open()) FAILF("xmx_open: %s", X.error());
    int portable = X.portable();
    char b[6][1200];
    if (X.init(spv(b[0], sizeof b[0], NULL, portable ? "gemm_portable_desc.spv" : "gemm_coopmat.spv")))
        FAILF("xmx_init: %s", X.error());
    if (X.res_init(spv(b[0], sizeof b[0], "XMX_GEMM_SPV", portable ? "gemm_portable.spv" : "gemm_resident.spv"),
                   spv(b[1], sizeof b[1], "XMX_UNARY_SPV", "resident.spv"),
                   spv(b[2], sizeof b[2], "XMX_ROW_SPV", "attention.spv"),
                   spv(b[3], sizeof b[3], "XMX_HISTORY_SPV", "history.spv"),
                   spv(b[4], sizeof b[4], "XMX_TILED_SPV", portable ? "gemm_portable_tiled.spv" : "gemm_tiled.spv"),
                   spv(b[5], sizeof b[5], "XMX_STAGED_SPV", portable ? "gemm_portable.spv" : "gemm_staged.spv")))
        FAILF("xmx_res_init: %s", X.error());
    /* the fused window attention (both output layouts) and the fused feed-forward, as
     * `xmxres.Runtime` names them: the `_portable` twins on a device without matrix units */
    const char *window = spv(b[0], sizeof b[0], "XMX_WINDOW_SPV",
                             portable ? "window_attention_portable.spv" : "window_attention.spv");
    if (X.window_init(window, 0) || X.window_init(window, 1))
        FAILF("xmx_window_init: %s", X.error());
    if (X.ffn_init(spv(b[1], sizeof b[1], "XMX_FFN_SPV", portable ? "ffn_fused_portable.spv" : "ffn_fused.spv")))
        FAILF("xmx_ffn_init: %s", X.error());
    /* as `xmxres._load`: the 32-row staged builds (accepted and not built by a runtime
     * without a staged kernel) and the whole-row softmax's 256-lane build */
    if (X.staged32_init(spv(b[2], sizeof b[2], "XMX_STAGED32_SPV", "gemm_staged32.spv"),
                        spv(b[3], sizeof b[3], "XMX_STAGED32_DEEP_SPV", "gemm_staged32_deep.spv")))
        FAILF("xmx_staged32_init: %s", X.error());
    if (X.rows_init(spv(b[4], sizeof b[4], "XMX_ROWS_SPV", "attention_rows.spv")))
        FAILF("xmx_rows_init: %s", X.error());
    xmx_is_ready = 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* half precision, exactly as NumPy's astype                                   */
/* ------------------------------------------------------------------------- */

static float half_to_float(uint16_t h) { return nr_half_to_float(h); }
static uint16_t float_to_half(float f) { return nr_float_to_half(f); }
static float half_round(float f) { return nr_half_round(f); }

/* ------------------------------------------------------------------------- */
/* the logical weights: a safetensors reader                                   */
/* ------------------------------------------------------------------------- */

struct tensor {
    char *name;
    int ndim;
    long shape[8];
    size_t count;
    float *data;            /* float32, as `load_logical` hands them over */
};

struct weights {
    struct tensor *t;
    size_t n;
};

/* A JSON scanner for exactly what a safetensors header holds: an object of objects with
 * string, integer-array and string-valued members. Anything else is rejected. */
struct js { const char *p, *end; };

static void js_ws(struct js *j) { while (j->p < j->end && strchr(" \t\r\n", *j->p)) j->p++; }
static int js_ch(struct js *j, char c) { js_ws(j); if (j->p < j->end && *j->p == c) { j->p++; return 1; } return 0; }

static int js_string(struct js *j, char *out, size_t cap)
{
    js_ws(j);
    if (j->p >= j->end || *j->p != '"') return -1;
    j->p++;
    size_t n = 0;
    while (j->p < j->end && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\' && j->p < j->end) c = *j->p++;    /* names carry no escapes that matter */
        if (out && n + 1 < cap) out[n] = c;
        n++;
    }
    if (j->p >= j->end) return -1;
    j->p++;
    if (out) out[n < cap ? n : cap - 1] = 0;
    return 0;
}

static int js_number(struct js *j, long long *out)
{
    js_ws(j);
    char *stop;
    errno = 0;
    long long v = strtoll(j->p, &stop, 10);
    if (stop == j->p || errno) return -1;
    j->p = stop;
    *out = v;
    return 0;
}

/* Skip any value: used for metadata values we do not read. */
static int js_skip(struct js *j)
{
    js_ws(j);
    if (j->p >= j->end) return -1;
    if (*j->p == '"') return js_string(j, NULL, 0);
    if (*j->p == '{' || *j->p == '[') {
        char open = *j->p++, close = open == '{' ? '}' : ']';
        int depth = 1;
        while (j->p < j->end && depth) {
            if (*j->p == '"') { if (js_string(j, NULL, 0)) return -1; continue; }
            if (*j->p == open) depth++;
            else if (*j->p == close) depth--;
            j->p++;
        }
        return depth ? -1 : 0;
    }
    while (j->p < j->end && !strchr(",}]", *j->p)) j->p++;
    return 0;
}

static int tensor_compare(const void *a, const void *b)
{
    return strcmp(((const struct tensor *)a)->name, ((const struct tensor *)b)->name);
}

static void weights_free(struct weights *w)
{
    for (size_t i = 0; i < w->n; i++) { free(w->t[i].name); free(w->t[i].data); }
    free(w->t);
    w->t = NULL; w->n = 0;
}

/* Where the safetensors bytes come from: a file, or the slices CMake compiled into this
 * library (nr_weights_embedded.h). The reader below asks for ranges and never for the
 * whole, so the 292 MB is converted tensor by tensor either way. */
struct source {
    FILE *f;                                    /* a file, or NULL for the embedded slices */
    const struct nr_embedded_chunk *chunks;
    size_t chunk_count;
    const char *label;                          /* the path, or "embedded weights" */
};

static int source_read(struct source *s, size_t offset, void *dst, size_t n)
{
    if (s->f) {
        if (fseek(s->f, (long)offset, SEEK_SET)) return -1;
        return fread(dst, 1, n, s->f) == n ? 0 : -1;
    }
    unsigned char *out = dst;
    size_t k = 0, start = 0;
    while (k < s->chunk_count && start + s->chunks[k].size <= offset) start += s->chunks[k++].size;
    while (n) {
        if (k == s->chunk_count) return -1;
        size_t within = offset - start;
        size_t take = s->chunks[k].size - within;
        if (take > n) take = n;
        memcpy(out, s->chunks[k].data + within, take);
        out += take; offset += take; n -= take;
        start += s->chunks[k].size; k++;
    }
    return 0;
}

static void source_close(struct source *s) { if (s->f) fclose(s->f); s->f = NULL; }

static int source_open(struct source *s, const char *path)
{
    memset(s, 0, sizeof *s);
    if (!path) {
#ifdef NR_EMBEDDED_WEIGHTS
        s->chunks = nr_embedded_weights_chunks;
        s->chunk_count = nr_embedded_weights_chunk_count;
        s->label = "embedded weights";
        return 0;
#else
        FAILF("no weights path, and this build of libnr_frame has no embedded weights (NR_EMBED_WEIGHTS with a weights directory, or NR_BIN2C_WEIGHTS and the safetensors)");
#endif
    }
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    fopen_s(&s->f, path, "rb");
#else
    s->f = fopen(path, "rb");
#endif
    if (!s->f) FAILF("cannot open %s: %s", path, strerror(errno));
    s->label = path;
    return 0;
}

size_t nr_frame_embedded_weights_size(void)
{
#ifdef NR_EMBEDDED_WEIGHTS
    return nr_embedded_weights_size;
#else
    return 0;
#endif
}

static int weights_load(struct weights *w, const char *path_or_null)
{
    struct source src;
    if (source_open(&src, path_or_null)) return -1;
    struct source *f = &src;
    const char *path = src.label;

    uint8_t lenb[8];
    if (source_read(f, 0, lenb, 8)) { source_close(f); FAILF("%s: not a safetensors file", path); }
    uint64_t hlen = 0;
    for (int i = 7; i >= 0; i--) hlen = (hlen << 8) | lenb[i];
    if (hlen > (1u << 26)) { source_close(f); FAILF("%s: header of %llu bytes", path, (unsigned long long)hlen); }
    char *header = malloc(hlen + 1);
    if (!header || source_read(f, 8, header, hlen)) { free(header); source_close(f); FAILF("%s: short header", path); }
    header[hlen] = 0;
    size_t base = 8 + hlen;

    struct js j = { header, header + hlen };
    if (!js_ch(&j, '{')) { free(header); source_close(f); FAILF("%s: header is not an object", path); }
    size_t cap = 700;
    w->t = calloc(cap, sizeof *w->t); w->n = 0;
    int logical = 0, entries = 0;
    char name[256], key[64], sval[128];
    while (!js_ch(&j, '}')) {
        /* the metadata object counts as an entry too: a comma follows it */
        if (entries++ && !js_ch(&j, ',')) goto bad;
        if (js_string(&j, name, sizeof name) || !js_ch(&j, ':')) break;
        if (!strcmp(name, "__metadata__")) {
            if (!js_ch(&j, '{')) break;
            while (!js_ch(&j, '}')) {
                js_ch(&j, ',');
                if (js_string(&j, key, sizeof key) || !js_ch(&j, ':')) goto bad;
                js_ws(&j);
                if (*j.p == '"') {
                    if (js_string(&j, sval, sizeof sval)) goto bad;
                    if (!strcmp(key, "fully_logical") && !strcmp(sval, "true")) logical = 1;
                } else if (js_skip(&j)) goto bad;
            }
            continue;
        }
        if (w->n == cap) { cap *= 2; w->t = realloc(w->t, cap * sizeof *w->t); }
        struct tensor *t = &w->t[w->n];
        memset(t, 0, sizeof *t);
        t->name = nr_strdup(name);
        int is16 = -1;
        long long off0 = -1, off1 = -1;
        if (!js_ch(&j, '{')) goto bad;
        while (!js_ch(&j, '}')) {
            js_ch(&j, ',');
            if (js_string(&j, key, sizeof key) || !js_ch(&j, ':')) goto bad;
            if (!strcmp(key, "dtype")) {
                if (js_string(&j, sval, sizeof sval)) goto bad;
                is16 = !strcmp(sval, "F16") ? 1 : !strcmp(sval, "F32") ? 0 : -1;
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
                if (is16 < 0) { sprintf_s(last_error, sizeof last_error, "%s: unsupported dtype %s", name, sval); goto fail; }
#else
                if (is16 < 0) { snprintf(last_error, sizeof last_error, "%s: unsupported dtype %s", name, sval); goto fail; }
#endif
            } else if (!strcmp(key, "shape")) {
                if (!js_ch(&j, '[')) goto bad;
                t->ndim = 0;
                while (!js_ch(&j, ']')) {
                    js_ch(&j, ',');
                    long long d;
                    if (js_number(&j, &d) || t->ndim == 8) goto bad;
                    t->shape[t->ndim++] = (long)d;
                }
            } else if (!strcmp(key, "data_offsets")) {
                if (!js_ch(&j, '[') || js_number(&j, &off0) || !js_ch(&j, ',') || js_number(&j, &off1) || !js_ch(&j, ']')) goto bad;
            } else if (js_skip(&j)) goto bad;
        }
        if (is16 < 0 || off0 < 0 || off1 < off0) goto bad;
        t->count = 1;
        for (int d = 0; d < t->ndim; d++) t->count *= (size_t)t->shape[d];
        size_t bytes = (size_t)(off1 - off0);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        if (bytes != t->count * (is16 ? 2 : 4)) { sprintf_s(last_error, sizeof last_error, "%s: %zu bytes for %zu elements", name, bytes, t->count); goto fail; }
#else
        if (bytes != t->count * (is16 ? 2 : 4)) { snprintf(last_error, sizeof last_error, "%s: %zu bytes for %zu elements", name, bytes, t->count); goto fail; }
#endif

        t->data = malloc(t->count * sizeof(float) + 4);
        if (!t->data) goto bad;
        if (is16) {
            uint16_t *raw = malloc(bytes + 2);
            if (!raw || source_read(f, base + (size_t)off0, raw, bytes)) { free(raw); goto bad; }
            for (size_t i = 0; i < t->count; i++) t->data[i] = half_to_float(raw[i]);
            free(raw);
        } else if (source_read(f, base + (size_t)off0, t->data, bytes)) goto bad;
        w->n++;
    }
    free(header);
    source_close(f);
    if (!logical) { weights_free(w); FAILF("%s: weights must declare fully_logical=true (the packed file is not a substitute)", path); }
    qsort(w->t, w->n, sizeof *w->t, tensor_compare);
    return 0;

bad:
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(last_error, sizeof last_error, "%s: malformed safetensors header near byte %ld", path, (long)(j.p - header));
#else
    snprintf(last_error, sizeof last_error, "%s: malformed safetensors header near byte %ld", path, (long)(j.p - header));
#endif

fail:
    free(header);
    source_close(f);
    weights_free(w);
    return -1;
}

static const struct tensor *weight(const struct weights *w, const char *name)
{
    struct tensor key = { .name = (char *)name };
    return bsearch(&key, w->t, w->n, sizeof *w->t, tensor_compare);
}

static const struct tensor *weightf(const struct weights *w, const char *fmt, int index)
{
    char name[128];

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(name, sizeof name, fmt, index);
#else
    snprintf(name, sizeof name, fmt, index);
#endif

    return weight(w, name);
}

/* ------------------------------------------------------------------------- */
/* device buffers                                                              */
/* ------------------------------------------------------------------------- */

enum { GRAPH = 0, HOST_READ = 1, HOST_WRITE = 2 };      /* xmxres.GRAPH / HOST_READ / HOST_WRITE */

/* A buffer holding `bytes` of `data` (or zeros): `Runtime.buffer_from`. */
static int buffer_with(const void *data, size_t bytes, int kind)
{
    int id = X.buf_create_kind(bytes ? bytes : 4, kind);
    if (id < 0) FAILF("xmx_buf_create: %s", X.error());
    if (!data) return id;
    if (X.buf_host_visible(id)) {
        memcpy(X.buf_ptr(id), data, bytes);
    } else if (X.buf_upload(id, data, 0, bytes)) {
        X.buf_destroy(id);
        FAILF("xmx_buf_upload: %s", X.error());
    }
    return id;
}

static int buffer_f16(const float *data, size_t count)
{
    uint16_t *h = malloc(count * 2 + 2);
    if (!h) FAILF("out of memory");
    for (size_t i = 0; i < count; i++) h[i] = float_to_half(data[i]);
    int id = buffer_with(h, count * 2, GRAPH);
    free(h);
    return id;
}

static int buffer_f32(const float *data, size_t count)
{
    return buffer_with(data, count * 4, GRAPH);
}

/* Host write into a HOST_WRITE buffer, host read out of a HOST_READ one:
 * `xmxres.host_write` / `host_view` for the two buffers the host touches. */
static int host_write(int id, const void *data, size_t bytes)
{
    if (X.buf_host_visible(id)) { memcpy(X.buf_ptr(id), data, bytes); return 0; }
    if (X.buf_upload(id, data, 0, bytes)) FAILF("xmx_buf_upload: %s", X.error());
    return 0;
}

static const void *host_read(int id, void *scratch, size_t bytes)
{
    if (X.buf_host_visible(id)) return X.buf_ptr(id);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (X.buf_download(id, scratch, 0, bytes)) { sprintf_s(last_error, sizeof last_error, "xmx_buf_download: %s", X.error()); return NULL; }
#else
    if (X.buf_download(id, scratch, 0, bytes)) { snprintf(last_error, sizeof last_error, "xmx_buf_download: %s", X.error()); return NULL; }
#endif

    return scratch;
}

/* ------------------------------------------------------------------------- */
/* the weights on the device, laid out as nr_resident.py lays them out         */
/* ------------------------------------------------------------------------- */

/* `nr_model.FRAGMENT_SWIZZLE_INDICES`: stored offset of every logical (query, key) entry
 * of a 64x64 window bias, undoing the fused kernel's mma fragment order. */
static int fragment_index(int entry)
{
    int query = entry / 64, key = entry % 64;
    int qy = query / 8, qx = query % 8, ky = key / 8, kx = key % 8;
#define BIT(v, p) (((v) >> (p)) & 1)
    return (BIT(qy, 2) << 11) | (BIT(qx, 2) << 10) | (BIT(ky, 2) << 9) | (BIT(kx, 2) << 8)
         | (BIT(qy, 0) << 7) | (BIT(qx, 1) << 6) | (BIT(qx, 0) << 5) | (BIT(ky, 0) << 4)
         | (BIT(kx, 1) << 3) | (BIT(ky, 1) << 2) | (BIT(qy, 1) << 1) | BIT(kx, 0);
#undef BIT
}

/* `nr_model.recovered_window_origin`: the vendor window origin (y, x) of a block. */
static void window_origin(int block, int *oy, int *ox)
{
    int phase;
    if (block == 0) phase = 0;
    else if (block <= 4) phase = block - 1;
    else if (block <= 8) phase = block - 5;
    else if (block <= 14) phase = block - 9;
    else if (block <= 22) phase = block - 15;
    else if (block <= 30) phase = block - 23;
    else if (block >= 40 && block <= 55) phase = block - (block < 48 ? 40 : 48);
    else if (block >= 56 && block <= 61) phase = block - 54;
    else if (block >= 62 && block <= 69) phase = block - (block < 66 ? 62 : 66);
    else if (block == 70) phase = 1;
    else { *oy = *ox = 0; return; }
    static const int ys[4] = { 0, -4, 0, -4 }, xs[4] = { 0, -4, -4, 0 };
    *oy = ys[phase % 4];
    *ox = xs[phase % 4];
}

enum family { WINDOW, SPLIT, GLOBAL };

struct block_w {
    int loaded;
    enum family family;
    int index, heads, channels, groups, hidden_width;
    int branched;
    int oy, ox;
    float logit_cap;
    /* device buffers; -1 where the family has none */
    int qkv, out, bias, scale, attn_cos, ffn_cos;
    int expand, branch, ffn_out;          /* window: feed-forward */
    int first, project, weight3;          /* split: the group MLP */
    int ffn_proj;                         /* global: the wide feed-forward's projection */
};

struct edge_w { int loaded; int weight0, sine, out_channels; };

#define NEED(t, w, fmt, i) const struct tensor *t = weightf(w, fmt, i); \
    if (!t) FAILF("missing weight " fmt, i)

/* `recover_attention_bias_layout` where `uses_fragment_swizzle` says so, then upload. */
static int upload_bias(const struct tensor *bias, int heads)
{
    if (bias->ndim != 3 || bias->shape[1] != 64 || bias->shape[2] != 64)
        FAILF("attention bias must be [heads, 64, 64]");
    if (!(heads == 1 || heads == 16)) return buffer_f32(bias->data, bias->count);
    float *fixed = malloc(bias->count * sizeof(float));
    if (!fixed) FAILF("out of memory");
    for (long h = 0; h < bias->shape[0]; h++)
        for (int e = 0; e < 4096; e++)
            fixed[h * 4096 + e] = bias->data[h * 4096 + fragment_index(e)];
    int id = buffer_f32(fixed, bias->count);
    free(fixed);
    return id;
}

static int load_window_block(struct block_w *b, const struct weights *w, int index, int heads)
{
    b->family = WINDOW; b->index = index; b->heads = heads;
    window_origin(index, &b->oy, &b->ox);
    NEED(proj, w, "block%d.layer0.projection_weight", index);
    b->channels = (int)proj->shape[0];
    NEED(bias, w, "block%d.layer0.attn_bias", index);
    NEED(qkv, w, "block%d.layer0.qkv_weight", index);
    NEED(scale, w, "block%d.layer0.attn_scale", index);
    NEED(acos, w, "block%d.layer0.attn_cos_skip", index);
    NEED(fcos, w, "block%d.layer0.ffn_cos_skip", index);
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->bias = upload_bias(bias, heads)) < 0) return -1;
    if ((b->scale = buffer_f32(scale->data, scale->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    const struct tensor *expand = weightf(w, "block%d.layer0.ffn_expand_weight", index);
    b->branched = expand != NULL;
    if (b->branched) {
        /* `_fused_branched_weights`: W[oh, br, ih, k, j] -> E[oh][ih*32+k][br*32+j] is one
         * (C, 128) expansion per output head; P[oh, br, k, j] -> (128, 32) contracts as
         * stored. The gate and the E4M3 publish between them are elementwise. */
        NEED(branch, w, "block%d.layer0.ffn_branch_projection_weight", index);
        NEED(ffn_out, w, "block%d.layer0.ffn_output_projection_weight", index);
        if (expand->ndim != 5 || expand->shape[1] != 4 || expand->shape[3] != 32 || expand->shape[4] != 32
            || expand->shape[0] != expand->shape[2])
            FAILF("block%d: unexpected ffn_expand_weight shape", index);
        int G = (int)expand->shape[0];
        b->groups = G;
        b->hidden_width = G * 128;
        if (b->channels != G * 32) FAILF("block%d: branched feed-forward does not match C", index);
        size_t n = (size_t)G * (G * 32) * 128;
        float *fused = malloc(n * sizeof(float));
        if (!fused) FAILF("out of memory");
        for (int oh = 0; oh < G; oh++)
            for (int br = 0; br < 4; br++)
                for (int ih = 0; ih < G; ih++)
                    for (int k = 0; k < 32; k++)
                        for (int j = 0; j < 32; j++)
                            fused[((size_t)oh * (G * 32) + ih * 32 + k) * 128 + br * 32 + j] =
                                expand->data[((((size_t)oh * 4 + br) * G + ih) * 32 + k) * 32 + j];
        b->expand = buffer_f16(fused, n);
        free(fused);
        if (b->expand < 0) return -1;
        if ((b->branch = buffer_f16(branch->data, branch->count)) < 0) return -1;
        if ((b->ffn_out = buffer_f16(ffn_out->data, ffn_out->count)) < 0) return -1;
    } else {
        NEED(w1, w, "block%d.layer0.weight1", index);
        NEED(w2, w, "block%d.layer0.weight2", index);
        b->groups = 0;
        b->hidden_width = (int)w1->shape[1];
        if ((b->expand = buffer_f16(w1->data, w1->count)) < 0) return -1;
        if ((b->branch = buffer_f16(w2->data, w2->count)) < 0) return -1;
        b->ffn_out = -1;
    }
    b->first = b->project = b->weight3 = b->ffn_proj = -1;
    b->loaded = 1;
    return 0;
}

static int load_split_block(struct block_w *b, const struct weights *w, int index)
{
    b->family = SPLIT; b->index = index; b->heads = 16;
    window_origin(index, &b->oy, &b->ox);
    NEED(proj, w, "block%d.layer3.projection_weight", index);
    b->channels = (int)proj->shape[0];
    b->groups = b->channels / 64;
    b->hidden_width = b->groups * 256;
    NEED(bias, w, "block%d.layer2.attn_bias", index);
    NEED(first, w, "block%d.layer0.first_projection_weight", index);
    NEED(expand, w, "block%d.layer0.group_expand_weight", index);
    NEED(project, w, "block%d.layer0.group_project_weight", index);
    NEED(w3, w, "block%d.layer1.weight3", index);
    NEED(fcos, w, "block%d.layer1.ffn_cos_skip", index);
    NEED(qkv, w, "block%d.layer2.qkv_weight", index);
    NEED(scale, w, "block%d.layer2.attn_scale", index);
    NEED(acos, w, "block%d.layer3.attn_cos_skip", index);
    if ((b->first = buffer_f16(first->data, first->count)) < 0) return -1;
    if ((b->expand = buffer_f16(expand->data, expand->count)) < 0) return -1;
    if ((b->project = buffer_f16(project->data, project->count)) < 0) return -1;
    if ((b->weight3 = buffer_f16(w3->data, w3->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    if ((b->scale = buffer_f32(scale->data, scale->count)) < 0) return -1;
    if ((b->bias = upload_bias(bias, 16)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    b->branch = b->ffn_out = b->ffn_proj = -1;
    b->branched = 0;
    b->loaded = 1;
    return 0;
}

static int load_global_block(struct block_w *b, const struct weights *w, int index)
{
    b->family = GLOBAL; b->index = index; b->heads = 32; b->oy = b->ox = 0;
    NEED(proj, w, "block%d.layer4.projection_weight", index);
    b->channels = (int)proj->shape[0];
    NEED(expand, w, "block%d.layer0.weight", index);
    NEED(ffn_proj, w, "block%d.layer1.weight", index);
    NEED(fcos, w, "block%d.layer1.ffn_cos_skip", index);
    NEED(qkv, w, "block%d.layer2.qkv_weight", index);
    NEED(scale, w, "block%d.layer2.attn_scale", index);
    NEED(acos, w, "block%d.layer4.attn_cos_skip", index);
    b->hidden_width = (int)expand->shape[1];
    if ((b->expand = buffer_f16(expand->data, expand->count)) < 0) return -1;
    if ((b->ffn_proj = buffer_f16(ffn_proj->data, ffn_proj->count)) < 0) return -1;
    if ((b->ffn_cos = buffer_f32(fcos->data, fcos->count)) < 0) return -1;
    if ((b->qkv = buffer_f16(qkv->data, qkv->count)) < 0) return -1;
    /* the global kernels fold sqrt(head_dim) into the per-head scale, in float32 */
    float folded[64];
    float root = (float)sqrt((double)(b->channels / b->heads));
    if (scale->count > 64) FAILF("block%d: too many heads", index);
    for (size_t i = 0; i < scale->count; i++) folded[i] = scale->data[i] * root;
    if ((b->scale = buffer_f32(folded, scale->count)) < 0) return -1;
    if ((b->out = buffer_f16(proj->data, proj->count)) < 0) return -1;
    if ((b->attn_cos = buffer_f32(acos->data, acos->count)) < 0) return -1;
    b->logit_cap = 3.0f;                  /* nr_model.GLOBAL_ATTENTION_LOGIT_CAP */
    b->bias = b->branch = b->ffn_out = b->first = b->project = b->weight3 = -1;
    b->groups = 0; b->branched = 0;
    b->loaded = 1;
    return 0;
}

static int load_edge(struct edge_w *e, const struct weights *w, int index, int up)
{
    NEED(w0, w, "block%d.layer0.weight0", index);
    if ((e->weight0 = buffer_f16(w0->data, w0->count)) < 0) return -1;
    e->out_channels = (int)w0->shape[1];
    e->sine = -1;
    if (up) {
        NEED(sine, w, "block%d.layer0.sin", index);
        if ((e->sine = buffer_f32(sine->data, sine->count)) < 0) return -1;
    }
    e->loaded = 1;
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the frame: weights, the scratch arena, and the graph for one extent          */
/* ------------------------------------------------------------------------- */

/* `xmxres.ScratchArena.ALIASES`: the roles a block's buffers alias, because their live
 * intervals are disjoint and the barriers between passes already order them. */
enum role {
    R_PROJECTION,      /* hidden16, proj, attended, attention, transition.padded, transition.projected */
    R_BRANCH_VALUE,    /* branch, v16 */
    R_INPUT_KEY,       /* value16, win16, ffn16, k16, transition.pooled16, transition.projected16 */
    R_QUERY_PROB,      /* heads16, core16, q16, probs16, merged16 */
    R_SCORES_CONTEXT,  /* scores, context, transition.upsampled */
    R_RESIDUAL,        /* ffn, transition.scaled */
    R_VALUE,           /* a block scratch's own `value`; never used by the frame */
    R_GLOBAL_VALUE,    /* the bottleneck's input, zero in its padding rows */
    R_GLOBAL_IO16,     /* the bottleneck chain's value as half, zero in its padding rows */
    R_GLOBAL_OUT,
    R_OUT,
    R_COUNT
};

#define MAX_NAMED 48

struct named { char name[24]; int id; size_t bytes; };

/* One buffer the plan asked a role for: its size, and whether the recording used it. */
struct request { int role; size_t bytes; int used; };

struct nr_frame {
    struct weights w;
    /* device weights, shared across extents */
    int adapter, merge_sin, merge_cos, merge_sincos, head;
    struct edge_w bottleneck, decoder_input, edges[71];
    struct block_w blocks[71];
    /* the graph for one extent */
    int height, width;                     /* the network extent */
    int levels[7][3];
    int planning;                          /* 1: size the arena, record nothing */
    /* the planned requests, each a stand-in id while planning (`ScratchArena.discover`) */
    struct request *requests;
    int request_count, request_cap;
    size_t role_bytes[R_COUNT];
    int role_id[R_COUNT];
    struct named named[MAX_NAMED];
    int named_count;
    int graph;                             /* captured commands, or -1 */
    /* `xmxres.Runtime.__init__`: the environment switches, read once, the same defaults.
     * Each one changes which passes are recorded, and the Python and this file must agree
     * on every one of them for the head to be bit-identical. */
    struct {
        int fuse_qk, batch_ffn, joint_qkv, fuse_residual, fuse_window_residual,
            fuse_window_attention, fuse_attention_merge, qkv_epilogue, fuse_glue, fuse_ffn,
            fuse_branched_ffn, fuse_partition, input_fp16, compact_head,
            fuse_transition, fuse_merge_ffn, fuse_stem_ffn, fuse_pool, fuse_window_block,
            fuse_head, fuse_global_attention;
    } opt;
    int min_extent;                        /* the network's floor for this extent's plan */
    int head_stride;                       /* floats per pixel in what run_graph returns */
    double split[3];
    /* host scratch */
    float *features_host;                  /* (H, W, 16) at the network extent */
    float *head_host;                      /* (H, W, 16) when the head buffer is unmapped */
    float *noise;                          /* (H, W, 3) for the extent and frame index */
    int noise_index;
    int32_t *rows, *cols;
    float *composed, *scratch_a, *scratch_b;
    size_t composed_pixels;
    float *gate_table;                     /* the gate on every half logit, for `gate_scale` */
    float gate_scale;
};

static int pad8(int e) { return (e + 7) / 8 * 8; }
static int align16(int e) { return (e + 15) / 16 * 16; }
static int align64(int e) { return (e + 63) / 64 * 64; }

/* -- the arena ---------------------------------------------------------- */

static void arena_reset(struct nr_frame *f)
{
    for (int r = 0; r < R_COUNT; r++) {
        if (f->role_id[r] >= 0) X.buf_destroy(f->role_id[r]);
        f->role_id[r] = -1;
        f->role_bytes[r] = 0;
    }
}

/* `ScratchArena.buffer` and `ScratchArena.discover`. While planning each request is a
 * stand-in id of its own, recorded against the shadow recorders below, which mark every
 * stand-in a pass is handed; each role is then sized by the largest request that was
 * used — nothing, if none was. The fused paths never touch the float32 scores, the half
 * probabilities or the float32 projection, the largest buffers in the frame, so this is
 * most of the frame's memory. Afterwards the first use of a role allocates it; a request
 * the recording never used gets an id no pass will be handed (`UNPLANNED`), as the
 * Python's unused handles are never asked for their buffer. Returns the buffer id. */
#define STAND_IN 0x01000000
#define UNPLANNED 0x3fffffff

static struct nr_frame *discovering;

static int arena(struct nr_frame *f, enum role r, size_t bytes)
{
    if (f->planning) {
        if (f->request_count == f->request_cap) {
            int cap = f->request_cap ? 2 * f->request_cap : 1024;
            struct request *grown = realloc(f->requests, (size_t)cap * sizeof *grown);
            if (!grown) { snprintf(last_error, sizeof last_error, "out of memory"); return -2; }
            f->requests = grown;
            f->request_cap = cap;
        }
        f->requests[f->request_count] = (struct request){ (int)r, bytes, 0 };
        return STAND_IN + f->request_count++;
    }
    if (!bytes || bytes > f->role_bytes[r]) return UNPLANNED;
    if (f->role_id[r] < 0) {
        int id = buffer_with(NULL, f->role_bytes[r], GRAPH);
        if (id < 0) return -2;
        if ((r == R_GLOBAL_VALUE || r == R_GLOBAL_IO16) && X.buf_zero(id)) {
            snprintf(last_error, sizeof last_error, "xmx_buf_zero: %s", X.error());
            return -2;
        }
        f->role_id[r] = id;
    }
    return f->role_id[r];
}

/* the shadow recorders: every buffer id a pass is handed, marked used */
static void mark(int id)
{
    struct nr_frame *f = discovering;
    if (f && id >= STAND_IN && id < STAND_IN + f->request_count) f->requests[id - STAND_IN].used = 1;
}
static void mark4(int a, int b, int c, int d) { mark(a); mark(b); mark(c); mark(d); }

static int sh_gemm(int a, int b, int c, unsigned m, unsigned n, unsigned k, unsigned batch, unsigned sa,
                   unsigned sb, unsigned sc, unsigned bt, unsigned lda, unsigned ldb, unsigned ldc,
                   unsigned oa, unsigned ob, unsigned oc)
{ mark4(a, b, c, -1); return 0; }
static int sh_unary(unsigned kind, int a, int b, int c, int d, unsigned n, unsigned ch, float p0,
                    unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k)
{ mark4(a, b, c, d); return 0; }
static int sh_row(unsigned kind, int a, int b, int c, int d, unsigned rows, unsigned width,
                  unsigned heads, unsigned scaled, unsigned stride, float cap)
{ mark4(a, b, c, d); return 0; }
static int sh_copy(int a, int b, unsigned long long n, unsigned long long x, unsigned long long y)
{ mark4(a, b, -1, -1); return 0; }
static int sh_gemm_residual(int a, int b, int c, int skip, int cosine, unsigned m, unsigned n,
                            unsigned k, unsigned flags)
{ mark4(a, b, c, skip); mark(cosine); return 0; }
static int sh_gemm_window_residual(int a, int b, int c, int skip, int cosine, unsigned m, unsigned n,
                                   unsigned k, unsigned flags, unsigned h, unsigned w, unsigned across,
                                   unsigned pad)
{ mark4(a, b, c, skip); mark(cosine); return 0; }
static int sh_gemm_qkv(int a, int weight, int q, int k, int v, int scale, unsigned m, unsigned ch,
                       unsigned heads, unsigned tokens)
{ mark4(a, weight, q, k); mark(v); mark(scale); return 0; }
static int sh_gemm_qkv_window(int a, int weight, int q, int k, int v, int scale, unsigned m, unsigned ch,
                              unsigned heads, unsigned tokens, unsigned w, unsigned h, unsigned across,
                              unsigned pad, unsigned image_half)
{ mark4(a, weight, q, k); mark(v); mark(scale); return 0; }
static int sh_gemm_dual(int a, int b, int c, int half_copy, unsigned m, unsigned n, unsigned k)
{ mark4(a, b, c, half_copy); return 0; }
static int sh_unary2(unsigned kind, int a, int b, int c, int d, int second, unsigned n, unsigned ch,
                     float p0, unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k)
{ mark4(a, b, c, d); mark(second); return 0; }
static int sh_qkv(int source, int q, int k, int v, int scale, unsigned rows, unsigned tokens,
                  unsigned heads)
{ mark4(source, q, k, v); mark(scale); return 0; }
static int sh_window_attention(int q, int k, int v, int bias, int out, unsigned batches, unsigned heads,
                               unsigned merged)
{ mark4(q, k, v, bias); mark(out); return 0; }
static int sh_ffn(int a, int expand, int projection, int out, int skip, int cosine, unsigned m,
                  unsigned cin, unsigned hidden, unsigned groups, unsigned flags)
{ mark4(a, expand, projection, out); mark(skip); mark(cosine); return 0; }
static int sh_ffn_merge(int source, int skip, int sincos, int expand, int projection, int out, int cosine,
                        unsigned h, unsigned w, unsigned sw, unsigned flags)
{ mark4(source, skip, sincos, expand); mark(projection); mark(out); mark(cosine); return 0; }
static int sh_ffn_stem(int features, int adapter, int expand, int projection, int out, int cosine,
                       unsigned m, unsigned flags)
{ mark4(features, adapter, expand, projection); mark(out); mark(cosine); return 0; }
static int sh_gemm_window_residual_pool(int a, int b, int skip_out, int skip, int cosine, int pooled,
                                        unsigned m, unsigned n, unsigned k, unsigned flags, unsigned h,
                                        unsigned w, unsigned across, unsigned pad)
{ mark4(a, b, skip_out, skip); mark(cosine); mark(pooled); return 0; }
static int sh_window_block(int image, int qkv, int projection, int target, int bias, int cosine, int scale,
                           int pooled, unsigned windows, unsigned h, unsigned w, unsigned across,
                           unsigned pad, unsigned flags)
{ mark4(image, qkv, projection, target); mark4(bias, cosine, scale, pooled); return 0; }
static int sh_global_attention(int q, int k, int v, int merged, unsigned rows, unsigned tokens,
                               unsigned heads, float cap)
{ mark4(q, k, v, merged); return 0; }
static int sh_sync(int on) { return 0; }

/* The recorders swapped for the shadows while `f` plans, and back. */
static void shadow_recorders(struct nr_frame *f, struct xmx *saved)
{
    *saved = X;
    discovering = f;
    X.rec_gemm = sh_gemm; X.rec_unary = sh_unary; X.rec_row = sh_row; X.rec_copy = sh_copy;
    X.rec_gemm_residual = sh_gemm_residual; X.rec_gemm_window_residual = sh_gemm_window_residual;
    X.rec_gemm_qkv = sh_gemm_qkv; X.rec_gemm_qkv_window = sh_gemm_qkv_window;
    X.rec_gemm_dual = sh_gemm_dual; X.rec_unary2 = sh_unary2; X.rec_qkv = sh_qkv;
    X.rec_window_attention = sh_window_attention; X.rec_ffn = sh_ffn;
    X.rec_ffn_merge = sh_ffn_merge; X.rec_ffn_stem = sh_ffn_stem;
    X.rec_gemm_window_residual_pool = sh_gemm_window_residual_pool;
    X.rec_window_block = sh_window_block; X.rec_global_attention = sh_global_attention;
    X.sync = sh_sync;
}

static void real_recorders(const struct xmx *saved)
{
    X = *saved;
    discovering = NULL;
}

/* Each role at the largest request the planning recording used. */
static void size_roles(struct nr_frame *f)
{
    for (int r = 0; r < R_COUNT; r++) f->role_bytes[r] = 0;
    for (int i = 0; i < f->request_count; i++) {
        const struct request *q = &f->requests[i];
        if (q->used && q->bytes > f->role_bytes[q->role]) f->role_bytes[q->role] = q->bytes;
    }
    f->request_count = 0;
}

/* `ResidentFrame.buffer`: a frame buffer by name, grown when a larger request arrives.
 * Two of them are the host's: the features go in, the head comes back. */
static int named_buffer(struct nr_frame *f, const char *name, size_t bytes)
{
    int kind = (!strcmp(name, "features") || !strcmp(name, "features_host16")) ? HOST_WRITE
             : (!strcmp(name, "head") || !strcmp(name, "head4")) ? HOST_READ : GRAPH;
    for (int i = 0; i < f->named_count; i++) {
        struct named *n = &f->named[i];
        if (strcmp(n->name, name)) continue;
        if (n->bytes >= bytes) return n->id;

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        if (!f->planning) { sprintf_s(last_error, sizeof last_error, "buffer %s grew after the plan", name); return -2; }
#else
        if (!f->planning) { snprintf(last_error, sizeof last_error, "buffer %s grew after the plan", name); return -2; }
#endif

        X.buf_destroy(n->id);
        n->id = buffer_with(NULL, bytes, kind);
        n->bytes = bytes;
        return n->id < 0 ? -2 : n->id;
    }
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (f->named_count == MAX_NAMED) { sprintf_s(last_error, sizeof last_error, "too many frame buffers"); return -2; }
#else
    if (f->named_count == MAX_NAMED) { snprintf(last_error, sizeof last_error, "too many frame buffers"); return -2; }
#endif

    struct named *n = &f->named[f->named_count];
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(n->name, sizeof n->name, "%s", name);
#else
    snprintf(n->name, sizeof n->name, "%s", name);
#endif

    n->id = buffer_with(NULL, bytes, kind);
    n->bytes = bytes;
    if (n->id < 0) return -2;
    f->named_count++;
    return n->id;
}

static int named_bufferf(struct nr_frame *f, const char *fmt, int i, size_t bytes)
{
    char name[24];
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(name, sizeof name, fmt, i);
#else
    snprintf(name, sizeof name, fmt, i);
#endif

    return named_buffer(f, name, bytes);
}

/* -- recording primitives: xmxres.Runtime, flag for flag ------------------ */

enum { E4M3 = 0, GATE, HALF, TO_HALF, SCALE, RESIDUAL, FROM_HALF, PARTITION, REVERSE, ADD_BIAS,
       SPLIT_HEADS, MERGE_HEADS, POOL2, UPSAMPLE2, SCALE_CHANNEL, ADD, PAD_END,
       GATE_E4M3_HALF, E4M3_HALF, GATE_HALF, UPSAMPLE_MERGE, UPSAMPLE_ADD, POOL2_SKIP };
enum { COSINE_PUBLISH = 0, SOFTMAX = 1 };
enum { EPI_NONE = 0, EPI_E4M3 = 1, EPI_GATE = 2, EPI_GATE_E4M3 = 3, EPI_HALF = 4 };

static unsigned publish(int epilogue, int narrow) { return ((unsigned)epilogue << 8) | (narrow ? 0x1000u : 0u); }
static unsigned reads(int a_half, int b_half) { return (a_half ? 0x8000u : 0u) | (b_half ? 0x10000u : 0u); }

/* Every recording call goes through here. While planning the recorders are the shadows
 * (`shadow_recorders`), which record nothing and mark the scratch each pass is handed. */
#define REC(f, call) do { if (call) FAILF("%s: %s", #call, X.error()); } while (0)
#define TRY(call) do { if (call) return -1; } while (0)

struct dims { unsigned batch, height, width, across; };

static int unary(struct nr_frame *f, unsigned kind, int source, int second, int target, int third,
                 size_t count, unsigned channels, float scale, int epilogue, int narrow,
                 int a_half, int b_half, struct dims d, unsigned pad)
{
    if (second < 0) second = source;
    if (third < 0) third = source;
    REC(f, X.rec_unary(kind | publish(epilogue, narrow) | reads(a_half, b_half), source, second, target,
                       third, (unsigned)count, channels, scale, d.batch, d.height, d.width, d.across, pad));
    return 0;
}

static const struct dims NODIMS = { 0, 0, 0, 0 };

static int to_half(struct nr_frame *f, int s, int t, size_t n)   { return unary(f, TO_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int from_half(struct nr_frame *f, int s, int t, size_t n) { return unary(f, FROM_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int e4m3(struct nr_frame *f, int s, int t, size_t n)      { return unary(f, E4M3, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }
static int e4m3_half(struct nr_frame *f, int s, int t, size_t n) { return unary(f, E4M3_HALF, s, -1, t, -1, n, 0, 1.0f, 0, 0, 0, 0, NODIMS, 0); }

/* `Runtime.window_extent`: the padded extent and pads of a shifted-window partition. */
static void window_extent(int height, int width, int oy, int ox, int size, int *ph, int *pw, int *top, int *left)
{
    int pt = -oy, pl = -ox;
    *ph = pt + height + ((size - (height + pt) % size) % size);
    *pw = pl + width + ((size - (width + pl) % size) % size);
    *top = pt; *left = pl;
}

static int residual(struct nr_frame *f, int branch, int skip, int cosine, int target, size_t count,
                    unsigned channels, int epilogue, int narrow, int a_half, int b_half,
                    int reverse, int height, int width, int oy, int ox)
{
    if (reverse) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        struct dims d = { 8, (unsigned)height, (unsigned)width, (unsigned)(pw / 8) };
        return unary(f, RESIDUAL | 0x4000u, branch, skip, target, cosine, count, channels, 1.0f,
                     epilogue, narrow, a_half, b_half, d, ((unsigned)top << 16) | (unsigned)left);
    }
    return unary(f, RESIDUAL, branch, skip, target, cosine, count, channels, 1.0f, epilogue, narrow,
                 a_half, b_half, NODIMS, 0);
}

static int partition(struct nr_frame *f, int source, int target, int height, int width, unsigned channels,
                     int oy, int ox, int narrow, int a_half)
{
    int ph, pw, top, left;
    window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
    struct dims d = { 8, (unsigned)height, (unsigned)width, (unsigned)(pw / 8) };
    return unary(f, PARTITION, source, -1, target, -1, (size_t)ph * pw * channels, channels, 1.0f,
                 0, narrow, a_half, 0, d, ((unsigned)top << 16) | (unsigned)left);
}

static int split_heads(struct nr_frame *f, int source, int target, unsigned windows, unsigned tokens,
                       unsigned channels, unsigned heads, unsigned part, int epilogue, int narrow)
{
    struct dims d = { heads, tokens, part, 0 };
    return unary(f, SPLIT_HEADS, source, -1, target, -1, (size_t)windows * tokens * channels, channels,
                 1.0f, epilogue, narrow, 0, 0, d, 0);
}

static int merge_heads(struct nr_frame *f, int source, int target, unsigned windows, unsigned tokens,
                       unsigned channels, unsigned heads, int epilogue, int narrow)
{
    struct dims d = { heads, tokens, 0, 0 };
    return unary(f, MERGE_HEADS, source, -1, target, -1, (size_t)windows * tokens * channels, channels,
                 1.0f, epilogue, narrow, 0, 0, d, 0);
}

static int pool2(struct nr_frame *f, int source, int target, int height, int width, unsigned channels,
                 int epilogue, int narrow, int a_half)
{
    struct dims d = { 0, (unsigned)height, (unsigned)width, 0 };
    return unary(f, POOL2, source, -1, target, -1, (size_t)(height / 2) * (width / 2) * channels, channels,
                 1.0f, epilogue, narrow, a_half, 0, d, 0);
}

static int upsample2(struct nr_frame *f, int source, int target, int source_width, int height, int width,
                     unsigned channels, int a_half)
{
    struct dims d = { 0, (unsigned)width, (unsigned)source_width, 0 };
    return unary(f, UPSAMPLE2, source, -1, target, -1, (size_t)height * width * channels, channels, 1.0f,
                 0, 0, a_half, 0, d, 0);
}

static int pad_end(struct nr_frame *f, int source, int target, int height, int width, int ph, int pw,
                   unsigned channels, int a_half, int narrow)
{
    struct dims d = { 0, (unsigned)height, (unsigned)width, (unsigned)pw };
    return unary(f, PAD_END, source, -1, target, -1, (size_t)ph * pw * channels, channels, 1.0f,
                 0, narrow, a_half, 0, d, 0);
}

static int scale_channel(struct nr_frame *f, int source, int factors, int target, size_t count,
                         unsigned channels, int a_half)
{
    return unary(f, SCALE_CHANNEL, source, -1, target, factors, count, channels, 1.0f, 0, 0, a_half, 0, NODIMS, 0);
}

static int add(struct nr_frame *f, int left, int right, int target, size_t count, int epilogue, int narrow)
{
    return unary(f, ADD, left, right, target, -1, count, 0, 1.0f, epilogue, narrow, 0, 0, NODIMS, 0);
}

static int cosine_publish(struct nr_frame *f, int source, int target, size_t rows, unsigned tokens,
                          unsigned heads, int scale, int narrow, int from_half_in, int qkv_part)
{
    unsigned flags = COSINE_PUBLISH | publish(0, narrow) | (from_half_in ? 0x8000u : 0u);
    if (qkv_part >= 0) flags |= 0x20000u | ((unsigned)qkv_part << 18);
    REC(f, X.rec_row(flags, source, source, target, scale >= 0 ? scale : source, (unsigned)rows, tokens,
                     heads, scale >= 0 ? 1u : 0u, 0, 0.0f));
    return 0;
}

static int softmax(struct nr_frame *f, int source, int target, size_t rows, unsigned width, unsigned stride,
                   float cap, int narrow, int bias, unsigned heads)
{
    unsigned flags = SOFTMAX | publish(0, narrow) | (bias >= 0 ? 0x2000u : 0u);
    REC(f, X.rec_row(flags, source, bias >= 0 ? bias : source, target, source, (unsigned)rows, width, heads,
                     0, stride, cap));
    return 0;
}

struct gemm_opt {
    unsigned batch;                 /* 0 -> 1 */
    unsigned long long sa, sb, sc;  /* per-batch strides, 0 -> dense */
    int transpose_b;
    unsigned lda, ldb, ldc;
    unsigned oa, ob, oc;
    int epilogue, narrow;
    unsigned extra;                 /* further flag bits: 0x10000 the compact head */
};

static int gemm(struct nr_frame *f, int a, int b, int c, unsigned rows, unsigned cols, unsigned inner,
                const struct gemm_opt *o)
{
    static const struct gemm_opt plain = { 0 };
    if (!o) o = &plain;
    if (rows % 8 || cols % 16 || inner % 16) FAILF("gemm %ux%ux%u is not tile-aligned", rows, cols, inner);
    unsigned batch = o->batch ? o->batch : 1;
    unsigned long long sa = o->sa, sb = o->sb, sc = o->sc;
    if (!sa && !sb && !sc) {
        sa = (unsigned long long)rows * inner;
        sb = o->transpose_b ? (unsigned long long)cols * inner : (unsigned long long)inner * cols;
        sc = (unsigned long long)rows * cols;
    }
    unsigned flags = (o->transpose_b ? 1u : 0u) | publish(o->epilogue, o->narrow) | o->extra;
    REC(f, X.rec_gemm(a, b, c, rows, cols, inner, batch, (unsigned)sa, (unsigned)sb, (unsigned)sc, flags,
                      o->lda, o->ldb, o->ldc, o->oa, o->ob, o->oc));
    return 0;
}

/* `Runtime.independent()`: no barrier between the dispatches inside. */
static int independent(struct nr_frame *f, int on)
{
    REC(f, X.sync(on ? 0 : 1));
    return 0;
}

/* -- the fused passes: xmxres.Runtime, flag for flag (notes/improve-fusions.md) --------- */

/* `Runtime.gemm_residual`: target = a @ b + skip * cosine, the residual in the GEMM's own
 * epilogue; with `reverse` the rows are a window block's, padded and in window order, and
 * the output lands straight back in the unpadded image. */
static int gemm_residual(struct nr_frame *f, int a, int b, int skip, int cosine, int target,
                         unsigned rows, unsigned cols, unsigned inner, int epilogue, int narrow,
                         int skip_half, int reverse, int height, int width, int oy, int ox)
{
    unsigned flags = publish(epilogue, narrow) | (skip_half ? 0x40000u : 0u);
    if (reverse) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        REC(f, X.rec_gemm_window_residual(a, b, target, skip, cosine, rows, cols, inner, flags,
                                          (unsigned)height, (unsigned)width, (unsigned)(pw / 8),
                                          ((unsigned)top << 16) | (unsigned)left));
        return 0;
    }
    REC(f, X.rec_gemm_residual(a, b, target, skip, cosine, rows, cols, inner, flags));
    return 0;
}

/* `Runtime.gemm_qkv`: the QKV projection finished in its own epilogue — Q and K
 * normalised, V published, (window, head, token, 32) halves out. With `window` the
 * operand is the image itself and the partition happens in the projection's loads. */
static int gemm_qkv(struct nr_frame *f, int a, int weight, int q, int k, int v, int scale,
                    unsigned rows, unsigned channels, unsigned heads, unsigned tokens,
                    int window, int height, int width, int oy, int ox, int image_half)
{
    if (window) {
        int ph, pw, top, left;
        window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
        REC(f, X.rec_gemm_qkv_window(a, weight, q, k, v, scale, rows, channels, heads, tokens,
                                     (unsigned)width, (unsigned)height, (unsigned)(pw / 8),
                                     ((unsigned)top << 16) | (unsigned)left, image_half ? 1u : 0u));
        return 0;
    }
    REC(f, X.rec_gemm_qkv(a, weight, q, k, v, scale, rows, channels, heads, tokens));
    return 0;
}

/* `Runtime.gemm_dual`: c = a @ b in float32, and the same values as half into `half_copy`. */
static int gemm_dual(struct nr_frame *f, int a, int b, int c, int half_copy, unsigned rows,
                     unsigned cols, unsigned inner)
{
    REC(f, X.rec_gemm_dual(a, b, c, half_copy, rows, cols, inner));
    return 0;
}

/* `Runtime.ffn_fused`: a feed-forward in one pass. One group of 32 channels with a skip is
 * the narrow blocks' whole feed-forward; `groups` without a skip is the branched blocks'
 * per-group expand and projection into `target`'s column slices. */
static int ffn_fused(struct nr_frame *f, int a, int expand, int projection, int target,
                     unsigned rows, unsigned channels, unsigned hidden, unsigned groups,
                     int skip, int cosine, int epilogue, int narrow, int skip_half)
{
    unsigned flags = publish(epilogue, narrow) | (skip_half ? 0x40000u : 0u);
    REC(f, X.rec_ffn(a, expand, projection, target, skip, cosine, rows, channels, hidden, groups, flags));
    return 0;
}

/* `Runtime.upsample_merge`: merged = upsample2(source) * sin + skip * cos, stored float32
 * and as half; `sincos` is the per-channel sin then cos. */
static int upsample_merge(struct nr_frame *f, int source, int skip, int sincos, int merged, int merged16,
                          int height, int width, int source_width, unsigned channels)
{
    REC(f, X.rec_unary2(UPSAMPLE_MERGE | reads(1, 1), source, skip, merged, sincos, merged16,
                        (unsigned)((size_t)height * width * channels), channels, 1.0f, 0,
                        (unsigned)width, (unsigned)source_width, 0, 0));
    return 0;
}

/* `Runtime.window_attention`: QK^T, softmax and PV for full 8x8 windows in one dispatch;
 * `merged` also does the head merge and publishes into `target` as half. */
static int window_attention(struct nr_frame *f, int q, int k, int v, int target, unsigned batches,
                            unsigned heads, int bias, int merged)
{
    REC(f, X.rec_window_attention(q, k, v, bias, target, batches, heads, merged ? 1u : 0u));
    return 0;
}

/* `Runtime.upsample_add`: target = upsample2(source) + skip * factors, published, in one
 * pass; `source` is float32. */
static int upsample_add(struct nr_frame *f, int source, int skip, int factors, int target, int height,
                        int width, int source_width, unsigned channels, int skip_half, int epilogue,
                        int narrow)
{
    struct dims d = { 0, (unsigned)width, (unsigned)source_width, 0 };
    return unary(f, UPSAMPLE_ADD, source, skip, target, factors, (size_t)height * width * channels,
                 channels, 1.0f, epilogue, narrow, 0, skip_half, d, 0);
}

/* `Runtime.pool2_skip`: the published pool into `pooled` and the published source into
 * `skip`, in one read of the float32 source. Even extents only. */
static int pool2_skip(struct nr_frame *f, int source, int pooled, int skip, int height, int width,
                      unsigned channels)
{
    size_t count = (size_t)(height / 2) * (width / 2) * channels;
    REC(f, X.rec_unary2(POOL2_SKIP | publish(EPI_E4M3, 1), source, source, pooled, source, skip,
                        (unsigned)count, channels, 1.0f, 0, (unsigned)height, (unsigned)width, 0, 0));
    return 0;
}

/* `Runtime.gemm_residual_pool`: block 0's window residual, its output served to both of its
 * readers from the epilogue — `skip_out` published as half, `pooled` the published pool. */
static int gemm_residual_pool(struct nr_frame *f, int a, int b, int skip, int cosine, int skip_out,
                              int pooled, unsigned rows, unsigned inner, int height, int width,
                              int oy, int ox, int skip_half)
{
    int ph, pw, top, left;
    window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
    REC(f, X.rec_gemm_window_residual_pool(a, b, skip_out, skip, cosine, pooled, rows, 32, inner,
                                           skip_half ? 0x40000u : 0u, (unsigned)height, (unsigned)width,
                                           (unsigned)(pw / 8), ((unsigned)top << 16) | (unsigned)left));
    return 0;
}

/* The fused kernels built on first use, as `xmxres.fused_shader` names them. */
static int window_block_ready(void)
{
    char b[1200];
    if (X.window_block_init(spv(b, sizeof b, "XMX_WINDOW_BLOCK_SPV",
                                X.portable() ? "window_block_portable.spv" : "window_block.spv")))
        FAILF("window block pipeline: %s", X.error());
    return 0;
}

static int global_attention_ready(void)
{
    char b[1200];
    if (X.global_attention_init(spv(b, sizeof b, "XMX_GLOBAL_ATTENTION_SPV",
                                    X.portable() ? "global_attention_portable.spv" : "global_attention.spv")))
        FAILF("global attention pipeline: %s", X.error());
    return 0;
}

/* `Runtime.window_block`: a 32-channel window block's attention half in one pass a window.
 * `pooled` is block 0's pooled output (the target then the published skip, half), `head`
 * block 70's head weights (the target the head, `head_columns` 4 or 16 float32 a pixel). */
static int window_block(struct nr_frame *f, int image, int qkv, int projection, int target, int bias,
                        int cosine, int scale, int height, int width, int oy, int ox, int epilogue,
                        int narrow, int image_half, int pooled, int head, int head_columns)
{
    int ph, pw, top, left;
    window_extent(height, width, oy, ox, 8, &ph, &pw, &top, &left);
    unsigned windows = (unsigned)((ph / 8) * (pw / 8));
    unsigned flags = publish(epilogue, narrow) | (image_half ? 0x8000u : 0u)
                   | (pooled >= 0 ? 0x800000u : 0u) | (head >= 0 ? 0x1000000u : 0u)
                   | (head >= 0 && head_columns == 16 ? 0x2000000u : 0u);
    if (pooled >= 0) flags &= ~0x1000u;          /* the pool's target is half by definition */
    TRY(window_block_ready());
    REC(f, X.rec_window_block(image, qkv, projection, target, bias, cosine, scale,
                              pooled >= 0 ? pooled : head, windows, (unsigned)height, (unsigned)width,
                              (unsigned)(pw / 8), ((unsigned)top << 16) | (unsigned)left, flags));
    return 0;
}

/* `Runtime.global_attention`: QK^T, the softmax, PV and the head merge in one pass. */
static int global_attention(struct nr_frame *f, int q, int k, int v, int merged, unsigned rows,
                            unsigned tokens, unsigned heads, float cap)
{
    TRY(global_attention_ready());
    REC(f, X.rec_global_attention(q, k, v, merged, rows, tokens, heads, cap));
    return 0;
}

/* `Runtime.ffn_fused_merge` / `ffn_fused_stem`: the narrow feed-forward making its own input
 * — block 70's merge, or block 0's stem. */
static int ffn_fused_merge(struct nr_frame *f, int source, int skip, int sincos, int expand, int projection,
                           int target, int cosine, int height, int width, int source_width)
{
    REC(f, X.rec_ffn_merge(source, skip, sincos, expand, projection, target, cosine, (unsigned)height,
                           (unsigned)width, (unsigned)source_width, publish(0, 0)));
    return 0;
}

static int ffn_fused_stem(struct nr_frame *f, int features, int adapter, int expand, int projection,
                          int target, int cosine, unsigned rows)
{
    REC(f, X.rec_ffn_stem(features, adapter, expand, projection, target, cosine, rows, publish(0, 0)));
    return 0;
}

/* `Runtime.prepare_qkv`: Q, K and V from the float32 projection in one row dispatch. */
static int prepare_qkv(struct nr_frame *f, int source, int q, int k, int v, int scale,
                       unsigned windows, unsigned tokens, unsigned heads)
{
    REC(f, X.rec_qkv(source, q, k, v, scale, windows * tokens * heads, tokens, heads));
    return 0;
}

/* ------------------------------------------------------------------------- */
/* blocks: nr_resident.py, transcribed                                          */
/* ------------------------------------------------------------------------- */

/* `BlockScratch`: the working buffers of one window or split block at one extent. With
 * the arena every field is a role, so the struct is the geometry plus role handles. */
struct scratch {
    int height, width, tokens, windows, batch, hidden_width;
    size_t pixels, windowed;
    int value, value16, hidden16, heads16, branch, ffn, win16, proj, q16, k16, v16,
        scores, probs16, context, merged16, attended, out, core16;
    /* key16: K when the projection's own epilogue writes it — k16's role is the projection's
     * input, still being read. context16: the fused attention's merged output, in the
     * scores' and context's role, which the fused path leaves unused. */
    int key16, context16;
};

#define ROLE(field, role, bytes) do { s->field = arena(f, role, bytes); if (s->field == -2) return -1; } while (0)

static int block_scratch(struct nr_frame *f, const struct block_w *w, int height, int width, struct scratch *s)
{
    int ph, pw, top, left;
    window_extent(height, width, -4, -4, 8, &ph, &pw, &top, &left);
    memset(s, 0, sizeof *s);
    s->height = height; s->width = width; s->tokens = 64;
    s->windows = (ph / 8) * (pw / 8);
    s->batch = s->windows * w->heads;
    s->pixels = (size_t)height * width;
    size_t C = (size_t)w->channels;
    s->windowed = (size_t)s->windows * 64 * C;
    s->hidden_width = w->hidden_width;
    size_t hidden = (size_t)s->hidden_width;
    int wide = w->branched || w->family == SPLIT;
    ROLE(value, R_VALUE, s->pixels * C * 4);
    ROLE(value16, R_INPUT_KEY, s->pixels * C * 2);
    ROLE(hidden16, R_PROJECTION, s->pixels * hidden * 2);
    if (wide) ROLE(heads16, R_QUERY_PROB, s->pixels * C * 2); else s->heads16 = -1;
    ROLE(branch, R_BRANCH_VALUE, s->pixels * C * 4);
    ROLE(ffn, R_RESIDUAL, s->pixels * C * 4);
    ROLE(win16, R_INPUT_KEY, s->windowed * 2);
    ROLE(proj, R_PROJECTION, s->windowed * 3 * 4);
    ROLE(q16, R_QUERY_PROB, s->windowed * 2);
    ROLE(k16, R_INPUT_KEY, s->windowed * 2);
    ROLE(v16, R_BRANCH_VALUE, s->windowed * 2);
    ROLE(key16, R_PROJECTION, s->windowed * 2);
    ROLE(scores, R_SCORES_CONTEXT, (size_t)s->batch * 64 * 64 * 4);
    ROLE(probs16, R_QUERY_PROB, (size_t)s->batch * 64 * 64 * 2);
    ROLE(context, R_SCORES_CONTEXT, (size_t)s->batch * 64 * 32 * 4);
    ROLE(merged16, R_QUERY_PROB, s->windowed * 2);
    ROLE(context16, R_SCORES_CONTEXT, s->windowed * 2);
    ROLE(attended, R_PROJECTION, s->windowed * 4);
    ROLE(out, R_OUT, s->pixels * C * 4);
    if (w->family == SPLIT) ROLE(core16, R_QUERY_PROB, s->pixels * C * 2); else s->core16 = -1;
    return 0;
}

/* `GlobalScratch`: a bottleneck block over `tokens` tokens, padded to the tile. */
struct gscratch {
    int tokens, padded;
    int io16, context16;
    int value, value16, hidden16, branch, ffn, ffn16, proj, q16, k16, v16, key16, scores, probs16,
        context, merged16, attention, out;
};

#define GROLE(field, role, bytes) do { s->field = arena(f, role, bytes); if (s->field == -2) return -1; } while (0)

static int global_scratch(struct nr_frame *f, const struct block_w *w, int tokens, struct gscratch *s)
{
    memset(s, 0, sizeof *s);
    /* `GlobalScratch`: whole 64-row blocks where the pad stays under an eighth, so the
     * bottleneck's deep-K GEMMs take the staged kernel; pad rows are zero and excluded from
     * the softmax, so every real row is bit-identical either way */
    int padded = align16(tokens);
    /* below 64 tokens (the small frames `min_extent` allows) the pad is taken outright, and
     * at 32 or fewer a 32-row staged build does better still (libxmx.c, `small`) */
    if (tokens <= 32)
        padded = 32;
    else if (tokens <= 64 || align64(tokens) * 8 <= padded * 9)
        padded = align64(tokens);
    s->tokens = tokens; s->padded = padded;
    size_t P = (size_t)s->padded, C = (size_t)w->channels, H = (size_t)w->hidden_width, heads = (size_t)w->heads;
    GROLE(value, R_GLOBAL_VALUE, P * C * 4);
    /* the chain's value as half, between the blocks and through them; pad rows zero */
    GROLE(io16, R_GLOBAL_IO16, P * C * 2);
    GROLE(value16, R_INPUT_KEY, P * C * 2);
    GROLE(hidden16, R_PROJECTION, P * H * 2);
    GROLE(branch, R_BRANCH_VALUE, P * C * 4);
    GROLE(ffn, R_RESIDUAL, P * C * 4);
    GROLE(ffn16, R_INPUT_KEY, P * C * 2);
    GROLE(proj, R_PROJECTION, P * C * 3 * 4);
    GROLE(q16, R_QUERY_PROB, P * C * 2);
    GROLE(k16, R_INPUT_KEY, P * C * 2);
    GROLE(v16, R_BRANCH_VALUE, P * C * 2);
    GROLE(key16, R_PROJECTION, P * C * 2);
    GROLE(scores, R_SCORES_CONTEXT, heads * P * P * 4);
    GROLE(probs16, R_QUERY_PROB, heads * P * P * 2);
    GROLE(context, R_SCORES_CONTEXT, heads * P * 32 * 4);
    GROLE(merged16, R_QUERY_PROB, P * C * 2);
    /* the fused attention's output: merged16 shares q16's role, which it still reads */
    GROLE(context16, R_SCORES_CONTEXT, P * C * 2);
    GROLE(attention, R_PROJECTION, P * C * 4);
    GROLE(out, R_GLOBAL_OUT, P * C * 4);
    return 0;
}

/* `TransitionScratch`: sized for the largest level that uses it, the request rounded up
 * to a power of two as the Python does. */
struct tscratch { int padded, pooled16, projected, projected16, upsampled, scaled; };

static int transition_scratch(struct nr_frame *f, size_t elements, struct tscratch *s)
{
    size_t rounded = 2;
    while (rounded < elements) rounded <<= 1;
    if ((s->padded = arena(f, R_PROJECTION, rounded * 4)) == -2) return -1;
    if ((s->pooled16 = arena(f, R_INPUT_KEY, rounded * 2)) == -2) return -1;
    if ((s->projected = arena(f, R_PROJECTION, rounded * 4)) == -2) return -1;
    if ((s->projected16 = arena(f, R_INPUT_KEY, rounded * 2)) == -2) return -1;
    if ((s->upsampled = arena(f, R_SCORES_CONTEXT, rounded * 4)) == -2) return -1;
    if ((s->scaled = arena(f, R_RESIDUAL, rounded * 4)) == -2) return -1;
    return 0;
}


/* `record_qkv`: split V; normalise Q and K straight out of the projection buffer. */
static int record_qkv(struct nr_frame *f, const struct block_w *w, int proj, int q16, int k16, int v16,
                      unsigned windows, unsigned tokens)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads;
    size_t rows = (size_t)windows * heads * tokens;
    if (f->opt.fuse_qk && f->opt.joint_qkv)
        return prepare_qkv(f, proj, q16, k16, v16, w->scale, windows, tokens, heads);
    if (f->opt.fuse_qk) {
        TRY(independent(f, 1));
        TRY(cosine_publish(f, proj, q16, rows, tokens, heads, w->scale, 1, 0, 0));
        TRY(cosine_publish(f, proj, k16, rows, tokens, heads, -1, 1, 0, 1));
        TRY(split_heads(f, proj, v16, windows, tokens, C, heads, 2, EPI_E4M3, 1));
        TRY(independent(f, 0));
        return 0;
    }
    TRY(independent(f, 1));
    TRY(split_heads(f, proj, q16, windows, tokens, C, heads, 0, EPI_HALF, 1));
    TRY(split_heads(f, proj, k16, windows, tokens, C, heads, 1, EPI_HALF, 1));
    TRY(split_heads(f, proj, v16, windows, tokens, C, heads, 2, EPI_E4M3, 1));
    TRY(independent(f, 0));
    TRY(independent(f, 1));
    TRY(cosine_publish(f, q16, q16, rows, tokens, heads, w->scale, 1, 1, -1));
    TRY(cosine_publish(f, k16, k16, rows, tokens, heads, -1, 1, 1, -1));
    TRY(independent(f, 0));
    return 0;
}

/* `record_qkv_projection`: the QKV projection and everything that prepares Q, K and V
 * after it. `*key` is the buffer K ended up in: `key16` under the epilogue, `k16` when the
 * projection went to memory as float32 and `record_qkv` read it back. With `window`, `a`
 * is the image (float32, or half with `image_half`) and the epilogue GEMM gathers its
 * window rows itself; without the epilogue the partition is recorded as it always was. */
static int record_qkv_projection(struct nr_frame *f, const struct block_w *w, int a,
                                 int proj, int q16, int k16, int v16, int key16, int win16,
                                 unsigned windows, unsigned tokens, int window, int height, int width,
                                 int image_half, int *key)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads;
    unsigned rows = windows * tokens;
    if (window) {
        if (f->opt.qkv_epilogue && rows % 64 == 0) {
            TRY(gemm_qkv(f, a, w->qkv, q16, key16, v16, w->scale, rows, C, heads, tokens,
                         1, height, width, w->oy, w->ox, image_half));
            *key = key16;
            return 0;
        }
        TRY(partition(f, a, win16, height, width, C, w->oy, w->ox, 1, image_half));
        a = win16;
    }
    if (f->opt.qkv_epilogue && rows % 16 == 0) {
        TRY(gemm_qkv(f, a, w->qkv, q16, key16, v16, w->scale, rows, C, heads, tokens, 0, 0, 0, 0, 0, 0));
        *key = key16;
        return 0;
    }
    TRY(gemm(f, a, w->qkv, proj, rows, 3 * C, C, NULL));
    TRY(record_qkv(f, w, proj, q16, k16, v16, windows, tokens));
    *key = k16;
    return 0;
}

/* `record_project_residual`: target = a @ weight + skip * cosine — fused into the GEMM's
 * epilogue, or the two passes the graph always had (`NR_FUSE_RESIDUAL=0`). */
static int record_project_residual(struct nr_frame *f, int a, int weight, int branch, int skip, int cosine,
                                   int target, unsigned rows, unsigned channels, unsigned inner,
                                   int epilogue, int skip_half, int narrow)
{
    if (f->opt.fuse_residual)
        return gemm_residual(f, a, weight, skip, cosine, target, rows, channels, inner, epilogue, narrow,
                             skip_half, 0, 0, 0, 0, 0);
    TRY(gemm(f, a, weight, branch, rows, channels, inner, NULL));
    return residual(f, branch, skip, cosine, target, (size_t)rows * channels, channels, epilogue, narrow,
                    0, skip_half, 0, 0, 0, 0, 0);
}

/* `_ffn_groups`: independent group products — one batched GEMM whose batch strides advance
 * the group (`NR_BATCH_FFN`), or one GEMM per group with the same tiles and rounding. */
static int ffn_groups(struct nr_frame *f, int a, int b, int c, unsigned rows, unsigned cols, unsigned inner,
                      unsigned groups, unsigned lda, unsigned ldb, unsigned ldc,
                      unsigned sa, unsigned sb, unsigned sc, int epilogue)
{
    if (f->opt.batch_ffn) {
        struct gemm_opt o = { .batch = groups, .sa = sa, .sb = sb, .sc = sc, .lda = lda, .ldb = ldb, .ldc = ldc,
                              .epilogue = epilogue, .narrow = 1 };
        return gemm(f, a, b, c, rows, cols, inner, &o);
    }
    TRY(independent(f, 1));
    for (unsigned g = 0; g < groups; g++) {
        struct gemm_opt o = { .lda = lda, .ldb = ldb, .ldc = ldc, .oa = g * sa, .ob = g * sb, .oc = g * sc,
                              .epilogue = epilogue, .narrow = 1 };
        TRY(gemm(f, a, b, c, rows, cols, inner, &o));
    }
    TRY(independent(f, 0));
    return 0;
}

/* nr_resident.py's predicates: which of the improve-b fusions a block can take. */
static int can_make_input(const struct nr_frame *f, const struct block_w *w, const struct scratch *s)
{
    return f->opt.fuse_ffn && !w->branched && w->family != SPLIT && w->channels == 32
           && s->hidden_width == 128 && s->pixels % 16 == 0;
}

static int can_merge_input(const struct nr_frame *f, const struct block_w *w, const struct scratch *s)
{
    return f->opt.fuse_merge_ffn && can_make_input(f, w, s) && s->height % 2 == 0 && s->width % 2 == 0;
}

static int can_make_stem(const struct nr_frame *f, const struct block_w *w, const struct scratch *s)
{
    return f->opt.fuse_stem_ffn && can_make_input(f, w, s);
}

static int can_fuse_window_block(const struct nr_frame *f, const struct block_w *w, const struct scratch *s)
{
    return w->channels == 32 && w->heads == 1 && s->tokens == 64 && f->opt.fuse_window_block
           && f->opt.qkv_epilogue && f->opt.fuse_partition && f->opt.fuse_window_attention
           && f->opt.fuse_attention_merge && f->opt.fuse_window_residual;
}

static int can_pool_output(const struct nr_frame *f, const struct block_w *w, const struct scratch *s)
{
    int ph, pw, top, left;
    window_extent(s->height, s->width, w->oy, w->ox, 8, &ph, &pw, &top, &left);
    return f->opt.fuse_pool && f->opt.fuse_window_residual && w->channels == 32 && w->family != SPLIT
           && s->tokens == 64 && s->height % 2 == 0 && s->width % 2 == 0 && top % 2 == 0 && left % 2 == 0;
}

/* What block 70's feed-forward makes its input from (`merge`), and block 0's (`stem`). */
struct made_merge { int above, skip, sincos, above_width; };
struct made_stem { int features, adapter; };

/* `record_feed_forward`: branched or plain, into `s->ffn`. `source16` is a half copy of a
 * float32 input already written by the pass that produced it (the glue), so no to_half is
 * needed. `*ffn_half` says whether `s->ffn` was stored as half: the branched blocks publish
 * it as E4M3, which half holds exactly, so they store it narrow. */
static int record_feed_forward(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                               int source, int source_half, int source16, int *ffn_half,
                               const struct made_merge *merge, const struct made_stem *stem)
{
    unsigned pixels = (unsigned)s->pixels, C = (unsigned)w->channels;
    unsigned hidden = (unsigned)s->hidden_width, groups = (unsigned)w->groups;
    *ffn_half = 0;
    if (merge) {
        if (!can_merge_input(f, w, s)) FAILF("block %d's feed-forward cannot make its own input", w->index);
        return ffn_fused_merge(f, merge->above, merge->skip, merge->sincos, w->expand, w->branch, s->ffn,
                               w->ffn_cos, s->height, s->width, merge->above_width);
    }
    if (stem) {
        if (!can_make_stem(f, w, s)) FAILF("block %d's feed-forward cannot make its own stem", w->index);
        return ffn_fused_stem(f, stem->features, stem->adapter, w->expand, w->branch, s->ffn, w->ffn_cos,
                              pixels);
    }
    int value16 = source_half ? source : (source16 >= 0 ? source16 : s->value16);
    if (!source_half && source16 < 0) TRY(to_half(f, source, s->value16, s->pixels * C));
    *ffn_half = 0;
    if (w->branched) {
        if (f->opt.fuse_branched_ffn && pixels % 16 == 0 && C % 16 == 0 && C == groups * 32) {
            /* every group's expand and projection in one pass, the hidden layer on chip */
            TRY(ffn_fused(f, value16, w->expand, w->branch, s->heads16, pixels, C, 128, groups,
                          -1, -1, EPI_E4M3, 1, 0));
        } else {
            TRY(ffn_groups(f, value16, w->expand, s->hidden16, pixels, 128, C, groups,
                           0, 0, hidden, 0, C * 128, 128, EPI_GATE_E4M3));
            TRY(ffn_groups(f, s->hidden16, w->branch, s->heads16, pixels, 32, 128, groups,
                           hidden, 0, C, 128, 128 * 32, 32, EPI_E4M3));
        }
        /* the fused multi-head kernels publish the residual before attention reads it,
         * which the residual now does on its way out — as half, which holds it exactly */
        TRY(record_project_residual(f, s->heads16, w->ffn_out, s->branch, source, w->ffn_cos, s->ffn,
                                    pixels, C, C, EPI_E4M3, source_half, 1));
        *ffn_half = 1;
        return 0;
    }
    if (f->opt.fuse_ffn && C == 32 && hidden % 32 == 0 && pixels % 16 == 0) {
        /* both GEMMs in one pass, the hidden layer never written (ffn_fused.comp) */
        return ffn_fused(f, value16, w->expand, w->branch, s->ffn, pixels, C, hidden, 1,
                         source, w->ffn_cos, 0, 0, source_half);
    }
    struct gemm_opt o = { .epilogue = EPI_GATE_E4M3, .narrow = 1 };
    TRY(gemm(f, value16, w->expand, s->hidden16, pixels, hidden, C, &o));
    return record_project_residual(f, s->hidden16, w->branch, s->branch, source, w->ffn_cos, s->ffn,
                                   pixels, C, hidden, 0, source_half, 0);
}

/* `record_split_feed_forward`: e4m3(x @ first), then a per-64-group 64 -> 256 -> 64 MLP. */
static int record_split_feed_forward(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                                     int source, int source_half)
{
    unsigned pixels = (unsigned)s->pixels, C = (unsigned)w->channels, groups = (unsigned)w->groups;
    unsigned wide = groups * 256;
    int value16 = source_half ? source : s->value16;
    if (!source_half) TRY(to_half(f, source, s->value16, s->pixels * C));
    struct gemm_opt first = { .epilogue = EPI_E4M3, .narrow = 1 };
    TRY(gemm(f, value16, w->first, s->heads16, pixels, C, C, &first));
    TRY(ffn_groups(f, s->heads16, w->expand, s->hidden16, pixels, 256, 64, groups,
                   C, 0, wide, 64, 64 * 256, 256, EPI_GATE));
    TRY(ffn_groups(f, s->hidden16, w->project, s->core16, pixels, 64, 256, groups,
                   wide, 0, C, 256, 256 * 64, 64, EPI_E4M3));
    return record_project_residual(f, s->core16, w->weight3, s->branch, source, w->ffn_cos, s->ffn,
                                   pixels, C, C, 0, source_half, 0);
}

/* `record_window_attention`: over `source`, in window order. With `to_target` the
 * output projection finishes the block — it adds `source * attn_cos` and writes straight
 * back into the unpadded image; otherwise the result goes to `s->attended` and the
 * closing residual's own gather reverses the windows. */
static int record_window_attention(struct nr_frame *f, const struct block_w *w, const struct scratch *s,
                                   int source, int to_target, int target, int publish_epilogue,
                                   int target_half, int source_half, const int *pool, const int *head)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads, tokens = 64;
    /* `pool` is (pooled, published), `head` (weights, target, columns) */
    if ((to_target || pool || head) && can_fuse_window_block(f, w, s)) {
        /* the three fused passes below in one, a window a workgroup (window_block.comp) */
        if (head)
            return window_block(f, source, w->qkv, w->out, head[1], w->bias, w->attn_cos, w->scale,
                                s->height, s->width, w->oy, w->ox, 0, 0, source_half, -1, head[0], head[2]);
        if (pool)
            return window_block(f, source, w->qkv, w->out, pool[1], w->bias, w->attn_cos, w->scale,
                                s->height, s->width, w->oy, w->ox, 0, 1, source_half, pool[0], -1, 0);
        return window_block(f, source, w->qkv, w->out, target, w->bias, w->attn_cos, w->scale,
                            s->height, s->width, w->oy, w->ox, publish_epilogue, target_half, source_half,
                            -1, -1, 0);
    }
    int ph, pw, top, left;
    window_extent(s->height, s->width, w->oy, w->ox, 8, &ph, &pw, &top, &left);
    unsigned windows = (unsigned)((ph / 8) * (pw / 8));
    unsigned batch = windows * heads;
    int key;
    if (f->opt.fuse_partition) {
        /* the projection gathers its window rows from the image itself */
        TRY(record_qkv_projection(f, w, source, s->proj, s->q16, s->k16, s->v16, s->key16, s->win16,
                                  windows, tokens, 1, s->height, s->width, source_half, &key));
    } else {
        TRY(partition(f, source, s->win16, s->height, s->width, C, w->oy, w->ox, 1, source_half));
        TRY(record_qkv_projection(f, w, s->win16, s->proj, s->q16, s->k16, s->v16, s->key16, s->win16,
                                  windows, tokens, 0, 0, 0, 0, &key));
    }
    int fused = f->opt.fuse_window_attention && tokens == 64;
    int merged = fused && f->opt.fuse_attention_merge;
    int attended = merged ? s->context16 : s->merged16;
    if (fused) {
        TRY(window_attention(f, s->q16, key, s->v16, merged ? attended : s->context, batch, heads,
                             w->bias, merged));
    } else {
        struct gemm_opt qk = { .batch = batch, .sa = tokens * 32, .sb = tokens * 32, .sc = tokens * tokens,
                               .transpose_b = 1 };
        TRY(gemm(f, s->q16, key, s->scores, tokens, tokens, 32, &qk));
        TRY(softmax(f, s->scores, s->probs16, (size_t)batch * tokens, tokens, 0, 0.0f, 1, w->bias, heads));
        struct gemm_opt pv = { .batch = batch, .sa = tokens * tokens, .sb = tokens * 32, .sc = tokens * 32 };
        TRY(gemm(f, s->probs16, s->v16, s->context, tokens, 32, tokens, &pv));
    }
    if (!merged) TRY(merge_heads(f, s->context, s->merged16, windows, tokens, C, heads, EPI_E4M3, 1));
    if (pool)
        /* block 0: the output only ever read pooled or published, both made here */
        return gemm_residual_pool(f, attended, w->out, source, w->attn_cos, pool[1], pool[0],
                                  windows * tokens, C, s->height, s->width, w->oy, w->ox, source_half);
    if (to_target)
        return gemm_residual(f, attended, w->out, source, w->attn_cos, target, windows * tokens, C, C,
                             publish_epilogue, target_half, source_half, 1, s->height, s->width, w->oy, w->ox);
    return gemm(f, attended, w->out, s->attended, windows * tokens, C, C, NULL);
}

/* `record_block`: feed-forward, attention, both residuals. `prepared` is the block's
 * scratch when the caller allocated it first (the glue writes into its `value16` before the
 * block runs); `source16` is that half copy. */
static int record_block_made(struct nr_frame *f, const struct block_w *w, int height, int width, int source,
                             int target, int publish_epilogue, int source_half, int target_half,
                             int source16, const struct scratch *prepared, const struct made_merge *merge,
                             const struct made_stem *stem, const int *pool, const int *head)
{
    struct scratch own;
    const struct scratch *s = prepared;
    if (!s) { TRY(block_scratch(f, w, height, width, &own)); s = &own; }
    int ffn_half = 0;
    if (w->family == SPLIT) {
        if (source16 >= 0) FAILF("the split feed-forward takes no prepared half copy");
        TRY(record_split_feed_forward(f, w, s, source, source_half));
    } else {
        TRY(record_feed_forward(f, w, s, source, source_half, source16, &ffn_half, merge, stem));
    }
    if (pool && !can_pool_output(f, w, s)) FAILF("block %d's window residual cannot pool its output", w->index);
    if (head && !can_fuse_window_block(f, w, s)) FAILF("only a fused window block computes the head itself");
    if (f->opt.fuse_window_residual) {
        return record_window_attention(f, w, s, s->ffn, 1, target, publish_epilogue, target_half,
                                       ffn_half, pool, head);
    }
    TRY(record_window_attention(f, w, s, s->ffn, 0, -1, 0, 0, ffn_half, NULL, NULL));
    return residual(f, s->attended, s->ffn, w->attn_cos, target, s->pixels * (size_t)w->channels,
                    (unsigned)w->channels, publish_epilogue, target_half, 0, ffn_half, 1, height, width,
                    w->oy, w->ox);
}

static int record_block(struct nr_frame *f, const struct block_w *w, int height, int width, int source,
                        int target, int publish_epilogue, int source_half, int target_half,
                        int source16, const struct scratch *prepared)
{
    return record_block_made(f, w, height, width, source, target, publish_epilogue, source_half,
                             target_half, source16, prepared, NULL, NULL, NULL, NULL);
}

/* `record_global_block`: the wide feed-forward, then attention over every token. With
 * `chain` the block reads and writes `s->io16`, the value as half: published E4M3 between
 * the blocks, so exact, and the feed-forward reads it, its residual widens it and the
 * output projection publishes into it over the real rows only — the pad rows stay zero. */
static int record_global_block(struct nr_frame *f, const struct block_w *w, const struct gscratch *s,
                               int chain)
{
    unsigned C = (unsigned)w->channels, heads = (unsigned)w->heads, P = (unsigned)s->padded;
    unsigned hidden = (unsigned)w->hidden_width;
    size_t PC = (size_t)P * C;
    int source = chain ? s->io16 : s->value, value16 = chain ? s->io16 : s->value16;
    if (!chain) TRY(to_half(f, s->value, s->value16, PC));
    struct gemm_opt gate = { .epilogue = EPI_GATE_E4M3, .narrow = 1 };
    TRY(gemm(f, value16, w->expand, s->hidden16, P, hidden, C, &gate));
    TRY(record_project_residual(f, s->hidden16, w->ffn_proj, s->branch, source, w->ffn_cos, s->ffn,
                                P, C, hidden, 0, chain, 0));
    TRY(to_half(f, s->ffn, s->ffn16, PC));
    int key;
    TRY(record_qkv_projection(f, w, s->ffn16, s->proj, s->q16, s->k16, s->v16, s->key16, -1, 1, P,
                              0, 0, 0, 0, &key));
    int merged;
    if (f->opt.fuse_global_attention) {
        /* QK^T, the softmax, PV and the head merge in one pass, and no score stored */
        merged = s->context16;
        TRY(global_attention(f, s->q16, key, s->v16, merged, P, (unsigned)s->tokens, heads, w->logit_cap));
    } else {
        merged = s->merged16;
        struct gemm_opt qk = { .batch = heads, .sa = (unsigned long long)P * 32, .sb = (unsigned long long)P * 32,
                               .sc = (unsigned long long)P * P, .transpose_b = 1 };
        TRY(gemm(f, s->q16, key, s->scores, P, P, 32, &qk));
        /* no attention bias here, and the logits are clamped symmetrically */
        TRY(softmax(f, s->scores, s->probs16, (size_t)heads * P, (unsigned)s->tokens, P, w->logit_cap, 1, -1, 0));
        struct gemm_opt pv = { .batch = heads, .sa = (unsigned long long)P * P, .sb = (unsigned long long)P * 32,
                               .sc = (unsigned long long)P * 32 };
        TRY(gemm(f, s->probs16, s->v16, s->context, P, 32, P, &pv));
        TRY(merge_heads(f, s->context, merged, 1, P, C, heads, EPI_E4M3, 1));
    }
    if (chain)
        return record_project_residual(f, merged, w->out, s->attention, s->ffn, w->attn_cos, s->io16,
                                       (unsigned)s->tokens, C, C, EPI_E4M3, 0, 1);
    return record_project_residual(f, merged, w->out, s->attention, s->ffn, w->attn_cos, s->out,
                                   P, C, C, 0, 0, 0);
}

/* `record_downsample`: pool the unpublished output, publish it, then project. */
static int record_downsample(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t, int source,
                             int target, int height, int width, unsigned C, int pad_to, int source_half,
                             int target_half)
{
    if (pad_to) {
        int ph = (height + pad_to - 1) / pad_to * pad_to, pw = (width + pad_to - 1) / pad_to * pad_to;
        TRY(pad_end(f, source, t->padded, height, width, ph, pw, C, source_half, source_half));
        source = t->padded; height = ph; width = pw;
    }
    unsigned pixels = (unsigned)((height / 2) * (width / 2));
    TRY(pool2(f, source, t->pooled16, height, width, C, EPI_E4M3, 1, source_half));
    struct gemm_opt o = { .epilogue = EPI_E4M3, .narrow = target_half };
    TRY(gemm(f, t->pooled16, e->weight0, target, pixels, (unsigned)e->out_channels, C, &o));
    return 0;
}

/* `record_plain_downsample`: block 30's bridge into the bottleneck, no publish between. */
static int record_plain_downsample(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t,
                                   int source, int target, int height, int width, unsigned C, int pad_to,
                                   int source_half, int target_half)
{
    if (pad_to) {
        int ph = (height + pad_to - 1) / pad_to * pad_to, pw = (width + pad_to - 1) / pad_to * pad_to;
        TRY(pad_end(f, source, t->padded, height, width, ph, pw, C, source_half, source_half));
        source = t->padded; height = ph; width = pw;
    }
    unsigned pixels = (unsigned)((height / 2) * (width / 2));
    TRY(pool2(f, source, t->pooled16, height, width, C, EPI_HALF, 1, source_half));
    struct gemm_opt o = { .epilogue = EPI_E4M3, .narrow = target_half };
    TRY(gemm(f, t->pooled16, e->weight0, target, pixels, (unsigned)e->out_channels, C, &o));
    return 0;
}

/* `record_upsample_merge`: project, nearest-upsample onto the skip, add the scaled skip,
 * publish. */
static int record_upsample_merge(struct nr_frame *f, const struct edge_w *e, const struct tscratch *t,
                                 int source, int skip, int target, int sh, int sw, int height, int width,
                                 unsigned C, unsigned out_C, int source_half, int skip_half, int target_half)
{
    unsigned source_pixels = (unsigned)(sh * sw);
    int projected16 = source_half ? source : t->projected16;
    if (!source_half) TRY(to_half(f, source, t->projected16, (size_t)source_pixels * C));
    TRY(gemm(f, projected16, e->weight0, t->projected, source_pixels, out_C, C, NULL));
    size_t count = (size_t)height * width * out_C;
    if (f->opt.fuse_transition)
        /* the upsample, the scaled skip and the add in one pass (resident.comp UPSAMPLE_ADD) */
        return upsample_add(f, t->projected, skip, e->sine, target, height, width, sw, out_C, skip_half,
                            EPI_E4M3, target_half);
    TRY(independent(f, 1));
    TRY(upsample2(f, t->projected, t->upsampled, sw, height, width, out_C, 0));
    TRY(scale_channel(f, skip, e->sine, t->scaled, count, out_C, skip_half));
    TRY(independent(f, 0));
    TRY(add(f, t->upsampled, t->scaled, target, count, EPI_E4M3, target_half));
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the graph for one extent: ResidentFrame._run in replay mode                  */
/* ------------------------------------------------------------------------- */

static const struct block_w *block(struct nr_frame *f, int index, int heads, enum family family)
{
    struct block_w *b = &f->blocks[index];
    if (b->loaded) return b;
    int r = family == SPLIT ? load_split_block(b, &f->w, index)
          : family == GLOBAL ? load_global_block(b, &f->w, index)
          : load_window_block(b, &f->w, index, heads);
    return r ? NULL : b;
}

static const struct edge_w *edge(struct nr_frame *f, int index, int up)
{
    struct edge_w *e = &f->edges[index];
    if (e->loaded) return e;
    return load_edge(e, &f->w, index, up) ? NULL : e;
}

static void plan_levels(struct nr_frame *f, int H, int W)
{
    int (*L)[3] = f->levels;
    L[0][0] = H;      L[0][1] = W;      L[0][2] = 32;
    L[1][0] = H / 2;  L[1][1] = W / 2;  L[1][2] = 32;
    L[2][0] = H / 4;  L[2][1] = W / 4;  L[2][2] = 64;
    L[3][0] = H / 8;  L[3][1] = W / 8;  L[3][2] = 128;
    L[4][0] = H / 16; L[4][1] = W / 16; L[4][2] = 256;
    L[5][0] = pad8(L[4][0]) / 2; L[5][1] = pad8(L[4][1]) / 2; L[5][2] = 512;
    L[6][0] = pad8(L[5][0]) / 2; L[6][1] = pad8(L[5][1]) / 2; L[6][2] = 1024;
}

#define BLOCK(var, index, heads, family) const struct block_w *var = block(f, index, heads, family); if (!var) return -1
#define EDGE(var, index, up) const struct edge_w *var = edge(f, index, up); if (!var) return -1
#define NAMED(var, name, bytes) int var = named_buffer(f, name, bytes); if (var == -2) return -1
#define NAMEDF(var, fmt, i, bytes) int var = named_bufferf(f, fmt, i, bytes); if (var == -2) return -1

static const struct { int first, last, transition, heads; } ENCODER[4] = {
    { 1, 3, 4, 1 }, { 5, 7, 8, 2 }, { 9, 13, 14, 4 }, { 15, 21, 22, 8 } };
static const struct { int transition, first, last, level, heads; } DECODER[4] = {
    { 48, 49, 55, 4, 8 }, { 56, 57, 61, 3, 4 }, { 62, 63, 65, 2, 2 }, { 66, 67, 69, 1, 1 } };

/* One pass over the graph. Planning, it sizes the arena; recording, it emits every
 * dispatch. The same code both times is what makes the plan sufficient. */
static int build(struct nr_frame *f)
{
    int H = f->height, W = f->width;
    size_t pixels = (size_t)H * W;
    int (*L)[3] = f->levels;

    /* separate names keep a captured graph's addresses valid across NR_INPUT_FP16 */
    int source16;
    if (f->opt.input_fp16) {
        NAMED(half_in, "features_host16", pixels * 16 * 2);
        source16 = half_in;
    } else {
        NAMED(source, "features", pixels * 16 * 4);
        NAMED(features16, "features16", pixels * 16 * 2);
        source16 = features16;
        if (!f->planning && X.begin()) FAILF("xmx_begin: %s", X.error());
        TRY(to_half(f, source, features16, pixels * 16));
    }
    if (f->opt.input_fp16 && !f->planning && X.begin()) FAILF("xmx_begin: %s", X.error());
    /* block 0 at full resolution: the skip the post block merges and, pooled, the
     * encoder's input; every published buffer is stored narrow. Its scratch is taken
     * first, because under the glue the stem's GEMM also writes its `value16`. */
    BLOCK(block0, 0, 1, WINDOW);
    struct scratch scratch0;
    TRY(block_scratch(f, block0, H, W, &scratch0));
    /* The stem is read only by block 0's feed-forward, which can make it itself — then
     * neither width of it is ever stored (`ffn_fused_stem`). */
    int made_stem = f->opt.fuse_glue && can_make_stem(f, block0, &scratch0);
    int stem = -1;
    if (!made_stem) {
        NAMED(stem_buffer, "stem", pixels * 32 * 4);
        stem = stem_buffer;
        if (f->opt.fuse_glue)
            TRY(gemm_dual(f, source16, f->adapter, stem, scratch0.value16, (unsigned)pixels, 32, 16));
        else
            TRY(gemm(f, source16, f->adapter, stem, (unsigned)pixels, 32, 16, NULL));
    }

    /* Block 0's output has two readers, the pool into the encoder and the published skip
     * the last block merges, and its closing residual can serve both itself; then the
     * float32 output is never stored. */
    int pooled_here = f->opt.fuse_glue && can_pool_output(f, block0, &scratch0);
    int raw = -1;
    if (!pooled_here) { NAMED(raw_buffer, "block0", pixels * 32 * 4); raw = raw_buffer; }
    NAMED(full_skip, "full_skip", pixels * 32 * 2);
    int h = L[1][0], w = L[1][1], C = L[1][2];
    NAMED(l1, "l1", (size_t)h * w * 32 * 2);
    int value = l1;
    struct made_stem stem_from = { source16, f->adapter };
    int pool[2] = { value, full_skip };
    TRY(record_block_made(f, block0, H, W, stem, raw, 0, 0, 0,
                          f->opt.fuse_glue && !made_stem ? scratch0.value16 : -1, &scratch0,
                          NULL, made_stem ? &stem_from : NULL, pooled_here ? pool : NULL, NULL));
    if (pooled_here) {
        /* both made by the block's own closing pass */
    } else if (f->opt.fuse_glue && H % 2 == 0 && W % 2 == 0) {
        TRY(pool2_skip(f, raw, value, full_skip, H, W, 32));
    } else {
        TRY(independent(f, 1));
        TRY(e4m3_half(f, raw, full_skip, pixels * 32));
        TRY(pool2(f, raw, value, H, W, 32, EPI_E4M3, 1, 0));
        TRY(independent(f, 0));
    }

    int skips[7] = { -1, -1, -1, -1, -1, -1, -1 };
    int level = 1;
    for (int e = 0; e < 4; e++) {
        h = L[level][0]; w = L[level][1]; C = L[level][2];
        for (int index = ENCODER[e].first; index <= ENCODER[e].last; index++) {
            BLOCK(b, index, ENCODER[e].heads, WINDOW);
            TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
        }
        /* The level's own buffer is its skip. The decoder writes d1-d4 and nothing writes
         * l1-l4 again in the frame, so a copy would move the same bytes into a second
         * buffer for nothing. */
        skips[level] = value;
        BLOCK(tb, ENCODER[e].transition, ENCODER[e].heads, WINDOW);
        EDGE(down, ENCODER[e].transition, 0);
        int nh = L[level + 1][0], nw = L[level + 1][1], nC = L[level + 1][2];
        NAMED(unpublished, "unpublished", (size_t)h * w * C * 4);
        NAMEDF(nxt, "l%d", level + 1, (size_t)nh * nw * nC * 2);
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)pad8(h) * pad8(w) * C, &t));
        TRY(record_block(f, tb, h, w, value, unpublished, 0, 1, 0, -1, NULL));
        TRY(record_downsample(f, down, &t, unpublished, nxt, h, w, (unsigned)C,
                              ENCODER[e].transition == 22 ? 8 : 0, 0, 1));
        value = nxt;
        level++;
    }

    /* the split family, then the bottleneck */
    h = L[5][0]; w = L[5][1]; C = L[5][2];
    for (int index = 23; index <= 30; index++) {
        BLOCK(b, index, 16, SPLIT);
        TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
    }
    /* l5 itself is the skip, as for the levels above: the decoder input merge below
     * writes d5 rather than l5, which is what a copy would protect. */
    int split_skip = value;
    int gh = L[6][0], gw = L[6][1], gC = L[6][2];
    int tokens = gh * gw;
    /* With the glue fused the eight blocks run in their scratch's half value itself
     * (`record_global_block`, `chain`), and the downsample writes it. */
    int chain = f->opt.fuse_glue;
    int deep;
    if (chain) {
        BLOCK(b31, 31, 32, GLOBAL);
        struct gscratch s31;
        TRY(global_scratch(f, b31, tokens, &s31));
        deep = s31.io16;
    } else {
        NAMED(l6, "l6", (size_t)gh * gw * gC * 2);
        deep = l6;
    }
    {
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)pad8(h) * pad8(w) * C, &t));
        TRY(record_plain_downsample(f, &f->bottleneck, &t, value, deep, h, w, (unsigned)C, 8, 1, 1));
    }

    for (int index = 31; index <= 38; index++) {
        BLOCK(b, index, 32, GLOBAL);
        struct gscratch s;
        TRY(global_scratch(f, b, tokens, &s));
        if (chain) { TRY(record_global_block(f, b, &s, 1)); continue; }
        TRY(from_half(f, deep, s.value, (size_t)tokens * gC));
        TRY(record_global_block(f, b, &s, 0));
        TRY(e4m3(f, s.out, s.out, (size_t)s.padded * gC));
        TRY(to_half(f, s.out, deep, (size_t)tokens * gC));
    }

    /* the decoder input merge, then the split family again */
    {
        NAMED(d5, "d5", (size_t)h * w * C * 2);
        value = d5;
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)h * w * C, &t));
        TRY(record_upsample_merge(f, &f->decoder_input, &t, deep, split_skip, value, gh, gw, h, w,
                                  (unsigned)gC, (unsigned)C, 1, 1, 1));
    }
    for (int index = 40; index <= 47; index++) {
        BLOCK(b, index, 16, SPLIT);
        TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
    }

    for (int d = 0; d < 4; d++) {
        int sl = DECODER[d].level;
        int sh = L[sl][0], sw = L[sl][1], sC = L[sl][2];
        EDGE(up, DECODER[d].transition, 1);
        NAMEDF(target, "d%d", sl, (size_t)sh * sw * sC * 2);
        struct tscratch t;
        TRY(transition_scratch(f, (size_t)sh * sw * (C > sC ? C : sC), &t));
        TRY(record_upsample_merge(f, up, &t, value, skips[sl], target, h, w, sh, sw, (unsigned)C,
                                  (unsigned)sC, 1, 1, 1));
        BLOCK(tb, DECODER[d].transition, DECODER[d].heads, WINDOW);
        TRY(record_block(f, tb, sh, sw, target, target, EPI_E4M3, 1, 1, -1, NULL));
        value = target; h = sh; w = sw; C = sC;
        for (int index = DECODER[d].first; index <= DECODER[d].last; index++) {
            BLOCK(b, index, DECODER[d].heads, WINDOW);
            TRY(record_block(f, b, h, w, value, value, EPI_E4M3, 1, 1, -1, NULL));
        }
    }

    /* back to full resolution, merged with block 0's output, then the head */
    BLOCK(block70, 70, 1, WINDOW);
    struct scratch scratch70;
    TRY(block_scratch(f, block70, H, W, &scratch70));
    /* The head reads block 70's output as half and nothing reads it as float32, so the
     * block's closing residual stores half itself — and with the fused window block the
     * head comes out of block 70's own pass, so neither is ever stored. */
    int fused_head = f->opt.fuse_head && can_fuse_window_block(f, block70, &scratch70);
    int out16 = -1;
    if (!fused_head) { NAMED(out16_buffer, "out16", pixels * 32 * 2); out16 = out16_buffer; }
    int head_target;
    if (f->opt.compact_head) { NAMED(head4, "head4", pixels * 4 * 4); head_target = head4; }
    else { NAMED(head16, "head", pixels * 16 * 4); head_target = head16; }
    /* Block 70's input is read only by its feed-forward, which can make it itself. */
    int merge_here = f->opt.fuse_glue && can_merge_input(f, block70, &scratch70);
    struct made_merge merge_from = { value, full_skip, f->merge_sincos, w };
    int merged = -1;
    if (!merge_here) { NAMED(merged_buffer, "merged", pixels * 32 * 4); merged = merged_buffer; }
    if (merge_here) {
        /* made inside the feed-forward */
    } else if (f->opt.fuse_glue) {
        TRY(upsample_merge(f, value, full_skip, f->merge_sincos, merged, scratch70.value16, H, W, w, 32));
    } else {
        NAMED(upsampled, "upsampled", pixels * 32 * 4);
        TRY(upsample2(f, value, upsampled, w, H, W, 32, 1));
        TRY(scale_channel(f, upsampled, f->merge_sin, merged, pixels * 32, 32, 0));
        TRY(residual(f, merged, full_skip, f->merge_cos, merged, pixels * 32, 32, 0, 0, 0, 1, 0, 0, 0, 0, 0));
    }
    int head_with[3] = { f->head, head_target, f->opt.compact_head ? 4 : 16 };
    TRY(record_block_made(f, block70, H, W, merged, out16, 0, 0, 1,
                          f->opt.fuse_glue && !merge_here ? scratch70.value16 : -1, &scratch70,
                          merge_here ? &merge_from : NULL, NULL, NULL, fused_head ? head_with : NULL));
    if (fused_head) {
        /* the head came out of the block's own pass */
    } else if (f->opt.compact_head) {
        /* only the four useful columns, at a row stride of four */
        struct gemm_opt o = { .sa = pixels * 32, .sb = 32 * 16, .sc = pixels * 4, .ldc = 4, .extra = 0x10000u };
        TRY(gemm(f, out16, f->head, head_target, (unsigned)pixels, 16, 32, &o));
    } else {
        TRY(gemm(f, out16, f->head, head_target, (unsigned)pixels, 16, 32, NULL));
    }

    if (!f->planning) {
        f->graph = X.graph_capture();
        if (f->graph < 0) { f->graph = -1; FAILF("xmx_graph_capture: %s", X.error()); }
    }
    return 0;
}

static void release_extent(struct nr_frame *f)
{
    if (f->graph >= 0) { X.graph_destroy(f->graph); f->graph = -1; }
    for (int i = 0; i < f->named_count; i++) X.buf_destroy(f->named[i].id);
    f->named_count = 0;
    arena_reset(f);
    f->height = f->width = 0;
}

/* The graph for a network extent: planned, then recorded and captured once. */
static int prepare_extent(struct nr_frame *f, int H, int W)
{
    if (f->height == H && f->width == W && f->graph >= 0) return 0;
    if (H % 64 || W % 64 || H < 128 || W < 128)
        FAILF("network extent %dx%d is not a multiple of 64 of at least 128", H, W);
    release_extent(f);
    f->height = H; f->width = W;
    plan_levels(f, H, W);
    f->planning = 1;
    struct xmx saved;
    shadow_recorders(f, &saved);
    int planned = build(f);
    real_recorders(&saved);
    if (planned) { f->planning = 0; release_extent(f); return -1; }
    size_roles(f);
    f->planning = 0;
    if (build(f)) { X.abort(); release_extent(f); return -1; }
    size_t pixels = (size_t)H * W;
    free(f->features_host); free(f->head_host); free(f->noise); free(f->rows); free(f->cols);
    f->features_host = malloc(pixels * 16 * sizeof(float));
    f->head_host = malloc(pixels * 16 * sizeof(float));
    f->noise = malloc(pixels * 3 * sizeof(float));
    f->rows = malloc((size_t)H * sizeof(int32_t));
    f->cols = malloc((size_t)W * sizeof(int32_t));
    f->noise_index = INT32_MIN;
    if (!f->features_host || !f->head_host || !f->noise || !f->rows || !f->cols) FAILF("out of memory");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* features: nr_frame.build_features, on nr_image's pass                        */
/* ------------------------------------------------------------------------- */

/* `nr_frame.network_geometry`: at least `minimum` a side — never below the graph's own
 * 128 — rounded up to 64. 320 is the vendor's floor (`NetworkGeometry.vendor_aligned`). */
static int aligned_extent(int extent, int minimum)
{
    /* 0 is a zeroed struct that never saw nr_frame_defaults: the vendor's 320, as before */
    int floor = minimum <= 0 ? 320 : minimum > 128 ? minimum : 128;
    int least = extent > floor ? extent : floor;
    return (least + 63) / 64 * 64;
}

void nr_frame_geometry(int height, int width, int *network_height, int *network_width)
{
    nr_frame_geometry_min(height, width, 320, network_height, network_width);
}

void nr_frame_geometry_min(int height, int width, int minimum, int *network_height, int *network_width)
{
    if (network_height) *network_height = aligned_extent(height, minimum);
    if (network_width) *network_width = aligned_extent(width, minimum);
}

/* `NetworkGeometry.extended_indices`: the image at the origin, the extension mirroring
 * it without repeating the edge. */
static void extended_indices(int32_t *out, int count, int extent)
{
    for (int i = 0; i < count; i++) {
        int mirrored = 2 * extent - 2 - i;
        out[i] = i < extent ? i : (mirrored > 0 ? mirrored : 0);
    }
}

/* `features.deterministic_noise`, in float32 with NumPy's operation order. The last bit
 * of `logf`, `sqrtf`, `cosf` and `sinf` is the C library's rather than NumPy's, and the
 * two can differ there; `test_nr_frame_c.py` measures how often. */
static uint32_t shift_mix(uint32_t v)
{
    uint32_t shift = (v >> 28) + 4u;
    return (v ^ (v >> shift)) * 0x108EF2D9u;
}

static float uniform24(uint32_t v)
{
    uint32_t mixed = shift_mix(v);
    uint32_t bits = (mixed >> 30) ^ (mixed >> 8);
    return (float)(bits + 1u) * 5.960464477539063e-8f;
}

struct noise_args { float *out; int width, frame_index; };

/* Rows [y0, y1) of the noise; every pixel is its own hash, so the pool's bands do not
 * change a bit. */
static void noise_rows(const void *args, size_t y0, size_t y1)
{
    const struct noise_args *a = args;
    float *out = a->out;
    int width = a->width, frame_index = a->frame_index;
    const float tau = 6.2831854820251465f;
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = 0; x < width; x++) {
            uint32_t seed = (uint32_t)y * 0xD8163841u;
            seed ^= (uint32_t)x * 0x8DA6B343u;
            seed ^= (uint32_t)((uint32_t)frame_index * 0x9E3779B9u);
            seed ^= 0x243F6A88u;
            uint32_t m = shift_mix(seed);
            uint32_t mixed = m ^ (m >> 22);
            float ra = uniform24(mixed * 0xCAA5B80Du + 0x21DD796Bu);
            float ab = uniform24(mixed * 0x83232C31u + 0x3463E0ACu);
            float rb = uniform24(mixed * 0x2C9277B5u + 0xAC564B05u);
            float aa = uniform24(mixed * 0xFA6DC5F9u + 0x4712A88Eu);
            float radius_a = sqrtf(-2.0f * logf(ra));
            float radius_b = sqrtf(-2.0f * logf(rb));
            float angle_a = tau * aa, angle_b = tau * ab;
            float *o = out + ((size_t)y * width + x) * 3;
            o[0] = half_round(radius_b * cosf(angle_a));
            o[1] = half_round(radius_b * sinf(angle_a));
            o[2] = half_round(radius_a * cosf(angle_b));
        }
}

static void deterministic_noise(float *out, int height, int width, int frame_index)
{
    struct noise_args a = { out, width, frame_index };
    nr_parallel_rows((size_t)height, noise_rows, &a);
}

/* The control mask's green and blue into channels 11 and 12 over rows [y0, y1) —
 * `half(mask[..., 1] * np.float32(local_tone_strength))`: the raw strength, not the
 * half-rounded one the scalar path uses — as float, or as half bits. */
struct mask_args {
    const float *mask; const int32_t *rows, *cols; int width, W; float tone, structure;
    float *out; uint16_t *out16;
};

static void mask_rows(const void *args, size_t y0, size_t y1)
{
    const struct mask_args *a = args;
    for (size_t y = y0; y < y1; y++)
        for (int x = 0; x < a->W; x++) {
            const float *m = a->mask + ((size_t)a->rows[y] * a->width + a->cols[x]) * 3;
            size_t at = (y * a->W + x) * 16;
            if (a->out16) {
                a->out16[at + 11] = float_to_half(m[1] * a->tone);
                a->out16[at + 12] = float_to_half(m[2] * a->structure);
            } else {
                a->out[at + 11] = half_round(m[1] * a->tone);
                a->out[at + 12] = half_round(m[2] * a->structure);
            }
        }
}

/* `make_features`' three ways of filling channels 10-14: a plain recipe, the automatic
 * mask (skin and automatic-mask structure, -1 following the local structure), or a
 * per-pixel control mask, whose green and blue scale tone and structure per pixel and
 * zero the three structure scalars. */
static int features_into(struct nr_frame *f, const float *colour, int height, int width, const float *history,
                         const float *mask, const nr_frame_params *p, float *out, uint16_t *out16)
{
    int H = f->height, W = f->width;
    if (f->noise_index != p->frame_index) {
        deterministic_noise(f->noise, H, W, p->frame_index);
        f->noise_index = p->frame_index;
    }
    extended_indices(f->rows, H, height);
    extended_indices(f->cols, W, width);
    float controls[5] = { half_round(p->normalized_style), half_round(p->local_tone),
                          half_round(p->local_structure), -1.0f, -1.0f };
    if (mask) {
        controls[2] = controls[3] = controls[4] = 0.0f;
    } else if (p->automatic_mask) {
        int enabled = (p->skin_structure > p->automatic_structure ? p->skin_structure : p->automatic_structure) >= 0.0f;
        controls[2] = half_round(enabled ? 1.0f : p->local_structure);
        controls[3] = half_round(enabled ? (p->skin_structure >= 0.0f ? p->skin_structure : p->local_structure) : -1.0f);
        controls[4] = half_round(enabled ? (p->automatic_structure >= 0.0f ? p->automatic_structure : p->local_structure) : -1.0f);
    }
    /* `out16`, when given, takes the features as half — the graph's own input under
     * NR_INPUT_FP16. Every feature is a half value already, so this is the float32 build
     * rounded, bit for bit (test_native_image.py). */
    if (out16)
        nr_features_half(colour, (ptrdiff_t)width * 3, 3, 1,
                         history, history ? (ptrdiff_t)width * 3 : 0, history ? 3 : 0, history ? 1 : 0,
                         f->rows, f->cols, (size_t)H, (size_t)W, f->noise, controls, out16);
    else
        nr_features(colour, (ptrdiff_t)width * 3, 3, 1,
                    history, history ? (ptrdiff_t)width * 3 : 0, history ? 3 : 0, history ? 1 : 0,
                    f->rows, f->cols, (size_t)H, (size_t)W, f->noise, controls, out);
    if (mask) {
        struct mask_args a = { mask, f->rows, f->cols, width, W, p->local_tone, p->local_structure,
                               out, out16 };
        nr_parallel_rows((size_t)H, mask_rows, &a);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* the network                                                                  */
/* ------------------------------------------------------------------------- */

static double now(void) { return nr_now(); }

/* NR_INPUT_FP16's input: where the half features go — the mapped input itself when it is
 * mapped, else the host scratch `run_graph` then writes across. */
static uint16_t *input_half(struct nr_frame *f)
{
    int source = named_buffer(f, "features_host16", (size_t)f->height * f->width * 16 * 2);
    if (source < 0) return NULL;
    return X.buf_host_visible(source) ? (uint16_t *)X.buf_ptr(source) : (uint16_t *)f->head_host;
}

/* features (H, W, 16) at the network extent -> the head (H, W, 16) as the device holds
 * it: the first four channels of each sixteen are the head. `features` NULL means they
 * were built as half by `input_half` already (NR_INPUT_FP16 only). */
static const float *run_graph(struct nr_frame *f, const float *features)
{
    size_t pixels = (size_t)f->height * f->width;
    int fp16 = f->opt.input_fp16, compact = f->opt.compact_head;
    int source = fp16 ? named_buffer(f, "features_host16", pixels * 16 * 2)
                      : named_buffer(f, "features", pixels * 16 * 4);
    int head = compact ? named_buffer(f, "head4", pixels * 4 * 4) : named_buffer(f, "head", pixels * 16 * 4);
    if (source < 0 || head < 0 || (!features && !fp16)) {
        snprintf(last_error, sizeof last_error, "graph buffers are missing");
        return NULL;
    }
    double t0 = now();
    if (fp16) {
        /* NR_INPUT_FP16: the features rounded to half on the host, as NumPy's astype does,
         * straight into the mapped input when it is mapped; no to_half pass on the device.
         * Built there already by the caller, nothing is left but the upload of an unmapped
         * one. */
        size_t n = pixels * 16;
        uint16_t *dst = input_half(f);
        if (features) nr_to_half(features, n, dst);
        if (!X.buf_host_visible(source) && host_write(source, dst, n * 2)) return NULL;
    } else if (host_write(source, features, pixels * 16 * 4)) {
        return NULL;
    }
    double t1 = now();
    if (X.graph_run(f->graph) < 0) {
        snprintf(last_error, sizeof last_error, "xmx_graph_run: %s", X.error());
        return NULL;
    }
    double t2 = now();
    f->head_stride = compact ? 4 : 16;
    const float *out = host_read(head, f->head_host, pixels * (size_t)f->head_stride * 4);
    f->split[0] = t1 - t0; f->split[1] = t2 - t1; f->split[2] = now() - t2;
    return out;
}

double nr_frame_split(const nr_frame *f, int which) { return which >= 0 && which < 3 ? f->split[which] : 0.0; }

/* ------------------------------------------------------------------------- */
/* composition: nr_frame.compose                                                */
/* ------------------------------------------------------------------------- */

static float unit(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

/* One axis of the detail blur over rows [y0, y1): along x (`axis` 1) or along y (0).
 * Rows are independent, so the pool's bands give the same bytes as one thread. */
struct blur_args { const float *kernel, *in; float *out; int taps, extent, height, width, axis; };

static void blur_rows(const void *args, size_t y0, size_t y1)
{
    const struct blur_args *b = args;
    const float *kernel = b->kernel, *in = b->in;
    int taps = b->taps, extent = b->extent, height = b->height, width = b->width;
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = 0; x < width; x++)
            for (int c = 0; c < 3; c++) {
                float acc = 0.0f;
                for (int k = 0; k < taps; k++) {
                    size_t at;
                    if (b->axis) {
                        int sx = x + k - extent;
                        sx = sx < 0 ? 0 : sx >= width ? width - 1 : sx;
                        at = ((size_t)y * width + sx) * 3 + c;
                    } else {
                        int sy = y + k - extent;
                        sy = sy < 0 ? 0 : sy >= height ? height - 1 : sy;
                        at = ((size_t)sy * width + x) * 3 + c;
                    }
                    acc += kernel[k] * in[at];
                }
                b->out[((size_t)y * width + x) * 3 + c] = acc;
            }
}

/* The two element-wise passes of `compose_detail` over rows [y0, y1): the change, and
 * the result from it and its low-pass. Each element is its own, so the pool's bands give
 * the same bytes as one thread. */
struct detail_args {
    const float *source; float *output, *change; const float *lowpass; int width;
    float colour_s, detail;
};

static void change_rows(const void *args, size_t y0, size_t y1)
{
    const struct detail_args *d = args;
    for (size_t i = y0 * d->width * 3; i < y1 * d->width * 3; i++)
        d->change[i] = d->output[i] - d->source[i];
}

static void detail_rows(const void *args, size_t y0, size_t y1)
{
    const struct detail_args *d = args;
    for (size_t i = y0 * d->width * 3; i < y1 * d->width * 3; i++) {
        float lowpass = d->lowpass[i];
        float term_low = d->colour_s * lowpass;
        float highpass = d->change[i] - lowpass;
        float term_high = d->detail * highpass;
        d->output[i] = unit(d->source[i] + term_low + term_high);
    }
}

/* `compose_detail`: result = source + colour * lowpass(change) + detail * highpass(change).
 * The kernel is `gaussian_kernel` and the blur the NumPy one (edge replication, one
 * axis then the other, accumulated in kernel order); `expf` stands where NumPy's
 * float32 exponential stood, and the two can differ in the last bit of a weight. */
static int compose_detail(struct nr_frame *f, const float *source, float *output, int height, int width,
                          float detail, float colour_s, float radius)
{
    if (detail == 1.0f && colour_s == 1.0f) return 0;
    if (!(radius > 0.0f) || !isfinite(radius) || !isfinite(detail) || !isfinite(colour_s))
        FAILF("strengths and radius must be finite and the radius positive");
    int extent = (int)ceilf(3.0f * radius);
    int taps = 2 * extent + 1;
    float *kernel = malloc((size_t)taps * sizeof(float));
    if (!kernel) FAILF("out of memory");
    float sum = 0.0f;
    float denominator = (float)(2.0 * (double)radius * (double)radius);   /* np.float32(2 * sigma * sigma) */
    for (int i = 0; i < taps; i++) {
        float o = (float)(i - extent);
        kernel[i] = expf(-o * o / denominator);
        sum += kernel[i];
    }
    for (int i = 0; i < taps; i++) kernel[i] = kernel[i] / sum;
    size_t pixels = (size_t)height * width, n = pixels * 3;
    /* three frame-sized buffers kept between frames: the change, and the blur's two axes */
    if (f->composed_pixels < pixels) {
        free(f->scratch_a); free(f->scratch_b); free(f->composed);
        f->scratch_a = malloc(n * sizeof(float));
        f->scratch_b = malloc(n * sizeof(float));
        f->composed = malloc(n * sizeof(float));
        f->composed_pixels = f->scratch_a && f->scratch_b && f->composed ? pixels : 0;
        if (!f->composed_pixels) { free(kernel); FAILF("out of memory"); }
    }
    float *change = f->scratch_a, *low = f->scratch_b, *vertical = f->composed;
    struct detail_args d = { source, output, change, NULL, width, colour_s, detail };
    nr_parallel_rows((size_t)height, change_rows, &d);
    /* horizontal, into `low`; then vertical, into `vertical` */
    struct blur_args across = { kernel, change, low, taps, extent, height, width, 1 };
    nr_parallel_rows((size_t)height, blur_rows, &across);
    struct blur_args down = { kernel, low, vertical, taps, extent, height, width, 0 };
    nr_parallel_rows((size_t)height, blur_rows, &down);
    d.lowpass = vertical;
    nr_parallel_rows((size_t)height, detail_rows, &d);
    free(kernel);
    return 0;
}

/* `compose_head` with a mask over rows [y0, y1). */
struct masked_args {
    const float *head, *colour, *mask; ptrdiff_t hy, hx; int width; float intensity; float *output;
};

static void masked_rows(const void *args, size_t y0, size_t y1)
{
    const struct masked_args *a = args;
    ptrdiff_t s = (ptrdiff_t)a->width * 3;
    for (int y = (int)y0; y < (int)y1; y++)
        for (int x = 0; x < a->width; x++) {
            const float *h = a->head + (ptrdiff_t)y * a->hy + (ptrdiff_t)x * a->hx;
            const float *rgb = a->colour + (size_t)y * s + (size_t)x * 3;
            float blend = a->mask[(size_t)y * s + (size_t)x * 3] * a->intensity;
            if (a->intensity <= 1.0f) blend = unit(blend);
            for (int c = 0; c < 3; c++) {
                float source = rgb[c];
                float predicted = unit(source + half_round(h[c]) * 0.25f);
                a->output[((size_t)y * a->width + x) * 3 + c] = unit(source + blend * (predicted - source));
            }
        }
}

/* `nr_frame.gate_table` for the blend scale in `p`, built once per scale. */
static int gate_table(struct nr_frame *f, const nr_frame_params *p)
{
    if (f->gate_table && f->gate_scale == p->blend_scale) return 0;
    if (!f->gate_table && !(f->gate_table = malloc((1u << 16) * sizeof(float))))
        FAILF("out of memory");
    float scale = half_round(p->blend_scale);
    for (uint32_t bits = 0; bits < (1u << 16); bits++) {
        float logit = half_to_float((uint16_t)bits);
        f->gate_table[bits] = unit(1.0f / (1.0f + expf(-logit)) * scale);
    }
    f->gate_scale = p->blend_scale;
    return 0;
}

/* `nr_frame.compose`. The head arrives with the device's stride of sixteen floats per
 * pixel over the network extent, and is read through it: no crop copy. */
/* `head` has `hy` floats per row and `hx` per pixel: 16 straight off the device, 4 for a
 * cropped head a caller hands back. `mask` is the control mask's RGB, its red the blend. */
static int compose(struct nr_frame *f, const float *head, ptrdiff_t hy, ptrdiff_t hx, const float *colour,
                   int height, int width, const float *history, const float *previous, const float *mask,
                   const nr_frame_params *p, float *output)
{
    ptrdiff_t s = (ptrdiff_t)width * 3;
    if (history) {
        /* `history_weight`: clip(sigmoid(half(logit)) * half(blend_scale), 0, 1), then the
         * confidence. `expf` here against NumPy's exp there: the one place the temporal
         * path can differ from the Python by a last bit. */
        /* The logit is rounded to half first, so the gate has 65536 inputs: the table below,
         * built once per blend scale and indexed in the pass by the half's bits, as
         * `nr_frame.gate_table` is in the Python — one expf per half value, not per pixel. */
        TRY(gate_table(f, p));
        float confidence = p->history_confidence != 1.0f ? unit(p->history_confidence) : 1.0f;
        nr_compose_temporal(head, hy, hx, 1, colour, s, 3, 1, history, s, 3, 1,
                            previous, previous ? s : 0, previous ? 3 : 0, previous ? 1 : 0,
                            NULL, 0, 0, f->gate_table, confidence,
                            mask, mask ? s : 0, mask ? 3 : 0, (size_t)height, (size_t)width,
                            p->intensity, p->blend_scale, previous ? p->hold : 0.0f, previous ? p->slope : 0.0f,
                            previous ? p->release : 0.0f,
                            output);
    } else if (mask) {
        /* `compose_head` with a mask: blend = clip(red * intensity, 0, 1) — and past
         * intensity 1 `nr_frame.compose` takes its own branch, where the blend is not
         * clamped so that it can extrapolate */
        struct masked_args a = { head, colour, mask, hy, hx, width, p->intensity, output };
        nr_parallel_rows((size_t)height, masked_rows, &a);
    } else {
        nr_compose(head, hy, hx, 1, colour, s, 3, 1, (size_t)height, (size_t)width, p->intensity, output);
    }
    return compose_detail(f, colour, output, height, width, p->detail_strength, p->colour_strength,
                          p->detail_radius);
}

/* ------------------------------------------------------------------------- */
/* the public API                                                               */
/* ------------------------------------------------------------------------- */

void nr_frame_defaults(nr_frame_params *p)
{
    memset(p, 0, sizeof *p);
    p->intensity = 1.0f; p->detail_strength = 1.0f; p->colour_strength = 1.0f; p->detail_radius = 4.0f;
    p->normalized_style = 0.0f; p->local_tone = 1.0f; p->local_structure = 1.0f; p->frame_index = 0;
    p->history_confidence = 1.0f; p->blend_scale = 0.73974609375f; p->hold = 0.0f; p->slope = 0.0f;
    p->release = 0.0f;
    p->min_extent = 320;
    p->automatic_mask = 0; p->skin_structure = -1.0f; p->automatic_structure = -1.0f;
}

const char *nr_frame_runtime(void)
{
#if defined(NR_STATIC_METAL)
    return "metal";
#elif defined(NR_STATIC_D3D12)
    return "d3d12";
#elif defined(NR_STATIC_XMX)
    return "vulkan";
#else
    const char *backend = getenv("NR_GPU_BACKEND");
    if (backend && !strcmp(backend, "metal")) return "metal";
    if (backend && !strcmp(backend, "d3d12")) return "d3d12";
    return "vulkan";
#endif
}

int nr_frame_adopt_vulkan(void *instance, void *physical_device, void *device, void *queue,
                          unsigned queue_family, int cooperative_matrix,
                          void *get_instance_proc_addr, void (*lock)(void *),
                          void (*unlock)(void *), void *lock_context)
{
    if (xmx_is_ready) FAILF("the device is open; nr_frame_shutdown first");
    if (!strcmp(nr_frame_runtime(), "d3d12"))
        FAILF("the runtime behind this library is Direct3D 12 (libd3dmx): nr_frame_adopt_d3d12 is the call");
    if (xmx_load()) return -1;
    if (!X.adopt) FAILF("this libxmx has no xmx_adopt");
    if (X.adopt(instance, physical_device, device, queue, queue_family, cooperative_matrix,
                get_instance_proc_addr, lock, unlock, lock_context))
        FAILF("xmx_adopt: %s", X.error());
    return 0;
}

int nr_frame_adopt_d3d12(void *device, void *queue, void (*lock)(void *), void (*unlock)(void *),
                         void *lock_context)
{
    if (xmx_is_ready) FAILF("the device is open; nr_frame_shutdown first");
    if (strcmp(nr_frame_runtime(), "d3d12"))
        FAILF("the runtime behind this library is %s, not Direct3D 12: nothing to adopt the device into",
              nr_frame_runtime());
    if (xmx_load()) return -1;
    if (!X.adopt) FAILF("this libd3dmx has no xmx_adopt");
    /* libd3dmx reads the device and the queue from libxmx's argument list and wants the
     * Vulkan-only arguments NULL */
    if (X.adopt(NULL, NULL, device, queue, 0, 0, NULL, lock, unlock, lock_context))
        FAILF("xmx_adopt: %s", X.error());
    return 0;
}

void nr_frame_shutdown(void)
{
    if (X.handle && X.close) X.close();
    xmx_is_ready = 0;
}

int nr_frame_shared_device(void)
{
    return (X.handle && X.adopted) ? X.adopted() : -1;
}

/* An NR_* switch as `os.environ.get(name, default) != "0"` reads it. */
static int env_switch(const char *name, int fallback)
{
    const char *value = getenv(name);
    if (!value) return fallback;
    return strcmp(value, "0") != 0;
}

nr_frame *nr_frame_open(const char *weights_path)
{
    if (xmx_ready()) return NULL;
    struct nr_frame *f = calloc(1, sizeof *f);
    if (!f) FAILP("out of memory");
    f->graph = -1;
    for (int r = 0; r < R_COUNT; r++) f->role_id[r] = -1;
    /* buffer ids start at zero, so "none" has to be -1 from the start */
    f->adapter = f->merge_sin = f->merge_cos = f->merge_sincos = f->head = -1;
    f->head_stride = 16;
    f->bottleneck.weight0 = f->bottleneck.sine = -1;
    f->decoder_input.weight0 = f->decoder_input.sine = -1;
    /* `xmxres.Runtime.__init__`, switch for switch and default for default */
    f->opt.fuse_qk = env_switch("NR_FUSE_QK", 1);
    f->opt.batch_ffn = env_switch("NR_BATCH_FFN", 1);
    /* Both on since 2026-09-25, as in xmxres.Runtime: the features are built as half
     * straight into the mapped input, and the last GEMM stores only the four head columns
     * the host reads. The same bytes; =0 restores the float32 input and sixteen columns. */
    f->opt.input_fp16 = env_switch("NR_INPUT_FP16", 1);
    f->opt.compact_head = env_switch("NR_COMPACT_HEAD", 1);
    f->opt.joint_qkv = env_switch("NR_JOINT_QKV", 0);
    f->opt.fuse_residual = env_switch("NR_FUSE_RESIDUAL", 1);
    f->opt.fuse_window_residual = env_switch("NR_FUSE_WINDOW_RESIDUAL", 1);
    f->opt.fuse_window_attention = env_switch("NR_FUSE_WINDOW_ATTENTION", 1);
    f->opt.fuse_attention_merge = env_switch("NR_FUSE_ATTENTION_MERGE", 1);
    f->opt.qkv_epilogue = env_switch("NR_QKV_EPILOGUE", 1);
    f->opt.fuse_glue = env_switch("NR_FUSE_GLUE", 1);
    f->opt.fuse_ffn = env_switch("NR_FUSE_FFN", 1);
    f->opt.fuse_branched_ffn = env_switch("NR_FUSE_BRANCHED_FFN", 0);
    /* off where the runtime cannot gather a window-ordered A, as xmxres.py decides it */
    f->opt.fuse_partition = env_switch("NR_FUSE_PARTITION", 1)
                            && X.window_gather && X.window_gather() == 1;
    /* improve-b.md, as xmxres.Runtime reads them: the pool off where the runtime can pool
     * nowhere (the same condition as the partition's) */
    f->opt.fuse_transition = env_switch("NR_FUSE_TRANSITION", 1);
    f->opt.fuse_merge_ffn = env_switch("NR_FUSE_MERGE_FFN", 1);
    f->opt.fuse_stem_ffn = env_switch("NR_FUSE_STEM_FFN", 1);
    f->opt.fuse_pool = env_switch("NR_FUSE_POOL", 1) && X.window_gather && X.window_gather() == 1;
    f->opt.fuse_window_block = env_switch("NR_FUSE_WINDOW_BLOCK", 1);
    f->opt.fuse_head = env_switch("NR_FUSE_HEAD", 1);
    f->opt.fuse_global_attention = env_switch("NR_FUSE_GLOBAL_ATTENTION", 1);
    if (weights_load(&f->w, weights_path)) { free(f); return NULL; }
    /* the six named weights `DeviceWeights` uploads before any block */
    const struct tensor *adapter = weight(&f->w, "block0.layer0.input_adapter_weight");
    const struct tensor *bottleneck = weight(&f->w, "block30.layer4.weight");
    const struct tensor *conv = weight(&f->w, "block39.layer0.conv_weight");
    const struct tensor *upsine = weight(&f->w, "block39.layer0.inp_upsample_sin");
    const struct tensor *msin = weight(&f->w, "block70.layer0.inp_merge_sin");
    const struct tensor *mcos = weight(&f->w, "block70.layer0.inp_merge_cos");
    const struct tensor *gain = weight(&f->w, "block70.layer0.out_gain");
    const struct tensor *oconv = weight(&f->w, "block70.layer0.out_conv_weight");
    if (!adapter || !bottleneck || !conv || !upsine || !msin || !mcos || !gain || !oconv) {
        nr_frame_close(f);
        FAILP("%s: the edge weights are missing; is this the logical file?", weights_path ? weights_path : "embedded weights");
    }
    if ((f->adapter = buffer_f16(adapter->data, adapter->count)) < 0) goto fail;
    if ((f->bottleneck.weight0 = buffer_f16(bottleneck->data, bottleneck->count)) < 0) goto fail;
    f->bottleneck.out_channels = (int)bottleneck->shape[1]; f->bottleneck.sine = -1; f->bottleneck.loaded = 1;
    if ((f->decoder_input.weight0 = buffer_f16(conv->data, conv->count)) < 0) goto fail;
    if ((f->decoder_input.sine = buffer_f32(upsine->data, upsine->count)) < 0) goto fail;
    f->decoder_input.out_channels = (int)conv->shape[1]; f->decoder_input.loaded = 1;
    if ((f->merge_sin = buffer_f32(msin->data, msin->count)) < 0) goto fail;
    if ((f->merge_cos = buffer_f32(mcos->data, mcos->count)) < 0) goto fail;
    {
        /* both, one after the other, for the pass that applies them together */
        float *sincos = malloc((msin->count + mcos->count) * sizeof(float));
        if (!sincos) { snprintf(last_error, sizeof last_error, "out of memory"); goto fail; }
        memcpy(sincos, msin->data, msin->count * sizeof(float));
        memcpy(sincos + msin->count, mcos->data, mcos->count * sizeof(float));
        f->merge_sincos = buffer_f32(sincos, msin->count + mcos->count);
        free(sincos);
        if (f->merge_sincos < 0) goto fail;
    }
    /* the head is 32 -> 4 in a (32, 16) matrix padded to the tile: gain over the first
     * sixteen rows, the convolution over the second, the first four columns the head */

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    if (gain->count != 64 || oconv->count != 64) { sprintf_s(last_error, sizeof last_error, "head weights are not (16, 4)"); goto fail; }
#else
    if (gain->count != 64 || oconv->count != 64) { snprintf(last_error, sizeof last_error, "head weights are not (16, 4)"); goto fail; }
#endif

    float headm[32 * 16] = { 0 };
    for (int r = 0; r < 16; r++)
        for (int c = 0; c < 4; c++) {
            headm[r * 16 + c] = gain->data[r * 4 + c];
            headm[(16 + r) * 16 + c] = oconv->data[r * 4 + c];
        }
    if ((f->head = buffer_f16(headm, 32 * 16)) < 0) goto fail;
    return f;
fail:
    nr_frame_close(f);
    return NULL;
}

void nr_frame_close(nr_frame *f)
{
    if (!f) return;
    release_extent(f);
    int *ids[] = { &f->adapter, &f->merge_sin, &f->merge_cos, &f->head,
                   &f->bottleneck.weight0, &f->decoder_input.weight0, &f->decoder_input.sine };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) if (*ids[i] >= 0) X.buf_destroy(*ids[i]);
    for (int i = 0; i < 71; i++) {
        struct block_w *b = &f->blocks[i];
        if (b->loaded) {
            int *bid[] = { &b->qkv, &b->out, &b->bias, &b->scale, &b->attn_cos, &b->ffn_cos, &b->expand,
                           &b->branch, &b->ffn_out, &b->first, &b->project, &b->weight3, &b->ffn_proj };
            for (size_t k = 0; k < sizeof bid / sizeof *bid; k++) if (*bid[k] >= 0) X.buf_destroy(*bid[k]);
        }
        if (f->edges[i].loaded) {
            X.buf_destroy(f->edges[i].weight0);
            if (f->edges[i].sine >= 0) X.buf_destroy(f->edges[i].sine);
        }
    }
    weights_free(&f->w);
    free(f->requests);
    free(f->features_host); free(f->head_host); free(f->noise); free(f->rows); free(f->cols);
    free(f->scratch_a); free(f->scratch_b); free(f->composed);
    free(f->gate_table);
    free(f);
}

const char *nr_frame_device(nr_frame *f) { (void)f; return X.device ? X.device() : "not opened"; }
const char *nr_frame_gemm_path(nr_frame *f) { (void)f; return X.path ? X.path() : "not opened"; }

int nr_frame_features_masked(nr_frame *f, const float *colour, int height, int width, const float *history,
                             const float *control_mask, const nr_frame_params *params, float *features)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !colour || !features) FAILF("features need a colour image and an output");
    if (prepare_extent(f, aligned_extent(height, params->min_extent), aligned_extent(width, params->min_extent))) return -1;
    return features_into(f, colour, height, width, history, control_mask, params, features, NULL);
}

int nr_frame_features(nr_frame *f, const float *colour, int height, int width, const float *history,
                      const nr_frame_params *params, float *features)
{
    return nr_frame_features_masked(f, colour, height, width, history, NULL, params, features);
}

int nr_frame_compose(nr_frame *f, const float *head, const float *colour, int height, int width,
                     const float *history, const float *previous, const float *control_mask,
                     const nr_frame_params *params, float *output)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !head || !colour || !output) FAILF("compose needs a head, a colour image and an output");
    return compose(f, head, (ptrdiff_t)width * 4, 4, colour, height, width, history, previous, control_mask,
                   params, output);
}

/* `nr_image._axis_plan`: pixel centres onto the source, the floor and its neighbour
 * clamped, the fraction as the weight — float32, as NumPy computes them. */
static void axis_plan(int extent, int count, int32_t *low, int32_t *high, float *weight)
{
    float ratio = (float)((double)extent / (double)count);
    for (int i = 0; i < count; i++) {
        float centre = ((float)i + 0.5f) * ratio - 0.5f;
        float floored = floorf(centre);
        int lo = floored < 0.0f ? 0 : floored > (float)(extent - 1) ? extent - 1 : (int)floored;
        low[i] = lo;
        high[i] = lo + 1 > extent - 1 ? extent - 1 : lo + 1;
        float frac = centre - (float)lo;
        weight[i] = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
    }
}

int nr_frame_compose_encode(nr_frame *f, const float *head, int head_height, int head_width,
                            const float *colour, int height, int width, const float *history,
                            const float *previous, const float *control_mask,
                            const nr_frame_params *params, float *output, unsigned char *encoded,
                            int frame_width, int top, int left, int bgra)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (!head || !colour || !output || !encoded || height <= 0 || width <= 0 || head_height <= 0
        || head_width <= 0 || head_height > height || head_width > width || top < 0 || left < 0
        || left + width > frame_width)
        FAILF("compose_encode needs a head no larger than the colour, and the colour inside the frame");
    int rows_plan = head_height != height, cols_plan = head_width != width;
    size_t plan_bytes = (size_t)(height + width) * (2 * sizeof(int32_t) + sizeof(float));
    unsigned char *plan = malloc(plan_bytes ? plan_bytes : 1);
    if (!plan) FAILF("out of memory");
    int32_t *low_y = (int32_t *)plan, *high_y = low_y + height;
    int32_t *low_x = high_y + height, *high_x = low_x + width;
    float *weight_y = (float *)(high_x + width), *weight_x = weight_y + height;
    if (rows_plan) axis_plan(head_height, height, low_y, high_y, weight_y);
    if (cols_plan) axis_plan(head_width, width, low_x, high_x, weight_x);
    ptrdiff_t s3 = (ptrdiff_t)width * 3;
    int fused = params->detail_strength == 1.0f && params->colour_strength == 1.0f
                && !(control_mask && !history);
    int rc = 0;
    if (fused) {
        /* `nr_frame.compose_encode`: where `compose_detail` hands the composition back
         * untouched, the upscale, the composition and the encoder in one pass */
        if (history && gate_table(f, params)) { free(plan); return -1; }
        float confidence = params->history_confidence != 1.0f ? unit(params->history_confidence) : 1.0f;
        size_t channels = history ? 4 : 3;
        nr_compose_encode(head, (ptrdiff_t)head_width * 4, 4, 1, (size_t)head_width, channels,
                          rows_plan ? low_y : NULL, rows_plan ? high_y : NULL, rows_plan ? weight_y : NULL,
                          cols_plan ? low_x : NULL, cols_plan ? high_x : NULL, cols_plan ? weight_x : NULL,
                          colour, s3, 3, 1,
                          history, history ? s3 : 0, history ? 3 : 0, history ? 1 : 0,
                          history ? previous : NULL, history && previous ? s3 : 0,
                          history && previous ? 3 : 0, history && previous ? 1 : 0,
                          history ? f->gate_table : NULL, confidence,
                          control_mask, control_mask ? s3 : 0, control_mask ? 3 : 0,
                          (size_t)height, (size_t)width, params->intensity,
                          params->blend_scale, previous ? params->hold : 0.0f,
                          previous ? params->slope : 0.0f, previous ? params->release : 0.0f,
                          output, encoded, (size_t)frame_width, (size_t)top, (size_t)left, bgra, NULL, 0);
    } else {
        /* the separate passes: the head brought up an axis at a time, composed, encoded */
        const float *full = head;
        float *middle = NULL, *up = NULL;
        if (rows_plan || cols_plan) {
            up = malloc((size_t)height * width * 4 * sizeof(float));
            if (rows_plan && cols_plan) middle = malloc((size_t)height * head_width * 4 * sizeof(float));
            if (!up || (rows_plan && cols_plan && !middle)) { free(up); free(middle); free(plan); FAILF("out of memory"); }
            const float *current = head;
            if (rows_plan) {
                float *target = cols_plan ? middle : up;
                nr_resize_axis(current, (ptrdiff_t)head_width * 4, 4, 1, (size_t)height, (size_t)head_width, 4,
                               0, low_y, high_y, weight_y, target);
                current = target;
            }
            if (cols_plan)
                nr_resize_axis(current, (ptrdiff_t)head_width * 4, 4, 1, (size_t)height, (size_t)width, 4,
                               1, low_x, high_x, weight_x, up);
            full = up;
        }
        rc = compose(f, full, (ptrdiff_t)width * 4, 4, colour, height, width, history, previous, control_mask,
                     params, output);
        if (!rc)
            for (int y = 0; y < height; y++) {
                unsigned char *row = encoded + ((size_t)(top + y) * frame_width + left) * 4;
                nr_encode8(output + (size_t)y * width * 3, s3, 3, 1, row, 1, (size_t)width, bgra, row);
            }
        free(up); free(middle);
    }
    free(plan);
    return rc;
}

/* The head's first four channels of each pixel, rows [0, height) and columns [0, width) of
 * the device's (hy, hx) layout, into a dense (height, width, 4): a row at a time where the
 * device already holds four columns (the compact head), else a pixel at a time. */
static void crop_head(const float *wide, ptrdiff_t hy, ptrdiff_t hx, int height, int width,
                      float *head)
{
    for (int y = 0; y < height; y++) {
        const float *from = wide + (size_t)y * hy;
        float *to = head + (size_t)y * width * 4;
        if (hx == 4) {
            memcpy(to, from, (size_t)width * 4 * sizeof(float));
        } else {
            for (int x = 0; x < width; x++)
                memcpy(to + (size_t)x * 4, from + (size_t)x * hx, 4 * sizeof(float));
        }
    }
}

int nr_frame_run_features(nr_frame *f, const float *features, int network_height, int network_width, float *head)
{
    if (!features || !head) FAILF("run_features needs features and a head");
    if (prepare_extent(f, network_height, network_width)) return -1;
    const float *wide = run_graph(f, features);
    if (!wide) return -1;
    crop_head(wide, (ptrdiff_t)network_width * f->head_stride, f->head_stride, network_height,
              network_width, head);
    return 0;
}

int nr_frame_update(nr_frame *f, const float *colour, int height, int width, const float *history,
                    const float *previous, const nr_frame_params *params, float *output, float *head_out)
{
    return nr_frame_update_masked(f, colour, height, width, history, previous, NULL, params, output, head_out);
}

int nr_frame_update_masked(nr_frame *f, const float *colour, int height, int width, const float *history,
                           const float *previous, const float *control_mask, const nr_frame_params *params,
                           float *output, float *head_out)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !colour || !output) FAILF("update needs a colour image and an output");
    if (prepare_extent(f, aligned_extent(height, params->min_extent), aligned_extent(width, params->min_extent))) return -1;
    /* Under NR_INPUT_FP16 the features are built as half in the graph's own input, so
     * neither a float32 copy nor a conversion stands between them and the first GEMM. */
    uint16_t *half_in = f->opt.input_fp16 ? input_half(f) : NULL;
    if (f->opt.input_fp16 && !half_in) FAILF("graph buffers are missing");
    if (features_into(f, colour, height, width, history, control_mask, params,
                      half_in ? NULL : f->features_host, half_in)) return -1;
    const float *wide = run_graph(f, half_in ? NULL : f->features_host);
    if (!wide) return -1;
    ptrdiff_t hx = f->head_stride, hy = (ptrdiff_t)f->width * hx;
    if (head_out) crop_head(wide, hy, hx, height, width, head_out);
    return compose(f, wide, hy, hx, colour, height, width, history, previous, control_mask, params, output);
}

int nr_frame_head(nr_frame *f, const float *colour, int height, int width, const float *history,
                  const float *control_mask, const nr_frame_params *params, float *head)
{
    nr_frame_params d;
    if (!params) { nr_frame_defaults(&d); params = &d; }
    if (height <= 0 || width <= 0 || !colour || !head) FAILF("head needs a colour image and an output");
    if (prepare_extent(f, aligned_extent(height, params->min_extent), aligned_extent(width, params->min_extent))) return -1;
    /* nr_frame_update's first half: the features as half in the graph's own input under
     * NR_INPUT_FP16, the graph, and the head cropped to the colour's extent. */
    uint16_t *half_in = f->opt.input_fp16 ? input_half(f) : NULL;
    if (f->opt.input_fp16 && !half_in) FAILF("graph buffers are missing");
    if (features_into(f, colour, height, width, history, control_mask, params,
                      half_in ? NULL : f->features_host, half_in)) return -1;
    const float *wide = run_graph(f, half_in ? NULL : f->features_host);
    if (!wide) return -1;
    ptrdiff_t hx = f->head_stride, hy = (ptrdiff_t)f->width * hx;
    crop_head(wide, hy, hx, height, width, head);
    return 0;
}
