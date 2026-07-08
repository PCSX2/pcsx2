/*
 * Copyright (C) 2026 D. R. Commander
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

import java.io.Closeable;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;

import com.sun.jna.Callback;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.Native;
import com.sun.jna.NativeLong;
import com.sun.jna.ptr.NativeLongByReference;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import com.sun.jna.Structure;
import com.sun.jna.Structure.FieldOrder;

@SuppressWarnings({ "checkstyle:JavadocVariable",
                    "checkstyle:VisibilityModifier" })

public final class TJ {
  private TJ() {}

  static {
    Native.register("turbojpeg");
  }

  // ==========================================================================
  // CONSTANTS
  // ==========================================================================

  public static final int TURBOJPEG_VERSION_NUMBER = 3002000;

  public static final int NUMINIT = 3;
  public static final int INIT_COMPRESS = 0;
  public static final int INIT_DECOMPRESS = 1;
  public static final int INIT_TRANSFORM = 2;

  public static final int NUMSAMP = 9;
  public static final int SAMP_444 = 0;
  public static final int SAMP_422 = 1;
  public static final int SAMP_420 = 2;
  public static final int SAMP_GRAY = 3;
  public static final int SAMP_440 = 4;
  public static final int SAMP_411 = 5;
  public static final int SAMP_441 = 6;
  public static final int SAMP_410 = 7;
  public static final int SAMP_24 = 8;
  public static final int SAMP_UNKNOWN = -1;

  public static final int[] MCU_WIDTH  = { 8, 16, 16, 8, 8, 32, 8, 32, 16 };
  public static final int[] MCU_HEIGHT = { 8, 8, 16, 8, 16, 8, 32, 16, 32 };

  public static final int NUMPF = 12;
  public static final int PF_RGB = 0;
  public static final int PF_BGR = 1;
  public static final int PF_RGBX = 2;
  public static final int PF_BGRX = 3;
  public static final int PF_XBGR = 4;
  public static final int PF_XRGB = 5;
  public static final int PF_GRAY = 6;
  public static final int PF_RGBA = 7;
  public static final int PF_BGRA = 8;
  public static final int PF_ABGR = 9;
  public static final int PF_ARGB = 10;
  public static final int PF_CMYK = 11;
  public static final int PF_UNKNOWN = -1;

  public static final int[] RED_OFFSET = {
    0, 2, 0, 2, 3, 1, -1, 0, 2, 3, 1, -1
  };
  public static final int[] GREEN_OFFSET = {
    1, 1, 1, 1, 2, 2, -1, 1, 1, 2, 2, -1
  };
  public static final int[] BLUE_OFFSET = {
    2, 0, 2, 0, 1, 3, -1, 2, 0, 1, 3, -1
  };
  public static final int[] ALPHA_OFFSET = {
    -1, -1, -1, -1, -1, -1, -1, 3, 3, 0, 0, -1
  };
  public static final int[] PIXEL_SIZE = {
    3, 3, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4
  };

  public static final int NUMCS = 5;
  public static final int CS_RGB = 0;
  @SuppressWarnings("checkstyle:ConstantName")
  public static final int CS_YCbCr = 1;
  public static final int CS_GRAY = 2;
  public static final int CS_CMYK = 3;
  public static final int CS_YCCK = 4;
  public static final int CS_DEFAULT = -1;

  public static final int PARAM_STOPONWARNING = 0;
  public static final int PARAM_BOTTOMUP = 1;
  public static final int PARAM_NOREALLOC = 2;
  public static final int PARAM_QUALITY = 3;
  public static final int PARAM_SUBSAMP = 4;
  public static final int PARAM_JPEGWIDTH = 5;
  public static final int PARAM_JPEGHEIGHT = 6;
  public static final int PARAM_PRECISION = 7;
  public static final int PARAM_COLORSPACE = 8;
  public static final int PARAM_FASTUPSAMPLE = 9;
  public static final int PARAM_FASTDCT = 10;
  public static final int PARAM_OPTIMIZE = 11;
  public static final int PARAM_PROGRESSIVE = 12;
  public static final int PARAM_SCANLIMIT = 13;
  public static final int PARAM_ARITHMETIC = 14;
  public static final int PARAM_LOSSLESS = 15;
  public static final int PARAM_LOSSLESSPSV = 16;
  public static final int PARAM_LOSSLESSPT = 17;
  public static final int PARAM_RESTARTBLOCKS = 18;
  public static final int PARAM_RESTARTROWS = 19;
  public static final int PARAM_XDENSITY = 20;
  public static final int PARAM_YDENSITY = 21;
  public static final int PARAM_DENSITYUNITS = 22;
  public static final int PARAM_MAXMEMORY = 23;
  public static final int PARAM_MAXPIXELS = 24;
  public static final int PARAM_SAVEMARKERS = 25;

  public static final int NUMERR = 2;
  public static final int ERR_WARNING = 0;
  public static final int ERR_FATAL = 1;

  public static final int NUMXOP = 8;
  public static final int XOP_NONE = 0;
  public static final int XOP_HFLIP = 1;
  public static final int XOP_VFLIP = 2;
  public static final int XOP_TRANSPOSE = 3;
  public static final int XOP_TRANSVERSE = 4;
  public static final int XOP_ROT90 = 5;
  public static final int XOP_ROT180 = 6;
  public static final int XOP_ROT270 = 7;

  public static final int XOPT_PERFECT = (1 << 0);
  public static final int XOPT_TRIM = (1 << 1);
  public static final int XOPT_CROP = (1 << 2);
  public static final int XOPT_GRAY = (1 << 3);
  public static final int XOPT_NOOUTPUT = (1 << 4);
  public static final int XOPT_PROGRESSIVE = (1 << 5);
  public static final int XOPT_COPYNONE = (1 << 6);
  public static final int XOPT_ARITHMETIC = (1 << 7);
  public static final int XOPT_OPTIMIZE = (1 << 8);

  // ==========================================================================
  // STRUCTURES AND NESTED CLASSES
  // ==========================================================================

  // tjscalingfactor

  @FieldOrder({ "num", "denom" })
  public static class ScalingFactor extends Structure {
    public int num, denom;

    public ScalingFactor() {}

    @SuppressWarnings("checkstyle:HiddenField")
    public ScalingFactor(int num, int denom) {
      this.num = num;
      this.denom = denom;
    }

    public static class ByValue extends ScalingFactor
      implements Structure.ByValue {
      public ByValue() {}

      public ByValue(int num, int denom) {
        super(num, denom);
      }

      public ByValue(ScalingFactor sf) {
        super(sf.num, sf.denom);
      }
    }

    protected final List<String> getFieldOrder() {
      return Arrays.asList("num", "denom");
    }
  };

  public static final ScalingFactor UNSCALED = new ScalingFactor.ByValue(1, 1);

  // tjregion

  @FieldOrder({ "x", "y", "w", "h" })
  public static class Region extends Structure {
    public int x, y, w, h;

    public Region() {}

    @SuppressWarnings("checkstyle:HiddenField")
    public Region(int x, int y, int w, int h) {
      this.x = x;
      this.y = y;
      this.w = w;
      this.h = h;
    }

    public static class ByValue extends Region implements Structure.ByValue {
      public ByValue() {}

      public ByValue(int x, int y, int w, int h) {
        super(x, y, w, h);
      }

      public ByValue(Region r) {
        super(r.x, r.y, r.w, r.h);
      }
    }

    protected final List<String> getFieldOrder() {
      return Arrays.asList("x", "y", "w", "h");
    }
  };

  public static final Region UNCROPPED = new Region.ByValue(0, 0, 0, 0);

  // tjtransform

  public interface CustomFilterFunc extends Callback {
    int invoke(Pointer coeffs, Region.ByValue arrayRegion,
               Region.ByValue planeRegion, int componentID, int transformID,
               Transform transform);
  }

  @FieldOrder({ "r", "op", "options", "data", "customFilter" })
  public static class Transform extends Structure {
    public Region r;
    public int op, options;
    public Pointer data;
    public CustomFilterFunc customFilter;

    public Transform() {}

    @SuppressWarnings("checkstyle:HiddenField")
    public Transform(Region r, int op, int options, Pointer data,
                     CustomFilterFunc customFilter) {
      this.r.x = r.x;
      this.r.y = r.y;
      this.r.w = r.w;
      this.r.h = r.h;
      this.op = op;
      this.options = options;
      this.data = data;
      this.customFilter = customFilter;
    }

    public static class ByValue extends Transform implements
      Structure.ByValue {
      public ByValue() {}

      public ByValue(Region r, int op, int options, Pointer data,
                     CustomFilterFunc customFilter) {
        super(r, op, options, data, customFilter);
      }

      public ByValue(Transform t) {
        super(t.r, t.op, t.options, t.data, t.customFilter);
      }
    }

    protected final List<String> getFieldOrder() {
      return Arrays.asList("r", "op", "options", "data", "customFilter");
    }
  };

  // TJSCALED()

  public static int scaled(int dimension, ScalingFactor scalingFactor) {
    if (scalingFactor == null || scalingFactor.denom == 0)
      return dimension;
    return ((dimension * scalingFactor.num + scalingFactor.denom - 1) /
            scalingFactor.denom);
  }

  // Exception thrown by the convenience methods

  public static final class Exception extends IOException {
    public Exception(String message, int code) {
      super(message);
      if (errorCode >= 0 && errorCode < NUMERR)
        errorCode = code;
    }

    // Equivalent of TJ.tj3GetErrorCode() when using the convenience methods
    public int getErrorCode() {
      return errorCode;
    }

    private int errorCode = ERR_FATAL;
  }

  // These com.sun.jna.Structure subclasses allow us to do the equivalent of
  // passing an array of PointerByReference or NativeLongByReference instances,
  // since JNA does not allow that.  They are used solely by the lossless
  // transformation methods.

  @FieldOrder("pointer")
  public static class PointerReference extends Structure {
    public Pointer pointer;

    protected final List<String> getFieldOrder() {
      return Arrays.asList("pointer");
    }
  };

  @FieldOrder("value")
  public static class NativeLongReference extends Structure {
    public NativeLong value;

    protected final List<String> getFieldOrder() {
      return Arrays.asList("value");
    }
  };

  // This com.sun.jna.Pointer subclass allows TurboJPEG instance handles to be
  // used with Java try-with-resources statements.

  public static class Handle extends Pointer implements Closeable {
    public Handle(int initType) throws Exception {
      super(nativeValue(init(initType)));
    }

    public final void close() {
      destroy(this);
    }
  }

  // ==========================================================================
  // METHODS
  // ==========================================================================

  public static native Pointer tj3InitVersion(int initType, int apiVersion);

  public static Pointer tj3Init(int initType) {
    return tj3InitVersion(initType, TURBOJPEG_VERSION_NUMBER);
  }

  public static Pointer init(int initType) throws Exception {
    Pointer handle = tj3Init(initType);
    if (handle == null)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
    return handle;
  }

  // --------------------------------------------------------------------------

  public static native void tj3Destroy(Pointer handle);

  public static void destroy(Pointer handle) {
    tj3Destroy(handle);
  }

  // --------------------------------------------------------------------------

  public static native String tj3GetErrorStr(Pointer handle);

  // --------------------------------------------------------------------------

  public static native int tj3GetErrorCode(Pointer handle);

  // --------------------------------------------------------------------------

  public static native int tj3Set(Pointer handle, int param, int value);

  public static void set(Pointer handle, int param, int value)
                         throws Exception {
    if (tj3Set(handle, param, value) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Get(Pointer handle, int param);

  public static int get(Pointer handle, int param) {
    return tj3Get(handle, param);
  }

  // --------------------------------------------------------------------------

  public static native Pointer tj3Alloc(NativeLong bytes);

  public static Pointer alloc(NativeLong bytes) {
    return tj3Alloc(bytes);
  }

  // --------------------------------------------------------------------------

  public static native void tj3Free(Pointer buffer);

  public static void free(Pointer buffer) {
    tj3Free(buffer);
  }

  // --------------------------------------------------------------------------

  public static native NativeLong tj3JPEGBufSize(int width, int height,
                                                 int jpegSubsamp);

  public static NativeLong jpegBufSize(int width, int height, int jpegSubsamp)
                                       throws Exception {
    NativeLong retval = tj3JPEGBufSize(width, height, jpegSubsamp);
    if (retval.longValue() == 0)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native NativeLong tj3YUVBufSize(int width, int align,
                                                int height, int subsamp);

  public static NativeLong yuvBufSize(int width, int align, int height,
                                      int subsamp) throws Exception {
    NativeLong retval = tj3YUVBufSize(width, align, height, subsamp);
    if (retval.longValue() == 0)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native NativeLong tj3YUVPlaneSize(int componentID, int width,
                                                  int stride, int height,
                                                  int subsamp);

  public static NativeLong yuvPlaneSize(int componentID, int width, int stride,
                                        int height, int subsamp)
                                        throws Exception {
    NativeLong retval = tj3YUVPlaneSize(componentID, width, stride, height,
                                        subsamp);
    if (retval.longValue() == 0)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native int tj3YUVPlaneWidth(int componentID, int width,
                                            int subsamp);

  public static int yuvPlaneWidth(int componentID, int width, int subsamp)
                                  throws Exception {
    int retval = tj3YUVPlaneWidth(componentID, width, subsamp);
    if (retval == 0)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native int tj3YUVPlaneHeight(int componentID, int height,
                                             int subsamp);

  public static int yuvPlaneHeight(int componentID, int width, int subsamp)
                                   throws Exception {
    int retval = tj3YUVPlaneHeight(componentID, width, subsamp);
    if (retval == 0)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native int tj3SetICCProfile(Pointer handle, Pointer iccBuf,
                                            NativeLong iccSize);

  public static void setICCProfile(Pointer handle, Pointer iccBuf,
                                    NativeLong iccSize) throws Exception {
    if (tj3SetICCProfile(handle, iccBuf, iccSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Compress8(Pointer handle, Pointer srcBuf,
                                        int width, int pitch, int height,
                                        int pixelFormat,
                                        PointerByReference jpegBuf,
                                        NativeLongByReference jpegSize);

  public static void compress8(Pointer handle, Pointer srcBuf, int width,
                               int pitch, int height, int pixelFormat,
                               PointerByReference jpegBuf,
                               NativeLongByReference jpegSize)
                               throws Exception {
    if (tj3Compress8(handle, srcBuf, width, pitch, height, pixelFormat,
                     jpegBuf, jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Compress12(Pointer handle, Pointer srcBuf,
                                         int width, int pitch, int height,
                                         int pixelFormat,
                                         PointerByReference jpegBuf,
                                         NativeLongByReference jpegSize);

  public static void compress12(Pointer handle, Pointer srcBuf, int width,
                                int pitch, int height, int pixelFormat,
                                PointerByReference jpegBuf,
                                NativeLongByReference jpegSize)
                                throws Exception {
    if (tj3Compress12(handle, srcBuf, width, pitch, height, pixelFormat,
                      jpegBuf, jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Compress16(Pointer handle, Pointer srcBuf,
                                         int width, int pitch, int height,
                                         int pixelFormat,
                                         PointerByReference jpegBuf,
                                         NativeLongByReference jpegSize);

  public static void compress16(Pointer handle, Pointer srcBuf, int width,
                                int pitch, int height, int pixelFormat,
                                PointerByReference jpegBuf,
                                NativeLongByReference jpegSize)
                                throws Exception {
    if (tj3Compress16(handle, srcBuf, width, pitch, height, pixelFormat,
                      jpegBuf, jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  // srcPlanes

  public static native int
    tj3CompressFromYUVPlanes8(Pointer handle, PointerReference srcPlanes,
                              int width, int[] strides, int height,
                              PointerByReference jpegBuf,
                              NativeLongByReference jpegSize);

  public static void compressFromYUVPlanes8(Pointer handle,
                                            PointerReference[] srcPlanes,
                                            int width, int[] strides,
                                            int height,
                                            PointerByReference jpegBuf,
                                            NativeLongByReference jpegSize)
                                            throws Exception {
    if (tj3CompressFromYUVPlanes8(handle, srcPlanes[0], width, strides, height,
                                  jpegBuf, jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3CompressFromYUV8(Pointer handle, Pointer srcBuf,
                                               int width, int align,
                                               int height,
                                               PointerByReference jpegBuf,
                                               NativeLongByReference jpegSize);

  public static void compressFromYUV8(Pointer handle, Pointer srcBuf,
                                      int width, int align, int height,
                                      PointerByReference jpegBuf,
                                      NativeLongByReference jpegSize)
                                      throws Exception {
    if (tj3CompressFromYUV8(handle, srcBuf, width, align, height, jpegBuf,
                            jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3EncodeYUVPlanes8(Pointer handle, Pointer srcBuf,
                                               int width, int pitch,
                                               int height, int pixelFormat,
                                               PointerReference dstPlanes,
                                               int[] strides);

  public static void encodeYUVPlanes8(Pointer handle, Pointer srcBuf,
                                      int width, int pitch, int height,
                                      int pixelFormat,
                                      PointerReference[] dstPlanes,
                                      int[] strides) throws Exception {
    if (tj3EncodeYUVPlanes8(handle, srcBuf, width, pitch, height, pixelFormat,
                            dstPlanes[0], strides) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3EncodeYUV8(Pointer handle, Pointer srcBuf,
                                         int width, int pitch, int height,
                                         int pixelFormat, Pointer dstBuf,
                                         int align);

  public static void encodeYUV8(Pointer handle, Pointer srcBuf, int width,
                                int pitch, int height, int pixelFormat,
                                Pointer dstBuf, int align) throws Exception {
    if (tj3EncodeYUV8(handle, srcBuf, width, pitch, height, pixelFormat,
                      dstBuf, align) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3DecompressHeader(Pointer handle, Pointer jpegBuf,
                                               NativeLong jpegSize);

  public static void decompressHeader(Pointer handle, Pointer jpegBuf,
                                      NativeLong jpegSize) throws Exception {
    if (tj3DecompressHeader(handle, jpegBuf, jpegSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3GetICCProfile(Pointer handle,
                                            PointerByReference iccBuf,
                                            NativeLongByReference iccSize);

  public static void getICCProfile(Pointer handle, PointerByReference iccBuf,
                                   NativeLongByReference iccSize)
                                   throws Exception {
    if (tj3GetICCProfile(handle, iccBuf, iccSize) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native ScalingFactor
    tj3GetScalingFactors(IntByReference numScalingFactors);

  public static ScalingFactor[]
    getScalingFactors(IntByReference numScalingFactors) throws Exception {
    ScalingFactor retval = tj3GetScalingFactors(numScalingFactors);
    if (retval == null)
      throw new Exception(tj3GetErrorStr(null), tj3GetErrorCode(null));
    if (numScalingFactors != null && numScalingFactors.getValue() > 0)
      return (ScalingFactor[])retval.toArray(numScalingFactors.getValue());
    return null;
  }

  // --------------------------------------------------------------------------

  public static native int
    tj3SetScalingFactor(Pointer handle, ScalingFactor.ByValue scalingFactor);

  public static void setScalingFactor(Pointer handle,
                                      ScalingFactor scalingFactor)
                                      throws Exception {
    ScalingFactor.ByValue sf =
      (scalingFactor instanceof ScalingFactor.ByValue ?
       (ScalingFactor.ByValue)scalingFactor :
       new ScalingFactor.ByValue(scalingFactor));
    if (tj3SetScalingFactor(handle, sf) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3SetCroppingRegion(Pointer handle,
                                                Region.ByValue croppingRegion);

  public static void setCroppingRegion(Pointer handle, Region croppingRegion)
                                       throws Exception {
    Region.ByValue cr =
      (croppingRegion instanceof Region.ByValue ?
       (Region.ByValue)croppingRegion : new Region.ByValue(croppingRegion));
    if (tj3SetCroppingRegion(handle, cr) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Decompress8(Pointer handle, Pointer jpegBuf,
                                          NativeLong jpegSize, Pointer dstBuf,
                                          int pitch, int pixelFormat);

  public static void decompress8(Pointer handle, Pointer jpegBuf,
                                 NativeLong jpegSize, Pointer dstBuf,
                                 int pitch, int pixelFormat) throws Exception {
    if (tj3Decompress8(handle, jpegBuf, jpegSize, dstBuf, pitch,
                       pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Decompress12(Pointer handle, Pointer jpegBuf,
                                           NativeLong jpegSize, Pointer dstBuf,
                                           int pitch, int pixelFormat);

  public static void decompress12(Pointer handle, Pointer jpegBuf,
                                  NativeLong jpegSize, Pointer dstBuf,
                                  int pitch, int pixelFormat)
                                  throws Exception {
    if (tj3Decompress12(handle, jpegBuf, jpegSize, dstBuf, pitch,
                        pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3Decompress16(Pointer handle, Pointer jpegBuf,
                                           NativeLong jpegSize, Pointer dstBuf,
                                           int pitch, int pixelFormat);

  public static void decompress16(Pointer handle, Pointer jpegBuf,
                                  NativeLong jpegSize, Pointer dstBuf,
                                  int pitch, int pixelFormat)
                                  throws Exception {
    if (tj3Decompress16(handle, jpegBuf, jpegSize, dstBuf, pitch,
                        pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int
    tj3DecompressToYUVPlanes8(Pointer handle, Pointer jpegBuf,
                              NativeLong jpegSize, PointerReference dstPlanes,
                              int[] strides);

  public static void decompressToYUVPlanes8(Pointer handle, Pointer jpegBuf,
                                            NativeLong jpegSize,
                                            PointerReference[] dstPlanes,
                                            int[] strides) throws Exception {
    if (tj3DecompressToYUVPlanes8(handle, jpegBuf, jpegSize, dstPlanes[0],
                                  strides) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3DecompressToYUV8(Pointer handle,
                                               Pointer jpegBuf,
                                               NativeLong jpegSize,
                                               Pointer dstBuf, int align);

  public static void decompressToYUV8(Pointer handle, Pointer jpegBuf,
                                      NativeLong jpegSize, Pointer dstBuf,
                                      int align) throws Exception {
    if (tj3DecompressToYUV8(handle, jpegBuf, jpegSize, dstBuf, align) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3DecodeYUVPlanes8(Pointer handle,
                                               PointerReference srcPlanes,
                                               int[] strides, Pointer dstBuf,
                                               int width, int pitch,
                                               int height, int pixelFormat);

  public static void decodeYUVPlanes8(Pointer handle,
                                      PointerReference[] srcPlanes,
                                      int[] strides, Pointer dstBuf, int width,
                                      int pitch, int height, int pixelFormat)
                                      throws Exception {
    if (tj3DecodeYUVPlanes8(handle, srcPlanes[0], strides, dstBuf, width,
                            pitch, height, pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3DecodeYUV8(Pointer handle, Pointer srcBuf,
                                         int align, Pointer dstBuf, int width,
                                         int pitch, int height,
                                         int pixelFormat);

  public static void decodeYUV8(Pointer handle, Pointer srcBuf, int align,
                                Pointer dstBuf, int width, int pitch,
                                int height, int pixelFormat) throws Exception {
    if (tj3DecodeYUV8(handle, srcBuf, align, dstBuf, width, pitch, height,
                      pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native NativeLong
    tj3TransformBufSize(Pointer handle, Transform.ByValue transform);

  public static NativeLong transformBufSize(Pointer handle,
                                            Transform transform)
                                            throws Exception {
    Transform.ByValue t =
      (transform instanceof Transform.ByValue ?
       (Transform.ByValue)transform : new Transform.ByValue(transform));
    NativeLong retval = tj3TransformBufSize(handle, t);
    if (retval.longValue() == 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native int tj3Transform(Pointer handle, Pointer jpegBuf,
                                        NativeLong jpegSize, int n,
                                        PointerReference dstBufs,
                                        NativeLongReference dstSizes,
                                        Transform transforms);

  public static void transform(Pointer handle, Pointer jpegBuf,
                               NativeLong jpegSize, int n,
                               PointerReference[] dstBufs,
                               NativeLongReference[] dstSizes,
                               Transform[] transforms) throws Exception {
    if (tj3Transform(handle, jpegBuf, jpegSize, n, dstBufs[0], dstSizes[0],
                     transforms[0]) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native Pointer tj3LoadImage8(Pointer handle, String filename,
                                             IntByReference width, int align,
                                             IntByReference height,
                                             IntByReference pixelFormat);

  public static Pointer loadImage8(Pointer handle, String filename,
                                   IntByReference width, int align,
                                   IntByReference height,
                                   IntByReference pixelFormat)
                                   throws Exception {
    Pointer retval = tj3LoadImage8(handle, filename, width, align, height,
                                   pixelFormat);
    if (retval == null)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native Pointer tj3LoadImage12(Pointer handle, String filename,
                                              IntByReference width, int align,
                                              IntByReference height,
                                              IntByReference pixelFormat);

  public static Pointer loadImage12(Pointer handle, String filename,
                                    IntByReference width, int align,
                                    IntByReference height,
                                    IntByReference pixelFormat)
                                    throws Exception {
    Pointer retval = tj3LoadImage12(handle, filename, width, align, height,
                                    pixelFormat);
    if (retval == null)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native Pointer tj3LoadImage16(Pointer handle, String filename,
                                              IntByReference width, int align,
                                              IntByReference height,
                                              IntByReference pixelFormat);

  public static Pointer loadImage16(Pointer handle, String filename,
                                    IntByReference width, int align,
                                    IntByReference height,
                                    IntByReference pixelFormat)
                                    throws Exception {
    Pointer retval = tj3LoadImage16(handle, filename, width, align, height,
                                    pixelFormat);
    if (retval == null)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
    return retval;
  }

  // --------------------------------------------------------------------------

  public static native int tj3SaveImage8(Pointer handle, String filename,
                                         Pointer buffer, int width, int pitch,
                                         int height, int pixelFormat);

  public static void saveImage8(Pointer handle, String filename,
                                Pointer buffer, int width, int pitch,
                                int height, int pixelFormat) throws Exception {
    if (tj3SaveImage8(handle, filename, buffer, width, pitch, height,
                      pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3SaveImage12(Pointer handle, String filename,
                                          Pointer buffer, int width, int pitch,
                                          int height, int pixelFormat);

  public static void saveImage12(Pointer handle, String filename,
                                 Pointer buffer, int width, int pitch,
                                 int height, int pixelFormat)
                                 throws Exception {
    if (tj3SaveImage12(handle, filename, buffer, width, pitch, height,
                       pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }

  // --------------------------------------------------------------------------

  public static native int tj3SaveImage16(Pointer handle, String filename,
                                          Pointer buffer, int width, int pitch,
                                          int height, int pixelFormat);

  public static void saveImage16(Pointer handle, String filename,
                                 Pointer buffer, int width, int pitch,
                                 int height, int pixelFormat)
                                 throws Exception {
    if (tj3SaveImage16(handle, filename, buffer, width, pitch, height,
                       pixelFormat) < 0)
      throw new Exception(tj3GetErrorStr(handle), tj3GetErrorCode(handle));
  }
};
