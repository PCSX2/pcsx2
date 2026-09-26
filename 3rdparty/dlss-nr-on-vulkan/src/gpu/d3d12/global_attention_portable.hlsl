/*
 * global_attention_portable — `global_attention_portable.comp` in HLSL: a bottleneck
 * block's attention in one pass, QK^T, the bit-affine softmax, PV and the head merge, no
 * score stored. A 256-lane group takes one head (y) and 64 query rows (x, split past 65535
 * with pc.spare), a 32-lane slice eight of them; the keys come through groupshared memory
 * 64 at a time, twice — the first time each row's weights are summed in key order for the
 * reciprocal, the second the logits are made again, normalised, published and multiplied
 * into V.
 *
 * It must equal this backend's four passes bit for bit: the batched transposed-B
 * `gemm_portable.hlsl` for QK^T, attention.hlsl's softmax, the batched `gemm_portable.hlsl`
 * for PV over every padded key, and merge_heads with the E4M3 publish. So the products are
 * the portable GEMM's lane layout and order (a lane one query row and four consecutive
 * columns, float4 accumulators from zero, the PV's running across the key blocks in key
 * order), and the clamp, the transform, the sum over the paired columns and the reciprocal
 * are attention.hlsl's to the expression. Not yet run on a Direct3D 12 device.
 *
 * 24.25 KB of groupshared memory: a block of keys and its values as halves, two to a word
 * (8 KB), two kilobytes of logits a slice (16 KB), the rows' reciprocals.
 *
 * Operands: u0 Q, u1 K, u3 V, each (heads, rows, 32) half; u2 the merged output (rows, 32 *
 * heads) half; m the rows (a multiple of 16), n the tokens the softmax counts, batch the
 * heads, p0 the logit clamp.
 */
#include "nr_d3d.hlsli"
#include "nr_epilogue.hlsli"

groupshared uint keys[1024];
groupshared uint values[1024];
groupshared float region[4096];                  /* 8 rows x 64 keys a slice */
groupshared float reciprocal[64];

float lo(uint w) { return f16tof32(w & 0xFFFFu); }
float hi(uint w) { return f16tof32(w >> 16); }
float word_half(uint w, uint i) { return (i & 1u) ? hi(w) : lo(w); }

/* attention.hlsl's `prepared` (no bias here) and `pair_fast` */
void pair_weights(float l0, float l1, out float w0, out float w1) {
    float logits[2] = { l0, l1 };
    float affine[2];
    [unroll] for (uint j = 0u; j < 2u; j++) {
        float logit = logits[j];
        if (pc.p0 != 0.0) logit = clamp(logit, -pc.p0, pc.p0);
        precise float scaled = half_round(logit) * 0.044921875;
        scaled += 1.30078125;
        affine[j] = clamp(scaled, 1.03125, 1.5693359375);
    }
    uint packed = f32tof16(affine[0]) | (f32tof16(affine[1]) << 16);
    uint transformed = (packed << 5) + 0x7FF88000u;
    w0 = lo(transformed);
    w1 = hi(transformed);
}

[numthreads(256, 1, 1)]
void main(uint3 gid : SV_GroupID, uint index : SV_GroupIndex) {
    uint rows = pc.m, heads = pc.batch, head = gid.y;
    uint paired = (pc.n + 1u) & ~1u;             // the columns the softmax counts, in pairs
    uint slice = index >> 5u, lane = index & 31u;
    uint r = lane >> 2u, cq = (lane & 3u) * 4u;
    uint row = (gid.x + pc.spare) * 64u + slice * 8u;
    bool busy = row < rows;                       // uniform across the slice
    uint mine = slice * 512u;
    uint plane = head * rows * 32u;               // this head's (rows, 32)
    uint blocks = (rows + 63u) / 64u;
    uint Q = pc.oa.x, K = pc.ob.x, V = pc.od.x;

    float total = 0.0;                            // lane r < 8: row r's sum
    float4 ctx[2];
    [unroll] for (uint j0 = 0u; j0 < 2u; j0++) ctx[j0] = float4(0.0, 0.0, 0.0, 0.0);

    for (uint pass = 0u; pass < 2u; pass++) {
        if (pass == 1u && lane < 8u) {
            precise float inverse = 1.0 / half_round(total);
            reciprocal[slice * 8u + lane] = half_round(inverse);
        }
        for (uint block = 0u; block < blocks; block++) {
            uint first = block * 64u, count = min(64u, rows - first);
            GroupMemoryBarrierWithGroupSync();    // the last block is done with
            {
                /* 16 bytes a lane: key lane / 4, a quarter of its 32 halves; a partial
                 * block's missing keys are zero */
                uint key = index / 4u;
                uint4 zero = uint4(0u, 0u, 0u, 0u);
                uint4 k4 = key < count ? bufB.Load4(K + (plane + first * 32u) * 2u + index * 16u) : zero;
                [unroll] for (uint w = 0u; w < 4u; w++) keys[index * 4u + w] = k4[w];
                if (pass == 1u) {
                    uint4 v4 = key < count ? bufD.Load4(V + (plane + first * 32u) * 2u + index * 16u) : zero;
                    [unroll] for (uint w2 = 0u; w2 < 4u; w2++) values[index * 4u + w2] = v4[w2];
                }
            }
            GroupMemoryBarrierWithGroupSync();
            if (busy) {
                /* QK^T for this block's keys, as the transposed-B portable GEMM sums it */
                float4 acc[4];
                [unroll] for (uint j1 = 0u; j1 < 4u; j1++) acc[j1] = float4(0.0, 0.0, 0.0, 0.0);
                for (uint c = 0u; c < 32u; c++) {
                    float4 kv[4];
                    [unroll] for (uint j = 0u; j < 4u; j++) {
                        uint at = (cq + j * 16u) * 32u + c;
                        kv[j] = float4(word_half(keys[at >> 1u], at), word_half(keys[(at + 32u) >> 1u], at),
                                       word_half(keys[(at + 64u) >> 1u], at), word_half(keys[(at + 96u) >> 1u], at));
                    }
                    float qv = ld_f16(bufA, Q, plane + (row + r) * 32u + c);
                    [unroll] for (uint j2 = 0u; j2 < 4u; j2++)
                        acc[j2] += qv * kv[j2];
                }
                [unroll] for (uint j3 = 0u; j3 < 4u; j3++)
                    [unroll] for (uint e = 0u; e < 4u; e++)
                        region[mine + r * 64u + cq + j3 * 16u + e] = acc[j3][e];
            }
            GroupMemoryBarrierWithGroupSync();
            if (busy) {
                for (uint at = lane * 2u; at < 512u; at += 64u) {
                    float w0, w1;
                    pair_weights(region[mine + at], region[mine + at + 1u], w0, w1);
                    if (pass == 0u) {
                        region[mine + at] = w0;
                        region[mine + at + 1u] = w1;
                    } else {
                        uint column = first + at % 64u;
                        float s = reciprocal[slice * 8u + at / 64u];
                        region[mine + at] = column < paired ? e4m3(hmul(w0, s)) : 0.0;
                        region[mine + at + 1u] = column + 1u < paired ? e4m3(hmul(w1, s)) : 0.0;
                    }
                }
            }
            GroupMemoryBarrierWithGroupSync();
            if (busy) {
                if (pass == 0u) {
                    /* each row's lane adds its weights in key order, the softmax's own */
                    if (lane < 8u) {
                        uint counted = paired > first ? min(64u, paired - first) : 0u;
                        for (uint i = 0u; i < counted; i++) total += region[mine + lane * 64u + i];
                    }
                } else {
                    /* PV as the plain portable GEMM sums it, key after key */
                    for (uint key2 = 0u; key2 < count; key2++) {
                        float4 vv[2];
                        [unroll] for (uint j = 0u; j < 2u; j++) {
                            uint at = (key2 * 32u + cq + j * 16u) / 2u;
                            uint w0 = values[at], w1 = values[at + 1u];
                            vv[j] = float4(lo(w0), hi(w0), lo(w1), hi(w1));
                        }
                        float p = region[mine + r * 64u + key2];
                        [unroll] for (uint j4 = 0u; j4 < 2u; j4++)
                            ctx[j4] += p * vv[j4];
                    }
                }
            }
        }
    }
    if (!busy) return;                            // no barrier follows

    /* the head merge's store, published: (rows, heads * 32), this head's 32 columns */
    uint channels = heads * 32u, C = pc.oc.x;
    [unroll] for (uint j5 = 0u; j5 < 2u; j5++) {
        float4 v;
        [unroll] for (uint e = 0u; e < 4u; e++) v[e] = e4m3(ctx[j5][e]);
        uint at = (row + r) * channels + head * 32u + cq + j5 * 16u;
        if ((C & 7u) == 0u) st_f16x4(bufC, C + at * 2u, v);
        else [unroll] for (uint e2 = 0u; e2 < 4u; e2++) st_f16(bufC, C, at + e2, v[e2]);
    }
}
