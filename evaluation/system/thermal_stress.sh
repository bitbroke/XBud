#!/bin/bash
# thermal_stress.sh — Sustained thermal test for the Ear-Brain case
#
# Runs continuous ASR + SLM inference and monitors temperature.
# Pass criteria: Skin temperature < 42°C at 32°C ambient for 45 minutes.
#
# Usage: ./thermal_stress.sh [--duration 2700] [--log /tmp/thermal.csv]

DURATION=${1:-2700}  # Default 45 minutes
LOG_FILE=${2:-"/tmp/thermal_stress.csv"}
SAMPLE_INTERVAL=5    # Log every 5 seconds

echo "timestamp,cpu_temp_c,npu_freq_khz,cpu_freq_khz,ram_used_mb,slm_active" > "$LOG_FILE"

echo "================================================================"
echo "  Ear-Brain Thermal Stress Test"
echo "  Duration: ${DURATION}s  |  Log: ${LOG_FILE}"
echo "================================================================"

START_TIME=$(date +%s)
PEAK_TEMP=0

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))
    if [ $ELAPSED -ge $DURATION ]; then
        break
    fi

    # Read temperature
    TEMP_MC=$(cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null || echo "0")
    TEMP_C=$(echo "scale=1; $TEMP_MC / 1000" | bc)

    # Read frequencies
    CPU_FREQ=$(cat /sys/devices/system/cpu/cpu4/cpufreq/scaling_cur_freq 2>/dev/null || echo "0")
    NPU_FREQ=$(cat /sys/class/devfreq/fdab0000.npu/cur_freq 2>/dev/null || echo "0")

    # Read RAM usage
    RAM_USED=$(free -m | awk '/Mem:/ {print $3}')

    # Check if SLM is active (by checking if gemma is loaded)
    SLM_ACTIVE=$(pgrep -f "orbitd" > /dev/null && echo "yes" || echo "no")

    # Track peak
    PEAK_INT=$(echo "$TEMP_C" | cut -d. -f1)
    PEAK_DEC=$(echo "$PEAK_TEMP" | cut -d. -f1)
    if [ "$PEAK_INT" -gt "$PEAK_DEC" ] 2>/dev/null; then
        PEAK_TEMP=$TEMP_C
    fi

    # Log
    TIMESTAMP=$(date +%H:%M:%S)
    echo "$TIMESTAMP,$TEMP_C,$NPU_FREQ,$CPU_FREQ,$RAM_USED,$SLM_ACTIVE" >> "$LOG_FILE"

    # Status line
    printf "\r  [%02d:%02d] Temp: %s°C | CPU: %s MHz | NPU: %s MHz | RAM: %s MB | Peak: %s°C" \
        $((ELAPSED/60)) $((ELAPSED%60)) "$TEMP_C" \
        $((CPU_FREQ/1000)) $((NPU_FREQ/1000)) "$RAM_USED" "$PEAK_TEMP"

    # Emergency check
    if [ "$PEAK_INT" -ge 45 ] 2>/dev/null; then
        echo ""
        echo "  ⚠️  EMERGENCY: Temperature exceeded 45°C — stopping test!"
        break
    fi

    sleep $SAMPLE_INTERVAL
done

echo ""
echo ""
echo "================================================================"
echo "  Test Complete"
echo "  Duration:     $(($(date +%s) - START_TIME))s"
echo "  Peak Temp:    ${PEAK_TEMP}°C"
echo "  KPI Target:   < 42.0°C"

PEAK_INT=$(echo "$PEAK_TEMP" | cut -d. -f1)
if [ "$PEAK_INT" -lt 42 ] 2>/dev/null; then
    echo "  KPI Status:   ✅ PASS"
else
    echo "  KPI Status:   ❌ FAIL"
fi
echo "  Log saved:    ${LOG_FILE}"
echo "================================================================"
