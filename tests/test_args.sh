#!/bin/bash
# Test argument parsing for motionCrop
# Run from project root: bash tests/test_args.sh

set -u

EXE="src/x64/Release/motionCrop.exe"
VIDEO="samples/sample.MP4"
PASS=0
FAIL=0

# Test that output contains expected string
test_output() {
    local desc="$1"
    local expected="$2"
    shift 2
    local output
    output=$("$EXE" "$@" 2>&1 | head -10)
    if echo "$output" | grep -qi "$expected"; then
        echo "PASS: $desc"
        ((PASS++))
    else
        echo "FAIL: $desc"
        echo "  Expected: '$expected'"
        echo "  Got: $output"
        ((FAIL++))
    fi
}

# Test exit code
test_exit() {
    local desc="$1"
    local expected_code="$2"
    shift 2
    "$EXE" "$@" >/dev/null 2>&1
    local code=$?
    if [ $code -eq $expected_code ]; then
        echo "PASS: $desc (exit $code)"
        ((PASS++))
    else
        echo "FAIL: $desc (exit $code, expected $expected_code)"
        ((FAIL++))
    fi
}

echo "=== MotionCrop Argument Tests ==="
echo ""

# Check executable exists
if [ ! -f "$EXE" ]; then
    echo "ERROR: Executable not found: $EXE"
    echo "Run: msbuild src/MotionCrop.sln -p:Configuration=Release -p:Platform=x64"
    exit 1
fi

echo "--- Help/Usage Tests ---"
test_output "No args shows usage" "Usage:"
test_output "-h shows usage" "Usage:" -h
test_output "-help shows usage" "Usage:" -help

echo ""
echo "--- Positional Mode Tests ---"
test_output "Filename only" "Window Size: 400" "$VIDEO" -nogui
test_output "Window size 500" "Window size set to 500" "$VIDEO" 500 -nogui
test_output "Threshold 0.5" "Threshold set to 0.5" "$VIDEO" 400 0.5 -nogui
test_output "Iterations 3" "Iterations set to 3" "$VIDEO" 400 1.0 3 -nogui
test_output "Verbose 1" "Verbose set to 1" "$VIDEO" 400 1.0 2 1 -nogui

echo ""
echo "--- Flag Mode Tests ---"
test_output "-w 500" "Window size set to 500" "$VIDEO" -w 500 -nogui
test_output "-window 500" "Window size set to 500" "$VIDEO" -window 500 -nogui
test_output "-t 0.5" "Threshold set to 0.5" "$VIDEO" -t 0.5 -nogui
test_output "-threshold 0.5" "Threshold set to 0.5" "$VIDEO" -threshold 0.5 -nogui
test_output "-i 3" "Iterations set to 3" "$VIDEO" -i 3 -nogui
test_output "-iterations 3" "Iterations set to 3" "$VIDEO" -iterations 3 -nogui
test_output "-v enables verbose" "Verbose set to 1" "$VIDEO" -v -nogui
test_output "-v 1" "Verbose set to 1" "$VIDEO" -v 1 -nogui
test_output "-verbose" "Verbose set to 1" "$VIDEO" -verbose -nogui
test_output "Flags before filename" "Window size set to 500" -w 500 "$VIDEO" -nogui

echo ""
echo "--- Codec Tests ---"
test_output "-c MJPG" "Codec set to MJPG" "$VIDEO" -c MJPG -nogui
test_output "-codec mjpg (lowercase)" "Codec set to MJPG" "$VIDEO" -codec mjpg -nogui
test_output "-c MPEG" "Codec set to MPEG" "$VIDEO" -c MPEG -nogui
test_output "-c MPV4" "Codec set to MPV4" "$VIDEO" -c MPV4 -nogui
test_output "-c WMV2" "Codec set to WMV2" "$VIDEO" -c WMV2 -nogui

echo ""
echo "--- Clamping Tests ---"
test_output "Window clamped to min 50" "Window size set to 50" "$VIDEO" 10 -nogui
test_output "Window clamped to max 5000" "Window size set to 5000" "$VIDEO" 9999 -nogui
test_output "Threshold clamped to min 0.05" "Threshold set to 0.05" "$VIDEO" 400 0.01 -nogui
test_output "Threshold clamped to max 2" "Threshold set to 2" "$VIDEO" 400 5.0 -nogui

echo ""
echo "--- Error Tests ---"
test_output "Unknown option -x" "Unknown option" "$VIDEO" -x
test_output "Unknown codec" "Unknown codec" "$VIDEO" -c INVALID -nogui

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
