#!/bin/bash
# battery_discharge.sh — Battery life test under active inference load
#
# Reads battery voltage/percentage and logs until shutdown.
# Pass criteria: > 3.5 hours of continuous active inference.
#
# Usage: ./battery_discharge.sh [--log /tmp/battery.csv]

LOG_FILE=${1:-"/tmp/battery_discharge.csv"}
BATTERY_PATH="/sys/class/power_supply/battery"

echo "timestamp,voltage_mv,capacity_pct,current_ma,temp_c,status" > "$LOG_FILE"

echo "================================================================"
echo "  Ear-Brain Battery Discharge Test"
echo "  KPI Target: > 3.5 hours continuous inference"
echo "================================================================"

START_TIME=$(date +%s)
START_CAPACITY=$(cat ${BATTERY_PATH}/capacity 2>/dev/null || echo "100")

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))

    # Read battery info
    VOLTAGE=$(cat ${BATTERY_PATH}/voltage_now 2>/dev/null || echo "0")
    CAPACITY=$(cat ${BATTERY_PATH}/capacity 2>/dev/null || echo "-1")
    CURRENT=$(cat ${BATTERY_PATH}/current_now 2>/dev/null || echo "0")
    TEMP=$(cat ${BATTERY_PATH}/temp 2>/dev/null || echo "0")
    STATUS=$(cat ${BATTERY_PATH}/status 2>/dev/null || echo "Unknown")

    VOLTAGE_MV=$((VOLTAGE / 1000))
    TEMP_C=$(echo "scale=1; $TEMP / 10" | bc 2>/dev/null || echo "0")
    CURRENT_MA=$((CURRENT / 1000))

    TIMESTAMP=$(date +%H:%M:%S)
    echo "$TIMESTAMP,$VOLTAGE_MV,$CAPACITY,$CURRENT_MA,$TEMP_C,$STATUS" >> "$LOG_FILE"

    # Status
    HOURS=$((ELAPSED / 3600))
    MINS=$(( (ELAPSED % 3600) / 60))
    printf "\r  [%02d:%02d] Battery: %s%% | Voltage: %dmV | Current: %dmA | Temp: %s°C" \
        "$HOURS" "$MINS" "$CAPACITY" "$VOLTAGE_MV" "$CURRENT_MA" "$TEMP_C"

    # Check if battery is depleted
    if [ "$CAPACITY" -le 5 ] 2>/dev/null; then
        echo ""
        echo "  Battery depleted (${CAPACITY}%) — ending test"
        break
    fi

    # Check if voltage is critically low
    if [ "$VOLTAGE_MV" -le 3200 ] 2>/dev/null; then
        echo ""
        echo "  Voltage critically low (${VOLTAGE_MV}mV) — ending test"
        break
    fi

    sleep 30
done

TOTAL_SECONDS=$(( $(date +%s) - START_TIME ))
TOTAL_HOURS=$(echo "scale=2; $TOTAL_SECONDS / 3600" | bc)

echo ""
echo "================================================================"
echo "  Test Complete"
echo "  Duration:       ${TOTAL_HOURS} hours (${TOTAL_SECONDS}s)"
echo "  Start Battery:  ${START_CAPACITY}%"
echo "  End Battery:    ${CAPACITY}%"
echo "  KPI Target:     > 3.5 hours"

TARGET_SECONDS=$((3 * 3600 + 1800))  # 3.5 hours in seconds
if [ "$TOTAL_SECONDS" -ge "$TARGET_SECONDS" ] 2>/dev/null; then
    echo "  KPI Status:     ✅ PASS"
else
    echo "  KPI Status:     ❌ FAIL"
fi
echo "  Log saved:      ${LOG_FILE}"
echo "================================================================"
