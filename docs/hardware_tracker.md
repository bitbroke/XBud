# Ear-Brain Hardware Tracker

## Target Specifications

| Component | Part Number / Description | Cost Estimate (USD) | Notes |
| :--- | :--- | :--- | :--- |
| **SoC** | Rockchip RK3576 (4xA72, 4xA53, 6 TOPS NPU) | $12.00 | Excellent AI/W ratio. Requires strict thermal management. |
| **RAM** | 4GB LPDDR4x | $6.50 | 4GB is absolute minimum for Gemma 3 1B + system overhead. |
| **Storage** | 32GB eMMC 5.1 | $4.50 | SQLCipher database requires fast random I/O. |
| **Bluetooth/Audio** | BES2600IWP | $3.50 | Handles HFP/SCO and A2DP. Routes SCO over I2S to RK3576. |
| **Battery (Case)** | 1,500mAh Li-Po (Custom Cell) | $2.50 | Upsized to meet 3.5h continuous inference KPI. |
| **PMIC** | Rockchip RK806-1 | $1.20 | Companion PMIC for RK3576 DVFS support. |
| **Thermal** | Copper heat spreader + graphite pad | $0.80 | Crucial for keeping skin temp < 42°C during SLM batching. |
| **Earbuds** | BES2500-based generic PCBA | $5.50 (pair) | Standard TWS earbuds, only need good mic quality. |
| **Misc** | PCB, passives, connectors, housing | $2.00 | |
| | **Total BOM (Target)** | **$38.50** | |

## Hardware Revisions

### Prototype (v0.1)
- **Platform**: Orange Pi Zero 4 (8GB RAM, RK3576)
- **Audio**: USB Bluetooth Dongle (CSR8510) + ALSA loopback for testing
- **Status**: Development platform for software stack validation.

### EVT (v0.5) - Target: Dec 2026
- Custom PCBA (RK3576 + 4GB RAM + BES2600).
- 3D printed housing.
- Thermal validation focus.

### DVT (v0.8) - Target: Mar 2027
- Tooling complete.
- Battery integration and power optimization.

### PVT / MP (v1.0) - Target: Jun 2027
- Final DPDP compliance audit.
- Mass production.

## Known Risks & Mitigations

1.  **Thermal Throttling**: The RK3576 can hit 85°C junction temp under full NPU+CPU load.
    *   *Mitigation*: The `ThermalGovernor` aggressively downclocks the CPU and spaces out SLM inference batches to maintain a safe case temperature.
2.  **RAM Exhaustion**: Gemma 3 1B takes ~1.2GB quantized, Whisper ~350MB, OS ~150MB. Leaving ~2.3GB. Memory leaks are fatal.
    *   *Mitigation*: `tmpfs` is strictly limited to 128MB. No dynamic memory allocation in audio path. Aggressive watchdog monitoring.
3.  **Battery Life**: Inference is power-hungry (~3W peak).
    *   *Mitigation*: BES2600 handles idle state. RK3576 is completely asleep until SCO link is established.
