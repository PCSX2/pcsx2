/*
 * nr_frame — one RGB frame in, one RGB frame out, through the recovered graph, in C.
 *
 * The C library that `src/ref/nr_frame.py` is: the same 16-channel feature assembly,
 * the same 71-block graph recorded against `libxmx` dispatch for dispatch as
 * `nr_frame_resident.py` records it, the same head-to-RGB composition. The head it
 * produces is bit-identical to the Python resident path on the same device — the
 * dispatches are the same, so the bytes are — and `src/ref/test_nr_frame_c.py` checks
 * that. The three places it can differ from NumPy by the last bit are named where they
 * happen: the noise channels, the temporal gate's exponential, and the detail blur.
 *
 * Images are float32 RGB in [0, 1], row-major (height, width, 3); the head is
 * (height, width, 4). One `nr_frame` holds the weights on the device and the graph for
 * the most recent extent; a new extent rebuilds the graph and keeps the weights.
 *
 * The host passes around the graph — feature assembly, the composition and its temporal
 * gate and floor, the detail blur — split their rows across nr_image.c's thread pool, one
 * thread per core (`NR_HOST_THREADS`, else `OMP_NUM_THREADS`, to change it, 1 for none).
 * The bytes do not depend on the count. The temporal gate is a 65536-entry table over the
 * half logit, built once per blend scale, as `nr_frame.gate_table` is in the Python.
 *
 *     nr_frame *f = nr_frame_open("work/mlxw/dlssnr-logical.safetensors");   // or NULL: the weights compiled in
 *     nr_frame_params p; nr_frame_defaults(&p);
 *     nr_frame_update(f, colour, height, width, NULL, NULL, &p, output, NULL);
 *     nr_frame_close(f);
 */
#ifndef NR_FRAME_H
#define NR_FRAME_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nr_frame nr_frame;

typedef struct nr_frame_params {
    /* the composition: post-network, free to sweep */
    float intensity;          /* blend of the model's picture against the source; > 1 extrapolates */
    float detail_strength;    /* high-frequency weight of the change (1 = unchanged) */
    float colour_strength;    /* low-frequency weight of the change (1 = unchanged) */
    float detail_radius;      /* sigma of the split, in pixels */
    /* the conditioning: reaches the network, so a change costs a forward pass */
    float normalized_style;   /* vendor style index / 128 */
    float local_tone;
    float local_structure;
    int   frame_index;        /* seeds the three noise channels */
    /* the temporal path, used when `history` is given */
    float history_confidence; /* scales the model's own gate: 0 the still path, 1 its answer */
    float blend_scale;        /* half(0.73974609375), the recovered package's */
    float hold;               /* floor under the gate where `previous` matches the colour: */
    float slope;              /*   max(alpha, min(clamp(moved * slope + hold, 0, hold), 1) * blend_scale) */
    float release;            /* where `previous` does not match: alpha *= clamp(moved * release + 1, 0, 1),
                               * before the floor; folded like `slope`, -255 / levels (`nr_frame.release_slope`),
                               * 0 off */
    /* the automatic mask: skin structure and automatic-mask structure, each -1 to follow
     * `local_structure`; off unless `automatic_mask` is set */
    int   automatic_mask;
    float skin_structure;
    float automatic_structure;
    /* the network's floor: the frame is padded by mirroring to at least this a side and to a
     * multiple of 64 (`nr_frame.network_geometry`). 320, the default, is the vendor's; the
     * graph runs down to 128, and a small live frame is then mostly picture, not padding.
     * Appended last, so a host built against the older header must be rebuilt. */
    int   min_extent;
} nr_frame_params;

/* The values `nr_frame.py` uses when nothing is asked: the `standard` profile. */
void nr_frame_defaults(nr_frame_params *params);

/* Load the logical weights and open the device. NULL on failure; `nr_frame_error()`.
 * A NULL path means the weights compiled into this library — CMake compiles the bin2c
 * slices in the weights directory (NR_WEIGHTS_DIR, regenerated from the safetensors with
 * NR_BIN2C_WEIGHTS) — and fails with a message in a build without them. */
nr_frame *nr_frame_open(const char *weights_path);

/* The size of the embedded safetensors, or 0 when this build carries none. */
size_t nr_frame_embedded_weights_size(void);
void nr_frame_close(nr_frame *frame);

/* Share a host's Vulkan instance and device instead of letting libxmx create its own.
 * Call before the first `nr_frame_open` (or after `nr_frame_shutdown`); the next open
 * takes the objects. Handles are `void *` (VkInstance, VkPhysicalDevice, VkDevice,
 * VkQueue) so no Vulkan header is needed here. The device needs Vulkan 1.3 with
 * storageBuffer16BitAccess, vulkanMemoryModel (+DeviceScope), shaderFloat16,
 * bufferDeviceAddress and scalarBlockLayout enabled, plus VK_KHR_portability_subset
 * where offered; `queue` must belong to a compute-capable family `queue_family`.
 * `cooperative_matrix` is a flags word: 1 (XMX_ADOPT_COOPMAT) when VK_KHR_cooperative_matrix
 * is enabled, plus 2 (XMX_ADOPT_EXPLICIT_LAYOUT) when VK_KHR_workgroup_memory_explicit_layout
 * is enabled with its scalar-block-layout and 16-bit-access features — the staged GEMM needs
 * it, and without it those shapes run on the smaller kernels and the window partition stays
 * a pass of its own. If
 * the host submits on the same queue it passes `lock`/`unlock` (bracketing every
 * submit here) and takes the same lock around its own submits, presents and idle
 * waits. `get_instance_proc_addr` is the host's vkGetInstanceProcAddr. 0 on success;
 * `nr_frame_error()` otherwise, and libxmx keeps making its own device.
 *
 * Refused, with a message, when the runtime behind this library is Metal (the Apple
 * libdlssnr, or NR_GPU_BACKEND=metal) or Direct3D 12 (a Windows libdlssnr built with
 * NR_DLSSNR_D3D12, or NR_GPU_BACKEND=d3d12): there is nothing Vulkan to adopt, and the
 * runtime opens its own device — or, on Direct3D 12, takes the host's through
 * `nr_frame_adopt_d3d12`. `nr_frame_runtime()` says which one is behind. */
int nr_frame_adopt_vulkan(void *instance, void *physical_device, void *device, void *queue,
                          unsigned queue_family, int cooperative_matrix,
                          void *get_instance_proc_addr, void (*lock)(void *),
                          void (*unlock)(void *), void *lock_context);

/* The Direct3D 12 counterpart, for the libd3dmx runtime: share the host's ID3D12Device and
 * ID3D12CommandQueue (as `void *`, so no D3D header is needed here). Lists are recorded
 * for the queue's type, direct or compute. The device must offer Shader Model 6.2 and
 * native 16-bit shader operations. If the host also submits on `queue` it passes
 * `lock`/`unlock` (bracketing every ExecuteCommandLists here) and takes the same lock
 * around its own submits and presents. Refused when the runtime behind this library is
 * not Direct3D 12. 0 on success; `nr_frame_error()` otherwise. */
int nr_frame_adopt_d3d12(void *device, void *queue, void (*lock)(void *), void (*unlock)(void *),
                         void *lock_context);

/* Release the device libxmx holds — its own, or an adopted one back to its host,
 * untouched. Every `nr_frame` must have been closed first. The next `nr_frame_open`
 * opens the device again (adopting whatever `nr_frame_adopt_vulkan` handed over since).
 * A host must call this before destroying a device it shared. */
void nr_frame_shutdown(void);

/* 1 while an adopted (shared) device is open, 0 for libxmx's own, -1 when none is. */
int nr_frame_shared_device(void);

/* The compute runtime behind this library: "vulkan" (libxmx), "metal" (libmetalmx — the
 * Apple libdlssnr, or a shared build under NR_GPU_BACKEND=metal) or "d3d12" (libd3dmx — a
 * Windows libdlssnr built with NR_DLSSNR_D3D12, or NR_GPU_BACKEND=d3d12). Known without
 * opening a device, so a host can decide which device, if any, to share. */
const char *nr_frame_runtime(void);

/* The last failure, for the calling thread's most recent call. */
const char *nr_frame_error(void);
const char *nr_frame_device(nr_frame *frame);
const char *nr_frame_gemm_path(nr_frame *frame);

/* The network extent for an output extent: at least 320, a multiple of 64. */
void nr_frame_geometry(int height, int width, int *network_height, int *network_width);
/* The same at the floor `minimum` (`nr_frame_params.min_extent`), never below 128. */
void nr_frame_geometry_min(int height, int width, int minimum, int *network_height, int *network_width);

/* colour (h, w, 3) -> output (h, w, 3). `history` is the previous output or NULL;
 * `previous` the previous *input* or NULL, for the floor. `head_out` (h, w, 4) optional.
 * 0 on success. */
int nr_frame_update(nr_frame *frame, const float *colour, int height, int width,
                    const float *history, const float *previous,
                    const nr_frame_params *params, float *output, float *head_out);

/* The same with a per-pixel control mask (h, w, 3): red scales the blend in the
 * composition, green the tone and blue the structure that reach the network. */
int nr_frame_update_masked(nr_frame *frame, const float *colour, int height, int width,
                           const float *history, const float *previous, const float *control_mask,
                           const nr_frame_params *params, float *output, float *head_out);

/* The halves, for a caller that wants to sit between them: features, the network, and
 * the composition of a cropped (h, w, 4) head — free to repeat at another intensity. */
int nr_frame_features_masked(nr_frame *frame, const float *colour, int height, int width,
                             const float *history, const float *control_mask,
                             const nr_frame_params *params, float *features);
int nr_frame_compose(nr_frame *frame, const float *head, const float *colour, int height, int width,
                     const float *history, const float *previous, const float *control_mask,
                     const nr_frame_params *params, float *output);

int nr_frame_features(nr_frame *frame, const float *colour, int height, int width,
                      const float *history, const nr_frame_params *params,
                      float *features /* (network_height, network_width, 16) */);
int nr_frame_run_features(nr_frame *frame, const float *features,
                          int network_height, int network_width,
                          float *head /* (network_height, network_width, 4) */);

/* The network half of `nr_frame_update`, as the daemon runs it under a render scale:
 * the features built as the update builds them (as half in the graph's own input under
 * NR_INPUT_FP16), the graph, and the head cropped to (h, w, 4). No float32 feature array
 * and no composition, so the caller can resample the head and compose it at another
 * extent with `nr_frame_compose`. `history` (h, w, 3) or NULL, `control_mask` likewise. */
int nr_frame_head(nr_frame *frame, const float *colour, int height, int width,
                  const float *history, const float *control_mask,
                  const nr_frame_params *params, float *head);

/* The daemon's end of a frame (`nr_frame.compose_encode`): the head (head_height,
 * head_width, 4) — at a render scale, smaller than the colour — brought up bilinearly to
 * (h, w) as `nr_daemon.resample` does, composed as `nr_frame_compose` composes it into
 * `output` (h, w, 3), and encoded to 8 bits into `encoded` at (top, left) of a frame
 * `frame_width` pixels wide, RGBA or with `bgra` BGRA. `encoded` is a copy of the request:
 * its alpha and everything outside the region stay as they came. Where the composition's
 * detail split is a no-op (detail and colour strength 1) all of it is one pass over the
 * output (nr_compose_encode); otherwise the separate passes, the same bytes either way. */
int nr_frame_compose_encode(nr_frame *frame, const float *head, int head_height, int head_width,
                            const float *colour, int height, int width, const float *history,
                            const float *previous, const float *control_mask,
                            const nr_frame_params *params, float *output, unsigned char *encoded,
                            int frame_width, int top, int left, int bgra);

/* Seconds spent, in the last run, writing the input, running the graph, reading the
 * head: 0, 1, 2. On a discrete card the outer two are the bus. */
double nr_frame_split(const nr_frame *frame, int which);

#ifdef __cplusplus
}
#endif
#endif
