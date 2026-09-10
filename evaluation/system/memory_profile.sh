#!/bin/bash
# memory_profile.sh — RAM usage profiling for orbitd daemon
#
# Monitors /proc/meminfo and process-specific memory every second.
# Pass criteria: Peak RAM < 3200 MB over 24-hour soak test.
#
# Usage: ./memory_profile.sh [--duration 86400] [--log /tmp/memory.csv]

DURATION=${1:-3600}  # Default 1 hour
LOG_FILE=${2:-"/tmp/memory_profile.csv"}

echo "timestamp,total_mb,used_mb,free_mb,available_mb,orbitd_rss_mb,orbitd_vsz_mb" > "$LOG_FILE"

echo "================================================================"
echo "  Ear-Brain Memory Profiler"
echo "  Duration: ${DURATION}s  |  Log: ${LOG_FILE}"
echo "================================================================"

START_TIME=$(date +%s)
PEAK_USED=0
PEAK_RSS=0

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))
    if [ $ELAPSED -ge $DURATION ]; then
        break
    fi

    # System-wide memory
    eval $(free -m | awk '/Mem:/ {printf "TOTAL=%d USED=%d FREE=%d", $2, $3, $4}')
    AVAILABLE=$(awk '/MemAvailable/ {printf "%d", $2/1024}' /proc/meminfo)

    # orbitd process memory
    ORBITD_PID=$(pgrep -o orbitd 2>/dev/null)
    if [ -n "$ORBITD_PID" ]; then
        ORBITD_RSS=$(awk '/VmRSS/ {print int($2/1024)}' /proc/$ORBITD_PID/status 2>/dev/null || echo "0")
        ORBITD_VSZ=$(awk '/VmSize/ {print int($2/1024)}' /proc/$ORBITD_PID/status 2>/dev/null || echo "0")
    else
        ORBITD_RSS=0
        ORBITD_VSZ=0
    fi

    # Track peaks
    [ "$USED" -gt "$PEAK_USED" ] 2>/dev/null && PEAK_USED=$USED
    [ "$ORBITD_RSS" -gt "$PEAK_RSS" ] 2>/dev/null && PEAK_RSS=$ORBITD_RSS

    # Log
    TIMESTAMP=$(date +%H:%M:%S)
    echo "$TIMESTAMP,$TOTAL,$USED,$FREE,$AVAILABLE,$ORBITD_RSS,$ORBITD_VSZ" >> "$LOG_FILE"

    # Status
    printf "\r  [%02d:%02d] System: %dMB/%dMB | orbitd RSS: %dMB | Peak: %dMB" \
        $((ELAPSED/60)) $((ELAPSED%60)) "$USED" "$TOTAL" "$ORBITD_RSS" "$PEAK_USED"

    sleep 1
done

echo ""
echo ""
echo "================================================================"
echo "  Test Complete"
echo "  Duration:      $(($(date +%s) - START_TIME))s"
echo "  Peak System:   ${PEAK_USED} MB"
echo "  Peak orbitd:   ${PEAK_RSS} MB"
echo "  KPI Target:    < 3200 MB peak"

if [ "$PEAK_USED" -lt 3200 ] 2>/dev/null; then
    echo "  KPI Status:    ✅ PASS"
else
    echo "  KPI Status:    ❌ FAIL"
fi
echo "  Log saved:     ${LOG_FILE}"
echo "================================================================"
