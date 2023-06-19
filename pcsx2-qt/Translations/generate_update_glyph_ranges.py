#!/usr/bin/env python3

import sys
import os
import re
import xml.etree.ElementTree as ET

src_path = os.path.join(os.path.dirname(__file__), "..", "Translations.cpp")

def parse_xml(path):
    translations = ""
    tree = ET.parse(path)
    root = tree.getroot()
    for node in root.findall("context/message/translation"):
        if node.text:
            translations += node.text

    chars = list(set([ord(ch) for ch in translations if ord(ch) >= 0x2000]))
    chars.sort()
    chars = "".join([chr(ch) for ch in chars])
    return chars

def update_src_file(ts_file, chars):
    ts_name = os.path.basename(ts_file)
    pattern = re.compile('(// auto update.*' + ts_name + '.*\n[^"]+")[^"]*(".*)')
    with open(src_path, "r", encoding="utf-8") as f:
        original = f.read()
        update = pattern.sub("\\1" + chars + "\\2", original)
    if original != update:
        with open(src_path, "w", encoding="utf-8") as f:
            f.write(update)
        print("Updated character list.")
    else:
        print("Character list is unchanged.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"usage: {sys.argv[0]} <pcsx2-qt_*.ts path>")
        sys.exit(1)

    chars = parse_xml(sys.argv[1])
    #print(chars)
    print(f"{len(chars)} character(s) detected.")
    update_src_file(sys.argv[1], chars)
