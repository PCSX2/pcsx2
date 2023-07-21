import argparse
import glob
import sys
import os
import re
import hashlib

from pathlib import Path

MAX_DIFF_FRAMES = 9999

SCRIPTDIR = os.path.abspath(os.path.dirname(__file__))
CSS_PATH = Path(os.path.join(SCRIPTDIR, "comparer.css")).as_uri()
JS_PATH = Path(os.path.join(SCRIPTDIR, "comparer.js")).as_uri()

FILE_HEADER = """
<!DOCTYPE html>
<html>
<head>
<title>Comparison</title>
""" + f"<link rel=\"stylesheet\" href=\"{CSS_PATH}\" />" + """
</head>
<body>
"""

FILE_FOOTER = """
<div id="myModal" class="modal" tabindex="0">
  <span class="close cursor" onclick="closeModal()">&times;</span>
  <div class="modal-content">
    <div id="compareTitle"></div>
    <img id="compareImage" />
    <div id="compareState"></div>
    <a class="prev">&#10094;</a>
    <a class="next">&#10095;</a>
    <div id="compareCaption"></div>
  </div>
</div>
""" + f"<script src=\"{JS_PATH}\"></script>\n" + """
</body>
</html>
"""

outfile = None
def write(line):
    outfile.write(line + "\n")


def compare_frames(path1, path2):
    try:
        with open(path1, "rb") as f:
            hash1 = hashlib.md5(f.read()).digest()
        with open(path2, "rb") as f:
            hash2 = hashlib.md5(f.read()).digest()

        return hash1 == hash2
    except (FileNotFoundError, IOError):
        return False


def extract_stats(file):
    stats = {}
    try:
        with open(file, "r") as f:
            for line in f.readlines():
                m = re.match(".*@HWSTAT@ ([^:]+): (.*) \(avg ([^)]+)\)$", line)
                if m is None:
                    continue
                stats[m[1]] = int(m[3])
    except FileNotFoundError:
        pass
    except IOError:
        pass
    return stats


def compare_stats(baselinedir, testdir):
    stats1 = extract_stats(os.path.join(baselinedir, "emulog.txt"))
    stats2 = extract_stats(os.path.join(testdir, "emulog.txt"))
    res = []
    for statname in stats1.keys():
        if statname not in stats2 or stats1[statname] == stats2[statname]:
            continue
        v2 = stats2[statname]
        v1 = stats1[statname]
        delta = v2 - v1
        res.append("%s: %s%d [%d=>%d]" % (statname, "+" if delta > 0 else "", delta, v1, v2))
    return res


def check_regression_test(baselinedir, testdir, name):
    #print("Checking '%s'..." % name)

    dir1 = os.path.join(baselinedir, name)
    dir2 = os.path.join(testdir, name)
    if not os.path.isdir(dir2):
        #print("*** %s is missing in test set" % name)
        return False

    images = glob.glob(os.path.join(dir1, "*_frame*.png"))
    diff_frames = []
    first_fail = True
    stats = compare_stats(dir1, dir2)

    for imagepath in images:
        imagename = Path(imagepath).name
        matches = re.match(".*_frame([0-9]+).png", imagename)
        if matches is None:
            continue

        framenum = int(matches[1])

        path1 = os.path.join(dir1, imagename)
        path2 = os.path.join(dir2, imagename)
        if not os.path.isfile(path2):
            print("--- Frame %u for %s is missing in test set" % (framenum, name))
            write("<h1>{}</h1>".format(name))
            write("--- Frame %u for %s is missing in test set" % (framenum, name))
            return False

        if not compare_frames(path1, path2):
            diff_frames.append(framenum)

            if first_fail:
                write("<div class=\"item\">")
                write("<h1>{}</h1>".format(name))
                write("<table width=\"100%\">")
                first_fail = False

            imguri1 = Path(path1).as_uri()
            imguri2 = Path(path2).as_uri()
            write("<tr><td colspan=\"2\">Frame %d</td></tr>" % (framenum))
            write("<tr><td><img class=\"before\" src=\"%s\" /></td><td><img class=\"after\" src=\"%s\" /></td></tr>" % (imguri1, imguri2))

            if len(diff_frames) == MAX_DIFF_FRAMES:
                break

    if len(diff_frames) > 0:
        write("</table>")
        write("<pre>Difference in frames [%s] for %s%s%s</pre>" % (",".join(map(str, diff_frames)), name, "\n" if len(stats) > 0 else "", "\n".join(stats)))
        write("</div>")
        print("*** Difference in frames [%s] for %s" % (",".join(map(str, diff_frames)), name))
        if len(stats) > 0:
            print(stats)
    elif len(stats) > 0:
        write("<h1>{}</h1>".format(name))
        write("<pre>%s</pre>" % "\n".join(stats))
        print(name, stats)

    return len(diff_frames) == 0


def check_regression_tests(baselinedir, testdir):
    gamedirs = glob.glob(baselinedir + "/*", recursive=False)

    success = 0
    failure = 0

    for gamedir in gamedirs:
        name = Path(gamedir).name
        if check_regression_test(baselinedir, testdir, name):
            success += 1
        else:
            failure += 1

    print("%d dumps unchanged" % success)
    print("%d dumps changed" % failure)
    return (failure == 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check frame dump images for regression tests")
    parser.add_argument("-baselinedir", action="store", required=True, help="Directory containing baseline frames to check against")
    parser.add_argument("-testdir", action="store", required=True, help="Directory containing frames to check")
    parser.add_argument("-maxframes", type=int, action="store", required=False, default=9999, help="Max frames to compare")
    parser.add_argument("outfile", action="store", help="The file to write the output to")

    args = parser.parse_args()
    MAX_DIFF_FRAMES = args.maxframes

    outfile = open(args.outfile, "w")
    write(FILE_HEADER)

    if not check_regression_tests(os.path.realpath(args.baselinedir), os.path.realpath(args.testdir)):
        write(FILE_FOOTER)
        outfile.close()
        sys.exit(1)
    else:
        outfile.close()
        os.remove(args.outfile)
        sys.exit(0)
