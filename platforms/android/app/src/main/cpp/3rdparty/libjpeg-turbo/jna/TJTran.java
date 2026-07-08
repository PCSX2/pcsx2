/*
 * Copyright (C) 2011-2012, 2014-2015, 2017-2018, 2022-2024, 2026
 *           D. R. Commander
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

// This program demonstrates how to use TurboJPEG/JNA to approximate the
// functionality of the IJG's jpegtran program.  jpegtran features that are not
// covered:
//
// - Scan scripts
// - Expanding the input image when cropping
// - Wiping a region of the input image
// - Dropping another JPEG image into the input image
// - Progress reporting
// - Treating warnings as non-fatal [limitation of the TurboJPEG Java API]
// - Debug output

import java.io.*;
import java.nio.*;
import java.util.*;

import com.sun.jna.*;
import com.sun.jna.ptr.*;


final class TJTran {

  private TJTran() {}

  static final String CLASS_NAME =
    new TJTran().getClass().getName();


  private static boolean isCropped(TJ.Region cr) {
    return (cr.x != 0 || cr.y != 0 || cr.w != 0 || cr.h != 0);
  }


  private static String tjErrorMsg;
  private static int tjErrorCode = -1;

  static void handleTJException(TJ.Exception e, int stopOnWarning)
                                throws TJ.Exception {
    String errorMsg = e.getMessage();
    int errorCode = e.getErrorCode();

    if (stopOnWarning != 1 && errorCode == TJ.ERR_WARNING) {
      if (tjErrorMsg == null || !tjErrorMsg.equals(errorMsg) ||
          tjErrorCode != errorCode) {
        tjErrorMsg = errorMsg;
        tjErrorCode = errorCode;
        System.out.println("WARNING: " + errorMsg);
      }
    } else
      throw e;
  }


  static void usage() {
    System.out.println("\nUSAGE: java [Java options] " + CLASS_NAME +
                       " [options] <JPEG input image> <JPEG output image>\n");

    System.out.println("This program reads the DCT coefficients from the lossy JPEG input image,");
    System.out.println("optionally transforms them, and writes them to a lossy JPEG output image.\n");

    System.out.println("OPTIONS (CAN BE ABBREVBIATED)");
    System.out.println("-----------------------------");
    System.out.println("-arithmetic");
    System.out.println("    Use arithmetic entropy coding in the output image instead of Huffman");
    System.out.println("    entropy coding (can be combined with -progressive)");
    System.out.println("-copy all");
    System.out.println("    Copy all extra markers (including comments, JFIF thumbnails, Exif data, and");
    System.out.println("    ICC profile data) from the input image to the output image");
    System.out.println("-copy comments");
    System.out.println("    Do not copy any extra markers, except comment markers, from the input");
    System.out.println("    image to the output image [default]");
    System.out.println("-copy icc");
    System.out.println("    Do not copy any extra markers, except ICC profile data, from the input");
    System.out.println("    image to the output image");
    System.out.println("-copy none");
    System.out.println("    Do not copy any extra markers from the input image to the output image");
    System.out.println("-crop WxH+X+Y");
    System.out.println("    Include only the specified region of the input image.  (W, H, X, and Y are");
    System.out.println("    the width, height, left boundary, and upper boundary of the region, all");
    System.out.println("    specified relative to the transformed image dimensions.)  If necessary, X");
    System.out.println("    and Y will be shifted up and left to the nearest iMCU boundary, and W and H");
    System.out.println("    will be increased accordingly.");
    System.out.println("-flip {horizontal|vertical}, -rotate {90|180|270}, -transpose, -transverse");
    System.out.println("    Perform the specified lossless transform operation (these options are");
    System.out.println("    mutually exclusive)");
    System.out.println("-grayscale");
    System.out.println("    Create a grayscale output image from a full-color input image");
    System.out.println("-icc FILE");
    System.out.println("    Embed the ICC (International Color Consortium) color management profile");
    System.out.println("    from the specified file into the output image");
    System.out.println("-maxmemory N");
    System.out.println("    Memory limit (in megabytes) for intermediate buffers used with progressive");
    System.out.println("    JPEG compression, Huffman table optimization, and lossless transformation");
    System.out.println("    [default = no limit]");
    System.out.println("-maxscans N");
    System.out.println("    Refuse to transform progressive JPEG images that have more than N scans");
    System.out.println("-optimize");
    System.out.println("    Use Huffman table optimization in the output image");
    System.out.println("-perfect");
    System.out.println("    Abort if the requested transform operation is imperfect (non-reversible.)");
    System.out.println("    '-flip horizontal', '-rotate 180', '-rotate 270', and '-transverse' are");
    System.out.println("    imperfect if the image width is not evenly divisible by the iMCU width.");
    System.out.println("    '-flip vertical', '-rotate 90', '-rotate 180', and '-transverse' are");
    System.out.println("    imperfect if the image height is not evenly divisible by the iMCU height.");
    System.out.println("-progressive");
    System.out.println("    Create a progressive output image instead of a single-scan output image");
    System.out.println("    (can be combined with -arithmetic; implies -optimize unless -arithmetic is");
    System.out.println("    also specified)");
    System.out.println("-restart N");
    System.out.println("    Add a restart marker every N MCU rows [default = 0 (no restart markers)].");
    System.out.println("    Append 'B' to specify the restart marker interval in MCUs.");
    System.out.println("-trim");
    System.out.println("    If necessary, trim the partial iMCUs at the right or bottom edge of the");
    System.out.println("    image to make the requested transform perfect\n");

    System.exit(1);
  }


  static boolean matchArg(String arg, String string, int minChars) {
    if (arg.length() > string.length() || arg.length() < minChars)
      return false;

    int cmpChars = Math.max(arg.length(), minChars);
    string = string.substring(0, cmpChars);

    return arg.equalsIgnoreCase(string);
  }


  public static void main(String[] argv) {
    int exitStatus = 0;
    TJ.PointerReference[] dstBuf =
      (TJ.PointerReference[])(new TJ.PointerReference().toArray(1));

    try {

      int i;
      int arithmetic = 0, maxMemory = -1, maxScans = -1, optimize = -1,
        progressive = 0, restartIntervalBlocks = -1, restartIntervalRows = -1,
        saveMarkers = 1, stopOnWarning = -1, subsamp;
      TJ.Transform[] xform = (TJ.Transform[])(new TJ.Transform().toArray(1));
      String iccFilename = null;
      NativeLong srcSize;
      TJ.NativeLongReference[] dstSize =
        (TJ.NativeLongReference[])(new TJ.NativeLongReference().toArray(1));
      Pointer srcBuf, iccBuf;
      int iccSize = 0;

      for (i = 0; i < argv.length; i++) {
        if (matchArg(argv[i], "-arithmetic", 2))
          arithmetic = 1;
        else if (matchArg(argv[i], "-crop", 3) && i < argv.length - 1) {
          int tempWidth = -1, tempHeight = -1, tempX = -1, tempY = -1;
          Scanner scanner = new Scanner(argv[++i]).useDelimiter("x|X|\\+");

          try {
            tempWidth = scanner.nextInt();
            tempHeight = scanner.nextInt();
            tempX = scanner.nextInt();
            tempY = scanner.nextInt();
          } catch (Exception e) {}

          if (tempWidth < 1 || tempHeight < 1 || tempX < 0 || tempY < 0)
            usage();
          xform[0].options |= TJ.XOPT_CROP;
          xform[0].r.w = tempWidth;
          xform[0].r.h = tempHeight;
          xform[0].r.x = tempX;
          xform[0].r.y = tempY;
        } else if (matchArg(argv[i], "-copy", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "all", 1))
            saveMarkers = 2;
          else if (matchArg(argv[i], "icc", 1))
            saveMarkers = 4;
          else if (matchArg(argv[i], "none", 1))
            saveMarkers = 0;
          else if (!matchArg(argv[i], "comments", 1))
            usage();
        } else if (matchArg(argv[i], "-flip", 2) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "horizontal", 1))
            xform[0].op = TJ.XOP_HFLIP;
          else if (matchArg(argv[i], "vertical", 1))
            xform[0].op = TJ.XOP_VFLIP;
          else
            usage();
        } else if (matchArg(argv[i], "-grayscale", 2) ||
                   matchArg(argv[i], "-greyscale", 2))
          xform[0].options |= TJ.XOPT_GRAY;
        else if (matchArg(argv[i], "-icc", 2) && i < argv.length - 1)
          iccFilename = argv[++i];
        else if (matchArg(argv[i], "-maxscans", 5) && i < argv.length - 1) {
          int temp = -1;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 0)
            usage();
          maxScans = temp;
        } else if (matchArg(argv[i], "-maxmemory", 2) && i < argv.length - 1) {
          int temp = -1;

          try {
            temp = Integer.parseInt(argv[++i]);
          } catch (NumberFormatException e) {}
          if (temp < 0)
            usage();
          maxMemory = temp;
        } else if (matchArg(argv[i], "-optimize", 2) ||
                   matchArg(argv[i], "-optimise", 2))
          optimize = 1;
        else if (matchArg(argv[i], "-perfect", 3))
          xform[0].options |= TJ.XOPT_PERFECT;
        else if (matchArg(argv[i], "-progressive", 2))
          progressive = 1;
        else if (matchArg(argv[i], "-rotate", 3) && i < argv.length - 1) {
          i++;
          if (matchArg(argv[i], "90", 2))
            xform[0].op = TJ.XOP_ROT90;
          else if (matchArg(argv[i], "180", 3))
            xform[0].op = TJ.XOP_ROT180;
          else if (matchArg(argv[i], "270", 3))
            xform[0].op = TJ.XOP_ROT270;
          else
            usage();
        } else if (matchArg(argv[i], "-restart", 2) && i < argv.length - 1) {
          int temp = -1;
          String arg = argv[++i];
          Scanner scanner = new Scanner(arg).useDelimiter("b|B");

          try {
            temp = scanner.nextInt();
          } catch (Exception e) {}

          if (temp < 0 || temp > 65535 || scanner.hasNext())
            usage();
          if (arg.endsWith("B") || arg.endsWith("b"))
            restartIntervalBlocks = temp;
          else
            restartIntervalRows = temp;
        } else if (matchArg(argv[i], "-transverse", 7))
          xform[0].op = TJ.XOP_TRANSVERSE;
        else if (matchArg(argv[i], "-trim", 4))
          xform[0].options |= TJ.XOPT_TRIM;
        else if (matchArg(argv[i], "-transpose", 2))
          xform[0].op = TJ.XOP_TRANSPOSE;
        else break;
      }

      if (i != argv.length - 2)
        usage();

      if (iccFilename != null) {
        if (saveMarkers == 2) saveMarkers = 3;
        else if (saveMarkers == 4) saveMarkers = 0;
      }

      try (TJ.Handle tjInstance = new TJ.Handle(TJ.INIT_TRANSFORM)) {

        if (optimize >= 0)
          TJ.set(tjInstance, TJ.PARAM_OPTIMIZE, optimize);
        if (maxScans >= 0)
          TJ.set(tjInstance, TJ.PARAM_SCANLIMIT, maxScans);
        if (restartIntervalBlocks >= 0)
          TJ.set(tjInstance, TJ.PARAM_RESTARTBLOCKS, restartIntervalBlocks);
        if (restartIntervalRows >= 0)
          TJ.set(tjInstance, TJ.PARAM_RESTARTROWS, restartIntervalRows);
        if (maxMemory >= 0)
          TJ.set(tjInstance, TJ.PARAM_MAXMEMORY, maxMemory);
        TJ.set(tjInstance, TJ.PARAM_SAVEMARKERS, saveMarkers);

        File inFile = new File(argv[i++]);
        try (FileInputStream fis = new FileInputStream(inFile)) {
          srcSize = new NativeLong(fis.available());
          if (srcSize.longValue() < 1)
            throw new Exception("Input file contains no data");
          srcBuf = new Memory(srcSize.longValue());
          fis.getChannel().read(srcBuf.getByteBuffer(0, srcSize.longValue()));
        }

        try {
          TJ.decompressHeader(tjInstance, srcBuf, srcSize);
        } catch (TJ.Exception e) { handleTJException(e, stopOnWarning); }
        subsamp = TJ.get(tjInstance, TJ.PARAM_SUBSAMP);
        if ((xform[0].options & TJ.XOPT_GRAY) != 0)
          subsamp = TJ.SAMP_GRAY;
        if (xform[0].op == TJ.XOP_TRANSPOSE ||
            xform[0].op == TJ.XOP_TRANSVERSE ||
            xform[0].op == TJ.XOP_ROT90 || xform[0].op == TJ.XOP_ROT270) {
          if (subsamp == TJ.SAMP_422)
            subsamp = TJ.SAMP_440;
          else if (subsamp == TJ.SAMP_440)
            subsamp = TJ.SAMP_422;
          else if (subsamp == TJ.SAMP_411)
            subsamp = TJ.SAMP_441;
          else if (subsamp == TJ.SAMP_441)
            subsamp = TJ.SAMP_411;
          else if (subsamp == TJ.SAMP_410)
            subsamp = TJ.SAMP_24;
          else if (subsamp == TJ.SAMP_24)
            subsamp = TJ.SAMP_410;
        }

        if (progressive >= 0)
          TJ.set(tjInstance, TJ.PARAM_PROGRESSIVE, progressive);
        if (arithmetic >= 0)
          TJ.set(tjInstance, TJ.PARAM_ARITHMETIC, arithmetic);

        if (isCropped(xform[0].r)) {
          int xAdjust, yAdjust;

          if (subsamp == TJ.SAMP_UNKNOWN)
            throw new Exception("Could not determine subsampling level of input image");
          xAdjust = xform[0].r.x % TJ.MCU_WIDTH[subsamp];
          yAdjust = xform[0].r.y % TJ.MCU_HEIGHT[subsamp];
          xform[0].r.x -= xAdjust;
          xform[0].r.w += xAdjust;
          xform[0].r.y -= yAdjust;
          xform[0].r.h += yAdjust;
        }

        if (iccFilename != null) {
          File iccFile = new File(iccFilename);
          try (FileInputStream fis = new FileInputStream(iccFile)) {
            iccSize = fis.available();
            if (iccSize < 1)
              throw new Exception("ICC profile contains no data");
            iccBuf = new Memory(iccSize);
            fis.getChannel().read(iccBuf.getByteBuffer(0, iccSize));
          }
          TJ.setICCProfile(tjInstance, iccBuf, new NativeLong(iccSize));
        }

        try {
          TJ.transform(tjInstance, srcBuf, srcSize, 1, dstBuf, dstSize, xform);
        } catch (TJ.Exception e) { handleTJException(e, stopOnWarning); }

      }  // try (tjInstance)

      File outFile = new File(argv[i]);
      try (FileOutputStream fos = new FileOutputStream(outFile)) {
        ByteBuffer bb =
          dstBuf[0].pointer.getByteBuffer(0, dstSize[0].value.longValue());
        fos.getChannel().write(bb);
      }
    } catch (Exception e) {
      e.printStackTrace();
      exitStatus = -1;
    } finally {
      TJ.free(dstBuf[0].pointer);
    }

    System.exit(exitStatus);
  }
};
