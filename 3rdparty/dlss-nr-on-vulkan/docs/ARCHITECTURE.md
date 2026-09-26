# DLSS-NR, as recovered

What the network is, what it expects, and what it returns — written for someone who wants
to run it somewhere it was not meant to run.

The `notes/` directory is the record of how each of these was established, including the
attempts that were wrong; this file is the answer without the archaeology. Where the two
disagree, **this file is current** and the note is dated.

Everything here was recovered from a shipped `nvngx_dlssnr.dll`, build 310.8.0.0, by
reading its container and its embedded code — and cross-checked against
[MLX-DLSS](https://github.com/iamwavecut/MLX-DLSS), an independent extraction of the same
binary from vendor captures. The two agree on all 649 tensors with 0 missing, 0 extra and
0 shape mismatches, which is the strongest evidence available without NVIDIA hardware.

**No weights are here.** This is the shape of the thing, not the thing.

---

## 1. What it is

A **one-step pixel-space diffusion model** that re-renders a frame's detail, conditioned
on the rendered frame, carried temporal state, and three artistic-direction scalars. It is
not a denoiser and not an upscaler: input and output are the same extent, and what it
returns is a *residual* to add to the frame it was given.

Internally: a symmetric U-Net of **71 blocks** — five encoder and five decoder stages of
shifted-window (Swin) attention, an eight-block ViT-1D bottleneck, and an upsample between
them. **145 755 123 parameters**, the large matrices stored as FP8. Vendor codenames: feature `CG2R`,
engine `HNet`, configs `crazy-cuckoo` and `hnet-vigilant-squid`.

## 2. The contract — this is the reusable part

### Extent

The network runs on an extent that is **at least 320 and a multiple of 64** on each axis.
A frame smaller than that, or not on the grid, is mirrored outward to fit and cropped back
afterwards; the mirror is a reflection of row and column indices, not padding.

```
1024x576 -> 1024x576      already aligned
 563x317 ->  576x320      rounded up
1920x1080 -> 1920x1088
   64x48 ->  320x320      the floor
```

### Input: 16 channels, float32

| channel | contents |
| --- | --- |
| 0–2 | deterministic noise for this extent and frame index |
| 3 | constant 1 |
| 4–6 | the current frame, scaled |
| 7–9 | the **previous output**, reprojected along motion, scaled the same way |
| 10 | normalised style index |
| 11 | local tone strength |
| 12 | local structure strength |
| 13 | skin structure strength, or −1 when the automatic mask is off |
| 14 | automatic-mask structure strength, or −1 |
| 15 | unused, zero |

The colour scaling is three FP16 roundings and is **not** a single multiply:

```
scaled(x) = half(half(half(x) - 0.5) * 0.125)
```

On a first frame, channels 7–9 repeat channels 4–6 — the model is told the history is the
current frame. The noise is a function of the extent and the frame index and nothing else,
so it is worth memoising; it is a Gaussian pair built from a hash of the pixel coordinates
and the frame index, rounded to FP16.

With a per-pixel **control mask**, channels 11 and 12 become that mask's green and blue
times the corresponding strength, and 13/14 go to zero.

### Output: a 4-channel head

| channel | contents |
| --- | --- |
| 0–2 | RGB residual |
| 3 | the temporal gate, as a logit |

The composition, with every rounding point that matters:

```
predicted = clamp(colour + half(head.rgb) * 0.25, 0, 1)
alpha     = clamp(sigmoid(half(head.a)) * half(0.73974609375), 0, 1)
output    = predicted + alpha * (history - predicted)
```

`history` here is channels 7–9 recovered as `channels * 8 + 0.5`. An `intensity` control
then blends the result against the untouched frame — above 1 it extrapolates past the
model's own picture, which the vendor's panel allows up to 2.

### The controls

Four profiles, which are three scalars and nothing more:

| profile | style | tone | structure |
| --- | --- | --- | --- |
| `standard` | 0 | 1 | 1 |
| `natural` | 1/128 | 1 | 1 |
| `cinematic` | 2/128 | 1 | 1 |
| `neutral` | 0 | 0 | 0 |

They are a trade, not a quality ladder: everything the pass adds to skin texture it takes
out of speculars and colour. `notes/phase44-profile-tradeoff.md` measures the curve.

## 3. The graph

Five encoder stages at **32 / 64 / 128 / 256 / 512 channels**, head dimension **32**, so
1/2/4/8/16 heads; an eight-block **ViT-1D** bottleneck at C = 1024; `dec_input_upsample
1024 -> 512`; five decoder stages back. Attention is shifted-window over **8x8 windows, 64
tokens**.

One transformer layer at C = 256 (8 heads), exactly as the extraction gives it:

| tensor | shape | |
| --- | --- | --- |
| `qkv_weight` | (C, 3C) | Q, K and V, each C x C — **full multi-head attention** |
| `projection_weight` | (C, C) | |
| `attn_bias` | (heads, 64, 64) | a relative bias over the 8x8 window, `128C` in total |
| `attn_scale` | (heads) | **FP32**, per head |
| `attn_cos_skip`, `ffn_cos_skip` | (C) | the cosine gates on the skip, exactly `C` long |
| `ffn_expand_weight` | (heads, 4, 8, 32, 32) | grouped expansion |
| `ffn_branch_projection_weight` | (heads, 4, 32, 32) | |
| `ffn_output_projection_weight` | (C, C) | |

**There is no grouped-query attention.** An early reading of the container sizes suggested
4:1 and a `1.5C²` QKV; the logical `qkv` is `(C, 3C)`, and the apparent halving was the
storage format, not the attention (section 4). This file said otherwise when it was first
published, repeating a claim the project's own notes had already withdrawn.

The non-linearity in attention is a **softmax**, hand-rolled in `f16x2` with hard logit
clamps and **no max subtraction**.

`notes/MODEL-SPEC.txt` tabulates the **container** — per-block element counts and layout as
stored. Those counts are storage, not parameters: their total, 73 841 889, is the weight
section's size divided by two, and the model has **145 755 123** (`notes/phase61`). The logical shape list is MLX-DLSS's `weight_spec.json`.

## 4. The weights

**145 755 123 parameters, in three storage formats.** The large matrices — 143.0 M
parameters — are **FP8 E4M3, one byte each**, in NVIDIA's QMMA tile layout. The small
tensors — attention biases, branch projections, cosine gates, 2.7 M — are FP16. The 714
`attn_scale` values are FP32. At those widths the model fits the DLL's 147.7 MB weight
section to within half a percent; stored densely as FP16 it would need 291.5 MB.

So there **is** a decode step: the extraction turns the E4M3 bytes into float and writes FP16
tensors, and those are what this implementation computes with. Read the container bytes as
dense FP16 instead and the values correlate **−0.02** with the truth — and, because its
headers say `data_len == 2 * n_elem`, the parameter count comes out at 73.8 M, half the real
one. This project made both mistakes, and published the second.

### Subnormals: a real hardware trap that these weights do not trigger

Intel's XMX units flush subnormal FP16 operands to zero. That is real, and a per-tensor
`2^k` rescale guards against it exactly. But **the real weights hold 7 subnormal values in
145.8 M**: E4M3's smallest non-zero magnitude sits far above FP16's normal threshold, so
decoded FP8 cannot land there. An earlier figure of 27 % was measured on the misread
container bytes and says nothing about the model. If your weights arrive in another format,
count before assuming either way.

## 5. Numerics, and what agreement is possible

**NVIDIA accumulates in FP16.** Zero of 218 PTX kernels use an FP32 accumulator. An
FP32-accumulate implementation is therefore 400–800x *more* accurate than the original on
an isolated GEMM — it is not a reproduction of it. Which you want depends on whether you
are matching their output or making a good picture.

**Per-element agreement is not a property a port can have.** The graph is chaotic: a
relative 1e-06 perturbation of the input moves the head as much as an FP16 GEMM does,
because roughly 100 E4M3 publishes, each with a 6.25 % quantum, stand between input and
output. NumPy's own float32 GEMM carries more error than the threshold below which
perturbations vanish. Judge on the composed image and on whether the controls behave;
`notes/phase9-numerics.md` has the measurements.

## 6. The temporal path

The previous output, reprojected along motion vectors into channels 7–9, blended back
through the head's fourth channel. The gate is learned and it discriminates:

| history given | gate |
| --- | --- |
| none | 0.008 |
| correct, reprojected | **0.705** |
| wrong — zero motion on a panning scene | **0.032** |

That last row is ghosting rejection, and it is why the vendor's output is temporally
stable when a single frame through the same network is not. Static-scene flicker falls
3.6x by the fourth frame.

**The gate is not local.** On a frame where most of the picture moves it reads about 0.12
even over pixels that did not move at all — the network is global, so a history that
disagrees over most of the frame is distrusted everywhere. `notes/phase54-flicker-fix.md`
measures this and what to do about it when you have no motion vectors.

## 7. What was not recovered

- **Which named parameter occupies which slice of each packed blob.** The totals are
  pinned and the names are known; the internal assignment is not. It does not matter if
  you use a logical extraction, which is why this stopped being urgent.
- **Which of `crazy-cuckoo` / `hnet-vigilant-squid` this blob is**, and whether both
  configurations ship.
- **The window shift offset.** Shifted windows are confirmed by `_shifted` kernel names
  and the window is 8x8; the shift itself is inferred, not read.

## 8. Where the evidence is

| question | note |
| --- | --- |
| the per-block table | `notes/MODEL-SPEC.txt` |
| widths, stages, kernel inventory | `notes/phase3-architecture.md` |
| kernel → layer class → template config | `notes/ptx-kernel-configs.md` |
| the container format | `notes/phase3-weight-format.md` |
| the subnormal flush | `notes/phase4-subnormal-flush.md` |
| the accumulator choice | `notes/phase4-accumulation-choice.md` |
| softmax and `attn_scale` | `notes/phase5-softmax-found.md`, `phase5-attn-scale-fp32.md` |
| the attention bias region | `notes/phase3-bias-region.md` |
| the 16 input channels | `notes/phase48-feature-inputs.md` |
| the controls | `notes/phase30-control-atlas.md` |
| the temporal path | `notes/phase12-temporal.md`, `phase54-flicker-fix.md` |
| why bit-exactness is impossible | `notes/phase9-numerics.md` |

`notes/INDEX.md` maps all of them.
