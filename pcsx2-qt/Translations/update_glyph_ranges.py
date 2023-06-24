#!/usr/bin/env python3

import os
import re
import xml.etree.ElementTree as ET

languages_to_update = [
    "ja-JP",
    "ko-KR",
    "zh-CN",
    "zh-TW"
]

src_path = os.path.join(os.path.dirname(__file__), "..", "Translations.cpp")
ts_dir = os.path.join(os.path.dirname(__file__))

def parse_xml(path):
    tree = ET.parse(path)
    root = tree.getroot()
    translations = ""
    for node in root.findall("context/message/translation"):
        if node.text:
            translations += node.text

    ords = list(set([ord(ch) for ch in translations if ord(ch) >= 0x2000]))
    if len(ords) == 0:
        return ""

    # Try to organize it into ranges
    ords.sort()
    ord_pairs = []
    start_ord = None
    last_ord = None
    for nord in ords:
        if start_ord is not None and nord == (last_ord + 1):
            last_ord = nord
            continue
        if start_ord is not None:
            ord_pairs.append(start_ord)
            ord_pairs.append(last_ord)
        start_ord = nord
        last_ord = nord

    if start_ord is not None:
        ord_pairs.append(start_ord)
        ord_pairs.append(last_ord)

    chars = "".join([chr(ch) for ch in ord_pairs])
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
        print(f"Updated character list for {ts_file}.")
    else:
        print(f"Character list is unchanged for {ts_file}.")

if __name__ == "__main__":
    for language in languages_to_update:
        ts_file = os.path.join(ts_dir, f"pcsx2-qt_{language}.ts")
        chars = parse_xml(ts_file)
        print(f"{language}: {len(chars)} character(s) detected.")
        update_src_file(ts_file, chars)
