import argparse
import glob
import sys
import os
import subprocess
import multiprocessing
from pathlib import Path
from functools import partial

def get_gs_name(path):
    lpath = path.lower()

    for extension in [".gs", ".gs.xz", ".gs.zst"]:
        if lpath.endswith(extension):
            return os.path.basename(path)[:-len(extension)]

    return None


def run_regression_test(runner, dumpdir, renderer, upscale, renderhacks, parallel, gspath):
    args = [runner]
    gsname = get_gs_name(gspath)

    real_dumpdir = os.path.join(dumpdir, gsname).strip()
    if not os.path.exists(real_dumpdir):
        os.mkdir(real_dumpdir)
    else:
        return

    if renderer is not None:
        args.extend(["-renderer", renderer])

    if upscale != 1.0:
        args.extend(["-upscale", str(upscale)])
        
    if renderhacks is not None:
        args.extend(["-renderhacks", renderhacks])

    args.extend(["-dumpdir", real_dumpdir])
    args.extend(["-logfile", os.path.join(real_dumpdir, "emulog.txt")])

    # loop a couple of times for those stubborn merge/interlace dumps that don't render anything
    # the first time around
    args.extend(["-loop", "2"])

    # disable shader cache for parallel runs, otherwise it'll have sharing violations
    if parallel > 1:
        args.append("-noshadercache")

    # run surfaceless, we don't want tons of windows popping up
    args.append("-surfaceless");

    # disable output console entirely
    environ = os.environ.copy()
    environ["PCSX2_NOCONSOLE"] = "1"

    args.append("--")
    args.append(gspath)

    #print("Running '%s'" % (" ".join(args)))
    subprocess.run(args, env=environ, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)


def run_regression_tests(runner, gsdir, dumpdir, renderer, upscale, renderhacks, parallel=1):
    paths = glob.glob(gsdir + "/*.*", recursive=True)
    gamepaths = list(filter(lambda x: get_gs_name(x) is not None, paths))

    if not os.path.isdir(dumpdir):
        os.mkdir(dumpdir)

    print("Found %u GS dumps" % len(gamepaths))

    if parallel <= 1:
        for game in gamepaths:
            run_regression_test(runner, dumpdir, renderer, upscale, renderhacks, parallel, game)
    else:
        print("Processing %u games on %u processors" % (len(gamepaths), parallel))
        func = partial(run_regression_test, runner, dumpdir, renderer, upscale, renderhacks, parallel)
        pool = multiprocessing.Pool(parallel)
        completed = 0
        for _ in pool.imap_unordered(func, gamepaths, chunksize=1):
            completed += 1
            print("Processed %u of %u GS dumps (%u%%)" % (completed, len(gamepaths), (completed * 100) // len(gamepaths)))
        pool.close()


    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate frame dump images for regression tests")
    parser.add_argument("-runner", action="store", required=True, help="Path to PCSX2 GS runner")
    parser.add_argument("-gsdir", action="store", required=True, help="Directory containing GS dumps")
    parser.add_argument("-dumpdir", action="store", required=True, help="Base directory to dump frames to")
    parser.add_argument("-renderer", action="store", required=False, help="Renderer to use")
    parser.add_argument("-upscale", action="store", type=float, default=1, help="Upscaling multiplier to use")
    parser.add_argument("-renderhacks", action="store", required=False, help="Enable HW Rendering hacks")
    parser.add_argument("-parallel", action="store", type=int, default=1, help="Number of proceeses to run")

    args = parser.parse_args()

    if not run_regression_tests(args.runner, os.path.realpath(args.gsdir), os.path.realpath(args.dumpdir), args.renderer, args.upscale, args.renderhacks, args.parallel):
        sys.exit(1)
    else:
        sys.exit(0)

