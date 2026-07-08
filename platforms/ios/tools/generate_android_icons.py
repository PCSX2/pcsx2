#!/usr/bin/env python3
"""
Generate Android launcher icons from a single 1024x1024 source image.
- Input: app_icons/icon.png (default) or --src path
- Output: ic_launcher.png and ic_launcher_round.png into mipmap-* folders
- Cleans conflicting resources (png/webp/jpeg) before writing to avoid aapt collisions

Usage:
  python3 tools/generate_android_icons.py --clean
  python3 tools/generate_android_icons.py --src app_icons/icon.png --res app/src/main/res --no-clean

Requires:
  pip install Pillow
"""
from __future__ import annotations
import argparse
import os
import sys
from pathlib import Path

try:
    from PIL import Image
except Exception as e:
    print("Pillow (PIL) is required. Install it via: pip install Pillow", file=sys.stderr)
    sys.exit(1)

DENSITY_SIZES = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192,
}

ICON_NAMES = (
    "ic_launcher", "ic_launcher_round",            
    "ic_launcher_static", "ic_launcher_round_static"  
)
CONFLICT_EXTS = (".png", ".webp", ".jpg", ".jpeg")


def parse_args():
    parser = argparse.ArgumentParser(description="Generate Android mipmap icons from a single source image")
    parser.add_argument("--src", default="app_icons/icon.png", help="Source image path (default: app_icons/icon.png)")
    parser.add_argument("--res", default="app/src/main/res", help="Android res directory (default: app/src/main/res)")
    parser.add_argument("--clean", dest="clean", action="store_true", help="Remove conflicting files before writing")
    parser.add_argument("--no-clean", dest="clean", action="store_false", help="Do not remove existing conflicting files")
    parser.set_defaults(clean=True)
    return parser.parse_args()


def ensure_dir(path: Path):
    path.mkdir(parents=True, exist_ok=True)


def remove_conflicts(dir_path: Path, base_name: str):
    for ext in CONFLICT_EXTS:
        p = dir_path / f"{base_name}{ext}"
        if p.exists():
            try:
                p.unlink()
                print(f"Removed existing {p}")
            except Exception as e:
                print(f"Warning: failed to remove {p}: {e}")


def generate_icons(src: Path, res_dir: Path, clean: bool):
    if not src.exists():
        print(f"Source not found: {src}", file=sys.stderr)
        sys.exit(2)

    with Image.open(src) as im:
        im = im.convert("RGBA")

        for folder, size in DENSITY_SIZES.items():
            out_dir = res_dir / folder
            ensure_dir(out_dir)

            for name in ICON_NAMES:
                if clean:
                    remove_conflicts(out_dir, name)

                out_path = out_dir / f"{name}.png"
                resized = im.resize((size, size), Image.LANCZOS)
                try:
                    resized.save(out_path, format="PNG", optimize=True)
                    print(f"Wrote {out_path}")
                except Exception as e:
                    print(f"Failed to write {out_path}: {e}", file=sys.stderr)
                    sys.exit(3)


def main():
    args = parse_args()
    src = Path(args.src)
    res_dir = Path(args.res)

    ensure_dir(res_dir)

    generate_icons(src, res_dir, args.clean)
    print("Done.")


if __name__ == "__main__":
    main()
