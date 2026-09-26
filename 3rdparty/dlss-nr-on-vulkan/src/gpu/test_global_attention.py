#!/usr/bin/env python3
"""A bottleneck block's attention in one pass, against the four passes it replaces.

`global_attention` must write what QK^T, the bit-affine softmax, PV and merge_heads write,
byte for byte: whole and partial 64-row blocks, token counts below the rows, an odd one
(whose last pair takes a padding column into the sum, as the softmax does), and nothing
written past the output.
"""
import numpy as np
import xmxres as X

GUARD = 64
FILL16 = np.array([0x7E5A], np.uint16).view(np.float16)[0]


def main():
    rt = X.Runtime()
    rng = np.random.default_rng(2610)
    cases = 0
    # the graph's own shapes (32 heads: 320x320, 1280x768 and a 128x128 network's 16
    # tokens on 32 rows) among partial blocks, odd token counts and a row count under 64
    for rows, tokens, heads in ((64, 64, 32), (256, 240, 32), (32, 16, 32), (96, 90, 8),
                                (112, 105, 4), (256, 256, 8), (640, 640, 4), (32, 17, 2)):
        channels = heads * 32
        buffers = []

        def alloc(n, dtype):
            b = rt.buffer(n, dtype)
            buffers.append(b)
            return b
        try:
            q, k, v = (alloc(heads * rows * 32, np.float16) for _ in range(3))
            scores = alloc(heads * rows * rows, np.float32)
            probs = alloc(heads * rows * rows, np.float16)
            context = alloc(heads * rows * 32, np.float32)
            for spread in (0.2, 1.0):
                # Q and K as the QKV epilogue leaves them — unit-ish rows, Q scaled
                for buf, scale in ((q, 6.0), (k, 1.0), (v, 1.0)):
                    X.host_write(buf, (rng.normal(0, spread, heads * rows * 32) * scale / 5.0)
                                 .astype(np.float16))
                for mask in (0, 7):
                    rt.specialize(mask)
                    outs = []
                    for fused in (False, True):
                        merged = alloc(rows * channels + GUARD, np.float16)
                        X.host_write(merged, np.full(rows * channels + GUARD, FILL16, np.float16))
                        rt.begin()
                        if fused:
                            rt.global_attention(q, k, v, merged, rows, tokens, heads, 3.0)
                        else:
                            rt.gemm(q, k, scores, rows, rows, 32, batch=heads,
                                    strides=(rows * 32, rows * 32, rows * rows), transpose_b=True)
                            rt.softmax(scores, probs, heads * rows, tokens, stride=rows, cap=3.0,
                                       narrow=True)
                            rt.gemm(probs, v, context, rows, 32, rows, batch=heads,
                                    strides=(rows * rows, rows * 32, rows * 32))
                            rt.merge_heads(context, merged, 1, rows, channels, heads,
                                           epilogue=X.EPI_E4M3, narrow=True)
                        rt.submit()
                        outs.append(X.host_view(merged, np.float16).copy())
                    n = rows * channels
                    name = f"{rows} rows, {tokens} tokens, {heads} heads, spread {spread}, mask {mask}"
                    want, got = outs[0][:n].view(np.uint16), outs[1][:n].view(np.uint16)
                    if not np.array_equal(got, want):
                        bad = np.flatnonzero(got != want)
                        raise AssertionError(f"{name}: {bad.size} of {n} differ, first at {bad[0]}")
                    assert (outs[1][n:].view(np.uint16) == np.uint16(0x7E5A)).all(), f"{name}: written past"
                    cases += 1
        finally:
            for b in buffers:
                b.free()
    print(f"global attention: {cases} cases bit-identical to the four passes it replaces; "
          f"staging={rt.staging}")


if __name__ == "__main__":
    main()
