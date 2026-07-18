#!/usr/bin/env bash
# ARMSX2 Play Store AAB (come.nanodata.armsx2) — dual-core (4k + 16k page size),
# PGO=optimize, "play" flavor (SAF-only, NO MANAGE_EXTERNAL_STORAGE).
#
# Dual-core trick for an AAB: an app bundle can't be built twice, so we
#   (1) assemblePlayRelease with the 16k core, extract its .so, stage it into
#       src/main/jniLibs/arm64-v8a, then
#   (2) bundlePlayRelease with the 4k core; AGP merges the CMake-built 4k .so
#       with the staged 16k .so into one bundle carrying both.
# The bundle is signed with the playRelease config (from armsx2_keystore.properties);
# Play re-signs with the app signing key on its side, so NO rotation lineage here.
#
# SECRETS: signing comes from armsx2_keystore.properties (gitignored). Nothing
# secret is hardcoded.
#
# Usage:  VC=<versionCode> VN=<versionName> tools/build-play-aab.sh [output.aab]
# Env:    PROF (default ~/Downloads/armsx2.profdata), PKG (default come.nanodata.armsx2)
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # platforms/android
GRADLE="$ROOT_DIR/gradlew"

VC="${VC:?set VC=<versionCode>}"
VN="${VN:?set VN=<versionName>}"
PKG="${PKG:-come.nanodata.armsx2}"
PROF="${PROF:-$HOME/Downloads/armsx2.profdata}"
OUTPUT_AAB="${1:-$HOME/Downloads/ARMSX2-${VN}-play-vc${VC}-dualcore.aab}"

if [[ -z "${JAVA_HOME:-}" && -d "/Applications/Android Studio.app/Contents/jbr/Contents/Home" ]]; then
	export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
fi
if [[ -z "${ANDROID_HOME:-}" && -d "$HOME/Library/Android/sdk" ]]; then
	export ANDROID_HOME="$HOME/Library/Android/sdk"
fi
[[ -f "$ROOT_DIR/armsx2_keystore.properties" ]] || \
	echo "warning: armsx2_keystore.properties absent — AAB will be debug-signed (NOT uploadable)" >&2
[[ -e "$PROF" ]] || { echo "FATAL missing PGO profile: $PROF" >&2; exit 1; }

APK16="$ROOT_DIR/app/build/outputs/apk/play/release/app-play-release.apk"
AAB="$ROOT_DIR/app/build/outputs/bundle/playRelease/app-play-release.aab"
JNI="$ROOT_DIR/app/src/main/jniLibs/arm64-v8a"

# --- 1) build 16k play core, extract .so, stage into jniLibs ---
echo "=== assemblePlayRelease 16k core ==="
rm -f "$APK16"
"$GRADLE" -p "$ROOT_DIR" :app:assemblePlayRelease \
	-Parmsx2.applicationId="$PKG" \
	-Parmsx2.hostPageSize=0x4000 -Parmsx2.nativeLibName=emucore_16k \
	-Parmsx2.pgo=optimize -Parmsx2.pgoProfile="$PROF" \
	-Parmsx2.versionCode="$VC" -Parmsx2.versionName="$VN"
[[ -f "$APK16" ]] || { echo "FATAL no 16k play apk" >&2; exit 1; }
mkdir -p "$JNI"
unzip -p "$APK16" lib/arm64-v8a/libemucore_16k.so > "$JNI/libemucore_16k.so"
sz=$(stat -f%z "$JNI/libemucore_16k.so"); echo "  staged libemucore_16k.so = $((sz/1024/1024)) MB"
[[ "$sz" -gt 10000000 ]] || { echo "FATAL staged 16k too small ($sz)" >&2; exit 1; }

# --- 2) build the AAB with the 4k core; AGP merges CMake 4k + staged 16k ---
echo "=== bundlePlayRelease 4k core (+ staged 16k) ==="
rm -f "$AAB"
"$GRADLE" -p "$ROOT_DIR" :app:bundlePlayRelease \
	-Parmsx2.applicationId="$PKG" \
	-Parmsx2.hostPageSize=0x1000 -Parmsx2.nativeLibName=emucore_4k \
	-Parmsx2.pgo=optimize -Parmsx2.pgoProfile="$PROF" \
	-Parmsx2.versionCode="$VC" -Parmsx2.versionName="$VN"
[[ -f "$AAB" ]] || { echo "FATAL bundlePlayRelease produced no AAB" >&2; exit 1; }
cp -f "$AAB" "$OUTPUT_AAB"

# --- 3) clean staged jniLibs so it can't pollute later single-core builds ---
rm -f "$JNI/libemucore_16k.so"

echo; echo "================= VERIFY ================="
echo "-- both cores in AAB (expect 4k AND 16k) --"
n_cores=$(unzip -l "$OUTPUT_AAB" | grep -cE "base/lib/arm64-v8a/libemucore_(4k|16k)\.so$" || true)
unzip -l "$OUTPUT_AAB" | grep -E "libemucore_(4k|16k)\.so" || { echo "FATAL cores missing" >&2; exit 1; }
[[ "$n_cores" -eq 2 ]] || { echo "FATAL expected 2 cores, got $n_cores" >&2; exit 1; }
echo "-- package (must be $PKG) --"
unzip -p "$OUTPUT_AAB" base/manifest/AndroidManifest.xml | strings | grep -oE "come\.nanodata\.armsx2" | head -1 \
	|| { echo "FATAL wrong package" >&2; exit 1; }
echo "-- MANAGE_EXTERNAL_STORAGE must be ABSENT (play flavor) --"
if unzip -p "$OUTPUT_AAB" base/manifest/AndroidManifest.xml | strings | grep -q "MANAGE_EXTERNAL_STORAGE"; then
	echo "  !! FATAL: MANAGE_EXTERNAL_STORAGE present in play AAB" >&2; exit 1
else echo "  absent OK"; fi
echo "-- versionName --"
unzip -p "$OUTPUT_AAB" base/manifest/AndroidManifest.xml | strings | grep -oE "$VN" | head -1
echo "-- jar signature --"
jarsigner -verify "$OUTPUT_AAB" 2>/dev/null | grep -iE "jar verified|not verified" | head -1
echo; echo "OUTPUT: $OUTPUT_AAB"; shasum -a256 "$OUTPUT_AAB"
echo "AAB-BUILD-DONE"
