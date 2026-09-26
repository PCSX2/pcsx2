#!/usr/bin/env python3
"""
nr_resident — whole blocks of the graph recorded as one device submit.

`xmxres` gives the primitives; this assembles them into the shapes the recovered
graph actually has. One block is a single command buffer: its feed-forward, its
window attention and both residuals, with nothing crossing back to the host in
between.

The E4M3 publishes are what make the layout work. Because the publish is
elementwise, a branched feed-forward can write each of its heads straight into a
slice of one wide buffer and publish the whole thing in a single dense pass — no
concatenation, no strided elementwise kernel.
"""
from __future__ import annotations

import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "ref"))

import nr_model  # noqa: E402
import xmxres  # noqa: E402


class SplitBlockWeights:
    """A split-family block (23-30, 40-47): four layers, sixteen heads, C=512."""

    def __init__(self, runtime, weights, index):
        self.index, self.heads = index, 16
        self.origin = nr_model.recovered_window_origin(index)
        self.projection = weights[f"block{index}.layer3.projection_weight"]
        self.channels = self.projection.shape[0]
        self.groups = self.channels // 64
        bias = weights[f"block{index}.layer2.attn_bias"]
        if nr_model.uses_fragment_swizzle(index, 16):
            bias = nr_model.recover_attention_bias_layout(bias)
        self.first = runtime.buffer_from(weights[f"block{index}.layer0.first_projection_weight"],
                                         np.float16)
        self.expand = runtime.buffer_from(weights[f"block{index}.layer0.group_expand_weight"],
                                          np.float16)
        self.project = runtime.buffer_from(weights[f"block{index}.layer0.group_project_weight"],
                                           np.float16)
        self.weight3 = runtime.buffer_from(weights[f"block{index}.layer1.weight3"], np.float16)
        self.ffn_cos = runtime.buffer_from(weights[f"block{index}.layer1.ffn_cos_skip"])
        self.qkv = runtime.buffer_from(weights[f"block{index}.layer2.qkv_weight"], np.float16)
        self.scale = runtime.buffer_from(weights[f"block{index}.layer2.attn_scale"])
        self.bias = runtime.buffer_from(bias)
        self.out = runtime.buffer_from(self.projection, np.float16)
        self.attn_cos = runtime.buffer_from(weights[f"block{index}.layer3.attn_cos_skip"])
        self.branched = False
        self.split = True


class BlockWeights:
    """One block's weights, uploaded once and kept on the device."""

    def __init__(self, runtime, weights, index, *, heads):
        prefix = f"block{index}.layer0"
        self.split = False
        self.index, self.heads = index, heads
        self.origin = nr_model.recovered_window_origin(index)
        self.projection = weights[f"{prefix}.projection_weight"]
        self.channels = self.projection.shape[0]

        bias = weights[f"{prefix}.attn_bias"]
        if nr_model.uses_fragment_swizzle(index, heads):
            bias = nr_model.recover_attention_bias_layout(bias)

        take = lambda name, dtype=np.float16: runtime.buffer_from(
            weights[f"{prefix}.{name}"], dtype)
        self.qkv = take("qkv_weight")
        self.out = take("projection_weight")
        self.bias = runtime.buffer_from(bias, np.float32)
        self.scale = runtime.buffer_from(weights[f"{prefix}.attn_scale"], np.float32)
        self.attn_cos = runtime.buffer_from(weights[f"{prefix}.attn_cos_skip"])
        self.ffn_cos = runtime.buffer_from(weights[f"{prefix}.ffn_cos_skip"])

        self.branched = f"{prefix}.ffn_expand_weight" in weights
        if self.branched:
            expansion, branch = nr_model._fused_branched_weights(
                weights[f"{prefix}.ffn_expand_weight"],
                weights[f"{prefix}.ffn_branch_projection_weight"])
            self.groups = expansion.shape[0]
            self.expand = runtime.buffer_from(expansion, np.float16)
            self.branch = runtime.buffer_from(branch, np.float16)
            self.ffn_out = take("ffn_output_projection_weight")
        else:
            self.groups = 0
            self.expand = take("weight1")
            self.branch = take("weight2")
            self.ffn_out = None


class GlobalBlockWeights:
    """A bottleneck block (31-38): every token attends to every other, 32 heads, C=1024."""

    def __init__(self, runtime, weights, index):
        import math
        self.index, self.heads, self.split, self.branched = index, 32, False, False
        self.projection = weights[f"block{index}.layer4.projection_weight"]
        self.channels = self.projection.shape[0]
        self.expand = runtime.buffer_from(weights[f"block{index}.layer0.weight"], np.float16)
        self.ffn_proj = runtime.buffer_from(weights[f"block{index}.layer1.weight"], np.float16)
        self.hidden_width = weights[f"block{index}.layer0.weight"].shape[1]
        self.ffn_cos = runtime.buffer_from(weights[f"block{index}.layer1.ffn_cos_skip"])
        self.qkv = runtime.buffer_from(weights[f"block{index}.layer2.qkv_weight"], np.float16)
        # the global kernels fold sqrt(head_dim) into the per-head scale
        scale = (weights[f"block{index}.layer2.attn_scale"]
                 * np.float32(math.sqrt(self.channels // self.heads)))
        self.scale = runtime.buffer_from(scale)
        self.out = runtime.buffer_from(self.projection, np.float16)
        self.attn_cos = runtime.buffer_from(weights[f"block{index}.layer4.attn_cos_skip"])
        self.logit_cap = nr_model.GLOBAL_ATTENTION_LOGIT_CAP


class GlobalScratch:
    """Working buffers for one bottleneck block over `tokens` tokens."""

    def __init__(self, runtime, weights, tokens, arena=None):
        channels, heads = weights.channels, weights.heads
        # the token count is the bottleneck's pixel count and need not be tile-aligned.
        # Whole 64-row blocks put its deep-K GEMMs (K up to 4096) on the staged kernel,
        # which is faster there: 240 -> 256 tokens at 720p took the frame from 240.6 to
        # 235.4 ms on the GPU, three alternating runs each. Pad rows are zero, are excluded
        # from the softmax, and leave every real row bit-identical. The eighth is a guard,
        # not a measurement: at 384x384 (64 tokens) and 1024x576 (192) the question does
        # not arise, and a large pad would pay for rows nobody needs. Below 64 tokens —
        # the small network frames `min_extent` allows — the pad is taken outright: those
        # GEMMs wait on their K loop, not on rows, so 64 cost what 16 do, and a whole block
        # lets the QKV projection's epilogue leave the tiled kernel (0.22 -> 0.14 ms a call,
        # about 0.6 ms of a 192x128 frame). At 32 tokens or fewer — 16 at 256x128; 320x320
        # has 64 — a 32-row build of the staged kernel does better still: half of each K
        # step's work on the 64-row block was the pad (libxmx.c, `small`).
        padded = xmxres.align(tokens, 16)
        if tokens <= 32:
            padded = 32
        elif tokens <= 64 or xmxres.align(tokens, 64) * 8 <= padded * 9:
            padded = xmxres.align(tokens, 64)
        self.tokens, self.padded = tokens, padded
        padded, hidden = self.padded, weights.hidden_width
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        self.value = make("global.value", padded * channels).zero()
        # The chain's value as half, between the blocks and through them (`chain` in
        # record_global_block). Its pad rows are zero from here on and never written.
        self.io16 = make("global.io16", padded * channels, np.float16).zero()
        self.value16 = make("value16", padded * channels, np.float16)
        self.hidden16 = make("hidden16", padded * hidden, np.float16)
        self.branch = make("branch", padded * channels)
        self.ffn = make("ffn", padded * channels)
        self.ffn16 = make("ffn16", padded * channels, np.float16)
        self.proj = make("proj", padded * channels * 3)
        self.q16, self.k16, self.v16 = (make(name, padded * channels, np.float16) for name in ("q16", "k16", "v16"))
        self.key16 = make("key16", padded * channels, np.float16)   # see record_qkv_projection
        self.scores = make("scores", heads * padded * padded)
        self.probs16 = make("probs16", heads * padded * padded, np.float16)
        self.context = make("context", heads * padded * 32)
        self.merged16 = make("merged16", padded * channels, np.float16)
        # the fused attention's output: merged16 shares q16's role, which it still reads
        self.context16 = make("context16", padded * channels, np.float16)
        self.attention = make("attention", padded * channels)
        self.out = make("global.out", padded * channels)

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def record_qkv(runtime, w, s, windows, tokens, channels, heads):
    """Split V; optionally normalize Q/K directly from the projection buffer."""
    if runtime.fuse_qk and runtime.joint_qkv:
        runtime.prepare_qkv(s.proj, s.q16, s.k16, s.v16, w.scale, windows, tokens, heads)
        return
    if runtime.fuse_qk:
        with runtime.independent():
            runtime.cosine_publish(s.proj, s.q16, windows * heads * tokens,
                                   tokens=tokens, heads=heads, scale=w.scale,
                                   narrow=True, qkv_part=0)
            runtime.cosine_publish(s.proj, s.k16, windows * heads * tokens,
                                   tokens=tokens, heads=heads, narrow=True, qkv_part=1)
            runtime.split_heads(s.proj, s.v16, windows, tokens, channels, heads, 2,
                                epilogue=xmxres.EPI_E4M3, narrow=True)
        return
    with runtime.independent():
        for index, part in enumerate((s.q16, s.k16)):
            runtime.split_heads(s.proj, part, windows, tokens, channels, heads, index,
                                epilogue=xmxres.EPI_HALF, narrow=True)
        runtime.split_heads(s.proj, s.v16, windows, tokens, channels, heads, 2,
                            epilogue=xmxres.EPI_E4M3, narrow=True)
    with runtime.independent():
        runtime.cosine_publish(s.q16, s.q16, windows * heads * tokens,
                               tokens=tokens, heads=heads, scale=w.scale,
                               narrow=True, from_half=True)
        runtime.cosine_publish(s.k16, s.k16, windows * heads * tokens,
                               tokens=tokens, heads=heads, narrow=True, from_half=True)


def record_qkv_projection(runtime, a, w, s, windows, tokens, channels, heads, *,
                          window=None, image_half=False):
    """The QKV projection and everything that prepares Q, K and V after it.

    Returns the buffer K ended up in. With `qkv_epilogue` it is one GEMM whose epilogue
    normalises Q and K and publishes V (`qkv_epilogue.glsl`), and K goes to `s.key16`
    rather than `s.k16`: k16 shares the arena role of the projection's input, which other
    workgroups are still reading while this one's epilogue writes. Otherwise the
    projection goes to memory in float32 and `record_qkv` reads it back, as it always did.

    With `window=(height, width, origin)` `a` is the image, not its partition: the epilogue
    GEMM gathers the window rows itself. Without the epilogue that gather has nowhere to
    live, so the partition is recorded here as it always was.
    """
    rows = windows * tokens
    if window is not None:
        if runtime.qkv_epilogue and rows % 64 == 0:
            runtime.gemm_qkv(a, w.qkv, s.q16, s.key16, s.v16, w.scale, rows, channels, heads,
                             tokens, window=window, image_half=image_half)
            return s.key16
        height, width, origin = window
        runtime.partition(a, s.win16, height, width, channels, origin=origin, narrow=True,
                          a_half=image_half)
        a = s.win16
    if runtime.qkv_epilogue and rows % 16 == 0:
        runtime.gemm_qkv(a, w.qkv, s.q16, s.key16, s.v16, w.scale, rows, channels, heads,
                         tokens)
        return s.key16
    runtime.gemm(a, w.qkv, s.proj, rows, 3 * channels, channels)
    record_qkv(runtime, w, s, windows, tokens, channels, heads)
    return s.k16


def record_project_residual(runtime, a, weight, branch, skip, cosine, target,
                            rows, channels, inner, *, epilogue=0, skip_half=False,
                            narrow=False):
    """A projection and its residual: `target = a @ weight + skip * cosine`.

    Fused, the residual is the GEMM's own epilogue and the float32 branch never goes
    to memory (ProjectsCodex's phase38, `notes/improve-fusions.md`). Unfused it is the
    two passes this graph always had, kept for paired comparison and as the reference
    the fused path is bit-identical to. `NR_FUSE_RESIDUAL=0` selects them.
    """
    if runtime.fuse_residual:
        runtime.gemm_residual(a, weight, skip, cosine, target, rows, channels, inner,
                              epilogue=epilogue, skip_half=skip_half, narrow=narrow)
    else:
        runtime.gemm(a, weight, branch, rows, channels, inner)
        runtime.residual(branch, skip, cosine, target, rows * channels, channels,
                         epilogue=epilogue, b_half=skip_half, narrow=narrow)


def record_global_block(runtime, w, s, source=None, target=None, *, chain=False):
    """A bottleneck block: the wide feed-forward, then attention over every token.

    With `chain` the block reads and writes `s.io16`, the value as half. Between the
    blocks it is published E4M3, so the half is exact, and the float32 copy each block
    went through — widened on the way in, narrowed again for the feed-forward, published
    and narrowed on the way out, four passes — is not needed: the feed-forward reads the
    half, its residual widens it, and the output projection publishes into it. Only the
    real rows are stored, so the pad rows stay the zeros the float32 input's were.
    """
    channels, heads, padded = w.channels, w.heads, s.padded
    if chain:
        source = value16 = s.io16
    else:
        source = source or s.value
        value16 = s.value16
        runtime.to_half(source, value16, padded * channels)
    target = s.io16 if chain else (target or s.out)
    runtime.gemm(value16, w.expand, s.hidden16, padded, w.hidden_width, channels,
                 epilogue=xmxres.EPI_GATE_E4M3, narrow=True)
    record_project_residual(runtime, s.hidden16, w.ffn_proj, s.branch, source, w.ffn_cos,
                            s.ffn, padded, channels, w.hidden_width, skip_half=chain)

    runtime.to_half(s.ffn, s.ffn16, padded * channels)
    key = record_qkv_projection(runtime, s.ffn16, w, s, 1, padded, channels, heads)
    if runtime.fuse_global_attention:
        # QK^T, the softmax, PV and the head merge in one pass, and no score stored
        merged = s.context16
        runtime.global_attention(s.q16, key, s.v16, merged, padded, s.tokens, heads,
                                 w.logit_cap)
    else:
        merged = s.merged16
        runtime.gemm(s.q16, key, s.scores, padded, padded, 32, batch=heads,
                     strides=(padded * 32, padded * 32, padded * padded), transpose_b=True)
        # no attention bias here, and the logits are clamped symmetrically
        runtime.softmax(s.scores, s.probs16, heads * padded, s.tokens,
                        stride=padded, cap=w.logit_cap, narrow=True)
        runtime.gemm(s.probs16, s.v16, s.context, padded, 32, padded, batch=heads,
                     strides=(padded * padded, padded * 32, padded * 32))
        runtime.merge_heads(s.context, merged, 1, padded, channels, heads,
                            epilogue=xmxres.EPI_E4M3, narrow=True)
    if chain:
        record_project_residual(runtime, merged, w.out, s.attention, s.ffn, w.attn_cos,
                                target, s.tokens, channels, channels,
                                epilogue=xmxres.EPI_E4M3, narrow=True)
    else:
        record_project_residual(runtime, merged, w.out, s.attention, s.ffn, w.attn_cos,
                                target, padded, channels, channels)


def run_global_block(runtime, w, s, value):
    """Host convenience: `value` is (tokens, channels)."""
    tokens, channels = value.shape
    xmxres.host_write(s.value, value, rows=(s.padded, channels))
    runtime.begin()
    record_global_block(runtime, w, s)
    passes = runtime.submit()
    return xmxres.host_view(s.out, shape=(s.padded, channels))[:tokens].copy(), passes


class BlockScratch:
    """Working buffers for one block at one extent, allocated once and reused."""

    def __init__(self, runtime, weights, height, width, arena=None):
        channels, heads = weights.channels, weights.heads
        # Sized for the largest window count any origin can produce, so blocks at the
        # same level share one scratch even though their shifts differ. Getting this
        # wrong is silent: a shifted block has more windows than an unshifted one and
        # would write past the end of buffers cut to the unshifted size.
        padded_height, padded_width, _ = runtime.window_extent(height, width, (-4, -4))
        self.height, self.width = height, width
        self.tokens = 64
        self.windows = (padded_height // 8) * (padded_width // 8)
        self.batch = self.windows * heads
        pixels = height * width
        windowed = self.windows * self.tokens * channels
        hidden = weights.groups * 128 if weights.branched else weights.expand.nbytes // 2 // channels

        if getattr(weights, "split", False):
            hidden = weights.groups * 256
        # Every buffer a block wrote in float32 only to read it straight back went
        # away when the publishes moved into the pass that produces the value.
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        self.value = make("value", pixels * channels)
        self.value16 = make("value16", pixels * channels, np.float16)
        self.hidden16 = make("hidden16", pixels * hidden, np.float16)
        self.heads16 = (make("heads16", pixels * channels, np.float16)
                        if weights.branched or getattr(weights, "split", False) else None)
        self.branch = make("branch", pixels * channels)
        self.ffn = make("ffn", pixels * channels)
        self.win16 = make("win16", windowed, np.float16)
        self.proj = make("proj", windowed * 3)
        self.q16, self.k16, self.v16 = (make(name, windowed, np.float16) for name in ("q16", "k16", "v16"))
        self.key16 = make("key16", windowed, np.float16)   # see record_qkv_projection
        self.scores = make("scores", self.batch * self.tokens * self.tokens)
        self.probs16 = make("probs16", self.batch * self.tokens * self.tokens, np.float16)
        self.context = make("context", self.batch * self.tokens * 32)
        self.merged16 = make("merged16", windowed, np.float16)
        # the fused attention's merged output: scores' and context's role, unused by then
        self.context16 = make("context16", windowed, np.float16)
        self.attended = make("attended", windowed)
        self.out = make("out", pixels * channels)
        self.core16 = (make("core16", pixels * channels, np.float16)
                       if getattr(weights, "split", False) else None)
        self.hidden_width = hidden

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def _ffn_groups(runtime, a, b, c, rows, cols, inner, groups, *, leading, strides,
                epilogue):
    """Independent group products, with the same tiles and rounding in either mode.

    Batch strides advance the group, row strides advance the pixel within that group.
    In particular, a zero A batch stride broadcasts the branched FFN input; output
    groups occupy disjoint column slices, not consecutive dense matrices.
    """
    if runtime.batch_ffn:
        runtime.gemm(a, b, c, rows, cols, inner, batch=groups, strides=strides,
                     leading=leading, epilogue=epilogue, narrow=True)
    else:
        with runtime.independent():
            for group in range(groups):
                runtime.gemm(a, b, c, rows, cols, inner, leading=leading,
                             offsets=tuple(group * step for step in strides),
                             epilogue=epilogue, narrow=True)


def _can_make_input(runtime, w, s):
    """Whether this block's feed-forward is the fused kernel's staged shape."""
    return (runtime.fuse_ffn and not w.branched and not getattr(w, "split", False)
            and w.channels == 32 and s.hidden_width == 128 and (s.height * s.width) % 16 == 0)


def can_merge_input(runtime, w, s):
    """Whether this block's feed-forward can make block 70's merged input itself."""
    return (runtime.fuse_merge_ffn and _can_make_input(runtime, w, s)
            and s.height % 2 == 0 and s.width % 2 == 0)


def can_make_stem(runtime, w, s):
    """Whether this block's feed-forward can make block 0's stem itself."""
    return runtime.fuse_stem_ffn and _can_make_input(runtime, w, s)


def record_feed_forward(runtime, w, s, source, source_half=False, source16=None,
                        merge=None, stem=None):
    """The block's feed-forward, into `s.ffn`. Branched or plain, as the block is.

    `source_half` says the block's input is already float16 — true whenever it is an
    E4M3 publish, which is exact in half — so the widening pass in front of the first
    GEMM is not needed and the residual reads the narrow buffer directly. `source16`
    says a float32 input's half copy has already been written there, by the pass that
    produced the input, so the to_half pass is not needed either.

    `merge` is block 70's input still unmade — the level above, the skip, the sin-cos
    table and the level above's width — for the feed-forward to make itself
    (`can_merge_input`); `stem` is block 0's, the features and the adapter
    (`can_make_stem`). `source` is then unused.

    Returns whether `s.ffn` was stored as half: the branched blocks publish it as E4M3,
    which half holds exactly, so they store it narrow and every pass after reads half the
    bytes; the plain blocks' output is unpublished float32 and stays that.
    """
    pixels, channels = s.height * s.width, w.channels
    if merge is not None:
        if not can_merge_input(runtime, w, s):
            raise ValueError("this block's feed-forward cannot make its own input")
        above, skip, sincos, above_width = merge
        runtime.ffn_fused_merge(above, skip, sincos, w.expand, w.branch, s.ffn, w.ffn_cos,
                                s.height, s.width, above_width)
        return False
    if stem is not None:
        if not can_make_stem(runtime, w, s):
            raise ValueError("this block's feed-forward cannot make its own stem")
        features, adapter = stem
        runtime.ffn_fused_stem(features, adapter, w.expand, w.branch, s.ffn, w.ffn_cos, pixels)
        return False
    value16 = source if source_half else (source16 or s.value16)
    if not source_half and source16 is None:
        runtime.to_half(source, s.value16, pixels * channels)
    if w.branched:
        if (runtime.fuse_branched_ffn and pixels % 16 == 0 and channels % 16 == 0
                and channels == w.groups * 32):
            # every group's expand and projection in one pass, the hidden layer on chip
            runtime.ffn_fused(value16, w.expand, w.branch, s.heads16, pixels, channels, 128,
                              groups=w.groups, epilogue=xmxres.EPI_E4M3, narrow=True)
        else:
            _ffn_groups(runtime, value16, w.expand, s.hidden16, pixels, 128, channels,
                        w.groups, leading=(0, 0, s.hidden_width),
                        strides=(0, channels * 128, 128), epilogue=xmxres.EPI_GATE_E4M3)
            _ffn_groups(runtime, s.hidden16, w.branch, s.heads16, pixels, 32, 128,
                        w.groups, leading=(s.hidden_width, 0, channels),
                        strides=(128, 128 * 32, 32), epilogue=xmxres.EPI_E4M3)
        # the fused multi-head kernels publish the residual before attention reads it,
        # which the residual now does on its way out — as half, which holds it exactly
        record_project_residual(runtime, s.heads16, w.ffn_out, s.branch, source, w.ffn_cos,
                                s.ffn, pixels, channels, channels,
                                epilogue=xmxres.EPI_E4M3, skip_half=source_half,
                                narrow=True)
        return True
    elif (runtime.fuse_ffn and channels == 32 and s.hidden_width % 32 == 0
          and pixels % 16 == 0):
        # both GEMMs in one pass, the hidden layer never written (ffn_fused.comp)
        runtime.ffn_fused(value16, w.expand, w.branch, s.ffn, pixels, channels,
                          s.hidden_width, skip=source, cosine=w.ffn_cos,
                          skip_half=source_half)
        return False
    else:
        runtime.gemm(value16, w.expand, s.hidden16, pixels, s.hidden_width, channels,
                     epilogue=xmxres.EPI_GATE_E4M3, narrow=True)
        record_project_residual(runtime, s.hidden16, w.branch, s.branch, source, w.ffn_cos,
                                s.ffn, pixels, channels, s.hidden_width,
                                skip_half=source_half)
        return False


def record_split_feed_forward(runtime, w, s, source, source_half=False):
    """The split family's core: e4m3(x @ first), then a per-64-group 64 -> 256 -> 64 MLP.

    The gate sits between the two group GEMMs with no publish, so the wide buffer is
    gated in one dense pass; the group outputs are published once, together.
    """
    pixels, channels, groups = s.height * s.width, w.channels, w.groups
    wide = groups * 256
    value16 = source if source_half else s.value16
    if not source_half:
        runtime.to_half(source, s.value16, pixels * channels)
    runtime.gemm(value16, w.first, s.heads16, pixels, channels, channels,
                 epilogue=xmxres.EPI_E4M3, narrow=True)
    _ffn_groups(runtime, s.heads16, w.expand, s.hidden16, pixels, 256, 64,
                groups, leading=(channels, 0, wide),
                strides=(64, 64 * 256, 256), epilogue=xmxres.EPI_GATE)
    _ffn_groups(runtime, s.hidden16, w.project, s.core16, pixels, 64, 256,
                groups, leading=(wide, 0, channels),
                strides=(256, 256 * 64, 64), epilogue=xmxres.EPI_E4M3)
    record_project_residual(runtime, s.core16, w.weight3, s.branch, source, w.ffn_cos,
                            s.ffn, pixels, channels, channels, skip_half=source_half)


def can_fuse_window_block(runtime, w, s):
    """Whether this block's attention half runs as one pass a window (window_block.comp)."""
    return (w.channels == 32 and w.heads == 1 and s.tokens == 64 and runtime.fuse_window_block
            and runtime.qkv_epilogue and runtime.fuse_partition
            and runtime.fuse_window_attention and runtime.fuse_attention_merge
            and runtime.fuse_window_residual)


def record_window_attention(runtime, w, s, source, target=None, publish=0,
                            target_half=False, source_half=False, pool=None, head=None):
    """Window attention over `source`, into `s.attended` — in window order.

    With a `target`, the output projection finishes the block instead: it adds
    `source * attn_cos` and writes straight back into the unpadded image, so neither
    `s.attended` nor the residual's own window-reversing pass is needed
    (ProjectsCodex's phase39). `source` is then the residual's skip, as it always was.

    The reverse back to image order is the following residual's own gather, so nothing
    is written in image order here. The window count follows this block's own origin,
    not the scratch's worst case.
    """
    channels, heads, tokens = w.channels, w.heads, s.tokens
    if ((target is not None or pool is not None or head is not None)
            and can_fuse_window_block(runtime, w, s)):
        # the three fused passes below in one, a window a workgroup (window_block.comp);
        # only on top of them, so that turning one of them off still compares its own path
        if head is not None:
            weights, head_target, columns = head
            runtime.window_block(source, w.qkv, w.out, head_target, w.bias, w.attn_cos,
                                 w.scale, s.height, s.width, w.origin,
                                 image_half=source_half, head=weights, head_columns=columns)
        elif pool is not None:
            pooled, published = pool
            runtime.window_block(source, w.qkv, w.out, published, w.bias, w.attn_cos, w.scale,
                                 s.height, s.width, w.origin, narrow=True,
                                 image_half=source_half, pooled=pooled)
        else:
            runtime.window_block(source, w.qkv, w.out, target, w.bias, w.attn_cos, w.scale,
                                 s.height, s.width, w.origin, epilogue=publish,
                                 narrow=target_half, image_half=source_half)
        return
    padded_height, padded_width, _ = runtime.window_extent(s.height, s.width, w.origin)
    windows = (padded_height // 8) * (padded_width // 8)
    batch = windows * heads
    windowed = windows * tokens * channels
    if runtime.fuse_partition:
        # the projection gathers its window rows from the image itself
        key = record_qkv_projection(runtime, source, w, s, windows, tokens, channels, heads,
                                    window=(s.height, s.width, w.origin),
                                    image_half=source_half)
    else:
        runtime.partition(source, s.win16, s.height, s.width, channels, origin=w.origin,
                          narrow=True, a_half=source_half)
        key = record_qkv_projection(runtime, s.win16, w, s, windows, tokens, channels, heads)
    fused = runtime.fuse_window_attention and tokens == 64
    merged = fused and runtime.fuse_attention_merge
    attended = s.context16 if merged else s.merged16
    if fused:
        # One dispatch for QK^T, softmax and PV (ProjectsCodex's phase42). Never into
        # `merged16`: it shares q16's arena role, and another workgroup may still be
        # reading Q while this one stores. Merged, the same dispatch also does the head
        # merge and writes the published result into `context16`, whose role (the
        # scores') the fused path leaves unused.
        runtime.window_attention(s.q16, key, s.v16, attended if merged else s.context,
                                 batch, heads, bias=w.bias, merged=merged)
    else:
        runtime.gemm(s.q16, key, s.scores, tokens, tokens, 32, batch=batch,
                     strides=(tokens * 32, tokens * 32, tokens * tokens), transpose_b=True)
        runtime.softmax(s.scores, s.probs16, batch * tokens, tokens, narrow=True,
                        bias=w.bias, heads=heads)
        runtime.gemm(s.probs16, s.v16, s.context, tokens, 32, tokens, batch=batch,
                     strides=(tokens * tokens, tokens * 32, tokens * 32))
    if not merged:
        runtime.merge_heads(s.context, s.merged16, windows, tokens, channels, heads,
                            epilogue=xmxres.EPI_E4M3, narrow=True)
    if pool is not None:
        # block 0: the output only ever read pooled or published, both made here
        pooled, published = pool
        runtime.gemm_residual_pool(attended, w.out, source, w.attn_cos, published, pooled,
                                   windows * tokens, channels,
                                   (s.height, s.width, 8, w.origin), skip_half=source_half)
    elif target is not None:
        runtime.gemm_residual(attended, w.out, source, w.attn_cos, target,
                              windows * tokens, channels, channels,
                              reverse=(s.height, s.width, 8, w.origin),
                              epilogue=publish, narrow=target_half, skip_half=source_half)
    else:
        runtime.gemm(attended, w.out, s.attended, windows * tokens, channels, channels)


def can_pool_output(runtime, w, s):
    """Whether this block's window residual can pool and publish its own output."""
    _, _, (top, left) = runtime.window_extent(s.height, s.width, w.origin)
    return (runtime.fuse_pool and runtime.fuse_window_residual and w.channels == 32
            and not getattr(w, "split", False) and s.tokens == 64
            and s.height % 2 == 0 and s.width % 2 == 0 and top % 2 == 0 and left % 2 == 0)


def record_block(runtime, w, s, source=None, target=None, publish=0, source_half=False,
                 target_half=False, source16=None, merge=None, stem=None, pool=None,
                 head=None):
    """A whole window block: feed-forward, attention, both residuals.

    `source` and `target` default to the scratch's own buffers; passing them lets one
    level's blocks chain into the next without a copy. `publish` is the epilogue the
    closing residual applies, which is how a block's output is published without a
    second pass over it. `pool=(pooled, published)` is block 0's: its output is written
    only pooled and published, both by the closing residual (`can_pool_output`), and
    `target` is unused.
    """
    source = source or s.value
    target = target or s.out
    pixels = s.height * s.width
    if getattr(w, "split", False):
        if source16 is not None:
            raise ValueError("the split feed-forward takes no prepared half copy")
        record_split_feed_forward(runtime, w, s, source, source_half)
        ffn_half = False
    else:
        ffn_half = record_feed_forward(runtime, w, s, source, source_half, source16=source16,
                                       merge=merge, stem=stem)
    if pool is not None and not can_pool_output(runtime, w, s):
        raise ValueError("this block's window residual cannot pool its own output")
    if head is not None and not can_fuse_window_block(runtime, w, s):
        raise ValueError("only a fused window block computes the head itself")
    if runtime.fuse_window_residual:
        record_window_attention(runtime, w, s, s.ffn, target=target, publish=publish,
                                target_half=target_half, source_half=ffn_half, pool=pool,
                                head=head)
        return
    record_window_attention(runtime, w, s, s.ffn, source_half=ffn_half)
    # the window reverse is the residual's own gather, not a pass of its own
    runtime.residual(s.attended, s.ffn, w.attn_cos, target, pixels * w.channels,
                     w.channels, reverse=(s.height, s.width, 8, w.origin),
                     epilogue=publish, narrow=target_half, b_half=ffn_half)


def run_block(runtime, w, s, value):
    """Host convenience: one block, one submit, numpy in and numpy out."""
    xmxres.host_write(s.value, value)
    runtime.begin()
    record_block(runtime, w, s)
    passes = runtime.submit()
    return xmxres.host_view(s.out, shape=value.shape).copy(), passes


# --------------------------------------------------------------------------
# transitions between levels
# --------------------------------------------------------------------------


class Transition:
    """The weights a level change needs, uploaded once."""

    def __init__(self, runtime, weights, index, *, kind):
        self.kind = kind
        prefix = f"block{index}.layer0"
        if kind in ("down", "up"):
            self.weight0 = runtime.buffer_from(weights[f"{prefix}.weight0"], np.float16)
            self.out_channels = weights[f"{prefix}.weight0"].shape[1]
        if kind == "up":
            self.sine = runtime.buffer_from(weights[f"{prefix}.sin"])


def record_downsample(runtime, transition, scratch, source, target, height, width,
                      channels, *, pad_to=0, source_half=False, target_half=False):
    """Pool the block's unpublished output, publish it, then project.

    The fused `ds` kernels pool the half-precision output before its E4M3 publish and
    publish the pooled tensor again before the QMMA projection, so both are here.
    """
    if pad_to:
        padded_height = -(-height // pad_to) * pad_to
        padded_width = -(-width // pad_to) * pad_to
        runtime.pad_end(source, scratch.padded, height, width, padded_height,
                        padded_width, channels, a_half=source_half, narrow=source_half)
        source, height, width = scratch.padded, padded_height, padded_width
    half_height, half_width = height // 2, width // 2
    pixels = half_height * half_width
    runtime.pool2(source, scratch.pooled16, height, width, channels,
                  epilogue=xmxres.EPI_E4M3, narrow=True, a_half=source_half)
    runtime.gemm(scratch.pooled16, transition.weight0, target, pixels,
                 transition.out_channels, channels, epilogue=xmxres.EPI_E4M3,
                 narrow=target_half)
    return half_height, half_width


def record_upsample_merge(runtime, transition, scratch, source, skip, target,
                          source_height, source_width, height, width, channels,
                          out_channels, *, source_half=False, skip_half=False,
                          target_half=False):
    """Project, nearest-upsample onto the skip, add the scaled skip, publish.

    The fused `upsample` kernels read the merged tensor as E4M3, so the publish is
    part of the transition rather than of the block that follows.
    """
    source_pixels = source_height * source_width
    projected16 = source if source_half else scratch.projected16
    if not source_half:
        runtime.to_half(source, scratch.projected16, source_pixels * channels)
    runtime.gemm(projected16, transition.weight0, scratch.projected,
                 source_pixels, out_channels, channels)
    if runtime.fuse_transition:
        # the upsample, the scaled skip and the add in one pass (resident.comp UPSAMPLE_ADD)
        runtime.upsample_add(scratch.projected, skip, transition.sine, target, height, width,
                             source_width, out_channels, skip_half=skip_half,
                             epilogue=xmxres.EPI_E4M3, narrow=target_half)
        return
    with runtime.independent():
        runtime.upsample2(scratch.projected, scratch.upsampled, source_width, height,
                          width, out_channels)
        runtime.scale_channel(skip, transition.sine, scratch.scaled,
                              height * width * out_channels, out_channels,
                              a_half=skip_half)
    runtime.add(scratch.upsampled, scratch.scaled, target, height * width * out_channels,
                epilogue=xmxres.EPI_E4M3, narrow=target_half)


class TransitionScratch:
    """Buffers a transition needs, sized for the largest level that uses it."""

    def __init__(self, runtime, elements, half_elements, arena=None):
        make = (arena.buffer if arena is not None else
                lambda name, count, dtype=np.float32: runtime.buffer(count, dtype))
        # `padded` holds whichever width its source has, so it is sized for float32
        self.padded = make("transition.padded", elements)
        self.pooled16 = make("transition.pooled16", elements, np.float16)
        self.projected = make("transition.projected", elements)
        self.projected16 = make("transition.projected16", elements, np.float16)
        self.upsampled = make("transition.upsampled", elements)
        self.scaled = make("transition.scaled", elements)

    def free(self):
        for name in dir(self):
            value = getattr(self, name)
            if isinstance(value, xmxres.Buffer):
                value.free()


def record_plain_downsample(runtime, edge, scratch, source, target, height, width,
                            channels, *, pad_to=0, source_half=False, target_half=False):
    """`downsample()`: pool then project, with no publish between.

    Block 30's bridge into the bottleneck, which unlike the encoder's `ds` kernels
    does not republish the pooled tensor before the projection.
    """
    if pad_to:
        padded_height = -(-height // pad_to) * pad_to
        padded_width = -(-width // pad_to) * pad_to
        runtime.pad_end(source, scratch.padded, height, width, padded_height,
                        padded_width, channels, a_half=source_half, narrow=source_half)
        source, height, width = scratch.padded, padded_height, padded_width
    pixels = (height // 2) * (width // 2)
    runtime.pool2(source, scratch.pooled16, height, width, channels,
                  epilogue=xmxres.EPI_HALF, narrow=True, a_half=source_half)
    runtime.gemm(scratch.pooled16, edge.weight0, target, pixels, edge.out_channels,
                 channels, epilogue=xmxres.EPI_E4M3, narrow=target_half)
