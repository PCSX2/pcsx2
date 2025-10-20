#!/usr/bin/env python3
import subprocess
from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("src_dir", type=Path, help="Source directory containing .xz files")
parser.add_argument("dst_dir", type=Path, help="Destination directory for .zst files")
parser.add_argument("-force", action="store_true", help="Force reconversion of all files")
parser.add_argument("-zstd-opts", nargs=argparse.REMAINDER, default=[], help="Additional options to pass to zstd")
args = parser.parse_args()

src_dir = args.src_dir
dst_dir = args.dst_dir
zstd_opts = args.zstd_opts
force = args.force

dst_dir.mkdir(parents=True, exist_ok=True)

for src_file in src_dir.glob("*.xz"):
  dst_file = dst_dir / f"{src_file.stem}.zst"

  if force and dst_file.exists():
    print(f"Removing existing '{dst_file}' due to force option")
    dst_file.unlink()
  
  # Skip if up-to-date
  if dst_file.exists() and dst_file.stat().st_mtime >= src_file.stat().st_mtime:
    print(f"Skipping '{src_file}' (up-to-date)")
    continue

  print(f"Converting '{src_file}' -> '{dst_file}'")
  try:
    cmd = f"""xzcat "{src_file}" | zstd {' '.join(zstd_opts)} -o "{dst_file}" """
    subprocess.run(cmd, check=True, shell=True)
  except subprocess.CalledProcessError as e:
    print(f"Error converting '{src_file}': {e}")