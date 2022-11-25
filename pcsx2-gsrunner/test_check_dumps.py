import argparse
import glob
import sys
import os
import re
import hashlib

from pathlib import Path


FILE_HEADER = """
<!DOCTYPE html>
<html>
<head>
<title>Comparison</title>
</head>
<body>
"""

FILE_FOOTER = """
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
                write("<h1>{}</h1>".format(name))
                write("<table width=\"100%\">")
                first_fail = False

            imguri1 = Path(path1).as_uri()
            imguri2 = Path(path2).as_uri()
            write("<tr><td colspan=\"2\">Frame %d</td></tr>" % (framenum))
            write("<tr><td><img src=\"%s\" /></td><td><img src=\"%s\" /></td></tr>" % (imguri1, imguri2))

    if len(diff_frames) > 0:
        write("</table>")
        write("<pre>Difference in frames [%s] for %s</pre>" % (",".join(map(str, diff_frames)), name))
        print("*** Difference in frames [%s] for %s" % (",".join(map(str, diff_frames)), name))
        return False

    return True


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

    return (failure == 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check frame dump images for regression tests")
    parser.add_argument("-baselinedir", action="store", required=True, help="Directory containing baseline frames to check against")
    parser.add_argument("-testdir", action="store", required=True, help="Directory containing frames to check")
    parser.add_argument("outfile", action="store", help="The file to write the output to")

    args = parser.parse_args()

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
