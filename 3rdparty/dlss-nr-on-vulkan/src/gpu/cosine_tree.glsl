/*
 * cosine_tree — the cosine normalise's reciprocal norm, one definition for every pass
 * that computes it: `attention.comp`'s row pass and the QKV projection's own epilogue
 * (`qkv_epilogue.glsl`). The two must agree to the bit, and the only way to be sure they
 * do is for there to be one copy of the arithmetic.
 *
 * Needs `half_round` from publish.glsl.
 */

const float COSINE_NORM_FLOOR = 0.00006198883056640625;

/* half(l*r), half(l+r), half(l*r + acc) — each a float32 expression with one rounding
 * at the end, matching `_half_multiply` / `_half_add` / `_half_fma`. `precise` stops an
 * FMA contraction, which would drop the float32 rounding between the multiply and the
 * add and give a different answer. */
float hmul(float l, float r) { precise float t = l * r; return half_round(t); }
float hadd(float l, float r) { precise float t = l + r; return half_round(t); }
float hfma(float l, float r, float acc) { precise float t = l * r + acc; return half_round(t); }

/* `h` is one head's 32 channels, already rounded to half. The vendor's fragment tree:
 * eight partial sums of four squares, then two butterfly levels, then the last add —
 * each step a half operation, in this order and no other. */
float cosine_reciprocal(float h[32]) {
    float partial[4][2];
    for (uint lane = 0u; lane < 4u; lane++)
        for (uint parity = 0u; parity < 2u; parity++) {
            uint ch = lane * 2u + parity;
            float first  = hfma(h[ch + 8u],  h[ch + 8u],  hmul(h[ch],       h[ch]));
            float second = hfma(h[ch + 24u], h[ch + 24u], hmul(h[ch + 16u], h[ch + 16u]));
            partial[lane][parity] = hadd(first, second);
        }
    float two[4][2], one[4][2];
    for (uint lane = 0u; lane < 4u; lane++)
        for (uint p = 0u; p < 2u; p++) two[lane][p] = hadd(partial[lane][p], partial[lane ^ 2u][p]);
    for (uint lane = 0u; lane < 4u; lane++)
        for (uint p = 0u; p < 2u; p++) one[lane][p] = hadd(two[lane][p], two[lane ^ 1u][p]);

    float norm = max(hadd(one[0][0], one[0][1]), half_round(COSINE_NORM_FLOOR));
    return half_round(inversesqrt(norm));
}
