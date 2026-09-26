/*
 * xmx.h — the public entry points of libxmx, the resident Vulkan compute runtime.
 *
 * The shared library needs no header: `nr_frame.c` and the Python bind its symbols by
 * name. This one is for the static build (`libdlssnr`, NR_STATIC_XMX), where nr_frame
 * calls these directly, and for checking the definitions in libxmx.c against one
 * declaration. Semantics are documented at the definitions.
 */
#ifndef XMX_H
#define XMX_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* device */
int xmx_open(void);
void xmx_close(void);
/* `cooperative_matrix` is a flags word (a plain 1 still means the matrix extension only):
 * XMX_ADOPT_EXPLICIT_LAYOUT says the host also enabled VK_KHR_workgroup_memory_explicit_layout
 * with all three of its features, which the staged GEMM needs; without it that kernel is not
 * built and its shapes take the 8x16 and 16x32 kernels. */
#define XMX_ADOPT_COOPMAT 1
#define XMX_ADOPT_EXPLICIT_LAYOUT 2
int xmx_adopt(void *instance, void *physical_device, void *device, void *queue, unsigned queue_family,
	      int cooperative_matrix, void *get_instance_proc_addr,
	      void (*lock)(void *), void (*unlock)(void *), void *lock_context);
int xmx_adopted(void);
int xmx_coopmat(void);
int xmx_portable(void);
int xmx_window_gather(void);
int xmx_device_lost(void);
const char *xmx_error(void);
const char *xmx_device(void);
const char *xmx_path(void);
const char *xmx_memory(void);
int xmx_staging_mode(void);
size_t xmx_embedded_shader(const char *name);

/* the plain GEMM path */
int xmx_init(const char *spv_path);
int xmx_reserve(unsigned M, unsigned N, unsigned K, void **pa, void **pb, void **pc);
int xmx_gemm(unsigned M, unsigned N, unsigned K, const void *a, const void *b, void *c, unsigned iters);
int xmx_init_batched(const char *spv_path);
int xmx_reserve_bytes(unsigned long long a, unsigned long long b, unsigned long long c,
		      void **pa, void **pb, void **pc);
int xmx_gemm_batched(unsigned M, unsigned N, unsigned K, unsigned batch,
		     unsigned sa, unsigned sb, unsigned sc, unsigned bt);

/* the resident runtime */
int xmx_res_init(const char *gemm_spv, const char *unary_spv, const char *row_spv,
		 const char *history_spv, const char *tiled_spv, const char *staged_spv);
int xmx_specialize(unsigned mask);
int xmx_staged_partial(unsigned on);  /* staged kernel takes a partial last 64-row block (default 1) */
/* the 32-row staged builds, for a bottleneck of 32 tokens or fewer and the 64-token one's
 * N <= 1024 GEMMs (default on, XMX_STAGED32=0 off); `shallow` a 32-deep K step, `deep` 64.
 * A runtime without a staged kernel accepts both and changes nothing. */
int xmx_staged32_init(const char *shallow, const char *deep);
int xmx_staged32(unsigned on);
unsigned xmx_staged32_calls(void);    /* GEMMs the 32-row build recorded: tests check routing */
/* the row passes' 256-lane build, for the whole-row softmax (attention_rows) */
int xmx_rows_init(const char *path);
unsigned xmx_specialized_count(void);
unsigned xmx_specialization(void);
int xmx_buf_create_kind(unsigned long long bytes, int kind);
int xmx_buf_create(unsigned long long bytes);
int xmx_buf_host_visible(int id);
int xmx_buf_zero(int id);
int xmx_buf_upload(int id, const void *src, unsigned long long offset, unsigned long long bytes);
int xmx_buf_download(int id, void *dst, unsigned long long offset, unsigned long long bytes);
unsigned long long xmx_buf_total_bytes(void);
void *xmx_buf_ptr(int id);
unsigned long long xmx_buf_bytes(int id);
int xmx_buf_destroy(int id);
int xmx_begin(void);
int xmx_abort(void);
int xmx_sync(int on);
int xmx_submit(void);
int xmx_rec_copy(int source, int target, unsigned long long bytes,
		 unsigned long long source_offset, unsigned long long target_offset);
int xmx_rec_gemm(int a, int b, int c, unsigned M, unsigned N, unsigned K, unsigned batch,
		 unsigned sa, unsigned sb, unsigned sc, unsigned bt,
		 unsigned lda, unsigned ldb, unsigned ldc,
		 unsigned oa, unsigned ob, unsigned oc);
int xmx_rec_unary(unsigned kind, int a, int b, int c, int d, unsigned n, unsigned channels,
		  float p0, unsigned batch, unsigned sa, unsigned sb, unsigned sc, unsigned k);
int xmx_rec_row(unsigned kind, int a, int b, int c, int d, unsigned rows, unsigned width,
		unsigned heads, unsigned scaled, unsigned stride, float cap);
int xmx_rec_history(int history, int motion, int out, unsigned pixels, unsigned channels,
		    unsigned height, unsigned width, unsigned absolute);
/* the fused passes (notes/improve-fusions.md, improve-qkv-epilogue.md, improve-joint-qkv.md):
 * the residual in a projection's epilogue, plain or written back through a window layout;
 * the QKV projection finished in its own epilogue, with A read straight or gathered from
 * the image in window order; a GEMM that also stores a half copy; a two-output elementwise
 * pass; Q/K/V prepared in one row dispatch; window attention in one dispatch; a narrow
 * block's feed-forward in one dispatch. Every runtime — libxmx, libmetalmx, libd3dmx —
 * exports all of them; the kernel names take a `_portable` twin on a device without matrix
 * units (`xmx_portable()`), which the caller passes. */
int xmx_rec_gemm_residual(int a, int b, int c, int skip, int cosine,
			  unsigned M, unsigned N, unsigned K, unsigned flags);
int xmx_rec_gemm_window_residual(int a, int b, int c, int skip, int cosine,
				 unsigned M, unsigned N, unsigned K, unsigned flags,
				 unsigned height, unsigned width, unsigned across, unsigned pad);
int xmx_rec_gemm_qkv(int a, int weight, int q, int k, int v, int scale,
		     unsigned M, unsigned channels, unsigned heads, unsigned tokens);
int xmx_rec_gemm_qkv_window(int image, int weight, int q, int k, int v, int scale,
			    unsigned M, unsigned channels, unsigned heads, unsigned tokens,
			    unsigned width, unsigned height, unsigned across, unsigned pad,
			    unsigned image_half);
int xmx_rec_gemm_dual(int a, int b, int c, int half_copy, unsigned M, unsigned N, unsigned K);
int xmx_rec_unary2(unsigned kind, int a, int b, int c, int d, int second, unsigned n,
		   unsigned channels, float p0, unsigned batch, unsigned sa, unsigned sb,
		   unsigned sc, unsigned k);
int xmx_rec_qkv(int source, int q, int k, int v, int scale,
		unsigned rows, unsigned tokens, unsigned heads);
int xmx_window_init(const char *path, unsigned merged);
int xmx_rec_window_attention(int q, int k, int v, int bias, int out,
			     unsigned batches, unsigned heads, unsigned merged);
int xmx_ffn_init(const char *path);
int xmx_rec_ffn(int a, int expand, int projection, int out, int skip, int cosine,
		unsigned M, unsigned cin, unsigned hidden, unsigned groups, unsigned flags);
/* (improve-b.md) block 70's feed-forward making its own merged input, and block 0's making
 * its own stem; block 0's window residual pooling and publishing its output in the
 * epilogue; a 32-channel window block's attention half in one pass a window (the head or
 * the pool too); a bottleneck block's attention in one pass; the integer staged GEMM. */
int xmx_rec_ffn_merge(int source, int skip, int sincos, int expand, int projection, int out,
		      int cosine, unsigned height, unsigned width, unsigned source_width,
		      unsigned flags);
int xmx_rec_ffn_stem(int features, int adapter, int expand, int projection, int out,
		     int cosine, unsigned M, unsigned flags);
int xmx_rec_gemm_window_residual_pool(int a, int b, int skip_out, int skip, int cosine,
				      int pooled, unsigned M, unsigned N, unsigned K,
				      unsigned flags, unsigned height, unsigned width,
				      unsigned across, unsigned pad);
int xmx_window_block_init(const char *path);
int xmx_rec_window_block(int image, int qkv, int projection, int target, int bias,
			 int cosine, int scale, int pooled, unsigned windows, unsigned height,
			 unsigned width, unsigned across, unsigned pad, unsigned flags);
int xmx_global_attention_init(const char *path);
int xmx_rec_global_attention(int q, int k, int v, int merged, unsigned rows,
			     unsigned tokens, unsigned heads, float cap);
int xmx_int8_init(const char *path);
int xmx_rec_gemm_int8(int a, int b, int c, int a_scale, int b_scale,
		      unsigned M, unsigned N, unsigned K, unsigned flags);
int xmx_graph_capture(void);
int xmx_graph_run(int id);
int xmx_graph_destroy(int id);

/* profiling */
int xmx_profile(int on);
void xmx_profile_reset(void);
double xmx_profile_ms(unsigned kind);
unsigned xmx_profile_count(unsigned kind);
unsigned xmx_profile_each_count(void);
double xmx_profile_each_ms(unsigned i);
/* the kind each pass xmx_profile_each_ms timed was stamped with: which family, the GEMM
 * kernel included, actually ran it */
unsigned xmx_profile_each_kind(unsigned i);

#ifdef __cplusplus
}
#endif
#endif
