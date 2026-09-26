#!/usr/bin/env python3
"""Write local Vulkan manifests for the available 64/32-bit layer libraries."""
import argparse
import json
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "src"))
import nr_build  # noqa: E402


# the platform's shared-library suffix, as both builds name them
SUFFIX = nr_build.SUFFIX


def prepare(destination):
    destination = pathlib.Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    template = json.loads((ROOT / 'src/layer/VkLayer_dlss_nr.json').read_text())
    for arch, filename, manifest_name in (
            ('64', 'libnr_layer' + SUFFIX, 'VkLayer_dlss_nr.json'),
            ('32', 'libnr_layer32' + SUFFIX, 'VkLayer_dlss_nr32.json')):
        library = nr_build.BUILD_DIR / filename   # where `make` or CMake put it
        if not library.exists():
            continue
        manifest = dict(template, layer=dict(template['layer'], library_path=str(library), library_arch=arch))
        (destination / manifest_name).write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=pathlib.Path)
    prepare(parser.parse_args().destination)
