/*
 * nr_frame — `src/ref/nr_frame.py`, the command, in C.
 *
 *     nr_frame IN.png OUT.png [--weights W] [--size HxW] [--profile P] [--style-index N]
 *              [--local-tone F] [--local-structure F] [--skin-structure F] [--auto-mask F]
 *              [--control-mask MASK.png] [--intensity F] [--intensity-ladder A,B,C]
 *              [--detail-strength F] [--colour-strength F] [--detail-radius F]
 *              [--frame-index N] [-v]
 *
 * The same flags, the same printed lines. Pictures are PNG, read and written with libpng:
 * any PNG in (8- or 16-bit, grey, palette, alpha — all reduced to 8-bit RGB), 8-bit RGB
 * out, the same `byte / 255` and `value * 255 + 0.5` as `image_io.py`. Where the Python
 * shells out to ImageMagick this does not, so `--size` is this project's own bilinear
 * resample (`nr_resize_axis`, the one the daemon uses) rather than ImageMagick's filter.
 * `--gpu`, `--resident` and `--accel` are accepted and mean nothing here: this program has
 * one backend, the resident graph in `libnr_frame`. The network runs once; the ladder
 * composes the head at each intensity as the Python does.
 */
#include "nr_frame.h"
#include "nr_image.h"
#include "nr_portable.h"

#include <png.h>
#include <setjmp.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) { return nr_now(); }

static void die(const char *what)
{
    fprintf(stderr, "nr_frame: %s\n", what);
    exit(1);
}

/* `image_io.load`, through libpng: (H, W, 3) float32 in [0, 1]. Sixteen-bit files are
 * reduced to eight as ImageMagick's `-depth 8` reduces them; grey, palette and alpha are
 * expanded or dropped. */

/* png_create_*_struct returns NULL when the libpng the binary was compiled against is not
 * the one it is running with (libpng refuses across major.minor); the file is not the
 * problem then, the build is, and the message should say so. */
static void png_version_mismatch(void)
{
    unsigned lib = png_access_version_number();
    if (lib / 100 != PNG_LIBPNG_VER / 100) {
        fprintf(stderr, "libpng mismatch: built against %s, running with %u.%u.%u; rebuild against the "
                        "headers of the libpng that is linked\n", PNG_LIBPNG_VER_STRING, lib / 10000, (lib / 100) % 100, lib % 100);
        exit(1);
    }
}
static float *read_png(const char *path, int *height, int *width)
{
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    FILE *f = NULL;
    fopen_s(&f, path, "rb");
#else
    FILE *f = fopen(path, "rb");
#endif

    if (!f) { perror(path); exit(1); }
    unsigned char sig[8];
    if (fread(sig, 1, 8, f) != 8 || png_sig_cmp(sig, 0, 8)) { fprintf(stderr, "%s: not a PNG\n", path); exit(1); }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) png_version_mismatch();
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) { fprintf(stderr, "%s: libpng could not read it\n", path); exit(1); }
    png_init_io(png, f);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);
    png_uint_32 w = png_get_image_width(png, info), h = png_get_image_height(png, info);
    int depth = png_get_bit_depth(png, info), type = png_get_color_type(png, info);
    if (type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (depth == 16) png_set_strip_16(png);
    if (type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_set_strip_alpha(png);
    png_set_packing(png);
    png_read_update_info(png, info);
    if (png_get_rowbytes(png, info) != (size_t)w * 3) { fprintf(stderr, "%s: unexpected row layout\n", path); exit(1); }
    unsigned char *raw = malloc((size_t)w * h * 3);
    png_bytep *rows = malloc(h * sizeof *rows);
    if (!raw || !rows) die("out of memory");
    for (png_uint_32 y = 0; y < h; y++) rows[y] = raw + (size_t)y * w * 3;
    png_read_image(png, rows);
    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(f);
    free(rows);
    size_t n = (size_t)w * h * 3;
    float *out = malloc(n * sizeof(float));
    if (!out) die("out of memory");
    for (size_t i = 0; i < n; i++) out[i] = (float)raw[i] / 255.0f;
    free(raw);
    *height = (int)h; *width = (int)w;
    return out;
}

/* `nr_image.bilinear`: the axis plan of `_axis_plan` — pixel centres mapped onto the
 * source, floor and its neighbour clamped, the fraction as the weight — then one axis at
 * a time through `nr_resize_axis`, the intermediate rounded to float32 between them. */
static float *resize(const float *source, int height, int width, int channels, int target_h, int target_w)
{
    const float *current = source;
    float *owned = NULL;
    int h = height, w = width;
    for (int axis = 0; axis < 2; axis++) {
        int extent = axis == 0 ? h : w, count = axis == 0 ? target_h : target_w;
        if (extent == count) continue;
        int32_t *low = malloc((size_t)count * sizeof *low), *high = malloc((size_t)count * sizeof *high);
        float *weight = malloc((size_t)count * sizeof *weight);
        if (!low || !high || !weight) die("out of memory");
        for (int i = 0; i < count; i++) {
            float centre = ((float)i + 0.5f) * ((float)extent / (float)count) - 0.5f;
            float floored = floorf(centre);
            int lo = (int)(floored < 0.0f ? 0.0f : floored > (float)(extent - 1) ? (float)(extent - 1) : floored);
            low[i] = lo;
            high[i] = lo + 1 > extent - 1 ? extent - 1 : lo + 1;
            float frac = centre - (float)lo;
            weight[i] = frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac;
        }
        int nh = axis == 0 ? count : h, nw = axis == 0 ? w : count;
        float *out = malloc((size_t)nh * nw * channels * sizeof(float));
        if (!out) die("out of memory");
        nr_resize_axis(current, (ptrdiff_t)w * channels, channels, 1, (size_t)nh, (size_t)nw, (size_t)channels,
                       axis, low, high, weight, out);
        free(low); free(high); free(weight); free(owned);
        owned = out; current = out; h = nh; w = nw;
    }
    if (!owned) {
        owned = malloc((size_t)h * w * channels * sizeof(float));
        if (!owned) die("out of memory");
        memcpy(owned, source, (size_t)h * w * channels * sizeof(float));
    }
    return owned;
}

static float *load_image(const char *path, int *height, int *width, int resize_h, int resize_w)
{
    float *image = read_png(path, height, width);
    if (resize_h && (resize_h != *height || resize_w != *width)) {
        float *resized = resize(image, *height, *width, 3, resize_h, resize_w);
        free(image);
        image = resized;
        *height = resize_h; *width = resize_w;
    }
    return image;
}

/* `image_io.save`: clip, `(a * 255 + 0.5)` to bytes, an 8-bit RGB PNG. */
static void save_image(const float *image, int height, int width, const char *path)
{
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    FILE *f = NULL;
    fopen_s(&f, path, "wb");
#else
    FILE *f = fopen(path, "wb");
#endif
    if (!f) { perror(path); exit(1); }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) png_version_mismatch();
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) { fprintf(stderr, "%s: libpng could not write it\n", path); exit(1); }
    png_init_io(png, f);
    png_set_IHDR(png, info, (png_uint_32)width, (png_uint_32)height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    size_t n = (size_t)height * width * 3;
    unsigned char *raw = malloc(n);
    png_bytep *rows = malloc((size_t)height * sizeof *rows);
    if (!raw || !rows) die("out of memory");
    for (size_t i = 0; i < n; i++) {
        float v = image[i];
        v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
        raw[i] = (unsigned char)(v * 255.0f + 0.5f);
    }
    for (int y = 0; y < height; y++) rows[y] = raw + (size_t)y * width * 3;
    png_write_image(png, rows);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    free(raw); free(rows);
}

static int readable(const char *path)
{
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    FILE *f = NULL;
    fopen_s(&f, path, "rb");
#else
    FILE *f = fopen(path, "rb");
#endif
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* The weights compiled into libnr_frame when the build has them (NULL asks for those), else
 * `ROOT / work / mlxw / dlssnr-logical.safetensors`: the weights are the owner's and live in
 * work/ whichever build made this executable: beside it when the Makefile put it in work/,
 * one directory up and over when CMake put it in `build/`, else relative to the caller. */
static const char *default_weights(char *buf, size_t cap)
{
    char dir[1024];
    if (nr_frame_embedded_weights_size()) return NULL;
    if (nr_dl_self_dir((const void *)&default_weights, dir, sizeof dir) == 0) {
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        sprintf_s(buf, cap, "%s/mlxw/dlssnr-logical.safetensors", dir);
#else
        snprintf(buf, cap, "%s/mlxw/dlssnr-logical.safetensors", dir);
#endif

        if (readable(buf)) return buf;
        
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
        sprintf_s(buf, cap, "%s/../work/mlxw/dlssnr-logical.safetensors", dir);
#else
        snprintf(buf, cap, "%s/../work/mlxw/dlssnr-logical.safetensors", dir);
#endif

        if (readable(buf)) return buf;
    }
    
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
    sprintf_s(buf, cap, "work/mlxw/dlssnr-logical.safetensors");
#else
    snprintf(buf, cap, "work/mlxw/dlssnr-logical.safetensors");
#endif

    return buf;
}

static const char *PROFILES[] = { "standard", "natural", "cinematic", "neutral", "vendor" };

static int profile(const char *name, nr_frame_params *p)
{
    /* `features.PROFILES` and `nr_frame.VENDOR_DEFAULTS` */
    if (!strcmp(name, "standard"))  { p->normalized_style = 0.0f;        p->local_tone = 1.0f; p->local_structure = 1.0f; return 0; }
    if (!strcmp(name, "natural"))   { p->normalized_style = 1.0f / 128;  p->local_tone = 1.0f; p->local_structure = 1.0f; return 0; }
    if (!strcmp(name, "cinematic")) { p->normalized_style = 2.0f / 128;  p->local_tone = 1.0f; p->local_structure = 1.0f; return 0; }
    if (!strcmp(name, "neutral"))   { p->normalized_style = 0.0f;        p->local_tone = 0.0f; p->local_structure = 0.0f; return 0; }
    if (!strcmp(name, "vendor"))    { p->normalized_style = 0.0f;        p->local_tone = 1.0f; p->local_structure = 1.5f; return 0; }
    return -1;
}

static void usage(void)
{
    fprintf(stderr,
        "usage: nr_frame IN OUT [--weights W] [--size HxW] [--profile standard|natural|cinematic|neutral|vendor]\n"
        "                (W defaults to the weights compiled into libnr_frame, else work/mlxw/dlssnr-logical.safetensors)\n"
        "                [--style-index N] [--local-tone F] [--local-structure F] [--skin-structure F]\n"
        "                [--auto-mask F] [--control-mask MASK] [--intensity F] [--intensity-ladder A,B,..]\n"
        "                [--detail-strength F] [--colour-strength F] [--detail-radius F] [--frame-index N] [-v]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *input = NULL, *output = NULL, *weights = NULL, *size = NULL, *ladder = NULL, *mask_path = NULL;
    const char *profile_name = "standard";
    int verbose = 0, have_style = 0, style_index = 0, have_tone = 0, have_structure = 0;
    float local_tone = 0, local_structure = 0;
    int have_skin = 0, have_auto = 0;
    float skin = -1.0f, automatic = -1.0f;
    nr_frame_params p;
    nr_frame_defaults(&p);

#define NEXT() (i + 1 < argc ? argv[++i] : (usage(), (char *)0))
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--weights")) weights = NEXT();
        else if (!strcmp(a, "--size")) size = NEXT();
        else if (!strcmp(a, "--profile")) profile_name = NEXT();
        else if (!strcmp(a, "--style-index")) { have_style = 1; style_index = atoi(NEXT()); }
        else if (!strcmp(a, "--local-tone")) { have_tone = 1; local_tone = (float)atof(NEXT()); }
        else if (!strcmp(a, "--local-structure")) { have_structure = 1; local_structure = (float)atof(NEXT()); }
        else if (!strcmp(a, "--skin-structure")) { have_skin = 1; skin = (float)atof(NEXT()); }
        else if (!strcmp(a, "--auto-mask")) { have_auto = 1; automatic = (float)atof(NEXT()); }
        else if (!strcmp(a, "--control-mask")) mask_path = NEXT();
        else if (!strcmp(a, "--intensity")) p.intensity = (float)atof(NEXT());
        else if (!strcmp(a, "--intensity-ladder")) ladder = NEXT();
        else if (!strcmp(a, "--detail-strength")) p.detail_strength = (float)atof(NEXT());
        else if (!strcmp(a, "--colour-strength")) p.colour_strength = (float)atof(NEXT());
        else if (!strcmp(a, "--detail-radius")) p.detail_radius = (float)atof(NEXT());
        else if (!strcmp(a, "--frame-index")) p.frame_index = atoi(NEXT());
        else if (!strcmp(a, "--gpu") || !strcmp(a, "--resident") || !strcmp(a, "--accel"))
            ;   /* one backend here: the resident graph */
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose")) verbose = 1;
        else if (a[0] == '-' && a[1]) usage();
        else if (!input) input = a;
        else if (!output) output = a;
        else usage();
    }
    if (!input || !output) usage();
    if (profile(profile_name, &p)) { fprintf(stderr, "unknown profile %s; one of", profile_name);
        for (size_t k = 0; k < sizeof PROFILES / sizeof *PROFILES; k++) fprintf(stderr, " %s", PROFILES[k]);
        fprintf(stderr, "\n"); return 2; }
    /* `nr_frame.controls`: the profile, then the explicit overrides */
    if (have_style) p.normalized_style = (float)style_index / 128.0f;
    if (have_tone) p.local_tone = local_tone;
    if (have_structure) p.local_structure = local_structure;
    if (have_skin || have_auto) { p.automatic_mask = 1; p.skin_structure = skin; p.automatic_structure = automatic; }

    char default_path[1200];
    if (!weights) weights = default_weights(default_path, sizeof default_path);

    double started = now();
    nr_frame *frame = nr_frame_open(weights);
    if (!frame) { fprintf(stderr, "nr_frame_open: %s\n", nr_frame_error()); return 1; }
    printf("resident backend ready in %.1fs (%s) on %s\n", now() - started, weights ? weights : "embedded weights",
           nr_frame_device(frame));
    fflush(stdout);

    int height, width, resize_h = 0, resize_w = 0;
    if (size && sscanf(size, "%dx%d", &resize_h, &resize_w) != 2) usage();
    float *colour = load_image(input, &height, &width, resize_h, resize_w);
    printf("input %dx%d\n", width, height);
    fflush(stdout);
    float *mask = NULL;
    if (mask_path) {
        int mh, mw;
        mask = load_image(mask_path, &mh, &mw, 0, 0);
        if (mh != height || mw != width) die("the control mask must match the colour image shape");
    }

    int H, W;
    nr_frame_geometry(height, width, &H, &W);
    if (verbose) { printf("  network extent %dx%d for output %dx%d\n", W, H, width, height); fflush(stdout); }
    size_t pixels = (size_t)height * width;
    float *features = malloc((size_t)H * W * 16 * sizeof(float));
    float *wide = malloc((size_t)H * W * 4 * sizeof(float));
    float *head = malloc(pixels * 4 * sizeof(float));
    float *composed = malloc(pixels * 3 * sizeof(float));
    if (!features || !wide || !head || !composed) die("out of memory");

    started = now();
    if (nr_frame_features_masked(frame, colour, height, width, NULL, mask, &p, features)
        || nr_frame_run_features(frame, features, H, W, wide)) {
        fprintf(stderr, "%s\n", nr_frame_error());
        return 1;
    }
    for (int y = 0; y < height; y++)               /* `geometry.crop(head)` */
        memcpy(head + (size_t)y * width * 4, wide + (size_t)y * W * 4, (size_t)width * 4 * sizeof(float));
    double elapsed = now() - started;
    printf("network %.1fs\n", elapsed);
    if (verbose)
        printf("    write %.1f ms, graph %.1f ms, read %.1f ms\n", nr_frame_split(frame, 0) * 1e3,
               nr_frame_split(frame, 1) * 1e3, nr_frame_split(frame, 2) * 1e3);
    double lo = head[0], hi = head[0], sum = 0, sumsq = 0;
    for (size_t i = 0; i < pixels * 4; i++) {
        double v = head[i];
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        sum += v; sumsq += v * v;
    }
    double mean = sum / (double)(pixels * 4);
    printf("head    min %+.4f max %+.4f sd %.4f\n", lo, hi, sqrt(sumsq / (double)(pixels * 4) - mean * mean));
    fflush(stdout);

    /* one render, one file per intensity */
    float ladder_values[64];
    int count = 0;
    if (ladder) {
        char *copy = nr_strdup(ladder), *save = NULL;
        for (char *tok = nr_strtok_r(copy, ",", &save); tok && count < 64; tok = nr_strtok_r(NULL, ",", &save))
            ladder_values[count++] = (float)atof(tok);
        free(copy);
    } else {
        ladder_values[count++] = p.intensity;
    }
    for (int k = 0; k < count; k++) {
        nr_frame_params q = p;
        q.intensity = ladder_values[k];
        if (nr_frame_compose(frame, head, colour, height, width, NULL, NULL, mask, &q, composed)) {
            fprintf(stderr, "nr_frame_compose: %s\n", nr_frame_error());
            return 1;
        }
        double total = 0, worst = 0;
        for (size_t i = 0; i < pixels * 3; i++) {
            double d = fabs((double)composed[i] - (double)colour[i]);
            total += d;
            if (d > worst) worst = d;
        }
        printf("intensity %5.2f  change mean|d| %.5f max|d| %.5f\n", q.intensity, total / (double)(pixels * 3), worst);
        char path[1200];
        if (ladder) {
            /* `stem_i{value:g}{suffix}` */
            const char *dot = strrchr(output, '.');
            const char *slash = strrchr(output, '/');
            if (dot && slash && dot < slash) dot = NULL;
            int stem = dot ? (int)(dot - output) : (int)strlen(output);

#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
            sprintf_s(path, sizeof path, "%.*s_i%g%s", stem, output, (double)q.intensity, dot ? dot : "");
#else
            snprintf(path, sizeof path, "%.*s_i%g%s", stem, output, (double)q.intensity, dot ? dot : "");
#endif
        } else {
#if defined(_WIN32) && __STDC_WANT_SECURE_LIB__
            sprintf_s(path, sizeof path, "%s", output);
#else
            snprintf(path, sizeof path, "%s", output);
#endif
        }
        save_image(composed, height, width, path);
        printf("wrote %s\n", path);
        fflush(stdout);
    }
    nr_frame_close(frame);
    free(colour); free(mask); free(features); free(wide); free(head); free(composed);
    return 0;
}
