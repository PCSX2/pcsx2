/*
 * Copyright (C) 2011-2018, 2022-2026 D. R. Commander
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

// This program tests the various code paths in TurboJPEG/JNA

import java.io.*;
import java.util.*;
import java.nio.file.*;

import com.sun.jna.*;
import com.sun.jna.ptr.*;

@SuppressWarnings("checkstyle:JavadocType")
final class TJUnitTest {

  private TJUnitTest() {}

  static final String CLASS_NAME =
    new TJUnitTest().getClass().getName();

  static void usage() {
    System.out.println("\nUSAGE: java " + CLASS_NAME + " [options]\n");
    System.out.println("Options:");
    System.out.println("-yuv = test YUV encoding/compression/decompression/decoding");
    System.out.println("       (8-bit data precision only)");
    System.out.println("-noyuvpad = do not pad each row in each Y, U, and V plane to the nearest");
    System.out.println("            multiple of 4 bytes");
    System.out.println("-precision N = test N-bit data precision (N=2..16; default is 8; if N is not 8");
    System.out.println("               or 12, then -lossless is implied)");
    System.out.println("-lossless = test lossless JPEG compression/decompression");
    System.out.println("-alloc = test automatic JPEG buffer allocation");
    System.out.println("-bmp = test packed-pixel image I/O\n");
    System.exit(1);
  }

  static final String[] SUBNAME_LONG = {
    "4:4:4", "4:2:2", "4:2:0", "GRAY", "4:4:0", "4:1:1", "4:4:1", "4:1:0",
    "2:4"
  };
  static final String[] SUBNAME = {
    "444", "422", "420", "GRAY", "440", "411", "441", "410", "24"
  };

  static final String[] PIXFORMATSTR = {
    "RGB", "BGR", "RGBX", "BGRX", "XBGR", "XRGB", "Grayscale",
    "RGBA", "BGRA", "ABGR", "ARGB", "CMYK"
  };

  static final int[] FORMATS_3SAMPLE = {
    TJ.PF_RGB, TJ.PF_BGR
  };
  static final int[] FORMATS_4SAMPLE = {
    TJ.PF_RGBX, TJ.PF_BGRX, TJ.PF_XBGR, TJ.PF_XRGB, TJ.PF_CMYK
  };
  static final int[] FORMATS_GRAY = {
    TJ.PF_GRAY
  };
  static final int[] FORMATS_RGB = {
    TJ.PF_RGB
  };

  private static boolean doYUV = false;
  private static boolean lossless = false;
  private static int psv = 1;
  private static boolean alloc = false;
  private static int yuvAlign = 4;
  private static int precision = 8;
  private static int sampleSize, maxSample, tolerance, redToY, yellowToY;

  private static int exitStatus = 0;

  private static String uniqueID =
    java.util.UUID.randomUUID().toString().replace("-", "");

  static void setVal(Pointer buf, int index, int value) {
    if (precision <= 8)
      buf.setByte(index, (byte)value);
    else
      buf.setShort(index * 2, (short)value);
  }

  static void initBuf(Pointer buf, int w, int h, int pf, boolean bottomUp)
                      throws Exception {
    int roffset = TJ.RED_OFFSET[pf];
    int goffset = TJ.GREEN_OFFSET[pf];
    int boffset = TJ.BLUE_OFFSET[pf];
    int ps = TJ.PIXEL_SIZE[pf];
    int index, halfway = 16;

    if (pf == TJ.PF_GRAY) {
      buf.clear(w * h * ps * sampleSize);
      for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
          if (bottomUp)
            index = (h - row - 1) * w + col;
          else
            index = row * w + col;
          if (((row / 8) + (col / 8)) % 2 == 0)
            setVal(buf, index, (row < halfway) ? maxSample : 0);
          else
            setVal(buf, index, (row < halfway) ? redToY : yellowToY);
        }
      }
    } else if (pf == TJ.PF_CMYK) {
      for (int i = 0; i < w * h * ps; i++)
        setVal(buf, i, maxSample);
      for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
          if (bottomUp)
            index = (h - row - 1) * w + col;
          else
            index = row * w + col;
          if (((row / 8) + (col / 8)) % 2 == 0) {
            if (row >= halfway) setVal(buf, index * ps + 3, 0);
          } else {
            setVal(buf, index * ps + 2, 0);
            if (row < halfway)
              setVal(buf, index * ps + 1, 0);
          }
        }
      }
    } else {
      buf.clear(w * h * ps * sampleSize);
      for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
          if (bottomUp)
            index = (h - row - 1) * w + col;
          else
            index = row * w + col;
          if (((row / 8) + (col / 8)) % 2 == 0) {
            if (row < halfway) {
              setVal(buf, index * ps + roffset, maxSample);
              setVal(buf, index * ps + goffset, maxSample);
              setVal(buf, index * ps + boffset, maxSample);
            }
          } else {
            setVal(buf, index * ps + roffset, maxSample);
            if (row >= halfway)
              setVal(buf, index * ps + goffset, maxSample);
          }
        }
      }
    }
  }

  static void checkVal(int row, int col, int v, String vname, int cv)
                       throws Exception {
    v = (v < 0) ? v + 256 : v;
    if (v < cv - tolerance || v > cv + tolerance) {
      throw new Exception("Comp. " + vname + " at " + row + "," + col +
                          " should be " + cv + ", not " + v);
    }
  }

  static void checkVal0(int row, int col, int v, String vname)
                        throws Exception {
    v = (v < 0) ? v + 256 : v;
    if (v > tolerance) {
      throw new Exception("Comp. " + vname + " at " + row + "," + col +
                          " should be 0, not " + v);
    }
  }

  static void checkValMax(int row, int col, int v, String vname)
                          throws Exception {
    v = (v < 0) ? v + 256 : v;
    if (v < maxSample - tolerance) {
      throw new Exception("Comp. " + vname + " at " + row + "," + col +
                          " should be " + maxSample + ", not " + v);
    }
  }

  static int getVal(Pointer buf, int index, int targetPrecision) {
    int v;
    if (targetPrecision <= 8)
      v = (int)buf.getByte(index);
    else
      v = (int)buf.getShort(index * 2);
    if (v < 0)
      v += (1 << targetPrecision);
    return v;
  }

  static boolean checkBuf(Pointer buf, int w, int h, int pf, int subsamp,
                          TJ.ScalingFactor sf, boolean bottomUp)
                          throws Exception {
    int roffset = TJ.RED_OFFSET[pf];
    int goffset = TJ.GREEN_OFFSET[pf];
    int boffset = TJ.BLUE_OFFSET[pf];
    int aoffset = TJ.ALPHA_OFFSET[pf];
    int ps = TJ.PIXEL_SIZE[pf];
    int index;
    boolean retval = true;
    int halfway = 16 * sf.num / sf.denom;
    int blockSize = 8 * sf.num / sf.denom;

    try {

      if (pf == TJ.PF_GRAY)
        roffset = goffset = boffset = 0;

      if (pf == TJ.PF_CMYK) {
        for (int row = 0; row < h; row++) {
          for (int col = 0; col < w; col++) {
            if (bottomUp)
              index = (h - row - 1) * w + col;
            else
              index = row * w + col;
            int c = getVal(buf, index * ps, precision);
            int m = getVal(buf, index * ps + 1, precision);
            int y = getVal(buf, index * ps + 2, precision);
            int k = getVal(buf, index * ps + 3, precision);
            checkValMax(row, col, c, "C");
            if (((row / blockSize) + (col / blockSize)) % 2 == 0) {
              checkValMax(row, col, m, "M");
              checkValMax(row, col, y, "Y");
              if (row < halfway)
                checkValMax(row, col, k, "K");
              else
                checkVal0(row, col, k, "K");
            } else {
              checkVal0(row, col, y, "Y");
              checkValMax(row, col, k, "K");
              if (row < halfway)
                checkVal0(row, col, m, "M");
              else
                checkValMax(row, col, m, "M");
            }
          }
        }
        return true;
      }

      for (int row = 0; row < halfway; row++) {
        for (int col = 0; col < w; col++) {
          if (bottomUp)
            index = (h - row - 1) * w + col;
          else
            index = row * w + col;
          int r = getVal(buf, index * ps + roffset, precision);
          int g = getVal(buf, index * ps + goffset, precision);
          int b = getVal(buf, index * ps + boffset, precision);
          int a = aoffset >= 0 ? getVal(buf, index * ps + aoffset, precision) :
                                 maxSample;
          if (((row / blockSize) + (col / blockSize)) % 2 == 0) {
            if (row < halfway) {
              checkValMax(row, col, r, "R");
              checkValMax(row, col, g, "G");
              checkValMax(row, col, b, "B");
            } else {
              checkVal0(row, col, r, "R");
              checkVal0(row, col, g, "G");
              checkVal0(row, col, b, "B");
            }
          } else {
            if (subsamp == TJ.SAMP_GRAY) {
              if (row < halfway) {
                checkVal(row, col, r, "R", redToY);
                checkVal(row, col, g, "G", redToY);
                checkVal(row, col, b, "B", redToY);
              } else {
                checkVal(row, col, r, "R", yellowToY);
                checkVal(row, col, g, "G", yellowToY);
                checkVal(row, col, b, "B", yellowToY);
              }
            } else {
              checkValMax(row, col, r, "R");
              if (row < halfway) {
                checkVal0(row, col, g, "G");
              } else {
                checkValMax(row, col, g, "G");
              }
              checkVal0(row, col, b, "B");
            }
          }
          checkValMax(row, col, a, "A");
        }
      }
    } catch (Exception e) {
      System.out.println("\n" + e.getMessage());
      retval = false;
    }

    if (!retval) {
      for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
          if (pf == TJ.PF_CMYK) {
            int c = getVal(buf, (row * w + col) * ps, precision);
            int m = getVal(buf, (row * w + col) * ps + 1, precision);
            int y = getVal(buf, (row * w + col) * ps + 2, precision);
            int k = getVal(buf, (row * w + col) * ps + 3, precision);
            System.out.format("%3d/%3d/%3d/%3d ", c, m, y, k);
          } else {
            int r = getVal(buf, (row * w + col) * ps + roffset, precision);
            int g = getVal(buf, (row * w + col) * ps + goffset, precision);
            int b = getVal(buf, (row * w + col) * ps + boffset, precision);
            System.out.format("%3d/%3d/%3d ", r, g, b);
          }
        }
        System.out.print("\n");
      }
    }
    return retval;
  }

  static int pad(int v, int p) {
    return ((v + (p) - 1) & (~((p) - 1)));
  }

  static boolean checkBufYUV(Pointer buf, int w, int h, int subsamp,
                             TJ.ScalingFactor sf) throws Exception {
    int hsf = TJ.MCU_WIDTH[subsamp] / 8, vsf = TJ.MCU_HEIGHT[subsamp] / 8;
    int pw = pad(w, hsf), ph = pad(h, vsf);
    int cw = pw / hsf, ch = ph / vsf;
    int ypitch = pad(pw, yuvAlign), uvpitch = pad(cw, yuvAlign);
    boolean retval = true;
    int halfway = 16 * sf.num / sf.denom;
    int blockSize = 8 * sf.num / sf.denom;

    try {
      for (int row = 0; row < ph; row++) {
        for (int col = 0; col < pw; col++) {
          byte y = buf.getByte(ypitch * row + col);
          if (((row / blockSize) + (col / blockSize)) % 2 == 0) {
            if (row < halfway)
              checkValMax(row, col, y, "Y");
            else
              checkVal0(row, col, y, "Y");
          } else {
            if (row < halfway)
              checkVal(row, col, y, "Y", 76);
            else
              checkVal(row, col, y, "Y", 226);
          }
        }
      }
      if (subsamp != TJ.SAMP_GRAY) {
        halfway = 16 / vsf * sf.num / sf.denom;
        for (int row = 0; row < ch; row++) {
          for (int col = 0; col < cw; col++) {
            byte u = buf.getByte(ypitch * ph + (uvpitch * row + col)),
                 v = buf.getByte(ypitch * ph + uvpitch * ch +
                                 (uvpitch * row + col));
            if (((row * vsf / blockSize) + (col * hsf / blockSize)) % 2 == 0) {
              checkVal(row, col, u, "U", 128);
              checkVal(row, col, v, "V", 128);
            } else {
              if (row < halfway) {
                checkVal(row, col, u, "U", 85);
                checkValMax(row, col, v, "V");
              } else {
                checkVal0(row, col, u, "U");
                checkVal(row, col, v, "V", 149);
              }
            }
          }
        }
      }
    } catch (Exception e) {
      System.out.println("\n" + e.getMessage());
      retval = false;
    }

    if (!retval) {
      for (int row = 0; row < ph; row++) {
        for (int col = 0; col < pw; col++) {
          int y = buf.getByte(ypitch * row + col);
          if (y < 0) y += 256;
          System.out.format("%3d ", y);
        }
        System.out.print("\n");
      }
      System.out.print("\n");
      for (int row = 0; row < ch; row++) {
        for (int col = 0; col < cw; col++) {
          int u = buf.getByte(ypitch * ph + (uvpitch * row + col));
          if (u < 0) u += 256;
          System.out.format("%3d ", u);
        }
        System.out.print("\n");
      }
      System.out.print("\n");
      for (int row = 0; row < ch; row++) {
        for (int col = 0; col < cw; col++) {
          int v = buf.getByte(ypitch * ph + uvpitch * ch +
                              (uvpitch * row + col));
          if (v < 0) v += 256;
          System.out.format("%3d ", v);
        }
        System.out.print("\n");
      }
    }

    return retval;
  }

  static void writeJPEG(Pointer jpegBuf, NativeLong jpegBufSize,
                        String filename) throws Exception {
    File file = new File(filename);
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.getChannel().write(jpegBuf.getByteBuffer(0,
                                                   jpegBufSize.longValue()));
    }
  }

  static String getMD5Sum(Pointer buffer, int len) throws Exception {
    byte[] bytes = buffer.getByteArray(0, len);
    byte[] md5sum =
      java.security.MessageDigest.getInstance("MD5").digest(bytes);
    return new java.math.BigInteger(1, md5sum).toString(16);
  }

  static void compTest(Pointer handle, PointerByReference dstBuf,
                       NativeLongByReference dstSize, int w, int h, int pf,
                       String baseName) throws Exception {
    String tempStr;
    String pfStr = PIXFORMATSTR[pf];
    boolean bottomUp = (TJ.get(handle, TJ.PARAM_BOTTOMUP) == 1);
    int subsamp = TJ.get(handle, TJ.PARAM_SUBSAMP);
    int jpegPSV = TJ.get(handle, TJ.PARAM_LOSSLESSPSV);
    int jpegQual = TJ.get(handle, TJ.PARAM_QUALITY);
    String buStrLong = bottomUp ? "Bottom-Up" : "Top-Down ";
    String buStr = bottomUp ? "BU" : "TD";

    try (Memory srcBuf = new Memory(w * h * TJ.PIXEL_SIZE[pf] * sampleSize)) {

      initBuf(srcBuf, w, h, pf, bottomUp);

      if (dstBuf.getValue() != null && dstSize.getValue().longValue() != 0)
        dstBuf.getValue().clear(dstSize.getValue().longValue());

      if (doYUV) {
        int nc = (subsamp == TJ.SAMP_GRAY ? 1 : 3);
        TJ.PointerReference[] yuvPlanes =
          (TJ.PointerReference[])(new TJ.PointerReference().toArray(nc));
        int[] yuvStrides = new int[nc];
        long yuvSize = TJ.yuvBufSize(w, yuvAlign, h, subsamp).longValue();

        try (Memory yuvBuf = new Memory(yuvSize)) {
          System.out.format("%s %s -> YUV %s ... ", pfStr, buStrLong,
                            SUBNAME_LONG[subsamp]);

          try (TJ.Handle handle2 = new TJ.Handle(TJ.INIT_COMPRESS)) {
            TJ.set(handle2, TJ.PARAM_BOTTOMUP, bottomUp ? 1 : 0);
            TJ.set(handle2, TJ.PARAM_SUBSAMP, subsamp);
            // Verify that TJ.tj3EncodeYUV*8() ignores TJ.PARAM_LOSSLESS and
            // TJ.PARAM_COLORSPACE.
            TJ.set(handle2, TJ.PARAM_LOSSLESS, 1);
            TJ.set(handle2, TJ.PARAM_COLORSPACE, TJ.CS_RGB);

            yuvBuf.clear(yuvSize);
            TJ.encodeYUV8(handle2, srcBuf, w, 0, h, pf, yuvBuf, yuvAlign);
            boolean success = checkBufYUV(yuvBuf, w, h, subsamp, TJ.UNSCALED);

            // Verify that TJ.tj3EncodeYUVPlanes8() produces the same
            // results.
            Pointer yuvPtr = yuvBuf;
            for (int i = 0; i < yuvPlanes.length; i++) {
              int planeWidth, planeHeight;
              NativeLong planeSize;

              planeWidth = TJ.yuvPlaneWidth(i, w, subsamp);
              planeHeight = TJ.yuvPlaneHeight(i, h, subsamp);
              yuvStrides[i] = pad(planeWidth, yuvAlign);
              planeSize = TJ.yuvPlaneSize(i, w, yuvStrides[i], h, subsamp);
              yuvPlanes[i].pointer = yuvPtr;
              yuvPtr = yuvPtr.share(planeSize.longValue() + yuvStrides[i] -
                                    planeWidth);
            }
            yuvBuf.clear(yuvSize);
            TJ.encodeYUVPlanes8(handle2, srcBuf, w, 0, h, pf, yuvPlanes,
                                yuvStrides);
            success &= checkBufYUV(yuvBuf, w, h, subsamp, TJ.UNSCALED);
            if (success)
              System.out.print("Passed.\n");
            else {
              System.out.print("FAILED!\n");
              exitStatus = -1;
            }
          }

          System.out.format("YUV %s %s -> JPEG Q%d ... ",
                            SUBNAME_LONG[subsamp], buStrLong, jpegQual);

          // Verify that TJ.tj3CompressFromYUV*8() ignores TJ.PARAM_LOSSLESS
          // and  TJ.PARAM_COLORSPACE.
          TJ.set(handle, TJ.PARAM_LOSSLESS, 1);
          TJ.set(handle, TJ.PARAM_COLORSPACE, TJ.CS_RGB);
          long dstBufSize = dstSize.getValue().longValue();
          TJ.compressFromYUV8(handle, yuvBuf, w, yuvAlign, h, dstBuf, dstSize);
          String md5ref = getMD5Sum(dstBuf.getValue(),
                                    dstSize.getValue().intValue());

          // Verify that TJ.tj3CompressFromYUVPlanes8() produces the same
          // results.
          dstSize.getValue().setValue(dstBufSize);
          if (dstBuf.getValue() != null && dstSize.getValue().longValue() != 0)
            dstBuf.getValue().clear(dstSize.getValue().longValue());
          TJ.compressFromYUVPlanes8(handle, yuvPlanes, w, yuvStrides, h,
                                    dstBuf, dstSize);
          String md5sum = getMD5Sum(dstBuf.getValue(),
                                    dstSize.getValue().intValue());
          if (!md5sum.equalsIgnoreCase(md5ref))
            throw new Exception("JPEG image has an MD5 sum of " + md5sum +
                                ".  Should be " + md5ref);
        }  // try (yuvBuf)
      } else {  // doYUV
        if (lossless) {
          TJ.set(handle, TJ.PARAM_PRECISION, precision);
          System.out.format("%s %s -> LOSSLESS PSV%d ... ", pfStr, buStrLong,
                            jpegPSV);
        } else
          System.out.format("%s %s -> %s Q%d ... ", pfStr, buStrLong,
                            SUBNAME_LONG[subsamp], jpegQual);
        if (precision <= 8)
          TJ.compress8(handle, srcBuf, w, 0, h, pf, dstBuf, dstSize);
        else if (precision <= 12)
          TJ.compress12(handle, srcBuf, w, 0, h, pf, dstBuf, dstSize);
        else
          TJ.compress16(handle, srcBuf, w, 0, h, pf, dstBuf, dstSize);
      }

    }  // try (srcBuf)

    if (lossless)
      tempStr = baseName + "_enc" + precision + "_" + pfStr + "_" + buStr +
                "_LOSSLESS_PSV" + jpegPSV + ".jpg";
    else
      tempStr = baseName + "_enc" + precision + "_" + pfStr + "_" + buStr +
                "_" + SUBNAME[subsamp] + "_Q" + jpegQual + ".jpg";
    writeJPEG(dstBuf.getValue(), dstSize.getValue(), tempStr);
    System.out.println("Done.\n  Result in " + tempStr);
  }

  static void decompTest(Pointer handle, Pointer jpegBuf, NativeLong jpegSize,
                         int w, int h, int pf, String baseName, int subsamp,
                         TJ.ScalingFactor sf) throws Exception {
    boolean success = true;
    int headerWidth = 0, headerHeight = 0, headerSubsamp;
    int scaledWidth = TJ.scaled(w, sf);
    int scaledHeight = TJ.scaled(h, sf);
    NativeLong dstSize;
    boolean bottomUp = (TJ.get(handle, TJ.PARAM_BOTTOMUP) == 1);

    TJ.setScalingFactor(handle, sf);

    TJ.decompressHeader(handle, jpegBuf, jpegSize);
    headerWidth = TJ.get(handle, TJ.PARAM_JPEGWIDTH);
    headerHeight = TJ.get(handle, TJ.PARAM_JPEGHEIGHT);
    headerSubsamp = TJ.get(handle, TJ.PARAM_SUBSAMP);
    if (lossless && subsamp != TJ.SAMP_444 && subsamp != TJ.SAMP_GRAY)
      subsamp = TJ.SAMP_444;
    if (headerWidth != w || headerHeight != h || headerSubsamp != subsamp)
      throw new Exception("Incorrect JPEG header");

    dstSize = new NativeLong(scaledWidth * scaledHeight * TJ.PIXEL_SIZE[pf]);

    try (Memory dstBuf = new Memory(dstSize.longValue() * sampleSize)) {

      dstBuf.clear(dstSize.longValue() * sampleSize);

      if (doYUV) {
        long yuvSize = TJ.yuvBufSize(scaledWidth, yuvAlign, scaledHeight,
                                     subsamp).longValue();
        try (Memory yuvBuf = new Memory(yuvSize)) {
          int nc = (subsamp == TJ.SAMP_GRAY ? 1 : 3);
          TJ.PointerReference[] yuvPlanes =
            (TJ.PointerReference[])(new TJ.PointerReference().toArray(nc));
          int[] yuvStrides = new int[nc];

          System.out.format("JPEG -> YUV %s ", SUBNAME_LONG[subsamp]);
          if (sf.num != 1 || sf.denom != 1)
            System.out.format("%d/%d ... ", sf.num, sf.denom);
          else System.out.print("... ");

          yuvBuf.clear(yuvSize);
          TJ.decompressToYUV8(handle, jpegBuf, jpegSize, yuvBuf, yuvAlign);
          success = checkBufYUV(yuvBuf, scaledWidth, scaledHeight, subsamp,
                                sf);

          // Verify that TJ.tj3DecompressToYUVPlanes8() produces the same
          // results.
          Pointer yuvPtr = yuvBuf;
          for (int i = 0; i < yuvPlanes.length; i++) {
            int planeWidth, planeHeight;
            NativeLong planeSize;

            planeWidth = TJ.yuvPlaneWidth(i, scaledWidth, subsamp);
            planeHeight = TJ.yuvPlaneHeight(i, scaledHeight, subsamp);
            yuvStrides[i] = pad(planeWidth, yuvAlign);
            planeSize = TJ.yuvPlaneSize(i, scaledWidth, yuvStrides[i],
                                        scaledHeight, subsamp);
            yuvPlanes[i].pointer = yuvPtr;
            long planeOffset = planeSize.longValue() + yuvStrides[i] -
                               planeWidth;
            yuvPtr = yuvPtr.share(planeSize.longValue() + yuvStrides[i] -
                                  planeWidth);
          }
          yuvBuf.clear(yuvSize);
          TJ.decompressToYUVPlanes8(handle, jpegBuf, jpegSize, yuvPlanes,
                                    yuvStrides);
          success &= checkBufYUV(yuvBuf, scaledWidth, scaledHeight, subsamp,
                                 sf);
          if (success)
            System.out.print("Passed.\n");
          else {
            System.out.print("FAILED!\n");
            exitStatus = -1;
          }

          System.out.format("YUV %s -> %s %s ... ", SUBNAME_LONG[subsamp],
                            PIXFORMATSTR[pf],
                            bottomUp ? "Bottom-Up" : "Top-Down ");

          try (TJ.Handle handle2 = new TJ.Handle(TJ.INIT_DECOMPRESS)) {
            TJ.set(handle2, TJ.PARAM_BOTTOMUP, bottomUp ? 1 : 0);
            TJ.set(handle2, TJ.PARAM_SUBSAMP, subsamp);

            TJ.decodeYUV8(handle2, yuvBuf, yuvAlign, dstBuf, scaledWidth, 0,
                          scaledHeight, pf);
            success = checkBuf(dstBuf, scaledWidth, scaledHeight, pf, subsamp,
                               sf, bottomUp);

            // Verify that TJ.tj3DecodeYUVPlanes8() produces the same
            // results.
            TJ.decodeYUVPlanes8(handle2, yuvPlanes, yuvStrides, dstBuf,
                                scaledWidth, 0, scaledHeight, pf);
          }
        }  // try (yuvBuf)
      } else {  // doYUV
        System.out.format("JPEG -> %s %s ", PIXFORMATSTR[pf],
                          bottomUp ? "Bottom-Up" : "Top-Down ");
        if (sf.num != 1 || sf.denom != 1)
          System.out.format("%d/%d ... ", sf.num, sf.denom);
        else System.out.print("... ");
        if (precision <= 8)
          TJ.decompress8(handle, jpegBuf, jpegSize, dstBuf, 0, pf);
        else if (precision <= 12)
          TJ.decompress12(handle, jpegBuf, jpegSize, dstBuf, 0, pf);
        else
          TJ.decompress16(handle, jpegBuf, jpegSize, dstBuf, 0, pf);
      }

      success &= checkBuf(dstBuf, scaledWidth, scaledHeight, pf, subsamp, sf,
                          bottomUp);
      if (success)
        System.out.print("Passed.\n");
      else {
        System.out.print("FAILED!\n");
        exitStatus = -1;
      }

    }  // try (dstBuf)
  }

  static void decompTest(Pointer handle, Pointer jpegBuf, NativeLong jpegSize,
                         int w, int h, int pf, String baseName, int subsamp)
                         throws Exception {
    if (lossless) {
      decompTest(handle, jpegBuf, jpegSize, w, h, pf, baseName, subsamp,
                 TJ.UNSCALED);
      return;
    }

    IntByReference n = new IntByReference();
    TJ.ScalingFactor[] sf = TJ.getScalingFactors(n);

    for (int i = 0; i < n.getValue(); i++) {
      if (subsamp == TJ.SAMP_444 || subsamp == TJ.SAMP_GRAY ||
          ((subsamp == TJ.SAMP_411 || subsamp == TJ.SAMP_441) &&
           sf[i].num == 1 && (sf[i].denom == 2 || sf[i].denom == 1)) ||
          (subsamp != TJ.SAMP_411 && subsamp != TJ.SAMP_441 &&
           subsamp != TJ.SAMP_410 && subsamp != TJ.SAMP_24 &&
           sf[i].num == 1 && (sf[i].denom == 4 || sf[i].denom == 2 ||
                              sf[i].denom == 1)) ||
          (subsamp == TJ.SAMP_420 && sf[i].num == 1 && sf[i].denom == 8 &&
           !doYUV))
        decompTest(handle, jpegBuf, jpegSize, w, h, pf, baseName, subsamp,
                   sf[i]);
    }
  }

  static void doTest(int w, int h, int[] formats, int subsamp, String baseName)
                     throws Exception {
    PointerByReference dstBuf = new PointerByReference();
    NativeLongByReference size = new NativeLongByReference();
    NativeLong bufSize = new NativeLong(0);

    if (lossless && subsamp != TJ.SAMP_GRAY)
      subsamp = TJ.SAMP_444;

    if (!alloc) {
      bufSize = TJ.jpegBufSize(w, h, subsamp);
      size.setValue(bufSize);
      dstBuf.setValue(TJ.alloc(bufSize));
    }

    try (TJ.Handle chandle = new TJ.Handle(TJ.INIT_COMPRESS);
         TJ.Handle dhandle = new TJ.Handle(TJ.INIT_DECOMPRESS)) {

      if (lossless) {
        TJ.set(chandle, TJ.PARAM_LOSSLESS, 1);
        TJ.set(chandle, TJ.PARAM_LOSSLESSPSV, ((psv++ - 1) % 7) + 1);
      } else {
        TJ.set(chandle, TJ.PARAM_QUALITY, 100);
        if (subsamp == TJ.SAMP_422 || subsamp == TJ.SAMP_420 ||
            subsamp == TJ.SAMP_440)
          TJ.set(dhandle, TJ.PARAM_FASTUPSAMPLE, 1);
      }
      TJ.set(chandle, TJ.PARAM_SUBSAMP, subsamp);

      for (int pf : formats) {
        if (pf < 0) continue;
        if (pf == TJ.PF_CMYK &&
            (subsamp == TJ.SAMP_410 || subsamp == TJ.SAMP_24))
          continue;
        for (int i = 0; i < 2; i++) {
          TJ.set(chandle, TJ.PARAM_BOTTOMUP, i == 1 ? 1 : 0);
          TJ.set(dhandle, TJ.PARAM_BOTTOMUP, i == 1 ? 1 : 0);
          if (!alloc) size.setValue(bufSize);
          compTest(chandle, dstBuf, size, w, h, pf, baseName);
          decompTest(dhandle, dstBuf.getValue(), size.getValue(), w, h, pf,
                     baseName, subsamp);
          if (pf >= TJ.PF_RGBX && pf <= TJ.PF_XRGB) {
            System.out.print("\n");
            decompTest(dhandle, dstBuf.getValue(), size.getValue(), w, h,
                       pf + (TJ.PF_RGBA - TJ.PF_RGBX), baseName, subsamp);
          }
          System.out.print("\n");
        }
      }
      System.out.print("--------------------\n\n");

    }  // try (chandle; dhandle)
  }

  static void checkSize(long size, boolean exception, String function)
                        throws Exception {
    if (NativeLong.SIZE == 8) {
      if (size != 0 && size < 0xFFFFFFFFL)
        throw new Exception(function + " overflow");
    } else {
      if (!exception)
        throw new Exception(function + " overflow");
    }
  }

  static void overflowTest() throws Exception {
    // Ensure that the various buffer size methods don't overflow
    NativeLong size = new NativeLong(1);
    int intSize = 1;
    boolean exception = false;

    try {
      exception = false;
      size = TJ.jpegBufSize(26755, 26755, TJ.SAMP_444);
    } catch (Exception e) { exception = true; }
    checkSize(size.longValue(), exception, "TJ.jpegBufSize()");

    try {
      exception = false;
      size = TJ.yuvBufSize(37838, 1, 37838, TJ.SAMP_444);
    } catch (Exception e) { exception = true; }
    checkSize(size.longValue(), exception, "TJ.yuvBufSize()");

    try {
      exception = false;
      size = TJ.yuvBufSize(37837, 3, 37837, TJ.SAMP_444);
    } catch (Exception e) { exception = true; }
    checkSize(size.longValue(), exception, "TJ.yuvBufSize()");

    try {
      exception = false;
      size = TJ.yuvBufSize(37837, -1, 37837, TJ.SAMP_444);
    } catch (Exception e) { exception = true; }
    checkSize(size.longValue(), exception, "TJ.yuvBufSize()");

    try {
      exception = false;
      size = TJ.yuvPlaneSize(0, 65536, 0, 65536, TJ.SAMP_444);
    } catch (Exception e) { exception = true; }
    checkSize(size.longValue(), exception, "TJ.yuvPlaneSize()");

    try {
      exception = false;
      intSize = TJ.yuvPlaneWidth(0, Integer.MAX_VALUE, TJ.SAMP_420);
    } catch (Exception e) { exception = true; }
    if (!exception)
      throw new Exception("TJ.yuvPlaneWidth() overflow");

    try {
      exception = false;
      intSize = TJ.yuvPlaneHeight(0, Integer.MAX_VALUE, TJ.SAMP_420);
    } catch (Exception e) { exception = true; }
    if (!exception)
      throw new Exception("TJ.yuvPlaneHeight() overflow");
  }

  static void bufSizeTest() throws Exception {
    PointerByReference dstBuf = new PointerByReference();
    NativeLongByReference dstSize = new NativeLongByReference();
    int numSamp = TJ.NUMSAMP;
    Random r = new Random();

    try (TJ.Handle handle = new TJ.Handle(TJ.INIT_COMPRESS)) {

      TJ.set(handle, TJ.PARAM_NOREALLOC, alloc ? 0 : 1);
      if (lossless) {
        TJ.set(handle, TJ.PARAM_PRECISION, precision);
        TJ.set(handle, TJ.PARAM_LOSSLESS, 1);
        TJ.set(handle, TJ.PARAM_LOSSLESSPSV, ((psv++ - 1) % 7) + 1);
        numSamp = 1;
      } else
        TJ.set(handle, TJ.PARAM_QUALITY, 100);

      System.out.println("Buffer size regression test");
      for (int subsamp = 0; subsamp < numSamp; subsamp++) {
        TJ.set(handle, TJ.PARAM_SUBSAMP, subsamp);
        for (int w = 1; w < 48; w++) {
          int maxh = (w == 1) ? 2048 : 48;
          for (int h = 1; h < maxh; h++) {
            if (h % 100 == 0)
              System.out.format("%04d x %04d\b\b\b\b\b\b\b\b\b\b\b", w, h);
            try (Memory srcBuf = new Memory(w * h * 4 * sampleSize)) {
              if (!alloc || doYUV) {
                if (doYUV)
                  dstSize.setValue(TJ.yuvBufSize(w, yuvAlign, h, subsamp));
                else
                  dstSize.setValue(TJ.jpegBufSize(w, h, subsamp));
                dstBuf.setValue(TJ.alloc(dstSize.getValue()));
              }

              for (int i = 0; i < w * h * 4; i++)
                setVal(srcBuf, i, r.nextInt(2) * maxSample);

              if (doYUV) {
                // Verify that TJ.tj3EncodeYUV*8() ignores TJ.PARAM_LOSSLESS
                // and TJ.PARAM_COLORSPACE.
                TJ.set(handle, TJ.PARAM_LOSSLESS, 1);
                TJ.set(handle, TJ.PARAM_COLORSPACE, TJ.CS_RGB);
                TJ.encodeYUV8(handle, srcBuf, w, 0, h, TJ.PF_BGRX,
                              dstBuf.getValue(), yuvAlign);
              } else {
                // Verify that the API is hardened against hypothetical
                // applications that may erroneously set the JPEG destination
                // buffer size to 0 while reusing the destination buffer
                // pointer.
                if (alloc && (w > 1 || h > 1))
                  dstSize.setValue(new NativeLong(0));
                if (precision <= 8)
                  TJ.compress8(handle, srcBuf, w, 0, h, TJ.PF_BGRX, dstBuf,
                               dstSize);
                else if (precision <= 12)
                  TJ.compress12(handle, srcBuf, w, 0, h, TJ.PF_BGRX, dstBuf,
                                dstSize);
                else
                  TJ.compress16(handle, srcBuf, w, 0, h, TJ.PF_BGRX, dstBuf,
                                dstSize);
              }
            }  // try (srcBuf)
            if (!alloc || doYUV) {
              TJ.free(dstBuf.getValue());  dstBuf.setValue(null);
            }

            try (Memory srcBuf = new Memory(h * w * 4 * sampleSize)) {
              if (!alloc || doYUV) {
                if (doYUV)
                  dstSize.setValue(TJ.yuvBufSize(h, yuvAlign, w, subsamp));
                else
                  dstSize.setValue(TJ.jpegBufSize(h, w, subsamp));
                dstBuf.setValue(TJ.alloc(dstSize.getValue()));
              }

              for (int i = 0; i < h * w * 4; i++)
                setVal(srcBuf, i, r.nextInt(2) * maxSample);

              if (doYUV) {
                // Verify that TJ.tj3EncodeYUV*8() ignores TJ.PARAM_LOSSLESS
                // and TJ.PARAM_COLORSPACE.
                TJ.set(handle, TJ.PARAM_LOSSLESS, 1);
                TJ.set(handle, TJ.PARAM_COLORSPACE, TJ.CS_RGB);
                TJ.encodeYUV8(handle, srcBuf, h, 0, w, TJ.PF_BGRX,
                              dstBuf.getValue(), yuvAlign);
              } else {
                if (alloc && (h > 1 || w > 1))
                  dstSize.setValue(new NativeLong(0));
                if (precision <= 8)
                  TJ.compress8(handle, srcBuf, h, 0, w, TJ.PF_BGRX, dstBuf,
                               dstSize);
                else if (precision <= 12)
                  TJ.compress12(handle, srcBuf, h, 0, w, TJ.PF_BGRX, dstBuf,
                                dstSize);
                else
                  TJ.compress16(handle, srcBuf, h, 0, w, TJ.PF_BGRX, dstBuf,
                                dstSize);
              }
            }  // try (srcBuf)
            if (!alloc || doYUV) {
              TJ.free(dstBuf.getValue());  dstBuf.setValue(null);
            }
          }
        }
      }
      System.out.println("Done.      ");

    }  // try (handle)
  }

  static void rgbToCMYK(int r, int g, int b, int[] c, int[] m, int[] y,
                        int[] k) {
    double ctmp = 1.0 - ((double)r / (double)maxSample);
    double mtmp = 1.0 - ((double)g / (double)maxSample);
    double ytmp = 1.0 - ((double)b / (double)maxSample);
    double ktmp = Math.min(Math.min(ctmp, mtmp), ytmp);

    if (ktmp == 1.0)
      ctmp = mtmp = ytmp = 0.0;
    else {
      ctmp = (ctmp - ktmp) / (1.0 - ktmp);
      mtmp = (mtmp - ktmp) / (1.0 - ktmp);
      ytmp = (ytmp - ktmp) / (1.0 - ktmp);
    }
    c[0] = (int)((double)maxSample - ctmp * (double)maxSample + 0.5);
    m[0] = (int)((double)maxSample - mtmp * (double)maxSample + 0.5);
    y[0] = (int)((double)maxSample - ytmp * (double)maxSample + 0.5);
    k[0] = (int)((double)maxSample - ktmp * (double)maxSample + 0.5);
  }

  static void initBitmap(Pointer buf, int width, int pitch, int height, int pf,
                         boolean bottomUp) {
    int roffset = TJ.RED_OFFSET[pf];
    int goffset = TJ.GREEN_OFFSET[pf];
    int boffset = TJ.BLUE_OFFSET[pf];
    int ps = TJ.PIXEL_SIZE[pf];

    for (int j = 0; j < height; j++) {
      int row = bottomUp ? height - j - 1 : j;

      for (int i = 0; i < width; i++) {
        int r = (i * (maxSample + 1) / width) % (maxSample + 1);
        int g = (j * (maxSample + 1) / height) % (maxSample + 1);
        int b = (j * (maxSample + 1) / height +
                 i * (maxSample + 1) / width) % (maxSample + 1);

        for (int ci = 0; ci < ps; ci++)
          setVal(buf, row * pitch + i * ps + ci, 0);
        if (pf == TJ.PF_GRAY)
          setVal(buf, row * pitch + i * ps, b);
        else if (pf == TJ.PF_CMYK) {
          int[] c = new int[1], m = new int[1], y = new int[1], k = new int[1];

          rgbToCMYK(r, g, b, c, m, y, k);
          setVal(buf, row * pitch + i * ps + 0, c[0]);
          setVal(buf, row * pitch + i * ps + 1, m[0]);
          setVal(buf, row * pitch + i * ps + 2, y[0]);
          setVal(buf, row * pitch + i * ps + 3, k[0]);
        } else {
          setVal(buf, row * pitch + i * ps + roffset, r);
          setVal(buf, row * pitch + i * ps + goffset, g);
          setVal(buf, row * pitch + i * ps + boffset, b);
        }
      }
    }
  }

  static void cmykToRGB(int c, int m, int y, int k, int[] r, int[] g,
                        int[] b, int targetMaxSample) {
    r[0] = (int)((double)c * (double)k / (double)targetMaxSample + 0.5);
    g[0] = (int)((double)m * (double)k / (double)targetMaxSample + 0.5);
    b[0] = (int)((double)y * (double)k / (double)targetMaxSample + 0.5);
  }

  static boolean cmpBitmap(Pointer buf, int width, int pitch, int height,
                           int pf, boolean bottomUp, boolean gray2rgb,
                           int targetPrecision, String ext) {
    int roffset = TJ.RED_OFFSET[pf];
    int goffset = TJ.GREEN_OFFSET[pf];
    int boffset = TJ.BLUE_OFFSET[pf];
    int aoffset = TJ.ALPHA_OFFSET[pf];
    int ps = TJ.PIXEL_SIZE[pf];

    if (ext.equalsIgnoreCase("bmp"))
      targetPrecision = 8;
    int targetMaxSample = (1 << targetPrecision) - 1;

    for (int j = 0; j < height; j++) {
      int row = bottomUp ? height - j - 1 : j;

      for (int i = 0; i < width; i++) {
        int r = (i * (maxSample + 1) / width) % (maxSample + 1);
        int g = (j * (maxSample + 1) / height) % (maxSample + 1);
        int b = (j * (maxSample + 1) / height +
                 i * (maxSample + 1) / width) % (maxSample + 1);

        if (precision != targetPrecision) {
          long halfMaxSample = maxSample / 2;

          // The expected results are slightly different with PNG files,
          // because the samples are scaled up to 8 or 16 bits of data
          // precision by the PNG writer and scaled down to the target data
          // precision by the PNG reader.
          if (ext.equalsIgnoreCase("png")) {
            if (precision <= 8) {
              r = (int)((r * 255 + halfMaxSample) / maxSample);
              r = (int)((r * ((1 << targetPrecision) - 1) + 127) / 255);
              g = (int)((g * 255 + halfMaxSample) / maxSample);
              g = (int)((g * ((1 << targetPrecision) - 1) + 127) / 255);
              b = (int)((b * 255 + halfMaxSample) / maxSample);
              b = (int)((b * ((1 << targetPrecision) - 1) + 127) / 255);
            } else {
              long rtemp = (long)((r * 65535L + halfMaxSample) / maxSample);
              long gtemp = (long)((g * 65535L + halfMaxSample) / maxSample);
              long btemp = (long)((b * 65535L + halfMaxSample) / maxSample);

              r = (int)((rtemp * ((1 << targetPrecision) - 1) + 32767) /
                        65535);
              g = (int)((gtemp * ((1 << targetPrecision) - 1) + 32767) /
                        65535);
              b = (int)((btemp * ((1 << targetPrecision) - 1) + 32767) /
                        65535);
            }
          } else {
            r = (int)((r * ((1 << targetPrecision) - 1) + halfMaxSample) /
                      maxSample);
            g = (int)((g * ((1 << targetPrecision) - 1) + halfMaxSample) /
                      maxSample);
            b = (int)((b * ((1 << targetPrecision) - 1) + halfMaxSample) /
                      maxSample);
          }
        }

        if (pf == TJ.PF_GRAY) {
          if (getVal(buf, row * pitch + i * ps, targetPrecision) != b)
            return false;
        } else if (pf == TJ.PF_CMYK) {
          int[] rf = new int[1], gf = new int[1], bf = new int[1];

          cmykToRGB(getVal(buf, row * pitch + i * ps + 0, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 1, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 2, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 3, targetPrecision),
                    rf, gf, bf, targetMaxSample);
          if (gray2rgb) {
            if (rf[0] != b || gf[0] != b || bf[0] != b)
              return false;
          } else if (rf[0] != r || gf[0] != g || bf[0] != b)
            return false;
        } else {
          if (gray2rgb) {
            if (getVal(buf, row * pitch + i * ps + roffset,
                       targetPrecision) != b ||
                getVal(buf, row * pitch + i * ps + goffset,
                       targetPrecision) != b ||
                getVal(buf, row * pitch + i * ps + boffset,
                       targetPrecision) != b)
              return false;
          } else if (getVal(buf, row * pitch + i * ps + roffset,
                            targetPrecision) != r ||
                     getVal(buf, row * pitch + i * ps + goffset,
                            targetPrecision) != g ||
                     getVal(buf, row * pitch + i * ps + boffset,
                            targetPrecision) != b)
            return false;
          if (aoffset >= 0 &&
              getVal(buf, row * pitch + i * ps + aoffset,
                     targetPrecision) != targetMaxSample)
            return false;
        }
      }
    }
    return true;
  }

  static String getMD5Sum(String filename) throws Exception {
    byte[] bytes = Files.readAllBytes(Paths.get(filename));
    byte[] md5sum =
      java.security.MessageDigest.getInstance("MD5").digest(bytes);
    return new java.math.BigInteger(1, md5sum).toString(16);
  }

  static void doBmpTest(String ext, int width, int align, int height, int pf,
                        boolean bottomUp) throws Exception {
    String filename, md5sum;
    int ps = TJ.PIXEL_SIZE[pf], pitch = pad(width * ps, align),
      pixelFormat = pf;
    IntByReference loadWidth = new IntByReference(0),
      loadHeight = new IntByReference(0), loadPF = new IntByReference(pf);
    Pointer buf = null;
    String md5ref;
    String[] colorPNGRefs = new String[] {
      "", "", "17aa360ae532ae6e91b2a28d38bd624a",
      "e2a50ec3401ad862738d6987c6ceb7a1", "463085239d02088e0dc797ec548d8ca2",
      "6568db6ab8cca36e28c49d54a5cf72d2", "7ef966e63e71c9a1d36fc7b063a6b188",
      "9b729cea39a6da8927c7d3e42fa8aa2a", "82795201e05fd48c3992d1a7277ff291",
      "80c1ea7e0d9d6a0a8d12722bc349f94c", "6a13b4aefcc08f136952cb77f934c969",
      "c459ba1d57d66b5025798cba6f9cb137", "ce7e67e24d223120ba49f81278549389",
      "1ebb0a8fc1d7d1b291f07a1743710427", "b68fd59466d37a4405fef22f50708c3e",
      "6941dae98e471279433de7edb9fe4e5",  "8df69d8b7ddd4fc45675c1404f8301df"
    };
    String[] grayPNGRefs = new String[] {
      "", "", "36b39208ebecab7e44d3e7d16540d9dd",
      "a23ea89e91a33e47d487da5e51aaf73c", "9fd24370306a37faab455f8569e364f1",
      "8190d5e4e51adeb7345b735b6dbdc07c", "8842cd8a1d8ee88cdf8f4ee3ec35ef9f",
      "7e8f8c89d62ccc14e062024f47896c78", "412f0ac480f413d39a36c8cd3f0ffff5",
      "d7d3588dc714e41e537acda1327dceaa", "1d18b75a701fae306e78d2c8d29ee8cd",
      "e65d72d45858b7f6edd22d49285086dc", "33ed8ea0a2c3faa1e24a766e0706f241",
      "8da798e493c3f0d335278dc69004444b", "f7c4aa691f459aef9e9796f2f8b10f3e",
      "7e601698088be2559538efc750b978a5", "580aea16346e6afb4317399539bf75e"
    };
    String[] colorPPMRefs = new String[] {
      "", "", "bad09d9ef38eda566848fb7c0b7fd0a",
      "7ef2c87261a8bd6838303b541563cf27", "28a37cf9636ff6bb9ed6b206bdac60db",
      "723307791d42e0b5f9e91625c7636086", "d729c4bcd3addc14abc16b656c6bbc98",
      "5d7636eedae3cf579b6de13078227548", "c0c9f772b464d1896326883a5c79c545",
      "fcf6490e0445569427f1d95baf5f8fcb", "5cbc3b0ccba23f5781d950a72e0ccc83",
      "d4e26d6d16d7bfee380f6feb10f7e53",  "2ff5299287017502832c99718450c90a",
      "44ae6cd70c798ea583ab0c8c03621092", "697b2fe03892bc9a75396ad3e73d9203",
      "599732f973eb7c0849a888e783bbe27e", "623f54661b928d170bd2324bc3620565"
    };
    String[] grayPPMRefs = new String[] {
      "", "", "7565be35a2ce909cae016fa282af8efa",
      "e86b9ea57f7d53f6b5497653740992b5", "8924d4d81fe0220c684719294f93407a",
      "e2e69ba70efcfae317528c91651c7ae2", "e6154aafc1eb9e4333d68ce7ad9df051",
      "3d7fe831d6fbe55d3fa12f52059c15d3", "112c682e82ce5de1cca089e20d60000b",
      "5a7ce86c649dda86d6fed185ab78a67",  "b723c0bc087592816523fbc906b7c3a",
      "5da422b1ddfd44c7659094d42ba5580c", "d1895c7e6f2b2c9af6e821a655c239c",
      "fc2803bca103ff75785ea0dca992aa",   "d8c91fac522c16b029e514d331a22bc4",
      "e50cff0b3562ed7e64dbfc093440e333", "64f3320b226ea37fb58080713b4df1b2"
    };
    int maxTargetPrecision = 16;

    try (TJ.Handle handle = new TJ.Handle(TJ.INIT_TRANSFORM)) {

      TJ.set(handle, TJ.PARAM_BOTTOMUP, bottomUp ? 1 : 0);
      TJ.set(handle, TJ.PARAM_PRECISION, precision);

      if (precision == 8 && ext.equalsIgnoreCase("bmp")) {
        md5ref = (pf == TJ.PF_GRAY ? "51976530acf75f02beddf5d21149101d" :
                                     "6d659071b9bfcdee2def22cb58ddadca");
        maxTargetPrecision = 8;
      } else if (ext.equalsIgnoreCase("png"))
        md5ref = (pf == TJ.PF_GRAY ? grayPNGRefs[precision] :
                                     colorPNGRefs[precision]);
      else
        md5ref = (pf == TJ.PF_GRAY ? grayPPMRefs[precision] :
                                     colorPPMRefs[precision]);

      try (Memory saveBuf = new Memory(pitch * height * sampleSize)) {
        initBitmap(saveBuf, width, pitch, height, pf, bottomUp);

        filename = String.format("test_bmp%d_%s_%d_%s_%s.%s", precision,
                                 PIXFORMATSTR[pf], align,
                                 bottomUp ? "bu" : "td", uniqueID, ext);
        if (precision <= 8)
          TJ.saveImage8(handle, filename, saveBuf, width, pitch, height, pf);
        else if (precision <= 12)
          TJ.saveImage12(handle, filename, saveBuf, width, pitch, height, pf);
        else
          TJ.saveImage16(handle, filename, saveBuf, width, pitch, height, pf);
      }
      md5sum = getMD5Sum(filename);
      if (md5sum == null)
        throw new Exception("Could not determine MD5 sum of " + filename);
      if (!md5sum.equalsIgnoreCase(md5ref))
        throw new Exception(filename + " has an MD5 sum of " + md5sum +
                            ".  Should be " + md5ref);

      for (int targetPrecision = 2; targetPrecision <= maxTargetPrecision;
           targetPrecision++) {
        TJ.set(handle, TJ.PARAM_PRECISION, targetPrecision);
        loadPF.setValue(pixelFormat);

        if (targetPrecision <= 8)
          buf = TJ.loadImage8(handle, filename, loadWidth, align, loadHeight,
                              loadPF);
        else if (targetPrecision <= 12)
          buf = TJ.loadImage12(handle, filename, loadWidth, align, loadHeight,
                               loadPF);
        else
          buf = TJ.loadImage16(handle, filename, loadWidth, align, loadHeight,
                               loadPF);
        pf = loadPF.getValue();
        if (width != loadWidth.getValue() || height != loadHeight.getValue())
          throw new Exception("Image dimensions of " + filename +
                              " are bogus");
        pitch = pad(width * TJ.PIXEL_SIZE[pf], align);
        if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, false,
                       targetPrecision, ext))
          throw new Exception("Pixel data in " + filename + " is bogus " +
                              "(target data precision = " + targetPrecision +
                              ")");
        TJ.free(buf);  buf = null;

        if (pf == TJ.PF_GRAY) {
          loadPF.setValue(TJ.PF_XBGR);
          if (targetPrecision <= 8)
            buf = TJ.loadImage8(handle, filename, loadWidth, align, loadHeight,
                                loadPF);
          else if (targetPrecision <= 12)
            buf = TJ.loadImage12(handle, filename, loadWidth, align,
                                 loadHeight, loadPF);
          else
            buf = TJ.loadImage16(handle, filename, loadWidth, align,
                                 loadHeight, loadPF);
          pf = loadPF.getValue();
          pitch = pad(width * TJ.PIXEL_SIZE[pf], align);
          if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, true,
                         targetPrecision, ext))
            throw new Exception("Converting " + filename + " to RGB failed " +
                                "(target data precision = " + targetPrecision +
                                ")");
          TJ.free(buf);  buf = null;

          loadPF.setValue(TJ.PF_CMYK);
          if (targetPrecision <= 8)
            buf = TJ.loadImage8(handle, filename, loadWidth, align, loadHeight,
                                loadPF);
          else if (targetPrecision <= 12)
            buf = TJ.loadImage12(handle, filename, loadWidth, align,
                                 loadHeight, loadPF);
          else
            buf = TJ.loadImage16(handle, filename, loadWidth, align,
                                 loadHeight, loadPF);
          pf = loadPF.getValue();
          pitch = pad(width * TJ.PIXEL_SIZE[pf], align);
          if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, true,
                         targetPrecision, ext))
            throw new Exception("Converting " + filename +
                                " to CMYK failed (target data precision = " +
                                targetPrecision + ")");
          TJ.free(buf);  buf = null;
        }

        // Verify that TJ.tj3LoadImage*() returns the proper "preferred" pixel
        // format for the file type.
        pf = pixelFormat;
        pixelFormat = TJ.PF_UNKNOWN;
        loadPF.setValue(pixelFormat);
        if (targetPrecision <= 8)
          buf = TJ.loadImage8(handle, filename, loadWidth, align, loadHeight,
                              loadPF);
        else if (targetPrecision <= 12)
          buf = TJ.loadImage12(handle, filename, loadWidth, align, loadHeight,
                               loadPF);
        else
          buf = TJ.loadImage16(handle, filename, loadWidth, align, loadHeight,
                               loadPF);
        pixelFormat = loadPF.getValue();
        TJ.free(buf);  buf = null;
        if ((pf == TJ.PF_GRAY && pixelFormat != TJ.PF_GRAY) ||
            (pf != TJ.PF_GRAY && ext.equalsIgnoreCase("bmp") &&
             pixelFormat != TJ.PF_BGR) ||
            (pf != TJ.PF_GRAY && ext.equalsIgnoreCase("ppm") &&
             pixelFormat != TJ.PF_RGB))
          throw new Exception("TJCompressor.loadImage() returned unexpected " +
                              "pixel format: " + PIXFORMATSTR[pixelFormat]);
      }
      File file = new File(filename);
      file.delete();

    } finally {  // try (handle)
      TJ.free(buf);
    }
  }

  static void bmpTest() throws Exception {
    int width = 35, height = 39;

    for (int align = 1; align <= 8; align *= 2) {
      for (int format = 0; format < TJ.NUMPF; format++) {
        if (precision == 8) {
          System.out.format("%s Top-Down BMP (row alignment = %d samples)  ...  ",
                            PIXFORMATSTR[format], align);
          doBmpTest("bmp", width, align, height, format, false);
          System.out.println("OK.");
        }

        System.out.format("%s Top-Down PNG (row alignment = %d samples)  ...  ",
                          PIXFORMATSTR[format], align);
        doBmpTest("png", width, align, height, format, false);
        System.out.println("OK.");

        System.out.format("%s Top-Down PPM (row alignment = %d samples)  ...  ",
                          PIXFORMATSTR[format], align);
        doBmpTest("ppm", width, align, height, format, false);
        System.out.println("OK.");

        if (precision == 8) {
          System.out.format("%s Bottom-Up BMP (row alignment = %d samples)  ...  ",
                            PIXFORMATSTR[format], align);
          doBmpTest("bmp", width, align, height, format, true);
          System.out.println("OK.");
        }

        System.out.format("%s Bottom-Up PNG (row alignment = %d samples)  ...  ",
                          PIXFORMATSTR[format], align);
        doBmpTest("png", width, align, height, format, true);
        System.out.println("OK.");

        System.out.format("%s Bottom-Up PPM (row alignment = %d samples)  ...  ",
                          PIXFORMATSTR[format], align);
        doBmpTest("ppm", width, align, height, format, true);
        System.out.println("OK.");
      }
    }
  }

  public static void main(String[] argv) {
    try {
      String testName = "javatest";
      boolean bmp = false;

      for (int i = 0; i < argv.length; i++) {
        if (argv[i].equalsIgnoreCase("-yuv"))
          doYUV = true;
        else if (argv[i].equalsIgnoreCase("-noyuvpad"))
          yuvAlign = 1;
        else if (argv[i].equalsIgnoreCase("-lossless"))
          lossless = true;
        else if (argv[i].equalsIgnoreCase("-alloc"))
          alloc = true;
        else if (argv[i].equalsIgnoreCase("-bmp"))
          bmp = true;
        else if (argv[i].equalsIgnoreCase("-precision") &&
                 i < argv.length - 1) {
          int tempi = -1;

          try {
            tempi = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (tempi < 2 || tempi > 16)
            usage();
          precision = tempi;
          if (precision != 8 && precision != 12)
            lossless = true;
        } else
          usage();
      }
      if (lossless && doYUV)
        throw new Exception("Lossless JPEG and YUV encoding/decoding are incompatible.");
      if (precision != 8 && doYUV)
        throw new Exception("YUV encoding/decoding requires 8-bit data precision.");

      System.out.format("Testing %d-bit precision\n", precision);
      sampleSize = (precision <= 8 ? 1 : 2);
      maxSample = (1 << precision) - 1;
      tolerance = (lossless ? 0 : (precision > 8 ? 2 : 1));
      redToY = (19595 * maxSample) >> 16;
      yellowToY = (58065 * maxSample) >> 16;

      if (bmp) {
        bmpTest();
        System.exit(exitStatus);
      }
      if (doYUV)
        FORMATS_4SAMPLE[4] = -1;
      overflowTest();
      doTest(35, 39, FORMATS_3SAMPLE, TJ.SAMP_444, testName);
      doTest(39, 41, FORMATS_4SAMPLE, TJ.SAMP_444, testName);
      doTest(41, 35, FORMATS_3SAMPLE, TJ.SAMP_422, testName);
      if (!lossless) {
        doTest(35, 39, FORMATS_4SAMPLE, TJ.SAMP_422, testName);
        doTest(39, 41, FORMATS_3SAMPLE, TJ.SAMP_420, testName);
        doTest(41, 35, FORMATS_4SAMPLE, TJ.SAMP_420, testName);
        doTest(35, 39, FORMATS_3SAMPLE, TJ.SAMP_440, testName);
        doTest(39, 41, FORMATS_4SAMPLE, TJ.SAMP_440, testName);
        doTest(41, 35, FORMATS_3SAMPLE, TJ.SAMP_411, testName);
        doTest(35, 39, FORMATS_4SAMPLE, TJ.SAMP_411, testName);
        doTest(39, 41, FORMATS_3SAMPLE, TJ.SAMP_441, testName);
        doTest(41, 35, FORMATS_4SAMPLE, TJ.SAMP_441, testName);
        doTest(35, 41, FORMATS_3SAMPLE, TJ.SAMP_410, testName);
        doTest(39, 35, FORMATS_4SAMPLE, TJ.SAMP_410, testName);
        doTest(41, 39, FORMATS_3SAMPLE, TJ.SAMP_24, testName);
        doTest(35, 41, FORMATS_4SAMPLE, TJ.SAMP_24, testName);
      }
      doTest(39, 41, FORMATS_GRAY, TJ.SAMP_GRAY, testName);
      if (!lossless) {
        doTest(41, 35, FORMATS_3SAMPLE, TJ.SAMP_GRAY, testName);
        FORMATS_4SAMPLE[4] = -1;
        doTest(35, 39, FORMATS_4SAMPLE, TJ.SAMP_GRAY, testName);
      }
      bufSizeTest();
      if (doYUV) {
        System.out.print("\n--------------------\n\n");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_444, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_422, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_420, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_440, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_411, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_441, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_410, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_24, "javatest_yuv0");
        doTest(48, 48, FORMATS_RGB, TJ.SAMP_GRAY, "javatest_yuv0");
        doTest(48, 48, FORMATS_GRAY, TJ.SAMP_GRAY, "javatest_yuv0");
      }
    } catch (Exception e) {
      e.printStackTrace();
      exitStatus = -1;
    }
    System.exit(exitStatus);
  }
}
