TurboJPEG/JNA
=============

[TurboJPEG/JNA](TJ.java) is example code that demonstrates how to use the full
TurboJPEG API from Java programs by way of
[Java Native Access (JNA)](https://github.com/java-native-access/jna).  To
minimize maintenance, documentation, and new feature development labor, the
TurboJPEG/JNA interfaces correspond to the TurboJPEG C interfaces on a 1:1
basis.

Historically, libjpeg-turbo provided a more Java-friendly API (the TurboJPEG
Java API) based on the Java Native Interface (JNI.)  However, the Java-friendly
design of that API (specifically, the requirement that it work directly with
Java arrays rather than NIO buffers) necessitated allocating all buffers on the
Java heap to avoid buffer copies.  That necessitated using fixed-size JPEG
buffers (the equivalent of `TJPARAM_NOREALLOC`), which meant that all JPEG
buffers had to be big enough to account for the size of the ICC profile and the
possibility of zero compression.  All of that made the TurboJPEG Java API more
complicated and difficult to maintain and extend than the TurboJPEG C API,
which blocked the evolution of the latter.  Thus, the TurboJPEG Java API was
moved to a
[dedicated repository](https://github.com/libjpeg-turbo/turbojpeg-java) where
it can evolve independently based on demand.

It is certainly possible to develop a more Java-friendly API on top of
TurboJPEG/JNA.  (The TurboJPEG Java API could even be updated to use JNA "under
the hood", although avoiding buffer copies would require using NIO buffers
rather than Java arrays throughout.)  It is also possible to wrap the libjpeg
API using JNA.  However, such projects are currently out of scope for
libjpeg-turbo.  We encourage those who want fancier Java APIs to implement them
downstream.

[TJComp.java](TJComp.java), [TJDecomp.java](TJDecomp.java), and
[TJTran.java](TJTran.java), which should be located in the same directory as
this README file, demonstrate how to use TurboJPEG/JNA to compress, decompress,
and transform JPEG images in memory.  A Java archive (JAR) file containing
TurboJPEG/JNA and these example programs is also shipped with the "official"
distribution packages of libjpeg-turbo.

Note that TurboJPEG/JNA is not a formal API.  It is example code and is thus
subject to change, but the underlying TurboJPEG API library maintains backward
API and ABI compatibility.  Thus, it should be possible to continue using an
older version of TurboJPEG/JNA with a newer version of the TurboJPEG API
library.

Note also that Java Native Access is provided under either the Apache 2.0
License or the LGPL v2.1+, so applications that incorporate it must manage
those license terms in addition to libjpeg-turbo's license terms.

API Notes
---------

### Naming Conventions

TurboJPEG/JNA provides interfaces that are direct equivalents to the TurboJPEG
C interfaces, except that the `TJ` and `tj` prefixes have been removed, and
Java naming conventions are used.  For example:

- `TJSAMP_444` becomes `TJ.SAMP_444`.
- `tjMCUWidth[]` becomes `TJ.MCU_WIDTH[]`.
- `tjregion` becomes `TJ.Region`.
- `TJSCALED()` becomes `TJ.scaled()`.

The native methods retain the same `tj3` prefix as the corresponding C
functions, both for simplicity (avoiding the need for a JNA function mapper)
and to remind callers that they must treat those methods like C functions and
check their return values.

### Convenience Methods and Classes

Exception-throwing convenience methods without the `tj3` prefix are provided.
Except as indicated below, these differ from the native methods only in that
they throw a `TJ.Exception` instance if the return value from the corresponding
native method indicates an error.  This avoids the need to check return values
or to call `TJ.tj3GetErrorStr()` or `TJ.tj3GetErrorCode()`.

The `TJ.Handle` class wraps `TJ.init()` and `TJ.destroy()` to allow TurboJPEG
instance handles to be used with Java try-with-resources statements.

### Memory Management

All buffers are allocated on the C heap and accessed as `com.sun.jna.Pointer`
instances.  As with the TurboJPEG C API, any buffer that is managed by the
TurboJPEG API library should be allocated using `TJ.tj3Alloc()` (or
`TJ.alloc()`) and freed using `TJ.tj3Free()` (or `TJ.free()`.)  Other buffers
can be allocated via the `com.sun.jna.Memory` class, which can also be used
with Java try-with-resources statements.  `com.sun.jna.Pointer` provides
various methods for obtaining a Java NIO buffer or primitive array from the C
buffer. Note, however, that obtaining a primitive array will trigger a buffer
copy.

### `size_t` Arguments and Return Values

A `size_t` value is implemented as a `com.sun.jna.NativeLong` instance, which
wraps a 4-byte integer when using a 32-bit JVM (not much of a thing anymore)
and an 8-byte integer when using a 64-bit JVM.  Callers initialize a
`NativeLong` instance with a Java `long` primitive (always 64-bit), so it is
necessary for callers to guard against 32-bit overflow if `NativeLong.SIZE` is
4.  (The same is true when initializing a `size_t` variable in C.)

### JPEG Destination Buffers

JPEG destination buffer arguments are implemented as a
`com.sun.jna.ptr.PointerByReference` instance (the equivalent of `void **`) and
a `com.sun.jna.ptr.NativeLongByReference` instance (the equivalent of
`size_t *`.)  The `com.sun.jna.Pointer` instance wrapped by
`PointerByReference` is null by default, and the `NativeLong` instance wrapped
by `NativeLongByReference` is 0 by default.  Thus, you can simply do:

```
NativeLongByReference jpegSize = new NativeLongByReference();
PointerByReference jpegBuf = new PointerByReference();
```

if you want the JPEG destination buffer to be fully and automatically
allocated.  Otherwise, you can do:

```
NativeLong jpegBufSize = new NativeLong(longValue);
// or NativeLong jpegBufSize = TJ.jpegBufSize(width, height, jpegSubsamp);
NativeLongByReference jpegSize = new NativeLongByReference(jpegBufSize);
PointerByReference jpegBuf = new PointerByReference(TJ.alloc(jpegBufSize));
```

to pre-allocate the JPEG destination buffer, which will be re-allocated as
needed.  As with the TurboJPEG C API, a compression or lossless transformation
operation may change the JPEG destination buffer pointer unless
`TJ.PARAM_NOREALLOC` is set, so call `jpegBuf.getValue()` and
`jpegSize.longValue()` to obtain the buffer pointer and size after one of these
operations.

### Arrays of Structures

To represent an array of structures, JNA uses a single instance of a
`com.sun.jna.Structure` subclass that points to the head of the array.  Thus,

- The `TJ.tj3GetScalingFactors()` native method returns a `TJ.ScalingFactor`
  instance that points to the first member of a `TJ.ScalingFactor[]` array.
  To get the entire array, simply type-cast the return value to
  `TJ.ScalingFactor[]`.  The `TJ.getScalingFactors()` convenience method
  performs the type cast internally and returns a `TJ.ScalingFactor[]`
  instance.

- The `TJ.tj3Transform()` native mathod accepts a `TJ.Transform` instance that
  points to the first member of a `TJ.Transform[]` array.  To pass the entire
  array, simply pass the first member.  The `TJ.transform()` convenience
  method accepts a `TJ.Transform[]` argument and passes the first member internally.

- To pass an array of structures to a native method, it is necessary to
  allocate the array in contiguous memory.  This is accomplished with
  `Structure.toArray()`.  For example:

  ```
  TJ.Transform[] xform = (TJ.Transform[])(new TJ.Transform().toArray(2));
  ```

### Arrays of `Pointer` or `NativeLong` Instances

JNA doesn't allow for passing an array of `Pointer` or `NativeLong` instances,
which are needed by `TJ.tj3EncodeYUVPlanes8()`,
`TJ.tj3CompressFromYUVPlanes8()`, `TJ.tj3DecompressToYUVPlanes8()`,
`TJ.tj3DecodeYUVPlanes8()`, and `TJ.tj3Transform()`.  Thus, TurboJPEG/JNA
provides two `Structure` subclasses, `TJ.PointerReference` and
`TJ.NativeLongReference`, that can be passed as arrays.  Per above, use
`Structure.toArray()` to ensure that the arrays are allocated contiguously.
For example:

```
TJ.PointerReference[] dstBufs =
  (TJ.PointerReference[])(new TJ.PointersByReference().toArray(2));
dstBufs[0].pointer = jpegBuf0;
dstBufs[1].pointer = jpegBuf1;
TJ.NativeLongReference[] dstSizes =
  (TJ.NativeLongReference[])(new TJ.NativeLongsByReference().toArray(2));
dstSizes[0].value = jpegSize0;
dstSizes[1].value = jpegSize1;
```

Then you can simply pass the first members of the arrays to
`TJ.tj3EncodeYUVPlanes8()`, `TJ.tj3CompressFromYUVPlanes8()`,
`TJ.tj3DecompressToYUVPlanes8()`, `TJ.tj3DecodeYUVPlanes8()`, or
`TJ.tj3Transform()`, as you did with the `TJ.Transform[]` array.  The
`TJ.encodeYUVPlanes8()`, `TJ.compressFromYUVPlanes8()`,
`TJ.decompressToYUVPlanes8()`, `TJ.decodeYUVPlanes8()`, and `TJ.transform()`
convenience methods accept `TJ.PointerReference[]` and
`TJ.NativeLongReference[]` arguments and pass the first members internally.

### Passing Structures By Value

JNA uses "tagging interfaces" to indicate that a `com.sun.jna.Structure`
instance should be passed by value instead of by reference.  Thus,

- When using `TJ.tj3SetScalingFactor()`, you can convert a `TJ.ScalingFactor`
  instance into a `TJ.ScalingFactor.ByValue` instance as follows:

  ```
  TJ.ScalingFactor sf = new TJ.ScalingFactor(num, denom);
  int retval =
      TJ.tj3SetScalingFactor(handle, new TJ.ScalingFactor.ByValue(sf));
  if (retval < 0)
      handleError();
  ```

  The `TJ.setScalingFactor()` convenience method does that automatically.

- When using `TJ.tj3SetCroppingRegion()`, you can convert a `TJ.Region`
  instance into a `TJ.Region.ByValue` instance as follows:

  ```
  TJ.Region cr = new TJ.Region(x, y, w, h);
  int retval = TJ.tj3SetCroppingRegion(handle, new TJ.Region.ByValue(cr));
  if (retval < 0)
      handleError();
  ```

  The `TJ.setCroppingRegion()` convenience method does that automatically.

- When using `TJ.tj3TransformBufSize()`, you can convert a `TJ.Transform`
  instance into a `TJ.Transform.ByValue` instance as follows:

  ```
  TJ.Transform t = new TJ.Transform();
  ...
  NativeLong transformBufSize =
      TJ.tj3TransformBufSize(handle, new TJ.Transform.ByValue(t));
  if (transformBufSize.longValue() == 0)
      handleError();
  ```

  The `TJ.transformBufSize()` convenience method does that automatically.

### Further Reading

Refer to the JNA documentation, the TurboJPEG C API documentation, and the
included example programs for further details.
