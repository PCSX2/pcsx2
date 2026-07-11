#!/usr/bin/env bash
# ARMSX2 sideload RELEASE APK (com.armsx2) — dual-core (4k + 16k page size),
# PGO=optimize, ROTATION-signed. This is the real distribution recipe; the
# sibling build-universal-page-apk.sh is only a debug/no-PGO smoke-test builder.
#
# WHY ROTATION SIGNING IS MANDATORY: every published com.armsx2 sideload APK is
# signed with a v3 signing-lineage rotation (old debug key for API<=32, new
# release key for API33+). Ship an APK signed any other way and on-device
# updates over an existing install will be rejected. gradle cannot express the
# lineage, so we let gradle build/sign, then strip META-INF and re-sign with
# apksigner below.
#
# SECRETS: the release key + passwords are read from armsx2_keystore.properties
# (gitignored, next to this repo's android gradle root) or from RELEASE_* env
# vars. Nothing secret is hardcoded here. The debug key uses the public
# "android" password by design.
#
# Usage:
#   VC=<versionCode> VN=<versionName> tools/build-release-apk.sh [output.apk]
# Env overrides:
#   VC, VN                versionCode / versionName (required)
#   PROF                  merged .profdata for PGO   (default ~/Downloads/armsx2.profdata)
#   LINEAGE               signing lineage .bin       (default ~/Downloads/armsx2_signing_lineage.bin)
#   DEBUG_KS              old-signer keystore        (default ~/.android/debug.keystore)
#   RELEASE_KS/RELEASE_KS_PASS/RELEASE_KEY_ALIAS/RELEASE_KEY_PASS
#                         new-signer overrides (else read from armsx2_keystore.properties)
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # platforms/android
GRADLE="$ROOT_DIR/gradlew"

VC="${VC:?set VC=<versionCode>}"
VN="${VN:?set VN=<versionName>}"
PROF="${PROF:-$HOME/Downloads/armsx2.profdata}"
LINEAGE="${LINEAGE:-$HOME/Downloads/armsx2_signing_lineage.bin}"
DEBUG_KS="${DEBUG_KS:-$HOME/.android/debug.keystore}"
OUTPUT_APK="${1:-$HOME/Downloads/ARMSX2-Refresh-${VN}-vc${VC}.apk}"

if [[ -z "${JAVA_HOME:-}" && -d "/Applications/Android Studio.app/Contents/jbr/Contents/Home" ]]; then
	export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
fi
if [[ -z "${ANDROID_HOME:-}" && -d "$HOME/Library/Android/sdk" ]]; then
	export ANDROID_HOME="$HOME/Library/Android/sdk"
fi
[[ -n "${ANDROID_HOME:-}" && -d "$ANDROID_HOME" ]] || { echo "error: ANDROID_HOME not set" >&2; exit 1; }

BT="$(find "$ANDROID_HOME/build-tools" -mindepth 1 -maxdepth 1 -type d | sort | tail -1)"
ZIPALIGN="$BT/zipalign"; APKSIGNER="$BT/apksigner"; AAPT="$BT/aapt2"

# --- resolve the release (new) signer from armsx2_keystore.properties or env ---
PROPS="$ROOT_DIR/armsx2_keystore.properties"
prop() { [[ -f "$PROPS" ]] && sed -n "s/^$1=//p" "$PROPS" | head -1; }
REL_KS="${RELEASE_KS:-$(prop storeFile)}"
REL_KS_PASS="${RELEASE_KS_PASS:-$(prop storePassword)}"
REL_KEY_ALIAS="${RELEASE_KEY_ALIAS:-$(prop keyAlias)}"
REL_KEY_PASS="${RELEASE_KEY_PASS:-$(prop keyPassword)}"
# storeFile in the properties file may be relative to the gradle root
[[ -n "$REL_KS" && ! -f "$REL_KS" && -f "$ROOT_DIR/$REL_KS" ]] && REL_KS="$ROOT_DIR/$REL_KS"

for kv in "REL_KS:$REL_KS" "REL_KS_PASS:$REL_KS_PASS" "REL_KEY_ALIAS:$REL_KEY_ALIAS" "REL_KEY_PASS:$REL_KEY_PASS"; do
	[[ -n "${kv#*:}" ]] || { echo "error: missing ${kv%%:*} (add armsx2_keystore.properties or set RELEASE_* env)" >&2; exit 1; }
done
for f in "$PROF" "$DEBUG_KS" "$REL_KS" "$LINEAGE" "$ZIPALIGN" "$APKSIGNER"; do
	[[ -e "$f" ]] || { echo "FATAL missing: $f" >&2; exit 1; }
done

WORK="$ROOT_DIR/app/build/rel-${VN}"; rm -rf "$WORK"; mkdir -p "$WORK/lib-stage/lib/arm64-v8a"
BUILT="$ROOT_DIR/app/build/outputs/apk/github/release/app-github-release.apk"

build_core() { # pagesize libname outapk
	local ps="$1" ln="$2" outapk="$3"
	echo "=== building core $ln (page=$ps, pgo=optimize) ==="
	rm -f "$BUILT"
	"$GRADLE" -p "$ROOT_DIR" :app:assembleGithubRelease \
		-Parmsx2.hostPageSize="$ps" \
		-Parmsx2.nativeLibName="$ln" \
		-Parmsx2.pgo="${PGO_MODE:-optimize}" \
		-Parmsx2.pgoProfile="$PROF" \
		-Parmsx2.versionCode="$VC" \
		-Parmsx2.versionName="$VN"
	[[ -f "$BUILT" ]] || { echo "FATAL gradle produced no APK for $ln" >&2; exit 1; }
	unzip -l "$BUILT" "lib/arm64-v8a/lib${ln}.so" >/dev/null || { echo "FATAL $ln .so missing" >&2; exit 1; }
	cp -f "$BUILT" "$outapk"
}

build_core 0x1000 emucore_4k  "$WORK/base-4k.apk"
[[ -n "${ONLY_4K:-}" ]] || build_core 0x4000 emucore_16k "$WORK/base-16k.apk"

unzip -p "$WORK/base-4k.apk"  lib/arm64-v8a/libemucore_4k.so  > "$WORK/lib-stage/lib/arm64-v8a/libemucore_4k.so"
[[ -n "${ONLY_4K:-}" ]] || unzip -p "$WORK/base-16k.apk" lib/arm64-v8a/libemucore_16k.so > "$WORK/lib-stage/lib/arm64-v8a/libemucore_16k.so"
for so in libemucore_4k $([[ -z "${ONLY_4K:-}" ]] && echo libemucore_16k); do
	sz=$(stat -f%z "$WORK/lib-stage/lib/arm64-v8a/$so.so")
	echo "  $so.so = $((sz/1024/1024)) MB"
	[[ "$sz" -gt 10000000 ]] || { echo "FATAL $so too small ($sz)" >&2; exit 1; }
done

# merge: 4k release apk as base, drop old signatures + both cores, re-add both cores STORED
UNSIGNED="$WORK/universal-unsigned.apk"; ALIGNED="$WORK/universal-aligned.apk"
cp -f "$WORK/base-4k.apk" "$UNSIGNED"
zip -qd "$UNSIGNED" "META-INF/*" >/dev/null 2>&1 || true
zip -qd "$UNSIGNED" "lib/arm64-v8a/libemucore_4k.so" "lib/arm64-v8a/libemucore_16k.so" >/dev/null 2>&1 || true
( cd "$WORK/lib-stage" && zip -qr -0 "$UNSIGNED" lib )
"$ZIPALIGN" -f -P 16 4 "$UNSIGNED" "$ALIGNED"

# ROTATION SIGN: old debug key (API<=32) --next-signer--> new release key (API33+) + lineage.
rm -f "$OUTPUT_APK"
"$APKSIGNER" sign \
	--ks "$DEBUG_KS" --ks-key-alias androiddebugkey --ks-pass pass:android --key-pass pass:android \
	--next-signer \
	--ks "$REL_KS" --ks-key-alias "$REL_KEY_ALIAS" --ks-pass "pass:$REL_KS_PASS" --key-pass "pass:$REL_KEY_PASS" \
	--lineage "$LINEAGE" \
	--in "$ALIGNED" --out "$OUTPUT_APK"

echo; echo "================= VERIFY ================="
echo "-- both cores present --"
unzip -l "$OUTPUT_APK" | grep -E "libemucore_(4k|16k)\.so" || { echo "FATAL cores missing" >&2; exit 1; }
echo "-- 16k alignment --"
"$ZIPALIGN" -c -P 16 4 "$OUTPUT_APK" && echo "  zipalign OK"
echo "-- signer certs (expect BOTH old debug + new release) --"
"$APKSIGNER" verify --print-certs "$OUTPUT_APK" | grep -iE "SHA-256|Signer" | head
echo "-- version / package --"
"$AAPT" dump badging "$OUTPUT_APK" 2>/dev/null | grep -E "package: name|versionCode|versionName" | head -2
echo; echo "OUTPUT: $OUTPUT_APK"; shasum -a256 "$OUTPUT_APK"
echo "RELEASE-BUILD-DONE"
