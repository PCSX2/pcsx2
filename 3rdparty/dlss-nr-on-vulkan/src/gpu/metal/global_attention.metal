/*
 * global_attention — a bottleneck block's attention, every token over every token, in one
 * pass: `global_attention.comp` (simdgroup, `global_attention`) and
 * `global_attention_portable.comp` (`global_attention_portable`) in MSL. QK^T, the
 * bit-affine softmax, PV and the head merge, no score stored.
 *
 * A 256-thread threadgroup takes one head and 64 query rows, a simdgroup (slice) eight of
 * them, and the keys come through threadgroup memory 64 at a time, twice: the first time
 * each row's weights are summed in key order — the row softmax's own order, carried from
 * block to block — for the reciprocal; the second the logits are made again, normalised
 * and published as the softmax publishes them, and multiplied into V.
 *
 * Each must equal this runtime's own four passes on its path, bit for bit: the batched
 * transposed-B GEMM for QK^T, attention.metal's whole-row softmax, the batched GEMM for PV
 * over every padded key, and merge_heads with the E4M3 publish. On the simdgroup path each
 * 8x8 tile is summed from zero over 8-wide K slices ascending, as every gemm_simd kernel
 * sums; on the portable path a float32 accumulator takes one K term at a time.
 *
 * Threadgroup memory: a block of keys and its values as halves (8 KB) and two kilobytes a
 * simdgroup (slice) — the simdgroup kernel stages its logits there 32 keys at a time and
 * keeps the block's 64 weights or probabilities as halves beside them; the portable kernel
 * keeps all 64 logits as floats — and the rows' reciprocals: 24.25 KB.
 *
 * Push: a Q, b K, d V, each (heads, rows, 32) half; c the merged output (rows, 32 * heads)
 * half; m the rows (a multiple of 16), n the tokens the softmax counts, batch the heads,
 * p0 the logit clamp. Grid (ceil(rows / 64), heads).
 */
#include "nr_epilogue.h"

/* attention.metal's prepared logit (no bias here) and its fast pair transform */
inline float2 ga_weights(float cap, float l0, float l1) {
    if (cap != 0.0f) { l0 = clamp(l0, -cap, cap); l1 = clamp(l1, -cap, cap); }
    float2 affine;
    for (uint j = 0u; j < 2u; j++) {
        float scaled = half_round(j == 0u ? l0 : l1) * 0.044921875f;
        scaled += 1.30078125f;
        affine[j] = clamp(scaled, 1.03125f, 1.5693359375f);
    }
    uint transformed = (as_type<uint>(half2(affine)) << 5) + 0x7FF88000u;
    return float2(as_type<half2>(transformed));
}

/* A block of keys (and values) into threadgroup memory, zero past `count`. */
inline void ga_load(constant Push &pc, uint plane, uint first, uint count, bool values,
                    threadgroup half *keys, threadgroup half *vals, uint lid) {
    for (uint e = lid; e < 64u * 32u; e += 256u) {
        bool in = e / 32u < count;
        keys[e] = in ? half_ptr(pc.b)[plane + first * 32u + e] : 0.0h;
        if (values) vals[e] = in ? half_ptr(pc.d)[plane + first * 32u + e] : 0.0h;
    }
}

kernel void global_attention(constant Push &pc [[buffer(0)]],
                             uint3 wg [[threadgroup_position_in_grid]],
                             uint lid [[thread_index_in_threadgroup]],
                             uint sg [[simdgroup_index_in_threadgroup]],
                             uint sl [[thread_index_in_simdgroup]]) {
    threadgroup half keys[64 * 32], vals[64 * 32];
    threadgroup float area[8 * 512];
    threadgroup float reciprocal[64];
    uint rows = pc.m, heads = pc.batch, head = wg.y;
    uint paired = (pc.n + 1u) & ~1u;
    uint row = wg.x * 64u + sg * 8u;
    bool busy = row < rows;                     /* uniform across the simdgroup */
    threadgroup float *stage = area + sg * 512u;                     /* 8 x 32 float */
    threadgroup half *weights = reinterpret_cast<threadgroup half *>(stage + 256u);   /* 8 x 64 half */
    uint plane = head * rows * 32u;
    uint blocks = (rows + 63u) / 64u;

    simdgroup_half8x8 query[4];
    if (busy)
        for (uint c = 0u; c < 4u; c++) simdgroup_load(query[c], half_ptr(pc.a) + plane + row * 32u + c * 8u, 32);
    float total = 0.0f;                          /* lane r < 8: row r's sum */
    simdgroup_float8x8 context[4];
    for (uint j = 0u; j < 4u; j++) context[j] = simdgroup_float8x8(0.0f);

    for (uint pass = 0u; pass < 2u; pass++) {
        if (pass == 1u && sl < 8u) reciprocal[sg * 8u + sl] = half_round(1.0f / half_round(total));
        for (uint block = 0u; block < blocks; block++) {
            uint first = block * 64u, count = min(64u, rows - first);
            threadgroup_barrier(mem_flags::mem_threadgroup);    /* the last block is done with */
            ga_load(pc, plane, first, count, pass == 1u, keys, vals, lid);
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (!busy) continue;
            for (uint half_keys = 0u; half_keys * 32u < count; half_keys++) {
                simdgroup_float8x8 logits[4];
                for (uint j = 0u; j < 4u; j++) logits[j] = simdgroup_float8x8(0.0f);
                for (uint c = 0u; c < 4u; c++)
                    for (uint j = 0u; j < 4u; j++) {
                        simdgroup_half8x8 k;
                        simdgroup_load(k, keys + (half_keys * 32u + j * 8u) * 32u + c * 8u, 32, ulong2(0, 0), true);
                        simdgroup_multiply_accumulate(logits[j], query[c], k, logits[j]);
                    }
                for (uint j = 0u; j < 4u; j++) simdgroup_store(logits[j], stage + j * 8u, 32);
                simdgroup_barrier(mem_flags::mem_threadgroup);
                for (uint t = 0u; t < 4u; t++) {
                    uint at = sl * 2u + t * 64u, rr = at / 32u, key = half_keys * 32u + at % 32u;
                    float2 w = ga_weights(pc.p0, stage[at], stage[at + 1u]);
                    if (pass == 1u) {
                        uint column = first + key;
                        float scale = reciprocal[sg * 8u + rr];
                        w.x = column < paired ? e4m3(hmul(w.x, scale)) : 0.0f;
                        w.y = column + 1u < paired ? e4m3(hmul(w.y, scale)) : 0.0f;
                    }
                    weights[rr * 64u + key] = half(w.x);
                    weights[rr * 64u + key + 1u] = half(w.y);
                }
                simdgroup_barrier(mem_flags::mem_threadgroup);
            }
            if (pass == 0u) {
                if (sl < 8u) {
                    uint counted = paired > first ? min(64u, paired - first) : 0u;
                    for (uint i = 0u; i < counted; i++) total += float(weights[sl * 64u + i]);
                }
            } else {
                for (uint c = 0u; c < count; c += 8u) {
                    simdgroup_half8x8 p;
                    simdgroup_load(p, weights + c, 64);
                    for (uint j = 0u; j < 4u; j++) {
                        simdgroup_half8x8 v;
                        simdgroup_load(v, vals + c * 32u + j * 8u, 32);
                        simdgroup_multiply_accumulate(context[j], p, v, context[j]);
                    }
                }
            }
            simdgroup_barrier(mem_flags::mem_threadgroup);
        }
    }
    if (!busy) return;
    for (uint j = 0u; j < 4u; j++) simdgroup_store(context[j], stage + j * 8u, 32);
    simdgroup_barrier(mem_flags::mem_threadgroup);
    uint channels = heads * 32u;
    for (uint e = sl * 4u; e < 256u; e += 128u) {
        uint r = row + e / 32u, d = e % 32u;
        half4 values;
        for (uint i = 0u; i < 4u; i++) values[i] = half(e4m3(stage[e + i]));
        reinterpret_cast<device half4 *>(half_out(pc.c))[(r * channels + head * 32u + d) >> 2] = values;
    }
}

kernel void global_attention_portable(constant Push &pc [[buffer(0)]],
                                      uint3 wg [[threadgroup_position_in_grid]],
                                      uint lid [[thread_index_in_threadgroup]]) {
    threadgroup half keys[64 * 32], vals[64 * 32];
    threadgroup float region[8 * 512];          /* 8 rows x 64 keys a slice */
    threadgroup float reciprocal[64];
    uint rows = pc.m, heads = pc.batch, head = wg.y;
    uint paired = (pc.n + 1u) & ~1u;
    uint slice = lid >> 5u, lane = lid & 31u;
    uint r = lane >> 2u, cq = (lane & 3u) * 4u;
    uint row = wg.x * 64u + slice * 8u;
    bool busy = row < rows;
    threadgroup float *mine = region + slice * 512u;
    uint plane = head * rows * 32u;
    uint blocks = (rows + 63u) / 64u;

    float total = 0.0f;
    float4 context[2];
    for (uint j = 0u; j < 2u; j++) context[j] = float4(0.0f);

    for (uint pass = 0u; pass < 2u; pass++) {
        if (pass == 1u && lane < 8u) reciprocal[slice * 8u + lane] = half_round(1.0f / half_round(total));
        for (uint block = 0u; block < blocks; block++) {
            uint first = block * 64u, count = min(64u, rows - first);
            threadgroup_barrier(mem_flags::mem_threadgroup);
            ga_load(pc, plane, first, count, pass == 1u, keys, vals, lid);
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (busy) {
                /* QK^T for this block's keys, as the transposed-B portable GEMM sums it */
                float4 acc[4];
                for (uint j = 0u; j < 4u; j++) acc[j] = float4(0.0f);
                for (uint c = 0u; c < 32u; c++) {
                    float4 kv[4];
                    for (uint j = 0u; j < 4u; j++) {
                        uint at = (cq + j * 16u) * 32u + c;
                        kv[j] = float4(float(keys[at]), float(keys[at + 32u]), float(keys[at + 64u]), float(keys[at + 96u]));
                    }
                    float qv = float(half_ptr(pc.a)[plane + (row + r) * 32u + c]);
                    for (uint j = 0u; j < 4u; j++) acc[j] += qv * kv[j];
                }
                for (uint j = 0u; j < 4u; j++)
                    for (uint e = 0u; e < 4u; e++) mine[r * 64u + cq + j * 16u + e] = acc[j][e];
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (busy) {
                for (uint at = lane * 2u; at < 512u; at += 64u) {
                    float2 w = ga_weights(pc.p0, mine[at], mine[at + 1u]);
                    if (pass == 1u) {
                        uint column = first + at % 64u;
                        float scale = reciprocal[slice * 8u + at / 64u];
                        w.x = column < paired ? e4m3(hmul(w.x, scale)) : 0.0f;
                        w.y = column + 1u < paired ? e4m3(hmul(w.y, scale)) : 0.0f;
                    }
                    mine[at] = w.x;
                    mine[at + 1u] = w.y;
                }
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (busy) {
                if (pass == 0u) {
                    if (lane < 8u) {
                        uint counted = paired > first ? min(64u, paired - first) : 0u;
                        for (uint i = 0u; i < counted; i++) total += mine[lane * 64u + i];
                    }
                } else {
                    for (uint key = 0u; key < count; key++) {
                        float4 vv[2];
                        for (uint j = 0u; j < 2u; j++) {
                            uint at = key * 32u + cq + j * 16u;
                            vv[j] = float4(float(vals[at]), float(vals[at + 1u]), float(vals[at + 2u]), float(vals[at + 3u]));
                        }
                        float p = mine[r * 64u + key];
                        for (uint j = 0u; j < 2u; j++) context[j] += p * vv[j];
                    }
                }
            }
        }
    }
    if (!busy) return;
    uint channels = heads * 32u;
    for (uint j = 0u; j < 2u; j++) {
        half4 values;
        for (uint e = 0u; e < 4u; e++) values[e] = half(e4m3(context[j][e]));
        reinterpret_cast<device half4 *>(half_out(pc.c))[((row + r) * channels + head * 32u + cq + j * 16u) >> 2] = values;
    }
}
