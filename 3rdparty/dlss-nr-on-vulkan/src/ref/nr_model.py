#!/usr/bin/env python3
"""
nr_model — the recovered DLSS-NR 71-block graph, in numpy.

A faithful port of `iamwavecut/MLX-DLSS` `python/mlxdlss/model.py` (PyTorch) to
numpy, because this machine has no torch and because the GEMM has to be
swappable for the Intel Xe2 XMX path.  Every rounding point of the original is
kept: the E4M3 publishes, the half-precision fragment-tree cosine normalise, and
the bit-affine softmax approximation.

Weights are the *logical* tensors (`work/mlxw/dlssnr-logical.safetensors`), not
the packed container.  See notes/phase6-mlx-dlss-unpack.md.

The single GEMM entry point is `matmul()`; point `nr_model.MATMUL` at another
callable to run the whole graph on different hardware.
"""
from __future__ import annotations

import json
import math
import os
import struct

import numpy as np

COSINE_NORM_FLOOR = 0.00006198883056640625
GLOBAL_ATTENTION_LOGIT_CAP = 3.0

# Token-local work and window attention run in chunks of at most this many
# tokens so temporaries scale with the chunk, not the frame.
CHUNK_TOKENS = int(os.environ.get("NR_CHUNK_TOKENS", str(1 << 13)))

# --------------------------------------------------------------------------
# the one GEMM
# --------------------------------------------------------------------------

# The branched feed-forward is written as 4*G^2 small GEMMs plus 4*G more; the
# same arithmetic is 1 + G + 1 larger ones (see `branched_feed_forward`). The
# fused form reassociates float32 sums, which is a ~1e-07 difference, so the
# reference keeps the literal form and a backend opts in.
FUSE_BRANCHED = os.environ.get("NR_FUSE_BRANCHED", "0") not in ("0", "", "no")

MATMUL = None      # a callable(a, b) -> a @ b, to redirect every GEMM
MATMUL_NT = None   # a callable(a, b) -> a @ b^T, for the attention scores


def matmul(a, b):
    if MATMUL is not None:
        return MATMUL(a, b)
    return a @ b


def matmul_nt(a, b):
    """a @ b^T over the last two axes.

    Kept separate so a backend can consume `b` in the layout it already has: the
    key matrix is stored (tokens, head_dim) and a cooperative-matrix load can read
    it column-major, which is the transpose, for free.
    """
    if MATMUL_NT is not None:
        return MATMUL_NT(a, b)
    return a @ b.swapaxes(-1, -2)


# --------------------------------------------------------------------------
# precision primitives
# --------------------------------------------------------------------------


def h(x):
    """Round to half."""
    with np.errstate(over="ignore"):
        return np.asarray(x, dtype=np.float32).astype(np.float16)


def f(x):
    """Widen to float32 without copying what is already float32.

    `astype` copies unconditionally; these helpers never write to their inputs,
    so a view is safe and the fragment-tree cosine normalise makes ~120 of these
    calls per invocation.
    """
    return np.asarray(x, dtype=np.float32)


# The vendor computes these in half and lets them overflow to inf; numpy warns
# where torch does not, and the saturating result is the behaviour we want.
def _half_errstate():
    return np.errstate(over="ignore", invalid="ignore")


def half_multiply(left, right):
    with _half_errstate():
        return (f(left) * f(right)).astype(np.float16)


def half_add(left, right):
    with _half_errstate():
        return (f(left) + f(right)).astype(np.float16)


def half_fma(left, right, accumulator):
    with _half_errstate():
        return (f(left) * f(right) + f(accumulator)).astype(np.float16)


_GATE_SLOPE = np.float32(-0.055908203125)
_GATE_INTERCEPT = np.float32(0.447265625)
_GATE_BIAS = np.float32(0.89453125)


# Optional accelerators. Both default to the numpy implementations below; a backend
# may replace them with something faster, and `src/ref/nr_accel.py` does exactly that
# with torch, which has SIMD conversions where numpy has none — measured 2.29 against
# 0.17 Gelem/s for the half round trip. Anything installed here must be bit-identical:
# `nr_accel.verify()` checks that over every representable value.
HALF_ROUND = None
E4M3_ROUND = None


def _half_rounded(value):
    """`value` rounded to half precision but carried in float32.

    numpy has no SIMD path for float16 *arithmetic* — it falls back to scalar — nor,
    measured, for the conversions: `astype(float16)` runs at 0.17 Gelem/s against 2.94
    for a float32 copy. It is still the quickest thing numpy alone can do; rounding on
    the float32 exponent and mantissa directly is exact but needs a second branch for
    the float16 subnormal range, which costs 14 passes against two and loses 2.2x.
    """
    if HALF_ROUND is not None:
        return HALF_ROUND(value)
    with np.errstate(over="ignore"):
        return np.asarray(value, dtype=np.float32).astype(np.float16).astype(np.float32)


def _gate_wide(wide):
    """gate(x) for an `x` already rounded to half and held in float32."""
    clamped = np.clip(wide, np.float32(-4), np.float32(4))
    linear = np.abs(clamped)
    linear *= _GATE_SLOPE
    linear += _GATE_INTERCEPT
    linear = _half_rounded(linear)
    linear *= clamped
    linear += _GATE_BIAS
    return _half_rounded(linear)


def quadratic_gate(value):
    return _gate_wide(_half_rounded(value))


def quadratic_gate_activation(value):
    """The vendor activation: x * gate(x), the product taken in half."""
    wide = _half_rounded(value)
    gate = _gate_wide(wide)
    gate *= wide
    return _half_rounded(gate)


_E4M3_NORMAL_FLOOR = np.float32(2.0 ** -6)
_E4M3_SUBNORMAL_STEP = np.float32(2.0 ** -9)


def _e4m3_chunk(value):
    single = value.astype(np.float32)
    magnitude = np.minimum(np.abs(single), np.float32(448.0))
    normal_magnitude = np.maximum(magnitude, _E4M3_NORMAL_FLOOR)
    exponent_bits = (normal_magnitude.view(np.int32) >> 23) & 0xFF
    normal_step = ((exponent_bits - 3) << 23).view(np.float32)
    step = np.where(magnitude < _E4M3_NORMAL_FLOOR, _E4M3_SUBNORMAL_STEP, normal_step)
    rounded = np.rint(magnitude / step) * step          # np.rint is round-half-even
    return np.where(single < 0, -rounded, rounded).astype(np.float32)


def e4m3(value):
    """Round to the nearest E4M3 value (half-even, saturating at 448)."""
    if E4M3_ROUND is not None:
        return E4M3_ROUND(value)
    value = np.asarray(value, dtype=np.float32)
    limit = 8 * CHUNK_TOKENS
    if CHUNK_TOKENS <= 0 or value.size <= limit:
        return _e4m3_chunk(value)
    flat = value.reshape(-1)
    out = np.empty_like(flat)
    for start in range(0, flat.shape[0], limit):
        out[start:start + limit] = _e4m3_chunk(flat[start:start + limit])
    return out.reshape(value.shape)


def vendor_approximate_softmax(value):
    """The fused kernels' half bit-affine softmax approximation."""
    if value.ndim == 0 or value.shape[-1] <= 0:
        raise ValueError("vendor softmax expects a non-empty row")
    if value.shape[-1] % 2:
        raise ValueError("vendor softmax expects an even token count")
    affine = _half_rounded(value)
    affine *= np.float32(0.044921875)
    affine += np.float32(1.30078125)
    # Both bounds are exactly representable in half, so clamping before the
    # rounding pass is the same as clamping after it, and much faster.
    np.clip(affine, np.float32(1.03125), np.float32(1.5693359375), out=affine)
    affine = affine.astype(np.float16)
    bits = affine.view(np.uint16).astype(np.uint32)
    pairs = bits.reshape(*bits.shape[:-1], bits.shape[-1] // 2, 2)
    packed = pairs[..., 0] | (pairs[..., 1] << np.uint32(16))
    transformed = (packed << np.uint32(5)) + np.uint32(0x7FF88000)
    weight_bits = np.stack(
        (transformed & np.uint32(0xFFFF), (transformed >> np.uint32(16)) & np.uint32(0xFFFF)),
        axis=-1,
    ).reshape(bits.shape)
    weights = weight_bits.astype(np.uint16).view(np.float16)
    # `sum(dtype=float16)` does not accumulate in half: numpy (and torch) run the
    # reduction sequentially in float32 and round once at the end, which is why this
    # is reproducible elsewhere at all. The division is then a float32 multiply with
    # one rounding pass, bit-identical to the half multiply and far quicker.
    totals = weights.sum(axis=-1, keepdims=True, dtype=np.float16)
    reciprocal = (np.float32(1.0) / totals.astype(np.float32)).astype(np.float16)
    return e4m3(_half_rounded(weights.astype(np.float32) * reciprocal.astype(np.float32)))


def vendor_cosine_normalize(value):
    """The fused kernels' half fragment-tree cosine normalization."""
    half = _half_rounded(value)
    if half.shape[-1] != 32:
        # Not reachable from this graph — head_dim is 32 at every width — but it is
        # part of the operator, and the vendor squares, sums and takes the reciprocal
        # square root all in half.
        with np.errstate(over="ignore"):
            narrow = half.astype(np.float16)
            squared = np.square(narrow).sum(axis=-1, keepdims=True, dtype=np.float16)
            squared = np.maximum(squared, np.float16(COSINE_NORM_FLOOR))
            # One rounding, as `rsqrt` is: a half sqrt followed by a half divide
            # rounds twice and drifts by 1e-03.
            reciprocal = (np.float32(1.0) / np.sqrt(squared.astype(np.float32))
                          ).astype(np.float16)
            return (narrow.astype(np.float32)
                    * reciprocal.astype(np.float32)).astype(np.float16).astype(np.float32)
    partial = []
    for lane in range(4):
        lane_partial = []
        for parity in range(2):
            channel = lane * 2 + parity
            first = half_fma(half[..., channel + 8], half[..., channel + 8],
                             half_multiply(half[..., channel], half[..., channel]))
            second = half_fma(half[..., channel + 24], half[..., channel + 24],
                              half_multiply(half[..., channel + 16], half[..., channel + 16]))
            lane_partial.append(half_add(first, second))
        partial.append(np.stack(lane_partial, axis=-1))
    partial_tensor = np.stack(partial, axis=-2)                     # (..., 4, 2)
    xor_two = np.stack([half_add(partial_tensor[..., lane, :],
                                 partial_tensor[..., lane ^ 2, :]) for lane in range(4)],
                       axis=-2)
    xor_one = np.stack([half_add(xor_two[..., lane, :], xor_two[..., lane ^ 1, :])
                        for lane in range(4)], axis=-2)
    norm = half_add(xor_one[..., 0, 0], xor_one[..., 0, 1]).astype(np.float32)
    norm = np.maximum(norm, np.float32(np.float16(COSINE_NORM_FLOOR)))
    reciprocal = _half_rounded(np.float32(1.0) / np.sqrt(norm))[..., None]
    return _half_rounded(half * reciprocal)


def vendor_cosine_publish(value, scale=None):
    normalized = vendor_cosine_normalize(value)
    if scale is not None:
        normalized = _half_rounded(
            normalized * _half_rounded(scale).reshape(1, scale.shape[0], 1, 1))
    return e4m3(normalized)


def cosine_residual(skip, branch, cosine):
    return branch + skip * cosine


# --------------------------------------------------------------------------
# attention-bias fragment order
# --------------------------------------------------------------------------


def _fragment_swizzle_indices():
    """Stored offset of every logical (query, key) window-bias entry."""
    indices = []
    for entry in range(64 * 64):
        query, key = divmod(entry, 64)
        query_y, query_x = divmod(query, 8)
        key_y, key_x = divmod(key, 8)

        def bit(value, position):
            return (value >> position) & 1

        indices.append(
            (bit(query_y, 2) << 11)
            | (bit(query_x, 2) << 10)
            | (bit(key_y, 2) << 9)
            | (bit(key_x, 2) << 8)
            | (bit(query_y, 0) << 7)
            | (bit(query_x, 1) << 6)
            | (bit(query_x, 0) << 5)
            | (bit(key_y, 0) << 4)
            | (bit(key_x, 1) << 3)
            | (bit(key_y, 1) << 2)
            | (bit(query_y, 1) << 1)
            | bit(key_x, 0)
        )
    return np.array(indices, dtype=np.intp)


FRAGMENT_SWIZZLE_INDICES = _fragment_swizzle_indices()


def recover_attention_bias_layout(bias):
    """Undo the fused kernel's mma fragment order of a [heads, 64, 64] bias."""
    if bias.ndim != 3 or bias.shape[1] != 64 or bias.shape[2] != 64:
        raise ValueError("attention bias must be [heads, 64, 64]")
    heads = bias.shape[0]
    flat = bias.reshape(heads, 64 * 64)
    return flat[:, FRAGMENT_SWIZZLE_INDICES].reshape(heads, 64, 64)


def uses_fragment_swizzle(block_index, head_count):
    """Single-head window blocks (block 0 included) and the 16-head split blocks."""
    return head_count in (1, 16)


def recovered_window_origin(block_index):
    """The vendor window origin as (y, x) for one graph block."""
    if block_index == 0:
        phase = 0
    elif 1 <= block_index <= 4:
        phase = block_index - 1
    elif 5 <= block_index <= 8:
        phase = block_index - 5
    elif 9 <= block_index <= 14:
        phase = block_index - 9
    elif 15 <= block_index <= 22:
        phase = block_index - 15
    elif 23 <= block_index <= 30:
        phase = block_index - 23
    elif 40 <= block_index <= 55:
        phase = block_index - (40 if block_index < 48 else 48)
    elif 56 <= block_index <= 61:
        phase = block_index - 54
    elif 62 <= block_index <= 69:
        phase = block_index - (62 if block_index < 66 else 66)
    elif block_index == 70:
        phase = 1
    else:
        return (0, 0)
    return ((0, -4, 0, -4)[phase % 4], (0, -4, -4, 0)[phase % 4])


# --------------------------------------------------------------------------
# chunk helpers
# --------------------------------------------------------------------------


def _per_token(function, value):
    """Apply a token-local function to [..., C] in chunks of CHUNK_TOKENS tokens."""
    channels = value.shape[-1]
    flat = value.reshape(-1, channels)
    if CHUNK_TOKENS <= 0 or flat.shape[0] <= CHUNK_TOKENS:
        return function(value)
    parts = [function(flat[start:start + CHUNK_TOKENS])
             for start in range(0, flat.shape[0], CHUNK_TOKENS)]
    return np.concatenate(parts, axis=0).reshape(*value.shape[:-1], parts[0].shape[-1])


def _rows(function, *tensors):
    """Apply a token-local function of several NHWC tensors in row chunks."""
    lead = tensors[0]
    if (CHUNK_TOKENS <= 0 or lead.ndim != 4
            or lead.shape[0] * lead.shape[1] * lead.shape[2] <= CHUNK_TOKENS):
        return function(*tensors)
    rows = max(1, CHUNK_TOKENS // (lead.shape[0] * lead.shape[2]))
    out = None
    for y0 in range(0, lead.shape[1], rows):
        part = function(*(t[:, y0:y0 + rows] for t in tensors))
        if out is None:
            out = np.empty((lead.shape[0], lead.shape[1], lead.shape[2], part.shape[-1]),
                           dtype=part.dtype)
        out[:, y0:y0 + rows] = part
    return out


def _residual_per_token(value, branch, cosine):
    """cosine_residual in row chunks, written into `branch`."""
    if CHUNK_TOKENS <= 0 or branch.ndim != 4 or branch.shape != value.shape:
        return cosine_residual(value, branch, cosine)
    rows = max(1, CHUNK_TOKENS // (branch.shape[0] * branch.shape[2]))
    for y0 in range(0, branch.shape[1], rows):
        branch[:, y0:y0 + rows] = cosine_residual(
            value[:, y0:y0 + rows], branch[:, y0:y0 + rows], cosine)
    return branch


# --------------------------------------------------------------------------
# attention
# --------------------------------------------------------------------------


def cosine_attention(value, *, qkv_weight, attention_scale, attention_bias,
                     projection_weight, head_count, logit_cap=None,
                     symmetric_logit_cap=False):
    if value.ndim != 3:
        raise ValueError("attention input must be rank 3")
    batch_count, token_count, channels = value.shape
    if head_count <= 0 or channels % head_count:
        raise ValueError("channels must be divisible by a positive head count")
    projected = matmul(value, qkv_weight)
    query, key, projected_value = np.split(projected, 3, axis=-1)
    head_channels = channels // head_count
    head_shape = (batch_count, token_count, head_count, head_channels)
    query = query.reshape(head_shape).transpose(0, 2, 1, 3)
    key = key.reshape(head_shape).transpose(0, 2, 1, 3)
    projected_value = projected_value.reshape(head_shape).transpose(0, 2, 1, 3)

    query = vendor_cosine_publish(np.ascontiguousarray(query), attention_scale)
    key = vendor_cosine_publish(np.ascontiguousarray(key))
    projected_value = e4m3(np.ascontiguousarray(projected_value))

    scores = matmul_nt(query, key)
    if attention_bias is not None:
        scores = scores + attention_bias.reshape(1, head_count, token_count, token_count)
    if logit_cap is not None:
        scores = (np.clip(scores, -logit_cap, logit_cap) if symmetric_logit_cap
                  else np.minimum(scores, logit_cap))
    probabilities = vendor_approximate_softmax(scores)
    attended = matmul(probabilities, projected_value).transpose(0, 2, 1, 3)
    attended = e4m3(np.ascontiguousarray(attended).reshape(batch_count, token_count, channels))
    return matmul(attended, projection_weight)


def partition_windows(value, window_size):
    if value.ndim != 4:
        raise ValueError("window input must be rank-4 NHWC")
    batch_count, height, width, channels = value.shape
    if height % window_size or width % window_size:
        raise ValueError("spatial dimensions must be divisible by window size")
    return (value.reshape(batch_count, height // window_size, window_size,
                          width // window_size, window_size, channels)
            .transpose(0, 1, 3, 2, 4, 5)
            .reshape(-1, window_size * window_size, channels))


def reverse_windows(windows, *, batch_count, height, width, window_size):
    channels = windows.shape[-1]
    return (windows.reshape(batch_count, height // window_size, width // window_size,
                            window_size, window_size, channels)
            .transpose(0, 1, 3, 2, 4, 5)
            .reshape(batch_count, height, width, channels))


def window_attention(value, *, qkv_weight, attention_scale, attention_bias,
                     projection_weight, head_count, window_size,
                     window_origin=(0, 0), logit_cap=None):
    if len(window_origin) != 2 or any(o > 0 or o <= -window_size for o in window_origin):
        raise ValueError("window origin must lie within one non-positive window")
    pad_top, pad_left = -window_origin[0], -window_origin[1]
    pad_bottom = (-(value.shape[1] + pad_top)) % window_size
    pad_right = (-(value.shape[2] + pad_left)) % window_size
    if pad_top or pad_left or pad_bottom or pad_right:
        padded_value = np.pad(value, ((0, 0), (pad_top, pad_bottom),
                                      (pad_left, pad_right), (0, 0)))
    else:
        padded_value = value

    def attend(strip):
        windows = np.ascontiguousarray(partition_windows(strip, window_size))
        attended = cosine_attention(
            windows, qkv_weight=qkv_weight, attention_scale=attention_scale,
            attention_bias=attention_bias, projection_weight=projection_weight,
            head_count=head_count, logit_cap=logit_cap)
        return reverse_windows(attended, batch_count=strip.shape[0],
                               height=strip.shape[1], width=strip.shape[2],
                               window_size=window_size)

    padded_height, padded_width = padded_value.shape[1], padded_value.shape[2]
    window_rows = padded_height // window_size
    tokens_per_window_row = padded_width * window_size
    rows_per_strip = (max(1, CHUNK_TOKENS // tokens_per_window_row)
                      if CHUNK_TOKENS > 0 else window_rows)
    if window_rows <= rows_per_strip:
        attention = attend(padded_value)
    else:
        attention = np.empty_like(padded_value)
        for first in range(0, window_rows, rows_per_strip):
            y0 = first * window_size
            y1 = min(first + rows_per_strip, window_rows) * window_size
            attention[:, y0:y1] = attend(padded_value[:, y0:y1])
    return attention[:, pad_top:pad_top + value.shape[1],
                     pad_left:pad_left + value.shape[2], :]


# --------------------------------------------------------------------------
# block families
# --------------------------------------------------------------------------


def window_block(value, *, expansion_weight, feed_forward_projection_weight,
                 feed_forward_cosine, qkv_weight, attention_scale, attention_bias,
                 attention_projection_weight, attention_cosine, head_count,
                 window_size, window_origin=(0, 0)):
    def feed_forward(tokens):
        branch = matmul(
            e4m3(quadratic_gate_activation(matmul(tokens, expansion_weight))),
            feed_forward_projection_weight)
        return cosine_residual(tokens, branch, feed_forward_cosine)

    feed_forward_output = _per_token(feed_forward, value)
    attention_branch = window_attention(
        feed_forward_output, qkv_weight=qkv_weight, attention_scale=attention_scale,
        attention_bias=attention_bias, projection_weight=attention_projection_weight,
        head_count=head_count, window_size=window_size, window_origin=window_origin)
    return _residual_per_token(feed_forward_output, attention_branch, attention_cosine)


_fused_branched: dict[tuple, tuple] = {}


def _fused_branched_weights(expansion_weight, branch_projection_weight):
    """Fold the branch and input-head axes into two ordinary matrices.

    `expansion_weight[oh, br, ih]` is (32, 32) and the block computes, per output
    head `oh`, `sum_br e4m3(gate(sum_ih x_ih @ W[oh, br, ih])) @ P[oh, br]`. Both
    sums are matrix products in disguise: stacking `ih` down the rows and `br`
    across the columns gives one (C, 128) expansion per output head, and stacking
    `br` down the rows of the projections gives one (128, 32) contraction. The
    gate and the E4M3 publish between them are elementwise, so they pass through
    the block structure untouched.
    """
    key = (expansion_weight.ctypes.data, branch_projection_weight.ctypes.data,
           expansion_weight.shape)
    hit = _fused_branched.get(key)
    if hit is None:
        groups = expansion_weight.shape[0]
        expansion = np.ascontiguousarray(
            expansion_weight.transpose(0, 2, 3, 1, 4).reshape(groups, groups * 32, 4 * 32))
        projection = np.ascontiguousarray(
            branch_projection_weight.reshape(groups, 4 * 32, 32))
        hit = (expansion_weight, branch_projection_weight, expansion, projection)
        _fused_branched[key] = hit
    return hit[2], hit[3]


def branched_feed_forward(value, *, expansion_weight, branch_projection_weight,
                          output_projection_weight):
    channels = value.shape[-1]
    if channels < 64 or channels % 32:
        raise ValueError("branched feed-forward input must have 32-aligned channels")
    channel_groups = channels // 32
    if expansion_weight.shape != (channel_groups, 4, channel_groups, 32, 32):
        raise ValueError("invalid branched expansion weight shape")
    if branch_projection_weight.shape != (channel_groups, 4, 32, 32):
        raise ValueError("invalid branch projection weight shape")
    if output_projection_weight.shape != (channels, channels):
        raise ValueError("invalid branched output projection weight shape")

    if FUSE_BRANCHED:
        expansion, projection = _fused_branched_weights(expansion_weight,
                                                        branch_projection_weight)
        output_heads = [
            e4m3(matmul(e4m3(quadratic_gate_activation(matmul(value, expansion[head]))),
                        projection[head]))
            for head in range(channel_groups)
        ]
        return matmul(np.concatenate(output_heads, axis=-1), output_projection_weight)

    input_heads = np.split(value, channel_groups, axis=-1)
    output_heads = []
    for output_head in range(channel_groups):
        branches = []
        for branch in range(4):
            expanded = matmul(input_heads[0],
                              expansion_weight[output_head, branch, 0])
            for input_head in range(1, channel_groups):
                expanded = expanded + matmul(
                    input_heads[input_head],
                    expansion_weight[output_head, branch, input_head])
            branches.append(matmul(e4m3(quadratic_gate_activation(expanded)),
                                   branch_projection_weight[output_head, branch]))
        total = branches[0]
        for branch in branches[1:]:
            total = total + branch
        output_heads.append(e4m3(total))
    return matmul(np.concatenate(output_heads, axis=-1), output_projection_weight)


def split_group_feed_forward(value, *, first_projection_weight, expand_weight,
                             project_weight):
    """e4m3(x @ first) then a per-64-channel-group 64 -> 256 -> 64 MLP."""
    channels = value.shape[-1]
    if channels % 64:
        raise ValueError("split feed-forward channels must be divisible by 64")
    groups = channels // 64
    if first_projection_weight.shape != (channels, channels):
        raise ValueError("invalid split first projection shape")
    if expand_weight.shape != (groups, 64, 256):
        raise ValueError("invalid split group expansion shape")
    if project_weight.shape != (groups, 256, 64):
        raise ValueError("invalid split group projection shape")
    hidden = e4m3(matmul(value, first_projection_weight))
    outputs = [
        matmul(quadratic_gate_activation(matmul(group_hidden, expand_weight[group])),
               project_weight[group])
        for group, group_hidden in enumerate(np.split(hidden, groups, axis=-1))
    ]
    return e4m3(np.concatenate(outputs, axis=-1))


def branched_window_block(value, *, expansion_weight, branch_projection_weight,
                          output_projection_weight, feed_forward_cosine, qkv_weight,
                          attention_scale, attention_bias, attention_projection_weight,
                          attention_cosine, head_count, window_size,
                          window_origin=(0, 0)):
    def feed_forward(tokens):
        branch = branched_feed_forward(
            tokens, expansion_weight=expansion_weight,
            branch_projection_weight=branch_projection_weight,
            output_projection_weight=output_projection_weight)
        return e4m3(cosine_residual(tokens, branch, feed_forward_cosine))

    feed_forward_output = _per_token(feed_forward, value)
    attention_branch = window_attention(
        feed_forward_output, qkv_weight=qkv_weight, attention_scale=attention_scale,
        attention_bias=attention_bias, projection_weight=attention_projection_weight,
        head_count=head_count, window_size=window_size, window_origin=window_origin)
    return _residual_per_token(feed_forward_output, attention_branch, attention_cosine)


def split_window_block(value, *, first_projection_weight, expand_weight, project_weight,
                       feed_forward_projection_weight, feed_forward_cosine, qkv_weight,
                       attention_scale, attention_bias, attention_projection_weight,
                       attention_cosine, head_count, window_size, window_origin=(0, 0)):
    def feed_forward(tokens):
        branch = matmul(
            split_group_feed_forward(tokens,
                                     first_projection_weight=first_projection_weight,
                                     expand_weight=expand_weight,
                                     project_weight=project_weight),
            feed_forward_projection_weight)
        return cosine_residual(tokens, branch, feed_forward_cosine)

    feed_forward_output = _per_token(feed_forward, value)
    attention_branch = window_attention(
        feed_forward_output, qkv_weight=qkv_weight, attention_scale=attention_scale,
        attention_bias=attention_bias, projection_weight=attention_projection_weight,
        head_count=head_count, window_size=window_size, window_origin=window_origin)
    return _residual_per_token(feed_forward_output, attention_branch, attention_cosine)


def global_block(value, *, expansion_weight, feed_forward_projection_weight,
                 feed_forward_cosine, qkv_weight, attention_scale,
                 attention_projection_weight, attention_cosine, head_count,
                 logit_cap=GLOBAL_ATTENTION_LOGIT_CAP):
    shape = value.shape
    tokens = value.reshape(shape[0], shape[1] * shape[2], shape[3])
    feed_forward_branch = matmul(
        e4m3(quadratic_gate_activation(matmul(tokens, expansion_weight))),
        feed_forward_projection_weight)
    feed_forward_output = cosine_residual(tokens, feed_forward_branch, feed_forward_cosine)
    attention_branch = cosine_attention(
        feed_forward_output, qkv_weight=qkv_weight,
        attention_scale=attention_scale * np.float32(
            math.sqrt(feed_forward_output.shape[-1] // head_count)),
        attention_bias=None,
        projection_weight=attention_projection_weight, head_count=head_count,
        logit_cap=logit_cap, symmetric_logit_cap=True)
    return cosine_residual(feed_forward_output, attention_branch,
                           attention_cosine).reshape(shape)


# --------------------------------------------------------------------------
# transitions
# --------------------------------------------------------------------------


def average_pool2(value):
    if value.ndim != 4 or value.shape[1] % 2 or value.shape[2] % 2:
        raise ValueError("average pool expects even rank-4 NHWC input")
    return (value[:, 0::2, 0::2, :] + value[:, 1::2, 0::2, :]
            + value[:, 0::2, 1::2, :] + value[:, 1::2, 1::2, :]) * np.float32(0.25)


def downsample(value, *, weight):
    return matmul(average_pool2(value), weight)


def pad_spatial_end(value, multiple):
    if value.ndim != 4 or multiple <= 0:
        raise ValueError("spatial padding expects rank-4 NHWC and a positive multiple")
    pad_bottom = (-value.shape[1]) % multiple
    pad_right = (-value.shape[2]) % multiple
    if not (pad_bottom or pad_right):
        return value
    return np.pad(value, ((0, 0), (0, pad_bottom), (0, pad_right), (0, 0)))


def nearest_upsample2_crop(value, *, height, width):
    if value.ndim != 4 or height <= 0 or width <= 0:
        raise ValueError("nearest upsample expects rank-4 NHWC input and target extent")
    upsampled = np.repeat(np.repeat(value, 2, axis=1), 2, axis=2)
    if height > upsampled.shape[1] or width > upsampled.shape[2]:
        raise ValueError("target exceeds the doubled latent extent")
    return upsampled[:, :height, :width, :]


def learned_upsample2(value, *, interpolation):
    """Double the extent, the new samples a per-channel lerp towards the next pixel.

    Recovered alongside the rest of the decoder but not reached by this graph's
    execution order, which upsamples with `nearest_upsample2_crop`. Kept so the
    operator set matches the reference.
    """
    if value.ndim != 4 or interpolation.shape != (value.shape[-1],):
        raise ValueError("upsample expects NHWC input and one interpolation per channel")
    batch_count, height, width, channels = value.shape
    right = np.concatenate((value[:, :, 1:, :], value[:, :, -1:, :]), axis=2)
    horizontal_midpoint = value * (1 - interpolation) + right * interpolation
    horizontal = np.stack((value, horizontal_midpoint), axis=3).reshape(
        batch_count, height, width * 2, channels)
    below = np.concatenate((horizontal[:, 1:, :, :], horizontal[:, -1:, :, :]), axis=1)
    vertical_midpoint = horizontal * (1 - interpolation) + below * interpolation
    return np.stack((horizontal, vertical_midpoint), axis=2).reshape(
        batch_count, height * 2, width * 2, channels)


def decoder_input_merge(value, *, skip, skip_sine):
    if value.ndim != 4 or skip.ndim != 4 or value.shape[0] != skip.shape[0]:
        raise ValueError("decoder input merge expects compatible NHWC tensors")
    if value.shape[-1] != skip.shape[-1] or skip_sine.shape != (value.shape[-1],):
        raise ValueError("decoder input merge channel mismatch")
    upsampled = nearest_upsample2_crop(value, height=skip.shape[1], width=skip.shape[2])
    return upsampled + skip * skip_sine


# --------------------------------------------------------------------------
# the model
# --------------------------------------------------------------------------

_ST_DTYPES = {"F16": np.float16, "F32": np.float32, "BF16": None}


def load_logical(path):
    """Read the logical safetensors into a dict of float32 arrays."""
    with open(path, "rb") as source:
        header_length = struct.unpack("<Q", source.read(8))[0]
        header = json.loads(source.read(header_length))
        base = 8 + header_length
        metadata = header.pop("__metadata__", {})
        if metadata.get("fully_logical") != "true":
            raise ValueError("weights must declare fully_logical=true")
        blob = np.memmap(path, dtype=np.uint8, mode="r")
    weights = {}
    for name, entry in header.items():
        dtype = _ST_DTYPES.get(entry["dtype"])
        if dtype is None:
            raise ValueError(f"{name}: unsupported dtype {entry['dtype']}")
        start, stop = entry["data_offsets"]
        raw = blob[base + start:base + stop].view(dtype)
        weights[name] = np.ascontiguousarray(raw.reshape(entry["shape"]),
                                             dtype=np.float32)
    return weights, metadata


class NeuralRenderingModel:
    """The fixed recovered 71-block graph over external logical weights."""

    def __init__(self, weights):
        self.weights = weights

    @classmethod
    def from_safetensors(cls, path):
        weights, _ = load_logical(path)
        return cls(weights)

    def weight(self, name):
        try:
            return self.weights[name]
        except KeyError:
            raise ValueError(f"missing logical weight: {name}") from None

    # -- block dispatch ---------------------------------------------------

    def _window(self, value, index, *, head_count, publish=True):
        prefix = f"block{index}.layer0"
        attention_bias = self.weight(f"{prefix}.attn_bias")
        if uses_fragment_swizzle(index, head_count):
            attention_bias = recover_attention_bias_layout(attention_bias)
        common = dict(
            feed_forward_cosine=self.weight(f"{prefix}.ffn_cos_skip"),
            qkv_weight=self.weight(f"{prefix}.qkv_weight"),
            attention_scale=self.weight(f"{prefix}.attn_scale"),
            attention_bias=attention_bias,
            attention_projection_weight=self.weight(f"{prefix}.projection_weight"),
            attention_cosine=self.weight(f"{prefix}.attn_cos_skip"),
            head_count=head_count, window_size=8,
            window_origin=recovered_window_origin(index),
        )
        if f"{prefix}.ffn_expand_weight" in self.weights:
            output = branched_window_block(
                value,
                expansion_weight=self.weight(f"{prefix}.ffn_expand_weight"),
                branch_projection_weight=self.weight(f"{prefix}.ffn_branch_projection_weight"),
                output_projection_weight=self.weight(f"{prefix}.ffn_output_projection_weight"),
                **common)
        else:
            output = window_block(
                value,
                expansion_weight=self.weight(f"{prefix}.weight1"),
                feed_forward_projection_weight=self.weight(f"{prefix}.weight2"),
                **common)
        # Block 0 is pooled before its publication and block 70 feeds the head.
        return output if index in (0, 70) or not publish else e4m3(output)

    def _split_window(self, value, index):
        prefix = f"block{index}"
        attention_bias = self.weight(f"{prefix}.layer2.attn_bias")
        if uses_fragment_swizzle(index, 16):
            attention_bias = recover_attention_bias_layout(attention_bias)
        return e4m3(split_window_block(
            value,
            first_projection_weight=self.weight(f"{prefix}.layer0.first_projection_weight"),
            expand_weight=self.weight(f"{prefix}.layer0.group_expand_weight"),
            project_weight=self.weight(f"{prefix}.layer0.group_project_weight"),
            feed_forward_projection_weight=self.weight(f"{prefix}.layer1.weight3"),
            feed_forward_cosine=self.weight(f"{prefix}.layer1.ffn_cos_skip"),
            qkv_weight=self.weight(f"{prefix}.layer2.qkv_weight"),
            attention_scale=self.weight(f"{prefix}.layer2.attn_scale"),
            attention_bias=attention_bias,
            attention_projection_weight=self.weight(f"{prefix}.layer3.projection_weight"),
            attention_cosine=self.weight(f"{prefix}.layer3.attn_cos_skip"),
            head_count=16, window_size=8,
            window_origin=recovered_window_origin(index)))

    def _global(self, value, index):
        prefix = f"block{index}"
        return e4m3(global_block(
            value,
            expansion_weight=self.weight(f"{prefix}.layer0.weight"),
            feed_forward_projection_weight=self.weight(f"{prefix}.layer1.weight"),
            feed_forward_cosine=self.weight(f"{prefix}.layer1.ffn_cos_skip"),
            qkv_weight=self.weight(f"{prefix}.layer2.qkv_weight"),
            attention_scale=self.weight(f"{prefix}.layer2.attn_scale"),
            attention_projection_weight=self.weight(f"{prefix}.layer4.projection_weight"),
            attention_cosine=self.weight(f"{prefix}.layer4.attn_cos_skip"),
            head_count=32))

    def _downsample_window(self, value, index, *, head_count):
        transformed = self._window(value, index, head_count=head_count, publish=False)
        if index == 22:
            transformed = pad_spatial_end(transformed, 8)
        return e4m3(matmul(e4m3(average_pool2(transformed)),
                           self.weight(f"block{index}.layer0.weight0")))

    def _upsample_window(self, value, skip, index, *, head_count):
        prefix = f"block{index}.layer0"
        projected = matmul(value, self.weight(f"{prefix}.weight0"))
        skip_path = skip * self.weight(f"{prefix}.sin")
        upsampled = nearest_upsample2_crop(projected, height=skip.shape[1],
                                           width=skip.shape[2])
        return self._window(e4m3(upsampled + skip_path), index, head_count=head_count)

    # -- forward ----------------------------------------------------------

    def forward(self, input_value, progress=None):
        def step(name):
            if progress is not None:
                progress(name)

        adapter = self.weight("block0.layer0.input_adapter_weight")
        value = _per_token(lambda tokens: matmul(tokens, adapter), input_value)
        step("adapter")
        block0_output = self._window(value, 0, head_count=1)
        del value
        step("block0")
        full_resolution_skip = e4m3(block0_output)
        value = e4m3(average_pool2(block0_output))
        del block0_output
        for index in range(1, 4):
            value = self._window(value, index, head_count=1)
            step(f"block{index}")
        skips = [value]
        value = self._downsample_window(value, 4, head_count=1)
        step("block4")

        for regular, transition, head_count in ((range(5, 8), 8, 2),
                                                (range(9, 14), 14, 4),
                                                (range(15, 22), 22, 8)):
            for index in regular:
                value = self._window(value, index, head_count=head_count)
                step(f"block{index}")
            skips.append(value)
            value = self._downsample_window(value, transition, head_count=head_count)
            step(f"block{transition}")

        for index in range(23, 31):
            value = self._split_window(value, index)
            step(f"block{index}")
        split_skip = value
        value = downsample(pad_spatial_end(value, 8),
                           weight=self.weight("block30.layer4.weight"))
        value = e4m3(value)
        step("block30.ds")
        for index in range(31, 39):
            value = self._global(value, index)
            step(f"block{index}")

        value = decoder_input_merge(
            matmul(value, self.weight("block39.layer0.conv_weight")),
            skip=split_skip, skip_sine=self.weight("block39.layer0.inp_upsample_sin"))
        value = e4m3(value)
        step("block39")
        for index in range(40, 48):
            value = self._split_window(value, index)
            step(f"block{index}")
        value = self._upsample_window(value, skips[3], 48, head_count=8)
        step("block48")
        for index in range(49, 56):
            value = self._window(value, index, head_count=8)
            step(f"block{index}")

        for transition, regular, skip_index, head_count in (
                (56, range(57, 62), 2, 4),
                (62, range(63, 66), 1, 2),
                (66, range(67, 70), 0, 1)):
            value = self._upsample_window(value, skips[skip_index], transition,
                                          head_count=head_count)
            step(f"block{transition}")
            for index in regular:
                value = self._window(value, index, head_count=head_count)
                step(f"block{index}")

        value = nearest_upsample2_crop(value, height=full_resolution_skip.shape[1],
                                       width=full_resolution_skip.shape[2])
        merge_sin = self.weight("block70.layer0.inp_merge_sin")
        merge_cos = self.weight("block70.layer0.inp_merge_cos")
        value = _rows(lambda up, skip: up * merge_sin + skip * merge_cos,
                      value, full_resolution_skip)
        del full_resolution_skip
        value = self._window(value, 70, head_count=1)
        step("block70")
        out_gain = self.weight("block70.layer0.out_gain")
        out_conv = self.weight("block70.layer0.out_conv_weight")
        return _per_token(
            lambda tokens: matmul(tokens[..., :16], out_gain)
            + matmul(tokens[..., 16:], out_conv), value)

    __call__ = forward
