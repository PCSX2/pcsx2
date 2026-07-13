#!/usr/bin/env bash
# ============================================================================
# ci-nightly-dualcore.sh — GitHub Actions dual-core (4k + 16k) + PGO nightly.
# ----------------------------------------------------------------------------
# A stock `./gradlew :app:assembleRelease` produces ONE core at the default
# host page size (emucore_4k @ 0x1000). That APK cannot load its native core on
# 16k-page devices (Android 15+ handhelds), so half the fleet is broken.
#
# PCSX2 fastmem bakes the host page size in at compile time, so a universal APK
# needs the core compiled TWICE (0x1000 and 0x4000) and both .so's merged into
# one APK — exactly what tools/build-release-apk.sh does for hand-built
# releases. This is the CI-portable (Linux, no macOS assumptions) version of
# that recipe, plus PGO=optimize so the nightly matches release performance.
#
# Runs from platforms/android (the gradle root); the nightly workflow sets
# working-directory accordingly.
#
# Env:
#   VC, VN                versionCode / versionName            (required)
#   FLAVOR                Github (sideload, com.armsx2) | Play (default Github)
#   PROF                  merged .profdata  (default pgo/armsx2.profdata; if the
#                         file is absent the build falls back to PGO=none rather
#                         than failing, so a missing profile never breaks CI)
#   OUT                   output apk path
#   NIGHTLY_KEYSTORE / NIGHTLY_KS_PASS / NIGHTLY_KEY_ALIAS / NIGHTLY_KEY_PASS
#                         stable signing key (recommended: add as a repo secret
#                         so nightly-over-nightly updates install; otherwise a
#                         throwaway debug key is generated and users must
#                         reinstall between nightlies).
# ============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # platforms/android
GRADLE="$ROOT_DIR/gradlew"

VC="${VC:?set VC=<versionCode>}"
VN="${VN:?set VN=<versionName>}"
FLAVOR="${FLAVOR:-Github}"                                    # Github | Play
flavor_lc="$(printf '%s' "$FLAVOR" | tr '[:upper:]' '[:lower:]')"
PROF="${PROF:-$ROOT_DIR/pgo/armsx2.profdata}"
OUT="${OUT:-$ROOT_DIR/app/build/outputs/apk/nightly/ARMSX2-nightly-${VN}-vc${VC}.apk}"

# --- resolve Android SDK build-tools (Linux CI exports ANDROID_HOME/SDK_ROOT) ---
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
[[ -n "$SDK" && -d "$SDK" ]] || { echo "FATAL: ANDROID_HOME/ANDROID_SDK_ROOT not set" >&2; exit 1; }
BT="$(find "$SDK/build-tools" -mindepth 1 -maxdepth 1 -type d | sort -V | tail -1)"
ZIPALIGN="$BT/zipalign"; APKSIGNER="$BT/apksigner"; AAPT="$BT/aapt2"
for t in "$ZIPALIGN" "$APKSIGNER"; do
	[[ -x "$t" ]] || { echo "FATAL: missing build-tool $t" >&2; exit 1; }
done

# --- PGO: optimize when the profile exists, else fall back so CI never fails ---
if [[ -f "$PROF" ]]; then
	PGO_ARGS=(-Parmsx2.pgo=optimize -Parmsx2.pgoProfile="$PROF")
	echo "PGO: optimize using $PROF ($(( $(wc -c < "$PROF") / 1024 / 1024 )) MB)"
else
	PGO_ARGS=(-Parmsx2.pgo=none)
	echo "WARNING: PGO profile not found at $PROF — building PGO=none (slower core)." >&2
fi

WORK="$ROOT_DIR/app/build/nightly-dualcore"; rm -rf "$WORK"
mkdir -p "$WORK/lib-stage/lib/arm64-v8a" "$(dirname "$OUT")"
BUILT="$ROOT_DIR/app/build/outputs/apk/${flavor_lc}/release/app-${flavor_lc}-release.apk"

fsize() { stat -c%s "$1" 2>/dev/null || stat -f%z "$1"; }   # Linux || macOS

build_core() { # pagesize libname
	local ps="$1" ln="$2"
	echo "=== core $ln (page=$ps, flavor=$FLAVOR) ==="
	rm -f "$BUILT"
	"$GRADLE" -p "$ROOT_DIR" ":app:assemble${FLAVOR}Release" \
		-Parmsx2.hostPageSize="$ps" \
		-Parmsx2.nativeLibName="$ln" \
		"${PGO_ARGS[@]}" \
		-Parmsx2.versionCode="$VC" \
		-Parmsx2.versionName="$VN" \
		--stacktrace
	[[ -f "$BUILT" ]] || { echo "FATAL: gradle produced no APK for $ln ($BUILT)" >&2; exit 1; }
	unzip -l "$BUILT" "lib/arm64-v8a/lib${ln}.so" >/dev/null \
		|| { echo "FATAL: lib${ln}.so missing from $BUILT" >&2; exit 1; }
	cp -f "$BUILT" "$WORK/base-${ln}.apk"
	unzip -p "$BUILT" "lib/arm64-v8a/lib${ln}.so" > "$WORK/lib-stage/lib/arm64-v8a/lib${ln}.so"
}

build_core 0x1000 emucore_4k
build_core 0x4000 emucore_16k

for so in emucore_4k emucore_16k; do
	sz="$(fsize "$WORK/lib-stage/lib/arm64-v8a/lib${so}.so")"
	echo "  lib${so}.so = $(( sz / 1024 / 1024 )) MB"
	[[ "$sz" -gt 10000000 ]] || { echo "FATAL: lib${so}.so too small ($sz bytes)" >&2; exit 1; }
done

# --- merge: 4k APK as base, drop old signatures + both cores, re-add STORED ---
UNS="$WORK/universal-unsigned.apk"; ALN="$WORK/universal-aligned.apk"
cp -f "$WORK/base-emucore_4k.apk" "$UNS"
zip -qd "$UNS" "META-INF/*" >/dev/null 2>&1 || true
zip -qd "$UNS" "lib/arm64-v8a/libemucore_4k.so" "lib/arm64-v8a/libemucore_16k.so" >/dev/null 2>&1 || true
( cd "$WORK/lib-stage" && zip -qr -0 "$UNS" lib )
"$ZIPALIGN" -f -P 16 4 "$UNS" "$ALN"

# --- sign ---------------------------------------------------------------------
# Preferred: the SAME v3 rotation lineage as the hand-built releases (old debug
# key for API<=32 --next-signer--> release key for API33+). This is what lets a
# nightly install OVER an existing com.armsx2 install. The three keystores +
# lineage arrive as repo secrets, decoded to files by the workflow and passed in
# as ROTATION_* env vars. If they're absent we fall back to a throwaway key and
# LOUDLY warn (that APK won't update over installed builds).
rm -f "$OUT"
if [[ -n "${ROTATION_DEBUG_KS:-}"   && -f "${ROTATION_DEBUG_KS:-/nope}"   \
   && -n "${ROTATION_RELEASE_KS:-}" && -f "${ROTATION_RELEASE_KS:-/nope}" \
   && -n "${ROTATION_LINEAGE:-}"    && -f "${ROTATION_LINEAGE:-/nope}"    \
   && -n "${ROTATION_RELEASE_KEY_ALIAS:-}" && -n "${ROTATION_RELEASE_KS_PASS:-}" ]]; then
	echo "Signing with the release rotation lineage (debug<=API32 -> release>=API33)."
	"$APKSIGNER" sign \
		--ks "$ROTATION_DEBUG_KS" --ks-key-alias "${ROTATION_DEBUG_ALIAS:-androiddebugkey}" \
		--ks-pass "pass:${ROTATION_DEBUG_PASS:-android}" --key-pass "pass:${ROTATION_DEBUG_PASS:-android}" \
		--next-signer \
		--ks "$ROTATION_RELEASE_KS" --ks-key-alias "$ROTATION_RELEASE_KEY_ALIAS" \
		--ks-pass "pass:$ROTATION_RELEASE_KS_PASS" --key-pass "pass:${ROTATION_RELEASE_KEY_PASS:-$ROTATION_RELEASE_KS_PASS}" \
		--lineage "$ROTATION_LINEAGE" \
		--in "$ALN" --out "$OUT"
else
	echo "WARNING: ROTATION_* signing secrets not set — using a throwaway key." >&2
	echo "         This nightly will NOT install over existing com.armsx2 builds." >&2
	KS="${NIGHTLY_KEYSTORE:-$HOME/.android/debug.keystore}"
	KS_PASS="${NIGHTLY_KS_PASS:-android}"; KS_ALIAS="${NIGHTLY_KEY_ALIAS:-androiddebugkey}"; KEY_PASS="${NIGHTLY_KEY_PASS:-android}"
	if [[ ! -f "$KS" ]]; then
		mkdir -p "$(dirname "$KS")"
		keytool -genkeypair -keystore "$KS" -storepass "$KS_PASS" -keypass "$KEY_PASS" \
			-alias "$KS_ALIAS" -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
	fi
	"$APKSIGNER" sign --ks "$KS" --ks-key-alias "$KS_ALIAS" --ks-pass "pass:$KS_PASS" --key-pass "pass:$KEY_PASS" --out "$OUT" "$ALN"
fi

echo; echo "================= VERIFY ================="
echo "-- both cores present --"
unzip -l "$OUT" | grep -E "libemucore_(4k|16k)\.so" || { echo "FATAL: cores missing" >&2; exit 1; }
echo "-- 16k alignment --"; "$ZIPALIGN" -c -P 16 4 "$OUT" && echo "  align OK"
echo "-- signature --"; "$APKSIGNER" verify "$OUT" && echo "  sig OK"
[[ -x "$AAPT" ]] && "$AAPT" dump badging "$OUT" 2>/dev/null | grep -E "package: name|versionCode|versionName" | head -2
echo; echo "OUTPUT: $OUT"
echo "NIGHTLY-DUALCORE-DONE"
