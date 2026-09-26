"""The C frame library (`work/libnr_frame.so`, `.dylib` on macOS, from `src/ref/nr_frame.c`) for NumPy callers.

`nr_frame.py` is the reference; this is the same frame in C, reached through ctypes so
the two can be run side by side. Images are float32 (height, width, 3) in [0, 1].

    frame = NativeFrame(weights_path)                   # or NativeFrame(): the weights compiled in
    output = frame.update(colour)                       # (h, w, 3)
    output, head = frame.update(colour, want_head=True) # head (h, w, 4)
    features = frame.features(colour)                   # (H, W, 16) at the network extent
    head = frame.run_features(features)                 # (H, W, 4)
"""
import ctypes as C
import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src"))
import nr_build  # noqa: E402
LIBRARY = nr_build.library("nr_frame")


class Params(C.Structure):
    _fields_ = [("intensity", C.c_float), ("detail_strength", C.c_float),
                ("colour_strength", C.c_float), ("detail_radius", C.c_float),
                ("normalized_style", C.c_float), ("local_tone", C.c_float),
                ("local_structure", C.c_float), ("frame_index", C.c_int),
                ("history_confidence", C.c_float), ("blend_scale", C.c_float),
                ("hold", C.c_float), ("slope", C.c_float), ("release", C.c_float),
                ("automatic_mask", C.c_int), ("skin_structure", C.c_float),
                ("automatic_structure", C.c_float), ("min_extent", C.c_int)]


_lib = None


def library():
    global _lib
    if _lib is None:
        if sys.platform == "darwin":
            os.environ.setdefault("MVK_CONFIG_LOG_LEVEL", "1")
            os.environ.setdefault("MVK_CONFIG_FAST_MATH_ENABLED", "0")
        lib = C.CDLL(str(LIBRARY))
        lib.nr_frame_open.argtypes = [C.c_char_p]
        lib.nr_frame_open.restype = C.c_void_p
        lib.nr_frame_close.argtypes = [C.c_void_p]
        lib.nr_frame_close.restype = None
        lib.nr_frame_error.restype = C.c_char_p
        lib.nr_frame_embedded_weights_size.argtypes = []
        lib.nr_frame_embedded_weights_size.restype = C.c_size_t
        lib.nr_frame_device.argtypes = [C.c_void_p]
        lib.nr_frame_device.restype = C.c_char_p
        lib.nr_frame_gemm_path.argtypes = [C.c_void_p]
        lib.nr_frame_gemm_path.restype = C.c_char_p
        lib.nr_frame_defaults.argtypes = [C.POINTER(Params)]
        lib.nr_frame_defaults.restype = None
        lib.nr_frame_geometry.argtypes = [C.c_int, C.c_int, C.POINTER(C.c_int), C.POINTER(C.c_int)]
        lib.nr_frame_geometry.restype = None
        lib.nr_frame_geometry_min.argtypes = [C.c_int, C.c_int, C.c_int, C.POINTER(C.c_int),
                                              C.POINTER(C.c_int)]
        lib.nr_frame_geometry_min.restype = None
        lib.nr_frame_compose_encode.argtypes = [
            C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
            C.c_void_p, C.c_void_p, C.POINTER(Params), C.c_void_p, C.c_void_p, C.c_int, C.c_int,
            C.c_int, C.c_int]
        lib.nr_frame_compose_encode.restype = C.c_int
        lib.nr_frame_update.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
                                        C.c_void_p, C.POINTER(Params), C.c_void_p, C.c_void_p]
        lib.nr_frame_update.restype = C.c_int
        lib.nr_frame_features.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
                                          C.POINTER(Params), C.c_void_p]
        lib.nr_frame_features.restype = C.c_int
        lib.nr_frame_run_features.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p]
        lib.nr_frame_run_features.restype = C.c_int
        lib.nr_frame_update_masked.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
                                               C.c_void_p, C.c_void_p, C.POINTER(Params), C.c_void_p,
                                               C.c_void_p]
        lib.nr_frame_update_masked.restype = C.c_int
        lib.nr_frame_features_masked.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
                                                 C.c_void_p, C.POINTER(Params), C.c_void_p]
        lib.nr_frame_features_masked.restype = C.c_int
        lib.nr_frame_compose.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_void_p,
                                         C.c_void_p, C.c_void_p, C.POINTER(Params), C.c_void_p]
        lib.nr_frame_compose.restype = C.c_int
        lib.nr_frame_split.argtypes = [C.c_void_p, C.c_int]
        lib.nr_frame_split.restype = C.c_double
        _lib = lib
    return _lib


def geometry(height, width, min_extent=320):
    """The network extent for an output extent, at the floor `min_extent`."""
    h, w = C.c_int(), C.c_int()
    library().nr_frame_geometry_min(height, width, min_extent, C.byref(h), C.byref(w))
    return h.value, w.value


def params(**values):
    p = Params()
    library().nr_frame_defaults(C.byref(p))
    for name, value in values.items():
        if not hasattr(p, name):
            raise TypeError(f"no such parameter: {name}")
        setattr(p, name, value)
    return p


def _image(array, name, channels):
    array = np.ascontiguousarray(array, dtype=np.float32)
    if array.ndim != 3 or array.shape[2] != channels:
        raise ValueError(f"{name} must be (height, width, {channels})")
    return array


def embedded_weights_size():
    """Bytes of logical safetensors compiled into the library (CMake, bin2c), or 0."""
    return library().nr_frame_embedded_weights_size()


class NativeFrame:
    def __init__(self, weights_path=None):
        """`weights_path` None reads the weights compiled into the library, and fails
        with the library's message in a build that has none."""
        self.lib = library()
        path = None if weights_path is None else str(weights_path).encode()
        self.handle = self.lib.nr_frame_open(path)
        if not self.handle:
            raise RuntimeError("nr_frame_open: " + self.lib.nr_frame_error().decode())

    def close(self):
        if self.handle:
            self.lib.nr_frame_close(self.handle)
            self.handle = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    @property
    def device(self):
        return self.lib.nr_frame_device(self.handle).decode()

    @property
    def gemm_path(self):
        return self.lib.nr_frame_gemm_path(self.handle).decode()

    @property
    def split(self):
        """Seconds of the last run: input write, graph, head read."""
        return tuple(self.lib.nr_frame_split(self.handle, i) for i in range(3))

    def _fail(self, call):
        raise RuntimeError(f"{call}: {self.lib.nr_frame_error().decode()}")

    @staticmethod
    def _optional(image, colour, name):
        if image is None:
            return None, None
        image = _image(image, name, 3)
        if image.shape != colour.shape:
            raise ValueError(f"{name} must match the colour image shape")
        return image, image.ctypes.data

    def update(self, colour, history=None, previous=None, control_mask=None, want_head=False,
               **values):
        colour = _image(colour, "colour", 3)
        height, width = colour.shape[:2]
        history, hp = self._optional(history, colour, "history")
        previous, pp = self._optional(previous, colour, "previous")
        control_mask, mp = self._optional(control_mask, colour, "control_mask")
        output = np.empty((height, width, 3), np.float32)
        head = np.empty((height, width, 4), np.float32) if want_head else None
        p = params(**values)
        if self.lib.nr_frame_update_masked(
                self.handle, colour.ctypes.data, height, width, hp, pp, mp,
                C.byref(p), output.ctypes.data,
                head.ctypes.data if head is not None else None):
            self._fail("nr_frame_update_masked")
        return (output, head) if want_head else output

    def features(self, colour, history=None, control_mask=None, **values):
        colour = _image(colour, "colour", 3)
        height, width = colour.shape[:2]
        history, hp = self._optional(history, colour, "history")
        control_mask, mp = self._optional(control_mask, colour, "control_mask")
        p = params(**values)
        H, W = geometry(height, width, p.min_extent)
        out = np.empty((H, W, 16), np.float32)
        if self.lib.nr_frame_features_masked(self.handle, colour.ctypes.data, height, width, hp, mp,
                                             C.byref(p), out.ctypes.data):
            self._fail("nr_frame_features_masked")
        return out

    def compose(self, head, colour, history=None, previous=None, control_mask=None, **values):
        """The composition of a cropped (h, w, 4) head, at any intensity, without the network."""
        colour = _image(colour, "colour", 3)
        head = _image(head, "head", 4)
        height, width = colour.shape[:2]
        if head.shape[:2] != colour.shape[:2]:
            raise ValueError("head and colour must share height and width")
        history, hp = self._optional(history, colour, "history")
        previous, pp = self._optional(previous, colour, "previous")
        control_mask, mp = self._optional(control_mask, colour, "control_mask")
        output = np.empty((height, width, 3), np.float32)
        p = params(**values)
        if self.lib.nr_frame_compose(self.handle, head.ctypes.data, colour.ctypes.data, height, width,
                                     hp, pp, mp, C.byref(p), output.ctypes.data):
            self._fail("nr_frame_compose")
        return output

    def compose_encode(self, head, colour, encoded, top=0, left=0, bgra=True, history=None,
                       previous=None, control_mask=None, **values):
        """`nr_frame.compose_encode` in C: the (hh, hw, 4) head brought up to the colour's
        extent, composed and encoded into `encoded` (a writable (H, W, 4) uint8 copy of the
        request) at (top, left). Returns the composition."""
        colour = _image(colour, "colour", 3)
        head = _image(head, "head", 4)
        height, width = colour.shape[:2]
        if (not isinstance(encoded, np.ndarray) or encoded.dtype != np.uint8 or encoded.ndim != 3
                or encoded.shape[2] != 4 or not encoded.flags.c_contiguous):
            raise ValueError("encoded must be a C-contiguous (H, W, 4) uint8 array")
        history, hp = self._optional(history, colour, "history")
        previous, pp = self._optional(previous, colour, "previous")
        control_mask, mp = self._optional(control_mask, colour, "control_mask")
        output = np.empty((height, width, 3), np.float32)
        p = params(**values)
        if self.lib.nr_frame_compose_encode(self.handle, head.ctypes.data, head.shape[0], head.shape[1],
                                            colour.ctypes.data, height, width, hp, pp, mp, C.byref(p),
                                            output.ctypes.data, encoded.ctypes.data, encoded.shape[1],
                                            top, left, int(bool(bgra))):
            self._fail("nr_frame_compose_encode")
        return output

    def run_features(self, features):
        features = _image(features, "features", 16)
        H, W = features.shape[:2]
        head = np.empty((H, W, 4), np.float32)
        if self.lib.nr_frame_run_features(self.handle, features.ctypes.data, H, W, head.ctypes.data):
            self._fail("nr_frame_run_features")
        return head
