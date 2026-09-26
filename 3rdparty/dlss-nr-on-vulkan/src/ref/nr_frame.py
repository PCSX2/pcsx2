#!/usr/bin/env python3
"""
nr_frame — one RGB frame in, one RGB frame out, through the recovered graph.

The network itself is `nr_model`.  The surrounding contract (the 16-channel
feature assembly with its deterministic noise, and the head-to-RGB composition)
lives in MLX-DLSS's pure-numpy `features.py` / `composition.py`, which are loaded
by path from the clone in `work/` — they need no torch.  Apache-2.0, see
work/mlx-dlss/LICENSE.

    python3 src/ref/nr_frame.py IN.png OUT.png [--scale 1] [--profile standard]
"""
from __future__ import annotations

import argparse
import importlib.util
import pathlib
import sys
import time
import types

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))

import nr_model  # noqa: E402
import image_io  # noqa: E402

WEIGHTS = ROOT / "work" / "mlxw" / "dlssnr-logical.safetensors"
_MLX = ROOT / "work" / "mlx-dlss" / "python" / "mlxdlss"


# The pure-numpy halves of MLX-DLSS. `temporal` and `motion_quality` import cv2 and
# PIL lazily — only for optical flow and for a processing scale other than 1 — so at
# scale 1 with engine motion they need neither.
MLX_NUMPY_MODULES = ("features", "composition", "motion_quality", "temporal")


def load_mlx_numpy_modules(names=MLX_NUMPY_MODULES):
    """Import MLX-DLSS's numpy modules without its torch-laden `__init__`."""
    if not _MLX.is_dir():
        raise SystemExit(
            f"missing {_MLX}\n"
            "  git clone --depth 1 https://github.com/iamwavecut/MLX-DLSS work/mlx-dlss")
    if "mlxnp" not in sys.modules:
        package = types.ModuleType("mlxnp")
        package.__path__ = [str(_MLX)]
        sys.modules["mlxnp"] = package
    loaded = []
    for name in names:
        key = f"mlxnp.{name}"
        module = sys.modules.get(key)
        if module is None:
            path = _MLX / f"{name}.py"
            spec = importlib.util.spec_from_file_location(key, path)
            module = importlib.util.module_from_spec(spec)
            sys.modules[key] = module
            if sys.version_info < (3, 10):
                # `motion_quality.py` writes `np.ndarray | None` in a dataclass body, which
                # Python evaluates when the class is built and 3.9 cannot (`|` on types
                # arrived in 3.10). Compiling the source under postponed evaluation turns
                # every annotation into a string and changes nothing else about the
                # module. macOS ships 3.9; the temporal path should not need a second
                # interpreter for one line of type syntax.
                import __future__
                code = compile(path.read_text(), str(path), "exec",
                               flags=__future__.annotations.compiler_flag, dont_inherit=True)
                exec(code, module.__dict__)
            else:
                spec.loader.exec_module(module)
        loaded.append(module)
    return loaded


features_mod, composition_mod = load_mlx_numpy_modules(("features", "composition"))
NetworkGeometry = features_mod.NetworkGeometry

# The network's frame is padded, by mirroring, to at least this on a side and to a multiple
# of 64. 320 is what NVIDIA's own driver does (`NetworkGeometry.vendor_aligned`); the graph
# itself runs down to 128 — a window of 8 at a sixteenth of the extent, MLX-DLSS's graph
# contract. At a small live size most of a 320x320 frame is mirror padding: 44 % of it for a
# 320x180 frame, 82 % for 179x101.
VENDOR_MINIMUM_EXTENT = 320
GRAPH_MINIMUM_EXTENT = 128


def network_geometry(width, height, minimum=VENDOR_MINIMUM_EXTENT):
    """The network extent for a `width` x `height` frame: at least `minimum` a side (never
    below the graph's 128), rounded up to 64. At 320 it is `NetworkGeometry.vendor_aligned`."""
    floor = max(GRAPH_MINIMUM_EXTENT, int(minimum))

    def aligned(extent):
        return -(-max(floor, extent) // 64) * 64
    return NetworkGeometry(width, height, aligned(width), aligned(height))
# The three noise channels depend on the extent and the frame index and on nothing else,
# and both callers copy the result into a slice rather than writing through it. In a live
# mode the frame index does not move — the daemon never sets one — so the same array was
# being rebuilt from four transcendentals per pixel on every frame, at 29-42 % of the
# whole feature assembly (notes/phase48). Memoised here rather than in `work/mlx-dlss`,
# which is a vendored dependency; the result is marked read-only so a future in-place
# user fails loudly instead of corrupting every later frame.
_raw_noise = features_mod.deterministic_noise
_noise_cache: dict = {}


def _cached_noise(height, width, frame_index=0):
    key = (int(height), int(width), int(frame_index))
    value = _noise_cache.get(key)
    if value is None:
        if len(_noise_cache) >= 8:          # extents change rarely; a swapchain resize
            _noise_cache.clear()            # or a scale change should not grow this
        value = _raw_noise(height, width, frame_index)
        value.flags.writeable = False
        _noise_cache[key] = value
    return value


features_mod.deterministic_noise = _cached_noise
deterministic_noise = _cached_noise
make_features = features_mod.make_features
PROFILES = features_mod.PROFILES
compose_head = composition_mod.compose_head
compose_detail = composition_mod.compose_detail
AutomaticMask = features_mod.AutomaticMask
half = features_mod.half
scaled_color = features_mod.scaled_color

try:
    import nr_image                      # the same passes in C, when they are built
except ImportError:                      # pragma: no cover - the fallback is the point
    nr_image = None

# half(blend_scale) of the recovered package, `notes/phase12-temporal.md`.
BLEND_SCALE = 0.73974609375


def build_features(colour, *, geometry, history=None, frame_index=0,
                   normalized_style=0.0, local_tone_strength=1.0,
                   local_structure_strength=1.0, out=None):
    """`make_features` with the history folded in, natively where that is available.

    The two steps are one pass in C: the mirror onto the network extent, the three FP16
    roundings of `scaled_color` and the history that replaces channels 7-9 all happen
    while each pixel is in a register. In NumPy they are a full-frame gather each, and
    feature assembly was the largest single host pass in the frame (`notes/phase57`).

    Only the plain recipe goes native — no control mask and no automatic mask — which is
    the one a game uses; anything else falls back and is bit-identical either way.

    `out`, when given, is where the features are written — the graph's own mapped input
    (`ResidentBackend.input_view`), float32 or half — and is returned.
    """
    values = dict(normalized_style=normalized_style,
                  local_tone_strength=local_tone_strength,
                  local_structure_strength=local_structure_strength)
    native = None
    if nr_image is not None:
        controls = np.array([half(normalized_style), half(local_tone_strength),
                             half(local_structure_strength), -1.0, -1.0], np.float32)
        native = nr_image.features(
            colour, geometry.source_rows(), geometry.source_columns(),
            deterministic_noise(geometry.network_height, geometry.network_width,
                                frame_index),
            controls, history=history, out=out)
    if native is not None:
        return native
    features = make_features(colour, geometry=geometry, frame_index=frame_index, **values)
    if history is not None:
        apply_history(features, history, geometry)
    if out is not None:
        # NumPy's half rounding is the GPU's to_half, to the bit (test_input_fp16.py)
        np.copyto(out, features, casting="unsafe")
        return out
    return features


def apply_history(features, history, geometry=None):
    """Put a previous output into feature channels 7-9, reprojected by the identity.

    MLX-DLSS's `make_temporal_features` samples the history along engine motion
    vectors. A Vulkan layer at `vkQueuePresentKHR` has no motion vectors — it sees a
    finished frame and nothing that produced it — so the reprojection is the identity,
    which `sample_history` computes bit-exactly at pixel centres (checked: max |delta|
    is 0 over a random image, so the five-tap collapses to its middle tap).

    That is the correct history wherever the scene stood still, and the *wrong* history
    wherever it moved. Rejecting the wrong one is the learned gate's job, and it does
    discriminate: 0.705 with correct history against 0.032 with wrong motion
    (`notes/phase12-temporal.md`). The first-frame layout already writes the current
    colour into these channels, so this is a drop-in replacement at the same scaling.
    """
    history = np.asarray(history, dtype=np.float32)
    if history.ndim != 3 or history.shape[2] != 3:
        raise ValueError("history must be (height, width, 3)")
    if geometry is not None and not geometry.is_identity:
        history = history[geometry.source_rows()[:, None],
                          geometry.source_columns()[None, :], :]
    if history.shape[:2] != features.shape[:2]:
        raise ValueError("history must match the feature extent")
    features[..., 7:10] = scaled_color(history)
    return features


def history_weight(head, *, blend_scale=BLEND_SCALE):
    """The per-pixel history weight the model asked for, from head channel 4.

    The logit is rounded to half before anything else, so the gate has 65536 possible
    inputs. NumPy's own expression — `gate_formula`, below — is evaluated once on every one
    of them, and each frame indexes that table: the same expression on the same values, so
    bit-identical, where it used to run an exp, a reciprocal and a clip on every pixel of
    the output. 3.1 ms of a 1280x720 frame. `test_nr_model.py` checks the table against the
    formula on all 65536 inputs.
    """
    bits = np.asarray(head, dtype=np.float32)[..., 3:4].astype(np.float16).view(np.uint16)
    return gate_table(float(blend_scale))[bits]


def gate_formula(logit, blend_scale=BLEND_SCALE):
    """The gate as the model defines it, on a logit already rounded to half."""
    return np.clip(1.0 / (1.0 + np.exp(-logit)) * half(blend_scale), 0, 1)


HOLD_RAMP = np.float32(4.0)


def release_slope(levels):
    """The folded constant of the release: the gate's share falls from all of it at no
    change to none by `levels` of 255. 0 turns it off."""
    return float(np.float32(-255.0 / levels)) if levels > 0 else 0.0


def release_factor(current, previous, slope):
    """How much of the model's gate a pixel keeps, from what the game did to it: the
    largest step of its three channels, `clip(1 + moved * slope, 0, 1)` in the order the
    native composition computes it."""
    moved = np.abs(np.subtract(current[..., 0], previous[..., 0], dtype=np.float32))
    scratch = np.empty_like(moved)
    for channel in (1, 2):
        np.subtract(current[..., channel], previous[..., channel], out=scratch)
        np.maximum(moved, np.abs(scratch, out=scratch), out=moved)
    np.multiply(moved, np.float32(slope), out=moved)
    np.add(moved, np.float32(1.0), out=moved)
    return np.clip(moved, 0, 1, out=moved)[..., None]


def hold_floor(current, previous, strength):
    """Per-pixel lower bound on the history weight, from what the *game* did: full where
    the game handed back the same pixel, gone by `HOLD_RAMP` levels of 255 (notes/phase54).

    Channel by channel and in place — the obvious `max(abs(a - b), axis=2)` builds two
    temporaries and runs seven full-frame passes at the output resolution — and folded
    into one multiply-add-clip, which is the order the native composition transcribes.
    """
    floor = np.abs(np.subtract(current[..., 0], previous[..., 0], dtype=np.float32))
    scratch = np.empty_like(floor)
    for channel in (1, 2):
        np.subtract(current[..., channel], previous[..., channel], out=scratch)
        np.maximum(floor, np.abs(scratch, out=scratch), out=floor)
    # clip(1 - moved * 255 / ramp, 0, 1) * strength, folded into one multiply-add-clip
    np.multiply(floor, np.float32(-255.0 * strength / HOLD_RAMP), out=floor)
    np.add(floor, np.float32(strength), out=floor)
    return np.clip(floor, 0, strength, out=floor)[..., None]


_GATE_TABLES = {}


def gate_table(blend_scale):
    """`gate_formula` on every half value, indexed by the value's sixteen bits."""
    table = _GATE_TABLES.get(blend_scale)
    if table is None:
        every = np.arange(1 << 16, dtype=np.uint32).astype(np.uint16).view(np.float16)
        with np.errstate(over="ignore", invalid="ignore"):
            table = gate_formula(every.astype(np.float32), blend_scale).astype(np.float32)
        _GATE_TABLES[blend_scale] = table
    return table


# What the vendor's own panel starts at, which is not what MLX-DLSS's profiles use:
# structure 1.5 rather than 1.0, skin structure 2.0, and the automatic mask on.
# Recovered from the shipped control surface, not from the DLL —
# `notes/phase30-control-atlas.md`.
VENDOR_DEFAULTS = {"normalized_style": 0.0, "local_tone_strength": 1.0,
                   "local_structure_strength": 1.5}
VENDOR_AUTOMATIC_MASK = (2.0, -1.0)     # skin structure, automatic-mask structure


def controls(profile="standard", style_index=None, local_tone=None,
             local_structure=None):
    """The three conditioning scalars, a profile plus any explicit overrides.

    `style_index` is the vendor's integer style, normalised by 1/128; the profiles
    are style 0 / 1 / 2 with tone and structure at 1, and `neutral` is style 0 with
    both at 0. `vendor` is what the shipped panel starts at.
    """
    values = dict(VENDOR_DEFAULTS if profile == "vendor" else PROFILES[profile])
    if style_index is not None:
        values["normalized_style"] = style_index / 128.0
    if local_tone is not None:
        values["local_tone_strength"] = local_tone
    if local_structure is not None:
        values["local_structure_strength"] = local_structure
    return values


class ResidentBackend:
    """Runs the graph on the GPU, keeping activations in device buffers.

    Retains the most recently used extents, one by default. Evicted frames are
    closed, releasing their GPU allocations and captured commands. A caller must
    not keep using a frame after asking this backend for a different extent.
    """

    def __init__(self, weights_path=None, *, max_cached_frames=1):
        if not isinstance(max_cached_frames, int) or max_cached_frames < 1:
            raise ValueError("max_cached_frames must be a positive integer")
        sys.path.insert(0, str(ROOT / "src" / "gpu"))
        import xmxres
        import nr_frame_resident
        self._module = nr_frame_resident
        self.runtime = xmxres.Runtime()
        self.weights, _ = nr_model.load_logical(weights_path or WEIGHTS)
        # one upload for as long as the backend is open: the extent changes under a knob
        # and the weights do not depend on it. Built on the first frame, not here, so that
        # constructing a backend still allocates nothing on the device.
        self.device_weights = None
        nr_model.FUSE_BRANCHED = True
        self._frames = {}
        self.split = (0.0, 0.0, 0.0)
        self.max_cached_frames = max_cached_frames

    def frame(self, height, width):
        key = (height, width)
        if key in self._frames:
            frame = self._frames.pop(key)
        else:
            while len(self._frames) >= self.max_cached_frames:
                self._frames.pop(next(iter(self._frames))).close()
            if self.device_weights is None:
                self.device_weights = self._module.DeviceWeights(self.runtime, self.weights)
            frame = self._module.ResidentFrame(self.runtime, self.device_weights,
                                               height, width)
        self._frames[key] = frame
        return frame

    def close(self):
        for frame in self._frames.values():
            frame.close()
        self._frames.clear()
        # closing means everything, including the weights; the next frame uploads them
        # again, which is what this cost before they were shared
        if self.device_weights is not None:
            self.device_weights.close()
            self.device_weights = None

    def input_view(self, height, width):
        """The mapped input of the frame at this extent, to build the features in: then
        `run_features` has nothing to copy."""
        return self.frame(height, width).input_view()

    def run_features(self, features):
        height, width = features.shape[:2]
        frame = self.frame(height, width)
        head = frame.run(features)
        self.split = frame.split          # host write, graph, host read
        return head


def run_head(model, color, *, profile="standard", frame_index=0, style_index=None,
             local_tone=None, local_structure=None, automatic_mask=None,
             control_mask=None, verbose=False):
    """The network alone: (H, W, 3) colour -> the cropped (H, W, 4) head.

    Everything that reaches the network is decided here — the conditioning scalars
    land in feature channels 10-14, so changing any of them costs a forward pass.
    The head can then be composed at any `intensity` for free.
    """
    height, width = color.shape[:2]
    geometry = NetworkGeometry.vendor_aligned(width, height)
    values = controls(profile, style_index, local_tone, local_structure)
    features = make_features(color, frame_index=frame_index, geometry=geometry,
                             automatic_mask=automatic_mask, control_mask=control_mask,
                             **values)
    if verbose:
        print(f"  network extent {geometry.network_width}x{geometry.network_height} "
              f"for output {width}x{height}", flush=True)

    started = time.perf_counter()
    marks = [started]

    def progress(name):
        marks.append(time.perf_counter())
        if verbose:
            print(f"    {name:<12} {marks[-1] - marks[-2]:6.2f}s "
                  f"(total {marks[-1] - started:7.2f}s)", flush=True)

    if isinstance(model, ResidentBackend):
        head = model.run_features(features)
    else:
        head = model.forward(features[None], progress=progress if verbose else None)[0]
    return geometry.crop(head), time.perf_counter() - started


def compose(head, color, *, intensity=1.0, detail_strength=1.0, colour_strength=1.0,
            detail_radius=4.0, control_mask=None, history=None,
            history_confidence=1.0, history_floor=None, history_previous=None,
            history_hold=0.0, history_release=0.0, blend_scale=BLEND_SCALE):
    """The head over the frame. Post-network and cheap: sweep it without re-running.

    With a `history` image this is MLX-DLSS's `compose_temporal` instead of its
    `compose_head`: the model's own gate decides, per pixel, how much of the previous
    output survives into this one. `history_confidence` scales that gate globally —
    0 is the still-frame path exactly, 1 is the model's own answer.

    The vendor reads the history back out of feature channels 7-9, which pins it to the
    network's extent; a live frame composes at the *output* extent, so it arrives here
    as an image instead. That skips one `scaled_color` round trip through fp16, worth
    at most 0.06 of a 0-255 level, and keeps the game's own pixels off the resampler.

    `history_floor` raises the gate per pixel rather than lowering it, which the vendor
    has no need of and we do. The gate is not local: on a frame where most things move it
    reads 0.12 even over pixels that did not move, against 0.705 on a scene where the
    history was correct everywhere (`notes/phase54-flicker-fix.md`, `phase12`). A caller
    that can prove the history is correct for a pixel — because the game handed back the
    same bytes — can say so here. The floor is still bounded by `blend_scale`, so no
    pixel is held harder than the model itself ever holds one.

    `history_release` is the other half of the same signal: where the game's pixel changed
    by `history_release` levels of 255 or more, none of the gate survives, and by less, a
    share that falls linearly with the change. Without motion vectors the history at a
    pixel something moved across is what was there before, and a gate that reads 0.6 over
    a whole Tekken frame kept a trail of it behind everything that moved. It lowers only
    the gate — never the floor, which is zero wherever the game changed a pixel anyway.

    `intensity` blends the model's picture against the source, per pixel when a
    ControlMask supplies its red channel. `detail_strength` and `colour_strength`
    then re-weight the high and low frequencies of whatever change remains.

    Above 1 the blend extrapolates past the model's own picture. MLX-DLSS's
    `compose_head` clamps it, so anything over 1 was silently a no-op here; the
    vendor's panel goes to 2 and ships screenshots at 1.66
    (`notes/phase30-control-atlas.md`). At or below 1 this is bit-identical to the
    clamped path — the extrapolation only replaces the final blend, and the residual,
    its half rounding and the [0,1] clamp on the result are unchanged.
    """
    composed = None
    if history is not None or intensity > 1.0:
        head = np.asarray(head, dtype=np.float32)
        color = np.asarray(color, dtype=np.float32)
        alpha = None
        if history is not None:
            if head.shape[2] < 4:
                raise ValueError("the temporal gate is head channel 4; pass all four")
            history = np.asarray(history, dtype=np.float32)
            if history.shape != color.shape:
                raise ValueError("history must match the colour image shape")
            previous = (history_previous if history_previous is not None
                        and (history_hold > 0 or history_release > 0) else None)
            slope_release = release_slope(history_release)
            if nr_image is not None and history_floor is None:
                # Everything temporal in the one native pass: the gate from its table (the
                # exp that kept it in NumPy is inside the table), the confidence, and the
                # floor from the game's previous frame in the same folded multiply-add-clip
                # as `hold_floor`. 7 ms of NumPy at a 1280x720 output.
                confidence = (float(np.clip(np.float32(history_confidence), 0, 1))
                              if history_confidence != 1.0 else 1.0)
                composed = nr_image.compose_temporal(
                    head, color, history, previous, None, control_mask,
                    intensity=intensity, blend_scale=blend_scale,
                    hold=float(history_hold) if previous is not None else 0.0,
                    slope=(float(np.float32(-255.0 * history_hold / HOLD_RAMP))
                           if previous is not None else 0.0),
                    table=gate_table(float(blend_scale)), confidence=confidence,
                    release=slope_release if previous is not None else 0.0)
            if previous is not None and history_floor is None and composed is None:
                history_floor = hold_floor(color, previous, history_hold)
            alpha = (history_weight(head, blend_scale=blend_scale)
                     if composed is None else None)
            if composed is None and history_confidence != 1.0:
                alpha = alpha * np.clip(np.float32(history_confidence), 0, 1)
            if composed is None and previous is not None and slope_release != 0.0:
                alpha = alpha * release_factor(color, previous, slope_release)
            if composed is None and history_floor is not None:
                floor = np.clip(np.asarray(history_floor, dtype=np.float32), 0, 1)
                np.maximum(alpha, floor * np.float32(blend_scale), out=alpha)
            if composed is None and nr_image is not None:
                composed = nr_image.compose_temporal(
                    head, color, history, None, alpha, control_mask,
                    intensity=intensity, blend_scale=blend_scale, hold=0.0, slope=0.0)
        elif nr_image is not None and control_mask is None:
            composed = nr_image.compose(head, color, intensity)
        if composed is None:
            residual = features_mod.half(head[..., :3]) * np.float32(0.25)
            predicted = np.clip(color + residual, 0, 1)
            if history is not None:
                predicted += alpha * (history - predicted)
            blend = np.float32(intensity)
            if control_mask is not None:
                blend = np.asarray(control_mask, dtype=np.float32)[..., :1] * blend
            composed = np.clip(color + blend * (predicted - color), 0, 1).astype(np.float32)
    else:
        # The still frame natively too: nr_compose clamps the blend to [0, 1] below 1 as
        # compose_head does, and is byte-identical to it (test_native_image.py). It was
        # only reached above 1, so photo mode and every cut paid 3.9 ms of NumPy at
        # 512x288 for the same bytes.
        if nr_image is not None and control_mask is None:
            composed = nr_image.compose(head, color, intensity)
        if composed is None:
            composed = compose_head(head, color, control_mask=control_mask,
                                    intensity=intensity)
    return compose_detail(color, composed, detail_strength=detail_strength,
                          colour_strength=colour_strength, radius=detail_radius)


def compose_encode(head, color, encoded, *, top=0, left=0, bgra=True, intensity=1.0,
                   control_mask=None, history=None, history_confidence=1.0,
                   history_previous=None, history_hold=0.0, history_release=0.0,
                   blend_scale=BLEND_SCALE, samples=None):
    """`compose` of the head brought up to the colour's size, encoded into `encoded` at
    (`top`, `left`), in one native pass; `None` where that pass does not apply, and then
    nothing has been written.

    The same composition as `resample` then `compose` then `nr_daemon.encode`, byte for
    byte (`test_native_image.py`), for the cases the daemon meets live: a history with or
    without a control mask, or a still frame without one, at detail and colour strength 1
    — where `compose_detail` hands the composition back untouched. The parameters are
    derived exactly as `compose`'s native branches derive them. With `samples`, a step,
    returns `(composition, head samples)` — see `nr_image.compose_encode`.
    """
    if nr_image is None or (history is None and control_mask is not None):
        return None
    if history is None:
        return nr_image.compose_encode(head, color, None, None, None, encoded, top=top,
                                       left=left, bgra=bgra, intensity=intensity,
                                       samples=samples)
    previous = (history_previous if history_previous is not None
                and (history_hold > 0 or history_release > 0) else None)
    confidence = (float(np.clip(np.float32(history_confidence), 0, 1))
                  if history_confidence != 1.0 else 1.0)
    return nr_image.compose_encode(
        head, color, history, previous, control_mask, encoded, top=top, left=left,
        bgra=bgra, intensity=intensity, blend_scale=blend_scale,
        hold=float(history_hold) if previous is not None else 0.0,
        slope=(float(np.float32(-255.0 * history_hold / HOLD_RAMP))
               if previous is not None else 0.0),
        table=gate_table(float(blend_scale)), confidence=confidence,
        release=release_slope(history_release) if previous is not None else 0.0,
        samples=samples)


def run_frame(model, color, *, intensity=1.0, detail_strength=1.0, colour_strength=1.0,
              detail_radius=4.0, control_mask=None, **head_options):
    """color: (H, W, 3) float32 in [0,1] -> (H, W, 3) float32."""
    head, elapsed = run_head(model, color, control_mask=control_mask, **head_options)
    output = compose(head, color, intensity=intensity, detail_strength=detail_strength,
                     colour_strength=colour_strength, detail_radius=detail_radius,
                     control_mask=control_mask)
    return output, head, elapsed


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--weights", default=str(WEIGHTS))
    parser.add_argument("--size", help="resize input first, as HxW")
    parser.add_argument("--profile", default="standard", choices=sorted(PROFILES),
                        help="preset for style/tone/structure; overridden by the flags below")
    parser.add_argument("--style-index", type=int,
                        help="vendor style index, normalised by 1/128 (0, 1, 2 are the presets)")
    parser.add_argument("--local-tone", type=float,
                        help="local tone strength, 0..2 (the vendor's range; default 1.0)")
    parser.add_argument("--local-structure", type=float,
                        help="local structure strength, 0..2 (vendor default 1.5)")
    parser.add_argument("--skin-structure", type=float,
                        help="skin structure strength; enables automatic masking, -1 to follow local structure")
    parser.add_argument("--auto-mask", type=float,
                        help="automatic-mask structure strength; -1 to follow local structure")
    parser.add_argument("--control-mask", help="per-pixel RGB mask: red intensity, green tone, blue structure")
    parser.add_argument("--intensity", type=float, default=1.0,
                        help="blend of the model's picture against the source, 0..2; "
                             "above 1 extrapolates past the model's own picture")
    parser.add_argument("--intensity-ladder",
                        help="comma-separated intensities; renders once and writes one file each")
    parser.add_argument("--detail-strength", type=float, default=1.0)
    parser.add_argument("--colour-strength", type=float, default=1.0)
    parser.add_argument("--detail-radius", type=float, default=4.0)
    parser.add_argument("--frame-index", type=int, default=0)
    parser.add_argument("--gpu", action="store_true",
                        help="run the weight GEMMs on the Xe2 XMX units")
    parser.add_argument("--resident", action="store_true",
                        help="run the whole graph on the GPU, activations never "
                             "returning to the host (the fastest path)")
    parser.add_argument("--accel", action="store_true",
                        help="use torch's SIMD half and E4M3 conversions (bit-identical)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if args.accel:
        import nr_accel
        if nr_accel.install():
            print("accel   torch rounding, verified bit-identical", flush=True)
        else:
            print("accel   requested but torch is not installed", flush=True)

    if args.resident:
        started = time.perf_counter()
        model = ResidentBackend(args.weights)
        print(f"resident backend ready in {time.perf_counter() - started:.1f}s", flush=True)

    backend = None
    if args.gpu and not args.resident:
        sys.path.insert(0, str(ROOT / "src" / "gpu"))
        import nr_xmx
        backend = nr_xmx
        print(f"backend {backend.install()}", flush=True)

    size = None
    if args.size:
        size = tuple(int(part) for part in args.size.lower().split("x"))
    color = image_io.load(args.input, size=size)
    print(f"input {color.shape[1]}x{color.shape[0]}", flush=True)

    if not args.resident:
        started = time.perf_counter()
        model = nr_model.NeuralRenderingModel.from_safetensors(args.weights)
        print(f"weights loaded in {time.perf_counter() - started:.1f}s "
              f"({len(model.weights)} tensors)", flush=True)

    mask = image_io.load(args.control_mask) if args.control_mask else None
    automatic = None
    if args.skin_structure is not None or args.auto_mask is not None:
        automatic = AutomaticMask(
            skin_structure_strength=(args.skin_structure
                                     if args.skin_structure is not None else -1.0),
            automatic_mask_structure_strength=(args.auto_mask
                                               if args.auto_mask is not None else -1.0))

    head, elapsed = run_head(
        model, color, profile=args.profile, frame_index=args.frame_index,
        style_index=args.style_index, local_tone=args.local_tone,
        local_structure=args.local_structure, automatic_mask=automatic,
        control_mask=mask, verbose=args.verbose)
    print(f"network {elapsed:.1f}s")
    if backend is not None:
        print(backend.report())
    print(f"head    min {head.min():+.4f} max {head.max():+.4f} sd {head.std():.4f}")

    def emit(path, intensity):
        output = compose(head, color, intensity=intensity,
                         detail_strength=args.detail_strength,
                         colour_strength=args.colour_strength,
                         detail_radius=args.detail_radius, control_mask=mask)
        residual = output - color
        print(f"intensity {intensity:5.2f}  change mean|d| {np.abs(residual).mean():.5f} "
              f"max|d| {np.abs(residual).max():.5f}")
        image_io.save(output, path)
        print(f"wrote {path}", flush=True)

    if args.intensity_ladder:
        stem = pathlib.Path(args.output)
        for value in (float(part) for part in args.intensity_ladder.split(",")):
            emit(str(stem.with_name(f"{stem.stem}_i{value:g}{stem.suffix}")), value)
    else:
        emit(args.output, args.intensity)


if __name__ == "__main__":
    main()
