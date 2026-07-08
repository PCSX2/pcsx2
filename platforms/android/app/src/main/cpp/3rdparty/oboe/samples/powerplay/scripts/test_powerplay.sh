#!/bin/bash
# PowerPlay Automation Test Script
# Demonstrates a full test cycle: Open -> Play Offload -> Wait -> Close

set -e

PACKAGE="com.google.oboe.samples.powerplay"
ACTIVITY="${PACKAGE}/.MainActivity"
LOG_TAG="PowerPlay"
TEST_DURATION_MS=10000  # 10 seconds

echo "=== PowerPlay Automation Test ==="
echo ""

# Check device connection
if ! adb devices | grep -q "device$"; then
    echo "ERROR: No Android device connected"
    exit 1
fi

echo "[1/6] Checking if PowerPlay is installed..."
if ! adb shell pm list packages | grep -q "$PACKAGE"; then
    echo "ERROR: PowerPlay is not installed. Please run:"
    echo "  cd samples/powerplay && ../../gradlew installDebug"
    exit 1
fi
echo "  ✓ PowerPlay is installed"

echo ""
echo "[2/6] Force-stopping any existing instance..."
adb shell am force-stop "$PACKAGE"
sleep 1
echo "  ✓ App stopped"

echo ""
echo "[3/6] Starting playback with PCM Offload mode..."
adb shell am start -n "$ACTIVITY" \
    --es command play \
    --es perf_mode offload \
    --ei duration_ms "$TEST_DURATION_MS" \
    --ei volume 75

echo "  ✓ Play command sent"

echo ""
echo "[4/6] Monitoring status (waiting for ${TEST_DURATION_MS}ms)..."
echo "---"

# Capture and display logs for the test duration
timeout $((TEST_DURATION_MS / 1000 + 2)) adb logcat -s "$LOG_TAG:V" 2>/dev/null | while read -r line; do
    if echo "$line" | grep -q "POWERPLAY_STATUS"; then
        echo "  LOG: $line"
    fi
done || true

echo "---"
echo "  ✓ Test duration completed"

echo ""
echo "[5/6] Verifying playback stopped..."
FINAL_STATUS=$(adb logcat -d -s "$LOG_TAG:V" | grep "POWERPLAY_STATUS" | tail -1)
if echo "$FINAL_STATUS" | grep -q "STOPPED"; then
    echo "  ✓ Playback stopped correctly"
else
    echo "  ⚠ Last status: $FINAL_STATUS"
fi

echo ""
echo "[6/6] Checking offload status from logs..."
if adb logcat -d -s "$LOG_TAG:V" | grep -q "OFFLOAD=true"; then
    echo "  ✓ PCM Offload was active during playback"
else
    echo "  ⚠ PCM Offload may not have been active (device might not support it)"
fi

echo ""
echo "=== Test Complete ==="
echo ""
echo "Full logs saved to: powerplay_test.log"
adb logcat -d -s "$LOG_TAG:V" > powerplay_test.log

echo ""
echo "To run additional tests:"
echo "  # Background playback"
echo "  adb shell am start -n $ACTIVITY --es command play --es perf_mode offload --ez background true"
