import os
import shutil

tag = os.environ['TAG_VAL']
scan_dir = os.environ['SCAN_DIR']
output_dir = os.environ['OUT_DIR']
accepted_exts = ["AppImage", "flatpak", "tar.xz", "7z"]


for dir_name in os.listdir(scan_dir):
  asset_name = "pcsx2-{}".format(tag)
  if "macos" in dir_name.lower():
    asset_name += "-macos"
  elif "linux" in dir_name.lower():
    if "flatpak" in dir_name.lower():
      asset_name += "-linux-flatpak-x64"
    else:
      asset_name += "-linux-appimage-x64"
  elif "windows" in dir_name.lower():
    asset_name += "-windows-x64"
  else:
    continue;

  if "avx2" in dir_name.lower():
    asset_name += "-AVX2"
  elif "sse4" in dir_name.lower():
    asset_name += "-SSE4"

  asset_name += "-Qt"

  if "symbols" in dir_name.lower():
    asset_name += "-symbols"

  print(asset_name)

  dir_handled = False
  for file in os.listdir(os.path.join(scan_dir, dir_name)):
    for ext in accepted_exts:
      if file.endswith(ext):
        dir_handled = True
        print("Moving {} to out dir".format(file))
        shutil.move(os.path.join(scan_dir, dir_name, file), os.path.join(output_dir, asset_name + "." + ext))
        break
    if dir_handled:
      break

  if not dir_handled:
    print("Could not find asset in directory when one was expected")
    exit(1)
