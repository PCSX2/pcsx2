import argparse
import sys
import os
import subprocess
import multiprocessing
from functools import partial
import platform
import time

def get_gs_name(path):
    lpath = path.lower()

    for extension in [".gs", ".gs.xz", ".gs.zst"]:
        if lpath.endswith(extension):
            return os.path.basename(path)[:-len(extension)]

    return None


def run_regression_test(runner, dumpdir, renderer, upscale, renderhacks, gspath, gsfastreopen, startfrom, nbatches, batch_id):
    args = [runner]

    args.extend(["-batch"])

    args.extend(["-nbatches", str(nbatches)])

    args.extend(["-batch-id", str(batch_id)])

    if renderer is not None:
        args.extend(["-renderer", renderer])

    if upscale != 1.0:
        args.extend(["-upscale", str(upscale)])

    if renderhacks is not None:
        args.extend(["-renderhacks", renderhacks])

    args.extend(["-dumpdir", dumpdir])
    args.extend(["-logfile", dumpdir])

    if gsfastreopen:
        args.extend(["-batch-gs-fast-reopen"])

    # loop a couple of times for those stubborn merge/interlace dumps that don't render anything
    # the first time around
    args.extend(["-loop", "2"])

    # disable shader cache for parallel runs, otherwise it'll have sharing violations
    if nbatches > 1:
        args.append("-noshadercache")

    # run surfaceless, we don't want tons of windows popping up
    args.append("-surfaceless")

    if startfrom is not None:
        args.extend(["-batch-start-from", startfrom])

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

    args.append("--")
    args.append(gspath)

    #print("Running '%s'" % (" ".join(args)))
    subprocess.run(args, env=environ, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL, creationflags=creationflags)


def run_regression_tests(runner, gsdir, dumpdir, renderer, upscale, renderhacks, gsfastreopen, startfrom, parallel):
    try:
        os.makedirs(dumpdir)
    except FileExistsError:
        pass

    if parallel <= 1:
        run_regression_test(runner, dumpdir, renderer, upscale, renderhacks, gsdir, gsfastreopen, startfrom, 1, 0)
    else:
        print("Processing on %u processors" % (parallel))
        func = partial(run_regression_test, runner, dumpdir, renderer, upscale, renderhacks, gsdir, gsfastreopen, startfrom, parallel)
        pool = multiprocessing.Pool(parallel)
        for i, _ in enumerate(pool.imap_unordered(func, range(parallel), chunksize=1)):
            print("Process %u finished" % (i))
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
    parser.add_argument("-parallel", action="store", type=int, default=1, help="Number of processes to run")
    parser.add_argument("-gsfastreopen", action="store_true", required=False, help="Enable GS fast reopen")
    parser.add_argument("-startfrom", action="store", required=False, help="Dump name/prefix to start from")

    args = parser.parse_args()

    start = time.time()
    res = run_regression_tests(
        runner = args.runner,
        gsdir = os.path.realpath(args.gsdir),
        dumpdir = os.path.realpath(args.dumpdir),
        renderer = args.renderer,
        upscale = args.upscale,
        renderhacks = args.renderhacks,
        startfrom = args.startfrom,
        parallel = args.parallel,
        gsfastreopen = args.gsfastreopen,
    )
    end = time.time()
    print("Regression test %.2f seconds" % (end - start))
    if not res:
        sys.exit(1)
    else:
        sys.exit(0)
