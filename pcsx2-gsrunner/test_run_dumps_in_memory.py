import argparse
import sys
import os
import subprocess
import platform
import time

def get_gs_name(path):
    lpath = path.lower()

    for extension in [".gs", ".gs.xz", ".gs.zst"]:
        if lpath.endswith(extension):
            return os.path.basename(path)[:-len(extension)]

    return None


def run_regression_test(tester, runner1, runner2, dumpdir, renderer, upscale, renderhacks, gspath, parallel, gsfastreopen, verbose):
    args = [tester]

    args.append("tester")

    args.extend(["-path", runner1, runner2])

    args.extend(["-nthreads", str(parallel)])

    if renderer is not None:
        args.extend(["-renderer", renderer])

    if upscale != 1.0:
        args.extend(["-upscale", str(upscale)])

    if renderhacks is not None:
        args.extend(["-renderhacks", str(renderhacks)])

    args.extend(["-output", dumpdir])

    args.extend(["-log"])

    args.extend(["-verbose-level", str(verbose)])

    # loop a couple of times for those stubborn merge/interlace dumps that don't render anything
    # the first time around
    args.extend(["-loop", "2"])

    if gsfastreopen:
        args.extend(["-batch-gs-fast-reopen"])

    # disable output console entirely
    environ = os.environ.copy()
    environ["PCSX2_NOCONSOLE"] = "1"

    creationflags = 0
    # Set low priority by default
    if platform.system() == "Windows":
        creationflags = 0x00004000  # BELOW_NORMAL_PRIORITY_CLASS
    elif platform.system() in ["Linux", "Darwin"]:
        try:
            os.nice(10)  # lower priority
        except OSError:
            pass

    args.extend(["-input", gspath])

    #print("Running '%s'" % (" ".join(args)))
    subprocess.run(args, env=environ, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, creationflags=creationflags)

    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate frame dump images for regression tests")
    parser.add_argument("-tester", action="store", required=True, help="Path to PCSX2 GS regression tester")
    parser.add_argument("-runner1", action="store", required=True, help="Path to PCSX2 GS runner (1)")
    parser.add_argument("-runner2", action="store", required=True, help="Path to PCSX2 GS runner (2)")
    parser.add_argument("-gsdir", action="store", required=True, help="Directory containing GS dumps")
    parser.add_argument("-dumpdir", action="store", required=True, help="Base directory to dump frames to")
    parser.add_argument("-renderer", action="store", required=False, help="Renderer to use")
    parser.add_argument("-upscale", action="store", type=float, default=1, help="Upscaling multiplier to use")
    parser.add_argument("-renderhacks", action="store", required=False, help="Enable HW Rendering hacks")
    parser.add_argument("-parallel", action="store", type=int, default=1, help="Number of processes to run")
    parser.add_argument("-gsfastreopen", action="store_true", required=False, help="Enable GS fast reopen")
    parser.add_argument("-verbose", action="store", type=int, default=0, required=False, help="Verbose logging level (0-3)")

    args = parser.parse_args()

    start = time.time()
    res = run_regression_test(
      tester = args.tester,
      runner1 = args.runner1,
      runner2 = args.runner2,
      dumpdir = os.path.realpath(args.dumpdir),
      gspath = os.path.realpath(args.gsdir),
      renderer = args.renderer,
      upscale = args.upscale,
      renderhacks = args.renderhacks,
      parallel = args.parallel,
      gsfastreopen = args.gsfastreopen,
      verbose = args.verbose
    )
    end = time.time()
    print("Regression test %.2f seconds" % (end - start))
    if not res:
        sys.exit(1)
    else:
        sys.exit(0)
