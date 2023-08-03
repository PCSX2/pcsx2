import sys
import xml.etree.ElementTree as ET
import re
import yaml

# Database downloadable from http://redump.org/datfile/ps2/serial,version,description

def parse_serials(serials_text):
    serials = []
    serials_text = serials_text.replace("&", ",")
    serials_text = serials_text.replace("/", ",")
    for serial in serials_text.split(","):
        serial = serial.strip()
        if len(serial) < 3:
            continue

        matches = re.match("([A-Z0-9a-z]+)[\- ]([0-9]+)\-([0-9]+).*", serial)
        if matches is not None:
            rlen = len(matches[3])
            base = matches[2][:-rlen]
            start = int(matches[2][-rlen:])
            end = int(matches[3])
            fmt = "%0" + str(rlen) + "d"
            for rbit in range(start, end + 1):
                code = matches[1] + "-" + base + (fmt % rbit)
                if code in serials:
                    continue

                serials.append(code)
        else:
            matches = re.match("([A-Z0-9a-z]+)[\- ]([0-9]+).*", serial)
            if matches is None:
                continue

            code = matches[1] + "-" + matches[2]
            if code in serials:
                continue

            serials.append(code)
    return serials


def parse_redump(filename):
    games = []
    tree = ET.parse(filename)
    for child in tree.getroot():
        if (child.tag != "game"):
            continue

        name = child.get("name")
        name = name.strip() if name is not None else ""
        node = child.find("version")
        version = node.text.strip() if node is not None else ""
        node = child.find("serial")
        serials_text = node.text.strip() if node is not None else ""
        serials = parse_serials(serials_text)

        # remove version from title if it exists
        sversion = "(" + version + ")"
        name = name.replace(sversion, "")

        hashes = []
        for grandchild in child:
            if grandchild.tag != "rom":
                continue

            tname = grandchild.get("name")
            if ".cue" in tname:
                continue

            tsize = int(grandchild.get("size"))
            tmd5 = grandchild.get("md5")

            track = 1
            matches = re.match(".*\(Track ([0-9]+)\)", tname)
            if matches is not None:
                track = int(matches[1])

            expected_track = len(hashes) + 1
            if track != expected_track:
                print("Expected track %d got track %d" % (expected_track, track))
            hashes.append({"size": tsize,
                          "md5": tmd5
                         })
        if len(hashes) == 0:
            print("No hashes for %s" % name)
            continue

        game = {
            "name": name,
            "hashes": hashes
        }
        if len(version) > 0:
            game["version"] = version
        if len(serials) > 0:
            game["serial"] = serials[0]
        games.append(game)
    return games


def write_yaml(games, filename):
    with open(filename, "w") as f:
        f.write(yaml.dump(games))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: %s <redump xml> <output yaml>" % sys.argv[0])
        sys.exit(1)

    print("Loading %s..." % sys.argv[1])
    games = parse_redump(sys.argv[1])
    if len(games) == 0:
        print("No games found in dat file")
        sys.exit(1)

    print("Writing %s..." % sys.argv[2])
    write_yaml(games, sys.argv[2])
    sys.exit(0)
